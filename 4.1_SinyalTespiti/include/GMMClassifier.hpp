/**
 * @file GMMClassifier.hpp
 * @brief Gaussian Mixture Model classifier for signal detection
 * @details 2-component GMM (noise + signal) using EM algorithm
 * 
 * System: Bilişsel RF Spektrum Haritalama Sistemi
 * Module: 4.1 Sinyal Tespiti (Signal Detection)
 * 
 * @author CognitiveRF Development Team
 * @date 2026-06-21
 */

#ifndef SIGNAL_DETECTION_GMM_CLASSIFIER_HPP
#define SIGNAL_DETECTION_GMM_CLASSIFIER_HPP

#include "DataStructures.hpp"
#include <vector>
#include <array>

namespace CognitiveRF {
namespace SignalDetection {

/**
 * @class GMMClassifier
 * @brief 2-component Gaussian Mixture Model for signal/noise classification
 * 
 * @details Implements Expectation-Maximization (EM) algorithm for:
 * - Component-0: Noise (low-power Gaussian)
 * - Component-1: Signal (high-power Gaussian)
 * 
 * Algorithm:
 * 1. Initialize parameters (mean, variance, weight)
 * 2. E-step: Compute posterior probabilities
 * 3. M-step: Update parameters
 * 4. Iterate until convergence
 */
class GMMClassifier {
public:
    /**
     * @brief Constructor
     * @param max_iterations Maximum EM iterations (default: 100)
     * @param convergence_threshold Convergence threshold (default: 1e-6)
     */
    explicit GMMClassifier(int max_iterations = 100, double convergence_threshold = 1e-6);
    
    /**
     * @brief Train GMM on feature data
     * @param features Training feature vectors
     * @return true if training succeeded, false otherwise
     */
    bool train(const std::vector<FeatureVector>& features);
    
    /**
     * @brief Classify a single feature vector
     * @param features Input feature vector
     * @return Classification result with likelihood and class
     */
    ClassificationResult classify(const FeatureVector& features);
    
    /**
     * @brief Get current GMM parameters
     * @return GMM parameters
     */
    const GMMParameters& getParameters() const { return params_; }
    
    /**
     * @brief Check if model is trained
     * @return true if trained, false otherwise
     */
    bool isTrained() const { return trained_; }

private:
    /**
     * @brief Initialize GMM parameters using k-means++ style initialization
     * @param data Training data (8D vectors)
     */
    void initializeParameters(const std::vector<std::array<double, 8>>& data);
    
    /**
     * @brief E-step: Compute posterior probabilities
     * @param data Training data
     * @param responsibilities Output: posterior probabilities [N x 2]
     * @return Log-likelihood
     */
    double expectationStep(
        const std::vector<std::array<double, 8>>& data,
        std::vector<std::array<double, 2>>& responsibilities
    );
    
    /**
     * @brief M-step: Update GMM parameters
     * @param data Training data
     * @param responsibilities Posterior probabilities [N x 2]
     */
    void maximizationStep(
        const std::vector<std::array<double, 8>>& data,
        const std::vector<std::array<double, 2>>& responsibilities
    );
    
    /**
     * @brief Compute multivariate Gaussian probability
     * @param x Input vector (8D)
     * @param component_idx Component index (0 or 1)
     * @return Probability density
     */
    double gaussianProbability(const std::array<double, 8>& x, int component_idx);
    
    /**
     * @brief Compute Mahalanobis distance
     * @param x Input vector (8D)
     * @param component_idx Component index (0 or 1)
     * @return Mahalanobis distance squared
     */
    double mahalanobisDistance(const std::array<double, 8>& x, int component_idx);
    
    /**
     * @brief Convert FeatureVector to 8D array
     * @param features Feature vector
     * @return 8D array
     */
    std::array<double, 8> featureToArray(const FeatureVector& features);
    
    GMMParameters params_;                       ///< GMM parameters
    int max_iterations_;                         ///< Maximum EM iterations
    double convergence_threshold_;               ///< Convergence threshold
    bool trained_;                               ///< Training status flag
};

} // namespace SignalDetection
} // namespace CognitiveRF

#endif // SIGNAL_DETECTION_GMM_CLASSIFIER_HPP
