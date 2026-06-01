#include <hackrf.h>
#include <fftw3.h>
#include <opencv2/opencv.hpp>
#include <opencv2/ml.hpp>

#include <iostream>
#include <vector>
#include <complex>
#include <thread>
#include <atomic>
#include <cmath>
#include <numeric>
#include <algorithm>
#include <array>
#include <chrono>
#include <iomanip>
#include <deque>
#include <unordered_map>
#include <fstream>

using namespace std;
using namespace cv;
using namespace cv::ml;

// ================= CONFIGURATION =================
constexpr uint64_t START_FREQ  = 1000000ULL;
constexpr uint64_t END_FREQ    = 6000000000ULL;
constexpr uint64_t STEP_FREQ   = 10000000ULL;

constexpr uint32_t SAMPLE_RATE = 20000000;       // 20 MSPS IQ stream
constexpr int FFT_SIZE = 1024;
constexpr int OVERLAP = 512;                     // 50% overlap for Welch
constexpr int STEP_SIZE = FFT_SIZE - OVERLAP;

// [CRITICAL] Feature Space Dimension - MUST BE 8
constexpr int FEATURES = 8;
static_assert(FEATURES == 8, "Feature Space Dimension MUST be 8!");

// Hardware calibration constants
constexpr double LNA_GAIN = 32.0;
constexpr double VGA_GAIN = 20.0;
constexpr double HACKRF_NOISE_FIGURE = 10.0;     // Approximate dB
constexpr double EPS = 1e-12;

// GMM Configuration - learns RF environment manifold
constexpr int GMM_COMPONENTS = 8;                 // K = 6-10 recommended
constexpr int TRAINING_SAMPLES = 2000;            // Global training pool

// Temporal tracking parameters
constexpr double EMA_ALPHA = 0.8;                 // Slow drift tracking
constexpr int LIKELIHOOD_HISTORY_SIZE = 100;      // For robust statistics

// Persistence configuration
const string MODEL_FILE = "rf_scene_model.xml";
const string SPECTRUM_FILE = "rf_spectrum_map.xml";

// ================= SPECTRUM MAPPING OUTPUT =================
struct SpectrumBin {
    uint64_t frequency;
    float occupancy_probability;                  // 0.0 = noise, 1.0 = continuous signal
    float noise_floor;                            // Adaptive noise estimate
    int activity_class;                           // 0=noise, 1=burst, 2=continuous
    float last_power_dbm;                         // For visualization
    float aoa;                                    // Angle of Arrival (degrees) [NEW]
    double target_lat;                            // Target latitude (geo-location) [NEW]
    double target_lon;                            // Target longitude (geo-location) [NEW]
};

// ================= DIRECTION FINDING & GEO-LOCATION STRUCTURES [NEW] =================
struct GeoCoordinate {
    double latitude;
    double longitude;
};

struct SignalRecord {
    uint64_t timestamp;
    double freq;
    double bw;
    int activity_class;
    int modulation;
    std::string protocol;
    float aoa;                                    // Angle of Arrival [NEW]
    double target_lat;                            // Target latitude [NEW]
    double target_lon;                            // Target longitude [NEW]
    std::string payload;
};

// ================= TEMPORAL STATE ENGINE =================
struct TemporalState {
    double ema_likelihood = 0.0;                  // Exponential moving average
    double occupancy_probability = 0.0;           // 0.0 (noise) to 1.0 (signal)
    int burst_counter = 0;                        // Burst detection
    double adaptive_noise_floor = 0.0;            // Per-frequency noise estimate
    double adaptive_noise_sigma = 1.0;            // Noise variance estimate
    deque<double> likelihood_history;             // For robust statistics
    
    void update_history(double likelihood) {
        likelihood_history.push_back(likelihood);
        if (likelihood_history.size() > LIKELIHOOD_HISTORY_SIZE) {
            likelihood_history.pop_front();
        }
    }
    
    // Robust noise floor estimation using Median and MAD
    void compute_robust_noise_floor() {
        if (likelihood_history.size() < 20) return;
        
        vector<double> sorted_history(likelihood_history.begin(), likelihood_history.end());
        sort(sorted_history.begin(), sorted_history.end());
        
        // Median as robust noise floor estimate
        size_t mid = sorted_history.size() / 2;
        adaptive_noise_floor = sorted_history[mid];
        
        // MAD (Median Absolute Deviation) for robust sigma
        vector<double> abs_deviations;
        for (double val : sorted_history) {
            abs_deviations.push_back(abs(val - adaptive_noise_floor));
        }
        sort(abs_deviations.begin(), abs_deviations.end());
        double mad = abs_deviations[abs_deviations.size() / 2];
        adaptive_noise_sigma = 1.4826 * mad;  // Scale factor for normal distribution
        
        if (adaptive_noise_sigma < 0.1) adaptive_noise_sigma = 0.1; // Prevent collapse
    }
};

