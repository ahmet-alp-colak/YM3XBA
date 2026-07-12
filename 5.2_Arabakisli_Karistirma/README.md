# 5.2 Arabakışlı Karıştırma (Look-Through Jamming)

## Genel Bakış / Overview

**Türkçe:**  
Arabakışlı Karıştırma modülü, önceden belirlenmiş süre paylaşımı (TDD - Time Division Duplexing) dâhilinde yürütülen tespit ve karıştırma fazlarından oluşan bir elektronik taarruz tekniğidir. Sistem, hedef kanalın aktif olup olmadığını milisaniye seviyesinde kontrol edip karıştırarak kaynak verimliliği sağlar.

**English:**  
The Look-Through Jamming module implements an electronic attack technique consisting of detection and jamming phases executed within a predefined time-division duplexing (TDD) scheme. The system checks target channel activity at millisecond-level precision to ensure efficient resource utilization.

---

## Sistem Mimarisi / System Architecture

### TDD Döngüsü / TDD Cycle

```
┌─────────────────────────────────────────────────────────────────┐
│                    TDD CYCLE (90ms typical)                      │
├─────────────────────────────────────────────────────────────────┤
│                                                                   │
│  ┌──────────────┐  ┌────────┐  ┌─────────────────────────────┐ │
│  │   SENSING    │  │ GUARD  │  │        JAMMING               │ │
│  │   (RX Mode)  │  │        │  │       (TX Mode)              │ │
│  │              │  │        │  │                              │ │
│  │   10 ms      │  │  2 ms  │  │         80 ms                │ │
│  └──────────────┘  └────────┘  └─────────────────────────────┘ │
│                                                                   │
│  Energy Detection    Switch     Generate & Transmit Jamming     │
│  ↓                   Delay      ↓                                │
│  Target Active?  ───────────→   YES: Continue cycle             │
│                                  NO:  Return to IDLE             │
└─────────────────────────────────────────────────────────────────┘
```

### Donanım Durum Makinesi / Hardware State Machine

```
┌──────────┐
│   IDLE   │ ←──────────────────────┐
└────┬─────┘                         │
     │ START Command                 │ STOP / Target Inactive
     ↓                               │
┌──────────┐                         │
│ SENSING  │ ←──────────────────┐   │
│ (RX Mode)│                     │   │
└────┬─────┘                     │   │
     │ Target Detected?          │   │
     │ YES                       │   │
     ↓                           │   │
┌──────────┐                     │   │
│ JAMMING  │                     │   │
│ (TX Mode)│                     │   │
└────┬─────┘                     │   │
     │ Pulse Complete            │   │
     └───────────────────────────┘   │
     │ Check Target Still Active     │
     └───────────────────────────────┘
```

### Threading Mimarisi / Threading Architecture

- **ET_Thread_2 (Main Processing Loop)**: TDD durum makinesini yönetir
- **ET_Thread_4 (Hardware TX Control)**: Harici HackRF yöneticisi tarafından işlenir

---

## Özellikler / Features

### Temel Özellikler / Core Features

- ✅ **TDD Tabanlı Operasyon**: Zaman bölmeli yarı-çift yönlü (half-duplex) işlem
- ✅ **Enerji Tespiti**: Adaptif eşik değeri ile hedef aktivite tespiti
- ✅ **Atomik Durum Geçişleri**: `std::atomic::compare_exchange_strong` ile yarış koşulu koruması
- ✅ **Verimli Kaynak Kullanımı**: Sadece hedef aktifken karıştırma
- ✅ **Termal Koruma**: Duty-cycle limitleri ve ısı takibi
- ✅ **Thread-Safe Operasyon**: Tam thread-safe kuyruk ve durum yönetimi

### Teknik Parametreler / Technical Parameters

