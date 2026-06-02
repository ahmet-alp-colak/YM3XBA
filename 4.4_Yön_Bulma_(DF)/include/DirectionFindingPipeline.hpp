/**
 * @file DirectionFindingPipeline.hpp
 * @brief Thread-Safe Asynchronous Direction Finding Pipeline
 * @details Orchestrates antenna switching, demodulation, and phase comparison
 *
 * System: Bilişsel RF Spektrum Haritalama Sistemi
 * Module: 4.4 Yön Bulma (Direction Finding)
 *
 * Architecture (follows 4.3_Sinyal_Izleme-Dinleme pattern):
 * - Thread-safe input/output queues
 * - Asynchronous processing thread
 * - Real-time callbacks for results
 * - Performance statistics tracking
 *
 * Threading Model:
 * ```
 * Main Thread           DF Pipeline Thread          Output
 * ============           ==================          ======
 * submitIQData()  →  Queue  →  Processing Loop  →  getResult()
 *                                    ↓
 *                            Direction Finding Engine
 *                                    ↓
 *                           Callback (optional)
 * ```
 *
 * @author Senior C++ Software Architect
 * @date 2026
 */

#ifndef DIRECTION_FINDING_PIPELINE_HPP
#define DIRECTION_FINDING_PIPELINE_HPP

#include "DataStructures.hpp"
#include "DirectionFindingEngine.hpp"
#include <vector>
#include <complex>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <functional>
#include <chrono>
#include <memory>
#include <optional>

namespace CognitiveRF {
namespace DirectionFinding {

/**
 * @class DirectionFindingPipeline
 * @brief Ana DF Pipeline Orkestratörü
 *
 * 4.3 Sinyal İzleme/Dinleme modülünün çıktısını alarak
 * yön bulma işlemini gerçek zamanlı olarak gerçekleştirir.
 *
 * Özellikleri:
 * - Thread-safe asenkron işleme
 * - Geri çağırma mekanizması (callbacks)
 * - Performans istatistikleri
 * - Girdi/Çıktı kuyrukları
 *
 * Entegrasyon:
 * 4.3 MonitoredSignal → 4.4 DirectionFindingPipeline → DirectionFindingResult
 */
class DirectionFindingPipeline {
public:
    /**
     * @struct PipelineConfig
     * @brief Pipeline Konfigürasyonu
     */
    struct PipelineConfig {
        uint32_t sample_rate_hz;                ///< Örnek hızı (20 MSPS)
        DirectionFindingConfig df_config;       ///< DF motor konfigürasyonu
        AntennaSwitchConfig antenna_config;     ///< Anten konfigürasyonu
        CircularAntennaArray antenna_array;     ///< Anten dizisi tanımı
        uint32_t max_queue_size;                ///< Maksimum kuyruk boyutu
        bool enable_statistics;                 ///< İstatistik takibi
        bool enable_callbacks;                  ///< Callback mekanizması

        PipelineConfig()
            : sample_rate_hz(20000000)
            , max_queue_size(100)
            , enable_statistics(true)
            , enable_callbacks(true)
        {}
    };

    /**
     * @typedef ResultCallback
     * @brief DF sonucu için callback fonksiyonu tipi
     *
     * İşleme tamamlandığında otomatik olarak çağrılır.
     */
    using ResultCallback = std::function<void(const DirectionFindingResult&)>;

    /**
     * @brief DirectionFindingPipeline Yapıcısı
     * @param config Pipeline konfigürasyonu
     */
    explicit DirectionFindingPipeline(const PipelineConfig& config = PipelineConfig());

    /**
     * @brief Yıkıcı
     */
    ~DirectionFindingPipeline();

    /**
     * @brief Pipeline'ı Başlat
     *
     * DF işleme thread'ini oluştur ve başlat.
     *
     * @return Başarılı mı?
     * @throws std::runtime_error Pipeline zaten çalışıyorsa
     */
    bool start();

    /**
     * @brief Pipeline'ı Durdur
     *
     * İşleme thread'ini kapat ve kaynakları temizle.
     */
    void stop();

    /**
     * @brief IQ Verisini Gönder
     *
     * DF işlemesi için IQ verisini giriş kuyruğuna ekle.
     *
     * @param iq_data IQ veri örnekleri
     * @param center_freq_hz Merkez frekansı (Hz)
     * @param snr_db Sinyal gürültü oranı (dB)
     * @param rssi_dbm Sinyal gücü (dBm)
     * @param tracking_id 4.3'ten sinyal ID (opsiyonel)
     * @return Başarılı mı?
     */
    bool submitIQData(
        const std::vector<std::complex<float>>& iq_data,
        float center_freq_hz,
        float snr_db,
        float rssi_dbm,
        uint64_t tracking_id = 0
    );

