# 🚀 Pipeline 4.2 Parametre Çıkarımı - Çalıştırma Rehberi

## 📋 Hızlı Başlangıç

Bu pipeline aşamasını çalıştırmak için **[`examples/example_usage.cpp`](examples/example_usage.cpp)** dosyasını kullanmanız gerekiyor.

---

## ⚙️ Adım 1: Gerekli Bağımlılıkları Kurun

### Zorunlu Bağımlılıklar:
```bash
# Ubuntu/Debian için
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    libsqlite3-dev \
    libpthread-stubs0-dev

# Fedora/RHEL için
sudo dnf install -y \
    gcc-c++ \
    cmake \
    sqlite-devel
```

### ONNX Runtime Kurulumu (AI Modülasyon Sınıflandırma için):
```bash
# ONNX Runtime indirin
cd /tmp
wget https://github.com/microsoft/onnxruntime/releases/download/v1.16.0/onnxruntime-linux-x64-1.16.0.tgz

# Çıkartın ve kurun
tar -xzf onnxruntime-linux-x64-1.16.0.tgz
sudo cp -r onnxruntime-linux-x64-1.16.0/include/* /usr/local/include/
sudo cp -r onnxruntime-linux-x64-1.16.0/lib/* /usr/local/lib/
sudo ldconfig
```

---

## 🔨 Adım 2: Projeyi Derleyin

```bash
# Proje dizinine gidin
cd "/home/alp/Masaüstü/Sinyaller (copy)/4.2_ParametreCikarimi"

# Build dizini oluşturun
mkdir -p build
cd build

# CMake yapılandırması
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_EXAMPLES=ON \
    -DONNXRUNTIME_ROOT=/usr/local

# Derleme (tüm CPU çekirdeklerini kullanarak)
cmake --build . -j$(nproc)
```

### Derleme Çıktısı:
Başarılı derleme sonrası şu dosyalar oluşacak:
- `build/libCognitiveRF_ParameterExtraction.a` - Ana kütüphane
- `build/examples/example_usage` - Çalıştırılabilir örnek program

---

## ▶️ Adım 3: Programı Çalıştırın

### Seçenek 1: ONNX Model Olmadan (Test Modu)

ONNX model dosyanız yoksa, program yine de çalışacak ancak modülasyon sınıflandırma "UNKNOWN" olarak dönecektir:

```bash
# Build dizininden
cd build
./examples/example_usage

# VEYA proje kök dizininden
./build/examples/example_usage
```

### Seçenek 2: ONNX Model ile (Tam Özellikli)

Eğer eğitilmiş bir ONNX model dosyanız varsa:

```bash
# Model dizini oluşturun
mkdir -p models

# Model dosyanızı kopyalayın
cp /path/to/your/rfconformer.onnx models/

# Programı çalıştırın
./build/examples/example_usage
```

---

## 📊 Beklenen Çıktı

Program çalıştığında şu adımları göreceksiniz:

```
╔════════════════════════════════════════════════════════════╗
║   Cognitive RF Spectrum Mapping System                     ║
║   Hybrid Parameter Extraction Pipeline - Example           ║
╚════════════════════════════════════════════════════════════╝

[1/5] Configuring pipeline...
  ✓ Sample Rate: 20 MSPS
  ✓ Voting Window: 5 frames
  ✓ CFO Correction: Enabled

[2/5] Initializing pipeline...
  ✓ Pipeline started

[3/5] Initializing persistence logger...
  ✓ Logger started (database: signal_records.db)

[4/5] Processing signals...

Processing signal 1/5...
  ✓ IQ data submitted (offset: 1 MHz)

╔════════════════════════════════════════════════════════════╗
║           DECODED SIGNAL PARAMETERS                        ║
╠════════════════════════════════════════════════════════════╣
║ Timestamp:      1234567890123456 µs ║
║ Frequency:      435.00 MHz ║
║ Bandwidth:      125.50 kHz ║
╠════════════════════════════════════════════════════════════╣
║ RSSI:           -45.2 dBm ║
║ SNR:            12.5 dB  ║
║ Baud Rate:      9600 Hz  ║
╠════════════════════════════════════════════════════════════╣
║ Modulation:     QPSK ║
║ Protocol:       UNKNOWN ║
║ Multiplexing:   TDMA ║
║ Signal Class:   1 ║
╠════════════════════════════════════════════════════════════╣
║ Confidence:     85.3 %  ║
╚════════════════════════════════════════════════════════════╝

  ✓ Logged to database

------------------------------------------------------------
...
```

---

## 🗄️ Veritabanı Kayıtları

