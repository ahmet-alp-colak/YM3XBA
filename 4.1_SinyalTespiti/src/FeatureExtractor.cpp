/**
 * @file FeatureExtractor.cpp
 * @brief Implementation of feature extraction
 * 
 * System: Bilişsel RF Spektrum Haritalama Sistemi
 * Module: 4.1 Sinyal Tespiti (Signal Detection)
 * 
 * @author CognitiveRF Development Team
 * @date 2026-06-21
 */

#include "FeatureExtractor.hpp"
#include <cmath>
#include <algorithm>
#include <numeric>

namespace CognitiveRF {
namespace SignalDetection {

FeatureVector FeatureExtractor::extractFeatures(
    const std::vector<double>& psd_linear,
    const std::vector<double>& sub_block_variances)
{
    FeatureVector features;
    
    // Compute global statistics
    double max_power, mean_power, variance, skewness, kurtosis;
    computeGlobalStatistics(psd_linear, max_power, mean_power, variance, skewness, kurtosis);
    
    // Compute spectral features
    double energy_ratio, peak_to_avg_ratio, spectral_flatness;
    computeSpectralFeatures(psd_linear, mean_power, energy_ratio, peak_to_avg_ratio, spectral_flatness);
    
    // Populate 8-dimensional feature vector
    features.max_power = max_power;
    features.mean_power = mean_power;
    features.variance = variance;
    features.skewness = skewness;
    features.kurtosis = kurtosis;
    features.energy_ratio = energy_ratio;
    features.peak_to_avg_ratio = peak_to_avg_ratio;
    features.spectral_flatness = spectral_flatness;
    
    // Additional features
    features.variance_of_variances = computeMicroVariance(sub_block_variances);
    features.gaussian_boundary_quality = computeGaussianBoundaryQuality(psd_linear, mean_power);
    
    return features;
}

void FeatureExtractor::computeGlobalStatistics(
    const std::vector<double>& psd,
    double& max_power,
    double& mean_power,
    double& variance,
    double& skewness,
    double& kurtosis)
{
    const size_t n = psd.size();
    if (n == 0) {
        max_power = mean_power = variance = skewness = kurtosis = 0.0;
        return;
    }
    
    // Max power
    max_power = *std::max_element(psd.begin(), psd.end());
    
    // Mean power
    mean_power = std::accumulate(psd.begin(), psd.end(), 0.0) / n;
    
    // Variance
    double sum_sq_diff = 0.0;
    for (double val : psd) {
        double diff = val - mean_power;
        sum_sq_diff += diff * diff;
    }
    variance = sum_sq_diff / n;
    
    // Standard deviation
    double std_dev = std::sqrt(variance);
    
    // Skewness and Kurtosis
    if (std_dev > 1e-10) {
        double sum_cubed = 0.0;
        double sum_fourth = 0.0;
        
        for (double val : psd) {
            double z = (val - mean_power) / std_dev;
            sum_cubed += z * z * z;
            sum_fourth += z * z * z * z;
        }
        
        skewness = sum_cubed / n;
        kurtosis = (sum_fourth / n) - 3.0;  // Excess kurtosis
    } else {
        skewness = 0.0;
        kurtosis = 0.0;
    }
}

void FeatureExtractor::computeSpectralFeatures(
    const std::vector<double>& psd,
    double mean_power,
    double& energy_ratio,
    double& peak_to_avg_ratio,
    double& spectral_flatness)
{
    const size_t n = psd.size();
    if (n == 0) {
        energy_ratio = peak_to_avg_ratio = spectral_flatness = 0.0;
        return;
    }
    
    // Energy ratio: energy above mean / total energy
    double total_energy = 0.0;
    double signal_energy = 0.0;
    
    for (double val : psd) {
        total_energy += val;
        if (val > mean_power) {
            signal_energy += val;
        }
    }
    
    energy_ratio = (total_energy > 1e-10) ? (signal_energy / total_energy) : 0.0;
    
    // Peak-to-average ratio
    double max_power = *std::max_element(psd.begin(), psd.end());
    double avg_power = total_energy / n;
    peak_to_avg_ratio = (avg_power > 1e-10) ? (max_power / avg_power) : 1.0;
    
    // Spectral flatness (Wiener entropy): geometric mean / arithmetic mean
    double log_sum = 0.0;
    int valid_bins = 0;
    
    for (double val : psd) {
        if (val > 1e-10) {  // Avoid log(0)
            log_sum += std::log(val);
            valid_bins++;
        }
    }
    
    if (valid_bins > 0 && avg_power > 1e-10) {
        double geometric_mean = std::exp(log_sum / valid_bins);
        spectral_flatness = geometric_mean / avg_power;
    } else {
        spectral_flatness = 0.0;
    }
}

double FeatureExtractor::computeMicroVariance(const std::vector<double>& sub_block_variances)
{
    const size_t n = sub_block_variances.size();
    if (n < 2) return 0.0;
    
    // Mean of variances
    double mean_var = std::accumulate(sub_block_variances.begin(), sub_block_variances.end(), 0.0) / n;
    
    // Variance of variances
    double sum_sq_diff = 0.0;
    for (double var : sub_block_variances) {
        double diff = var - mean_var;
        sum_sq_diff += diff * diff;
    }
    
    return sum_sq_diff / n;
}

double FeatureExtractor::computeGaussianBoundaryQuality(
    const std::vector<double>& psd,
    double mean_power)
{
    // Compute gradient (rate of change) at signal boundaries
    std::vector<double> gradients;
    
    for (size_t i = 1; i < psd.size(); i++) {
        double grad = std::abs(psd[i] - psd[i-1]);
        
        // Only consider gradients near the mean threshold
        if ((psd[i] > mean_power && psd[i-1] <= mean_power) ||
            (psd[i] <= mean_power && psd[i-1] > mean_power)) {
            gradients.push_back(grad);
        }
    }
    
    if (gradients.empty()) return 0.0;
    
    // Average boundary gradient normalized by mean power
    double avg_gradient = std::accumulate(gradients.begin(), gradients.end(), 0.0) / gradients.size();
    
    // Quality metric: higher gradient = sharper boundary = better signal
    // Normalize to [0, 1] range
    double quality = avg_gradient / (mean_power + 1e-10);
    return std::min(1.0, quality);
}

} // namespace SignalDetection
} // namespace CognitiveRF
