/**
 * @file DataStructures.hpp
 * @brief Core data structures for Direction Finding (DF) system
 * @details Defines structures for antenna configuration, DF parameters, and AoA results
 *
 * System: Bilişsel RF Spektrum Haritalama Sistemi
 * Module: 4.4 Yön Bulma (Direction Finding)
 *
 * References:
 * - 4.4_Yon_Bulma.txt: Technical specifications
 * - ED_pipeline.txt: Section [8] (Pseudo-Doppler DF)
 *
 * @author Senior C++ Software Architect
 * @date 2026
 */

#ifndef DIRECTION_FINDING_DATA_STRUCTURES_HPP
#define DIRECTION_FINDING_DATA_STRUCTURES_HPP

#include <complex>
#include <vector>
#include <string>
#include <cstdint>
#include <optional>
#include <deque>

namespace CognitiveRF {
namespace DirectionFinding {

/**
 * @struct AntennaSwitchConfig
 * @brief RF Anahtarlama Kartı Konfigürasyonu
 * @details Dairesel anten dizisinin anahtarlama parametreleri
 */
struct AntennaSwitchConfig {
    uint32_t num_antennas;                      ///< Anten sayısı (tipik: 4)
    float antenna_spacing_wavelengths;          ///< Dalga boyu cinsinden (λ)
    uint32_t switching_frequency_hz;            ///< Anahtarlama sıklığı (kHz mertebesi)
    uint32_t switching_period_us;               ///< Her anten için zaman (μs)
    std::vector<uint32_t> switch_gpio_pins;     ///< GPIO pin numaraları
    std::vector<std::string> antenna_labels;    ///< Anten isimleri (Ant1, Ant2, vb.)

    AntennaSwitchConfig()
        : num_antennas(4)
        , antenna_spacing_wavelengths(1.0f)
        , switching_frequency_hz(10000)         // 10 kHz
        , switching_period_us(25)               // 4x25μs = 100μs = 10kHz
    {}
};

/**
 * @struct CircularAntennaArray
 * @brief Dairesel Anten Dizisi Tanımı ve Kalibrasyonu
 * @details Anten konumları, fazlar ve magnitude düzeltmeleri
 */
struct CircularAntennaArray {
    uint32_t array_id;
    float diameter_meters;                      ///< Dizi çapı (metre)
    uint32_t num_elements;                      ///< Anten elemanları (4)
    std::vector<float> element_angles_deg;      ///< Derece cinsinden açılar [0°, 90°, 180°, 270°]
    std::vector<std::complex<float>> element_positions;  ///< Kompleks düzlemde konumlar

    /**
     * @struct ElementCalibration
     * @brief Anten kalibrasyonu (faz/magnitude düzeltmeler)
     */
    struct ElementCalibration {
        float magnitude_correction_db;          ///< Büyüklük düzeltmesi (dB)
        float phase_correction_radians;         ///< Faz düzeltmesi (radyan)
    };

    std::vector<ElementCalibration> calibration_data;

    CircularAntennaArray()
        : array_id(0)
        , diameter_meters(0.2f)                 // 200 mm
        , num_elements(4)
        , element_angles_deg{0.0f, 90.0f, 180.0f, 270.0f}
    {
        // Kompleks düzlemde dizi konumları başlat
        for (size_t i = 0; i < num_elements; i++) {
            float angle_rad = element_angles_deg[i] * M_PI / 180.0f;
            float radius = diameter_meters / 2.0f;
            element_positions.push_back(std::complex<float>(
                radius * std::cos(angle_rad),
                radius * std::sin(angle_rad)
            ));
            calibration_data.push_back({0.0f, 0.0f});  // No correction initially
        }
    }
};

/**
 * @struct DirectionFindingConfig
 * @brief DF Motor Konfigürasyonu
 * @details Pseudo-Doppler DF çalışması parametreleri
 */
struct DirectionFindingConfig {
    float center_frequency_hz;                  ///< Merkez frekansı (Hz)
    float bandwidth_hz;                         ///< Bant genişliği (Hz)
    float fm_demod_bandwidth_hz;                ///< FM demodülasyon band genişliği
    float phase_comparison_threshold;           ///< Faz karşılaştırma eşiği (radyan)
    float aoa_rms_tolerance_degrees;            ///< RMS hata toleransı (±10°)
    bool enable_calibration;                    ///< Kalibrasyonu etkinleştir
    uint32_t max_iterations;                    ///< Maximum yeniden deneme sayısı

    DirectionFindingConfig()
        : center_frequency_hz(435e6)            // 435 MHz (UHF)
        , bandwidth_hz(25e3)                    // 25 kHz
        , fm_demod_bandwidth_hz(10e3)           // 10 kHz
        , phase_comparison_threshold(0.1f)      // 0.1 rad
        , aoa_rms_tolerance_degrees(10.0f)      // ±10°
        , enable_calibration(true)
        , max_iterations(3)
    {}
};

/**
 * @struct DirectionFindingResult
 * @brief DF İşlemi Sonuç Yapısı
 * @details Kestirim edilen Geliş Açısı ve ilgili bilgiler
 *
 * Reference: 4.4_Yon_Bulma.txt - RMS Yön Doğruluğu bölümü
 */
struct DirectionFindingResult {
    uint64_t timestamp_us;                      ///< İşlem zamanı (μs)
    uint64_t tracking_id;                       ///< 4.3'ten sinyal tracking ID
    float center_frequency_hz;                  ///< Sinyal merkez frekansı