// ================= GLOBAL RF ENGINE =================
struct GlobalRFEngine {
    bool trained = false;
    Ptr<EM> gmm;
    
    Mat weights, means;
    vector<Mat> inv_covs;                         // Precomputed inverse covariances
    vector<double> log_cov_dets;                  // Precomputed log determinants
};

GlobalRFEngine global_engine;
unordered_map<uint64_t, TemporalState> freq_state_map;
unordered_map<uint64_t, SpectrumBin> spectrum_map;

// ================= LOCK-FREE SPSC RING BUFFER =================
struct IQDataBuffer {
    uint64_t freq;
    vector<complex<float>> iq;
};

template<size_t Size>
class LockFreeQueue {
    array<IQDataBuffer, Size> buffer;
    atomic<size_t> head{0};
    atomic<size_t> tail{0};
public:
    bool push(uint64_t freq, const hackrf_transfer* transfer) {
        size_t current_tail = tail.load(memory_order_relaxed);
        size_t next_tail = (current_tail + 1) % Size;
        if (next_tail == head.load(memory_order_acquire)) return false; 
        
        buffer[current_tail].freq = freq;
        buffer[current_tail].iq.clear();
        buffer[current_tail].iq.reserve(transfer->valid_length / 2);
        for (int i = 0; i < transfer->valid_length; i += 2) {
            float I = (transfer->buffer[i] - 128.0f) / 128.0f;
            float Q = (transfer->buffer[i+1] - 128.0f) / 128.0f;
            buffer[current_tail].iq.emplace_back(I, Q);
        }
        tail.store(next_tail, memory_order_release);
        return true;
    }

    bool pop(IQDataBuffer& item) {
        size_t current_head = head.load(memory_order_relaxed);
        if (current_head == tail.load(memory_order_acquire)) return false; 
        
        item = move(buffer[current_head]);
        head.store((current_head + 1) % Size, memory_order_release);
        return true;
    }
};

LockFreeQueue<200> iq_queue;

// ================= GLOBALS =================
hackrf_device* dev = nullptr;
atomic<bool> running(true);
atomic<uint64_t> active_capture_freq(START_FREQ);
atomic<bool> is_capturing(false);
atomic<int> captured_blocks(0);

// ================= HARDWARE RX CALLBACK =================
int rx_callback(hackrf_transfer* transfer) {
    if (!running || !is_capturing.load(memory_order_acquire)) return 0;
    uint64_t freq = active_capture_freq.load(memory_order_relaxed);
    
    if (iq_queue.push(freq, transfer)) {
        int current = captured_blocks.fetch_add(1, memory_order_relaxed) + 1;
        if (current >= 15) {
            is_capturing.store(false, memory_order_release); 
        }
    }
    return 0;
}

// ================= DSP LAYER: RF FRONT-END =================
// Generate Hamming window with power compensation
vector<double> generate_hamming_window(int N, double& out_window_power) {
    vector<double> w(N);
    out_window_power = 0.0;
    for (int i = 0; i < N; i++) {
        w[i] = 0.54 - 0.46 * cos(2.0 * M_PI * i / (N - 1));
        out_window_power += w[i] * w[i];
    }
    return w;
}

// Welch method: averaged periodogram with overlapping windows
void compute_welch_psd(const vector<complex<float>>& iq_data, 
                       vector<double>& avg_psd_linear,
                       vector<double>& sub_block_variances,
                       const vector<double>& window,
                       double window_power,
                       fftw_plan& plan,
                       fftw_complex* fftw_in,
                       fftw_complex* fftw_out) {
    
    fill(avg_psd_linear.begin(), avg_psd_linear.end(), 0.0);
    sub_block_variances.clear();
    int segments = 0;

    for (size_t offset = 0; offset + FFT_SIZE <= iq_data.size(); offset += STEP_SIZE) {
        // Apply window
        for (int i = 0; i < FFT_SIZE; i++) {
            fftw_in[i][0] = iq_data[offset + i].real() * window[i];
            fftw_in[i][1] = iq_data[offset + i].imag() * window[i];
        }
        
        // FFT
        fftw_execute(plan);

        // Compute PSD and accumulate
        double block_energy = 0.0;
        for (int i = 0; i < FFT_SIZE; i++) {
            double pwr = (fftw_out[i][0] * fftw_out[i][0]) + 
                        (fftw_out[i][1] * fftw_out[i][1]);
            avg_psd_linear[i] += pwr;
            block_energy += pwr;
        }
        
        // Micro-variance: track energy variance across sub-blocks
        sub_block_variances.push_back(block_energy / FFT_SIZE);
        segments++;
    }

    // Welch averaging with window power compensation
    for (int i = 0; i < FFT_SIZE; i++) {
        avg_psd_linear[i] = avg_psd_linear[i] / (segments * window_power * FFT_SIZE);
    }
}

