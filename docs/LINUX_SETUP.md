# SeaDrop Linux Setup Guide

This guide covers all dependencies required to build and run SeaDrop on Linux.

## Quick Install (Debian/Ubuntu)

```bash
# Build essentials
sudo apt update
sudo apt install -y build-essential cmake git pkg-config

# Core dependencies
sudo apt install -y libsodium-dev libsqlite3-dev

# Platform dependencies (BLE & WiFi Direct)
sudo apt install -y libdbus-1-dev libbluetooth-dev

# Clipboard tools
sudo apt install -y xclip xsel wl-clipboard

# Testing framework
sudo apt install -y libgtest-dev

# Runtime services
sudo apt install -y bluez wpasupplicant
```

## Quick Install (Fedora/RHEL)

```bash
# Build essentials
sudo dnf install -y gcc-c++ cmake git pkgconfig

# Core dependencies
sudo dnf install -y libsodium-devel sqlite-devel

# Platform dependencies
sudo dnf install -y dbus-devel bluez-libs-devel

# Clipboard tools
sudo dnf install -y xclip xsel wl-clipboard

# Testing framework
sudo dnf install -y gtest-devel

# Runtime services
sudo dnf install -y bluez wpasupplicant
```

## Quick Install (Arch Linux)

```bash
# Build essentials
sudo pacman -S --needed base-devel cmake git pkgconf

# Core dependencies
sudo pacman -S --needed libsodium sqlite

# Platform dependencies
sudo pacman -S --needed dbus bluez-libs

# Clipboard tools
sudo pacman -S --needed xclip xsel wl-clipboard

# Testing framework
sudo pacman -S --needed gtest

# Runtime services
sudo pacman -S --needed bluez wpa_supplicant
```

---

## Detailed Dependency Reference

### 1. Build Tools

| Package | Purpose | Required |
|---------|---------|----------|
| `gcc` / `g++` | C/C++ compiler (C++17) | ✅ Yes |
| `cmake` | Build system (3.16+) | ✅ Yes |
| `pkg-config` | Library discovery | ✅ Yes |
| `git` | Version control | ✅ Yes |
| `make` | Build automation | ✅ Yes |

### 2. Core Libraries

| Package | Purpose | Required |
|---------|---------|----------|
| `libsodium-dev` | Encryption, key exchange, signatures | ✅ Yes |
| `libsqlite3-dev` | Local database for device/trust storage | ✅ Yes |

### 3. Platform Libraries (Linux-specific)

| Package | Purpose | Required |
|---------|---------|----------|
| `libdbus-1-dev` | D-Bus IPC for BlueZ & wpa_supplicant | ✅ Yes |
| `libbluetooth-dev` | BlueZ development headers | ✅ Yes |

### 4. Clipboard Tools (at least one)

| Package | Purpose | Display Server |
|---------|---------|----------------|
| `xclip` | Clipboard access | X11 |
| `xsel` | Clipboard access (alternative) | X11 |
| `wl-clipboard` | Clipboard access | Wayland |

### 5. Runtime Services

| Service | Purpose | Required |
|---------|---------|----------|
| `bluez` | Bluetooth daemon | ✅ Yes |
| `wpasupplicant` | WiFi Direct (P2P) | ✅ Yes |

### 6. Testing (Optional)

| Package | Purpose |
|---------|---------|
| `libgtest-dev` | Google Test framework |

---

## Service Configuration

### Enable Bluetooth

```bash
# Start and enable BlueZ service
sudo systemctl enable bluetooth
sudo systemctl start bluetooth

# Verify Bluetooth is working
bluetoothctl show
```

### Enable wpa_supplicant

```bash
# Check if WiFi interface supports P2P
iw phy | grep P2P

# wpa_supplicant is typically started automatically
# If needed, start manually:
sudo systemctl start wpa_supplicant
```

---

## Permissions Setup

### Add User to Required Groups

```bash
# Bluetooth access
sudo usermod -aG bluetooth $USER

# Network/WiFi access (optional, for direct wpa_supplicant access)
sudo usermod -aG netdev $USER

# Log out and back in for changes to take effect
```

### D-Bus Permissions

Create `/etc/dbus-1/system.d/seadrop.conf`:

```xml
<!DOCTYPE busconfig PUBLIC "-//freedesktop//DTD D-BUS Bus Configuration 1.0//EN"
  "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
<busconfig>
  <policy user="your_username">
    <!-- BlueZ access -->
    <allow send_destination="org.bluez"/>
    <allow send_interface="org.bluez.Adapter1"/>
    <allow send_interface="org.bluez.LEAdvertisingManager1"/>
    
    <!-- wpa_supplicant access -->
    <allow send_destination="fi.w1.wpa_supplicant1"/>
    <allow send_interface="fi.w1.wpa_supplicant1.Interface.P2PDevice"/>
  </policy>
</busconfig>
```

Then reload D-Bus:
```bash
sudo systemctl reload dbus
```

---

## Build SeaDrop

```bash
# Clone repository
git clone https://github.com/your-repo/seadrop.git
cd seadrop

# Create build directory
mkdir build && cd build

# Configure
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
make -j$(nproc)

# Run tests
ctest --output-on-failure
```

---

## Troubleshooting

### "dbus/dbus.h not found"
```bash
sudo apt install libdbus-1-dev  # Debian/Ubuntu
```

### "bluetooth/bluetooth.h not found"
```bash
sudo apt install libbluetooth-dev  # Debian/Ubuntu
```

### "No Bluetooth adapter found"
```bash
# Check if adapter exists
hciconfig -a

# If powered off, turn on
sudo hciconfig hci0 up
```

### "WiFi interface doesn't support P2P"
Not all WiFi adapters support WiFi Direct. Check with:
```bash
iw phy | grep -A 20 "Supported interface modes" | grep P2P
```

### Clipboard not working
```bash
# Check display server
echo $XDG_SESSION_TYPE  # Should show "x11" or "wayland"

# Install appropriate tool
sudo apt install xclip        # for X11
sudo apt install wl-clipboard  # for Wayland
```

---

## Minimum System Requirements

| Component | Minimum |
|-----------|---------|
| Linux Kernel | 5.0+ |
| BlueZ | 5.50+ |
| wpa_supplicant | 2.9+ |
| GCC/Clang | GCC 8+ / Clang 7+ (C++17) |
| CMake | 3.16+ |
