/**
 * @file KalmanCFOTracker.cpp
 * @brief Implementation of Kalman Filter for CFO tracking
 * 
 * @author Senior C++ Software Architect
 * @date 2026
 */

#include "KalmanCFOTracker.hpp"
#include <cmath>

namespace CognitiveRF {

KalmanCFOTracker::KalmanCFOTracker(
    double process_noise,
    double measurement_noise,
    double initial_estimate
)
    : state_{initial_estimate, 0.0}
    , measurement_noise_(measurement_noise)
{
    // Initialize state covariance with high uncertainty
    covariance_[0][0] = 1.0;  // CFO variance
    covariance_[0][1] = 0.0;  // Cross-correlation
    covariance_[1][0] = 0.0;  // Cross-correlation
    covariance_[1][1] = 0.1;  // CFO rate variance
    
    // Initialize process noise covariance
    process_noise_[0][0] = process_noise;
    process_noise_[0][1] = 0.0;
    process_noise_[1][0] = 0.0;
    process_noise_[1][1] = process_noise * 0.1;  // Drift rate changes slower
}

void KalmanCFOTracker::predict() {
    // State prediction: X_pred = A * X
    // X = [CFO, CFO_rate]
    // A = [[1, 1], [0, 1]]
    
    double cfo_pred = state_[0] + state_[1];  // CFO(k) = CFO(k-1) + rate(k-1)
    double rate_pred = state_[1];              // rate(k) = rate(k-1)
    
    state_[0] = cfo_pred;
    state_[1] = rate_pred;
    
    // Covariance prediction: P_pred = A * P * A^T + Q
    // Manual matrix multiplication for 2x2 case (optimized)
    
    double p00 = covariance_[0][0];
    double p01 = covariance_[0][1];
    double p10 = covariance_[1][0];
    double p11 = covariance_[1][1];
    
    // A * P
    double ap00 = p00 + p10;
    double ap01 = p01 + p11;
    double ap10 = p10;
    double ap11 = p11;
    
    // (A * P) * A^T
    double apat00 = ap00 + ap01;
    double apat01 = ap01;
    double apat10 = ap10 + ap11;
    double apat11 = ap11;
    
    // Add process noise Q
    covariance_[0][0] = apat00 + process_noise_[0][0];
    covariance_[0][1] = apat01 + process_noise_[0][1];
    covariance_[1][0] = apat10 + process_noise_[1][0];
    covariance_[1][1] = apat11 + process_noise_[1][1];
}

void KalmanCFOTracker::update(double measurement) {
    // Innovation (measurement residual): y = Z - H * X_pred
    // H = [1, 0] (we only measure CFO, not its rate)
    double innovation = measurement - state_[0];
    
    // Innovation covariance: S = H * P_pred * H^T + R
    // For our case: S = P[0][0] + R (scalar)
    double innovation_covariance = covariance_[0][0] + measurement_noise_;
    
    // Kalman gain: K = P_pred * H^T * S^-1
    // K is a 2x1 vector
    double k0 = covariance_[0][0] / innovation_covariance;
    double k1 = covariance_[1][0] / innovation_covariance;
    
    // State update: X = X_pred + K * innovation
    state_[0] += k0 * innovation;
    state_[1] += k1 * innovation;
    
    // Covariance update: P = (I - K * H) * P_pred
    // Using Joseph form for numerical stability
    // (I - K*H) = [[1-k0, 0], [-k1, 1]]
    
    double i_kh_00 = 1.0 - k0;
    double i_kh_01 = 0.0;
    double i_kh_10 = -k1;
    double i_kh_11 = 1.0;
    
    // (I - K*H) * P_pred
    double p00_new = i_kh_00 * covariance_[0][0] + i_kh_01 * covariance_[1][0];
    double p01_new = i_kh_00 * covariance_[0][1] + i_kh_01 * covariance_[1][1];
    double p10_new = i_kh_10 * covariance_[0][0] + i_kh_11 * covariance_[1][0];
    double p11_new = i_kh_10 * covariance_[0][1] + i_kh_11 * covariance_[1][1];
    
    covariance_[0][0] = p00_new;
    covariance_[0][1] = p01_new;
    covariance_[1][0] = p10_new;
    covariance_[1][1] = p11_new;
}

void KalmanCFOTracker::reset(double initial_cfo) {
    state_[0] = initial_cfo;
    state_[1] = 0.0;
    
    // Reset to high uncertainty
    covariance_[0][0] = 1.0;
    covariance_[0][1] = 0.0;
    covariance_[1][0] = 0.0;
    covariance_[1][1] = 0.1;
}

} // namespace CognitiveRF
