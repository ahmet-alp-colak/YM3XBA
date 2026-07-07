# 5.1 Sürekli Karıştırma (Spot Jamming) Module

## 🇹🇷 Türkçe

### Genel Bakış
**Sürekli Karıştırma (Spot Jamming)** modülü, belirlenen hedef frekanslar üzerinde yüksek yoğunluklu RF karıştırma sinyalleri üreterek düşman haberleşme sistemlerini kesintiye uğratmak için tasarlanmış bir Elektronik Taarruz (ET) sistemidir.

Bu modül, **Bilişsel RF Spektrum Haritalama Sistemi (YM3XBA)** projesinin ET (Electronic Attack) alt sisteminin parçasıdır ve Elektronik Destek (ED) pipeline'ından gelen istihbarat verileriyle çalışır.

### Temel Özellikler
- ✅ **Spot Jamming**: Tek hedef frekansına odaklanmış karıştırma
- ✅ **Çoklu Karıştırma Tipleri**: CW, Gaussian Noise, Modulated Noise
- ✅ **Thread-Safe Pipeline**: Asenkron komut ve durum yönetimi
- ✅ **Hardware State Machine**: HackRF half-duplex güvenli yönetimi
- ✅ **Real-time Status Reporting**: Anlık durum raporlama

### Sistem Mimarisi
```
ED Pipeline → Target Intelligence → Command Queue → Jamming Engine → TX Hardware
                                          ↓
                                    Status Queue → User Interface
```

### Teknik Özellikler
- **Frekans Aralığı**: 1 MHz - 3 GHz (ET mode)
- **Bandwidth**: Hedef sinyale göre ayarlanabilir (örn: 25 kHz)
- **Sample Rate**: 20 MSPS
- **Threading Model**: Multi-threaded with atomic state management

---

## 🇬🇧 English

### Overview
The **Spot Jamming** module is an Electronic Attack system designed to disrupt enemy communication systems by generating high-intensity RF jamming signals on specified target frequencies.

### Key Features
- ✅ **Spot Jamming**: Focused jamming on single target frequency
- ✅ **Multiple Jamming Types**: CW, Gaussian Noise, Modulated Noise
- ✅ **Thread-Safe Pipeline**: Asynchronous command and status management
- ✅ **Hardware State Machine**: Safe HackRF half-duplex management
- ✅ **Real-time Status Reporting**: Instant status updates

---

## 📁 Module Structure
```
5.1_Surekli_Karistirma/
├── include/
│   ├── DataStructures.hpp
│   ├── SpotJammingEngine.hpp
│   └── SpotJammingPipeline.hpp
├── src/
│   ├── SpotJammingEngine.cpp
│   └── SpotJammingPipeline.cpp
├── examples/
│   └── example_usage.cpp
├── CMakeLists.txt
├── README.md
└── NASIL_CALISTIRILIR.md
```

---

## 🚀 Quick Start

### Build Instructions
```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
./example_usage
```

---

## 📖 Usage Example

```cpp
#include "SpotJammingPipeline.hpp"
using namespace CognitiveRF::SpotJamming;

int main() {
    PipelineConfig config;
    config.hardware.center_freq_hz = 433.92e6;
    config.signal.type = JammingType::GAUSSIAN_NOISE;
    
    SpotJammingPipeline pipeline(config);
    pipeline.start();
    
    TargetIntelligence target;
    target.target_id = 12345;
    target.center_freq_hz = 433.92e6;
    
    JammingCommand cmd;
    cmd.action = JammingCommand::Action::START;
    cmd.target = target;
    pipeline.submitCommand(cmd);
    
    // Monitor and stop...
    pipeline.stop();
    return 0;
}
```

---

## 📊 Performance Metrics

| Parameter | Target | Achieved |
|-----------|--------|----------|
| Sample Rate | 20 MSPS | 20 MSPS |
| TX Latency | <100 ms | ~50 ms |
| Memory Usage | <100 MB | ~50 MB |

---

## 📚 References

1. [HackRF One Documentation](https://hackrf.readthedocs.io/)
2. ET Pipeline Specification: `ET_pipeline.txt`
3. System Design: `3.1_DetaylıTasarimOzeti.txt`

---

## 👥 Authors

**YM3XBA Development Team** - Module 5.1 - 2026-06-29

