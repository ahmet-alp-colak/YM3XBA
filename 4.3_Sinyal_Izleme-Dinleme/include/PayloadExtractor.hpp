/**
 * @file PayloadExtractor.hpp
 * @brief Protocol analysis and payload extraction
 * @details Extracts communication data from demodulated bitstreams
 * 
 * System: Bilişsel RF Spektrum Haritalama Sistemi
 * Module: 4.3 Sinyal İzleme/Dinleme
 * 
 * Reference: pipeline.txt Section [17] - Payload Extraction
 * Reference: 4.3_SinyalDinleme-Izleme.txt Section 4.3.3
 * 
 * Supported Protocols:
 * - DMR (Digital Mobile Radio)
 * - TETRA (Terrestrial Trunked Radio)
 * - AX.25 (Amateur Packet Radio)
 * - APRS (Automatic Packet Reporting System)
 * - ISM band protocols
 * 
 * @author Senior C++ Software Architect
 * @date 2026
 */

#ifndef PAYLOAD_EXTRACTOR_HPP
#define PAYLOAD_EXTRACTOR_HPP

#include "DataStructures.hpp"
#include <vector>
#include <string>
#include <optional>

namespace CognitiveRF {
namespace SignalMonitoring {

/**
 * @class PayloadExtractor
 * @brief Extracts payload from demodulated bitstreams
 * 
 * Features:
 * - Frame synchronization
 * - CRC validation
 * - FEC (Forward Error Correction)
 * - Protocol-specific decoding
 * - Metadata extraction
 */
class PayloadExtractor {
public:
    /**
     * @struct ExtractorConfig
     * @brief Configuration for payload extraction
     */
    struct ExtractorConfig {
        bool enable_fec = true;
        bool enable_crc_check = true;
        uint32_t max_payload_size = 4096;
        uint32_t frame_sync_threshold = 3;  // Consecutive valid frames
    };

    /**
     * @brief Constructor
     * @param config Extractor configuration
     */
    explicit PayloadExtractor(const ExtractorConfig& config = ExtractorConfig());
    
    /**
     * @brief Extract payload from bitstream
     * @param bitstream Demodulated bitstream
     * @param protocol_hint Protocol name hint (optional)
     * @return Extracted payload or nullopt if extraction failed
     */
    std::optional<ExtractedPayload> extractPayload(
        const DemodulatedBitstream& bitstream,
        const std::string& protocol_hint = ""
    );
    
    /**
     * @brief Extract DMR payload
     * @param bits Raw bit sequence
     * @return Extracted payload
     */
    std::optional<ExtractedPayload> extractDMR(const std::vector<uint8_t>& bits);
    
    /**
     * @brief Extract TETRA payload
     * @param bits Raw bit sequence
     * @return Extracted payload
     */
    std::optional<ExtractedPayload> extractTETRA(const std::vector<uint8_t>& bits);
    
    /**
     * @brief Extract AX.25 payload
     * @param bits Raw bit sequence
     * @return Extracted payload
     */
    std::optional<ExtractedPayload> extractAX25(const std::vector<uint8_t>& bits);
    
    /**
     * @brief Extract APRS payload
     * @param bits Raw bit sequence
     * @return Extracted payload
     */
    std::optional<ExtractedPayload> extractAPRS(const std::vector<uint8_t>& bits);
    
    /**
     * @brief Generic ISM packet extraction
     * @param bits Raw bit sequence
     * @return Extracted payload
     */
    std::optional<ExtractedPayload> extractISM(const std::vector<uint8_t>& bits);
    
    /**
     * @brief Get extraction statistics
     */
    struct Statistics {
        uint32_t total_attempts;
        uint32_t successful_extractions;
        uint32_t crc_failures;
        uint32_t fec_corrections;
        float success_rate;
    };
    
    Statistics getStatistics() const;
    
    /**
     * @brief Reset statistics
     */
    void resetStatistics();

private:
    /**
     * @brief Find frame synchronization pattern
     * @param bits Bit sequence
     * @param sync_pattern Expected sync pattern
     * @return Index of sync pattern, or -1 if not found
     */
    int findFrameSync(
        const std::vector<uint8_t>& bits,
        const std::vector<uint8_t>& sync_pattern
    ) const;
    
    /**
     * @brief Validate CRC
     * @param data Data bytes
     * @param crc_type CRC algorithm type
     * @return True if CRC valid
     */
    bool validateCRC(
        const std::vector<uint8_t>& data,
        const std::string& crc_type
    ) const;
    
    /**
     * @brief Apply FEC decoding
     * @param encoded Encoded data
     * @param fec_type FEC algorithm type
     * @return Decoded data and number of corrected errors
     */
    std::pair<std::vector<uint8_t>, uint32_t> applyFEC(
        const std::vector<uint8_t>& encoded,
        const std::string& fec_type
    ) const;
    
    /**
     * @brief Decode text from payload
     * @param payload Raw payload bytes
     * @param encoding Encoding type (ASCII, UTF-8, etc.)
     * @return Decoded text
     */
    std::string decodeText(
        const std::vector<uint8_t>& payload,
        const std::string& encoding = "ASCII"
    ) const;
    
    /**
     * @brief Extract metadata from protocol-specific fields
     * @param payload Payload data
     * @param protocol Protocol name
     * @return Metadata map
     */
    std::map<std::string, std::string> extractMetadata(
        const std::vector<uint8_t>& payload,
        const std::string& protocol
    ) const;

    // Configuration
    ExtractorConfig config_;
    
    // Statistics
    mutable uint32_t total_attempts_;
    mutable uint32_t successful_extractions_;
    mutable uint32_t crc_failures_;
    mutable uint32_t fec_corrections_;
};

} // namespace SignalMonitoring
} // namespace CognitiveRF

#endif // PAYLOAD_EXTRACTOR_HPP
