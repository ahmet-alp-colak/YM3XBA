/**
 * @file DataStructures.hpp
 * @brief Core data structures for Signal Detection module
 * @details Defines signal classification, configuration, and data containers
 * 
 * System: Bilişsel RF Spektrum Haritalama Sistemi
 * Module: 4.1 Sinyal Tespiti (Signal Detection)
 * 
 * @author CognitiveRF Development Team
 * @date 2026-06-21
 */

#ifndef SIGNAL_DETECTION_DATA_STRUCTURES_HPP
#define SIGNAL_DETECTION_DATA_STRUCTURES_HPP

#include <complex>
#include <vector>
#include <string>
#include <cstdint>
#include <deque>

namespace CognitiveRF {
namespace SignalDetection {

// ============================================================================
// TYPE ALIASES
// ============================================================================

using IQSample = std::complex<float>;
using FrequencyHz = uint64_t;

// ============================================================================
// MATHEMATICAL CONSTANTS
// ============================================================================

constexpr double PI = 3.14159265358979323846;
constexpr double TWO_PI = 2.0 * PI;
constexpr double EPS = 1e-12;

// ============================================================================
// ENUMERATIONS
// ============================================================================

/**
 * @enum SignalClass
 * @brief Classification of detected RF activity
 */
enum class SignalClass : uint8_t {
    NOISE = 0,        ///< Background noise or known RF environment
    BURST = 1,        ///< Transient/bursty signal (Wi-Fi, Bluetooth, FHSS)
    CONTINUOUS = 2    ///< Persistent continuous signal (broadcast, jamming)
};

/**
 * @enum HardwareState
 * @brief HackRF hardware state machine
 */
enum class HardwareState : uint8_t {
    IDLE = 0,
    SENSING = 1,     ///< RX mode active
    JAMMING = 2,     ///< TX mode (for future ET integration)
    SPOOFING = 3     ///< TX mode for GNSS spoofing
};

// ============================================================================
// CONFIGURATION STRUCTURES
// ============================================================================

/**
 * @struct SignalDetectionConfig
 * @brief Configuration parameters for signal detection pipeline
 */
struct SignalDetectionConfig {
    // Frequency sweep parameters
    FrequencyHz start_freq_hz = 1000000ULL;      ///< 1 MHz
    FrequencyHz end_freq_hz = 6000000000ULL;     ///< 6 GHz
    FrequencyHz step_freq_hz = 10000000ULL;      ///< 10 MHz step
    
    // SDR parameters
    uint32_t sample_rate_hz = 20000000;          ///< 20 MSPS
    double lna_gain_db = 32.0;                   ///< LNA gain
    double vga_gain_db = 20.0;                   ///< VGA gain
    bool amp_enable = false;                     ///< Amplifier enable
    
    // DSP parameters
    int fft_size = 1024;                         ///< FFT size for Welch PSD
    int overlap = 512;                           ///< 50% overlap
    
    // GMM parameters
    int gmm_components = 8;                      ///< Number of Gaussian components
    int training_samples = 2000;                 ///< Samples for training
    
    // Temporal tracking parameters
    double ema_alpha = 0.8;                      ///< EMA smoothing factor
    int likelihood_history_size = 100;           ///< History buffer size
    
    // Hardware timing
    int settling_time_ms = 40;                   ///< PLL lock settling time
    int capture_blocks = 15;                     ///< Blocks per frequency
    
    // Persistence
    std::string model_file = "rf_scene_model.xml";
    std::string spectrum_file = "rf_spectrum_map.xml";
};

// ============================================================================
// DATA BUFFER STRUCTURES
// ============================================================================

/**
 * @struct IQDataBuffer
 * @brief Container for IQ samples with metadata
 */
struct IQDataBuffer {
    FrequencyHz frequency_hz;                    ///< Center frequency
    std::vector<IQSample> iq_samples;            ///< IQ data
    uint64_t timestamp_ms;                       ///< Capture timestamp
    
    IQDataBuffer() : frequency_hz(0), timestamp_ms(0) {}
    
