# Contributing to Winland Server

Thank you for your interest in contributing to Winland Server! This document provides guidelines and instructions for contributing.

## 🎯 How to Contribute

### Reporting Bugs

Before creating a bug report, please:
1. Check if the issue already exists
2. Use the latest version
3. Collect relevant logs

**Bug Report Template:**
```markdown
**Description:**
Clear description of the bug

**Steps to Reproduce:**
1. Step one
2. Step two
3. ...

**Expected Behavior:**
What should happen

**Actual Behavior:**
What actually happens

**Environment:**
- Device: [e.g., Pixel 7]
- Android Version: [e.g., Android 13]
- Winland Version: [e.g., 1.0.0]
- Root: [Yes/No, Magisk/KernelSU version]

**Logs:**
```
Paste relevant logs here
```
```

### Suggesting Features

We welcome feature suggestions! Please:
1. Check if the feature was already suggested
2. Explain the use case
3. Describe the expected behavior

### Pull Requests

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Make your changes
4. Commit with clear messages
5. Push to your fork
6. Open a Pull Request

## 🏗️ Development Setup

### Prerequisites

- Android Studio Arctic Fox (2020.3.1) or newer
- Android NDK r25c or newer
- CMake 3.22+
- Python 3.8+
- Git

### Building

```bash
# Clone repository
git clone https://github.com/yourusername/winland-server.git
cd winland-server

# Setup environment
bash scripts/setup-dev.sh

# Build everything
bash scripts/build-automation.sh all

# Or use Android Studio
# Open the project in Android Studio and click "Build"
```

### Project Structure

```
winland-server/
├── app/                    # Android application
│   ├── src/main/
│   │   ├── cpp/           # JNI and native code
│   │   ├── java/          # Java source files
│   │   └── res/           # Android resources
│   └── build.gradle       # App build configuration
├── src/                   # Native source code
│   ├── native/            # Core Wayland compositor
│   │   ├── wayland_compositor.c
│   │   ├── egl_display.c
│   │   ├── input_handler.c
│   │   └── ...
│   ├── bridge/            # Bridges (audio, USB, clipboard)
│   │   ├── audio_bridge.c
│   │   ├── usb_redirect.c
│   │   └── clipboard_bridge.c
│   └── protocols/         # Wayland protocol definitions
├── scripts/               # Build and utility scripts
├── docs/                  # Documentation
└── tests/                 # Unit tests
```

## 📝 Code Style

### C/C++ Code

- Follow Linux kernel coding style
- Use 4 spaces for indentation
- Maximum line length: 100 characters
- Use meaningful variable names

```c
// Good
int surface_attach_buffer(struct wl_surface *surface, 
                          struct wl_buffer *buffer)
{
    if (!surface || !buffer) {
        return -EINVAL;
    }
    
    surface->buffer = buffer;
    return 0;
}

// Bad
int attach(struct wl_surface*s,struct wl_buffer*b){
if(!s||!b)return -1;
s->buffer=b;
return 0;
}
```

### Java Code

- Follow Android coding conventions
- Use 4 spaces for indentation
- Class names: PascalCase
- Method names: camelCase
- Constants: UPPER_SNAKE_CASE

```java
// Good
public class WinlandService extends Service {
    private static final String TAG = "WinlandService";
    
    public void startServer() {
        // Implementation
    }
}

// Bad
public class winlandservice extends Service{
private static final String tag="WinlandService";
public void StartServer(){
// Implementation
}
}
```

## 🧪 Testing

### Unit Tests

```bash
# Run native tests
cd build
ctest --output-on-failure

# Run Android tests
./gradlew test
```

### Integration Tests

```bash
# Start the server
adb shell am start -n com.winland.server/.MainActivity

# Run integration tests
python3 tests/integration_test.py
```

### Manual Testing Checklist

- [ ] Server starts without errors
- [ ] Wayland clients can connect
- [ ] Input (touch, keyboard) works
- [ ] Audio playback works
- [ ] Window management works
- [ ] Tiling layouts work
- [ ] VNC server works
- [ ] Debug overlay displays correctly

## 📚 Documentation

- Update README.md for user-facing changes
- Update docs/ for technical changes
- Add inline comments for complex code
- Update CHANGELOG.md

## 🔒 Security

### Reporting Security Issues

Please report security vulnerabilities privately:
- Email: security@winland-server.dev
- Do NOT open public issues

### Security Best Practices

- Validate all inputs
- Use safe string functions
- Check buffer bounds
- Don't log sensitive information
- Use least privilege principle

## 🏷️ Commit Messages

Use conventional commits format:

```
<type>(<scope>): <subject>

<body>

<footer>
```

Types:
- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation changes
- `style`: Code style changes
- `refactor`: Code refactoring
- `test`: Test changes
- `chore`: Build/dependency changes

Examples:
```
feat(tiling): add spiral layout

Implement Fibonacci spiral tiling layout
similar to i3's spiral mode.

Closes #123
```

```
fix(egl): fix buffer swap on Mali GPUs

Workaround for Mali driver bug that caused
flickering during buffer swaps.

Fixes #456
```

## 🎨 Design Guidelines

### UI/UX

- Follow Material Design guidelines
- Support dark mode
- Ensure accessibility
- Test on different screen sizes

### Icons

- Use vector drawables when possible
- Provide all density versions
- Follow Android icon guidelines

## 📦 Release Process

1. Update version in:
   - `app/build.gradle`
   - `CMakeLists.txt`
   - `README.md`

2. Update `CHANGELOG.md`

3. Create release branch:
   ```bash
   git checkout -b release/v1.0.0
   ```

4. Build release APK:
   ```bash
   ./gradlew assembleRelease
   ```

5. Tag and release:
   ```bash
   git tag -a v1.0.0 -m "Release version 1.0.0"
   git push origin v1.0.0
   ```

## 💬 Communication

- Be respectful and constructive
- Provide context in discussions
- Ask questions if unclear
- Help others in the community

## 📄 License

By contributing, you agree that your contributions will be licensed under the MIT License.

## 🙏 Thank You!

Every contribution, no matter how small, helps make Winland Server better. Thank you for your support!
