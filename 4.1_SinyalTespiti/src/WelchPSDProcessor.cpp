/**
 * @file WelchPSDProcessor.cpp
 * @brief Implementation of Welch PSD processor
 * 
 * System: Bilişsel RF Spektrum Haritalama Sistemi
 * Module: 4.1 Sinyal Tespiti (Signal Detection)
 * 
 * @author CognitiveRF Development Team
 * @date 2026-06-21
 */

#include "WelchPSDProcessor.hpp"
#include <cmath>
#include <algorithm>

namespace CognitiveRF {
namespace SignalDetection {

WelchPSDProcessor::WelchPSDProcessor(int fft_size, int overlap)
    : fft_size_(fft_size)
    , overlap_(overlap)
    , step_size_(fft_size - overlap)
    , window_power_(0.0)
    , fftw_in_(nullptr)
    , fftw_out_(nullptr)
    , fftw_plan_(nullptr)
{
    // Generate Hamming window
    generateHammingWindow();
    
    // Allocate FFTW buffers
    fftw_in_ = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * fft_size_);
    fftw_out_ = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * fft_size_);
    
    // Create FFTW plan with MEASURE for optimization
    fftw_plan_ = fftw_plan_dft_1d(
        fft_size_, 
        fftw_in_, 
        fftw_out_, 
        FFTW_FORWARD, 
        FFTW_MEASURE
    );
}

WelchPSDProcessor::~WelchPSDProcessor() {
    // Clean up FFTW resources
    if (fftw_plan_) {
        fftw_destroy_plan(fftw_plan_);
    }
    if (fftw_in_) {
        fftw_free(fftw_in_);
    }
    if (fftw_out_) {
        fftw_free(fftw_out_);
    }
}

void WelchPSDProcessor::generateHammingWindow() {
    hamming_window_.resize(fft_size_);
    window_power_ = 0.0;
    
    for (int i = 0; i < fft_size_; i++) {
        hamming_window_[i] = 0.54 - 0.46 * std::cos(TWO_PI * i / (fft_size_ - 1));
        window_power_ += hamming_window_[i] * hamming_window_[i];
    }
}

bool WelchPSDProcessor::computeWelchPSD(
    const std::vector<IQSample>& iq_samples,
    std::vector<double>& avg_psd_linear,
    std::vector<double>& sub_block_variances)
{
    // Validate input
    if (iq_samples.size() < static_cast<size_t>(fft_size_)) {
        return false;
    }
    
    // Initialize output buffers
    avg_psd_linear.assign(fft_size_, 0.0);
    sub_block_variances.clear();
    
    int segments = 0;
    
    // Process overlapping segments
    for (size_t offset = 0; offset + fft_size_ <= iq_samples.size(); offset += step_size_) {
        // Apply Hamming window to input data
        for (int i = 0; i < fft_size_; i++) {
            fftw_in_[i][0] = iq_samples[offset + i].real() * hamming_window_[i];
            fftw_in_[i][1] = iq_samples[offset + i].imag() * hamming_window_[i];
        }
        
        // Execute FFT
        fftw_execute(fftw_plan_);
        
        // Compute power spectrum and accumulate
        double block_energy = 0.0;
        for (int i = 0; i < fft_size_; i++) {
            double power = (fftw_out_[i][0] * fftw_out_[i][0]) + 
                          (fftw_out_[i][1] * fftw_out_[i][1]);
            avg_psd_linear[i] += power;
            block_energy += power;
        }
        
        // Store sub-block variance for micro-variance feature
        sub_block_variances.push_back(block_energy / fft_size_);
        segments++;
    }
    
    if (segments == 0) {
        return false;
    }
    
    // Welch averaging with window power compensation
    for (int i = 0; i < fft_size_; i++) {
        avg_psd_linear[i] = avg_psd_linear[i] / (segments * window_power_ * fft_size_);
    }
    
    return true;
}

} // namespace SignalDetection
} // namespace CognitiveRF
