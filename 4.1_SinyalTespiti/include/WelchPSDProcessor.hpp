/**
 * @file WelchPSDProcessor.hpp
 * @brief Welch PSD processor for spectral analysis
 * @details Implements Welch's method for power spectral density estimation
 * 
 * System: Bilişsel RF Spektrum Haritalama Sistemi
 * Module: 4.1 Sinyal Tespiti (Signal Detection)
 * 
 * @author CognitiveRF Development Team
 * @date 2026-06-21
 */

#ifndef SIGNAL_DETECTION_WELCH_PSD_PROCESSOR_HPP
#define SIGNAL_DETECTION_WELCH_PSD_PROCESSOR_HPP

#include "DataStructures.hpp"
#include <fftw3.h>
#include <vector>

namespace CognitiveRF {
namespace SignalDetection {

/**
 * @class WelchPSDProcessor
 * @brief Computes power spectral density using Welch's method
 * 
 * @details Welch's method reduces noise by averaging multiple overlapping
 * windowed FFT segments. Uses Hamming window to reduce spectral leakage.
 * 
 * Formula: P_Welch(k) = (1/M) * sum_{m=0}^{M-1} |FFT{x_m(n) * w(n)}|^2
 */
class WelchPSDProcessor {
public:
    /**
     * @brief Constructor
     * @param fft_size FFT size (default: 1024)
     * @param overlap Overlap size (default: 512 for 50% overlap)
     */
    explicit WelchPSDProcessor(int fft_size = 1024, int overlap = 512);
    
    /**
     * @brief Destructor - cleans up FFTW resources
     */
    ~WelchPSDProcessor();
    
    // Delete copy constructor and assignment operator
    WelchPSDProcessor(const WelchPSDProcessor&) = delete;
    WelchPSDProcessor& operator=(const WelchPSDProcessor&) = delete;
    
    /**
     * @brief Compute Welch PSD from IQ samples
     * @param iq_samples Input IQ data
     * @param avg_psd_linear Output averaged PSD (linear scale)
     * @param sub_block_variances Output variance of each sub-block
     * @return true if successful, false otherwise
     */
    bool computeWelchPSD(
        const std::vector<IQSample>& iq_samples,
        std::vector<double>& avg_psd_linear,
        std::vector<double>& sub_block_variances
    );
    
    /**
     * @brief Get FFT size
     * @return FFT size
     */
    int getFFTSize() const { return fft_size_; }
    
    /**
     * @brief Get overlap size
     * @return Overlap size
     */
    int getOverlap() const { return overlap_; }
    
    /**
     * @brief Get step size
     * @return Step size (FFT_SIZE - OVERLAP)
     */
    int getStepSize() const { return step_size_; }

private:
    /**
     * @brief Generate Hamming window
     * @details Hamming window reduces spectral leakage
     * w(n) = 0.54 - 0.46 * cos(2π * n / (N-1))
     */
    void generateHammingWindow();
    
    int fft_size_;                               ///< FFT size
    int overlap_;                                ///< Overlap size
    int step_size_;                              ///< Step size
    
    std::vector<double> hamming_window_;         ///< Hamming window coefficients
    double window_power_;                        ///< Window power normalization
    
    // FFTW3 resources
    fftw_complex* fftw_in_;                      ///< FFTW input buffer
    fftw_complex* fftw_out_;                     ///< FFTW output buffer
    fftw_plan fftw_plan_;                        ///< FFTW plan
};

} // namespace SignalDetection
} // namespace CognitiveRF

#endif // SIGNAL_DETECTION_WELCH_PSD_PROCESSOR_HPP