// ================= FEATURE ENGINEERING LAYER =================
// Extract 8-dimensional scale-invariant feature vector
vector<float> extract_feature_vector(uint64_t freq, 
                                     const vector<double>& psd_linear,
                                     const vector<double>& sub_block_variances) {
    int N = psd_linear.size();
    double total_power = 0.0, max_power = -1e9;
    
    for (int i = 0; i < N; i++) {
        total_power += psd_linear[i];
        if (psd_linear[i] > max_power) max_power = psd_linear[i];
    }
    double mean_power = total_power / N;

    // F1: Spectral Entropy (scale-invariant)
    double entropy = 0.0;
    for (int i = 0; i < N; i++) {
        double p = psd_linear[i] / (total_power + EPS);
        if (p > 1e-10) entropy -= p * log2(p);
    }
    entropy /= log2(N); // Normalize to [0,1]

    // F2: Spectral Flatness (scale-invariant)
    double log_sum = 0.0;
    for (int i = 0; i < N; i++) {
        log_sum += log(psd_linear[i] + EPS);
    }
    double geometric_mean = exp(log_sum / N);
    double flatness = geometric_mean / (mean_power + EPS);

    // F3: Crest Factor / PAPR (scale-invariant)
    double crest_factor = max_power / (mean_power + EPS);

    // F4 & F5: Skewness and Kurtosis (shape descriptors, scale-invariant)
    vector<double> psd_db(N);
    double db_mean = 0.0;
    for (int i = 0; i < N; i++) {
        psd_db[i] = 10.0 * log10(psd_linear[i] + EPS);
        db_mean += psd_db[i];
    }
    db_mean /= N;
    
    double db_var = 0.0;
    for (double v : psd_db) {
        db_var += (v - db_mean) * (v - db_mean);
    }
    db_var /= N;
    double db_std = sqrt(db_var + EPS);

    double skewness = 0.0, kurtosis = 0.0;
    for (double v : psd_db) {
        double z = (v - db_mean) / db_std;
        skewness += z * z * z;
        kurtosis += z * z * z * z;
    }
    skewness /= N;
    kurtosis /= N;

    // F6: Micro-variance (temporal energy variance, burst detection)
    double micro_var_mean = 0.0;
    for (double v : sub_block_variances) micro_var_mean += v;
    micro_var_mean /= sub_block_variances.size();
    
    double micro_variance = 0.0;
    for (double v : sub_block_variances) {
        micro_variance += (v - micro_var_mean) * (v - micro_var_mean);
    }
    micro_variance /= sub_block_variances.size();
    micro_variance = log10(micro_variance + EPS); // Log scale for stability

    // F7: Relative Power (normalized to prevent absolute power dominance)
    double relative_power = log10(mean_power + EPS);

    // F8: Temporal Derivative (EMA change rate) - will be computed in temporal engine
    // For now, use normalized frequency as placeholder (will be updated in tracking)
    double norm_freq = (double)(freq - START_FREQ) / (double)(END_FREQ - START_FREQ);

    return {
        (float)entropy,
        (float)flatness,
        (float)crest_factor,
        (float)skewness,
        (float)kurtosis,
        (float)micro_variance,
        (float)relative_power,
        (float)norm_freq
    };
}

// ================= UNSUPERVISED RF MODEL LAYER =================
// Compute log-likelihood using precomputed Mahalanobis distance
double compute_gmm_log_likelihood(const Mat& sample) {
    double total_likelihood = 0.0;
    Mat sample_double;
    sample.convertTo(sample_double, CV_64F);

    for (int k = 0; k < GMM_COMPONENTS; k++) {
        double weight = global_engine.weights.at<double>(0, k);
        Mat diff = sample_double - global_engine.means.row(k);
        Mat intermediate = diff * global_engine.inv_covs[k];
        double mahalanobis = intermediate.dot(diff);
        
        // Log PDF of multivariate Gaussian
        double log_pdf = -0.5 * (FEATURES * log(2.0 * M_PI) + 
                                 global_engine.log_cov_dets[k] + 
                                 mahalanobis);
        total_likelihood += weight * exp(log_pdf);
    }
    
    return log(total_likelihood + EPS);
}

