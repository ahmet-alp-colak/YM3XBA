# 4.1 Sinyal Tespiti - Nasıl Çalıştırılır

**Detaylı Kurulum ve Kullanım Kılavuzu**

---

## 📋 İçindekiler

1. [Sistem Gereksinimleri](#1-sistem-gereksinimleri)
2. [Bağımlılıkların Kurulumu](#2-bağımlılıkların-kurulumu)
3. [Modülün Derlenmesi](#3-modülün-derlenmesi)
4. [Örnek Programın Çalıştırılması](#4-örnek-programın-çalıştırılması)
5. [Kullanım Örnekleri](#5-kullanım-örnekleri)
6. [Konfigürasyon](#6-konfigürasyon)
7. [Sorun Giderme](#7-sorun-giderme)

---

## 1. Sistem Gereksinimleri

### Minimum Gereksinimler
- **İşletim Sistemi**: Linux (Ubuntu 20.04+, Debian 11+) veya macOS
- **İşlemci**: x86_64 (Intel/AMD) veya ARM64
- **RAM**: 2 GB (önerilen: 4 GB)
- **Disk**: 100 MB boş alan

### Yazılım Gereksinimleri
- **Derleyici**: GCC 7+ veya Clang 5+
- **CMake**: 3.15 veya üzeri
- **Git**: Kaynak kod indirmek için

---

## 2. Bağımlılıkların Kurulumu

### Ubuntu/Debian

```bash
# Temel araçları kur
sudo apt-get update
sudo apt-get install -y build-essential cmake git

# FFTW3 kütüphanesini kur (Welch PSD için gerekli)
sudo apt-get install -y libfftw3-dev

# Opsiyonel: pkg-config (bağımlılık tespiti için)
sudo apt-get install -y pkg-config
```

### Fedora/RHEL/CentOS

```bash
# Temel araçları kur
sudo dnf install -y gcc-c++ cmake git

# FFTW3 kütüphanesini kur
sudo dnf install -y fftw-devel

# Opsiyonel: pkg-config
sudo dnf install -y pkgconfig
```

### macOS (Homebrew)

```bash
# Homebrew kurulu değilse önce kurun:
# /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# CMake ve FFTW3 kur
brew install cmake fftw

# Xcode Command Line Tools (derleyici için)
xcode-select --install
```

### Windows (MSYS2/MinGW)

```bash
# MSYS2 içinde:
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-fftw
```

---

## 3. Modülün Derlenmesi

### Adım 1: Proje Dizinine Git

```bash
cd YM3XBA/4.1_SinyalTespiti
```

### Adım 2: Build Dizini Oluştur

```bash
mkdir build
cd build
```

### Adım 3: CMake ile Yapılandır

```bash
# Release modu (optimizasyon aktif)
cmake -DCMAKE_BUILD_TYPE=Release ..

# VEYA Debug modu (hata ayıklama için)
cmake -DCMAKE_BUILD_TYPE=Debug ..
```

**Opsiyonel CMake Parametreleri:**

```bash
# Örnek programları derleme (default: ON)
cmake -DBUILD_EXAMPLES=ON ..

# FFTW3 manuel yol belirtme (otomatik bulunamazsa)
cmake -DFFTW3_INCLUDE_DIR=/path/to/fftw3/include \
      -DFFTW3_LIBRARY=/path/to/fftw3/lib/libfftw3.so ..
```

### Adım 4: Derle

```bash
# Tüm CPU çekirdeklerini kullanarak derle
make -j$(nproc)

# VEYA tek çekirdekle derle
make
```

### Adım 5: Başarılı Derleme Kontrolü

```bash
# Kütüphane oluşturuldu mu?
ls libCognitiveRF_SignalDetection.a

# Örnek program oluşturuldu mu?
ls bin/example_signal_detection
```

**Beklenen Çıktı:**
```
libCognitiveRF_SignalDetection.a
bin/example_signal_detection
```

---

## 4. Örnek Programın Çalıştırılması

### Temel Çalıştırma

```bash
# Build dizinindeyken:
./bin/example_signal_detection
```

### Beklenen Çıktı

Program 3 örnek senaryoyu çalıştırır:

```
========================================
Signal Detection Module - Example Usage
Module 4.1: Sinyal Tespiti
========================================

================================================
EXAMPLE 1: Synchronous Processing
================================================
[1] Generating synthetic SIGNAL (1024 samples)...
[2] Processing signal...

========================================
Detection Result:
========================================
Frequency: 434 MHz
Timestamp: 1718970123456 ms
Signal Class: SIGNAL
Raw Likelihood: 0.8756
Filtered Likelihood: 0.8756
Adaptive Threshold: 0.5000
Classifier Trained: No

Features:
  Max Power: 1.2345
  Mean Power: 0.5678
  Variance: 0.1234
  ...

[3] Generating synthetic NOISE (1024 samples)...
[4] Processing noise...
...

================================================
EXAMPLE 2: GMM Classifier Training
================================================
[1] Collecting training samples...
    Collected 100 training samples
[2] Training GMM classifier...
    Training successful!
...

================================================
EXAMPLE 3: Asynchronous Pipeline
================================================
[1] Starting pipeline...
    Pipeline started successfully.
[2] Submitting 10 samples for processing...
    Submitted sample 1
    Submitted sample 2
    ...
[CALLBACK] Result received: SIGNAL at 434 MHz
[CALLBACK] Result received: NOISE at 435 MHz
...
[4] Pipeline Statistics:
    Total Processed: 10
    Total Signals: 5
    Total Noise: 5
    Avg Processing Time: 4.52 ms

========================================
All examples completed successfully!
========================================
```

---

## 5. Kullanım Örnekleri

### Örnek 1: Basit Sinyal Tespiti

```cpp
#include "SignalDetectionEngine.hpp"
#include <iostream>
#include <vector>
#include <complex>

int main() {
    // 1. Konfigürasyon oluştur
    CognitiveRF::SignalDetection::DetectionConfig config;
    config.fft_size = 1024;
    config.overlap_size = 512;
    config.default_threshold = 0.5;
    
    // 2. Engine oluştur
    CognitiveRF::SignalDetection::SignalDetectionEngine engine(config);
    
    // 3. IQ verisi hazırla (örnek: HackRF'ten gelen veri)
    std::vector<std::complex<float>> iq_samples(2048);
    // ... HackRF'ten veri oku ve iq_samples'a doldur
    
    // 4. Input oluştur
    CognitiveRF::SignalDetection::DetectionInput input;
    input.center_frequency_hz = 434000000;  // 434 MHz
    input.sample_rate_hz = 20000000;        // 20 MSPS
    input.iq_samples = iq_samples;
    
    // 5. İşle
    CognitiveRF::SignalDetection::DetectionOutput output;
    if (engine.processSignal(input, output)) {
        // 6. Sonucu kontrol et
        if (output.signal_class == CognitiveRF::SignalDetection::SignalClass::SIGNAL) {
            std::cout << "Signal detected at " 
                      << output.center_frequency_hz / 1e6 << " MHz\n";
            std::cout << "Likelihood: " << output.filtered_likelihood << "\n";
        } else {
            std::cout << "No signal (noise only)\n";
        }
    }
    
    return 0;
}
```

### Örnek 2: GMM Eğitimi ve Kullanımı

```cpp
#include "SignalDetectionEngine.hpp"
#include <vector>

int main() {
    CognitiveRF::SignalDetection::SignalDetectionEngine engine;
    
    // 1. Training verisi topla
    std::vector<CognitiveRF::SignalDetection::FeatureVector> training_data;
    
    // 50 sinyal + 50 gürültü örneği işle ve feature'ları topla
    for (int i = 0; i < 100; i++) {
        // IQ verisini al
        auto iq_data = getIQFromHackRF();  // Kendi fonksiyonunuz
        
        CognitiveRF::SignalDetection::DetectionInput input;
        input.iq_samples = iq_data;
        input.center_frequency_hz = 434000000;
        input.sample_rate_hz = 20000000;
        
        CognitiveRF::SignalDetection::DetectionOutput output;
        if (engine.processSignal(input, output)) {
            training_data.push_back(output.features);
        }
    }
    
    // 2. GMM'i eğit
    if (engine.trainClassifier(training_data)) {
        std::cout << "Training successful!\n";
    }
    
    // 3. Eğitilmiş classifier ile test et
    auto test_data = getIQFromHackRF();
    CognitiveRF::SignalDetection::DetectionInput input;
    input.iq_samples = test_data;
    input.center_frequency_hz = 434000000;
    input.sample_rate_hz = 20000000;
    
    CognitiveRF::SignalDetection::DetectionOutput output;
    engine.processSignal(input, output);
    
    std::cout << "Classification: " 
              << (output.signal_class == CognitiveRF::SignalDetection::SignalClass::SIGNAL 
                  ? "SIGNAL" : "NOISE") << "\n";
    std::cout << "Trained: " << output.is_classifier_trained << "\n";
    
    return 0;
}
```

### Örnek 3: Asenkron Pipeline ile Sürekli İzleme

```cpp
#include "SignalDetectionPipeline.hpp"
#include <iostream>
#include <atomic>

int main() {
    // 1. Pipeline oluştur
    CognitiveRF::SignalDetection::DetectionConfig config;
    CognitiveRF::SignalDetection::SignalDetectionPipeline pipeline(config, 100, 100);
    
    // 2. Callback tanımla
    std::atomic<int> signal_count{0};
    pipeline.setResultCallback([&signal_count](const auto& output) {
        if (output.signal_class == CognitiveRF::SignalDetection::SignalClass::SIGNAL) {
            signal_count++;
            std::cout << "Signal detected at " 
                      << output.center_frequency_hz / 1e6 << " MHz\n";
        }
    });
    
    // 3. Pipeline başlat
    pipeline.start();
    
    // 4. Sürekli veri gönder (örnek: HackRF sweep)
    for (uint64_t freq = 434e6; freq <= 440e6; freq += 1e6) {
        auto iq_data = getIQFromHackRF(freq);  // Kendi fonksiyonunuz
        
        CognitiveRF::SignalDetection::DetectionInput input;
        input.center_frequency_hz = freq;
        input.sample_rate_hz = 20e6;
        input.iq_samples = iq_data;
        
        pipeline.submitData(input);
    }
    
    // 5. İşlemin bitmesini bekle
    std::this_thread::sleep_for(std::chrono::seconds(5));
    
    // 6. İstatistikleri al
    auto stats = pipeline.getStatistics();
    std::cout << "\nStatistics:\n";
    std::cout << "  Total Processed: " << stats.total_processed << "\n";
    std::cout << "  Signals Found: " << signal_count << "\n";
    std::cout << "  Avg Processing: " << stats.avg_processing_time_ms << " ms\n";
    
    // 7. Pipeline durdur
    pipeline.stop();
    
    return 0;
}
```

---

## 6. Konfigürasyon

### DetectionConfig Parametreleri

```cpp
struct DetectionConfig {
    // Welch PSD parametreleri
    int fft_size = 1024;                    // FFT boyutu (512, 1024, 2048, 4096)
    int overlap_size = 512;                 // Overlap (tipik: fft_size/2)
    
    // GMM parametreleri
    int gmm_max_iterations = 100;           // Maksimum EM iterasyonu
    double gmm_convergence_threshold = 1e-6; // Yakınsama eşiği
    
    // Temporal filtreleme
    size_t temporal_history_size = 100;     // Frekans başına history boyutu
    bool enable_temporal_filtering = true;   // Temporal filtrelemeyi aktif et
    
    // Tespit parametreleri
    double default_threshold = 0.5;         // Varsayılan likelihood eşiği
    double detection_n_sigma = 3.0;         // Adaptive threshold için sigma sayısı
    
    // State management
    uint64_t state_cleanup_threshold_sec = 300; // 5 dakika sonra temizle
};
```

### Optimal Ayarlar

#### Yüksek Hassasiyet (Low False Alarm)
```cpp
config.default_threshold = 0.7;          // Yüksek eşik
config.detection_n_sigma = 4.0;          // 4-sigma
config.enable_temporal_filtering = true;
config.temporal_history_size = 50;
```

#### Düşük Latency (Hızlı Tepki)
```cpp
config.fft_size = 512;                   // Küçük FFT
config.overlap_size = 256;
config.temporal_history_size = 20;       // Kısa history
config.gmm_max_iterations = 50;          // Hızlı eğitim
```

#### Yüksek Accuracy (En İyi Tespit)
```cpp
config.fft_size = 2048;                  // Büyük FFT
config.overlap_size = 1024;
config.temporal_history_size = 100;
config.enable_temporal_filtering = true;
```

---

## 7. Sorun Giderme

### Problem: "FFTW3 not found"

**Çözüm:**
```bash
# Ubuntu/Debian
sudo apt-get install libfftw3-dev

# Fedora/RHEL
sudo dnf install fftw-devel

# macOS
brew install fftw

# Manuel yol belirtme
cmake -DFFTW3_INCLUDE_DIR=/path/to/include \
      -DFFTW3_LIBRARY=/path/to/lib/libfftw3.so ..
```

### Problem: "undefined reference to pthread"

**Çözüm:** CMakeLists.txt'de threads linklenmiş olmalı. Değilse:
```bash
# Temiz build
rm -rf build
mkdir build && cd build
cmake ..
make
```

### Problem: Segmentation Fault

**Olası Nedenler:**
1. IQ verisi boş (`iq_samples.empty()`)
2. FFT boyutu veri boyutundan büyük
3. Thread-safe olmayan kullanım

**Çözüm:**
```cpp
// IQ verisini kontrol et
if (input.iq_samples.size() < config.fft_size) {
    std::cerr << "Insufficient IQ samples\n";
    return false;
}

// Pipeline'ı thread-safe kullan
// YANLIŞ: Birden fazla thread'den aynı engine'e erişim
// DOĞRU: Pipeline kullan veya her thread için ayrı engine
```

### Problem: Düşük Performans

**Optimizasyon:**
```bash
# Release mode ile derle
cmake -DCMAKE_BUILD_TYPE=Release ..
make

# CPU optimizasyonları aktif (-march=native)
# Otomatik olarak Release modda aktif

# FFTW3 wisdom kullan (FFT optimizasyonu)
# İlk çalıştırmada wisdom dosyası oluşturulur
```

### Problem: Memory Leak

**Çözüm:**
```cpp
// Temporal state cleanup'ı düzenli çağır
auto now = std::chrono::system_clock::now();
auto timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
    now.time_since_epoch()
).count();

engine.cleanupOldStates(timestamp_ms);  // Her 1 dakikada bir çağır
```

### Problem: Queue Full (Pipeline)

**Çözüm:**
```cpp
// Queue boyutlarını artır
SignalDetectionPipeline pipeline(config, 500, 500);  // 500 yerine 100

// Veya veri gönderimini yavaşlat
if (!pipeline.submitData(input)) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    pipeline.submitData(input);  // Tekrar dene
}
```

---

## 📞 Destek

Sorun yaşarsanız:
1. [README.md](README.md) dosyasını kontrol edin
2. [MODULE_SUMMARY.md](MODULE_SUMMARY.md) dosyasındaki teknik detayları inceleyin
3. GitHub Issues bölümünde soru sorun

---

**İyi Çalışmalar! 🚀**
