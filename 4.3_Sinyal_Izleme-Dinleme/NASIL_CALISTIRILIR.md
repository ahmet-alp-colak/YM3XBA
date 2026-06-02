# Sinyal Izleme/Dinleme Modülü - Calistirma Kilavuzu

## Genel Bakis

Bu kilavuz, **4.3 Sinyal Izleme/Dinleme** modülünün nasil derlenecegini ve
calistirilacagini adim adim aciklar.

### Modül Hakkinda

- **Amac**: RF sinyallerinin zamansal takibi, demodülasyonu ve payload cikarimi
- **Özellikler**:
  - Analog (AM/FM) ve dijital (PSK/FSK/OFDM) demodülasyon
  - FHSS (Frequency Hopping) tespiti
  - DMR, TETRA, AX.25, APRS protokol cözümlemesi
  - Thread-safe asenkron isleme pipeline'i

---

## Sistem Gereksinimleri

### Minimum Gereksinimler

| Bilesen         | Gereksinim                                     |
|-----------------|------------------------------------------------|
| Isletim Sistemi | Linux (Ubuntu 20.04+, Debian 11+, Fedora 35+)  |
| Derleyici       | GCC 7+ veya Clang 6+ (C++17 destegi)           |
| CMake           | 3.15 veya üzeri                                |
| RAM             | 2 GB (önerilen: 4 GB)                          |
| CPU             | 4+ cekirdek (önerilen: Intel i5/i7 veya Ryzen) |

### Gerekli Kütüphaneler

```bash
# Ubuntu/Debian icin
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    git \
    libpthread-stubs0-dev

# Fedora/RHEL icin
sudo dnf install -y \
    gcc-c++ \
    cmake \
    git \
    glibc-devel
```

### Opsiyonel Kütüphaneler

```bash
# liquid-dsp (üretim kalitesi demodülasyon)
sudo apt-get install -y libliquid-dev

# FFTW3 (OFDM demodülasyonu)
sudo apt-get install -y libfftw3-dev
```

---

## Kurulum ve Derleme

### Adim 1: Proje Dizinine Gidin

```bash
cd "/home/alp/Masaüstü/Sinyaller (copy)/4.3_Sinyal_Izleme-Dinleme"
```

### Adim 2: Build Dizini Olusturun

```bash
mkdir -p build
cd build
```

### Adim 3: CMake Yapilandirmasi

```bash
# Release modunda (optimize edilmis)
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_EXAMPLES=ON

# Debug modunda (hata ayiklama icin)
cmake .. \
    -DCMAKE_BUILD_TYPE=Debug \
    -DBUILD_EXAMPLES=ON
```

Basarili yapilandirma ciktisi:

```
========================================
  CognitiveRF_SignalMonitoring v1.0.0
========================================
Build type:        Release
C++ standard:      17
Install prefix:    /usr/local

Dependencies:
  liquid-dsp:      /usr/lib/x86_64-linux-gnu/libliquid.so
  FFTW3:           TRUE

Options:
  Build examples:  ON
========================================
```

### Adim 4: Derleme

```bash
cmake --build . -j$(nproc)
```

Basarili derleme ciktisi:

```
[ 16%] Building CXX object CMakeFiles/.../SignalTracker.cpp.o
[ 33%] Building CXX object CMakeFiles/.../AnalogDemodulator.cpp.o
[ 50%] Building CXX object CMakeFiles/.../DigitalDemodulator.cpp.o
[ 66%] Building CXX object CMakeFiles/.../PayloadExtractor.cpp.o
[ 83%] Building CXX object CMakeFiles/.../SignalMonitoringPipeline.cpp.o
[100%] Linking CXX static library libCognitiveRF_SignalMonitoring.a
[100%] Built target CognitiveRF_SignalMonitoring
[100%] Building CXX object CMakeFiles/example_monitoring.dir/...
[100%] Linking CXX executable example_monitoring
[100%] Built target example_monitoring
```

### Adim 5: Derleme Kontrolü

```bash
ls -lh
# Görmeniz gerekenler:
#   libCognitiveRF_SignalMonitoring.a  ->  statik kütüphane
#   example_monitoring                 ->  örnek program
```

---

## Programi Calistirma

```bash
./example_monitoring
```

Beklenen cikti:

