# 🌊 SeaDrop

**SeamlessDrop - An Open-Source AirDrop for Linux + Android**

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://isocpp.org/)
[![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20Android-green.svg)](#)

SeaDrop is a cross-platform peer-to-peer file sharing application that brings AirDrop-like simplicity to Linux and Android. It uses **WiFi Direct** for high-speed transfers and **BLE** for discovery, with a unique **distance-based trust system** for security.

## ✨ Key Features

### 🤝 Trust-Based Auto-Accept
Paired devices can share files without confirmation dialogs - the core "seamless" experience.

### 📏 Distance-Based Trust Zones
RSSI monitoring creates dynamic trust levels based on physical proximity:

| Zone | Distance | Trust Level |
|------|----------|-------------|
| **Intimate** | 0-3m | Full auto-accept, clipboard sharing |
| **Close** | 3-10m | Auto-accept with toast notification |
| **Nearby** | 10-30m | Large files need confirmation |
| **Far** | 30m+ | Always confirm + security alert |

### 📋 Clipboard Sharing
P2P clipboard push (not sync) - you decide when to share:
- **Explicit push**: Tap "Share Clipboard" button
- **Hotkey**: `Ctrl+Shift+V` on desktop
- **Auto-share**: Optional, only in Zone 1 (Intimate)

### 📁 Smart File Handling
- Auto-rename on conflicts: `photo.jpg` → `photo (1).jpg`
- Folder support with progress per file
- Chunked transfers (64KB), resumable on disconnect
- End-to-end encryption with libsodium

## 🏗️ Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                      SeaDrop Architecture                    │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│   ┌─────────────────┐           ┌─────────────────────┐     │
│   │  Qt 6 Desktop   │           │   Android (Kotlin)  │     │
│   │   Application   │           │     Application     │     │
│   └────────┬────────┘           └──────────┬──────────┘     │
│            │                               │                │
│            └───────────┬───────────────────┘                │
│                        │                                    │
│              ┌─────────▼─────────┐                          │
│              │    libseadrop     │                          │
│              │     (C++17)       │                          │
│              ├───────────────────┤                          │
│              │ • Discovery (BLE) │                          │
│              │ • Connection      │                          │
│              │   (WiFi Direct)   │                          │
│              │ • Transfer        │                          │
│              │ • Clipboard       │                          │
│              │ • Distance Monitor│                          │
│              │ • Security        │                          │
│              │   (libsodium)     │                          │
│              └───────────────────┘                          │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

## 📦 Building

### Prerequisites

**Linux:**
```bash
# Ubuntu/Debian
sudo apt install build-essential cmake pkg-config \
    libsodium-dev libsqlite3-dev \
    libbluetooth-dev libdbus-1-dev \
    qt6-base-dev qt6-tools-dev

# Fedora
sudo dnf install gcc-c++ cmake pkgconfig \
    libsodium-devel sqlite-devel \
    bluez-libs-devel dbus-devel \
    qt6-qtbase-devel qt6-qttools-devel
```

### Build Commands

```bash
# Clone the repository
git clone https://github.com/seadrop/seadrop.git
cd seadrop

# Create build directory
mkdir build && cd build

# Configure
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build . -j$(nproc)

# Run tests
ctest --output-on-failure

# Install (optional)
sudo cmake --install .
```

### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `BUILD_DESKTOP` | ON | Build Qt desktop application |
| `BUILD_TESTS` | ON | Build unit and integration tests |
| `BUILD_SHARED` | ON | Build shared library |
| `ENABLE_SANITIZERS` | OFF | Enable AddressSanitizer |

## 🔒 Security

SeaDrop uses industry-standard cryptography via [libsodium](https://libsodium.org/):

| Algorithm | Purpose |
|-----------|---------|
| **X25519** | Key exchange (ECDH) |
| **XChaCha20-Poly1305** | Authenticated encryption |
| **Ed25519** | Digital signatures |
| **BLAKE2b** | Hashing and checksums |

All transfers are end-to-end encrypted with perfect forward secrecy.

## 📖 Usage

### Quick Start (Desktop)

1. **Launch SeaDrop** - opens in the system tray
2. **Enable discovery** - nearby devices appear in the list
3. **Pair a device** - verify 6-digit PIN on both devices
4. **Share files** - drag & drop or click to browse

### API Example (C++)

```cpp
#include <seadrop/seadrop.h>

int main() {
    seadrop::SeaDrop app;
    
    seadrop::SeaDropConfig config;
    config.device_name = "My Laptop";
    config.download_path = "/home/user/Downloads/SeaDrop";
    
    app.init(config);
    
    // Discovery callback
    app.on_device_discovered([](const seadrop::Device& device) {
        std::cout << "Found: " << device.name << std::endl;
    });
    
    // Start discovering nearby devices
    app.start_discovery();
    
    // ... later, send a file to connected peer
    app.send_file("/path/to/photo.jpg");
    
    return 0;
}
```

## 📁 Project Structure

```
seadrop/
├── CMakeLists.txt              # Root CMake configuration
├── README.md                   # This file
├── LICENSE                     # GPL-3.0-or-later
│
├── libseadrop/                 # Core library
│   ├── include/seadrop/        # Public headers
│   │   ├── seadrop.h           # Main API
│   │   ├── device.h            # Device management
│   │   ├── discovery.h         # BLE discovery
│   │   ├── connection.h        # WiFi Direct
│   │   ├── transfer.h          # File transfer
│   │   ├── clipboard.h         # Clipboard sharing
│   │   ├── distance.h          # Trust zones
│   │   ├── security.h          # Encryption
│   │   └── ...
│   └── src/                    # Implementation
│       ├── platform/linux/     # BlueZ, wpa_supplicant
│       └── platform/android/   # JNI bindings
│
├── desktop/                    # Qt desktop application
│   ├── src/
│   └── resources/
│
├── android/                    # Android application
│   └── app/
│
└── tests/                      # Unit and integration tests
    ├── unit/
    └── integration/
```

## 🛠️ Development Status

### Phase 1: Foundation ✅
- [x] Project structure
- [x] Core API headers
- [x] Encryption module (libsodium)
- [x] Error handling

### Phase 2: Core Protocol 🚧
- [x] File transfer protocol design
- [x] Distance monitoring
- [ ] Transfer state machine
- [ ] Protocol wire format

### Phase 3: Linux Implementation 📋
- [ ] BlueZ BLE integration
- [ ] wpa_supplicant WiFi Direct
- [ ] Qt desktop UI

### Phase 4: Android Implementation 📋
- [ ] Android project setup
- [ ] JNI bridge
- [ ] Material UI

## 🤝 Contributing

Contributions are welcome! Please read our contributing guidelines and submit pull requests.

## 📄 License

SeaDrop is licensed under the **GNU General Public License v3.0 or later**.

See [LICENSE](LICENSE) for the full text.

---

## 🌊 SeaDrop: Seamless. Secure. Offline-First. 💧