Program çalıştıktan sonra `signal_records.db` SQLite veritabanı dosyası oluşacaktır. İçeriğini görüntülemek için:

```bash
# SQLite ile veritabanını açın
sqlite3 signal_records.db

# Tabloları listeleyin
.tables

# Kayıtları görüntüleyin
SELECT * FROM signals;

# Çıkış
.quit
```

---

## 🔧 Sorun Giderme

### Problem: ONNX Runtime bulunamadı
```bash
# Hata: "ONNX Runtime not found"
# Çözüm: ONNX Runtime yolunu belirtin
cmake .. -DONNXRUNTIME_ROOT=/path/to/onnxruntime
```

### Problem: SQLite3 bulunamadı
```bash
# Hata: "Could NOT find SQLite3"
# Çözüm: SQLite3 geliştirme paketini kurun
sudo apt-get install libsqlite3-dev
```

### Problem: Derleme hatası - C++17 desteği yok
```bash
# Çözüm: Daha yeni bir GCC/Clang sürümü kullanın
# GCC 7+ veya Clang 5+ gereklidir
g++ --version  # Sürümü kontrol edin
```

### Problem: Çalışma zamanı hatası - shared library bulunamadı
```bash
# Hata: "error while loading shared libraries: libonnxruntime.so"
# Çözüm: Library yolunu ekleyin
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH

# Kalıcı yapmak için ~/.bashrc dosyasına ekleyin
echo 'export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH' >> ~/.bashrc
source ~/.bashrc
```

---

## 📝 Özelleştirme

### Model Yolunu Değiştirme

[`examples/example_usage.cpp`](examples/example_usage.cpp) dosyasında 113. satırı düzenleyin:

```cpp
config.onnx_model_path = "models/rfconformer.onnx";  // Kendi model yolunuz
```

### Örnekleme Hızını Değiştirme

```cpp
config.sample_rate_hz = 20e6;  // 20 MSPS (varsayılan)
// config.sample_rate_hz = 10e6;  // 10 MSPS için
```

### Veritabanı Yolunu Değiştirme

```cpp
logger_config.database_path = "signal_records.db";  // Varsayılan
// logger_config.database_path = "/tmp/my_signals.db";  // Özel yol
```

---

## 🎯 Pipeline'ın Yaptığı İşlemler

Bu program **Pipeline'ın 4.2 Parametre Çıkarımı** aşamasını simüle eder:

1. ✅ **Kalman Filtreli CFO Düzeltme** - Frekans kayması düzeltme
2. ✅ **Sinyal Düzeltme** - Karmaşık frekans kaydırma
3. ✅ **DSP Ölçümleri** - RSSI, SNR, Bandwidth, Baud Rate
4. ✅ **AI Modülasyon Sınıflandırma** - 14 farklı modülasyon türü
5. ✅ **Protokol Analizi** - DMR, TETRA, GSM, P25 vb. tanıma
6. ✅ **FHSS Tespiti** - Frekans atlamalı sinyal algılama
7. ✅ **Veritabanı Loglama** - SQLite ile kalıcı kayıt

---

## 📚 İlgili Dosyalar

- **Ana Program**: [`examples/example_usage.cpp`](examples/example_usage.cpp)
- **Pipeline Sınıfı**: [`include/ParameterExtractionPipeline.hpp`](include/ParameterExtractionPipeline.hpp)
- **Veri Yapıları**: [`include/DataStructures.hpp`](include/DataStructures.hpp)
- **Logger**: [`include/PersistenceLogger.hpp`](include/PersistenceLogger.hpp)
- **Build Sistemi**: [`CMakeLists.txt`](CMakeLists.txt)
- **Detaylı Dokümantasyon**: [`README.md`](README.md)
- **Pipeline Mimarisi**: [`pipeline.txt`](pipeline.txt)

---

## 💡 Sonraki Adımlar

1. **Gerçek SDR Entegrasyonu**: HackRF/RTL-SDR'den gerçek IQ verisi alın
2. **Model Eğitimi**: Kendi RF veri setinizle ONNX model eğitin
3. **UI Entegrasyonu**: Qt/ImGui ile görsel arayüz ekleyin
4. **Performans Optimizasyonu**: GPU acceleration, SIMD optimizasyonları

---

## 📧 Destek

Sorularınız için proje dokümantasyonuna bakın:
- [`README.md`](README.md) - Genel kullanım
- [`PROJECT_STRUCTURE.md`](PROJECT_STRUCTURE.md) - Mimari detayları
- [`pipeline.txt`](pipeline.txt) - Tam sistem mimarisi

---

**Son Güncelleme**: 2026-05-29  
**Versiyon**: 1.1.0  
**Durum**: Production Ready ✅