```
===============================================================
  Signal Monitoring & Listening Pipeline - Examples
  Bilissel RF Spektrum Haritalama Sistemi
===============================================================

----------------------------------------
Example 1: Basic Signal Monitoring
----------------------------------------
Pipeline started successfully.
[OK] IQ data submitted

[SIGNAL] Signal Monitored:
  Frequency    : 435.0 MHz
  Modulation   : FM
  RSSI         : -50.0 dBm
  SNR          : 20.0 dB
  Tracking ID  : 1
  Audio samples: 1024
[OK] Audio saved to output_fm.wav

[STATS] Pipeline Statistics:
  Total processed        : 1
  Total demodulated      : 1
  Active tracked signals : 1

Pipeline stopped.

----------------------------------------
Example 2: FHSS Detection
----------------------------------------
Simulating frequency hopping signal...
  Hop 1: 435.0  MHz
  Hop 2: 435.5  MHz
  Hop 3: 436.0  MHz
  Hop 4: 435.25 MHz
  Hop 5: 435.75 MHz

[!!!] FHSS DETECTED!
  Tracking ID: 2
  Occupancy  : 85.5%

----------------------------------------
Example 3: Real-time Callback
----------------------------------------
[CB] Signal at 435.0 MHz, SNR: 20.0 dB
[CB] Signal at 436.0 MHz, SNR: 18.0 dB
[CB] Signal at 437.0 MHz, SNR: 16.0 dB
[CB] Signal at 438.0 MHz, SNR: 14.0 dB
[CB] Signal at 439.0 MHz, SNR: 12.0 dB

[OK] All examples completed successfully!
```

---

## Örnek Senaryolar

### Senaryo 1: FM Radyo Sinyali Izleme

Program FM sinyalini otomatik demodüle eder ve `output_fm.wav` dosyasina kaydeder.

```bash
./example_monitoring

#### Ses dosyasini dinlemek icin:
aplay output_fm.wav      # ALSA ile
ffplay output_fm.wav     # FFmpeg ile
```

### Senaryo 2: FHSS Tespiti

Program frekans atlamali sinyalleri otomatik tespit eder ve alarm üretir.

Tespit kriterleri:

- Minimum 5 frekans atlamasi
- Güc varyansi < 3 dB
- Atlama hizi: 1-1000 hop/saniye

### Senaryo 3: Gercek Zamanli Callback

Program her islenen sinyal icin callback fonksiyonunu cagirir.

---

## Sorun Giderme

### Derleme Hatalari

#### "CMake version too old"

```bash
sudo apt-get install -y cmake

# Manuel kurulum (eger paket eski ise):
wget https://github.com/Kitware/CMake/releases/download/v3.25.0/cmake-3.25.0-linux-x86_64.sh
sudo sh cmake-3.25.0-linux-x86_64.sh --prefix=/usr/local --skip-license
```

#### "C++17 not supported"

```bash
gcc --version   # GCC 7+ olmali

sudo apt-get install -y gcc-9 g++-9
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-9 90
sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-9 90
```

#### "Threads::Threads not found"

```bash
sudo apt-get install -y libpthread-stubs0-dev
```

### Calisma Zamani Hatalari

#### "Pipeline failed to start"

Neden: Thread olusturma hatasi veya bellek yetersizligi.

```bash
free -h
top
# Diger uygulamalari kapatip tekrar deneyin.
```

#### "No result received (timeout)"

Neden: Isleme süresi timeout degerini asiyor.

Cözüm: Örnek kodda `getResult(result, 5000)` seklinde timeout degerini artirin.

---

## Performans Ipuclari

### Optimizasyon Seçenekleri

#### 1. Release Modunda Derleyin

```bash
cmake .. -DCMAKE_BUILD_TYPE=Release
```

Fayda: %300-500 daha hizli isleme.

#### 2. Native CPU Optimizasyonu

```bash
cmake .. -DCMAKE_CXX_FLAGS="-O3 -march=native -mtune=native"
```

Fayda: CPU'ya özel SIMD optimizasyonlari.

#### 3. liquid-dsp Kullanin

```bash
sudo apt-get install -y libliquid-dev
cmake .. -DCMAKE_BUILD_TYPE=Release
```

Fayda: Üretim kalitesi demodülasyon algoritmalari.

### Performans Metrikleri

| Islem               | Süre          | Notlar                  |
|---------------------|---------------|-------------------------|
| Signal Tracking     | ~0.1 ms       | Frekans eslestirme      |
| AM Demodulation     | ~2 ms         | 1024 örnek              |
| FM Demodulation     | ~5 ms         | Filtreler dahil         |
| PSK Demodulation    | ~15 ms        | Carrier/timing recovery |
| Payload Extraction  | ~3 ms         | CRC + FEC               |
| **Toplam Pipeline** | **~10-25 ms** | Sinyal basina           |

---

## Entegrasyon

### Modül Mimarisi

