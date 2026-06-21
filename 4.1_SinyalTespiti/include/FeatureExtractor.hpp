/**
 * @file FeatureExtractor.hpp
 * @brief Feature extraction from PSD data
 * @details Extracts 8-dimensional feature vector from Welch PSD for GMM classification
 * 
 * System: Bilişsel RF Spektrum Haritalama Sistemi
 * Module: 4.1 Sinyal Tespiti (Signal Detection)
 * 
 * @author CognitiveRF Development Team
 * @date 2026-06-21
 */

#ifndef SIGNAL_DETECTION_FEATURE_EXTRACTOR_HPP
#define SIGNAL_DETECTION_FEATURE_EXTRACTOR_HPP

#include "DataStructures.hpp"
#include <vector>

namespace CognitiveRF {
namespace SignalDetection {

/**
 * @class FeatureExtractor
 * @brief Extracts statistical features from PSD data
 * 
 * @details Computes 8-dimensional feature vector:
 * 1. max_power: Maximum power in PSD
 * 2. mean_power: Average power
 * 3. variance: Power variance
 * 4. skewness: Distribution asymmetry
 * 5. kurtosis: Distribution peakedness
 * 6. energy_ratio: Signal band energy / total energy
 * 7. peak_to_avg_ratio: Peak power / average power
 * 8. spectral_flatness: Geometric mean / arithmetic mean (Wiener entropy)
 * 
 * Additional computed features:
 * - variance_of_variances: Micro-variance feature
 * - gaussian_boundary_quality: Boundary sharpness metric
 */
class FeatureExtractor {
public:
    /**
     * @brief Default constructor
     */
    FeatureExtractor() = default;
    
    /**
     * @brief Extract features from PSD data
     * @param psd_linear PSD data (linear scale)
     * @param sub_block_variances Variance of each Welch sub-block
     * @return FeatureVector with 8 features
     */
    FeatureVector extractFeatures(
        const std::vector<double>& psd_linear,
        const std::vector<double>& sub_block_variances
    );

private:
    /**
     * @brief Compute global power statistics
     * @param psd PSD data
     * @param max_power Output: maximum power
     * @param mean_power Output: mean power
     * @param variance Output: power variance
     * @param skewness Output: skewness
     * @param kurtosis Output: kurtosis
     */
    void computeGlobalStatistics(
        const std::vector<double>& psd,
        double& max_power,
        double& mean_power,
        double& variance,
        double& skewness,
        double& kurtosis
    );
    
    /**
     * @brief Compute spectral features
     * @param psd PSD data
     * @param mean_power Mean power (for threshold)
     * @param energy_ratio Output: energy ratio
     * @param peak_to_avg_ratio Output: peak-to-average ratio
     * @param spectral_flatness Output: spectral flatness (Wiener entropy)
     */
    void computeSpectralFeatures(
        const std::vector<double>& psd,
        double mean_power,
        double& energy_ratio,
        double& peak_to_avg_ratio,
        double& spectral_flatness
    );
    
    /**
     * @brief Compute micro-variance feature
     * @param sub_block_variances Variance of each Welch sub-block
     * @return Variance of variances
     */
    double computeMicroVariance(const std::vector<double>& sub_block_variances);
    
    /**
     * @brief Compute Gaussian boundary quality
     * @param psd PSD data
     * @param mean_power Mean power threshold
     * @return Boundary sharpness metric [0, 1]
     */
    double computeGaussianBoundaryQuality(
        const std::vector<double>& psd,
        double mean_power
    );
};

} // namespace SignalDetection
} // namespace CognitiveRF

#endif // SIGNAL_DETECTION_FEATURE_EXTRACTOR_HPP
