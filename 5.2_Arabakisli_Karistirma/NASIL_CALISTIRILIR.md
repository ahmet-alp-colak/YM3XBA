# 5.2 Arabakışlı Karıştırma - Nasıl Çalıştırılır

## Hızlı Başlangıç

Bu dokuman, 5.2 Arabakışlı Karıştırma modülünün nasıl derleneceği ve çalıştırılacağını adım adım açıklar.

---

## Ön Gereksinimler

### Yazılım Gereksinimleri

1. **İşletim Sistemi**: Linux (Ubuntu 20.04+ önerilir) veya Windows (WSL2 ile)
2. **C++ Derleyici**: GCC 7+ veya Clang 5+
3. **CMake**: 3.15 veya üzeri
4. **Git**: Versiyon kontrolü için

### Kurulum Komutları (Ubuntu/Debian)

```bash
# Temel araçları kur
sudo apt update
sudo apt install -y build-essential cmake git

# C++17 desteği için GCC güncellemesi (gerekirse)
sudo apt install -y gcc-9 g++-9
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-9 90
sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-9 90
```

### HackRF (Opsiyonel - Gerçek Donanım İçin)

```bash
# libhackrf kütüphanesini kur
sudo apt install -y libhackrf-dev hackrf

# HackRF USB izinlerini ayarla
sudo usermod -a -G plugdev $USER
```

---

## Adım 1: Proje Dizinine Git

```bash
cd /path/to/YM3XBA/5.2_Arabakisli_Karistirma
```

---

## Adım 2: Build Dizini Oluştur

```bash
mkdir -p build
cd build
```

---

## Adım 3: CMake ile Yapılandır

### Temel Yapılandırma

```bash
cmake ..
```

### Debug Modu (Geliştirme İçin)

```bash
cmake -DCMAKE_BUILD_TYPE=Debug ..
```

### Release Modu (Üretim İçin)

```bash
cmake -DCMAKE_BUILD_TYPE=Release ..
```

### Örnek Programları Devre Dışı Bırak

```bash
cmake -DBUILD_EXAMPLES=OFF ..
```

---

## Adım 4: Derleme

### Tüm Çekirdekleri Kullanarak Hızlı Derleme

```bash
make -j$(nproc)
```

### Tek Çekirdekle Derleme (Hata Ayıklama İçin)

```bash
make
```

### Sadece Kütüphaneyi Derle

```bash
make CognitiveRF_LookThroughJamming
```

---

## Adım 5: Çalıştırma

### Örnek Programı Çalıştır

```bash
./bin/example_look_through
```

### Beklenen Çıktı

```
============================================================
Look-Through Jamming Module - Example Usage
Module 5.2: Arabakışlı Karıştırma
============================================================

[STEP 1] Configuring pipeline...
[OK] Configuration validated

[STEP 2] Creating pipeline...
[LookThrough] Pipeline constructed
[LookThrough] Pipeline started
[OK] Pipeline started

[STEP 3] Creating target intelligence...
[OK] Target created: ID=1001, Freq=433.92 MHz

[STEP 4] Starting jamming operation...
[LookThrough] Engine initialized for target ID=1001 freq=433.92 MHz bw=25 kHz adaptive_noise_floor=-100 dBm
[LookThrough] START command executed successfully
[OK] START command submitted

Look-Through jamming active. TDD cycle running:
  - RX Phase: 10 ms
  - TX Phase: 80 ms
  - Total cycle: 90 ms

[STEP 5] Simulating sensing phase...
Press Ctrl+C to stop...

[LookThrough] Sensing: Power=-20.0 dBm, Threshold=-90.0 dBm, Detected=YES
[LookThrough] Target detected, switching to JAMMING
[SIMULATED TX] Transmitted 1600000 jamming samples
...
```

---

## Yaygın Sorunlar ve Çözümler

### Sorun 1: CMake Threads Bulamıyor

**Hata:**
```
CMake Error: Could not find Threads
```

**Çözüm:**
```bash
sudo apt install -y libpthread-stubs0-dev
```

### Sorun 2: C++17 Desteği Yok

**Hata:**
```
error: #error This file requires compiler and library support for the ISO C++ 2017 standard.
```

**Çözüm:**
```bash
# GCC versiyonunu kontrol et
gcc --version

# GCC 7 veya üzeri gerekli. Güncelle:
sudo apt install -y gcc-9 g++-9
```

### Sorun 3: Header Dosyası Bulunamıyor

**Hata:**
```
fatal error: LookThroughPipeline.hpp: No such file or directory
```

**Çözüm:**
```bash
# include/ dizininin varlığını kontrol et
ls -la include/

# CMake cache'i temizle ve tekrar dene
rm -rf build/*
cd build
cmake ..
make
```

### Sorun 4: Linking Hatası

**Hata:**
```
undefined reference to `pthread_create'
```

**Çözüm:**
```bash
# CMakeLists.txt'de Threads kütüphanesinin eklendiğinden emin ol
# Eğer sorun devam ederse:
cmake .. -DCMAKE_CXX_FLAGS="-pthread"
make
```

---

## İleri Düzey Kullanım

### Özel Yapılandırma ile Çalıştırma

Kendi C++ programınızı oluşturun:

```cpp
// my_jamming_app.cpp
#include "LookThroughPipeline.hpp"

