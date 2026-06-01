/**
 * @file ProtocolAnalyzer.hpp
 * @brief Rule-based protocol and multiplexing identification
 * @details Decision tree for inferring communication protocols from signal parameters
 * 
 * Input Parameters:
 * - Modulation type (from AI classifier)
 * - Signal class (Burst/Continuous from temporal state engine)
 * - Bandwidth
 * - Frequency tracking history
 * 
 * Output:
 * - Protocol type (DMR, TETRA, GSM, etc.)
 * - Multiplexing method (TDMA, FDMA, CSMA)
 * - ECCM flags (FHSS detection)
 * 
 * References:
 * - Section 4.2.3: Karar Ağacı Tabanlı Protokol ve Çoklama Analizi
 * - Pipeline Stage [17]: Protokol ve Çoklama Analizi
 * 
 * Decision Rules:
 * - DMR: 4FSK + Burst (TDMA) + BW=12.5kHz
 * - GSM: GMSK + Burst (TDMA)
 * - TETRA: QPSK + Continuous (FDMA) + BW=25kHz
 * - FHSS: Burst + Stable power + Changing frequency
 * 
 * @author Senior C++ Software Architect
 * @date 2026
 */

#ifndef PROTOCOL_ANALYZER_HPP
#define PROTOCOL_ANALYZER_HPP

#include "DataStructures.hpp"
#include <vector>
#include <deque>
#include <cmath>

namespace CognitiveRF {

/**
 * @struct ProtocolAnalysisInput
 * @brief Input parameters for protocol analysis
 */
struct ProtocolAnalysisInput {
    ModulationType modulation;
    SignalClass signal_class;
    double bandwidth_hz;
    double carrier_frequency_hz;
    float rssi_dbm;
    
    // Optional: frequency tracking history for FHSS detection
    std::vector<double> frequency_history;
};

/**
 * @struct ProtocolAnalysisResult
 * @brief Output of protocol analysis
 */
struct ProtocolAnalysisResult {
    ProtocolType protocol;
    MultiplexingType multiplexing;
    bool fhss_suspected;
    bool jamming_suspected;
    float confidence;  ///< Rule matching confidence [0.0, 1.0]
};

/**
 * @class ProtocolAnalyzer
 * @brief Rule-based decision tree for protocol identification
 * 
 * Architecture:
 * 1. Multiplexing inference (from signal class)
 * 2. Protocol matching (decision tree with bandwidth/modulation rules)
 * 3. ECCM detection (FHSS pattern recognition)
 */
class ProtocolAnalyzer {
public:
    /**
     * @brief Constructor
     * @param fhss_detection_window Number of frequency samples to track for FHSS
     * @param fhss_hop_threshold Minimum frequency change to consider as hop (Hz)
     */
    explicit ProtocolAnalyzer(
        size_t fhss_detection_window = 10,
        double fhss_hop_threshold = 1e6  // 1 MHz
    );

    /**
     * @brief Analyze signal and infer protocol
     * @param input Signal parameters
     * @return Protocol analysis result
     */
    ProtocolAnalysisResult analyze(const ProtocolAnalysisInput& input);

    /**
     * @brief Update frequency tracking for FHSS detection
     * @param frequency Current carrier frequency
     * @param power Current signal power
     * 
     * @details Maintains history of frequency observations to detect
     * frequency hopping patterns (stable power, changing frequency)
     */
    void updateFrequencyTracking(double frequency, float power);

    /**
     * @brief Reset frequency tracking history
     */
    void resetTracking() {
        frequency_history_.clear();
        power_history_.clear();
    }

    /**
     * @brief Check if FHSS pattern is detected
     * @return true if frequency hopping suspected
     */
    bool isFHSSDetected() const;

private:
    // FHSS detection parameters
    size_t fhss_detection_window_;
    double fhss_hop_threshold_;
    
    // Tracking history
    std::deque<double> frequency_history_;
    std::deque<float> power_history_;
    
    /**
     * @brief Infer multiplexing method from signal class
     * @param signal_class Burst (Class 1) or Continuous (Class 2)
     * @return Multiplexing type
     * 
     * Rules:
     * - Burst (Class 1) -> TDMA or CSMA
     * - Continuous (Class 2) -> FDMA
     */
    MultiplexingType inferMultiplexing(SignalClass signal_class) const;

    /**
     * @brief Match protocol using decision tree
     * @param modulation Detected modulation
     * @param signal_class Signal temporal behavior
     * @param bandwidth_hz Signal bandwidth
     * @param multiplexing Inferred multiplexing
     * @return Protocol type and confidence
     */
    std::pair<ProtocolType, float> matchProtocol(
        ModulationType modulation,
        SignalClass signal_class,
        double bandwidth_hz,
        MultiplexingType multiplexing
    ) const;

    /**
     * @brief Detect FHSS from frequency tracking history
     * @return true if FHSS pattern detected
     * 
     * Criteria:
     * - Signal class is Burst (Class 1)
     * - Power remains stable (low variance)
     * - Frequency changes significantly between observations
     */
    bool detectFHSS() const;

    /**
     * @brief Check if bandwidth matches expected value (with tolerance)
     * @param actual_bw Measured bandwidth
     * @param expected_bw Expected bandwidth for protocol
     * @param tolerance_percent Tolerance percentage (default 20%)
     * @return true if match
     */
    bool bandwidthMatches(
        double actual_bw,
        double expected_bw,
        double tolerance_percent = 0.2
    ) const;

    /**
     * @brief Compute variance of power history
     * @return Power variance (for FHSS stability check)
     */
    float computePowerVariance() const;

    /**
     * @brief Count significant frequency hops in history
     * @return Number of hops exceeding threshold
     */
    size_t countFrequencyHops() const;
};

} // namespace CognitiveRF

#endif // PROTOCOL_ANALYZER_HPP
