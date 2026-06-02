/**
 * @file PseudoDopplerDemodulator.hpp
 * @brief Pseudo-Doppler FM Demodulator
 * @details FM demodulation of artificial Doppler shift created by antenna switching
 *
 * System: Bilişsel RF Spektrum Haritalama Sistemi
 * Module: 4.4 Yön Bulma (Direction Finding)
 *
 * The antenna switching creates an artificial FM modulation of the RF signal.
 * This demodulator extracts the phase information from this modulation.
 *
 * Algorithm:
 * Phase[n] = atan2(Q[n]*I[n-1] - I[n]*Q[n-1], I[n]*I[n-1] + Q[n]*Q[n-1])
 * Frequency deviation = Phase[n] / (2π)
 *
 * @author Senior C++ Software Architect
 * @date 2026
 */

#ifndef PSEUDO_DOPPLER_DEMODULATOR_HPP
#define PSEUDO_DOPPLER_DEMODULATOR_HPP

#include "DataStructures.hpp"
#include <vector>
#include <complex>
#include <cmath>
#include <deque>

namespace CognitiveRF {
namespace DirectionFinding {

/**
 * @class PseudoDopplerDemodulator
 * @brief Pseudo-Doppler FM Demodülatörü
 *
 * Anten anahtarlama işleminin RF sinyalinde yarattığı yapay FM
 * modülasyonunu demodüle ederek sinyal fazını çıkarır.
 *
 * Kullanılan Algoritma: Frequency Discriminator (Phase Derivative Method)
 *
 * FM Demodülasyon Formülü:
 * ```
 * y[n] = (1/2π) * arg(conj(x[n-1]) * x[n])
 *      = (1/2π) * atan2(Im(conj(x[n-1])*x[n]), Re(conj(x[n-1])*x[n]))
 * ```
 *
 * Çıktı:
 * - Demodüle edilmiş eğri (normalized frequency deviation)
 * - Faz geçmişi (phase history)
 * - Anlık faz değeri (instantaneous phase)
 * - Faz değişim hızı (phase rate)
 */
class PseudoDopplerDemodulator {
public:
    /**
     * @brief Demodülatör Yapıcısı
     * @param sample_rate_hz Örnek hızı (tipik: 20 MSPS)
     * @param bandwidth_hz Demodülasyon band genişliği (Hz)
     */
    PseudoDopplerDemodulator(
        uint32_t sample_rate_hz,
        float bandwidth_hz
    );

    /**
     * @brief Yıkıcı
     */
    ~PseudoDopplerDemodulator();

    /**
     * @brief Pseudo-Doppler FM Demodülasyonu
     *
     * IQ verisini demodüle et ve faz bilgisini çıkar.
     *
     * @param iq_data Gelen IQ veri
     * @param snr_db Sinyal gürültü oranı
     * @return DemodulatedPseudoDopplerSignal Demodüle edilmiş sinyal ve faz
     *
     * @throws std::invalid_argument Eğer iq_data boş ise
     */
    DemodulatedPseudoDopplerSignal demodulate(
        const std::vector<std::complex<float>>& iq_data,
        float snr_db
    );

    /**
     * @brief Faz Türevini Hesapla
     *
     * Ardışık IQ örneklerinden faz değişimini hesapla.
     *
     * @param iq_sample_prev Önceki örnek
     * @param iq_sample_curr Şimdiki örnek
     * @return Faz türevi (rad/sample)
     */
    float computePhaseDerivative(
        std::complex<float> iq_sample_prev,
        std::complex<float> iq_sample_curr
    ) const;

    /**
     * @brief Anlık Fazı Döndür
     * @return Faz (radyan)
     */
    float getInstantaneousPhase() const { return instantaneous_phase_; }

    /**
     * @brief Faz Geçmişini Döndür
     * @return Son N örneğin faz değerleri
     */
    const std::deque<float>& getPhaseHistory() const { return phase_history_; }

    /**
     * @brief Faz Geçmişi Penceresini Döndür
     * @param max_size Maksimum geçmiş boyutu
     * @return Son max_size örneğin fazları
     */
    std::vector<float> getPhaseHistoryWindow(size_t max_size = 1024) const;

    /**
     * @brief Demodülasyonu Sıfırla
     */
    void reset();

    /**
     * @brief Bant Genişliğini Döndür
     * @return Bandwidth (Hz)
     */
    float getBandwidth() const { return bandwidth_hz_; }

    /**
     * @brief Örnek Hızını Döndür
     * @return Sample rate (Hz)
     */
    uint32_t getSampleRate() const { return sample_rate_hz_; }

    /**
     * @brief Ortalama SNR Döndür
     * @return SNR (dB)
     */
    float getAverageSNR() const { return avg_snr_db_; }

private:
    uint32_t sample_rate_hz_;
    float bandwidth_hz_;
    std::deque<float> phase_history_;
    std::deque<std::complex<float>> iq_history_;  // Kayan pencere
    float instantaneous_phase_;
    float phase_rate_;
    float avg_snr_db_;
    static constexpr size_t MAX_HISTORY_SIZE = 4096;

    /**
     * @brief Faz Unwrapping
     *
     * [-π, π] aralığında sıçrayan faz değerlerini
     * sürekli eğriye dönüştür
     *
     * @param phase Wrapped faz
     * @param previous_phase Önceki faz değeri
     * @return Unwrapped faz
     */
    float unwrapPhase(float phase, float previous_phase) const;

    /**
     * @brief Faz Normalleştirmesi
     *
     * Fazı [-π, π] aralığına sıkıştır
     *
     * @param phase Normalleştirilecek faz
     * @return Normalized faz
     */
    static float normalizePhase(float phase);

    /**
     * @brief Tümleştirme Filtresi (Lowpass)
     *
     * Gürültüyü bastırmak için faz geçmişine
     * basit moving average uygula
     *
     * @param window_size Pencere boyutu
     * @return Filtered phase
     */
    float applyPhaseFilter(size_t window_size = 10) const;
};

} // namespace DirectionFinding
} // namespace CognitiveRF

#endif // PSEUDO_DOPPLER_DEMODULATOR_HPP
