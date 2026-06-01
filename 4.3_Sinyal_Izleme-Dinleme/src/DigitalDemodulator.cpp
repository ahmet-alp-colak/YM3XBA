/**
 * @file DigitalDemodulator.cpp
 * @brief Implementation of digital signal demodulation (PSK/FSK/OFDM)
 * 
 * System: Bilişsel RF Spektrum Haritalama Sistemi
 * Module: 4.3 Sinyal İzleme/Dinleme
 * 
 * @author Senior C++ Software Architect
 * @date 2026
 */

#include "DigitalDemodulator.hpp"
#include <cmath>
#include <algorithm>
#include <numeric>

namespace CognitiveRF {
namespace SignalMonitoring {

// ============================================================================
// Constructor
// ============================================================================

DigitalDemodulator::DigitalDemodulator(const DemodConfig& config)
    : config_(config)
    , costas_phase_(0.0f)
    , costas_freq_(0.0f)
    , timing_phase_(0.0f)
    , timing_freq_(1.0f / config.samples_per_symbol)
    , timing_error_(0.0f)
    , sync_locked_(false)
    , sync_counter_(0)
{
    // Calculate loop filter coefficients
    // Second-order loop filter (proportional + integral)
    float damping = config_.costas_damping;
    float bw = config_.costas_loop_bw;
    float denom = 1.0f + 2.0f * damping * bw + bw * bw;
    costas_alpha_ = (4.0f * damping * bw) / denom;
    costas_beta_ = (4.0f * bw * bw) / denom;
    
    // Gardner timing loop coefficients
    damping = config_.gardner_damping;
    bw = config_.gardner_loop_bw;
    denom = 1.0f + 2.0f * damping * bw + bw * bw;
    timing_alpha_ = (4.0f * damping * bw) / denom;
    timing_beta_ = (4.0f * bw * bw) / denom;
}

// ============================================================================
// BPSK Demodulation
// ============================================================================

DemodulatedBitstream DigitalDemodulator::demodulateBPSK(
    const std::vector<std::complex<float>>& iq_samples)
{
    DemodulatedBitstream result;
    result.modulation_type = ModulationType::BPSK;
    result.symbol_rate_hz = config_.symbol_rate_hz;
    
    if (iq_samples.empty()) {
        return result;
    }
    
    // Step 1: Carrier recovery (Costas Loop)
    auto phase_corrected = applyCostasLoop(iq_samples);
    
    // Step 2: Timing recovery (Gardner)
    auto symbols = applyGardnerTiming(phase_corrected);
    
    // Step 3: Symbol slicing (BPSK: 2 constellation points)
    result.bits = sliceSymbols(symbols, 2);
    
    // Step 4: Estimate BER
    result.ber_estimate = estimateBER(symbols);
    
    // Step 5: Check sync
    result.sync_locked = detectSync(symbols);
    
    return result;
}

// ============================================================================
// QPSK Demodulation
// ============================================================================

DemodulatedBitstream DigitalDemodulator::demodulateQPSK(
    const std::vector<std::complex<float>>& iq_samples)
{
    DemodulatedBitstream result;
    result.modulation_type = ModulationType::QPSK;
    result.symbol_rate_hz = config_.symbol_rate_hz;
    
    if (iq_samples.empty()) {
        return result;
    }
    
    // Step 1: Carrier recovery
    auto phase_corrected = applyCostasLoop(iq_samples);
    
    // Step 2: Timing recovery
    auto symbols = applyGardnerTiming(phase_corrected);
    
    // Step 3: Symbol slicing (QPSK: 4 constellation points)
    result.bits = sliceSymbols(symbols, 4);
    
    // Step 4: Estimate BER
    result.ber_estimate = estimateBER(symbols);
    
    // Step 5: Check sync
    result.sync_locked = detectSync(symbols);
    
    return result;
}

// ============================================================================
// 8PSK Demodulation
// ============================================================================

DemodulatedBitstream DigitalDemodulator::demodulate8PSK(
    const std::vector<std::complex<float>>& iq_samples)
{
    DemodulatedBitstream result;
    result.modulation_type = ModulationType::PSK_8;
    result.symbol_rate_hz = config_.symbol_rate_hz;
    
    if (iq_samples.empty()) {
        return result;
    }
    
    // Step 1: Carrier recovery
    auto phase_corrected = applyCostasLoop(iq_samples);
    
    // Step 2: Timing recovery
    auto symbols = applyGardnerTiming(phase_corrected);
    
    // Step 3: Symbol slicing (8PSK: 8 constellation points)
    result.bits = sliceSymbols(symbols, 8);
    
    // Step 4: Estimate BER
    result.ber_estimate = estimateBER(symbols);
    
    // Step 5: Check sync
    result.sync_locked = detectSync(symbols);
    
    return result;
}

// ============================================================================
// FSK Demodulation
// ============================================================================

DemodulatedBitstream DigitalDemodulator::demodulateFSK(
    const std::vector<std::complex<float>>& iq_samples,
    uint32_t num_tones)
{
    DemodulatedBitstream result;
    result.modulation_type = (num_tones == 2) ? ModulationType::FSK_2 : ModulationType::FSK_4;
    result.symbol_rate_hz = config_.symbol_rate_hz;
    
    if (iq_samples.empty()) {
        return result;
    }
    
    // FSK demodulation via frequency discriminator
    std::vector<float> freq_samples;
    freq_samples.reserve(iq_samples.size());
    
    for (size_t i = 1; i < iq_samples.size(); ++i) {
        std::complex<float> product = iq_samples[i] * std::conj(iq_samples[i-1]);
        float inst_freq = std::arg(product);
        freq_samples.push_back(inst_freq);
    }
    
    // Symbol timing recovery (simplified)
    size_t samples_per_symbol = config_.samples_per_symbol;
    std::vector<float> symbols;
    
    for (size_t i = 0; i + samples_per_symbol < freq_samples.size(); i += samples_per_symbol) {
        // Average over symbol period
        float symbol_value = 0.0f;
        for (size_t j = 0; j < samples_per_symbol; ++j) {
            symbol_value += freq_samples[i + j];
        }
        symbol_value /= samples_per_symbol;
        symbols.push_back(symbol_value);
    }
    
    // Quantize to tones
    if (!symbols.empty()) {
        float min_val = *std::min_element(symbols.begin(), symbols.end());
        float max_val = *std::max_element(symbols.begin(), symbols.end());
        float range = max_val - min_val;
        
        for (float symbol : symbols) {
            if (range > 0) {
                float normalized = (symbol - min_val) / range;
                uint8_t tone = static_cast<uint8_t>(normalized * (num_tones - 1) + 0.5f);
                
                // Convert tone to bits
                uint8_t bits_per_tone = (num_tones == 2) ? 1 : 2;
                for (uint8_t b = 0; b < bits_per_tone; ++b) {
                    result.bits.push_back((tone >> (bits_per_tone - 1 - b)) & 1);
                }
            }
        }
    }
    
    result.sync_locked = !result.bits.empty();
    result.ber_estimate = 0.1f; // Placeholder
    
    return result;
}

// ============================================================================
// GMSK Demodulation
// ============================================================================

DemodulatedBitstream DigitalDemodulator::demodulateGMSK(
    const std::vector<std::complex<float>>& iq_samples)
{
    // GMSK is similar to FSK with Gaussian filtering
    return demodulateFSK(iq_samples, 2);
}

// ============================================================================
// OFDM Demodulation
// ============================================================================

DemodulatedBitstream DigitalDemodulator::demodulateOFDM(
    const std::vector<std::complex<float>>& iq_samples)
{
    DemodulatedBitstream result;
    result.modulation_type = ModulationType::OFDM;
    result.symbol_rate_hz = config_.symbol_rate_hz;
    
    // OFDM demodulation requires FFT and pilot synchronization
    // This is a simplified placeholder implementation
    // Full implementation would use FFTW3 and proper OFDM frame structure
    
    result.bits.clear();
    result.sync_locked = false;
    result.ber_estimate = 1.0f;
    
    return result;
}

// ============================================================================
// Generic Demodulation Dispatcher
// ============================================================================

DemodulatedBitstream DigitalDemodulator::demodulate(
    const std::vector<std::complex<float>>& iq_samples,
    ModulationType modulation)
{
    switch (modulation) {
        case ModulationType::BPSK:
            return demodulateBPSK(iq_samples);
            
        case ModulationType::QPSK:
        case ModulationType::OQPSK:
            return demodulateQPSK(iq_samples);
            
        case ModulationType::PSK_8:
            return demodulate8PSK(iq_samples);
            
        case ModulationType::FSK_2:
            return demodulateFSK(iq_samples, 2);
            
        case ModulationType::FSK_4:
            return demodulateFSK(iq_samples, 4);
            
        case ModulationType::GMSK:
        case ModulationType::GFSK:
        case ModulationType::MSK:
            return demodulateGMSK(iq_samples);
            
        case ModulationType::OFDM:
            return demodulateOFDM(iq_samples);
            
        default:
            DemodulatedBitstream result;
            result.modulation_type = modulation;
            return result;
    }
}

// ============================================================================
// Reset State
// ============================================================================

void DigitalDemodulator::reset()
{
    costas_phase_ = 0.0f;
    costas_freq_ = 0.0f;
    timing_phase_ = 0.0f;
    timing_freq_ = 1.0f / config_.samples_per_symbol;
    timing_error_ = 0.0f;
    sync_locked_ = false;
    sync_counter_ = 0;
    symbol_buffer_.clear();
}

// ============================================================================
// Private Methods - Costas Loop
// ============================================================================

std::vector<std::complex<float>> DigitalDemodulator::applyCostasLoop(
    const std::vector<std::complex<float>>& samples)
{
    std::vector<std::complex<float>> output;
    output.reserve(samples.size());
    
    for (const auto& sample : samples) {
        // Rotate by current phase estimate
        float cos_phase = std::cos(-costas_phase_);
        float sin_phase = std::sin(-costas_phase_);
        std::complex<float> rotated(
            sample.real() * cos_phase - sample.imag() * sin_phase,
            sample.real() * sin_phase + sample.imag() * cos_phase
        );
        
        output.push_back(rotated);
        
        // Phase error detector (decision-directed)
        float error = 0.0f;
        float i = rotated.real();
        float q = rotated.imag();
        
        // QPSK error detector: error = sign(I) * Q - sign(Q) * I
        float sign_i = (i >= 0) ? 1.0f : -1.0f;
        float sign_q = (q >= 0) ? 1.0f : -1.0f;
        error = sign_i * q - sign_q * i;
        
        // Update phase and frequency
        costas_freq_ += costas_beta_ * error;
        costas_phase_ += costas_freq_ + costas_alpha_ * error;
        
        // Wrap phase to [-π, π]
        while (costas_phase_ > M_PI) costas_phase_ -= 2.0f * M_PI;
        while (costas_phase_ < -M_PI) costas_phase_ += 2.0f * M_PI;
    }
    
    return output;
}

// ============================================================================
// Private Methods - Gardner Timing Recovery
// ============================================================================

std::vector<std::complex<float>> DigitalDemodulator::applyGardnerTiming(
    const std::vector<std::complex<float>>& samples)
{
    std::vector<std::complex<float>> symbols;
    
    if (samples.size() < 3) {
        return symbols;
    }
    
    // Interpolator state
    size_t in_idx = 0;
    float mu = 0.0f; // Fractional timing offset
    
    std::complex<float> y_prev(0.0f, 0.0f);
    std::complex<float> y_mid(0.0f, 0.0f);
    
    while (in_idx + 2 < samples.size()) {
        // Interpolate at current timing point
        size_t idx = static_cast<size_t>(timing_phase_);
        if (idx + 1 >= samples.size()) break;
        
        float frac = timing_phase_ - idx;
        std::complex<float> y_curr = samples[idx] * (1.0f - frac) + samples[idx + 1] * frac;
        
        // Output symbol
        symbols.push_back(y_curr);
        
        // Gardner timing error: e[k] = (y[kT] - y[kT-T]) * y[kT-T/2]
        if (symbols.size() > 1) {
            std::complex<float> error_vec = (y_curr - y_prev) * std::conj(y_mid);
            timing_error_ = error_vec.real();
            
            // Update timing
            timing_freq_ += timing_beta_ * timing_error_;
            timing_phase_ += timing_freq_ + timing_alpha_ * timing_error_;
        } else {
            timing_phase_ += timing_freq_;
        }
        
        // Update history
        y_prev = y_curr;
        
        // Calculate mid-point for next iteration
        float mid_phase = timing_phase_ - timing_freq_ * 0.5f;
        if (mid_phase >= 0 && mid_phase + 1 < samples.size()) {
            size_t mid_idx = static_cast<size_t>(mid_phase);
            float mid_frac = mid_phase - mid_idx;
            y_mid = samples[mid_idx] * (1.0f - mid_frac) + samples[mid_idx + 1] * mid_frac;
        }
        
        in_idx = static_cast<size_t>(timing_phase_);
    }
    
    return symbols;
}

// ============================================================================
// Private Methods - Symbol Slicing
// ============================================================================

std::vector<uint8_t> DigitalDemodulator::sliceSymbols(
    const std::vector<std::complex<float>>& symbols,
    uint32_t constellation_size)
{
    std::vector<uint8_t> bits;
    
    for (const auto& symbol : symbols) {
        if (constellation_size == 2) {
            // BPSK: decision on real part
            bits.push_back(symbol.real() >= 0 ? 1 : 0);
        }
        else if (constellation_size == 4) {
            // QPSK: 2 bits per symbol
            bits.push_back(symbol.real() >= 0 ? 1 : 0);
            bits.push_back(symbol.imag() >= 0 ? 1 : 0);
        }
        else if (constellation_size == 8) {
            // 8PSK: 3 bits per symbol
            float angle = std::atan2(symbol.imag(), symbol.real());
            if (angle < 0) angle += 2.0f * M_PI;
            uint8_t symbol_idx = static_cast<uint8_t>((angle / (2.0f * M_PI)) * 8 + 0.5f) % 8;
            bits.push_back((symbol_idx >> 2) & 1);
            bits.push_back((symbol_idx >> 1) & 1);
            bits.push_back(symbol_idx & 1);
        }
    }
    
    return bits;
}

// ============================================================================
// Private Methods - BER Estimation
// ============================================================================

float DigitalDemodulator::estimateBER(const std::vector<std::complex<float>>& symbols) const
{
    if (symbols.empty()) {
        return 1.0f;
    }
    
    // Estimate BER from constellation error
    float total_error = 0.0f;
    for (const auto& symbol : symbols) {
        // Distance to nearest constellation point (simplified for QPSK)
        float i_error = std::abs(symbol.real()) - 1.0f;
        float q_error = std::abs(symbol.imag()) - 1.0f;
        total_error += std::abs(i_error) + std::abs(q_error);
    }
    
    float avg_error = total_error / symbols.size();
    float ber = std::min(0.5f, avg_error / 4.0f);
    
    return ber;
}

// ============================================================================
// Private Methods - Sync Detection
// ============================================================================

bool DigitalDemodulator::detectSync(const std::vector<std::complex<float>>& symbols)
{
    if (symbols.empty()) {
        sync_locked_ = false;
        sync_counter_ = 0;
        return false;
    }
    
    // Check constellation quality
    float avg_magnitude = 0.0f;
    for (const auto& symbol : symbols) {
        avg_magnitude += std::abs(symbol);
    }
    avg_magnitude /= symbols.size();
    
    // Good sync if magnitude is reasonable
    if (avg_magnitude > 0.5f && avg_magnitude < 2.0f) {
        sync_counter_++;
        if (sync_counter_ >= config_.sync_threshold) {
            sync_locked_ = true;
        }
    } else {
        sync_counter_ = 0;
        sync_locked_ = false;
    }
    
    return sync_locked_;
}

} // namespace SignalMonitoring
} // namespace CognitiveRF
