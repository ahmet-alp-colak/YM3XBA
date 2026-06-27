/**
 * @file GeolocationPipeline.hpp
 * @brief Thread-safe asynchronous geolocation pipeline
 * @details Integrates with 4.4 Direction Finding module output
 *
 * System: Bilişsel RF Spektrum Haritalama Sistemi
 * Module: 4.5 Konum Belirleme (Geolocation)
 *
 * @author Senior C++ Software Architect
 * @date 2026
 */

#ifndef GEOLOCATION_PIPELINE_HPP
#define GEOLOCATION_PIPELINE_HPP

#include "DataStructures.hpp"
#include "GeolocationEngine.hpp"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <functional>

namespace CognitiveRF {
namespace Geolocation {

/**
 * @class GeolocationPipeline
 * @brief Thread-safe asenkron geolocation pipeline
 * @details Thread-5 (ED_pipeline.txt) implementation
 */
class GeolocationPipeline {
public:
    using ResultCallback = std::function<void(const GeolocationResult&)>;
    
    struct PipelineConfig {
        GeolocationConfig geolocation_config;
        uint32_t input_queue_size;      ///< Maksimum queue boyutu
        uint32_t result_queue_size;
        
        PipelineConfig()
            : input_queue_size(100)
            , result_queue_size(50)
        {}
    };
    
    /**
     * @brief Constructor
     */
    explicit GeolocationPipeline(const PipelineConfig& config = PipelineConfig());
    
    /**
     * @brief Destructor
     */
    ~GeolocationPipeline();
    
    /**
     * @brief Pipeline'ı başlat
     */
    void start();
    
    /**
     * @brief Pipeline'ı durdur
     */
    void stop();
    
    /**
     * @brief AoA measurements gönder (4.4 modülünden)
     * @param measurements AoA measurement batch
     * @return true = başarılı
     */
    bool submitMeasurements(const std::vector<AoAMeasurement>& measurements);
    
    /**
     * @brief Sonuç al (blocking)
     * @param result Çıktı parametresi
     * @param timeout_ms Timeout (ms)
     * @return true = sonuç alındı
     */
    bool getResult(GeolocationResult& result, uint32_t timeout_ms = 1000);
    
    /**
     * @brief Result callback ayarla (non-blocking notification)
     */
    void setResultCallback(ResultCallback callback);
    
    /**
     * @brief İstatistikleri al
     */
    GeolocationStatistics getStatistics() const;
    
    /**
     * @brief Pipeline çalışıyor mu
     */
    bool isRunning() const { return running_.load(); }

private:
    PipelineConfig config_;
    GeolocationEngine engine_;
    
    std::thread processing_thread_;
    std::atomic<bool> running_;
    
    // Input queue
    std::queue<std::vector<AoAMeasurement>> input_queue_;
    std::mutex input_mutex_;
    std::condition_variable input_cv_;
    
    // Output queue
    std::queue<GeolocationResult> result_queue_;
    std::mutex result_mutex_;
    std::condition_variable result_cv_;
    
    // Callback
    ResultCallback result_callback_;
    std::mutex callback_mutex_;
    
    /**
     * @brief Processing thread ana döngüsü
     */
    void processingLoop();
};

} // namespace Geolocation
} // namespace CognitiveRF

#endif // GEOLOCATION_PIPELINE_HPP
