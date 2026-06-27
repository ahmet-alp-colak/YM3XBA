/**
 * @file LeastSquaresOptimizer.hpp
 * @brief Least Squares optimization for geolocation
 * @details Minimizes perpendicular distance residuals
 *
 * System: Bilişsel RF Spektrum Haritalama Sistemi
 * Module: 4.5 Konum Belirleme (Geolocation)
 *
 * @author Senior C++ Software Architect
 * @date 2026
 */

#ifndef GEOLOCATION_LEAST_SQUARES_OPTIMIZER_HPP
#define GEOLOCATION_LEAST_SQUARES_OPTIMIZER_HPP

#include "DataStructures.hpp"
#include "LineOfBearingCalculator.hpp"
#include <vector>
#include <tuple>

namespace CognitiveRF {
namespace Geolocation {

/**
 * @class LeastSquaresOptimizer
 * @brief En Küçük Kareler Optimizasyonu
 * @details LOB'lara dik uzaklık karelerini minimize eder
 */
class LeastSquaresOptimizer {
public:
    /**
     * @brief Least Squares ile konum optimizasyonu
     * @param lobs LOB vektörü
     * @param initial_guess Başlangıç tahmini (x, y)
     * @param config Optimizasyon konfigürasyonu
     * @return tuple<converged, final_position, rms_residual, iterations>
     */
    static std::tuple<bool, std::tuple<double, double>, float, uint32_t> optimize(
        const std::vector<LineOfBearing>& lobs,
        const std::tuple<double, double>& initial_guess,
        const GeolocationConfig& config
    );
    
    /**
     * @brief RMS (Root Mean Square) residual hesapla
     * @param lobs LOB vektörü
     * @param position Test noktası (x, y)
     * @return RMS değeri (metre)
     */
    static float computeRMSResidual(
        const std::vector<LineOfBearing>& lobs,
        const std::tuple<double, double>& position
    );
    
    /**
     * @brief Hata kovaryans matrisinden belirsizlik elipsi hesapla
     * @param lobs LOB vektörü
     * @param position Optimum nokta
     * @return UncertaintyEllipse
     */
    static UncertaintyEllipse computeUncertaintyEllipse(
        const std::vector<LineOfBearing>& lobs,
        const std::tuple<double, double>& position
    );

private:
    /**
     * @brief Gradient descent step
     * @return tuple<dx, dy> hareket yönü
     */
    static std::tuple<double, double> computeGradient(
        const std::vector<LineOfBearing>& lobs,
        const std::tuple<double, double>& position
    );
    
    /**
     * @brief Cost function (objective)
     * @return Toplam kare hata
     */
    static double computeCost(
        const std::vector<LineOfBearing>& lobs,
        const std::tuple<double, double>& position
    );
};

} // namespace Geolocation
} // namespace CognitiveRF

#endif // GEOLOCATION_LEAST_SQUARES_OPTIMIZER_HPP
