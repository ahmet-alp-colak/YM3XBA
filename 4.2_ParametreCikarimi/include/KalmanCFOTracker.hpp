/**
 * @file KalmanCFOTracker.hpp
 * @brief Kalman Filter for Carrier Frequency Offset (CFO) tracking
 * @details Implements 1D Kalman Filter to track hardware oscillator drift
 * 
 * Purpose: Filter out noise-induced frequency jitter and track real oscillator drift
 * 
 * State Vector: X = [CFO, CFO_rate]
 * Measurement: Z = instantaneous frequency offset from FFT/Costas Loop
 * 
 * References:
 * - Section 4.2.1: Donanım Kaynaklı Frekans Sapması (CFO) ve Kalman Filtresi
 * - Pipeline Stage [8]: Adaptif CFO Düzeltme ve Kalman Filtresi
 * 
 * @author Senior C++ Software Architect
 * @date 2026
 */

#ifndef KALMAN_CFO_TRACKER_HPP
#define KALMAN_CFO_TRACKER_HPP

#include <array>
#include <cmath>

namespace CognitiveRF {

/**
 * @class KalmanCFOTracker
 * @brief 1-Dimensional Kalman Filter for CFO estimation
 * 
 * Mathematical Model:
 * Predict:
 *   X_pred(k) = A * X(k-1)
 *   P_pred(k) = A * P(k-1) * A^T + Q
 * 
 * Update:
 *   K(k) = P_pred(k) * H^T * [H * P_pred(k) * H^T + R]^-1
 *   X(k) = X_pred(k) + K(k) * [Z(k) - H * X_pred(k)]
 *   P(k) = [I - K(k) * H] * P_pred(k)
 * 
 * State: [CFO, CFO_drift_rate]
 */
class KalmanCFOTracker {
public:
    /**
     * @brief Constructor with configurable noise parameters
     * @param process_noise Process noise covariance (Q) - models system uncertainty
     * @param measurement_noise Measurement noise covariance (R) - models sensor noise
     * @param initial_estimate Initial CFO estimate in Hz
     */
    explicit KalmanCFOTracker(
        double process_noise = 1e-5,
        double measurement_noise = 1e-2,
        double initial_estimate = 0.0
    );

    /**
     * @brief Predict step - time update
     * @details Projects state forward in time based on system model
     * 
     * X_pred = A * X_prev
     * P_pred = A * P_prev * A^T + Q
     */
    void predict();

    /**
     * @brief Update step - measurement update
     * @details Incorporates new measurement to correct prediction
     * 
     * @param measurement Measured frequency offset in Hz (from FFT peak or Costas Loop)
     * 
     * Innovation: y = Z - H * X_pred
     * Kalman Gain: K = P_pred * H^T * (H * P_pred * H^T + R)^-1
     * State Update: X = X_pred + K * y
     * Covariance Update: P = (I - K * H) * P_pred
     */
    void update(double measurement);

    /**
     * @brief Get current CFO estimate
     * @return Filtered CFO value in Hz
     */
    double getCFO() const { return state_[0]; }

    /**
     * @brief Get CFO drift rate estimate
     * @return Rate of frequency change in Hz/sample
     */
    double getCFORate() const { return state_[1]; }

    /**
     * @brief Get estimation uncertainty
     * @return Standard deviation of CFO estimate
     */
    double getUncertainty() const { return std::sqrt(covariance_[0][0]); }

    /**
     * @brief Reset filter to initial state
     * @param initial_cfo New initial CFO estimate
     */
    void reset(double initial_cfo = 0.0);

    /**
     * @brief Check if filter has converged
     * @param threshold Convergence threshold (uncertainty < threshold)
     * @return true if filter is stable
     */
    bool hasConverged(double threshold = 1.0) const {
        return getUncertainty() < threshold;
    }

private:
    // State vector: [CFO, CFO_rate]
    std::array<double, 2> state_;
    
    // State covariance matrix P (2x2)
    std::array<std::array<double, 2>, 2> covariance_;
    
    // Process noise covariance Q (2x2)
    std::array<std::array<double, 2>, 2> process_noise_;
    
    // Measurement noise covariance R (scalar for 1D measurement)
    double measurement_noise_;
    
    // State transition matrix A (2x2)
    // Models: CFO(k) = CFO(k-1) + CFO_rate(k-1)
    //         CFO_rate(k) = CFO_rate(k-1)
    static constexpr std::array<std::array<double, 2>, 2> A_ = {{
        {1.0, 1.0},  // CFO evolves with drift rate
        {0.0, 1.0}   // Drift rate is constant (random walk)
    }};
    
    // Measurement matrix H (1x2) - we only measure CFO directly
    static constexpr std::array<double, 2> H_ = {1.0, 0.0};
};

} // namespace CognitiveRF

#endif // KALMAN_CFO_TRACKER_HPP
