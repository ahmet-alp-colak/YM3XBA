/**
 * @file DataStructures.hpp
 * @brief Data structures for Look-Through Jamming (Arabakışlı Karıştırma) module
 * @details Type definitions, enumerations, and configuration structures for
 *          time-division duplexing (TDD) based jamming operations with sensing phases.
 * 
 * System: Bilişsel RF Spektrum Haritalama Sistemi
 * Module: 5.2 Arabakışlı Karıştırma (Look-Through Jamming)
 * 
 * @author YM3XBA Development Team
 * @date 2026-07-12
 */

#ifndef COGNITIVE_RF_LOOK_THROUGH_JAMMING_DATA_STRUCTURES_HPP
#define COGNITIVE_RF_LOOK_THROUGH_JAMMING_DATA_STRUCTURES_HPP

#include <complex>
#include <vector>
#include <cstdint>
#include <string>
#include <atomic>

namespace CognitiveRF {
namespace LookThroughJamming {

// ============================================================================
// TYPE ALIASES
// ============================================================================

using IQSample = std::complex<float>;
using IQBuffer = std::vector<IQSample>;

// ============================================================================
// ENUMERATIONS
// ============================================================================

/**
 * @brief Hardware state machine for Half-Duplex TDD operation
 * 
 * State transitions:
 * IDLE -> SENSING (RX mode activated for energy detection)
 * SENSING -> JAMMING (Target detected, switch to TX)
 * JAMMING -> SENSING (Jamming pulse complete, return to sensing)
 * SENSING -> IDLE (Target inactive, return to idle)
 * JAMMING -> IDLE (Operator abort)
 */
enum class HardwareState : int {
    IDLE = 0,       // Hardware idle, neither RX nor TX
    SENSING = 1,    // RX mode active (listening phase)
    JAMMING = 2     // TX mode active (jamming phase)
};

enum class JammingType : uint8_t {
    CONTINUOUS_WAVE = 0,
    GAUSSIAN_NOISE = 1,
    MODULATED_NOISE = 2
};

enum class JammingStatus : uint8_t {
    IDLE = 0,
    SENSING_TARGET = 1,    // Currently in RX phase
    JAMMING_ACTIVE = 2,    // Currently in TX phase
    STOPPING = 3,
    ERROR_STATE = 4
};

enum class TargetActivityState : uint8_t {
    UNKNOWN = 0,
    INACTIVE = 1,
    ACTIVE = 2
};

// ============================================================================
// CONFIGURATION STRUCTURES
// ============================================================================

struct HardwareConfig {
    double center_freq_hz = 433.92e6;
    double sample_rate_hz = 20e6;
    double bandwidth_hz = 25e3;
    
    // TX parameters
    uint32_t lna_gain_db = 0;
    uint32_t vga_gain_db = 20;
    bool tx_amp_enable = true;
    
    // RX parameters (for sensing phase)
    uint32_t rx_lna_gain_db = 24;
    uint32_t rx_vga_gain_db = 16;
    
    // Thermal and duty cycle protection
    float max_duty_cycle = 0.9f;
    float thermal_limit_c = 80.0f;
    
    uint32_t tx_buffer_samples = 8192;
    uint32_t rx_buffer_samples = 8192;
    
    bool validate() const {
        // ET (TX) Mode: 1 MHz - 3 GHz (System specification)
        if (center_freq_hz < 1e6 || center_freq_hz > 3e9) return false;
        if (bandwidth_hz <= 0 || bandwidth_hz > sample_rate_hz) return false;
        if (lna_gain_db > 40 || vga_gain_db > 62) return false;
        if (rx_lna_gain_db > 40 || rx_vga_gain_db > 62) return false;
        if (max_duty_cycle < 0.0f || max_duty_cycle > 1.0f) return false;
        if (tx_buffer_samples < 1024 || (tx_buffer_samples & (tx_buffer_samples - 1)) != 0) {
            return false;
        }
        if (rx_buffer_samples < 1024 || (rx_buffer_samples & (rx_buffer_samples - 1)) != 0) {
            return false;
        }
        return true;
    }
};

struct SignalConfig {
    JammingType type = JammingType::GAUSSIAN_NOISE;
    
    // CW parameters
    double cw_offset_hz = 0.0;
    float cw_amplitude = 0.8f;
    
    // Noise parameters
    float noise_amplitude = 0.5f;
    uint32_t noise_seed = 0;
    
    // Modulation parameters
    double modulation_rate_hz = 1000.0;
    
