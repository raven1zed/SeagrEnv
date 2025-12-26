# SeaDrop

A cross-platform peer-to-peer file sharing engine that delivers AirDrop-like functionality for Linux and Android. SeaDrop combines BLE-based discovery with WiFi Direct transfers, implementing distance-based trust zones for intelligent security decisions.

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://isocpp.org/)
[![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20Android-green.svg)](#)

## Features

- **Trust-Based Auto-Accept** - Paired devices can share files without confirmation dialogs
- **Distance-Based Trust Zones** - RSSI monitoring creates dynamic security policies based on physical proximity
- **Clipboard Sharing** - Explicit peer-to-peer clipboard push with hotkey support
- **Smart File Handling** - Automatic conflict resolution, folder support, and resumable transfers
- **End-to-End Encryption** - X25519 key exchange, XChaCha20-Poly1305 encryption, Ed25519 signatures

## Architecture

```
┌─────────────────────────────────────────────┐
│              Client Applications            │
│   ┌─────────────┐      ┌─────────────┐     │
│   │ Qt Desktop  │      │   Android   │     │
│   └──────┬──────┘      └──────┬──────┘     │
├──────────┴───────────────────┴──────────────┤
│            libseadrop (C++17)               │
│                                             │
│  • Discovery (BLE)                          │
│  • Connection (WiFi Direct)                 │
│  • Transfer Protocol                        │
│  • Clipboard                                │
│  • Distance Monitor                         │
│  • Security (libsodium)                     │
│  • State Machine                            │
│  • Database (SQLite)                        │
└─────────────────────────────────────────────┘
```

### Trust Zones

| Zone     | Distance | Behavior                              |
|----------|----------|---------------------------------------|
| Intimate | 0-3m     | Full auto-accept, clipboard sharing   |
| Close    | 3-10m    | Auto-accept with notification         |
| Nearby   | 10-30m   | Large files require confirmation      |
| Far      | 30m+     | Always confirm with security alert    |

## Quick Start

### Prerequisites

**Ubuntu/Debian:**
```bash
sudo apt install build-essential cmake pkg-config \
    libsodium-dev libsqlite3-dev \
    libbluetooth-dev libdbus-1-dev \
    qt6-base-dev qt6-tools-dev
```

**Fedora:**
```bash
sudo dnf install gcc-c++ cmake pkgconfig \
    libsodium-devel sqlite-devel \
    bluez-libs-devel dbus-devel \
    qt6-qtbase-devel qt6-qttools-devel
```

### Building

```bash
git clone https://github.com/seadrop/seadrop.git
cd seadrop
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
ctest --output-on-failure
```

### Build Options

| Option              | Default | Description                      |
|---------------------|---------|----------------------------------|
| `BUILD_DESKTOP`     | ON      | Build Qt desktop application     |
| `BUILD_TESTS`       | ON      | Build unit and integration tests |
| `BUILD_SHARED`      | ON      | Build shared library             |
| `ENABLE_SANITIZERS` | OFF     | Enable AddressSanitizer          |

## Usage

### Desktop Application

1. Launch SeaDrop (system tray icon)
2. Enable discovery to find nearby devices
3. Pair with 6-digit PIN verification
4. Share files via drag-and-drop

### Library API

```cpp
#include <seadrop/seadrop.h>

int main() {
    seadrop::SeaDrop app;
    
    seadrop::SeaDropConfig config;
    config.device_name = "My Laptop";
    config.download_path = "/home/user/Downloads/SeaDrop";
    
    app.init(config);
    
    app.on_device_discovered([](const seadrop::Device& device) {
        std::cout << "Found: " << device.name << std::endl;
    });
    
    app.start_discovery();
    app.send_file("/path/to/photo.jpg");
    
    return 0;
}
```

## Project Structure

```
seadrop/
├── libseadrop/            Core C++17 library
│   ├── include/seadrop/   Public API headers
│   └── src/               Implementation + platform code
├── desktop/               Qt desktop application
├── android/               Android application
└── tests/                 Unit and integration tests
```

### Key Modules

- `seadrop.h` - Main façade interface
- `discovery.h` - BLE device discovery
- `connection.h` - WiFi Direct connection management
- `transfer.h` - Chunked file transfer protocol
- `clipboard.h` - Clipboard sharing
- `distance.h` - RSSI-based trust zone calculations
- `security.h` - Cryptographic operations
- `database.h` - SQLite persistence layer

## Security

All transfers use industry-standard cryptography via libsodium:

- **X25519** - Key exchange (ECDH)
- **XChaCha20-Poly1305** - Authenticated encryption
- **Ed25519** - Digital signatures
- **BLAKE2b** - Hashing and checksums

Perfect forward secrecy is maintained for all sessions.

## Contributing

Contributions are welcome. Please ensure all tests pass before submitting pull requests.

## License

Licensed under the GNU General Public License v3.0 or later. See [LICENSE](LICENSE) for details.