// Train global GMM on collected features
void train_global_gmm(const vector<vector<float>>& training_data) {
    cout << "\n[GLOBAL TRAINING] Constructing RF Environment Manifold...\n";
    
    Mat train_matrix(training_data.size(), FEATURES, CV_32F);
    for (size_t i = 0; i < training_data.size(); i++) {
        for (int j = 0; j < FEATURES; j++) {
            train_matrix.at<float>(i, j) = training_data[i][j];
        }
    }

    // Train GMM with full covariance
    auto gmm_ptr = EM::create();
    gmm_ptr->setClustersNumber(GMM_COMPONENTS);
    gmm_ptr->setCovarianceMatrixType(EM::COV_MAT_GENERIC);
    gmm_ptr->setTermCriteria(TermCriteria(TermCriteria::COUNT + TermCriteria::EPS, 300, 0.1));
    gmm_ptr->trainEM(train_matrix);

    global_engine.gmm = gmm_ptr;
    global_engine.weights = gmm_ptr->getWeights();
    global_engine.means = gmm_ptr->getMeans();
    
    // Precompute inverse covariances and log determinants for speed
    vector<Mat> raw_covs;
    gmm_ptr->getCovariances(raw_covs);
    global_engine.inv_covs.resize(GMM_COMPONENTS);
    global_engine.log_cov_dets.resize(GMM_COMPONENTS);

    for (int k = 0; k < GMM_COMPONENTS; k++) {
        Mat cov_double;
        raw_covs[k].convertTo(cov_double, CV_64F);
        
        // Regularization to prevent singularity
        cov_double += Mat::eye(FEATURES, FEATURES, CV_64F) * 1e-4;

        // SVD for stable inversion
        Mat w, u, vt;
        SVD::compute(cov_double, w, u, vt);
        
        double log_det = 0.0;
        Mat w_inv = Mat::zeros(FEATURES, FEATURES, CV_64F);
        for (int i = 0; i < FEATURES; i++) {
            double val = max(w.at<double>(i, 0), 1e-6);
            log_det += log(val);
            w_inv.at<double>(i, i) = 1.0 / val;
        }
        
        global_engine.log_cov_dets[k] = log_det;
        global_engine.inv_covs[k] = vt.t() * w_inv * u.t();
    }

    global_engine.trained = true;
    cout << "[SYSTEM ONLINE] RF Scene Engine Deployed.\n";
}
// ================= PERSISTENCE (SAVE / LOAD) LAYER =================
bool load_rf_model() {
    FileStorage fs(MODEL_FILE, FileStorage::READ);
    if (!fs.isOpened()) return false; // Dosya yoksa ilk kurulum (sıfırdan eğitim) yapılacak

    cout << "[SYSTEM] Onceki RF Scene Modeli bulundu. Yukleniyor...\n";
    
    fs["weights"] >> global_engine.weights;
    fs["means"] >> global_engine.means;
    
    int cov_size;
    fs["cov_size"] >> cov_size;
    
    global_engine.inv_covs.resize(cov_size);
    global_engine.log_cov_dets.resize(cov_size);
    
    for (int i = 0; i < cov_size; i++) {
        fs["inv_cov_" + to_string(i)] >> global_engine.inv_covs[i];
        fs["log_cov_det_" + to_string(i)] >> global_engine.log_cov_dets[i];
    }
    
    fs.release();
    global_engine.trained = true; // Modeli yüklendi olarak işaretle (Eğitim fazını atlar!)
    cout << "[SYSTEM] RF Scene Modeli basariyla yuklendi! Kesif asmasi atlandi.\n";
    return true;
}

