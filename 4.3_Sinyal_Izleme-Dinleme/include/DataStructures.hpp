/**
 * @file DataStructures.hpp
 * @brief Core data structures for Signal Monitoring/Listening System
 * @details Defines structures for demodulation, tracking, and payload extraction
 * 
 * System: Bilişsel RF Spektrum Haritalama Sistemi
 * Module: 4.3 Sinyal İzleme/Dinleme (Signal Monitoring/Listening)
 * 
 * References:
 * - pipeline.txt: Sections [14-17] (Demodulation & Payload Extraction)
 * - 4.3_SinyalDinleme-Izleme.txt: Full technical specifications
 * 
 * @author Senior C++ Software Architect
 * @date 2026
 */

#ifndef SIGNAL_MONITORING_DATA_STRUCTURES_HPP
#define SIGNAL_MONITORING_DATA_STRUCTURES_HPP

#include <complex>
#include <vector>
#include <string>
#include <cstdint>
#include <optional>
#include <deque>
#include <map>

namespace CognitiveRF {
namespace SignalMonitoring {

/**
 * @enum ModulationType
 * @brief Supported modulation types for demodulation
 */
enum class ModulationType : uint8_t {
    UNKNOWN = 0,
    // Analog
    AM,
    FM,
    // Digital PSK
    BPSK,
    QPSK,
    OQPSK,
    PSK8,
    // Digital QAM
    QAM16,
    QAM64,
    // Digital FSK
    FSK2,
    FSK4,
    GMSK,
    GFSK,
    MSK,
    // Multi-carrier
    OFDM
};

/**
 * @enum SignalClass
 * @brief Signal temporal behavior from detection module
 */
enum class SignalClass : uint8_t {
    NOISE = 0,
    BURST = 1,
    CONTINUOUS = 2
};

/**
 * @struct TrackedSignal
 * @brief State space for signal tracking and FHSS detection
 * @details Maintains temporal history for each tracked signal
 * 
 * Reference: pipeline.txt Section [7] - Signal Tracking
 */
struct TrackedSignal {
    uint32_t tracking_id;
    uint64_t first_seen_us;
    uint64_t last_seen_us;
    
    // Frequency history for FHSS detection
    std::deque<double> frequency_history;
    std::deque<uint64_t> frequency_timestamps;
    
    // Power and bandwidth tracking
    std::deque<float> power_history;
    float avg_power_dbm;
    float avg_bandwidth_hz;
    
    // Occupancy tracking
    uint32_t detection_count;
    float occupancy_ratio;  // 0.0 to 1.0
    
    // FHSS detection state
    bool fhss_suspected;
    uint32_t frequency_hop_count;
    float hop_rate_hz;  // Hops per second
    
    // Signal classification
    SignalClass signal_class;
    ModulationType modulation;
    
    TrackedSignal() 
        : tracking_id(0)
        , first_seen_us(0)
        , last_seen_us(0)
        , avg_power_dbm(-120.0f)
        , avg_bandwidth_hz(0.0f)
        , detection_count(0)
        , occupancy_ratio(0.0f)
        , fhss_suspected(false)
        , frequency_hop_count(0)
        , hop_rate_hz(0.0f)
        , signal_class(SignalClass::NOISE)
        , modulation(ModulationType::UNKNOWN)
    {}
};

/**
 * @struct DemodulatedAudio
 * @brief PCM audio output from analog demodulation
 * @details Used for AM/FM demodulation output
 * 
 * Reference: pipeline.txt Section [15] - Analog Demodulation
 */
struct DemodulatedAudio {
    std::vector<float> pcm_samples;  // Mono audio, normalized [-1.0, 1.0]
    uint32_t sample_rate_hz;         // Audio sample rate (typically 48kHz)
    uint64_t timestamp_us;
    ModulationType source_modulation;
    float audio_snr_db;
    
    DemodulatedAudio()
        : sample_rate_hz(48000)
        , timestamp_us(0)
        , source_modulation(ModulationType::UNKNOWN)
        , audio_snr_db(0.0f)
    {}
};

/**
 * @struct DemodulatedBitstream
 * @brief Raw bitstream from digital demodulation
 * @details Output from BPSK/QPSK/FSK/OFDM demodulators
 * 
 * Reference: pipeline.txt Section [16] - Digital Demodulation
 */
struct DemodulatedBitstream {
    std::vector<uint8_t> bits;       // Raw bit sequence
    uint32_t bit_rate_bps;           // Bits per second
    uint64_t timestamp_us;
    ModulationType source_modulation;
    float bit_error_estimate;        // Estimated BER [0.0, 1.0]
    bool sync_locked;                // Carrier/timing sync status
    
