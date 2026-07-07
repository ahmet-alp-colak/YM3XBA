# 5.1 Sürekli Karıştırma (Spot Jamming) - Nasıl Çalıştırılır?

## 📋 İçindekiler
1. [Sistem Gereksinimleri](#sistem-gereksinimleri)
2. [Kurulum](#kurulum)
3. [Derleme](#derleme)
4. [Temel Kullanım](#temel-kullanım)
5. [İleri Seviye Yapılandırma](#ileri-seviye-yapılandırma)
6. [Sorun Giderme](#sorun-giderme)

---

## 🖥️ Sistem Gereksinimleri

### Donanım
- **SDR**: HackRF One (TX: 1 MHz - 3 GHz)
- **CPU**: Çok çekirdekli işlemci (4+ core önerilir)
- **RAM**: Minimum 4 GB
- **İşletim Sistemi**: Linux (Ubuntu 20.04+ önerilir)

### Yazılım
- **Derleyici**: GCC 7+ veya Clang 5+
- **CMake**: 3.15 veya üzeri
- **Kütüphaneler**: pthread, libhackrf

---

## 📦 Kurulum

```bash
# 1. Depoyu klonlayın
git clone https://github.com/ahmet-alp-colak/YM3XBA.git
cd YM3XBA/5.1_Surekli_Karistirma

# 2. Bağımlılıkları yükleyin
sudo apt update
sudo apt install build-essential cmake libhackrf-dev

# 3. HackRF test edin
hackrf_info
```

---

## 🔨 Derleme

```bash
# Debug derlemesi
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)

# Release derlemesi (performans)
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)

# Örnek programı çalıştırın
./example_usage
```

---

## 🚀 Temel Kullanım

```cpp
#include "SpotJammingPipeline.hpp"
using namespace CognitiveRF::SpotJamming;

int main() {
    // 1. Yapılandırma
    PipelineConfig config;
    config.hardware.center_freq_hz = 433.92e6;
    config.hardware.bandwidth_hz = 25e3;
    config.signal.type = JammingType::GAUSSIAN_NOISE;
    
    // 2. Pipeline oluştur
    SpotJammingPipeline pipeline(config);
    pipeline.start();
    
    // 3. Hedef bilgisi
    TargetIntelligence target;
    target.target_id = 12345;
    target.center_freq_hz = 433.92e6;
    
    // 4. Karıştırmayı başlat
    JammingCommand cmd;
    cmd.action = JammingCommand::Action::START;
    cmd.target = target;
    pipeline.submitCommand(cmd);
    
    // 5. İzle ve durdur
    JammingReport report;
    while (pipeline.getStatus(report)) {
        std::cout << "Samples: " << report.samples_transmitted << std::endl;
    }
    
    cmd.action = JammingCommand::Action::STOP;
    pipeline.submitCommand(cmd);
    pipeline.stop();
    
    return 0;
}
```

---

## ⚙️ İleri Seviye Yapılandırma

### Karıştırma Tipleri

**CW (Continuous Wave):**
```cpp
config.signal.type = JammingType::CONTINUOUS_WAVE;
config.signal.cw_amplitude = 0.8f;
```

**Gaussian Noise (Önerilen):**
```cpp
config.signal.type = JammingType::GAUSSIAN_NOISE;
config.signal.noise_amplitude = 0.7f;
```

**Modulated Noise:**
```cpp
config.signal.type = JammingType::MODULATED_NOISE;
config.signal.modulation_rate_hz = 1000.0;
```

### HackRF Kazanç Ayarları
```cpp
config.hardware.lna_gain_db = 0;
config.hardware.vga_gain_db = 20;
config.hardware.tx_amp_enable = true;
```



---

## 🐛 Sorun Giderme

### "Pipeline başlatılamadı"
- Konfigürasyonu doğrulayın: `config.validate()`
- HackRF bağlı mı: `hackrf_info`
- İzinleri kontrol edin: `sudo usermod -aG plugdev $USER`

### "Hardware state transition failed"
- HackRF başka program tarafından kullanılıyor olabilir
- `killall hackrf_transfer` ile çakışan programları kapatın
- USB kablosunu çıkarıp tekrar takın

### "TX errors > 0"
Buffer underrun - işlemci yetişemiyor:
```cpp
config.hardware.tx_buffer_samples = 16384;  // Buffer boyutunu artır
```

### Düşük Performans
```bash
# Release modda derleyin
cmake -DCMAKE_BUILD_TYPE=Release ..

# CPU governor'ı performance moduna alın
sudo cpupower frequency-set -g performance
```

---

## 📊 Test Senaryoları

### 433 MHz ISM Band Karıştırma
```cpp
config.hardware.center_freq_hz = 433.92e6;
config.hardware.bandwidth_hz = 25e3;
config.signal.type = JammingType::GAUSSIAN_NOISE;
```

### 2.4 GHz WiFi Karıştırma
```cpp
config.hardware.center_freq_hz = 2.437e9;  // Kanal 6
config.hardware.bandwidth_hz = 20e6;
config.signal.type = JammingType::GAUSSIAN_NOISE;
```

### Dar Bant CW Karıştırma
```cpp
config.hardware.center_freq_hz = 150e6;
config.hardware.bandwidth_hz = 12.5e3;
config.signal.type = JammingType::CONTINUOUS_WAVE;
```

---

## 🔍 Debugging

```cpp
// Log çıktısını etkinleştir
config.enable_logging = true;
config.log_prefix = "[DEBUG]";

// Callback ile detaylı izleme
pipeline.setStatusCallback([](const JammingReport& report) {
    std::cout << "Samples: " << report.samples_transmitted 
              << " | Errors: " << report.tx_errors << std::endl;
});
```

---

## ⚠️ Güvenlik Uyarıları

1. **Yasal Uyarı**: RF karıştırma birçok ülkede yasadışıdır. Sadece yetkili test ortamlarında kullanın.
2. **Termal Koruma**: HackRF uzun süreli kullanımda ısınabilir.
3. **Duty Cycle**: Sürekli TX donanıma zarar verebilir.
4. **Spektrum**: İstenmeden kritik sistemlere müdahale etmeyin.

---

## 📞 Destek

- **Dokümantasyon**: README.md
- **Örnek Kod**: examples/example_usage.cpp
- **GitHub**: https://github.com/ahmet-alp-colak/YM3XBA

---

**Son Güncelleme**: 2026-06-29  
**Yazar**: YM3XBA Development Team
