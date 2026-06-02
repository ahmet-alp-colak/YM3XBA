# Yön Bulma (Direction Finding) Modülü - Calistirma Kilavuzu

## Genel Bakis

Bu kilavuz, **4.4 Yön Bulma (Direction Finding)** modülünün nasil derlenecegini ve
calistirilacagini adim adim aciklar.

### Modül Hakkinda

- **Amac**: Pseudo-Doppler yöntemiyle RF sinyallerinin geliş açısını (AoA) ölçme
- **Özellikler**:
  - Dairesel anten dizisi (4 eleman)
  - Yüksek hızlı RF anahtarlama (kHz mertebesi)
  - Pseudo-Doppler FM demodülasyonu
  - Faz karşılaştırması ve AoA hesaplama
  - ±10° RMS doğruluk
  - Thread-safe asenkron pipeline

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
# liquid-dsp (geliştirilmiş demodülasyon)
sudo apt-get install -y libliquid-dev

# FFTW3 (gelecek iyileştirmeler için)
sudo apt-get install -y libfftw3-dev
```

---

## Kurulum ve Derleme

### Adim 1: Proje Dizinine Gidin

```bash
cd "/home/alp/GitHub/Signals/Sinyaller-Linux/4.4_Yön_Bulma_(DF)"
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
  CognitiveRF_DirectionFinding v1.0.0
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
[ 16%] Building CXX object CMakeFiles/.../DirectionFindingEngine.cpp.o
[ 32%] Building CXX object CMakeFiles/.../AntennaSwitcher.cpp.o
[ 48%] Building CXX object CMakeFiles/.../PseudoDopplerDemodulator.cpp.o
[ 64%] Building CXX object CMakeFiles/.../PhaseComparator.cpp.o
[ 80%] Building CXX object CMakeFiles/.../DirectionFindingPipeline.cpp.o
[100%] Linking CXX static library libCognitiveRF_DirectionFinding.a
[100%] Built target CognitiveRF_DirectionFinding
[100%] Building CXX object CMakeFiles/example_direction_finding.dir/...
[100%] Linking CXX executable example_direction_finding
[100%] Built target example_direction_finding
```

### Adim 5: Derleme Kontrolü

```bash
ls -lh
# Görmeniz gerekenler:
#   libCognitiveRF_DirectionFinding.a  ->  statik kütüphane
#   example_direction_finding          ->  örnek program
```

---

## Programi Calistirma

```bash
./example_direction_finding
```

Beklenen cikti:

```
===============================================================
  Direction Finding (DF) Pipeline - Test Scenarios
  Bilişsel RF Spektrum Haritalama Sistemi
===============================================================

========================================
Scenario 1: Basic Direction Finding
========================================
Pipeline started.
Submitted signal: AoA_test=0°, SNR=20 dB
  → Estimated AoA: 0.5° (RMS error: 2.1°)
Submitted signal: AoA_test=45°, SNR=20 dB
  → Estimated AoA: 44.8° (RMS error: 1.9°)
[... more results ...]

[STATS] Processing Summary:
  Total processed:    15
  Valid estimates:    15
  Average confidence: 92.34%
  Average RMS error:  2.15°
Pipeline stopped.

========================================
Scenario 2: FHSS Signal Direction Finding
========================================
[...]

========================================
Scenario 3: Real-time Callback Processing
========================================
[...]

========================================
Scenario 4: Accuracy vs SNR Analysis
========================================
   SNR (dB) |  Est. AoA (°) | RMS Error (°) |   Confidence
--------------------------------------------------------
      -5.00 |         0.32 |         34.2 |        12.5%
       0.00 |        -1.15 |         24.8 |        28.3%
       5.00 |         0.98 |         15.6 |        58.9%
      10.00 |         0.12 |          8.9 |        82.4%
      15.00 |        -0.45 |          4.2 |        94.6%
      20.00 |         0.08 |          2.1 |        98.7%
      25.00 |         0.03 |          1.1 |        99.5%