    /**
     * @brief Sonucu Al
     *
     * Çıktı kuyruğundan DF sonucunu al.
     *
     * @param result [çıkış] Elde edilen sonuç
     * @param timeout_ms Bekleme zamanı (ms). 0 = sonsuz
     * @return Sonuç elde edildi mi?
     *
     * @throws std::runtime_error Pipeline çalışmıyorsa
     */
    bool getResult(DirectionFindingResult& result, uint32_t timeout_ms = 1000);

    /**
     * @brief Callback Fonksiyonunu Ayarla
     *
     * Her DF sonucu elde edildiğinde otomatik çağrılacak fonksiyon
     *
     * @param callback Çağrılacak fonksiyon
     *
     * Örnek:
     * ```cpp
     * pipeline.setResultCallback([](const DirectionFindingResult& result) {
     *     if (result.is_valid) {
     *         std::cout << "AoA: " << result.angle_of_arrival_degrees << "°\n";
     *     }
     * });
     * ```
     */
    void setResultCallback(const ResultCallback& callback);

    /**
     * @brief Pipeline İstatistiklerini Döndür
     *
     * İşleme performansı ve kalite metrikleri
     *
     * @return DirectionFindingStatistics İstatistik yapısı
     */
    const DirectionFindingStatistics& getStatistics() const;

    /**
     * @brief İstatistikleri Sıfırla
     */
    void resetStatistics();

    /**
     * @brief Kuyruk Dolu Mu?
     * @return Kuyruk dolu mu?
     */
    bool isInputQueueFull() const;

    /**
     * @brief Kuyrukta Sonuç Var Mı?
     * @return Çıktı kuyruğunda bekleyen sonuç var mı?
     */
    bool hasResult() const;

    /**
     * @brief Pipeline Çalışıyor Mu?
     * @return Çalışıyor mu?
     */
    bool isRunning() const { return is_running_; }

    /**
     * @brief Giriş Kuyruğu Boyutunu Döndür
     * @return Kuyrukta bekleyen öğe sayısı
     */
    uint32_t getInputQueueSize() const;

    /**
     * @brief Çıktı Kuyruğu Boyutunu Döndür
     * @return Sonuç kuyruğunda bekleyen öğe sayısı
     */
    uint32_t getOutputQueueSize() const;

    /**
     * @brief DF Motor Referansını Döndür
     * @return Direktöy bulma motoru
     */
    DirectionFindingEngine* getDirectionFindingEngine() const { return df_engine_.get(); }

    /**
     * @brief Konfigürasyonu Güncelle
     * @param config Yeni pipeline konfigürasyonu
     */
    void updateConfig(const PipelineConfig& config);

private:
    PipelineConfig config_;
    std::unique_ptr<DirectionFindingEngine> df_engine_;

    // Thread management
    std::unique_ptr<std::thread> processing_thread_;
    std::atomic<bool> is_running_{false};

    // Input queue (IQ data + metadata)
    struct IQInputData {
        std::vector<std::complex<float>> iq_data;
        float center_freq_hz;
        float snr_db;
        float rssi_dbm;
        uint64_t tracking_id;
        uint64_t timestamp_us;
    };
    std::queue<IQInputData> input_queue_;
    mutable std::mutex input_queue_mutex_;
    std::condition_variable input_queue_cv_;

    // Output queue (DF results)
    std::queue<DirectionFindingResult> output_queue_;
    mutable std::mutex output_queue_mutex_;
    std::condition_variable output_queue_cv_;

    // Callback
    ResultCallback result_callback_;
    std::mutex callback_mutex_;

    // Statistics
    DirectionFindingStatistics statistics_;
    mutable std::mutex statistics_mutex_;

    /**
     * @brief İşleme Thread Ana Döngüsü
     *
     * Giriş kuyruğundan verileri al, DF işlemesini yap,
     * sonuçları çıktı kuyruğuna ve callback'lere gönder
     */
    void processingThreadLoop();

    /**
     * @brief Sonuç İstatistiklerini Güncelle
     * @param result Elde edilen sonuç
     */
    void updateStatistics(const DirectionFindingResult& result);
};

} // namespace DirectionFinding
} // namespace CognitiveRF

#endif // DIRECTION_FINDING_PIPELINE_HPP