    // ===== DF SONUÇLARI =====
    float angle_of_arrival_degrees;             ///< AoA tahmini [-180°, 180°]
    float aoa_confidence;                       ///< Güven skoru [0.0, 1.0]
    float aoa_rms_error_degrees;                ///< Tahmini RMS hatası (±°)

    // ===== FAS BİLGİSİ =====
    float demodulated_phase_radians;            ///< Demodüle edilen sinyalin fazı
    float reference_phase_radians;              ///< Referans sinyalin fazı
    float phase_difference_radians;             ///< Faz farkı (rad)

    // ===== KALİTE METRİKLERİ =====
    float signal_strength_dbm;                  ///< Sinyal gücü (dBm)
    float snr_db;                               ///< Sinyal gürültü oranı (dB)

    // ===== DOĞRULAMA =====
    bool is_valid;                              ///< Sonuç geçerlidir mi?
    std::string validation_notes;               ///< Doğrulama notları
    uint32_t iteration_count;                   ///< Kaç iterasyon gerekti

    DirectionFindingResult()
        : timestamp_us(0)
        , tracking_id(0)
        , center_frequency_hz(0.0f)
        , angle_of_arrival_degrees(0.0f)
        , aoa_confidence(0.0f)
        , aoa_rms_error_degrees(0.0f)
        , demodulated_phase_radians(0.0f)
        , reference_phase_radians(0.0f)
        , phase_difference_radians(0.0f)
        , signal_strength_dbm(-120.0f)
        , snr_db(0.0f)
        , is_valid(false)
        , iteration_count(0)
    {}
};

/**
 * @struct PseudoDopplerSignal
 * @brief Pseudo-Doppler FM Modülasyonu Yapısı
 * @details Anten anahtarlama işlemi tarafından oluşturulan yapay Doppler sinyali
 */
struct PseudoDopplerSignal {
    std::vector<std::complex<float>> modulated_iq;  ///< Yapay modülasyonlu IQ
    std::vector<float> switching_pattern;           ///< Anahtarlama deseni
    uint64_t timestamp_us;
    uint32_t sample_rate_hz;

    PseudoDopplerSignal()
        : sample_rate_hz(20000000)  // 20 MSPS
    {}
};

/**
 * @struct DemodulatedPseudoDopplerSignal
 * @brief Pseudo-Doppler FM Demodülasyonu Sonucu
 * @details Demodüle edilmiş sinyal ve faz bilgisi
 */
struct DemodulatedPseudoDopplerSignal {
    std::vector<float> demodulated_signal;      ///< Demodüle edilmiş eğri
    std::deque<float> phase_history;            ///< Faz geçmişi
    float instantaneous_phase_radians;          ///< Anlık faz
    float phase_rate_rad_per_sample;            ///< Faz değişim hızı
    uint64_t timestamp_us;
    float snr_db;

    DemodulatedPseudoDopplerSignal()
        : instantaneous_phase_radians(0.0f)
        , phase_rate_rad_per_sample(0.0f)
        , timestamp_us(0)
        , snr_db(0.0f)
    {}
};

/**
 * @struct ReferenceSignal
 * @brief RF Anahtarlama Kontrolcüsünden Referans Sinyali
 * @details Anten anahtarlama sırası hakkında bilgi (kare dalga)
 */
struct ReferenceSignal {
    std::vector<float> reference_waveform;      ///< Kare dalga (0 veya 1)
    std::vector<float> reference_phase;         ///< Referans fazı her örnek
    uint32_t sample_rate_hz;
    uint32_t switching_frequency_hz;
    float reference_phase_radians;              ///< Anlık referans fazı

    ReferenceSignal()
        : sample_rate_hz(20000000)
        , switching_frequency_hz(10000)
        , reference_phase_radians(0.0f)
    {}
};

/**
 * @struct DirectionFindingStatistics
 * @brief DF Motor İstatistikleri
 * @details Performans metrikleri ve hata analizi
 */
struct DirectionFindingStatistics {
    uint64_t total_processed;                   ///< İşlenen toplam sinyal
    uint64_t valid_aoa_estimates;               ///< Geçerli AoA tahminleri
    float avg_aoa_confidence;                   ///< Ortalama güven
    float avg_aoa_error_degrees;                ///< Ortalama hata
    float min_aoa_error_degrees;                ///< Minimum hata
    float max_aoa_error_degrees;                ///< Maximum hata
    float avg_processing_time_ms;               ///< Ortalama işlem süresi (ms)

    DirectionFindingStatistics()
        : total_processed(0)
        , valid_aoa_estimates(0)
        , avg_aoa_confidence(0.0f)
        , avg_aoa_error_degrees(0.0f)
        , min_aoa_error_degrees(360.0f)
        , max_aoa_error_degrees(0.0f)
        , avg_processing_time_ms(0.0f)
    {}
};

} // namespace DirectionFinding
} // namespace CognitiveRF

#endif // DIRECTION_FINDING_DATA_STRUCTURES_HPP
