/**
 * @file AnalogDemodulator.cpp
 * @brief Implementation of analog signal demodulation (AM/FM)
 * 
 * System: Bilişsel RF Spektrum Haritalama Sistemi
 * Module: 4.3 Sinyal İzleme/Dinleme
 * 
 * @author Senior C++ Software Architect
 * @date 2026
 */

#include "AnalogDemodulator.hpp"
#include <cmath>
#include <algorithm>
#include <numeric>

namespace CognitiveRF {
namespace SignalMonitoring {

// ============================================================================
// Constructor
// ============================================================================

AnalogDemodulator::AnalogDemodulator(const DemodConfig& config)
    : config_(config)
    , last_fm_sample_(0.0f, 0.0f)
    , dc_block_x_prev_(0.0f)
    , dc_block_y_prev_(0.0f)
    , deemph_y_prev_(0.0f)
{
}

// ============================================================================
// AM Demodulation (Envelope Detector)
// ============================================================================

DemodulatedAudio AnalogDemodulator::demodulateAM(
    const std::vector<std::complex<float>>& iq_samples)
{
    DemodulatedAudio result;
    result.modulation_type = ModulationType::AM;
    result.sample_rate_hz = config_.input_sample_rate_hz;
    
    if (iq_samples.empty()) {
        return result;
    }
    
    // Step 1: Envelope detection - y[n] = √(I²[n] + Q²[n])
    std::vector<float> audio;
    audio.reserve(iq_samples.size());
    
    for (const auto& sample : iq_samples) {
        float envelope = std::sqrt(sample.real() * sample.real() + 
                                   sample.imag() * sample.imag());
        audio.push_back(envelope);
    }
    
    // Step 2: DC blocking filter
    if (config_.enable_dc_blocking) {
        applyDCBlockingFilter(audio);
    }
    
    // Step 3: Resample to audio rate
    result.pcm_samples = resampleAudio(audio, 
                                       config_.input_sample_rate_hz,
                                       config_.audio_sample_rate_hz);
    
    // Step 4: Apply gain
    for (auto& sample : result.pcm_samples) {
        sample *= config_.audio_gain;
    }
    
    // Step 5: Calculate audio SNR
    result.audio_snr_db = estimateAudioSNR(result.pcm_samples);
    result.sample_rate_hz = config_.audio_sample_rate_hz;
    
    return result;
}

// ============================================================================
// FM Demodulation (Frequency Discriminator)
// ============================================================================

DemodulatedAudio AnalogDemodulator::demodulateFM(
    const std::vector<std::complex<float>>& iq_samples)
{
    DemodulatedAudio result;
    result.modulation_type = ModulationType::FM;
    result.sample_rate_hz = config_.input_sample_rate_hz;
    
    if (iq_samples.empty()) {
        return result;
    }
    
    // Step 1: Frequency discriminator - Δθ[n] = arg(x[n] · x*[n-1])
    std::vector<float> audio;
    audio.reserve(iq_samples.size());
    
    std::complex<float> prev = (last_fm_sample_ != std::complex<float>(0.0f, 0.0f)) 
                                ? last_fm_sample_ 
                                : iq_samples[0];
    
    for (const auto& sample : iq_samples) {
        // Complex conjugate multiplication
        std::complex<float> product = sample * std::conj(prev);
        
        // Phase difference (instantaneous frequency)
        float phase_diff = std::arg(product);
        
        // Normalize to [-1, 1]
        audio.push_back(phase_diff / (2.0f * M_PI));
        
        prev = sample;
    }
    
    // Save last sample for streaming mode
    last_fm_sample_ = iq_samples.back();
    
    // Step 2: DC blocking filter
    if (config_.enable_dc_blocking) {
        applyDCBlockingFilter(audio);
    }
    
    // Step 3: De-emphasis filter (75μs time constant)
    if (config_.enable_deemphasis) {
        applyDeemphasisFilter(audio);
    }
    
    // Step 4: Resample to audio rate
    result.pcm_samples = resampleAudio(audio,
                                       config_.input_sample_rate_hz,
                                       config_.audio_sample_rate_hz);
    
    // Step 5: Apply gain
    for (auto& sample : result.pcm_samples) {
        sample *= config_.audio_gain;
    }
    
    // Step 6: Calculate audio SNR
    result.audio_snr_db = estimateAudioSNR(result.pcm_samples);
    result.sample_rate_hz = config_.audio_sample_rate_hz;
    
    return result;
}

// ============================================================================
// Generic Demodulation Dispatcher
// ============================================================================

DemodulatedAudio AnalogDemodulator::demodulate(
    const std::vector<std::complex<float>>& iq_samples,
    ModulationType modulation)
{
    switch (modulation) {
        case ModulationType::AM:
        case ModulationType::AM_DSB:
        case ModulationType::AM_USB:
        case ModulationType::AM_LSB:
            return demodulateAM(iq_samples);
            
        case ModulationType::FM:
        case ModulationType::FM_NARROW:
        case ModulationType::FM_WIDE:
            return demodulateFM(iq_samples);
            
        default:
            // Return empty result for non-analog modulations
            DemodulatedAudio result;
            result.modulation_type = modulation;
            return result;
    }
}

// ============================================================================
// Reset State
// ============================================================================

void AnalogDemodulator::reset()
{
    last_fm_sample_ = std::complex<float>(0.0f, 0.0f);
    dc_block_x_prev_ = 0.0f;
    dc_block_y_prev_ = 0.0f;
    deemph_y_prev_ = 0.0f;
}

// ============================================================================
// Private Methods
// ============================================================================

void AnalogDemodulator::applyDCBlockingFilter(std::vector<float>& samples)
{
    // First-order IIR DC blocking filter
    // y[n] = x[n] - x[n-1] + 0.995 * y[n-1]
    const float alpha = 0.995f;
    
    for (size_t i = 0; i < samples.size(); ++i) {
        float x = samples[i];
        float y = x - dc_block_x_prev_ + alpha * dc_block_y_prev_;
        
        dc_block_x_prev_ = x;
        dc_block_y_prev_ = y;
        samples[i] = y;
    }
}

void AnalogDemodulator::applyDeemphasisFilter(std::vector<float>& samples)
{
    // First-order IIR de-emphasis filter
    // Time constant: τ = 75μs (US standard)
    // y[n] = α * x[n] + (1 - α) * y[n-1]
    
    const float tau_us = config_.deemphasis_tau_us;
    const float sample_period_us = 1e6f / config_.input_sample_rate_hz;
    const float alpha = sample_period_us / (tau_us + sample_period_us);
    
    for (size_t i = 0; i < samples.size(); ++i) {
        float y = alpha * samples[i] + (1.0f - alpha) * deemph_y_prev_;
        deemph_y_prev_ = y;
        samples[i] = y;
    }
}

std::vector<float> AnalogDemodulator::resampleAudio(
    const std::vector<float>& input,
    uint32_t input_rate,
    uint32_t output_rate)
{
    if (input.empty() || input_rate == output_rate) {
        return input;
    }
    
    // Simple linear interpolation resampling
    const float ratio = static_cast<float>(input_rate) / output_rate;
    const size_t output_size = static_cast<size_t>(input.size() / ratio);
    
    std::vector<float> output;
    output.reserve(output_size);
    
    for (size_t i = 0; i < output_size; ++i) {
        float pos = i * ratio;
        size_t idx = static_cast<size_t>(pos);
        float frac = pos - idx;
        
        if (idx + 1 < input.size()) {
            // Linear interpolation
            float sample = input[idx] * (1.0f - frac) + input[idx + 1] * frac;
            output.push_back(sample);
        } else if (idx < input.size()) {
            output.push_back(input[idx]);
        }
    }
    
    return output;
}

float AnalogDemodulator::estimateAudioSNR(const std::vector<float>& samples) const
{
    if (samples.size() < 100) {
        return 0.0f;
    }
    
    // Calculate signal power (RMS)
    float signal_power = 0.0f;
    for (const auto& sample : samples) {
        signal_power += sample * sample;
    }
    signal_power /= samples.size();
    
    // Estimate noise power from high-frequency components
    // Simple approach: difference between consecutive samples
    float noise_power = 0.0f;
    for (size_t i = 1; i < samples.size(); ++i) {
        float diff = samples[i] - samples[i-1];
        noise_power += diff * diff;
    }
    noise_power /= (samples.size() - 1);
    noise_power /= 2.0f; // Approximate correction factor
    
    // Avoid division by zero
    if (noise_power < 1e-10f) {
        return 60.0f; // Very high SNR
    }
    
    // SNR in dB
    float snr_db = 10.0f * std::log10(signal_power / noise_power);
    
    // Clamp to reasonable range
    return std::max(0.0f, std::min(60.0f, snr_db));
}

} // namespace SignalMonitoring
} // namespace CognitiveRF