| Parametre | Değer | Açıklama |
|-----------|-------|----------|
| RX Fazı Süresi | 10 ms (varsayılan) | Enerji tespit penceresi |
| TX Fazı Süresi | 50-100 ms | Karıştırma darbe süresi |
| Geçiş Gecikmesi | 2 ms | RX/TX anahtarlama koruma süresi |
| Frekans Aralığı | 1 MHz - 3 GHz | ET (TX) modu limiti |
| Sample Rate | 20 MSPS | HackRF One standart |
| Tespit Eşiği | 10 dB (varsayılan) | Gürültü tabanı üzerinde SNR |

---

## Kurulum / Installation

### Gereksinimler / Requirements

- **C++17 uyumlu derleyici** (GCC 7+ veya Clang 5+)
- **CMake 3.15+**
- **Threads kütüphanesi** (pthread)
- **libhackrf** (opsiyonel, gerçek donanım için)

### Derleme / Build

```bash
cd 5.2_Arabakisli_Karistirma
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Test / Testing

```bash
# Örnek programı çalıştır
./bin/example_look_through
```

---

## Kullanım / Usage

### Temel Kullanım / Basic Usage

```cpp
#include "LookThroughPipeline.hpp"

using namespace CognitiveRF::LookThroughJamming;

// 1. Yapılandırma
PipelineConfig config;
config.hardware.center_freq_hz = 433.92e6;
config.tdd.sensing_window_ms = 10;
config.tdd.jamming_pulse_ms = 80;
config.tdd.detection_threshold_db = 10.0f;

// 2. Pipeline oluştur ve başlat
LookThroughPipeline pipeline(config);
pipeline.start();

// 3. Hedef istihbaratı (ED pipeline'dan)
TargetIntelligence target;
target.target_id = 1001;
target.center_freq_hz = 433.92e6;
target.bandwidth_hz = 25e3;

// 4. START komutu gönder
JammingCommand start_cmd;
start_cmd.action = JammingCommand::Action::START;
start_cmd.target = target;
pipeline.submitCommand(start_cmd);

// 5. TDD döngüsü otomatik olarak çalışır
// RX fazında: pipeline.submitRxData(iq_samples)
// TX fazında: pipeline.getTxBuffer(output)

// 6. Durdur
JammingCommand stop_cmd;
stop_cmd.action = JammingCommand::Action::STOP;
pipeline.submitCommand(stop_cmd);
```

### HackRF Entegrasyonu / HackRF Integration

```cpp
// RX Callback (SENSING phase)
int rx_callback(hackrf_transfer* transfer) {
    if (pipeline.getHardwareState() == HardwareState::SENSING) {
        IQBuffer rx_data(transfer->buffer, 
                         transfer->buffer + transfer->valid_length);
        pipeline.submitRxData(rx_data);
    }
    return 0;
}

// TX Callback (JAMMING phase)
int tx_callback(hackrf_transfer* transfer) {
    if (pipeline.getHardwareState() == HardwareState::JAMMING) {
        IQBuffer tx_data;
        if (pipeline.getTxBuffer(tx_data)) {
            memcpy(transfer->buffer, tx_data.data(), 
                   tx_data.size() * sizeof(IQSample));
        }
    }
    return 0;
}
```

---

## Algoritma Detayları / Algorithm Details

### Enerji Tespit Algoritması / Energy Detection Algorithm

```
1. Anlık Güç Hesaplama:
   P[n] = |I[n]|² + |Q[n]|²

2. Ortalama Güç:
   P_avg = (1/N) * Σ P[n]

3. dBm Dönüşümü:
   P_dBm = 10 * log₁₀(P_avg) + kalibrasyon_ofseti

4. Karar:
   hedef_aktif = (P_dBm > gürültü_tabanı + eşik_dB)
```

### Adaptif Gürültü Tabanı / Adaptive Noise Floor

```
Exponential Moving Average (EMA):

noise_floor_yeni = α * ölçülen_güç + (1-α) * noise_floor_eski