    bool validate() const {
        if (cw_amplitude < 0.0f || cw_amplitude > 1.0f) return false;
        if (noise_amplitude < 0.0f || noise_amplitude > 1.0f) return false;
        if (modulation_rate_hz <= 0.0) return false;
        return true;
    }
};

/**
 * @brief TDD timing configuration for Look-Through operation
 */
struct TDDTimingConfig {
    uint32_t sensing_window_ms = 10;      // RX phase duration
    uint32_t jamming_pulse_ms = 50;       // TX phase duration (50-100ms typical)
    uint32_t rx_tx_switching_delay_ms = 2; // Hardware switching guard time
    
    // Energy detection thresholds
    float noise_floor_dbm = -100.0f;      // Default noise floor
    float detection_threshold_db = 10.0f; // SNR threshold above noise floor
    bool use_adaptive_threshold = true;    // Compute threshold dynamically
    
    // Settling time for HackRF PLL after frequency change
    uint32_t pll_settling_ms = 40;
    
    bool validate() const {
        if (sensing_window_ms < 1 || sensing_window_ms > 100) return false;
        if (jamming_pulse_ms < 10 || jamming_pulse_ms > 500) return false;
        if (rx_tx_switching_delay_ms > 10) return false;
        if (detection_threshold_db < 0.0f || detection_threshold_db > 50.0f) return false;
        return true;
    }
};

struct PipelineConfig {
    HardwareConfig hardware;
    SignalConfig signal;
    TDDTimingConfig tdd;
    
    uint32_t queue_max_size = 100;
    uint32_t processing_timeout_ms = 1000;
    
    bool enable_thermal_monitoring = true;
    bool enable_duty_cycle_limit = true;
    
    bool enable_logging = true;
    std::string log_prefix = "[LookThroughJamming]";
    
    bool validate() const {
        return hardware.validate() && signal.validate() && tdd.validate();
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
    uint32_t duration_ms = 0;  // 0 = continuous until stopped
    
    bool isValid() const {
        if (action == Action::START) {
            return target.isValid();
        }
        return true;
    }
};

/**
 * @brief Energy detection result from sensing phase
 */
struct SensingResult {
    uint64_t timestamp_ms = 0;
    double center_freq_hz = 0.0;
    
    float measured_rssi_dbm = -100.0f;
    float measured_power_db = 0.0f;
    float noise_floor_dbm = -100.0f;
    
    TargetActivityState activity = TargetActivityState::UNKNOWN;
    bool target_detected = false;
    
    uint32_t num_samples_processed = 0;
    uint32_t sensing_duration_ms = 0;
};

/**
 * @brief Complete status report including TDD state
 */
struct JammingReport {
    uint64_t timestamp_ms = 0;
    JammingStatus status = JammingStatus::IDLE;
    HardwareState hw_state = HardwareState::IDLE;
    
    uint64_t target_id = 0;
    double active_freq_hz = 0.0;
    double active_bandwidth_hz = 0.0;
    
    // TDD cycle statistics
    uint32_t sensing_cycles_completed = 0;
    uint32_t jamming_pulses_transmitted = 0;
    uint32_t target_detections = 0;
    
    uint64_t total_rx_samples = 0;
    uint64_t total_tx_samples = 0;
    
    uint32_t rx_errors = 0;
    uint32_t tx_errors = 0;
    uint32_t state_transition_errors = 0;
    
    float duty_cycle_current = 0.0f;
    float temperature_c = 0.0f;
    bool thermal_limit_reached = false;
    
    // Latest sensing result
    SensingResult last_sensing;
    
    uint64_t operation_start_ms = 0;
    uint64_t operation_duration_ms = 0;
    
    bool isActive() const {
        return (status == JammingStatus::SENSING_TARGET || 
                status == JammingStatus::JAMMING_ACTIVE);
    }
    
    bool isJamming() const {
        return (status == JammingStatus::JAMMING_ACTIVE) &&
               (hw_state == HardwareState::JAMMING);
    }
};

// ============================================================================
// CONSTANTS
// ============================================================================

namespace Constants {
    constexpr double PI = 3.14159265358979323846;
    constexpr double TWO_PI = 2.0 * PI;
    constexpr double SPEED_OF_LIGHT = 299792458.0;
    
    // Minimum detectable signal (typical SDR noise floor)
    constexpr float MIN_DETECTABLE_SIGNAL_DBM = -120.0f;
    
    // Hardware switching guard times (ms)
    constexpr uint32_t MIN_RX_TX_GUARD_MS = 1;
    constexpr uint32_t MIN_TX_RX_GUARD_MS = 1;
}

} // namespace LookThroughJamming
} // namespace CognitiveRF

#endif // COGNITIVE_RF_LOOK_THROUGH_JAMMING_DATA_STRUCTURES_HPP