    explicit IQDataBuffer(FrequencyHz freq) 
        : frequency_hz(freq), timestamp_ms(0) {}
};

// ============================================================================
// TEMPORAL STATE STRUCTURES
// ============================================================================

/**
 * @struct TemporalState
 * @brief Temporal state tracker for each frequency
 * @details Maintains history and computes adaptive thresholds using MAD
 */
struct TemporalState {
    double ema_likelihood = 0.0;                 ///< Exponential moving average
    double occupancy_probability = 0.0;          ///< Signal occupancy [0.0-1.0]
    int burst_counter = 0;                       ///< Burst detection counter
    double adaptive_noise_floor = 0.0;           ///< Median-based noise floor
    double adaptive_noise_sigma = 1.0;           ///< MAD-based noise variance
    std::deque<double> likelihood_history;       ///< Log-likelihood history
    
    /**
     * @brief Update likelihood history buffer
     * @param likelihood New log-likelihood value
     */
    void updateHistory(double likelihood);
    
    /**
     * @brief Compute robust noise floor using Median and MAD
     * @details Uses Median Absolute Deviation for robust statistics
     */
    void computeRobustNoiseFloor();
};

// ============================================================================
// DETECTION RESULT STRUCTURES
// ============================================================================

/**
 * @struct DetectedSignal
 * @brief Complete signal detection result
 */
struct DetectedSignal {
    FrequencyHz frequency_hz;                    ///< Center frequency
    SignalClass signal_class;                    ///< Classification result
    float occupancy_probability;                 ///< Occupancy ratio [0.0-1.0]
    float noise_floor_dbm;                       ///< Adaptive noise floor
    float power_dbm;                             ///< Signal power
    float confidence;                            ///< Detection confidence
    uint64_t timestamp_ms;                       ///< Detection timestamp
    std::vector<float> features;                 ///< 8-dimensional features
    
    // Direction finding & geo-location (for future integration)
    float aoa_degrees = 0.0f;                    ///< Angle of Arrival
    double target_lat = 0.0;                     ///< Target latitude
    double target_lon = 0.0;                     ///< Target longitude
    
    DetectedSignal() 
        : frequency_hz(0), signal_class(SignalClass::NOISE),
          occupancy_probability(0.0f), noise_floor_dbm(0.0f),
          power_dbm(0.0f), confidence(0.0f), timestamp_ms(0) {}
};

/**
 * @struct SpectrumBin
 * @brief Spectrum map entry for visualization
 */
struct SpectrumBin {
    FrequencyHz frequency_hz;                    ///< Center frequency
    float occupancy_probability;                 ///< Occupancy [0.0-1.0]
    float noise_floor_dbm;                       ///< Noise floor estimate
    SignalClass activity_class;                  ///< Current classification
    float last_power_dbm;                        ///< Last measured power
    
    // Direction finding & geo-location
    float aoa_degrees = 0.0f;
    double target_lat = 0.0;
    double target_lon = 0.0;
    
    SpectrumBin() 
        : frequency_hz(0), occupancy_probability(0.0f),
          noise_floor_dbm(0.0f), activity_class(SignalClass::NOISE),
          last_power_dbm(0.0f) {}
};

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

/**
 * @brief Convert SignalClass enum to string
 * @param signal_class Enum value
 * @return String representation
 */
inline const char* signalClassToString(SignalClass signal_class) {
    switch (signal_class) {
        case SignalClass::NOISE:      return "NOISE";
        case SignalClass::BURST:      return "BURST";
        case SignalClass::CONTINUOUS: return "CONTINUOUS";
        default:                      return "UNKNOWN";
    }
}

/**
 * @brief Convert HardwareState enum to string
 * @param state Enum value
 * @return String representation
 */
inline const char* hardwareStateToString(HardwareState state) {
    switch (state) {
        case HardwareState::IDLE:     return "IDLE";
        case HardwareState::SENSING:  return "SENSING";
        case HardwareState::JAMMING:  return "JAMMING";
        case HardwareState::SPOOFING: return "SPOOFING";
        default:                      return "UNKNOWN";
    }
}

} // namespace SignalDetection
} // namespace CognitiveRF

#endif // SIGNAL_DETECTION_DATA_STRUCTURES_HPP