===============================================================
✓ All scenarios completed successfully!
===============================================================
```

---

## Örnek Senaryolar

### Senaryo 1: Temel Yön Bulma

Program birden fazla test sinyalini yön bulma işleminden geçirir ve sonuçları gösterir.

```bash
./example_direction_finding
# Çıktı: Scenario 1 bölümü
```

Sonuçlar:
- Gelen açı (AoA) tahmini
- RMS hata (derece cinsinden)
- Güven skoru (%)

### Senaryo 2: FHSS Sinyallerinin Yön Bulması

Program frekans atlayan (FHSS) sinyalleri tespit ederek her atlamadaki yönü ölçer.

Tespit kriterleri:
- Minimum 5 frekans atlamasi
- Güc varyansi < 3 dB
- Atlama hizi: 1-1000 hop/saniye

### Senaryo 3: Gerçek Zamanli Callback

Program her işlenen sinyal için callback fonksiyonunu çağırır ve sonuçları yazdırır.

### Senaryo 4: AoA Doğruluğu vs SNR

Program farklı SNR seviyelerinde (−5 dB ila +25 dB) yön bulma performansını analiz eder.

Beklenen Davranış:
- SNR arttıkça RMS hata azalır
- SNR > 10 dB'de ±10° toleransı sağlanır

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

Neden: Thread oluşturma hatasi veya bellek yetersizligi.

```bash
free -h
top
# Diger uygulamalari kapatip tekrar deneyin.
```

#### "No result received (timeout)"

Neden: İşleme süresi timeout değerini aşıyor.

Çözüm: Örnek kodda `getResult(result, 5000)` şeklinde timeout değerini artırın.

#### "Invalid AoA estimate"

Neden: RMS hatası ±10° toleransını aşıyor (düşük SNR)

Çözüm: 
- Sinyal gücü veya SNR'ı artırın
- Test sinyalinin kalitesini iyileştirin
- Anten kalibrasyonunu kontrol edin

---

## Performans Ipuclari

### Optimizasyon Seçenekleri

#### 1. Release Modunda Derleyin

```bash
cmake .. -DCMAKE_BUILD_TYPE=Release
```

Fayda: %300-500 daha hizli işleme.

#### 2. Native CPU Optimizasyonu

```bash
cmake .. -DCMAKE_CXX_FLAGS="-O3 -march=native -mtune=native"
```

Fayda: CPU'ya özel SIMD optimizasyonları.

#### 3. liquid-dsp Kullanin

```bash
sudo apt-get install -y libliquid-dev
cmake .. -DCMAKE_BUILD_TYPE=Release
```

Fayda: Üretim kalitesi demodülasyon algoritmaları.

### Performans Metrikleri

| İşlem                       | Süre          | Notlar                     |
|----------------------------|---------------|---------------------------|
| Anten Anahtarlaması         | ~0.05 ms      | Simülasyon                 |
| FM Demodülasyonu           | ~3.0 ms       | Frequency discriminator    |
| Faz Karşılaştırması        | ~0.2 ms       | Phase comparison           |
| Sonuç Doğrulaması          | ~0.1 ms       | Tolerance check            |
| **Toplam Pipeline**        | **~3.5 ms**   | Sinyal başına              |

### Bellek Optimizasyonu

- Queue boyutunu azaltın: `config.max_queue_size = 50`
- Phase history sınırını düşürün: `MAX_HISTORY_SIZE = 512`
- Thread sayısını uyarla yin (1 thread'e iş düşer)

---

## Entegrasyon

### Modül Mimarisi

```
+----------------------------------------------------------+
|  4.1 Sinyal Tespiti                                      |
|  - Sinyal tespit ve sınıflandırması                      |
+-------------------------+--------------------------------+
                          |
                          v
+----------------------------------------------------------+
|  4.2 Parametre Cikarimi                                  |
|  - Modulation classification                             |
|  - CFO correction                                        |
+-------------------------+--------------------------------+
                          |
                          v
+----------------------------------------------------------+
|  4.3 Sinyal Izleme/Dinleme                               |
|  - Signal tracking                                       |
|  - Demodulation (AM/FM/PSK/FSK)                          |
|  - Payload extraction                                    |
+-------------------------+--------------------------------+
                          |
                          v
+----------------------------------------------------------+
|  4.4 Yön Bulma (DF) <-- Bu Modül                         |
|  - Pseudo-Doppler DF                                     |
|  - AoA estimation                                        |
|  - Angle of Arrival output                               |
+----------------------------------------------------------+
```

### Kendi Kodunuzda Kullanim

```cpp
#include "DirectionFindingPipeline.hpp"

using namespace CognitiveRF::DirectionFinding;

