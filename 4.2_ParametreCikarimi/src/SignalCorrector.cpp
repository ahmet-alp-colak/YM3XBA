/**
 * @file SignalCorrector.cpp
 * @brief Implementation of CFO correction
 * 
 * @author Senior C++ Software Architect
 * @date 2026
 */

#include "SignalCorrector.hpp"
#include <cmath>

namespace CognitiveRF {

SignalCorrector::SignalCorrector(double sample_rate_hz)
    : sample_rate_hz_(sample_rate_hz)
    , phase_accumulator_(0.0)
{
}

void SignalCorrector::correctFrequency(
    const std::vector<std::complex<float>>& input,
    double cfo_hz,
    std::vector<std::complex<float>>& output
)
{
    const size_t N = input.size();
    output.resize(N);
    
    // Pre-compute phase increment per sample
    const double phase_increment = -2.0 * M_PI * cfo_hz / sample_rate_hz_;
    
    // Apply complex rotation: output[n] = input[n] * e^(-j * phase)
    for (size_t n = 0; n < N; ++n) {
        const double phase = phase_increment * static_cast<double>(n);
        const std::complex<float> phasor(std::cos(phase), std::sin(phase));
        output[n] = input[n] * phasor;
    }
}

void SignalCorrector::correctFrequencyInPlace(
    std::vector<std::complex<float>>& iq_data,
    double cfo_hz
)
{
    const size_t N = iq_data.size();
    const double phase_increment = -2.0 * M_PI * cfo_hz / sample_rate_hz_;
    
    // In-place correction - more memory efficient
    for (size_t n = 0; n < N; ++n) {
        const double phase = phase_increment * static_cast<double>(n);
        const std::complex<float> phasor(std::cos(phase), std::sin(phase));
        iq_data[n] *= phasor;
    }
}

void SignalCorrector::correctFrequencyContinuous(
    const std::vector<std::complex<float>>& input,
    double cfo_hz,
    std::vector<std::complex<float>>& output
)
{
    const size_t N = input.size();
    output.resize(N);
    
    const double phase_increment = -2.0 * M_PI * cfo_hz / sample_rate_hz_;
    
    // Maintain phase continuity across buffer boundaries
    for (size_t n = 0; n < N; ++n) {
        const std::complex<float> phasor(
            std::cos(phase_accumulator_),
            std::sin(phase_accumulator_)
        );
        output[n] = input[n] * phasor;
        
        // Accumulate phase
        phase_accumulator_ += phase_increment;
        
        // Wrap phase to [-π, π] to prevent numerical drift
        if (phase_accumulator_ > M_PI) {
            phase_accumulator_ -= 2.0 * M_PI;
        } else if (phase_accumulator_ < -M_PI) {
            phase_accumulator_ += 2.0 * M_PI;
        }
    }
}

} // namespace CognitiveRF
