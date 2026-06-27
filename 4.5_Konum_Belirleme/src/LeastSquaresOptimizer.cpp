/**
 * @file LeastSquaresOptimizer.cpp
 * @brief Implementation of Least Squares optimizer
 *
 * System: Bilişsel RF Spektrum Haritalama Sistemi
 * Module: 4.5 Konum Belirleme (Geolocation)
 *
 * @author Senior C++ Software Architect
 * @date 2026
 */

#include "../include/LeastSquaresOptimizer.hpp"
#include <cmath>
#include <algorithm>

namespace CognitiveRF {
namespace Geolocation {

std::tuple<bool, std::tuple<double, double>, float, uint32_t> LeastSquaresOptimizer::optimize(
    const std::vector<LineOfBearing>& lobs,
    const std::tuple<double, double>& initial_guess,
    const GeolocationConfig& config
) {
    if (lobs.size() < 2) {
        return std::make_tuple(false, initial_guess, 0.0f, 0);
    }
    
    auto [x, y] = initial_guess;
    const double learning_rate = 10.0;  // metre/iterasyon
    const double decay = 0.95;
    
    uint32_t iteration = 0;
    bool converged = false;
    double alpha = learning_rate;
    
    for (; iteration < config.max_iterations; iteration++) {
        // Gradient hesapla
        auto [grad_x, grad_y] = computeGradient(lobs, std::make_tuple(x, y));
        
        // Gradient magnitude
        const double grad_magnitude = std::sqrt(grad_x * grad_x + grad_y * grad_y);
        
        // Convergence check
        if (grad_magnitude < config.convergence_tolerance_meters) {
            converged = true;
            break;
        }
        
        // Update position (gradient descent)
        x -= alpha * grad_x;
        y -= alpha * grad_y;
        
        // Learning rate decay
        alpha *= decay;
    }
    
    // Final RMS hesapla
    const float rms = computeRMSResidual(lobs, std::make_tuple(x, y));
    
    // Convergence validation
    if (rms > config.max_residual_rms_meters) {
        converged = false;
    }
    
    return std::make_tuple(converged, std::make_tuple(x, y), rms, iteration);
}

float LeastSquaresOptimizer::computeRMSResidual(
    const std::vector<LineOfBearing>& lobs,
    const std::tuple<double, double>& position
) {
    if (lobs.empty()) return 0.0f;
    
    auto [x, y] = position;
    double sum_squared = 0.0;
    
    for (const auto& lob : lobs) {
        const double dist = LineOfBearingCalculator::perpendicularDistance(lob, x, y);
        sum_squared += dist * dist;
    }
    
    return static_cast<float>(std::sqrt(sum_squared / lobs.size()));
}

UncertaintyEllipse LeastSquaresOptimizer::computeUncertaintyEllipse(
    const std::vector<LineOfBearing>& lobs,
    const std::tuple<double, double>& position
) {
    UncertaintyEllipse ellipse;
    
    if (lobs.size() < 2) {
        ellipse.semi_major_axis_meters = 1000.0f;
        ellipse.semi_minor_axis_meters = 1000.0f;
        ellipse.orientation_degrees = 0.0f;
        return ellipse;
    }
    
    // Basitleştirilmiş hata elipsi: RMS residual'e ve GDOP'a dayalı
    const float rms = computeRMSResidual(lobs, position);
    const float gdop = GeodeticUtils::computeGDOP(lobs);
    
    // Elips eksenleri: RMS * GDOP * faktör
    const float scale_factor = 2.0f;  // ~95% güven aralığı
    ellipse.semi_major_axis_meters = rms * gdop * scale_factor;
    ellipse.semi_minor_axis_meters = rms * scale_factor;
    
    // Oryantasyon: ilk LOB yönü
    ellipse.orientation_degrees = lobs[0].bearing_degrees;
    
    return ellipse;
}

std::tuple<double, double> LeastSquaresOptimizer::computeGradient(
    const std::vector<LineOfBearing>& lobs,
    const std::tuple<double, double>& position
) {
    auto [x, y] = position;
    double grad_x = 0.0;
    double grad_y = 0.0;
    
    // Gradient of cost function: ∂C/∂x, ∂C/∂y
    // C = Σ d_perp²
    
    for (const auto& lob : lobs) {
        const double ox = lob.parametric.origin_x;
        const double oy = lob.parametric.origin_y;
        const double dx = lob.parametric.direction_x;
        const double dy = lob.parametric.direction_y;
        
        // Vector from origin to point
        const double vx = x - ox;
        const double vy = y - oy;
        
        // Projection onto line
        const double dot = vx * dx + vy * dy;
        
        // Perpendicular component
        const double perp_x = vx - dot * dx;
        const double perp_y = vy - dot * dy;
        
        // Gradient contribution: 2 * perp_component * (1 - dx²) for x
        // Simplified: gradient points towards line
        grad_x += 2.0 * perp_x;
        grad_y += 2.0 * perp_y;
    }
    
    return std::make_tuple(grad_x / lobs.size(), grad_y / lobs.size());
}

double LeastSquaresOptimizer::computeCost(
    const std::vector<LineOfBearing>& lobs,
    const std::tuple<double, double>& position
) {
    auto [x, y] = position;
    double cost = 0.0;
    
    for (const auto& lob : lobs) {
        const double dist = LineOfBearingCalculator::perpendicularDistance(lob, x, y);
        cost += dist * dist;
    }
    
    return cost;
}

} // namespace Geolocation
} // namespace CognitiveRF
