/**
 * @file GeolocationEngine.hpp
 * @brief Main geolocation engine orchestrator
 * @details Integrates all components: LOB, Cross-fixing, Least Squares
 *
 * System: Bilişsel RF Spektrum Haritalama Sistemi
 * Module: 4.5 Konum Belirleme (Geolocation)
 *
 * @author Senior C++ Software Architect
 * @date 2026
 */

#ifndef GEOLOCATION_ENGINE_HPP
#define GEOLOCATION_ENGINE_HPP

#include "DataStructures.hpp"
#include "GeodeticUtils.hpp"
#include "LineOfBearingCalculator.hpp"
#include "CrossFixingSolver.hpp"
#include "LeastSquaresOptimizer.hpp"
#include <vector>
#include <mutex>

namespace CognitiveRF {
namespace Geolocation {

/**
 * @class GeolocationEngine
 * @brief Ana Konum Belirleme Motoru
 * @details Cross-fixing + Least Squares pipeline'ını orkestre eder
 */
class GeolocationEngine {
public:
    /**
     * @brief Constructor
     * @param config Geolocation konfigürasyonu
     */
    explicit GeolocationEngine(const GeolocationConfig& config = GeolocationConfig());
    
    /**
     * @brief Tam geolocation işlemini gerçekleştir
     * @param measurements AoA measurements (2+ istasyon)
     * @return GeolocationResult
     */
    GeolocationResult performGeolocation(const std::vector<AoAMeasurement>& measurements);
    
    /**
     * @brief Konfigürasyon güncelle
     */
    void updateConfig(const GeolocationConfig& config);
    
    /**
     * @brief İstatistikleri al
     */
    GeolocationStatistics getStatistics() const;
    
    /**
     * @brief İstatistikleri sıfırla
     */
    void resetStatistics();

private:
    GeolocationConfig config_;
    StationInfo reference_station_;  ///< Lokal Kartezyen origin
    GeolocationStatistics statistics_;
    mutable std::mutex mutex_;
    
    /**
     * @brief Ölçümleri valide et
     */
    bool validateMeasurements(const std::vector<AoAMeasurement>& measurements,
                             std::string& error_msg);
    
    /**
     * @brief LOB'ları oluştur
     */
    std::vector<LineOfBearing> createLOBs(const std::vector<AoAMeasurement>& measurements);
    
    /**
     * @brief İstatistikleri güncelle
     */
    void updateStatistics(const GeolocationResult& result, double processing_time_ms);
};

} // namespace Geolocation
} // namespace CognitiveRF

#endif // GEOLOCATION_ENGINE_HPP