    DemodulatedBitstream()
        : bit_rate_bps(0)
        , timestamp_us(0)
        , source_modulation(ModulationType::UNKNOWN)
        , bit_error_estimate(0.0f)
        , sync_locked(false)
    {}
};

/**
 * @struct ExtractedPayload
 * @brief Decoded payload after protocol analysis
 * @details Contains actual communication data after FEC/CRC
 * 
 * Reference: pipeline.txt Section [17] - Payload Extraction
 */
struct ExtractedPayload {
    std::vector<uint8_t> payload_data;
    std::string protocol_name;       // DMR, TETRA, AX.25, APRS, etc.
    bool crc_valid;
    bool fec_corrected;
    uint32_t corrected_errors;
    
    // Optional decoded content
    std::optional<std::string> decoded_text;
    std::optional<std::map<std::string, std::string>> metadata;
    
    // Geolocation data (if available)
    float aoa_degrees;               // Angle of Arrival
    double target_latitude;
    double target_longitude;
    
    uint64_t timestamp_us;
    
    ExtractedPayload()
        : crc_valid(false)
        , fec_corrected(false)
        , corrected_errors(0)
        , aoa_degrees(0.0f)
        , target_latitude(0.0)
        , target_longitude(0.0)
        , timestamp_us(0)
    {}
};

/**
 * @struct MonitoredSignal
 * @brief Complete signal monitoring result
 * @details Aggregates tracking, demodulation, and payload extraction
 */
struct MonitoredSignal {
    // Identification
    uint32_t tracking_id;
    uint64_t timestamp_us;
    
    // RF Parameters
    double carrier_frequency_hz;
    double bandwidth_hz;
    float rssi_dbm;
    float snr_db;
    
    // Classification
    ModulationType modulation;
    SignalClass signal_class;
    std::string protocol;
    
    // Tracking state
    bool fhss_suspected;
    float occupancy_ratio;
    
    // Demodulation results
    std::optional<DemodulatedAudio> audio;
    std::optional<DemodulatedBitstream> bitstream;
    std::optional<ExtractedPayload> payload;
    
    // Confidence
    float confidence;
    
    MonitoredSignal()
        : tracking_id(0)
        , timestamp_us(0)
        , carrier_frequency_hz(0.0)
        , bandwidth_hz(0.0)
        , rssi_dbm(-120.0f)
        , snr_db(-20.0f)
        , modulation(ModulationType::UNKNOWN)
        , signal_class(SignalClass::NOISE)
        , fhss_suspected(false)
        , occupancy_ratio(0.0f)
        , confidence(0.0f)
    {}
};

/**
 * @struct IQDataBuffer
 * @brief Container for raw IQ samples with metadata
 */
struct IQDataBuffer {
    uint64_t center_freq_hz;
    uint32_t sample_rate_hz;
    uint64_t timestamp_us;
    std::vector<std::complex<float>> iq_data;
    
    // Additional metadata from detection/parameter extraction
    ModulationType detected_modulation;
    SignalClass signal_class;
    float snr_db;
    uint32_t tracking_id;
    
    IQDataBuffer()
        : center_freq_hz(0)
        , sample_rate_hz(0)
        , timestamp_us(0)
        , detected_modulation(ModulationType::UNKNOWN)
        , signal_class(SignalClass::NOISE)
        , snr_db(-20.0f)
        , tracking_id(0)
    {}
};

/**
 * @brief Convert ModulationType to string
 */
inline const char* ModulationTypeToString(ModulationType mod) {
    switch (mod) {
        case ModulationType::AM: return "AM";
        case ModulationType::FM: return "FM";
        case ModulationType::BPSK: return "BPSK";
        case ModulationType::QPSK: return "QPSK";
        case ModulationType::OQPSK: return "OQPSK";
        case ModulationType::PSK8: return "8PSK";
        case ModulationType::QAM16: return "QAM16";
        case ModulationType::QAM64: return "QAM64";
        case ModulationType::FSK2: return "2FSK";
        case ModulationType::FSK4: return "4FSK";
        case ModulationType::GMSK: return "GMSK";
        case ModulationType::GFSK: return "GFSK";
        case ModulationType::MSK: return "MSK";
        case ModulationType::OFDM: return "OFDM";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Convert SignalClass to string
 */
inline const char* SignalClassToString(SignalClass cls) {
    switch (cls) {
        case SignalClass::NOISE: return "NOISE";
        case SignalClass::BURST: return "BURST";
        case SignalClass::CONTINUOUS: return "CONTINUOUS";
        default: return "UNKNOWN";
    }
}

} // namespace SignalMonitoring
} // namespace CognitiveRF

#endif // SIGNAL_MONITORING_DATA_STRUCTURES_HPP
