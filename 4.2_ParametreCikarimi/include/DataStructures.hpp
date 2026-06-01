/**
 * @file DataStructures.hpp
 * @brief Core data structures for Cognitive RF Spectrum Mapping System
 * @details Defines signal representation structures used across all modules
 * 
 * System: Bilişsel RF Spektrum Haritalama Sistemi
 * Module: 4.2 Parametre Çıkarımı (Parameter Extraction)
 * 
 * @author Senior C++ Software Architect
 * @date 2026
 */

#ifndef DATA_STRUCTURES_HPP
#define DATA_STRUCTURES_HPP

#include <complex>
#include <vector>
#include <string>
#include <cstdint>
#include <optional>

namespace CognitiveRF {

/**
 * @enum ModulationType
 * @brief Supported modulation types detected by AI classifier
 */
enum class ModulationType : uint8_t {
    UNKNOWN = 0,
    AM,
    FM,
    BPSK,
    QPSK,
    OQPSK,
    PSK8,
    QAM16,
    QAM64,
    FSK2,
    FSK4,
    GMSK,
    OFDM,
    GFSK,
    MSK
};

/**
 * @enum ProtocolType
 * @brief Communication protocol identification
 */
enum class ProtocolType : uint8_t {
    UNKNOWN = 0,
    DMR,
    TETRA,
    GSM,
    P25,
    DPMR,
    NXDN,
    APCO_P25,
    LTE,
    WIFI,
    BLUETOOTH,
    ZIGBEE,
    CUSTOM
};

/**
 * @enum MultiplexingType
 * @brief Multiple access / multiplexing method
 */
enum class MultiplexingType : uint8_t {
    UNKNOWN = 0,
    TDMA,      ///< Time Division Multiple Access
    FDMA,      ///< Frequency Division Multiple Access
    CDMA,      ///< Code Division Multiple Access
    CSMA,      ///< Carrier Sense Multiple Access
    OFDMA      ///< Orthogonal FDMA
};

/**
 * @enum SignalClass
 * @brief Signal temporal behavior classification
 */
enum class SignalClass : uint8_t {
    NOISE = 0,        ///< Background noise (Class 0)
    BURST = 1,        ///< Burst/Transient signal (Class 1)
    CONTINUOUS = 2    ///< Continuous signal (Class 2)
};

/**
 * @struct DecodedSignal
 * @brief Complete signal parameter extraction result
 * @details This structure carries all extracted parameters across system modules
 * 
 * Used by:
 * - ModulationClassifier (AI output)
 * - ProtocolAnalyzer (protocol inference)
 * - DSPEngine (physical measurements)
 * - ParameterExtractionPipeline (orchestration)
 * - UI Thread (visualization)
 */
struct DecodedSignal {
    // Frequency Domain Parameters
    double carrier_frequency_hz;     ///< Center frequency in Hz (CFO corrected)
    double bandwidth_hz;             ///< Occupied bandwidth in Hz
    
    // Power Measurements
    float rssi_dbm;                  ///< Received Signal Strength Indicator (dBm)
    float snr_db;                    ///< Signal-to-Noise Ratio (dB)
    
    // Digital Parameters
    float baud_rate_hz;              ///< Symbol rate for digital signals (0 for analog)
    
    // Classification Results
    ModulationType modulation;       ///< Detected modulation type
    ProtocolType protocol;           ///< Inferred protocol
    MultiplexingType multiplexing;   ///< Multiple access method
    SignalClass signal_class;        ///< Temporal behavior class
    
    // Confidence and Metadata
    float confidence;                ///< Classification confidence [0.0, 1.0]
    uint64_t timestamp_us;           ///< Detection timestamp (microseconds since epoch)
    uint32_t tracking_id;            ///< Unique ID for signal tracking
    
    // ECCM (Electronic Counter-Counter Measures) Flags
    bool fhss_suspected;             ///< Frequency Hopping Spread Spectrum detected
    bool jamming_suspected;          ///< Jamming/interference detected
    
    // Optional Payload (for decoded signals)
    std::optional<std::vector<uint8_t>> payload;  ///< Demodulated bitstream
    std::optional<std::string> decoded_text;      ///< Human-readable payload
    