void save_rf_model() {
    if (!global_engine.trained) return; // Eğitilmemiş modeli kaydetmeye gerek yok
    
    FileStorage fs(MODEL_FILE, FileStorage::WRITE);
    if (!fs.isOpened()) {
        cerr << "[ERROR] Model dosyasi yazilamadi!\n";
        return;
    }

    fs << "weights" << global_engine.weights;
    fs << "means" << global_engine.means;
    
    fs << "cov_size" << (int)global_engine.inv_covs.size();
    for (int i = 0; i < global_engine.inv_covs.size(); i++) {
        fs << "inv_cov_" + to_string(i) << global_engine.inv_covs[i];
        fs << "log_cov_det_" + to_string(i) << global_engine.log_cov_dets[i];
    }
    
    fs.release();
    cout << "[SHUTDOWN] RF Scene Modeli '" << MODEL_FILE << "' dosyasina kaydedildi.\n";
}
// ================= DECISION MECHANISM =================
// Classify RF activity based on temporal state and robust statistics
int classify_activity(TemporalState& state, double current_likelihood) {
    // Decision thresholds based on adaptive noise floor
    double noise_floor = state.adaptive_noise_floor;
    double sigma = state.adaptive_noise_sigma;
    
    double high_threshold = noise_floor + 2.0 * sigma;
    double low_threshold = noise_floor - 1.0 * sigma;
    
    if (current_likelihood > high_threshold) {
        // Background / known RF activity
        state.occupancy_probability = max(0.0, state.occupancy_probability - 0.05);
        state.burst_counter = 0;
        return 0; // Noise/background
    }
    else if (current_likelihood > low_threshold) {
        // Transient / weak signal
        state.burst_counter++;
        if (state.burst_counter > 3) {
            state.occupancy_probability = min(1.0, state.occupancy_probability + 0.1);
            return 1; // Burst
        }
        return 0;
    }
    else {
        // Active signal present
        state.occupancy_probability = min(1.0, state.occupancy_probability + 0.3);
        state.burst_counter = 0;
        return 2; // Continuous signal
    }
}

// ================= THREAD 1: HARDWARE SWEEP ORCHESTRATOR =================
void hardware_sweep_thread() {
    uint64_t current_freq = START_FREQ;

    while (running) {
        hackrf_set_freq(dev, current_freq);
        // Hardware settling time: PLL lock + analog filters + AGC
        this_thread::sleep_for(chrono::milliseconds(40));

        active_capture_freq.store(current_freq, memory_order_relaxed);
        captured_blocks.store(0, memory_order_relaxed);
        is_capturing.store(true, memory_order_release);

        while (is_capturing.load(memory_order_acquire) && running) {
            this_thread::yield(); 
        }

        current_freq += STEP_FREQ;
        if (current_freq > END_FREQ) current_freq = START_FREQ;
    }
}