int main() {
    using namespace CognitiveRF::LookThroughJamming;
    
    // Yapılandırma
    PipelineConfig config;
    config.hardware.center_freq_hz = 433.92e6;
    config.tdd.sensing_window_ms = 5;   // Daha kısa RX
    config.tdd.jamming_pulse_ms = 100;  // Daha uzun TX
    
    // Pipeline oluştur
    LookThroughPipeline pipeline(config);
    pipeline.start();
    
    // ... programın geri kalanı
    
    return 0;
}
```

Derle:

```bash
g++ -std=c++17 -I../include -o my_app my_jamming_app.cpp \
    -L. -lCognitiveRF_LookThroughJamming -pthread
```

### HackRF ile Gerçek Donanım Kullanımı

```cpp
#include "LookThroughPipeline.hpp"
#include <libhackrf/hackrf.h>

// RX Callback
int rx_callback(hackrf_transfer* transfer) {
    // IQ verilerini dönüştür
    IQBuffer rx_data;
    for (int i = 0; i < transfer->valid_length; i += 2) {
        float I = (transfer->buffer[i] - 127.5f) / 128.0f;
        float Q = (transfer->buffer[i+1] - 127.5f) / 128.0f;
        rx_data.push_back(IQSample(I, Q));
    }
    
    // Pipeline'a gönder
    pipeline.submitRxData(rx_data);
    return 0;
}

// TX Callback
int tx_callback(hackrf_transfer* transfer) {
    IQBuffer tx_data;
    if (pipeline.getTxBuffer(tx_data)) {
        for (size_t i = 0; i < tx_data.size() && i < transfer->valid_length/2; ++i) {
            transfer->buffer[i*2] = (uint8_t)((tx_data[i].real() * 128.0f) + 127.5f);
            transfer->buffer[i*2+1] = (uint8_t)((tx_data[i].imag() * 128.0f) + 127.5f);
        }
    }
    return 0;
}
```

---

## Test ve Doğrulama

### Birim Testleri Çalıştır (Eğer Varsa)

```bash
cd build
ctest --verbose
```

### Bellek Sızıntısı Kontrolü (Valgrind)

```bash
sudo apt install -y valgrind

valgrind --leak-check=full --show-leak-kinds=all \
         ./bin/example_look_through
```

### CPU Profiling (gprof)

```bash
# Debug modda profiling flag'i ile derle
cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-pg" ..
make

# Çalıştır
./bin/example_look_through

# Profil analizi
gprof ./bin/example_look_through gmon.out > analysis.txt
less analysis.txt
```

---

## Performans Ayarları

### Düşük Latency İçin Optimizasyon

```cpp
config.tdd.sensing_window_ms = 5;    // Daha kısa RX
config.tdd.jamming_pulse_ms = 50;    // Daha kısa TX
config.tdd.rx_tx_switching_delay_ms = 1;  // Minimum geçiş
```

### Düşük Güç Tüketimi İçin

```cpp
config.hardware.max_duty_cycle = 0.5f;  // %50 duty cycle
config.tdd.jamming_pulse_ms = 50;       // Kısa TX darbeleri
```

### Maksimum Etkililik İçin

```cpp
config.signal.type = JammingType::GAUSSIAN_NOISE;
config.signal.noise_amplitude = 0.8f;   // Yüksek amplitüd
config.tdd.jamming_pulse_ms = 100;      // Uzun TX darbeleri
```

---

## Loglama ve Debug

### Log Seviyesini Artır

```cpp
config.enable_logging = true;
config.log_prefix = "[LookThrough-DEBUG]";
```

### Status Callback ile Gerçek Zamanlı Takip

```cpp
pipeline.setStatusCallback([](const JammingReport& report) {
    std::cout << "Status Update:" << std::endl;
    std::cout << "  Cycles: " << report.sensing_cycles_completed << std::endl;
    std::cout << "  Detections: " << report.target_detections << std::endl;
    std::cout << "  Duty Cycle: " << (report.duty_cycle_current * 100.0f) << "%" << std::endl;
});
```

---

## Üretim Ortamına Deployment

### 1. Release Modda Derle

```bash
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

### 2. Kütüphaneyi Kur

```bash
sudo make install
```

### 3. Sistem Servis Olarak Çalıştır (systemd)

`/etc/systemd/system/look-through-jamming.service` dosyası oluştur:

```ini
[Unit]
Description=Look-Through Jamming Service
After=network.target

[Service]
Type=simple
User=radio
ExecStart=/usr/local/bin/look_through_daemon
Restart=on-failure
RestartSec=5s

[Install]
WantedBy=multi-user.target
```

Servisi etkinleştir:

```bash
sudo systemctl daemon-reload
sudo systemctl enable look-through-jamming
sudo systemctl start look-through-jamming
sudo systemctl status look-through-jamming
```

---

## Destek ve Yardım

### Dokümantasyon

- **README.md**: Genel bakış ve kullanım örnekleri
- **MODULE_SUMMARY.md**: Teknik detaylar ve algoritma açıklamaları
- **5.2_Arabakisli_Karistirma.txt**: Tam spesifikasyon

### İletişim

- **GitHub Issues**: https://github.com/ahmet-alp-colak/YM3XBA/issues
- **Geliştirici Ekibi**: YM3XBA Development Team

---

## Sık Kullanılan Komutlar - Özet

```bash
# Hızlı derleme
cd 5.2_Arabakisli_Karistirma
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)

# Örnek çalıştır
./bin/example_look_through

# Temizlik
make clean

# Tam temizlik (CMake cache dahil)
cd .. && rm -rf build

# Yeniden derle
mkdir build && cd build && cmake .. && make -j$(nproc)
```

---

**Son Güncelleme**: 2026-07-12  
**Versiyon**: 1.0.0  
**Durum**: Üretim Hazır ✅