    /**
     * @brief Default constructor with safe initialization
     */
    DecodedSignal() 
        : carrier_frequency_hz(0.0)
        , bandwidth_hz(0.0)
        , rssi_dbm(-120.0f)
        , snr_db(-20.0f)
        , baud_rate_hz(0.0f)
        , modulation(ModulationType::UNKNOWN)
        , protocol(ProtocolType::UNKNOWN)
        , multiplexing(MultiplexingType::UNKNOWN)
        , signal_class(SignalClass::NOISE)
        , confidence(0.0f)
        , timestamp_us(0)
        , tracking_id(0)
        , fhss_suspected(false)
        , jamming_suspected(false)
    {}
};

/**
 * @struct IQDataBuffer
 * @brief Container for raw IQ samples with metadata
 * @details Used in SPSC lock-free queue for zero-copy data transfer
 */
struct IQDataBuffer {
    uint64_t center_freq_hz;                    ///< SDR center frequency
    uint32_t sample_rate_hz;                    ///< Sampling rate
    uint64_t timestamp_us;                      ///< Capture timestamp
    std::vector<std::complex<float>> iq_data;   ///< I/Q samples
    
    /**
     * @brief Constructor with pre-allocation
     * @param freq Center frequency in Hz
     * @param rate Sample rate in Hz
     * @param capacity Pre-allocated buffer size
     */
    IQDataBuffer(uint64_t freq = 0, uint32_t rate = 0, size_t capacity = 0)
        : center_freq_hz(freq)
        , sample_rate_hz(rate)
        , timestamp_us(0)
    {
        if (capacity > 0) {
            iq_data.reserve(capacity);
        }
    }
};

/**
 * @struct SignalRecord
 * @brief Database persistence structure
 * @details Used for logging detected signals to SQLite/PostgreSQL
 *
 * Extended with Direction Finding and Geolocation capabilities:
 * - aoa: Angle of Arrival from Pseudo-Doppler DF system (Section [8])
 * - target_lat/target_lon: Cross-fixing geolocation (Section [9])
 */
struct SignalRecord {
    uint64_t timestamp_us;
    double frequency_hz;
    double bandwidth_hz;
    int activity_class;
    int modulation_type;
    std::string protocol_name;
    float aoa;                    ///< Angle of Arrival in degrees [NEW - Pipeline V1.1.0]
    double target_lat;            ///< Target latitude [NEW - Pipeline V1.1.0]
    double target_lon;            ///< Target longitude [NEW - Pipeline V1.1.0]
    std::string payload_hex;
    float confidence;
    float rssi_dbm;
    float snr_db;
};

/**
 * @brief Convert ModulationType enum to string
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
        case ModulationType::OFDM: return "OFDM";
        case ModulationType::GFSK: return "GFSK";
        case ModulationType::MSK: return "MSK";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Convert ProtocolType enum to string
 */
inline const char* ProtocolTypeToString(ProtocolType proto) {
    switch (proto) {
        case ProtocolType::DMR: return "DMR";
        case ProtocolType::TETRA: return "TETRA";
        case ProtocolType::GSM: return "GSM";
        case ProtocolType::P25: return "P25";
        case ProtocolType::DPMR: return "DPMR";
        case ProtocolType::NXDN: return "NXDN";
        case ProtocolType::APCO_P25: return "APCO_P25";
        case ProtocolType::LTE: return "LTE";
        case ProtocolType::WIFI: return "WiFi";
        case ProtocolType::BLUETOOTH: return "Bluetooth";
        case ProtocolType::ZIGBEE: return "ZigBee";
        case ProtocolType::CUSTOM: return "CUSTOM";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Convert MultiplexingType enum to string
 */
inline const char* MultiplexingTypeToString(MultiplexingType mux) {
    switch (mux) {
        case MultiplexingType::TDMA: return "TDMA";
        case MultiplexingType::FDMA: return "FDMA";
        case MultiplexingType::CDMA: return "CDMA";
        case MultiplexingType::CSMA: return "CSMA";
        case MultiplexingType::OFDMA: return "OFDMA";
        default: return "UNKNOWN";
    }
}

} // namespace CognitiveRF

#endif // DATA_STRUCTURES_HPP
