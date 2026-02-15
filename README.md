# Winland Server 🐧📱

**Wayland Compositor for Android**

Winland Server is a full-featured Wayland compositor that runs on Android devices, allowing you to run Linux GUI applications directly on your phone or tablet.

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Platform](https://img.shields.io/badge/platform-Android-green.svg)
![Version](https://img.shields.io/badge/version-1.0.0-orange.svg)

## ✨ Features

### Core Features
- 🖥️ **Full Wayland Compositor**: Complete implementation of Wayland protocol
- 🎮 **Hardware Acceleration**: OpenGL ES 2.0/3.0 support with zero-copy buffer sharing
- 🖱️ **Input Handling**: Touch, keyboard, and mouse support
- 🔊 **Audio Bridge**: PulseAudio/PipeWire integration
- 🔌 **USB Redirection**: Access USB devices from Linux apps

### Advanced Features
- 🪟 **Tiling Window Manager**: i3/Sway-style layouts (tiling, monocle, grid, spiral, dwindle)
- 📺 **VNC Server**: Remote access to your Android desktop
- 📋 **Shared Clipboard**: Copy/paste between Android and Linux apps
- 🐛 **Debug Overlay**: Real-time performance monitoring
- 🔐 **Root Support**: Magisk/KernelSU integration for enhanced functionality

### Performance Optimizations
- ⚡ **Zero-Copy DMA-BUF**: Direct buffer sharing for minimal overhead
- 🎯 **Hardware Compositing**: GPU-accelerated rendering
- 💾 **Memory Efficient**: Optimized for mobile devices
- 🔋 **Battery Friendly**: Optimized rendering loop

## 📋 Requirements

### Minimum Requirements
- Android 8.0 (API 26) or higher
- ARM64 or ARMv7 processor
- 2GB RAM
- 500MB free storage

### Recommended
- Android 10+ with root access (Magisk/KernelSU)
- 4GB+ RAM
- Adreno 500 series or Mali-G71+ GPU

## 🚀 Installation

### Method 1: APK Installation (Non-root)
1. Download the latest APK from [Releases](../../releases)
2. Enable "Install from Unknown Sources" in Settings
3. Install the APK
4. Launch Winland Server and tap "Start Server"

### Method 2: Termux (Recommended)
```bash
# Install Termux from F-Droid
pkg update
pkg install git

# Clone the repository
git clone https://github.com/yourusername/winland-server.git
cd winland-server

# Run setup script
bash scripts/setup-termux.sh

# Build and run
bash scripts/build-automation.sh
```

### Method 3: Magisk Module (Root)
1. Download `winland-magisk-module.zip`
2. Open Magisk Manager
3. Go to Modules → Install from Storage
4. Select the ZIP file and reboot

## 🔧 Building from Source

### Prerequisites
- Android Studio Arctic Fox or newer
- Android NDK r25c or newer
- CMake 3.22+
- Python 3.8+

### Build Steps
```bash
# Clone repository
git clone https://github.com/yourusername/winland-server.git
cd winland-server

# Download dependencies and build
bash scripts/build-automation.sh all

# Or build specific components
bash scripts/build-automation.sh native    # Native code only
bash scripts/build-automation.sh app       # Android app only
```

### Build Options
```bash
# Clean build
bash scripts/build-automation.sh --clean all

# Parallel build with 4 jobs
bash scripts/build-automation.sh --jobs 4 all

# Verbose output
bash scripts/build-automation.sh --verbose all
```

## 📖 Usage

### Starting the Server
1. Open Winland Server app
2. Grant necessary permissions (Overlay, Storage, Audio)
3. Tap "Start Server"
4. The floating window will appear

### Connecting Linux Apps
```bash
# Set Wayland display
export WAYLAND_DISPLAY=wayland-0
export XDG_RUNTIME_DIR=/data/data/com.winland.server/files

# Run your favorite Linux app
weston-terminal
firefox
vlc
```

### Keyboard Shortcuts
- `Super + Enter`: Open terminal
- `Super + Q`: Close focused window
- `Super + Arrow Keys`: Focus window in direction
- `Super + Shift + Arrow Keys`: Move window
- `Super + 1-9`: Switch workspace
- `Super + Shift + 1-9`: Move window to workspace
- `Super + F`: Toggle fullscreen
- `Super + Space`: Next layout
- `Super + Shift + Space`: Previous layout

### Tiling Layouts
- **Tiling**: Windows arranged side by side
- **Monocle**: Single window fullscreen
- **Grid**: Windows in a grid pattern
- **Stacked**: Windows stacked with titles
- **Tabbed**: Windows as tabs
- **Spiral**: Fibonacci spiral layout
- **Dwindle**: Binary space partitioning

## 🔌 VNC Remote Access

Enable VNC server to access your Android desktop remotely:

```bash
# In Winland Server settings, enable VNC
# Default port: 5900

# Connect from another device
vncviewer android-device-ip:5900
```

## 🐛 Debug Mode

Enable debug overlay to monitor performance:

```bash
# In app settings, enable Debug Overlay
# Or via ADB:
adb shell am start -a android.intent.action.MAIN \
    -n com.winland.server/.MainActivity \
    --ez enable_debug true
```

Debug info includes:
- FPS and frame time
- Memory usage
- Surface count
- Input events
- Draw calls
- Connected clients

## 🔐 Security

### Rootless Mode
Winland Server can run without root, but with limited functionality:
- ✅ Basic Wayland compositor
- ✅ Software rendering
- ❌ Hardware acceleration
- ❌ USB redirection
- ❌ Some input devices

### Sandboxing
When running with root, the server uses namespaces to sandbox Linux applications:
- Isolated filesystem view
- Restricted network access
- Limited device access

## 🛠️ Troubleshooting

### Common Issues

#### "Cannot start server"
- Check if overlay permission is granted
- Ensure sufficient RAM is available
- Try restarting the app

#### "Black screen"
- Verify GPU drivers are up to date
- Try software rendering mode
- Check logcat for errors

#### "Apps not connecting"
- Verify `WAYLAND_DISPLAY` is set correctly
- Check `XDG_RUNTIME_DIR` permissions
- Ensure the server socket exists

### Log Collection
```bash
# Collect logs for bug reports
adb logcat -d > winland-logs.txt
adb shell dumpsys meminfo com.winland.server >> winland-logs.txt
```

## 🤝 Contributing

We welcome contributions! Please see [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

### Development Setup
```bash
# Fork and clone
git clone https://github.com/yourusername/winland-server.git

# Create branch
git checkout -b feature/your-feature

# Make changes and commit
git commit -am "Add your feature"

# Push and create PR
git push origin feature/your-feature
```

## 📄 License

Winland Server is licensed under the MIT License. See [LICENSE](LICENSE) for details.

## 🙏 Acknowledgments

- [Wayland](https://wayland.freedesktop.org/) - Display server protocol
- [wlroots](https://gitlab.freedesktop.org/wlroots/wlroots/) - Reference compositor library
- [Sway](https://swaywm.org/) - Tiling Wayland compositor inspiration
- [Termux](https://termux.com/) - Android terminal emulator

## 📞 Support

- 📧 Email: support@winland-server.dev
- 💬 Discord: [Join our server](https://discord.gg/winland)
- 🐛 Issues: [GitHub Issues](../../issues)

---

<p align="center">
  Made with ❤️ for the Linux on Android community
</p>
