# 4.1 Sinyal Tespiti (Signal Detection)

**Bilişsel RF Spektrum Haritalama Sistemi - Modül 4.1**

![Module Status](https://img.shields.io/badge/Status-Production%20Ready-brightgreen)
![C++ Standard](https://img.shields.io/badge/C++-17-blue)
![Threading](https://img.shields.io/badge/Threading-Multi--threaded-orange)

---

## 📋 İçindekiler / Table of Contents

- [Türkçe Açıklama](#türkçe-açıklama)
- [English Description](#english-description)

---

## Türkçe Açıklama

### 🎯 Genel Bakış

**4.1 Sinyal Tespiti Modülü**, RF spektrumda sinyal varlığını tespit eden ilk aşama modülüdür. Welch PSD analizi, istatistiksel özellik çıkarımı, GMM (Gaussian Mixture Model) sınıflandırması ve temporal filtreleme kullanarak gürültüden sinyali ayırt eder.

### ✨ Özellikler

#### 🔬 Spektral Analiz
- **Welch PSD**: Hamming window ile 50% overlap
- **FFT Optimizasyonu**: FFTW3 kütüphanesi ile yüksek performans
- **Adaptive Windowing**: Konfigüre edilebilir FFT boyutu (512-4096)

#### 📊 Özellik Çıkarımı (8 Boyutlu Vektör)
1. **Max Power**: Maksimum güç
2. **Mean Power**: Ortalama güç
3. **Variance**: Güç varyansı
4. **Skewness**: Dağılım asimetrisi
5. **Kurtosis**: Dağılım sivrililiği
6. **Energy Ratio**: Sinyal bandı enerji oranı
7. **Peak-to-Average**: Tepe/ortalama oranı
8. **Spectral Flatness**: Wiener entropi

#### 🤖 GMM Sınıflandırıcı
- **2-Component GMM**: Gürültü + Sinyal
- **EM Algoritması**: Expectation-Maximization
- **Diagonal Covariance**: Hesaplama optimizasyonu
- **Online Training**: Runtime'da eğitim

#### ⏱️ Temporal Filtreleme
- **Likelihood History**: Frekans bazlı geçmiş takibi
- **Adaptive Noise Floor**: Medyan + MAD tabanlı robust tahmin
- **Exponential Moving Average**: α=0.3 ile filtreleme
- **State Management**: Otomatik eski veri temizleme

#### 🔄 Pipeline Mimarisi
- **Thread-Safe Queues**: Girdi/çıktı kuyrukları
- **Asynchronous Processing**: Ayrı thread'de işleme
- **Callback Support**: Real-time bildirim
- **Statistics Tracking**: Performans metrikleri

### 📐 Mimari

```
┌─────────────────────────────────────────────────────────────┐
│                  Signal Detection Pipeline                  │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
                    ┌──────────────────┐
                    │   Input Queue    │ (Thread-safe)
                    └──────────────────┘
                              │
                              ▼
        ┌─────────────────────────────────────────────┐
        │        SignalDetectionEngine Thread         │
        │                                             │
        │  ┌─────────────────────────────────────┐   │
        │  │  1. WelchPSDProcessor               │   │
        │  │     └─> Hamming Window + FFT       │   │
        │  └─────────────────────────────────────┘   │
        │                  │                          │
        │                  ▼                          │
        │  ┌─────────────────────────────────────┐   │
        │  │  2. FeatureExtractor                │   │
        │  │     └─> 8D Feature Vector           │   │
        │  └─────────────────────────────────────┘   │
        │                  │                          │
        │                  ▼                          │
        │  ┌─────────────────────────────────────┐   │
        │  │  3. GMMClassifier                   │   │
        │  │     └─> Signal/Noise Classification │   │
        │  └─────────────────────────────────────┘   │
        │                  │                          │
        │                  ▼                          │
        │  ┌─────────────────────────────────────┐   │
        │  │  4. TemporalStateEngine             │   │
        │  │     └─> Temporal Filtering + State  │   │
        │  └─────────────────────────────────────┘   │
        │                  │                          │
        └──────────────────┼──────────────────────────┘
                           ▼
                 ┌──────────────────┐
                 │  Output Queue    │ (Thread-safe)
                 └──────────────────┘
                           │
                           ▼
                    ┌────────────┐
                    │  Callback  │ (Optional)
                    └────────────┘
```

### 🚀 Kullanım

#### Basit Kullanım (Synchronous)
```cpp
#include "SignalDetectionEngine.hpp"

// Konfigürasyon
DetectionConfig config;
config.fft_size = 1024;
config.overlap_size = 512;

// Engine oluştur
SignalDetectionEngine engine(config);

// IQ verisini işle
DetectionInput input;
input.center_frequency_hz = 434000000;  // 434 MHz
input.sample_rate_hz = 20000000;        // 20 MSPS
input.iq_samples = your_iq_data;

DetectionOutput output;
engine.processSignal(input, output);

// Sonuç
if (output.signal_class == SignalClass::SIGNAL) {
    std::cout << "Signal detected!\n";
}
```

#### Async Pipeline Kullanımı
```cpp
#include "SignalDetectionPipeline.hpp"

// Pipeline oluştur
SignalDetectionPipeline pipeline(config);

// Callback ayarla
pipeline.setResultCallback([](const DetectionOutput& output) {
    // Real-time işleme
});

// Pipeline başlat
pipeline.start();

// Veri gönder
pipeline.submitData(input);

// Sonuç al
DetectionOutput output;
pipeline.getResult(output, 100);  // 100ms timeout

// Pipeline durdur
pipeline.stop();
```

### 📦 Bağımlılıklar

- **C++17** (zorunlu)
- **Threads** (std::thread)
- **FFTW3** (Welch PSD için gerekli)

### 🛠️ Derleme

```bash
cd 4.1_SinyalTespiti
mkdir build && cd build
cmake ..
make -j$(nproc)

# Örnek programı çalıştır
./bin/example_signal_detection
```

### 📊 Performans

| İşlem | Süre | Notlar |
|-------|------|--------|
| Welch PSD (1024) | ~2 ms | FFTW3 optimized |
| Feature Extraction | <1 ms | 8D vector |
| GMM Classification | <1 ms | Diagonal covariance |
| Temporal Update | <0.5 ms | Hash map lookup |
| **Total Pipeline** | **~5 ms** | Per signal |

---

## English Description

### 🎯 Overview

**Module 4.1 Signal Detection** is the first-stage module for detecting signal presence in RF spectrum. It distinguishes signals from noise using Welch PSD analysis, statistical feature extraction, GMM (Gaussian Mixture Model) classification, and temporal filtering.

### ✨ Features

#### 🔬 Spectral Analysis
- **Welch PSD**: Hamming window with 50% overlap
- **FFT Optimization**: High performance with FFTW3 library
- **Adaptive Windowing**: Configurable FFT size (512-4096)

#### 📊 Feature Extraction (8-Dimensional Vector)
1. **Max Power**: Maximum power in PSD
2. **Mean Power**: Average power
3. **Variance**: Power variance
4. **Skewness**: Distribution asymmetry
5. **Kurtosis**: Distribution peakedness
6. **Energy Ratio**: Signal band energy ratio
7. **Peak-to-Average**: Peak power / average power
8. **Spectral Flatness**: Wiener entropy

#### 🤖 GMM Classifier
- **2-Component GMM**: Noise + Signal
- **EM Algorithm**: Expectation-Maximization
- **Diagonal Covariance**: Computation optimization
- **Online Training**: Runtime training capability

#### ⏱️ Temporal Filtering
- **Likelihood History**: Per-frequency history tracking
- **Adaptive Noise Floor**: Robust estimation with median + MAD
- **Exponential Moving Average**: Filtering with α=0.3
- **State Management**: Automatic old data cleanup

#### 🔄 Pipeline Architecture
- **Thread-Safe Queues**: Input/output queues
- **Asynchronous Processing**: Separate processing thread
- **Callback Support**: Real-time notifications
- **Statistics Tracking**: Performance metrics

### 🚀 Usage

See [NASIL_CALISTIRILIR.md](NASIL_CALISTIRILIR.md) for detailed instructions.

### 📦 Dependencies

- **C++17** (required)
- **Threads** (std::thread)
- **FFTW3** (required for Welch PSD)

### 🛠️ Build

```bash
cd 4.1_SinyalTespiti
mkdir build && cd build
cmake ..
make -j$(nproc)

# Run example
./bin/example_signal_detection
```

### 📊 Performance

| Operation | Time | Notes |
|-----------|------|-------|
| Welch PSD (1024) | ~2 ms | FFTW3 optimized |
| Feature Extraction | <1 ms | 8D vector |
| GMM Classification | <1 ms | Diagonal covariance |
| Temporal Update | <0.5 ms | Hash map lookup |
| **Total Pipeline** | **~5 ms** | Per signal |

---

## 📚 Dokümantasyon / Documentation

- **[NASIL_CALISTIRILIR.md](NASIL_CALISTIRILIR.md)**: Detaylı kurulum ve kullanım (Turkish)
- **[MODULE_SUMMARY.md](MODULE_SUMMARY.md)**: Teknik özet ve algoritmalar
- **[4.1_SinyalTespti.txt](4.1_SinyalTespti.txt)**: Orijinal tasarım spesifikasyonu

---

## 🔗 İlgili Modüller / Related Modules

- **4.2 Parametre Çıkarımı**: Tespit edilen sinyallerin detaylı analizi
- **4.3 Sinyal İzleme-Dinleme**: Sürekli sinyal takibi
- **4.4 Yön Bulma**: Sinyal yönü belirleme
- **4.5 Konum Belirleme**: Geo-location

---

## 📄 Lisans / License

Bu proje YM3XBA EW sistemi kapsamında geliştirilmektedir.

---

## 👥 Geliştirici / Developer

**CognitiveRF Development Team**  
Bilişsel RF Spektrum Haritalama Sistemi  
Module: 4.1 Sinyal Tespiti

---

## 📝 Notlar / Notes

- GMM classifier'ı kullanmadan önce eğitilmelidir (veya default threshold kullanılır)
- FFTW3 wisdom dosyası kullanılarak FFT optimizasyonu artırılabilir
- Temporal state cleanup düzenli olarak çağrılmalıdır (memory management)
- Pipeline queue boyutları sistem gereksinimlerine göre ayarlanmalıdır

**İyi kullanımlar! / Happy coding!** 🚀