int main() {
    DirectionFindingPipeline::PipelineConfig config;
    config.df_config.center_frequency_hz = 435000000;   // 435 MHz
    config.df_config.aoa_rms_tolerance_degrees = 10.0f;
    config.antenna_config.switching_frequency_hz = 10000;  // 10 kHz

    DirectionFindingPipeline pipeline(config);
    pipeline.start();

    // IQ veri hazırlıkları
    std::vector<std::complex<float>> iq_data;
    // ... iq_data'yı doldur (SDR veya test verisi) ...

    // DF işlemesini yap
    pipeline.submitIQData(
        iq_data,
        435000000,      // center_freq_hz
        15.0f,          // snr_db
        -50.0f,         // rssi_dbm
        12345           // tracking_id (4.3'ten)
    );

    // Sonuç al
    DirectionFindingResult result;
    if (pipeline.getResult(result, 1000)) {
        std::cout << "AoA: " << result.angle_of_arrival_degrees << "°\n";
        std::cout << "RMS Error: " << result.aoa_rms_error_degrees << "°\n";
        std::cout << "Confidence: " << result.aoa_confidence * 100 << "%\n";
    }

    pipeline.stop();
    return 0;
}
```

### Derleme (Kendi Projenizde)

```bash
g++ -std=c++17 my_app.cpp \
    -I/path/to/4.4_Yön_Bulma/include \
    -L/path/to/4.4_Yön_Bulma/build \
    -lCognitiveRF_DirectionFinding \
    -lpthread \
    -o my_app
```

---

## Ek Kaynaklar

### Dokümantasyon

| Dosya                        | Aciklama                         |
|------------------------------|----------------------------------|
| `Module_summary.md`          | Modül özeti ve mimari             |
| `examples/example_usage.cpp` | Örnek kod (4 senaryo)             |
| `4.4_Yon_Bulma.txt`          | Teknik spesifikasyonlar           |

### Header Dosyalari

| Header                                 | Aciklama                   |
|----------------------------------------|----------------------------|
| `include/DataStructures.hpp`           | Veri yapilari              |
| `include/DirectionFindingEngine.hpp`   | Ana DF motoru              |
| `include/AntennaSwitcher.hpp`          | Anten anahtarlama          |
| `include/PseudoDopplerDemodulator.hpp` | FM demodülasyon            |
| `include/PhaseComparator.hpp`          | Faz karşılaştırması        |
| `include/DirectionFindingPipeline.hpp` | Thread-safe pipeline       |

### Referanslar

1. **4.4_Yon_Bulma.txt**: Pseudo-Doppler DF teknik detayları
2. **ED_pipeline.txt**: Section [8] (Yön Bulma)
3. **4.4_Yon_Bulma.drawio**: Akış şeması
4. **Phase-based Direction Finding**: İngilizce DF teorisi

---

## Sik Sorulan Sorular

**S: Program çalıştı ama sonuç alamıyorum?**
C: Giriş kuyruğunun dolu olup olmadığını kontrol edin. `getResult()` timeout değerini artırın.

**S: AoA tahminleri çok hatalı, neden?**
C: Düşük SNR veya yanlış anten kalibrasyonunun nedeni olabilir. SNR ≥ 10 dB hedefleyin.

**S: Compiler hatası: "complex not defined"?**
C: `#include <complex>` başlığını eklediğinizden emin olun.

**S: Pipeline thread'i çalışmıyor mu?**
C: `pipeline.start()` başarı döndürüyor mu kontrol edin. Thread başlatma başarısız olabilir.

**S: Kendi IQ verilerimi nasıl kullanum?**
C: `iq_data` vektörünü doldurup `submitIQData()` ile gönderin (std::complex<float> türünde).

**S: Dairesel antenna dizisi kullanmak zorundu mu?**
C: Evet, Pseudo-Doppler DF dairesel geometri gerektirir (4 eleman, 90° aralıklı).

---

## Hizli Baslangic Özeti

```bash
# 1. Proje dizinine git
cd "/home/alp/GitHub/Signals/Sinyaller-Linux/4.4_Yön_Bulma_(DF)"

# 2. Build dizini olustur
mkdir -p build && cd build

# 3. CMake yapilandir
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_EXAMPLES=ON

# 4. Derle
cmake --build . -j$(nproc)

# 5. Calistir
./example_direction_finding

# 6. Sonuclari kontrol et
# Tüm senaryolar başarıyla tamamlandı mı?
```

---

Son Güncelleme : 2026-06-02
Versiyon       : 1.0.0
Durum          : Production Ready