// ================= THREAD 2: DSP ENGINE & TEMPORAL TRACKING =================
void dsp_engine_thread() {
    vector<vector<float>> global_training_pool;
    
    double window_power;
    auto window = generate_hamming_window(FFT_SIZE, window_power);

    // FFTW setup with MEASURE plan for optimization
    fftw_complex* fftw_in = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * FFT_SIZE);
    fftw_complex* fftw_out = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * FFT_SIZE);
    fftw_plan plan = fftw_plan_dft_1d(FFT_SIZE, fftw_in, fftw_out, FFTW_FORWARD, FFTW_MEASURE);

    IQDataBuffer block;
    int processed_frames = 0;

    while (running) {
        if (!iq_queue.pop(block)) {
            this_thread::sleep_for(chrono::milliseconds(1)); 
            continue;
        }

        if (block.iq.size() < FFT_SIZE) continue;

        uint64_t freq = block.freq;
        
        // ===== DSP LAYER: Welch PSD =====
        vector<double> avg_psd_linear(FFT_SIZE, 0.0);
        vector<double> sub_block_variances;
        
        compute_welch_psd(block.iq, avg_psd_linear, sub_block_variances,
                         window, window_power, plan, fftw_in, fftw_out);

        // ===== FEATURE ENGINEERING LAYER =====
        auto features = extract_feature_vector(freq, avg_psd_linear, sub_block_variances);
        
        // ===== PHASE 1: UNSUPERVISED LEARNING =====
        if (!global_engine.trained) {
            global_training_pool.push_back(features);
            
            if (global_training_pool.size() % 100 == 0) {
                cout << "[SPECTRUM EXPLORATION] Mapping RF Environment... " 
                     << global_training_pool.size() << "/" << TRAINING_SAMPLES << "\r" << flush;
            }

            if (global_training_pool.size() >= TRAINING_SAMPLES) {
                train_global_gmm(global_training_pool);
            }
        }
        // ===== PHASE 2: TEMPORAL STATE ENGINE & DECISION =====
        else {
            Mat sample(1, FEATURES, CV_32F);
            for (int i = 0; i < FEATURES; i++) {
                sample.at<float>(0, i) = features[i];
            }

            double log_likelihood = compute_gmm_log_likelihood(sample);
            TemporalState& state = freq_state_map[freq];

            // Update likelihood history for robust statistics
            state.update_history(log_likelihood);
            
            // Compute adaptive noise floor using robust statistics
            state.compute_robust_noise_floor();

            // EMA smoothing for temporal consistency
            if (state.ema_likelihood == 0.0) {
                state.ema_likelihood = log_likelihood;
            } else {
                state.ema_likelihood = EMA_ALPHA * state.ema_likelihood + 
                                      (1.0 - EMA_ALPHA) * log_likelihood;
            }

            // Decision mechanism
            int activity_class = classify_activity(state, state.ema_likelihood);

            // Update spectrum map
            SpectrumBin& bin = spectrum_map[freq];
            bin.frequency = freq;
            bin.occupancy_probability = state.occupancy_probability;
            bin.noise_floor = state.adaptive_noise_floor;
            bin.activity_class = activity_class;
            bin.last_power_dbm = features[6]; // Relative power feature

            // Output significant events
            processed_frames++;
            if (processed_frames % 50 == 0) {
                // Periodic status update
                int active_signals = 0;
                for (const auto& [f, b] : spectrum_map) {
                    if (b.occupancy_probability > 0.7) active_signals++;
                }
                cout << "[STATUS] Active Signals: " << active_signals 
                     << " | Tracked Frequencies: " << spectrum_map.size() << "    \r" << flush;
            }

            // Alert on high occupancy
            if (state.occupancy_probability > 0.8) {
                cout << "\n[!!! SIGNAL DETECTED !!!] Freq: " << freq / 1e6 
                     << " MHz | Occupancy: " << fixed << setprecision(2) 
                     << state.occupancy_probability * 100 << "% | Class: ";
                if (activity_class == 2) cout << "CONTINUOUS";
                else if (activity_class == 1) cout << "BURST";
                else cout << "TRANSIENT";
                cout << " | Power: " << setprecision(1) << features[6] << " (rel)\n";
            }
        }
    }

    fftw_destroy_plan(plan);
    fftw_free(fftw_in);
    fftw_free(fftw_out);
}

// ================= MAIN =================
int main() {
    cout << "═══════════════════════════════════════════════════════════\n";
    cout << "  RF SPECTRUM ACTIVITY MAPPING SYSTEM\n";
    cout << "  Unsupervised Continuous Learning RF Environment Sensor\n";
    cout << "═══════════════════════════════════════════════════════════\n\n";
    
    // Açılışta eski modeli yüklemeyi dene
    load_rf_model();
    
    if (hackrf_init() != HACKRF_SUCCESS) {
        cerr << "[ERROR] HackRF initialization failed!\n";
        return -1;
    }
    
    if (hackrf_open(&dev) != HACKRF_SUCCESS) {
        cerr << "[ERROR] Failed to open HackRF device!\n";
        hackrf_exit();
        return -1;
    }

    hackrf_set_sample_rate(dev, SAMPLE_RATE);
    hackrf_set_lna_gain(dev, LNA_GAIN);
    hackrf_set_vga_gain(dev, VGA_GAIN);
    hackrf_set_amp_enable(dev, 0);

    cout << "[CONFIG] Sample Rate: " << SAMPLE_RATE / 1e6 << " MSPS\n";
    cout << "[CONFIG] Frequency Range: " << START_FREQ / 1e6 << " - " 
         << END_FREQ / 1e6 << " MHz\n";

    hackrf_start_rx(dev, rx_callback, NULL);

    thread sweep_thread(hardware_sweep_thread);
    thread dsp_thread(dsp_engine_thread);

    cout << "[SYSTEM] RF Surveillance Engine ACTIVE. Press ENTER to stop...\n\n";
    cin.get(); // Kullanıcı Enter'a basana kadar bekle

    cout << "\n[SHUTDOWN] Stopping system...\n";
    running = false; // Thread'leri durdur
    
    sweep_thread.join();
    dsp_thread.join();

    // Kapanmadan hemen önce öğrenilen Modeli diske yazdır
    save_rf_model();

    hackrf_stop_rx(dev);
    hackrf_close(dev);
    hackrf_exit();
    
    cout << "[SHUTDOWN] Complete. Final spectrum map contains " 
         << spectrum_map.size() << " tracked frequencies.\n";
    
    return 0;
}
