# GGG (General Gadget Generator)

A modular, event-driven, zero-malloc firmware framework and Hardware Abstraction Layer (HAL) engineered for deeply constrained microcontroller platforms.

## Overview

The GGG (General Gadget Generator) framework provides a lightweight, deterministic runtime architecture designed to decouple low-level embedded hardware details from application logic. Engineered primarily for real-time, low-power embedded systems (such as ARM Cortex-M and ESP32 architectures operating under tight RAM and Flash constraints), GGG enforces strict memory safety and deterministic execution patterns.

### Core Architectural Principles

- **Zero Dynamic Allocation (Zero-Malloc)**: All core subsystems, task stacks, queues, and storage blocks operate strictly on statically allocated memory pools. Dynamic heap allocation (`malloc`, `free`, `new`, `delete`) is prohibited in the core execution path to eliminate heap fragmentation and out-of-memory crashes.
- **Event-Driven Asynchronous Concurrency**: A high-performance System Event Bus built atop FreeRTOS queues enables loose coupling between hardware peripherals, protocols, and application tasks.
- **Stream-Centric Hardware Abstraction**: Standardized abstract interfaces for storage and stream I/O facilitate zero-copy serialization and transactional block storage.
- **Kconfig-Driven Modularity**: Linux-kernel-style Kconfig integration automates feature selection, memory dimensioning, and build-time code injection without runtime overhead.

---

## Architectural Overview of included Subsystems

### 1. Hardware Abstraction Layer (HAL)

The HAL exposes uniform, platform-agnostic C++ pure virtual interfaces:

- **`ggg::hal::IStorage`**: Defines block and stream storage abstractions supporting transactional write semantics:
  - `beginWrite(estimatedSize)`: Initiates a contiguous or segmented write transaction.
  - `writeData(handle, buffer, length)`: Progressively writes data chunks directly to storage media.
  - `commitWrite(handle)`: Finalizes the transaction and marks the data valid.
  - `abortWrite(handle)`: Atomically releases allocated storage blocks upon transmission or checksum failures, guaranteeing rollback without fragmentation.
  - `readData(handle, offset, buffer, length)`: Provides random-access reads across allocated blocks.
- **`ggg::hal::RamStorage`**: Reference static in-RAM block storage implementation with configurable block counts and block sizes, providing zero-malloc storage for rapid simulation and resource-constrained nodes.
- **`ggg::hal::IInputStream` & `ggg::hal::IOutputStream`**: Sequential streaming interfaces enabling progressive encoding/decoding (e.g., CBOR, Protocol Buffers) directly across storage boundaries.

### 2. Zero-Malloc System Event Bus (`SystemBus`)

The `ggg::system::SystemBus` coordinates asynchronous message passing between concurrent tasks and ISRs:

- **Fixed-Size Event Structure (`ggg::system::SystemEvent`)**:
  - 16-bit Event ID (`eventId`).
  - 16-bit Origin/Context metadata (`contextId`).
  - Strict 64-bit (8-byte) payload union (`SystemEventPayload`): Supports integer scalar values (`u32[2]`, `u64`), opaque handles (`StorageHandle_t`), and memory offsets.
- **Deterministic FreeRTOS Queue**: Fixed-depth static queue managed by an independent RTOS task (`SystemBusTask`), isolating publisher execution from subscriber latency.
- **Observer Interface (`ggg::system::IEventListener`)**: Subscription-based dispatch enabling decoupled modular plugins.

### 3. Plugin Architecture (`ggg::core::IPlugin`)

Modular extensions implement the `ggg::core::IPlugin` lifecycle contract:
- `init()`: Peripheral initialization, queue registration, and pre-scheduler configuration.
- `start()`: Task activation and event subscription upon RTOS scheduler start.
- `stop()`: Controlled teardown and power-down of peripheral resources.

### 4. Build Engine & Kconfig Integration

The GGG build engine combines Linux-standard Kconfig definitions with PlatformIO SCons build scripts:

- **`scripts/apply_kconfig.py`**: Recursively discovers module `Kconfig` files, parses configuration states (`.config`), generates C++ macro headers (`include/autoconf.h`), injects include paths (`CPPPATH`), registers active source filters (`SRC_FILTER`), and resolves external library dependencies (`LIB_DEPS`).
- **`scripts/custom_targets.py`**: Enables interactive terminal configuration via `pio run -t menuconfig`.

---

## Repository Structure

```
ggg/
├── include/
│   ├── ggg.h                     # Main framework umbrella header
│   └── ggg/
│       ├── core/
│       │   └── IPlugin.h         # Plugin lifecycle contract
│       ├── hal/
│       │   ├── IStorage.h        # Storage abstraction with transactional rollback
│       │   └── IStream.h         # Stream I/O interfaces
│       └── system/
│           ├── IEventListener.h  # Observer subscription interface
│           ├── SystemBus.h       # Event bus umbrella header
│           └── SystemEvent.h     # Fixed 8-byte event structure
├── lib/
│   ├── hal_ram_storage/          # Static in-RAM block storage driver
│   └── system_bus/               # FreeRTOS SystemBus implementation
├── scripts/
│   ├── apply_kconfig.py          # Kconfig parser and PlatformIO build integrator
│   └── custom_targets.py         # SCons menuconfig execution target
├── src/
│   └── main.cpp                  # Reference initialization firmware
├── test/
│   └── test_core/                # Unit tests for HAL, EventBus, and Kconfig
├── Kconfig                       # Root configuration menu
├── library.json                  # PlatformIO library specification
└── platformio.ini                # Build environments (native, embedded)
```

---

## Build and Execution Guide

### Prerequisites

1. **Python 3.8+**
2. **PlatformIO Core (CLI)**:
   ```bash
   pip install platformio
   ```
3. **Kconfiglib & Curses (for interactive configuration)**:
   ```bash
   pip install kconfiglib windows-curses   # Windows
   pip install kconfiglib curses           # Linux / macOS
   ```

### 1. Interactive Configuration (menuconfig)

To inspect or modify framework parameters (queue sizes, storage dimensions, task priorities):

```bash
pio run -t menuconfig
```

Alternatively, default values defined in `Kconfig` files are applied automatically if `.config` is absent.

### 2. Running Native Unit Tests

The core framework includes native host unit tests simulating FreeRTOS primitives and validating HAL storage lifecycle:

```bash
pio test -e native
```

### 3. Compiling for Embedded Targets

To compile the baseline framework for microcontrollers (e.g. Atmel SAMD21 Cortex-M0+):

```bash
pio run -e adafruit_feather_m0
```

---

## Integration in Downstream Projects

Downstream firmware applications can incorporate GGG as a modular library in `platformio.ini`:

```ini
[env:my_target]
platform = atmelsam
board = adafruit_feather_m0
framework = arduino
lib_deps =
    symlink://../ggg
extra_scripts =
    pre:../ggg/scripts/apply_kconfig.py
    ../ggg/scripts/custom_targets.py
```

---

## Status and License

> **Notice**: This software is an active Work in Progress (WIP). Interfaces, data contracts, and architectural implementations are subject to evolution and refinement without prior notice.

**Author & Copyright Owner**: Alberto Soncini (`alberto@synergon-lab.xyz`)  
**Copyright**: Copyright (c) 2012-2026 Alberto Soncini.  
**License**: GNU General Public License v3.0 or later (GPL-3.0-or-later). See `LICENSE` for details.
