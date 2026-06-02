/**
 * @file AntennaSwitcher.hpp
 * @brief RF Antenna Switch Matrix Controller
 * @details Manages high-speed circular antenna switching for pseudo-Doppler DF
 *
 * System: Bilişsel RF Spektrum Haritalama Sistemi
 * Module: 4.4 Yön Bulma (Direction Finding)
 *
 * Hardware: 4-element circular antenna array + RF switch matrix
 * Switching Pattern: Antenna 1 → 2 → 3 → 4 → 1 (circular)
 * Switching Frequency: ~10 kHz (programmable)
 *
 * @author Senior C++ Software Architect
 * @date 2026
 */

#ifndef ANTENNA_SWITCHER_HPP
#define ANTENNA_SWITCHER_HPP

#include "DataStructures.hpp"
#include <vector>
#include <cstdint>
#include <chrono>
#include <thread>
#include <atomic>
#include <memory>

namespace CognitiveRF {
namespace DirectionFinding {

/**
 * @class AntennaSwitcher
 * @brief RF Antenna Switch Kartı Kontrolcüsü
 *
 * Dairesel anten dizisini yüksek hızda anahtarlayarak
 * gelen RF sinyalini yapay olarak modüle eder.
 *
 * İşleyiş:
 * 1. GPIO veya USB arayüzü üzerinden switch kartını kontrol et
 * 2. Belirli periyotta antenleri döngü sırası ile seç
 * 3. Referans sinyalini (kare dalga) üret
 * 4. Zamanlaması doğru olan depolanmış anahtarlama deseni döndür
 */
class AntennaSwitcher {
public:
    /**
     * @enum SwitchingMode
     * @brief Anahtarlama modu
     */
    enum class SwitchingMode {
        GPIO,           ///< Doğrudan GPIO kontrolü
        USB,            ///< USB arayüzü (gelecek)
        SIMULATED       ///< Yazılım simülasyonu (test/demo)
    };

    /**
     * @brief AntennaSwitcher Yapıcısı
     * @param config Anten anahtarlama konfigürasyonu
     * @param mode Anahtarlama modu (GPIO/USB/Simulated)
     */
    AntennaSwitcher(
        const AntennaSwitchConfig& config,
        SwitchingMode mode = SwitchingMode::SIMULATED
    );

    /**
     * @brief Yıkıcı
     */
    ~AntennaSwitcher();

    /**
     * @brief Anten Anahtarlamayı Başlat
     * @return Başarılı mı?
     */
    bool startSwitching();

    /**
     * @brief Anten Anahtarlamayı Durdur
     */
    void stopSwitching();

    /**
     * @brief Geçerli Anten İndeksini Döndür
     * @return Aktif anten (0-3)
     */
    uint32_t getCurrentAntennaIndex() const;

    /**
     * @brief Geçerli Anten Etiketini Döndür
     * @return Anten adı ("Ant1", "Ant2", vb.)
     */
    std::string getCurrentAntennaLabel() const;

    /**
     * @brief Referans Sinyal (Kare Dalga) Döndür
     *
     * Anten anahtarlama sırasının kare dalga temsili.
     * Faz karşılaştırmasında kullanılır.
     *
     * @param num_samples İstenilen örnek sayısı
     * @return ReferenceSignal yapısı
     */
    ReferenceSignal getReferenceSignal(uint32_t num_samples);

    /**
     * @brief Anahtarlama Deseni Döndür
     *
     * Tüm antenleri seçtiği sırayı içeren desen.
     * Örnek: [0, 1, 2, 3, 0, 1, 2, 3, ...] (dairesel)
     *
     * @param num_samples Örnek sayısı
     * @return Anahtarlama indeksleri
     */
    std::vector<uint32_t> getSwitchingPattern(uint32_t num_samples);

    /**
     * @brief Anahtarlama Frekansını Döndür
     * @return Frekans (Hz)
     */
    uint32_t getSwitchingFrequency() const;

    /**
     * @brief Anahtarlama Periyodunu Döndür
     * @return Periyot (μs)
     */
    uint32_t getSwitchingPeriod() const;

    /**
     * @brief Konfigürasyonu Güncelle
     * @param config Yeni konfigürasyon
     */
    void updateConfig(const AntennaSwitchConfig& config);

    /**
     * @brief Simülasyon Modunda Mı?
     * @return Simülasyon aktif mi?
     */
    bool isSimulated() const { return mode_ == SwitchingMode::SIMULATED; }

private:
    AntennaSwitchConfig config_;
    SwitchingMode mode_;
    std::atomic<bool> is_switching_{false};
    std::atomic<uint32_t> current_antenna_index_{0};
    std::unique_ptr<std::thread> switching_thread_;

    uint64_t last_switch_time_us_;
    std::vector<uint32_t> switching_sequence_;  // [0,1,2,3,0,1,2,3,...]
    uint32_t sequence_index_;

    /**
     * @brief Anahtarlama Thread'i Çalışması
     */
    void switchingThreadLoop();

    /**
     * @brief GPIO Üzerinden Switch Kartını Kontrol Et
     * @param antenna_index Seçilecek anten (0-3)
     * @return Başarılı mı?
     */
    bool controlGPIOSwitch(uint32_t antenna_index);

    /**
     * @brief Dairesel Dönüş Sekvansını Oluştur
     *
     * [0,1,2,3,0,1,2,3,...] şeklinde dizi
     */
    void initializeSwitchingSequence();
};

} // namespace DirectionFinding
} // namespace CognitiveRF

#endif // ANTENNA_SWITCHER_HPP
