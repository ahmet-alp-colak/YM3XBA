/**
 * @file PayloadExtractor.cpp
 * @brief Implementation of protocol analysis and payload extraction
 * 
 * System: Bilişsel RF Spektrum Haritalama Sistemi
 * Module: 4.3 Sinyal İzleme/Dinleme
 * 
 * @author Senior C++ Software Architect
 * @date 2026
 */

#include "PayloadExtractor.hpp"
#include <algorithm>
#include <cstring>
#include <sstream>
#include <iomanip>

namespace CognitiveRF {
namespace SignalMonitoring {

// ============================================================================
// Constructor
// ============================================================================

PayloadExtractor::PayloadExtractor(const ExtractorConfig& config)
    : config_(config)
    , total_attempts_(0)
    , successful_extractions_(0)
    , crc_failures_(0)
    , fec_corrections_(0)
{
}

// ============================================================================
// Main Extraction Entry Point
// ============================================================================

std::optional<ExtractedPayload> PayloadExtractor::extractPayload(
    const DemodulatedBitstream& bitstream,
    const std::string& protocol_hint)
{
    total_attempts_++;
    
    if (bitstream.bits.empty()) {
        return std::nullopt;
    }
    
    // Try protocol-specific extraction based on hint
    std::optional<ExtractedPayload> result;
    
    if (protocol_hint == "DMR" || protocol_hint.empty()) {
        result = extractDMR(bitstream.bits);
        if (result) return result;
    }
    
    if (protocol_hint == "TETRA" || protocol_hint.empty()) {
        result = extractTETRA(bitstream.bits);
        if (result) return result;
    }
    
    if (protocol_hint == "AX.25" || protocol_hint.empty()) {
        result = extractAX25(bitstream.bits);
        if (result) return result;
    }
    
    if (protocol_hint == "APRS" || protocol_hint.empty()) {
        result = extractAPRS(bitstream.bits);
        if (result) return result;
    }
    
    if (protocol_hint == "ISM" || protocol_hint.empty()) {
        result = extractISM(bitstream.bits);
        if (result) return result;
    }
    
    return std::nullopt;
}

// ============================================================================
// DMR Extraction
// ============================================================================

std::optional<ExtractedPayload> PayloadExtractor::extractDMR(const std::vector<uint8_t>& bits)
{
    // DMR frame structure:
    // - Sync pattern: 48 bits
    // - Slot type: 20 bits
    // - Payload: 216 bits
    // - Total: 264 bits per burst
    
    const std::vector<uint8_t> dmr_sync = {
        0,1,1,0,1,1,0,1,0,1,0,1,0,1,1,1,
        0,1,0,1,0,0,0,0,1,0,0,1,1,0,0,0,
        1,1,1,1,0,0,1,0,1,1,0,0,1,0,0,0
    };
    
    int sync_pos = findFrameSync(bits, dmr_sync);
    if (sync_pos < 0 || sync_pos + 264 > static_cast<int>(bits.size())) {
        return std::nullopt;
    }
    
    ExtractedPayload payload;
    payload.protocol_name = "DMR";
    payload.frame_sync_found = true;
    
    // Extract payload bits (skip sync and slot type)
    size_t payload_start = sync_pos + 48 + 20;
    size_t payload_bits = 216;
    
    for (size_t i = 0; i < payload_bits && payload_start + i < bits.size(); ++i) {
        if (i % 8 == 0) {
            payload.payload_data.push_back(0);
        }
        payload.payload_data.back() |= (bits[payload_start + i] << (7 - (i % 8)));
    }
    
    // Apply FEC if enabled
    if (config_.enable_fec) {
        auto [decoded, corrections] = applyFEC(payload.payload_data, "DMR_FEC");
        payload.payload_data = decoded;
        fec_corrections_ += corrections;
    }
    
    // Validate CRC
    if (config_.enable_crc_check) {
        payload.crc_valid = validateCRC(payload.payload_data, "CRC-16");
        if (!payload.crc_valid) {
            crc_failures_++;
        }
    } else {
        payload.crc_valid = true;
    }
    
    // Extract metadata
    payload.metadata = extractMetadata(payload.payload_data, "DMR");
    
    successful_extractions_++;
    return payload;
}

// ============================================================================
// TETRA Extraction
// ============================================================================

std::optional<ExtractedPayload> PayloadExtractor::extractTETRA(const std::vector<uint8_t>& bits)
{
    // TETRA frame structure (simplified)
    // - Sync pattern: 22 bits
    // - Control: 18 bits
    // - Payload: variable
    
    const std::vector<uint8_t> tetra_sync = {
        1,1,1,0,1,0,0,0,1,1,0,1,0,1,1,1,0,0,1,0,0,0
    };
    
    int sync_pos = findFrameSync(bits, tetra_sync);
    if (sync_pos < 0 || sync_pos + 200 > static_cast<int>(bits.size())) {
        return std::nullopt;
    }
    
    ExtractedPayload payload;
    payload.protocol_name = "TETRA";
    payload.frame_sync_found = true;
    
    // Extract payload
    size_t payload_start = sync_pos + 22 + 18;
    size_t payload_bits = std::min(size_t(160), bits.size() - payload_start);
    
    for (size_t i = 0; i < payload_bits; ++i) {
        if (i % 8 == 0) {
            payload.payload_data.push_back(0);
        }
        payload.payload_data.back() |= (bits[payload_start + i] << (7 - (i % 8)));
    }
    
    // Apply FEC
    if (config_.enable_fec) {
        auto [decoded, corrections] = applyFEC(payload.payload_data, "TETRA_FEC");
        payload.payload_data = decoded;
        fec_corrections_ += corrections;
    }
    
    // Validate CRC
    payload.crc_valid = config_.enable_crc_check ? 
                        validateCRC(payload.payload_data, "CRC-16") : true;
    
    if (!payload.crc_valid) crc_failures_++;
    
    payload.metadata = extractMetadata(payload.payload_data, "TETRA");
    
    successful_extractions_++;
    return payload;
}

// ============================================================================
// AX.25 Extraction
// ============================================================================

std::optional<ExtractedPayload> PayloadExtractor::extractAX25(const std::vector<uint8_t>& bits)
{
    // AX.25 frame structure:
    // - Flag: 0x7E (01111110)
    // - Address: 112-560 bits
    // - Control: 8 bits
    // - PID: 8 bits
    // - Info: variable
    // - FCS: 16 bits
    // - Flag: 0x7E
    
    const std::vector<uint8_t> ax25_flag = {0,1,1,1,1,1,1,0};
    
    int sync_pos = findFrameSync(bits, ax25_flag);
    if (sync_pos < 0 || sync_pos + 200 > static_cast<int>(bits.size())) {
        return std::nullopt;
    }
    
    ExtractedPayload payload;
    payload.protocol_name = "AX.25";
    payload.frame_sync_found = true;
    
    // Convert bits to bytes (simplified extraction)
    size_t byte_start = (sync_pos + 8) / 8;
    size_t max_bytes = std::min(size_t(256), (bits.size() - sync_pos) / 8);
    
    for (size_t i = 0; i < max_bytes; ++i) {
        uint8_t byte = 0;
        for (int b = 0; b < 8; ++b) {
            size_t bit_idx = (byte_start * 8) + (i * 8) + b;
            if (bit_idx < bits.size()) {
                byte |= (bits[bit_idx] << (7 - b));
            }
        }
        payload.payload_data.push_back(byte);
        
        // Stop at ending flag
        if (byte == 0x7E && i > 10) break;
    }
    
    // Validate CRC (FCS)
    payload.crc_valid = config_.enable_crc_check ?
                        validateCRC(payload.payload_data, "CRC-16") : true;
    
    if (!payload.crc_valid) crc_failures_++;
    
    payload.metadata = extractMetadata(payload.payload_data, "AX.25");
    
    successful_extractions_++;
    return payload;
}

// ============================================================================
// APRS Extraction
// ============================================================================

std::optional<ExtractedPayload> PayloadExtractor::extractAPRS(const std::vector<uint8_t>& bits)
{
    // APRS uses AX.25 as transport layer
    auto ax25_result = extractAX25(bits);
    
    if (!ax25_result) {
        return std::nullopt;
    }
    
    // Convert to APRS
    ax25_result->protocol_name = "APRS";
    
    // Decode APRS text payload
    if (!ax25_result->payload_data.empty()) {
        std::string aprs_text = decodeText(ax25_result->payload_data, "ASCII");
        ax25_result->metadata["aprs_message"] = aprs_text;
    }
    
    return ax25_result;
}

// ============================================================================
// ISM Extraction
// ============================================================================

std::optional<ExtractedPayload> PayloadExtractor::extractISM(const std::vector<uint8_t>& bits)
{
    // Generic ISM packet extraction
    // Assumes simple preamble + payload + CRC structure
    
    if (bits.size() < 64) {
        return std::nullopt;
    }
    
    ExtractedPayload payload;
    payload.protocol_name = "ISM_Generic";
    payload.frame_sync_found = true;
    
    // Skip preamble (assume first 32 bits)
    size_t payload_start = 32;
    size_t payload_bits = std::min(size_t(256), bits.size() - payload_start - 16);
    
    for (size_t i = 0; i < payload_bits; ++i) {
        if (i % 8 == 0) {
            payload.payload_data.push_back(0);
        }
        payload.payload_data.back() |= (bits[payload_start + i] << (7 - (i % 8)));
    }
    
    // Simple CRC check
    payload.crc_valid = config_.enable_crc_check ?
                        validateCRC(payload.payload_data, "CRC-16") : true;
    
    if (!payload.crc_valid) crc_failures_++;
    
    successful_extractions_++;
    return payload;
}

// ============================================================================
// Statistics
// ============================================================================

PayloadExtractor::Statistics PayloadExtractor::getStatistics() const
{
    Statistics stats;
    stats.total_attempts = total_attempts_;
    stats.successful_extractions = successful_extractions_;
    stats.crc_failures = crc_failures_;
    stats.fec_corrections = fec_corrections_;
    stats.success_rate = (total_attempts_ > 0) ?
                         static_cast<float>(successful_extractions_) / total_attempts_ : 0.0f;
    return stats;
}

void PayloadExtractor::resetStatistics()
{
    total_attempts_ = 0;
    successful_extractions_ = 0;
    crc_failures_ = 0;
    fec_corrections_ = 0;
}

// ============================================================================
// Private Helper Methods
// ============================================================================

int PayloadExtractor::findFrameSync(
    const std::vector<uint8_t>& bits,
    const std::vector<uint8_t>& sync_pattern) const
{
    if (bits.size() < sync_pattern.size()) {
        return -1;
    }
    
    // Search for sync pattern
    for (size_t i = 0; i <= bits.size() - sync_pattern.size(); ++i) {
        bool match = true;
        for (size_t j = 0; j < sync_pattern.size(); ++j) {
            if (bits[i + j] != sync_pattern[j]) {
                match = false;
                break;
            }
        }
        if (match) {
            return static_cast<int>(i);
        }
    }
    
    return -1;
}

bool PayloadExtractor::validateCRC(
    const std::vector<uint8_t>& data,
    const std::string& crc_type) const
{
    if (data.size() < 3) {
        return false;
    }
    
    // Simplified CRC validation
    // In production, use proper CRC-16 or CRC-32 implementation
    
    if (crc_type == "CRC-16") {
        // CRC-16-CCITT polynomial: 0x1021
        uint16_t crc = 0xFFFF;
        
        for (size_t i = 0; i < data.size() - 2; ++i) {
            crc ^= (static_cast<uint16_t>(data[i]) << 8);
            for (int j = 0; j < 8; ++j) {
                if (crc & 0x8000) {
                    crc = (crc << 1) ^ 0x1021;
                } else {
                    crc <<= 1;
                }
            }
        }
        
        // Compare with received CRC
        uint16_t received_crc = (static_cast<uint16_t>(data[data.size()-2]) << 8) |
                                 data[data.size()-1];
        
        return (crc == received_crc);
    }
    
    // Default: assume valid
    return true;
}

std::pair<std::vector<uint8_t>, uint32_t> PayloadExtractor::applyFEC(
    const std::vector<uint8_t>& encoded,
    const std::string& fec_type) const
{
    // Simplified FEC decoding
    // In production, implement proper Hamming, Reed-Solomon, or Viterbi decoding
    
    std::vector<uint8_t> decoded = encoded;
    uint32_t corrections = 0;
    
    // Placeholder: no actual FEC correction
    // Real implementation would use fec_type to select algorithm
    
    return {decoded, corrections};
}

std::string PayloadExtractor::decodeText(
    const std::vector<uint8_t>& payload,
    const std::string& encoding) const
{
    std::string text;
    
    for (uint8_t byte : payload) {
        // Skip non-printable characters
        if (byte >= 32 && byte <= 126) {
            text += static_cast<char>(byte);
        } else if (byte == 0) {
            break; // Null terminator
        }
    }
    
    return text;
}

std::map<std::string, std::string> PayloadExtractor::extractMetadata(
    const std::vector<uint8_t>& payload,
    const std::string& protocol) const
{
    std::map<std::string, std::string> metadata;
    
    metadata["protocol"] = protocol;
    metadata["payload_size"] = std::to_string(payload.size());
    
    // Protocol-specific metadata extraction
    if (protocol == "DMR") {
        metadata["slot_type"] = "voice"; // Placeholder
        metadata["color_code"] = "1";
    }
    else if (protocol == "TETRA") {
        metadata["encryption"] = "none";
        metadata["channel_type"] = "traffic";
    }
    else if (protocol == "AX.25" || protocol == "APRS") {
        // Extract callsigns from address field (simplified)
        if (payload.size() >= 14) {
            std::string source_call;
            for (int i = 0; i < 6; ++i) {
                char c = (payload[i] >> 1) & 0x7F;
                if (c != ' ') source_call += c;
            }
            metadata["source_callsign"] = source_call;
        }
    }
    
    return metadata;
}

} // namespace SignalMonitoring
} // namespace CognitiveRF
