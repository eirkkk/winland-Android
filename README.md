# Winland Server 🐧📱

**Native Wayland Compositor for Android**

Winland Server is a full-featured Wayland compositor that runs natively on Android devices, enabling Linux GUI applications to run directly on phones and tablets. Built with the [Smithay](https://github.com/Smithay/smithay) compositor library in Rust with a Kotlin/Compose Multiplatform UI.

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Android%208.0%2B-green.svg)](app/build.gradle.kts)
[![Architecture](https://img.shields.io/badge/arch-ARM64-red.svg)](.cargo/config.toml)
[![API](https://img.shields.io/badge/API-26%2B-orange.svg)](app/build.gradle.kts)

---

## ✨ Features

### 🖥️ Display & Graphics
- **Native Wayland compositor** built with [Smithay](https://github.com/Smithay/smithay) — no VNC/RDP/streaming overhead
- **Vulkan hardware acceleration** via Zink (OpenGL-on-Vulkan) using Turnip driver on Adreno GPUs, fully integrated with LabWC for desktop-wide GPU compositing
- **GPU acceleration per-app**: Type `gpu` in the terminal followed by the game/app name to launch it with the Zink accelerator enabled
- **OpenGL ES path** via Android HardwareBuffers for devices without Vulkan
- **XWayland** support for legacy X11 applications
- **Dynamic resolution** with persistent scale locking across Android lifecycle events
- **Screen rotation support** — seamless landscape/portrait switching with correct input coordinate mapping and pointer icon management
- **Multi-display ready** architecture

### 🎮 Input Systems
- **Touch** — Direct touch-to-Wayland-touch translation with gesture support (window move/resize, 3-finger swipe to cycle windows)
- **Trackpad** — Relative pointer motion with acceleration, two-finger tap for click, pinch-to-zoom
- **Bluetooth & wired mouse** — full desktop-grade pointer support with left-click, right-click, hover, and orientation-aware coordinates
- **Bluetooth & USB keyboard** — complete keymap handling with XKB, multi-key shortcuts, physical keyboard input via Android

### 📦 Linux Distro Management
- Install, setup, run, stop, and restart Linux distributions
- Root filesystem download and extraction
- Chroot-based isolation with bind-mount management
- Clean unmount on shutdown/reboot

### 🔓 Rootless Mode (No Root Required)
- **Run Linux desktops without root** via bundled `proot` (user-space chroot over `ptrace`)
- **Execution mode choice** at launch and in Settings: Root chroot or Rootless proot
- proot shipped as a native library (`jniLibs`, exec-safe on Android 10+ W^X) with an assets fallback for older devices
- Pure-Java rootfs extractor (gzip/xz) — no dependency on executable helpers in app-private storage
- Same desktop experience: XFCE on LabWC, Wayland socket sharing, PulseAudio bridge, hardware-independent software rendering
- proot sources vendored in `proot/` and built automatically by `build-arm64.sh` (see `docs/PROOT_MODE.md`)

### 🔊 Audio & Clipboard
- **Native audio playback** — Zero-copy PCM audio routed via Oboe (AAudio) between chroot PulseAudio and Android audio hardware through a named FIFO pipe
- **Native microphone input** — Android mic captured via Oboe (AAudio) at 44.1kHz mono 16-bit and streamed into chroot PulseAudio through a dedicated FIFO pipe
- **Clipboard sync** — Full copy/paste between Android and Linux applications (X11 and Wayland)

### 🔧 Developer Tools
- Live log panel with search, copy, and pause
- Real-time compositor diagnostics (FPS, surface count, input events)
- Runtime stats overlay
- Debug logging via Android logcat

### 📦 Chroot Environment
- **Ubuntu 24.04 (Noble)** and **Kali Nethunter** rootfs support
- **XFCE desktop** session running on **LabWC** (Wayland compositor) with GPU-accelerated compositing via Zink+Turnip
- Automatic rootfs download, extraction, and dependency installation
- BusyBox-based bootstrap for reliable chroot setup

---

## 📋 Requirements

| Requirement | Minimum | Recommended |
|------------|---------|-------------|
| **OS** | Android 8.0 (API 26) | Android 10+ |
| **Architecture** | ARM64 (arm64-v8a) | ARM64 |
| **RAM** | 3 GB | 6 GB+ |
| **Storage** | 2 GB free | 8 GB+ |
| **GPU** | Adreno 650+ (Vulkan 1.1+ for Turnip) | OpenGL ES 3.0+ |
| **Root** | Not required (Rootless proot mode) | Rooted device (Root chroot mode, optional) |

---

## 🚀 Quick Start

### Install APK
1. Download the latest APK from [Releases](https://github.com/anomalyco/winland-android/releases)
2. Enable **Install from Unknown Sources** in Android Settings
3. Install the APK and launch **Winland Server**
4. Choose the **execution mode** (Rootless proot for non-rooted devices, Root chroot for rooted ones) — changeable later in Settings
5. Select a Linux distribution from the **Home** tab and tap **Install**
6. Once installed, tap **Run** to start the compositor

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

**Full device build (recommended on ARM64):**
```bash
./build-arm64.sh   # libxkbcommon + proot (from proot/ sources) + Rust + APK
```
The script builds `proot`/`proot-loader` from the vendored sources in `proot/` (Termux fork + static talloc, NDK `/opt/android/ndk`) and deploys them to `jniLibs/` and `assets/bin/` automatically when missing.

See [BUILD_ARM64.md](BUILD_ARM64.md) for detailed ARM64 build instructions.

---

## How It Works

1. **Bootstrapping**: The app downloads a compressed Linux rootfs (Ubuntu or Kali) and extracts it into the app's private data directory.

2. **Chroot Setup**: With root privileges, the app mounts `/proc`, `/sys`, `/dev`, and bind-mounts the Wayland socket and audio FIFO into the chroot. In **Rootless (proot) mode** the same layout is achieved with user-space binds — no mounts, no privileges needed.

3. **Compositor**: The Rust-based Smithay compositor creates a Wayland socket inside the app's data directory. This socket is bind-mounted into the chroot so Linux GUI apps can connect to it.

4. **Desktop Launch**: Inside the chroot, LabWC (a Wayland compositor) starts as the window manager with GPU-accelerated compositing via Zink+Turnip, XFCE as the desktop environment, and XWayland for X11 app compatibility.

5. **Input Routing**: Touch events, keyboard input, and mouse events from Android are forwarded through UniFFI to the Rust compositor, which injects them into the Wayland protocol.

6. **Audio Bridge (Native)**: PulseAudio inside the chroot writes PCM audio to a named FIFO; the Rust audio bridge reads it via Oboe (AAudio) for zero-copy playback on Android speakers. For microphone input, Oboe captures 44.1kHz mono 16-bit audio from the Android mic and writes to a second FIFO, which PulseAudio reads as the `AndroidMic` source — all natively with no streaming overhead.

7. **Rendering**: The compositor renders directly onto an Android `SurfaceView` using Vulkan (Zink/Turnip) or OpenGL ES via HardwareBuffers.

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
| **Rendering** | OpenGL ES 2.0 / Vulkan (Zink+Turnip) | Surface compositing, texture upload, NDC mapping |
| **Audio** | Oboe (AAudio) | Native zero-copy audio playback + mic capture via FIFO pipes |
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
| Low FPS | Missing GPU acceleration | Verify OpenGL ES / Vulkan support |

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
- The [Turnip](https://gitlab.freedesktop.org/mesa/mesa/-/tree/master/src/gallium/drivers/zink) driver team for Vulkan-on-Adreno support
- [Termux](https://termux.com/) — Android terminal emulator (terminal-view/terminal-emulator modules)
- [proot (Termux fork)](https://github.com/termux/proot) — user-space chroot powering Rootless mode
- [UniFFI](https://github.com/mozilla/uniffi-rs) — Rust-to-Kotlin bindings generator
- [Android NDK](https://developer.android.com/ndk) — Native development kit

---

<p align="center">
  Made with ❤️ for the Linux on Android community
</p>
