/**
 * @file GMMClassifier.cpp
 * @brief Implementation of GMM classifier using EM algorithm
 * 
 * System: Bilişsel RF Spektrum Haritalama Sistemi
 * Module: 4.1 Sinyal Tespiti (Signal Detection)
 * 
 * @author CognitiveRF Development Team
 * @date 2026-06-21
 */

#include "GMMClassifier.hpp"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <limits>

namespace CognitiveRF {
namespace SignalDetection {

GMMClassifier::GMMClassifier(int max_iterations, double convergence_threshold)
    : max_iterations_(max_iterations)
    , convergence_threshold_(convergence_threshold)
    , trained_(false)
{
    // Initialize to default values
    params_.weights[0] = 0.5;
    params_.weights[1] = 0.5;
}

bool GMMClassifier::train(const std::vector<FeatureVector>& features) {
    if (features.size() < 10) {
        return false;  // Need minimum samples
    }
    
    // Convert FeatureVector to 8D arrays
    std::vector<std::array<double, 8>> data;
    data.reserve(features.size());
    for (const auto& f : features) {
        data.push_back(featureToArray(f));
    }
    
    // Initialize parameters
    initializeParameters(data);
    
    // EM iterations
    double prev_log_likelihood = -std::numeric_limits<double>::infinity();
    std::vector<std::array<double, 2>> responsibilities(data.size());
    
    for (int iter = 0; iter < max_iterations_; iter++) {
        // E-step
        double log_likelihood = expectationStep(data, responsibilities);
        
        // Check convergence
        if (std::abs(log_likelihood - prev_log_likelihood) < convergence_threshold_) {
            trained_ = true;
            return true;
        }
        
        // M-step
        maximizationStep(data, responsibilities);
        
        prev_log_likelihood = log_likelihood;
    }
    
    trained_ = true;
    return true;
}

ClassificationResult GMMClassifier::classify(const FeatureVector& features) {
    ClassificationResult result;
    
    if (!trained_) {
        result.signal_class = SignalClass::NOISE;
        result.likelihood = 0.0;
        return result;
    }
    
    std::array<double, 8> x = featureToArray(features);
    
    // Compute posterior probabilities for both components
    double p_noise = params_.weights[0] * gaussianProbability(x, 0);
    double p_signal = params_.weights[1] * gaussianProbability(x, 1);
    double total = p_noise + p_signal;
    
    if (total < 1e-100) {
        result.signal_class = SignalClass::NOISE;
        result.likelihood = 0.0;
        return result;
    }
    
    // Normalize to get posterior probabilities
    double posterior_signal = p_signal / total;
    double posterior_noise = p_noise / total;
    
    // Classify based on maximum posterior
    if (posterior_signal > posterior_noise) {
        result.signal_class = SignalClass::SIGNAL;
        result.likelihood = posterior_signal;
    } else {
        result.signal_class = SignalClass::NOISE;
        result.likelihood = posterior_noise;
    }
    
    return result;
}

void GMMClassifier::initializeParameters(const std::vector<std::array<double, 8>>& data) {
    const size_t n = data.size();
    
    // Compute global mean for each dimension
    std::array<double, 8> global_mean = {};
    for (const auto& x : data) {
        for (int d = 0; d < 8; d++) {
            global_mean[d] += x[d];
        }
    }
    for (int d = 0; d < 8; d++) {
        global_mean[d] /= n;
    }
    
    // Use first principal component (max_power) to separate noise/signal
    std::vector<double> max_powers;
    max_powers.reserve(n);
    for (const auto& x : data) {
        max_powers.push_back(x[0]);  // max_power is first feature
    }
    std::sort(max_powers.begin(), max_powers.end());
    
    // Split at 30th percentile (noise) and 70th percentile (signal)
    size_t split_low = n * 30 / 100;
    size_t split_high = n * 70 / 100;
    
    // Initialize Component-0 (noise): lower 30%
    params_.means[0].fill(0.0);
    int count_noise = 0;
    for (const auto& x : data) {
        if (x[0] <= max_powers[split_low]) {
            for (int d = 0; d < 8; d++) {
                params_.means[0][d] += x[d];
            }
            count_noise++;
        }
    }
    if (count_noise > 0) {
        for (int d = 0; d < 8; d++) {
            params_.means[0][d] /= count_noise;
        }
    }
    
    // Initialize Component-1 (signal): upper 30%
    params_.means[1].fill(0.0);
    int count_signal = 0;
    for (const auto& x : data) {
        if (x[0] >= max_powers[split_high]) {
            for (int d = 0; d < 8; d++) {
                params_.means[1][d] += x[d];
            }
            count_signal++;
        }
    }
    if (count_signal > 0) {
        for (int d = 0; d < 8; d++) {
            params_.means[1][d] /= count_signal;
        }
    }
    
    // Initialize variances (diagonal covariance)
    for (int k = 0; k < 2; k++) {
        params_.variances[k].fill(1.0);  // Start with unit variance
        
        double sum_sq = 0.0;
        int count = 0;
        for (const auto& x : data) {
            if ((k == 0 && x[0] <= max_powers[split_low]) ||
                (k == 1 && x[0] >= max_powers[split_high])) {
                for (int d = 0; d < 8; d++) {
                    double diff = x[d] - params_.means[k][d];
                    params_.variances[k][d] += diff * diff;
                }
                count++;
            }
        }
        
        if (count > 1) {
            for (int d = 0; d < 8; d++) {
                params_.variances[k][d] /= count;
                // Prevent collapse
                if (params_.variances[k][d] < 1e-6) {
                    params_.variances[k][d] = 1e-6;
                }
            }
        }
    }
    
    // Initialize weights based on count
    params_.weights[0] = static_cast<double>(count_noise) / n;
    params_.weights[1] = static_cast<double>(count_signal) / n;
}

double GMMClassifier::expectationStep(
    const std::vector<std::array<double, 8>>& data,
    std::vector<std::array<double, 2>>& responsibilities)
{
    double log_likelihood = 0.0;
    
    for (size_t i = 0; i < data.size(); i++) {
        // Compute weighted probabilities
        double p0 = params_.weights[0] * gaussianProbability(data[i], 0);
        double p1 = params_.weights[1] * gaussianProbability(data[i], 1);
        double total = p0 + p1;
        
        if (total < 1e-100) {
            responsibilities[i][0] = 0.5;
            responsibilities[i][1] = 0.5;
        } else {
            responsibilities[i][0] = p0 / total;
            responsibilities[i][1] = p1 / total;
            log_likelihood += std::log(total);
        }
    }
    
    return log_likelihood;
}

void GMMClassifier::maximizationStep(
    const std::vector<std::array<double, 8>>& data,
    const std::vector<std::array<double, 2>>& responsibilities)
{
    const size_t n = data.size();
    
    // Compute effective sample sizes
    double n_k[2] = {0.0, 0.0};
    for (size_t i = 0; i < n; i++) {
        n_k[0] += responsibilities[i][0];
        n_k[1] += responsibilities[i][1];
    }
    
    // Update means
    for (int k = 0; k < 2; k++) {
        params_.means[k].fill(0.0);
        
        for (size_t i = 0; i < n; i++) {
            for (int d = 0; d < 8; d++) {
                params_.means[k][d] += responsibilities[i][k] * data[i][d];
            }
        }
        
        if (n_k[k] > 1e-10) {
            for (int d = 0; d < 8; d++) {
                params_.means[k][d] /= n_k[k];
            }
        }
    }
    
    // Update variances (diagonal covariance)
    for (int k = 0; k < 2; k++) {
        params_.variances[k].fill(0.0);
        
        for (size_t i = 0; i < n; i++) {
            for (int d = 0; d < 8; d++) {
                double diff = data[i][d] - params_.means[k][d];
                params_.variances[k][d] += responsibilities[i][k] * diff * diff;
            }
        }
        
        if (n_k[k] > 1e-10) {
            for (int d = 0; d < 8; d++) {
                params_.variances[k][d] /= n_k[k];
                // Prevent collapse
                if (params_.variances[k][d] < 1e-6) {
                    params_.variances[k][d] = 1e-6;
                }
            }
        }
    }
    
    // Update weights
    params_.weights[0] = n_k[0] / n;
    params_.weights[1] = n_k[1] / n;
}

double GMMClassifier::gaussianProbability(const std::array<double, 8>& x, int component_idx) {
    // Multivariate Gaussian with diagonal covariance
    // p(x) = (1 / sqrt((2π)^d * |Σ|)) * exp(-0.5 * (x-μ)^T * Σ^-1 * (x-μ))
    
    double log_det = 0.0;
    double mahal_dist = 0.0;
    
    for (int d = 0; d < 8; d++) {
        double var = params_.variances[component_idx][d];
        log_det += std::log(var);
        
        double diff = x[d] - params_.means[component_idx][d];
        mahal_dist += (diff * diff) / var;
    }
    
    double log_prob = -0.5 * (8.0 * std::log(TWO_PI) + log_det + mahal_dist);
    return std::exp(log_prob);
}

double GMMClassifier::mahalanobisDistance(const std::array<double, 8>& x, int component_idx) {
    double dist = 0.0;
    
    for (int d = 0; d < 8; d++) {
        double diff = x[d] - params_.means[component_idx][d];
        dist += (diff * diff) / params_.variances[component_idx][d];
    }
    
    return dist;
}

std::array<double, 8> GMMClassifier::featureToArray(const FeatureVector& features) {
    return {
        features.max_power,
        features.mean_power,
        features.variance,
        features.skewness,
        features.kurtosis,
        features.energy_ratio,
        features.peak_to_avg_ratio,
        features.spectral_flatness
    };
}

} // namespace SignalDetection
} // namespace CognitiveRF
