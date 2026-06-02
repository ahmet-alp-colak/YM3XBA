/**
 * @file PhaseComparator.hpp
 * @brief Phase Comparison and Angle of Arrival (AoA) Estimation
 * @details Compares phase of demodulated signal with reference signal to estimate AoA
 *
 * System: Bilişsel RF Spektrum Haritalama Sistemi
 * Module: 4.4 Yön Bulma (Direction Finding)
 *
 * Core Equation:
 * AoA [degrees] = f(Δφ) = (Δφ / 2π) * (360° / num_antennas) * phase_to_angle_constant
 *
 * Relationship between phase difference and angle:
 * - Δφ = phase_demodulated - phase_reference
 * - AoA = (Δφ / 2π) * (360° / 4) = (Δφ / 2π) * 90°
 *
 * Expected RMS accuracy: ±10° (per 4.4_Yon_Bulma.txt)
 *
 * @author Senior C++ Software Architect
 * @date 2026
 */

#ifndef PHASE_COMPARATOR_HPP
#define PHASE_COMPARATOR_HPP

#include "DataStructures.hpp"
#include <vector>
#include <deque>
#include <cmath>
#include <memory>

namespace CognitiveRF {
namespace DirectionFinding {

/**
 * @class PhaseComparator
 * @brief Faz Karşılaştırması ve AoA Tahmini
 *
 * Demodüle edilen sinyalin fazı ile RF anahtarlama kontrolcüsünün
 * referans sinyalinin fazını karşılaştırarak hedefin Geliş Açısını (AoA)
 * hesaplar.
 *
 * İşleyiş:
 * 1. Demodüle edilen sinyal fazı al
 * 2. Referans sinyal fazı al
 * 3. Faz farkını hesapla: Δφ = φ_demod - φ_ref
 * 4. Δφ'yi AoA'ya dönüştür (derece cinsinden)
 * 5. RMS tolerans kontrolü (±10°)
 *
 * Matematiksel Model:
 * ```
 * Dairesel anten dizisinde 4 eleman için:
 * φ[n] = 2π * (f_c / c) * d * sin(θ) * cos(φ_anten[n])
 *
 * Burada:
 * f_c: Merkez frekansı
 * c: Işık hızı
 * d: Anten arası mesafe
 * θ: Azimut açısı (AoA)
 * φ_anten[n]: Anten n'in açısı
 * ```
 */
class PhaseComparator {
public:
    /**
     * @brief PhaseComparator Yapıcısı
     * @param antenna_array Anten dizisi bilgileri
     * @param center_frequency_hz Merkez frekansı
     */
    PhaseComparator(
        const CircularAntennaArray& antenna_array,
        float center_frequency_hz
    );

    /**
     * @brief Yıkıcı
     */
    ~PhaseComparator();

    /**
     * @brief Faz Karşılaştırması ve AoA Tahmini
     *
     * Demodüle edilen sinyalin fazı ile referans sinyalinin fazını
     * karşılaştırarak AoA'yı hesapla
     *
     * @param demodulated_phase Demodüle edilen sinyal fazı (rad)
     * @param reference_phase Referans sinyal fazı (rad)
     * @param snr_db Sinyal gürültü oranı (dB)
     * @return DirectionFindingResult AoA tahmini ve kalite metrikleri
     */
    DirectionFindingResult comparePhases(
        float demodulated_phase,
        float reference_phase,
        float snr_db
    );

    /**
     * @brief Faz Farkını AoA'ya Dönüştür
     *
     * Faz farkı → Azimut açısı (derece)
     *
     * @param phase_difference Faz farkı (radyan) [-π, π]
     * @return AoA (derece) [-180°, 180°]
     */
    float phaseToAngle(float phase_difference) const;

    /**
     * @brief AoA'yı Faz Farkına Dönüştür (Ters işlem)
     *
     * @param angle_degrees Azimut açısı (derece)
     * @return Faz farkı (radyan)
     */
    float angleToPhase(float angle_degrees) const;