α = 0.1 (yavaş adaptasyon)
```

### Atomik Durum Geçişleri / Atomic State Transitions

```cpp
// Race condition koruması
HardwareState expected = HardwareState::IDLE;
if (hw_state_.compare_exchange_strong(expected, 
                                      HardwareState::SENSING,
                                      std::memory_order_acq_rel)) {
    // Başarılı geçiş
    // SENSING moduna geç
}
```

---

## Avantajlar ve Dezavantajlar / Advantages and Disadvantages

### Avantajlar / Advantages

✅ **Verimli Kaynak Kullanımı**: Sürekli karıştırmaya göre %80-90 daha az enerji  
✅ **Termal Yönetim**: HackRF Duty-Cycle limitleri aşılmaz  
✅ **Hafif Donanım**: Tek SDR ile çalışır, ek RX gerektirmez  
✅ **Adaptif Tespit**: Gürültü tabanı dinamik olarak güncellenir  
✅ **Thread-Safe**: Tam atomik durum yönetimi  

### Dezavantajlar / Disadvantages

⚠️ **Veri Sızıntısı**: 10ms RX penceresinde hedef küçük paketler gönderebilir  
⚠️ **Gecikme**: TDD döngüsü nedeniyle ~90ms yanıt süresi  
⚠️ **Half-Duplex**: Eş zamanlı RX/TX mümkün değil  

### Karşılaştırma / Comparison

| Özellik | Arabakışlı | Sürekli Karıştırma | Çift Donanım |
|---------|-----------|-------------------|--------------|
| Güç Tüketimi | Düşük | Yüksek | Orta |
| Termal Limit | Aşılmaz | Risk var | Aşılmaz |
| Veri Sızıntısı | Var (küçük) | Yok | Yok |
| Donanım Sayısı | 1 SDR | 1 SDR | 2 SDR |
| Maliyet | Düşük | Düşük | Yüksek |

---

## Performans / Performance

### Beklenen Performans / Expected Performance

- **Tespit Doğruluğu**: %95+ (SNR > 10 dB)
- **Tepki Süresi**: ~90 ms (TDD döngüsü)
- **Duty Cycle**: %80-90 (tipik)
- **İşlem Yükü**: ~15% CPU (tek çekirdek)

### Optimizasyon İpuçları / Optimization Tips

1. **RX Penceresi**: Daha kısa (5ms) = daha az sızıntı, daha sık geçiş
2. **TX Darbesi**: Daha uzun (100ms) = daha etkili karıştırma
3. **Eşik Değeri**: Daha düşük = daha hassas, daha çok yanlış alarm

---

## Bağımlılıklar / Dependencies

- **ED Pipeline** (Modul 4.X): Hedef istihbaratı sağlar
- **HackRF Hardware Manager**: RX/TX kontrol ve callback yönetimi
- **Atomic Operations**: C++17 `<atomic>` kütüphanesi
- **Threading**: C++17 `<thread>`, `<mutex>`, `<condition_variable>`

---

## Referanslar / References

- **5.2_Arabakisli_Karistirma.txt**: Tam teknik spesifikasyon
- **ET_pipeline.txt**: ET sistem mimarisi
- **ED_pipeline.txt**: ED-ET köprü arayüzü
- **.clinerules**: Proje kodlama standartları

---

## Geliştirici Notları / Developer Notes

### Thread Safety

Tüm public metodlar thread-safe'tir. Atomic operasyonlar ve mutex koruması kullanılır.

### Hardware State Management

`hw_state_` değişkeni atomik olarak yönetilir. Race condition koruması için `compare_exchange_strong` kullanılır.

### Memory Management

- Zero-copy işlemler mümkün olduğunda kullanılır
- IQ bufferları pre-allocated edilir
- Kuyruk boyutları yapılandırılabilir

---

## Lisans / License

Bu modül YM3XBA projesinin bir parçasıdır.  
Copyright © 2026 YM3XBA Development Team

---

## İletişim / Contact

**Geliştirici Ekibi**: YM3XBA Development Team  
**Repository**: https://github.com/ahmet-alp-colak/YM3XBA  
**Modül**: 5.2 Arabakışlı Karıştırma (Look-Through Jamming)