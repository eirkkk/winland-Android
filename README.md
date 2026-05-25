# Winland Server 🐧📱

**Native Wayland Compositor for Android**

Winland Server is a full-featured Wayland compositor that runs natively on Android devices, enabling Linux GUI applications to run directly on phones and tablets. Built with the [Smithay](https://github.com/Smithay/smithay) compositor library in Rust with a Kotlin/Compose Multiplatform UI.

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Android%208.0%2B-green.svg)](app/build.gradle.kts)
[![Architecture](https://img.shields.io/badge/arch-ARM64-red.svg)](.cargo/config.toml)
[![API](https://img.shields.io/badge/API-26%2B-orange.svg)](app/build.gradle.kts)

---

## ✨ Features

### 🖥️ Wayland Compositor
- Full Wayland protocol implementation via Rust/Smithay stack
- OpenGL ES 2.0 hardware-accelerated rendering via EGL
- Fractional scaling support (1080p / 720p resolution presets)
- XWayland compatibility for X11-only applications
- SHM and DMA-BUF buffer strategies

### 🎮 Input Modes
- **Touch** — Direct touch-to-Wayland-touch translation with gesture support (window move/resize, 3-finger swipe to cycle windows)
- **Trackpad** — Relative pointer motion with acceleration, two-finger tap for click, pinch-to-zoom
- **Mouse** — Absolute pointer emulation with constrained-relative mode for pointer-lock applications (games, design tools)

### 📦 Linux Distro Management
- Install, setup, run, stop, and restart Linux distributions
- Root filesystem download and extraction
- Chroot-based isolation with bind-mount management
- Clean unmount on shutdown/reboot

### 🔊 Audio & USB
- Audio bridge via CPAL (cross-platform audio library)
- USB device redirection for peripherals

### 🔧 Developer Tools
- Live log panel with search, copy, and pause
- Real-time compositor diagnostics (FPS, surface count, input events)
- Runtime stats overlay
- Debug logging via Android logcat

---

## 📋 Requirements

| Requirement | Minimum | Recommended |
|------------|---------|-------------|
| **OS** | Android 8.0 (API 26) | Android 10+ |
| **Architecture** | ARM64 (arm64-v8a) | ARM64 |
| **RAM** | 3 GB | 6 GB+ |
| **Storage** | 2 GB free | 8 GB+ |
| **GPU** | OpenGL ES 2.0 | OpenGL ES 3.0+ |

---

## 🚀 Quick Start

### Install APK
1. Download the latest APK from [Releases](https://github.com/anomalyco/winland-android/releases)
2. Enable **Install from Unknown Sources** in Android Settings
3. Install the APK and launch **Winland Server**
4. Select a Linux distribution from the **Home** tab and tap **Install**
5. Once installed, tap **Run** to start the compositor

### Build from Source

**Prerequisites:**
- Android Studio Hedgehog (2023.1.1+) or newer
- Android NDK r26+ (set path in `.cargo/config.toml`)
- Rust toolchain with `aarch64-linux-android` target
- JDK 17+

```bash
git clone https://github.com/anomalyco/winland-android.git
cd winland-android

# Install dependencies
make setup

# Full build (Rust + bindings + APK)
make build

# Or build individually:
make build-rust        # Build Rust library only
make generate-bindings # Generate UniFFI bindings
make build-app         # Build Android APK

# Build release with LTO/NEON optimizations
make build-release

# Install on connected device
make install
```

**Manual build:**
```bash
# Build Rust native library
cargo build --release --target aarch64-linux-android --features smithay_android

# Copy library
cp native/target/aarch64-linux-android/release/libuniffi_winland_core.so \
   app/src/main/jniLibs/arm64-v8a/

# Build APK
./gradlew clean assembleDebug
```

See [BUILD_ARM64.md](BUILD_ARM64.md) for detailed ARM64 build instructions.

---

## 📖 Usage

### Dashboard
The main dashboard has three tabs:

| Tab | Description |
|-----|-------------|
| **Home** | Distro management (install/setup/run/stop/restart), live logs, connection instructions |
| **Terminal** | Embedded Android terminal via Termux emulator library |
| **Settings** | Display (resolution), input mode (Touch/Trackpad/Mouse), scroll sensitivity, refresh rate, theme, runtime controls |

### Input Mode Descriptions
| Mode | Best For | Behavior |
|------|----------|----------|
| **Touch** | General navigation | Direct touch input, window move/resize by titlebar/edges, 3-finger swipe to cycle windows |
| **Trackpad** | Precise cursor control | Smooth relative motion with acceleration, one-finger drag, two-finger tap for click |
| **Mouse** | Gaming / pointer-lock | Absolute pointer positioning, constrained-relative mode for pointer lock |

### Display Settings
- **1080p** (scale 1.0) — Native resolution, sharper text
- **720p** (scale 1.5) — Larger UI elements, better readability on small screens

### Running Linux Apps
Once the compositor is running (tap **Run**), Linux applications connect to the Wayland socket:
```bash
export WAYLAND_DISPLAY=wayland-0
export XDG_RUNTIME_DIR=/data/data/com.winland.server/files
your-linux-app
```

### Keyboard Shortcuts
| Shortcut | Action |
|----------|--------|
| `Alt + Tab` | Cycle through windows forward |
| `Alt + Shift + Tab` | Cycle through windows backward |
| Titlebar ✕ button | Close window |

---

## 🏗️ Architecture

```
┌─────────────────────────────────────────────────────┐
│                   Android Device                     │
│  ┌───────────────────────────────────────────┐      │
│  │          Kotlin/Compose UI                 │      │
│  │  ┌────────┐  ┌──────────┐  ┌───────────┐  │      │
│  │  │Dashboard│  │ Display  │  │ Terminal  │  │      │
│  │  │ Screen  │  │ Activity │  │ Activity  │  │      │
│  │  └────────┘  └──────────┘  └───────────┘  │      │
│  └───────────────────────────────────────────┘      │
│                          │ JNI / UniFFI             │
│  ┌───────────────────────────────────────────┐      │
│  │          Rust Native Library               │      │
│  │  ┌────────────┐  ┌──────────────────┐     │      │
│  │  │  JNI Bridge │  │  Wayland Server   │     │      │
│  │  │  (input,    │  │  (Smithay-based)  │     │      │
│  │  │   surface,  │  │  ┌────────────┐   │     │      │
│  │  │   lifecycle)│  │  │ Seat/Input │   │     │      │
│  │  └────────────┘  │  ├────────────┤   │     │      │
│  │                  │  │ Shell (XDG)│   │     │      │
│  │  ┌────────────┐  │  ├────────────┤   │     │      │
│  │  │  Audio /   │  │  │ Renderer   │   │     │      │
│  │  │  USB /     │  │  │ (OpenGL ES)│   │     │      │
│  │  │  Distro    │  │  └────────────┘   │     │      │
│  │  └────────────┘  └──────────────────┘     │      │
│  └───────────────────────────────────────────┘      │
└─────────────────────────────────────────────────────┘
```

### Key Components
| Layer | Technology | Purpose |
|-------|-----------|---------|
| **UI** | Kotlin + Compose | Dashboard, settings, live logs |
| **Display** | SurfaceView + EGL | Full-screen compositor output, touch input routing |
| **Bridge** | JNI + UniFFI | Kotlin↔Rust communication |
| **Compositor** | Rust (Smithay) | Wayland protocol, input routing, shell management |
| **Rendering** | OpenGL ES 2.0 | Surface compositing, texture upload, NDC mapping |
| **Audio** | CPAL | Audio playback from Linux apps |
| **Distro** | Rust bindings | Rootfs management, chroot lifecycle |

---

## 🛠️ Makefile Targets

| Target | Description |
|--------|-------------|
| `make setup` | Install Rust ARM64 target, create cargo config |
| `make build-rust` | Build Rust library for ARM64, copy .so to jniLibs |
| `make generate-bindings` | Generate UniFFI Kotlin bindings from UDL |
| `make build-app` | Build APK (requires built Rust library) |
| `make build` | Full build (Rust → bindings → APK) |
| `make build-release` | Release build with LTO/NEON |
| `make install` | Build + install APK via adb |
| `make clean` | Clean Gradle + Cargo artifacts |
| `make test` | Run Rust + Gradle tests |
| `make lint` | Run Clippy + Android lint |
| `make docs` | Generate Rust documentation |

---

## 🐛 Debugging & Diagnostics

### Log Collection
```bash
# Filter Winland logs
adb logcat -s WinlandServer,NativeBridge,SmithayRuntime,Compositor

# Full dump with memory info
adb logcat -d > winland-logs.txt
adb shell dumpsys meminfo com.winland.server >> winland-logs.txt
```

### Runtime Stats
The compositor exposes real-time diagnostics via `getWaylandRuntimeStats()` (accessible from the Dashboard debug panel).

### Common Issues

| Symptom | Likely Cause | Solution |
|---------|-------------|----------|
| Black screen | EGL init failure / Surface not bound | Restart the compositor |
| Input not working | Wrong input mode | Switch mode in Settings tab |
| Apps crash on connect | XDG_RUNTIME_DIR permissions | Ensure directory is accessible |
| Low FPS | Missing GPU acceleration | Verify OpenGL ES support |

---

## 🤝 Contributing

Contributions are welcome! Please read [CONTRIBUTING.md](CONTRIBUTING.md) for our contribution guidelines, code of conduct, and development workflow.

### Development Process
1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes following Conventional Commits
4. Push and open a Pull Request

See [ARCHITECTURE.md](docs/ARCHITECTURE.md) for detailed system architecture documentation.

---

## 📄 License

Distributed under the **MIT License**. See [LICENSE](LICENSE) for more information.

Copyright (c) 2024 Winland Server Contributors

---

## 🙏 Acknowledgments

- [Smithay](https://github.com/Smithay/smithay) — Rust Wayland compositor library
- [Wayland](https://wayland.freedesktop.org/) — Display server protocol
- [Termux](https://termux.com/) — Android terminal emulator (terminal-view/terminal-emulator modules)
- [UniFFI](https://github.com/mozilla/uniffi-rs) — Rust-to-Kotlin bindings generator
- [Android NDK](https://developer.android.com/ndk) — Native development kit

---

<p align="center">
  Made with ❤️ for the Linux on Android community
</p>
