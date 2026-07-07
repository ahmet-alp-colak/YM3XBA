# Module 5.1: Sürekli Karıştırma (Spot Jamming) - Technical Summary

## Module Information
- **Module ID**: 5.1
- **Name**: Sürekli Karıştırma (Spot Jamming)
- **Category**: Electronic Attack (ET)
- **Status**: ✅ PRODUCTION READY
- **Version**: 1.0.0
- **Date**: 2026-06-29

---

## Overview

The Spot Jamming module provides **continuous RF jamming capabilities** for disrupting enemy communication systems. It focuses all available TX power on a single target frequency for maximum effectiveness.

### Key Capabilities
- **Spot Jamming**: Single-target, high-power density jamming
- **Signal Types**: CW, Gaussian Noise, Modulated Noise
- **Frequency Range**: 1 MHz - 3 GHz (HackRF capable)
- **Real-time Control**: Asynchronous command/status management
- **Thread-Safe**: Multi-threaded with atomic state machine

---

## Architecture

### Component Structure
```
SpotJammingPipeline (Thread Orchestrator)
    ├── SpotJammingEngine (Signal Generator)
    ├── ThreadSafeQueue<Command> (Input)
    ├── ThreadSafeQueue<Report> (Output)
    └── Hardware State Machine (Atomic TX/RX control)
```

### Data Flow
```
ED Pipeline → TargetIntelligence → JammingCommand → Engine → HackRF TX
                                         ↓
                                   JammingReport → User
```

---

## Technical Specifications

| Parameter | Value | Unit |
|-----------|-------|------|
| Frequency Range | 1 - 3000 | MHz |
| Sample Rate | 20 | MSPS |
| Bandwidth | Configurable | Hz |
| Signal Types | 3 (CW, Noise, Modulated) | - |
| TX Buffer | 4096 - 16384 | samples |
| Processing Latency | ~50 | ms |
| Memory Usage | ~50 | MB |

---

## Signal Generation Algorithms

### 1. Continuous Wave (CW)
```
φ(n) = 2π * f_offset / f_s * n + φ_acc
I(n) = A * cos(φ(n))
Q(n) = A * sin(φ(n))
```

### 2. Gaussian Noise (Recommended)
```
I(n) = N(0, σ²) * A
Q(n) = N(0, σ²) * A
```

### 3. Modulated Noise
```
A_env(t) = 1 + 0.5 * sin(2π * f_mod * t)
I(n) = N(0, σ²) * A * A_env(t)
```

---

## Hardware State Machine

### States
- **IDLE**: No TX/RX activity
- **SENSING**: RX mode (for look-through jamming integration)
- **JAMMING**: TX mode active

### Transitions
```
IDLE ──► JAMMING  (Start)
JAMMING ──► IDLE  (Stop)
SENSING ──► JAMMING  (Look-through)
```

---

## Performance Characteristics

### Advantages
✅ Maximum Power Density  
✅ Extended Range (2-3x barrage)  
✅ Low Energy Consumption  
✅ Simple Hardware Requirements  

### Limitations
⚠️ Single Target Only  
⚠️ FHSS Ineffective  
⚠️ Manual Control Required  

---

## File Structure

```
5.1_Surekli_Karistirma/
├── include/           (3 files, ~740 lines)
├── src/              (2 files, ~440 lines)
├── examples/         (1 file, ~170 lines)
├── CMakeLists.txt    (104 lines)
├── README.md
├── NASIL_CALISTIRILIR.md
└── MODULE_SUMMARY.md
```

**Total**: ~1,600 lines of code

---

## Compliance

- ✅ C++17 standard
- ✅ Thread-safe patterns
- ✅ Atomic state machine
- ✅ Naming conventions
- ✅ CMake build system
- ✅ Documentation complete

---

## Authors

**YM3XBA Development Team**  
Module: 5.1 Sürekli Karıştırma  
Date: 2026-06-29

**Status**: ✅ Production Ready
