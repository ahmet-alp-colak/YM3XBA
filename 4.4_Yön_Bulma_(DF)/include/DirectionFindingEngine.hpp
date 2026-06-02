/**
 * @file DirectionFindingEngine.hpp
 * @brief Main Direction Finding Engine - Pseudo-Doppler DF Orchestrator
 * @details Coordinates antenna switching, FM demodulation, and phase comparison
 *
 * System: Bilişsel RF Spektrum Haritalama Sistemi
 * Module: 4.4 Yön Bulma (Direction Finding)
 *
 * Algorithm Flow:
 * 1. Anten Dizisini anahtarla → Yapay Doppler kayması oluştur
 * 2. IQ verisini FM demodüle et
 * 3. Referans sinyalin fazı ile demodüle edilen sinyalin fazını karşılaştır
 * 4. Faz farkından AoA kestir
 * 5. RMS tolerans kontrolü (±10°)
 *
 * @author Senior C++ Software Architect
 * @date 2026
 */

#ifndef DIRECTION_FINDING_ENGINE_HPP
#define DIRECTION_FINDING_ENGINE_HPP

#include "DataStructures.hpp"
#include "AntennaSwitcher.hpp"
#include "PseudoDopplerDemodulator.hpp"
#include "PhaseComparator.hpp"
#include <vector>
#include <complex>
#include <memory>
#include <cmath>

namespace CognitiveRF {
namespace DirectionFinding {

/**
 * @class DirectionFindingEngine
 * @brief Ana Yön Bulma Motoru - Pseudo-Doppler DF
 *
 * Sistemin çeşitli bileşenlerini koordine ederek DF işlemini gerçekleştirir:
 * - AntennaSwitcher: Dairesel anten anahtarlama
 * - PseudoDopplerDemodulator: FM demodülasyonu
 * - PhaseComparator: Faz karşılaştırması ve AoA hesaplama
 *
 * İlişkili Teknik Belgeler:
 * - 4.4_Yon_Bulma.txt (Teknik Spesifikasyonlar)
 * - ED_pipeline.txt Section [8] (Pseudo-Doppler DF)
 * - 4.4_Yon_Bulma.drawio (Akış şeması)
 */
class DirectionFindingEngine {
public:
    /**
     * @brief DirectionFindingEngine Yapıcısı
     * @param config DF motor konfigürasyonu
     * @param antenna_config Anten dizisi konfigürasyonu
     * @param antenna_array Anten dizisi tanımı ve kalibrasyonu
     */
    DirectionFindingEngine(
        const DirectionFindingConfig& config,
        const AntennaSwitchConfig& antenna_config,
        const CircularAntennaArray& antenna_array
    );

    /**
     * @brief Yıkıcı
     */
    ~DirectionFindingEngine();

    /**
     * @brief Pseudo-Doppler DF Çalışmasını Başlat
     *
     * Tam DF akışını gerçekleştirir:
     * 1. Anten anahtarlamasını konfigüre et
     * 2. IQ verisine pseudo-Doppler modülasyonu uygula
     * 3. FM demodülasyonu yap
     * 4. Referans sinyalin fazı ile karşılaştır
     * 5. AoA kestir
     *
     * @param iq_data Gelen IQ veri (20 MSPS)
     * @param center_freq_hz Merkez frekansı (Hz)
     * @param snr_db Sinyal gürültü oranı (dB)
     * @param rssi_dbm Sinyal gücü (dBm)
     * @return DirectionFindingResult AoA tahmini ve kalite metrikleri
     *
     * @throws std::runtime_error Anten anahtarlama başarısız ise
     */
    DirectionFindingResult performDirectionFinding(
        const std::vector<std::complex<float>>& iq_data,
        float center_freq_hz,
        float snr_db,
        float rssi_dbm
    );

    /**
     * @brief Anten Kalibrasyonu
     *
     * Anten dizisinin faz ve magnitude düzeltmelerini ayarla
     *
     * @param antenna_index Anten indeksi (0-3)
     * @param phase_correction Faz düzeltmesi (rad)
     * @param magnitude_correction Büyüklük düzeltmesi (dB)
     */
    void calibrateAntenna(
        uint32_t antenna_index,
        float phase_correction,
        float magnitude_correction
    );

    /**
     * @brief Kalibrasyonu Yeniden Sıfırla
     */
    void resetCalibration();

    /**
     * @brief Istatistikleri Döndür
     * @return Performans istatistikleri
     */
    const DirectionFindingStatistics& getStatistics() const;

    /**
     * @brief İstatistikleri Sıfırla
     */
    void resetStatistics();

    /**
     * @brief Konfigürasyonu Güncelle
     * @param config Yeni DF konfigürasyonu
     */
    void updateConfig(const DirectionFindingConfig& config);

    /**
     * @brief Anten Konfigürasyonunu Döndür
     * @return Aktif anten konfigürasyonu
     */
    const AntennaSwitchConfig& getAntennaConfig() const;

    /**
     * @brief Anten Dizisini Döndür
     * @return Anten dizisi bilgileri
     */
    const CircularAntennaArray& getAntennaArray() const;

private:
    DirectionFindingConfig config_;
    AntennaSwitchConfig antenna_switch_config_;
    CircularAntennaArray antenna_array_;

    std::unique_ptr<AntennaSwitcher> antenna_switcher_;
    std::unique_ptr<PseudoDopplerDemodulator> pseudo_doppler_demod_;
    std::unique_ptr<PhaseComparator> phase_comparator_;

    DirectionFindingStatistics statistics_;

    /**
     * @brief Yapay Doppler Modülasyonu Uygula
     *
     * IQ verisine dairesel anten anahtarlama simülasyonunu
     * FM modülasyonu olarak uygula
     *
     * @param iq_data Orijinal IQ veri
     * @param center_freq_hz Merkez frekansı
     * @return Pseudo-Doppler modülasyonlu veri
     */
    PseudoDopplerSignal generatePseudoDopplerSignal(
        const std::vector<std::complex<float>>& iq_data,
        float center_freq_hz
    );

    /**
     * @brief Referans Sinyali Oluştur
     *
     * Anten anahtarlama sırasından referans sinyal (kare dalga) oluştur
     *
     * @param num_samples Örnek sayısı
     * @return ReferenceSignal
     */
    ReferenceSignal generateReferenceSignal(uint32_t num_samples);

    /**
     * @brief Sonuç Doğrulaması
     *
     * AoA sonucunun RMS hata toleransı içinde olup olmadığını kontrol et
     *
     * @param result Doğrulanacak sonuç
     * @return Doğrulama başarılı mı?
     */
    bool validateResult(DirectionFindingResult& result);

    /**
     * @brief İstatistikleri Güncelle
     * @param result İşlem sonucu
     */
    void updateStatistics(const DirectionFindingResult& result);
};

} // namespace DirectionFinding
} // namespace CognitiveRF

#endif // DIRECTION_FINDING_ENGINE_HPP
