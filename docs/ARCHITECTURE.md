# Winland Server Architecture

## Overview

Winland Server is a Wayland compositor designed specifically for Android devices. It consists of multiple layers working together to provide a complete Linux GUI environment on mobile devices.

## System Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                      Android Layer                          │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │   Activity   │  │   Service    │  │   Overlay    │      │
│  │  (Main UI)   │  │(Background)  │  │   Window     │      │
│  └──────────────┘  └──────────────┘  └──────────────┘      │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼ JNI
┌─────────────────────────────────────────────────────────────┐
│                      Native Layer                           │
│  ┌─────────────────────────────────────────────────────┐   │
│  │              Wayland Compositor Core                 │   │
│  │  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐   │   │
│  │  │ Display │ │ Surface │ │ Output  │ │  Seat   │   │   │
│  │  │ Manager │ │ Manager │ │ Manager │ │ Manager │   │   │
│  │  └─────────┘ └─────────┘ └─────────┘ └─────────┘   │   │
│  └─────────────────────────────────────────────────────┘   │
│  ┌─────────────────────────────────────────────────────┐   │
│  │              Graphics & Rendering                    │   │
│  │  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐   │   │
│  │  │   EGL   │ │ OpenGL  │ │ DMA-BUF │ │  VNC    │   │   │
│  │  │ Display │ │   ES    │ │ Handler │ │ Server  │   │   │
│  │  └─────────┘ └─────────┘ └─────────┘ └─────────┘   │   │
│  └─────────────────────────────────────────────────────┘   │
│  ┌─────────────────────────────────────────────────────┐   │
│  │              Window Management                       │   │
│  │  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐   │   │
│  │  │  XDG    │ │ Tiling  │ │ Floating│ │  Focus  │   │   │
│  │  │  Shell  │ │ Layout  │ │ Windows │ │ Manager │   │   │
│  │  └─────────┘ └─────────┘ └─────────┘ └─────────┘   │   │
│  └─────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼ Wayland Protocol
┌─────────────────────────────────────────────────────────────┐
│                    Linux Applications                       │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐    │
│  │  GTK App │  │ Qt App   │  │ SDL App  │  │ Weston   │    │
│  │          │  │          │  │          │  │ Terminal │    │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘    │
└─────────────────────────────────────────────────────────────┘
```

## Core Components

### 1. Wayland Compositor Core

The heart of Winland Server, implementing the Wayland protocol.

#### Display Manager
- Manages the Wayland display connection
- Handles client connections and disconnections
- Processes Wayland protocol messages

#### Surface Manager
- Tracks all surfaces (windows) from clients
- Manages surface lifecycle (create, destroy, commit)
- Handles surface stacking order

#### Output Manager
- Manages display outputs
- Handles resolution changes
- Sends output information to clients

#### Seat Manager
- Manages input devices (keyboard, pointer, touch)
- Sends input events to clients
- Handles focus changes

### 2. Graphics & Rendering

#### EGL Display
- Creates EGL context for rendering
- Manages the framebuffer
- Handles buffer swaps

#### OpenGL ES Renderer
- Renders surfaces using OpenGL ES 2.0/3.0
- Applies transformations and effects
- Composites multiple surfaces

#### DMA-BUF Handler
- Enables zero-copy buffer sharing
- Imports/export DMA-BUF buffers
- Optimizes performance

#### VNC Server
- Provides remote access to the display
- Supports multiple VNC clients
- Implements RFB protocol

### 3. Window Management

#### XDG Shell Implementation
- Implements xdg-shell protocol
- Handles toplevel windows
- Manages popups and dialogs

#### Tiling Layout Engine
- Implements i3/Sway-style layouts
- Supports multiple layout types:
  - Tiling
  - Monocle
  - Grid
  - Stacked
  - Tabbed
  - Spiral
  - Dwindle

#### Floating Windows
- Supports free-floating windows
- Allows manual positioning and sizing
- Provides window decorations

### 4. Bridges

#### Audio Bridge
- Connects Linux audio to Android
- Supports PulseAudio/PipeWire
- Low-latency audio streaming

#### USB Redirect
- Redirects USB devices to Linux
- Supports various device types
- Uses USB/IP protocol

#### Clipboard Bridge
- Synchronizes clipboard between Android and Linux
- Supports multiple MIME types
- Handles text, images, and files

## Data Flow

### Rendering Pipeline

```
┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│   Client    │────▶│   Surface   │────▶│   Buffer    │
│   Buffer    │     │   Commit    │     │   Upload    │
└─────────────┘     └─────────────┘     └──────┬──────┘
                                                │
                                                ▼
┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│   Screen    │◀────│   Compose   │◀────│   Texture   │
│   Display   │     │   Render    │     │   Create    │
└─────────────┘     └─────────────┘     └─────────────┘
```

### Input Pipeline

```
┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│   Android   │────▶│   Input     │────▶│   Wayland   │
│   Event     │     │   Handler   │     │   Event     │
└─────────────┘     └─────────────┘     └──────┬──────┘
                                                │
                                                ▼
                                         ┌─────────────┐
                                         │   Client    │
                                         │   Process   │
                                         └─────────────┘
```

## Threading Model

```
┌─────────────────────────────────────────────────────────┐
│                    Main Thread                            │
│  - Wayland event loop                                     │
│  - Client message processing                              │
│  - Window management                                      │
└─────────────────────────────────────────────────────────┘
                            │
        ┌───────────────────┼───────────────────┐
        ▼                   ▼                   ▼
┌───────────────┐   ┌───────────────┐   ┌───────────────┐
│ Render Thread │   │  VNC Thread   │   │  Audio Thread │
│               │   │               │   │               │
│ - OpenGL ES   │   │ - RFB Protocol│   │ - PulseAudio  │
│ - Compositing │   │ - Encoding    │   │ - Streaming   │
└───────────────┘   └───────────────┘   └───────────────┘
```

## Memory Management

### Buffer Lifecycle

1. **Allocation**: Client allocates buffer via wl_shm or dma-buf
2. **Attach**: Surface attaches the buffer
3. **Commit**: Surface commits changes
4. **Upload**: Buffer uploaded to GPU texture
5. **Render**: Texture rendered to screen
6. **Release**: Buffer released back to client

### Memory Pools

- **Surface Pool**: Manages surface structures
- **Buffer Pool**: Manages buffer references
- **Texture Pool**: Manages GPU textures
- **Event Pool**: Manages pending events

## Security Model

### Sandboxing

```
┌─────────────────────────────────────────────────────────┐
│                    Android Sandbox                        │
│  ┌─────────────────────────────────────────────────┐   │
│  │              Winland Server Process               │   │
│  │  ┌─────────────────────────────────────────┐   │   │
│  │  │         Linux App Namespace             │   │   │
│  │  │  - Isolated filesystem view             │   │   │
│  │  │  - Restricted network access            │   │   │
│  │  │  - Limited device access                │   │   │
│  │  └─────────────────────────────────────────┘   │   │
│  └─────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
```

### Permission Model

| Feature | Non-Root | Root |
|---------|----------|------|
| Basic Compositor | ✅ | ✅ |
| Hardware Acceleration | ❌ | ✅ |
| USB Redirection | ❌ | ✅ |
| Full Input Access | ❌ | ✅ |
| Custom Resolutions | ❌ | ✅ |

## Performance Considerations

### Optimizations

1. **Zero-Copy Rendering**: DMA-BUF eliminates buffer copies
2. **GPU Compositing**: Hardware-accelerated surface composition
3. **Damage Tracking**: Only redraw changed regions
4. **Texture Caching**: Reuse textures when possible
5. **Event Coalescing**: Batch input events

### Benchmarks

| Metric | Target | Notes |
|--------|--------|-------|
| FPS | 60 | At 1080p |
| Latency | <16ms | Input to display |
| Memory | <200MB | Idle state |
| CPU | <10% | Idle state |

## Extension Points

### Adding New Layouts

```c
// In tiling_layout.c
void layout_apply_custom(struct tiling_container* parent,
                         int32_t x, int32_t y,
                         int32_t width, int32_t height) {
    // Your layout logic here
}
```

### Adding New Protocols

```c
// Create new protocol file
// In wayland_compositor.c
wl_global_create(display, &your_protocol_interface,
                 version, data, bind_callback);
```

## Debugging

### Debug Overlay

The debug overlay provides real-time information:
- FPS counter
- Memory usage
- Surface count
- Input events
- Draw calls

### Logging

```c
// Set log level
export WINLAND_LOG_LEVEL=debug

// Enable specific modules
export WINLAND_LOG_MODULES=display,input,render
```

## Future Enhancements

### Planned Features

1. **XWayland Support**: Run X11 applications
2. **VR/AR Support**: Head-mounted display support
3. **Multi-Display**: Multiple monitor support
4. **Game Mode**: Optimized for gaming
5. **AI Upscaling**: DLSS-style upscaling

### Research Areas

1. **Vulkan Renderer**: Modern graphics API
2. **Wayland Security Context**: Better sandboxing
3. **Color Management**: HDR support
4. **Variable Refresh Rate**: Adaptive sync

## References

- [Wayland Protocol](https://wayland.freedesktop.org/docs/html/)
- [wlroots](https://gitlab.freedesktop.org/wlroots/wlroots)
- [Android NDK](https://developer.android.com/ndk)
- [OpenGL ES](https://www.khronos.org/opengles/)
