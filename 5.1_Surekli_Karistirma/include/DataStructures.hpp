/**
 * @file DataStructures.hpp
 * @brief Data structures for Spot Jamming (Sürekli Karıştırma) module
 * @details Type definitions, enumerations, and configuration structures for
 *          continuous RF jamming operations targeting specific frequencies.
 * 
 * System: Bilişsel RF Spektrum Haritalama Sistemi
 * Module: 5.1 Sürekli Karıştırma (Spot Jamming)
 * 
 * @author YM3XBA Development Team
 * @date 2026-06-29
 */

#ifndef COGNITIVE_RF_SPOT_JAMMING_DATA_STRUCTURES_HPP
#define COGNITIVE_RF_SPOT_JAMMING_DATA_STRUCTURES_HPP

#include <complex>
#include <vector>
#include <cstdint>
#include <string>

namespace CognitiveRF {
namespace SpotJamming {

// ============================================================================
// TYPE ALIASES
// ============================================================================

using IQSample = std::complex<float>;
using IQBuffer = std::vector<IQSample>;

// ============================================================================
// ENUMERATIONS
// ============================================================================

enum class JammingType : uint8_t {
    CONTINUOUS_WAVE = 0,
    GAUSSIAN_NOISE = 1,
    MODULATED_NOISE = 2
};

enum class HardwareState : int {
    IDLE = 0,
    SENSING = 1,
    JAMMING = 2
};

enum class JammingStatus : uint8_t {
    IDLE = 0,
    STARTING = 1,
    ACTIVE = 2,
    STOPPING = 3,
    ERROR_STATE = 4
};

// ============================================================================
// CONFIGURATION STRUCTURES
// ============================================================================

struct HardwareConfig {
    double center_freq_hz = 433.92e6;
    double sample_rate_hz = 20e6;
    double bandwidth_hz = 25e3;
    
    uint32_t lna_gain_db = 0;
    uint32_t vga_gain_db = 20;
    bool tx_amp_enable = true;
    
    float max_duty_cycle = 0.9f;
    float thermal_limit_c = 80.0f;
    
    uint32_t tx_buffer_samples = 8192;
    
    bool validate() const {
        // ET (TX) Mode: 1 MHz - 3 GHz (System specification for jamming operations)
        // Note: HackRF One supports up to 4 GHz TX and 6 GHz RX, but ET tasks limited to 3 GHz
        if (center_freq_hz < 1e6 || center_freq_hz > 3e9) return false;
        if (bandwidth_hz <= 0 || bandwidth_hz > sample_rate_hz) return false;
        if (lna_gain_db > 40 || vga_gain_db > 62) return false;
        if (max_duty_cycle < 0.0f || max_duty_cycle > 1.0f) return false;
        if (tx_buffer_samples < 1024 || (tx_buffer_samples & (tx_buffer_samples - 1)) != 0) {
            return false;
        }
        return true;
    }
};

struct SignalConfig {
    JammingType type = JammingType::GAUSSIAN_NOISE;
    
    double cw_offset_hz = 0.0;
    float cw_amplitude = 0.8f;
    
    float noise_amplitude = 0.5f;
    uint32_t noise_seed = 0;
    
    double modulation_rate_hz = 1000.0;
    
    bool validate() const {
        if (cw_amplitude < 0.0f || cw_amplitude > 1.0f) return false;
        if (noise_amplitude < 0.0f || noise_amplitude > 1.0f) return false;
        if (modulation_rate_hz <= 0.0) return false;
        return true;
    }
};

struct PipelineConfig {
    HardwareConfig hardware;
    SignalConfig signal;
    
    uint32_t queue_max_size = 100;
    uint32_t processing_timeout_ms = 1000;
    
    bool enable_thermal_monitoring = true;
    bool enable_duty_cycle_limit = true;
    
    bool enable_logging = true;
    std::string log_prefix = "[SpotJamming]";
    
    bool validate() const {
        return hardware.validate() && signal.validate();
    }
};

// ============================================================================
// DATA TRANSFER STRUCTURES
// ============================================================================

struct TargetIntelligence {
    uint64_t target_id = 0;
    double center_freq_hz = 0.0;
    double bandwidth_hz = 0.0;
    uint64_t timestamp_ms = 0;
    
    float rssi_dbm = -100.0f;
    float snr_db = 0.0f;
    
    uint8_t priority = 0;
    
    bool isValid() const {
        return (target_id > 0) && 
               (center_freq_hz >= 1e6 && center_freq_hz <= 3e9) &&
               (bandwidth_hz > 0);
    }
};

struct JammingCommand {
    enum class Action : uint8_t {
        START = 0,
        STOP = 1,
        PAUSE = 2,
        RESUME = 3
    };
    
    Action action = Action::START;
    TargetIntelligence target;
    uint32_t duration_ms = 0;
    
    bool isValid() const {
        if (action == Action::START) {
            return target.isValid();
        }
        return true;
    }
};

struct JammingReport {
    uint64_t timestamp_ms = 0;
    JammingStatus status = JammingStatus::IDLE;
    HardwareState hw_state = HardwareState::IDLE;
    
    uint64_t target_id = 0;
    double active_freq_hz = 0.0;
    double active_bandwidth_hz = 0.0;
    
    uint64_t samples_transmitted = 0;
    uint32_t tx_errors = 0;
    float duty_cycle_current = 0.0f;
    
    float temperature_c = 0.0f;
    bool thermal_limit_reached = false;
    
    uint64_t jamming_start_ms = 0;
    uint64_t jamming_duration_ms = 0;
    
    bool isActive() const {
        return (status == JammingStatus::ACTIVE) && 
               (hw_state == HardwareState::JAMMING);
    }
};

namespace Constants {
    constexpr double PI = 3.14159265358979323846;
    constexpr double TWO_PI = 2.0 * PI;
    constexpr double SPEED_OF_LIGHT = 299792458.0;
}

} // namespace SpotJamming
} // namespace CognitiveRF

#endif // COGNITIVE_RF_SPOT_JAMMING_DATA_STRUCTURES_HPP