```
+----------------------------------------------------------+
|  4.1 Sinyal Tespiti                                      |
|  - GMM anomaly detection                                 |
|  - Signal class: NOISE / BURST / CONTINUOUS              |
+-------------------------+--------------------------------+
                          |
                          v
+----------------------------------------------------------+
|  4.2 Parametre Cikarimi                                  |
|  - Modulation classification                             |
|  - CFO correction                                        |
|  - Protocol inference                                    |
+-------------------------+--------------------------------+
                          |
                          v
+----------------------------------------------------------+
|  4.3 Sinyal Izleme/Dinleme  <-- Bu Modül                 |
|  - Signal tracking                                       |
|  - Demodulation (AM / FM / PSK / FSK)                    |
|  - Payload extraction                                    |
+----------------------------------------------------------+
```

### Kendi Kodunuzda Kullanim

```cpp
#include "SignalMonitoringPipeline.hpp"

using namespace CognitiveRF::SignalMonitoring;

int main() {
    SignalMonitoringPipeline::PipelineConfig config;
    config.sample_rate_hz            = 20000000;
    config.enable_tracking           = true;
    config.enable_demodulation       = true;
    config.enable_payload_extraction = true;

    SignalMonitoringPipeline pipeline(config);
    pipeline.start();

    IQDataBuffer iq_buffer;
    iq_buffer.center_freq_hz      = 435000000;   // 435 MHz
    iq_buffer.sample_rate_hz      = 20000000;
    iq_buffer.detected_modulation = ModulationType::FM;
    iq_buffer.signal_class        = SignalClass::CONTINUOUS;
    iq_buffer.snr_db              = 15.0f;
    // ... IQ data ekleyin ...

    pipeline.submitIQData(iq_buffer);

    MonitoredSignal result;
    if (pipeline.getResult(result, 1000)) {
        std::cout << "Frekans: " << result.carrier_frequency_hz << " Hz\n";

        if (result.audio)          { /* Ses verisini isle    */ }
        if (result.payload)        { /* Payload verisini isle */ }
        if (result.fhss_suspected) { std::cout << "[!!!] FHSS ALARM!\n"; }
    }

    pipeline.stop();
    return 0;
}
```

### Derleme (Kendi Projenizde)

```bash
g++ -std=c++17 my_app.cpp \
    -I/path/to/4.3_Sinyal_Izleme-Dinleme/include \
    -L/path/to/4.3_Sinyal_Izleme-Dinleme/build \
    -lCognitiveRF_SignalMonitoring \
    -lpthread \
    -o my_app
```

---

## Ek Kaynaklar

### Dokümantasyon

| Dosya                        | Aciklama                     |
|------------------------------|------------------------------|
| `README.md`                  | Detayli modül dokümantasyonu |
| `MODULE_SUMMARY.md`          | Modül özeti ve mimari        |
| `examples/example_usage.cpp` | Örnek kod                    |

### Header Dosyalari

| Header                                 | Aciklama              |
|----------------------------------------|-----------------------|
| `include/DataStructures.hpp`           | Veri yapilari         |
| `include/SignalMonitoringPipeline.hpp` | Ana API               |
| `include/SignalTracker.hpp`            | Tracking API          |
| `include/AnalogDemodulator.hpp`        | Analog demodülasyon   |
| `include/DigitalDemodulator.hpp`       | Dijital demodülasyon  |
| `include/PayloadExtractor.hpp`         | Payload cikarimi      |

### Referanslar

1. **liquid-dsp**: https://liquidsdr.org/
2. **Gardner Timing Recovery** — "A BPSK/QPSK Timing-Error Detector for Sampled Receivers"
3. **Costas Loop** — "The Costas Loop: A Synchronization Technique for Suppressed Carrier Signals"

---

## Sik Sorulan Sorular

**S: Program calisiyor ama ses dosyasi olusmuyor?**
C: `build` dizininde `output_fm.wav` dosyasini kontrol edin. Yazma izinlerini dogrulayin.

**S: FHSS tespiti calısmiyor?**
C: Minimum 5 frekans atlamasi gereklidir; daha fazla test sinyali gönderin.

**S: Performans cok düsük?**
C: Release modunda derleyin ve liquid-dsp kütüphanesini yükleyin.

**S: Kendi IQ verilerimi nasil kullanirim?**
C: `IQDataBuffer` yapisini doldurun ve `submitIQData()` ile gönderin.

---

## Hizli Baslangic Özeti

```bash
# 1. Proje dizinine git
cd "/home/alp/Masaüstü/Sinyaller (copy)/4.3_Sinyal_Izleme-Dinleme"

# 2. Build dizini olustur
mkdir -p build && cd build

# 3. CMake yapilandir
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_EXAMPLES=ON

# 4. Derle
cmake --build . -j$(nproc)

# 5. Calistir
./example_monitoring

# 6. Sonuclari kontrol et
ls -lh output_fm.wav
```

---

Son Güncelleme : 2026-05-29
Versiyon       : 1.0.0
Durum          : Production Ready