    /**
     * @brief Güven Skoru Hesapla
     *
     * SNR'a dayalı olarak AoA tahmininin güvenilirliğini hesapla.
     * Daha yüksek SNR → Daha yüksek güven
     *
     * @param snr_db Sinyal gürültü oranı (dB)
     * @return Güven skoru [0.0, 1.0]
     */
    float computeConfidence(float snr_db) const;

    /**
     * @brief RMS Hatasını Tahmin Et
     *
     * SNR ve anten geometrisine dayalı olarak
     * beklenen RMS hatasını tahmin et
     *
     * @param snr_db Sinyal gürültü oranı (dB)
     * @return Tahmini RMS hatası (derece)
     */
    float estimateRMSError(float snr_db) const;

    /**
     * @brief Geçmiş AoA Tahminlerini Döndür
     * @param window_size Pencere boyutu
     * @return Son N tahminin ortalaması (derece)
     */
    float getAverageAoA(size_t window_size = 10) const;

    /**
     * @brief AoA Geçmişini Döndür
     * @return Tüm geçmiş tahminler
     */
    const std::deque<float>& getAoAHistory() const { return aoa_history_; }

    /**
     * @brief Faz Farkı Geçmişini Döndür
     * @return Tüm geçmiş faz farkları
     */
    const std::deque<float>& getPhaseDifferenceHistory() const { return phase_diff_history_; }

    /**
     * @brief Kalibrasyonu Sıfırla
     */
    void resetCalibration();

    /**
     * @brief Merkez Frekansını Döndür
     * @return Frekans (Hz)
     */
    float getCenterFrequency() const { return center_frequency_hz_; }

    /**
     * @brief Merkez Frekansını Güncelle
     * @param freq_hz Yeni frekans
     */
    void setCenterFrequency(float freq_hz) { center_frequency_hz_ = freq_hz; }

    /**
     * @brief Geçmişi Temizle
     */
    void clearHistory();

private:
    CircularAntennaArray antenna_array_;
    float center_frequency_hz_;
    std::deque<float> aoa_history_;             // Geçmiş AoA tahminleri
    std::deque<float> phase_diff_history_;      // Geçmiş faz farkları
    std::deque<float> confidence_history_;      // Geçmiş güven skorları
    static constexpr size_t MAX_HISTORY_SIZE = 1024;
    static constexpr float SPEED_OF_LIGHT = 3.0e8f;  // m/s

    /**
     * @brief Doğrusal Faz-AoA Dönüşümü Katsayılarını Hesapla
     *
     * Kullanılan doğrusal model:
     * AoA = k1 * Δφ + k0
     *
     * Burada k1 = (360° / 4) / (2π) = 90° / 2π
     */
    void computeTransformCoefficients();

    float phase_to_angle_scale_;  // Ölçek faktörü (radyan → derece)
    float phase_to_angle_offset_; // Offset

    /**
     * @brief Kalibrasyon Faktörlerini Uyarla
     *
     * Frekansına bağlı olarak dönüşüm faktörlerini güncelle
     */
    void updateTransformForFrequency();

    /**
     * @brief SNR'a Dayalı Güven Hesapla
     *
     * Lojistik model:
     * confidence = 1 / (1 + exp(-k * (snr - snr_mid)))
     * k = 0.5 (eğim), snr_mid = 10 dB (orta nokta)
     *
     * @param snr_db SNR (dB)
     * @return Güven [0.0, 1.0]
     */
    static float logisticConfidence(float snr_db);

    /**
     * @brief Faz-Güven İlişkisi
     *
     * Faz tahmininin stabiliteğine göre güven ayarla
     *
     * @param phase_variance Son N faz farkının varyansı
     * @return Güven ayarı [0.8, 1.0]
     */
    float computePhaseStabilityConfidence(float phase_variance) const;
};

} // namespace DirectionFinding
} // namespace CognitiveRF

#endif // PHASE_COMPARATOR_HPP
