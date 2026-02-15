# Changelog

All notable changes to Winland Server will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Initial release of Winland Server
- Full Wayland compositor implementation
- Hardware acceleration with OpenGL ES
- DMA-BUF zero-copy buffer sharing
- Tiling window manager with multiple layouts
- VNC server for remote access
- Audio bridge with PulseAudio/PipeWire support
- USB device redirection
- Shared clipboard between Android and Linux
- Debug overlay with performance metrics
- Magisk/KernelSU root support
- Multi-window support for Android
- Gesture recognition
- Hot reloading for development

### Security
- Sandboxing for Linux applications
- Rootless mode support
- Permission-based access control

## [1.0.0] - 2024-XX-XX

### Added
- Core Wayland compositor
- EGL display backend
- Input handling (touch, keyboard, mouse)
- Surface management
- Output management
- Seat management
- XDG shell implementation
- Basic window management
- Android app interface
- Floating window overlay
- Service-based architecture

### Technical
- CMake build system
- Gradle integration
- Automated build scripts
- Cross-compilation support
- NDK integration

## Roadmap

### [1.1.0] - Planned
- PipeWire audio support improvements
- Better hardware codec support
- Multi-monitor support
- Touchpad gesture improvements
- Performance optimizations

### [1.2.0] - Planned
- RDP server support
- Better clipboard integration
- File sharing between Android and Linux
- Notification integration

### [2.0.0] - Planned
- Full XWayland support
- GPU passthrough
- Better game controller support
- VR/AR support

---

## Version History

### Version Numbering

- **Major** (X.0.0): Breaking changes, major features
- **Minor** (0.X.0): New features, backwards compatible
- **Patch** (0.0.X): Bug fixes, security updates

### Pre-releases

- Alpha: `1.0.0-alpha.1`
- Beta: `1.0.0-beta.1`
- RC: `1.0.0-rc.1`

---

## Contributing to Changelog

When making changes, add an entry under the `[Unreleased]` section:

```markdown
### Added
- Description of new feature

### Changed
- Description of change

### Deprecated
- Description of deprecated feature

### Removed
- Description of removed feature

### Fixed
- Description of bug fix

### Security
- Description of security fix
```
