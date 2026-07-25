# Troubleshooting Guide

## Common User Errors & Solutions

### Git & Version Control

| Error | Root Cause | Solution |
|-------|-----------|----------|
| `src refspec main matches more than one` | Both a tag and branch named `main` exist | Use full ref: `git push origin refs/heads/main` or delete the conflicting tag: `git tag -d main` |
| `Updates were rejected because the remote contains work...` | Remote has commits not in local history (diverged) | `git pull --rebase` then push again, or `git push --force` if you intentionally rewrote history |
| `error: could not apply ... CONFLICT (content)` | Merge conflict during rebase | Resolve conflict markers (`<<<<<<<`, `=======`, `>>>>>>>`), `git add <file>`, then `git rebase --continue` |
| `Permission denied (publickey)` | SSH key not added to GitHub account | Add public key via `GitHub → Settings → SSH and GPG keys`, or switch to HTTPS |
| `The requested URL returned error: 403` | Token lacks write permissions | Update token scopes: add `Contents: write` (or entire `repo` scope) in `GitHub → Settings → Developer settings → Personal access tokens` |
| `fatal: could not read Username for 'https://github.com'` | No TTY for interactive login | Use `gh auth login` (pick GitHub.com) or embed token in remote URL |
| `gh auth login` tries GitHub Enterprise Server | Wrong account type selected | Use arrow keys to select **GitHub.com** instead of GitHub Enterprise Server |
| `src refspec main matches more than one` on push --force | Same as above — tag/branch name conflict | Specify `refs/heads/main:refs/heads/main` |
| Rebase stuck with `error: could not apply` | Conflict in a commit during rebase | `git status` → resolve conflicts → `git add` → `git rebase --continue`. Use `GIT_EDITOR=true git rebase --continue` if no editor available |

### Bluetooth Mouse

| Error | Root Cause | Solution |
|-------|-----------|----------|
| Cursor freezes after first BT mouse click | `return` inside `thread::spawn` kills the compositor thread | Fixed in code — ensure `continue` is used instead of `return` in the event loop |
| Right-click triggers left-click | No button state check in `onTouchEvent` | Fixed — `sendMouseClick` now passes button type (`0x110` left, `0x111` right) |
| Coordinate jump in landscape mode | Raw coordinates used for hover but offset-subtracted for click | Fixed — both hover and click now apply the same `x_offset`/`y_offset` |
| Android pointer reappears after rotation | System resets `pointerIcon` on surface change | Fixed — `reapplyPointerIcon()` called in `surfaceChanged` callback |
| Crash on re-entry after stopping compositor | `OnceLock` prevents resetting `COMMAND_TX` | Fixed — replaced with `Mutex<Option<Sender>>` |

### Display & Rendering

| Error | Root Cause | Solution |
|-------|-----------|----------|
| Black screen | EGL init failure / Surface not bound | Restart the compositor. Check `logcat` for EGL errors |
| Low FPS | Missing GPU acceleration | Verify OpenGL ES / Vulkan support on device. Enable Zink: `MESA_LOADER_DRIVER_OVERRIDE=zink` |
| Screen flickers on rotation | Surface destroyed and recreated | This is expected — the compositor handles `surfaceChanged` with new dimensions |
| Double cursor on first entry | `OnSharedPreferenceChangeListener` only fires on *changes*, not initial value | Fixed — immediate `input_mode_mask` read in `init` block hides pointer on startup |

### Input Modes

| Error | Root Cause | Solution |
|-------|-----------|----------|
| Touch not working | Wrong input mode selected | Switch to **Touch** mode in Settings tab |
| Trackpad cursor jumps | Relative motion misconfigured | Switch mode between Trackpad and Touch to recalibrate |
| Mouse pointer doesn't move | Android system pointer obscuring compositor cursor | Enable **Mouse** mode (setting `input_mode_mask = 4`). In Mouse mode the Android pointer is hidden |

### Clipboard

| Error | Root Cause | Solution |
|-------|-----------|----------|
| Copy/paste not working between Android and Linux | XWayland clipboard bridge not connected | Ensure Wayland clipboard sync is initialized — check `logcat` for clipboard events |
| Paste pastes old content | Clipboard not refreshed after new copy | Clipboard sync is automatic; restart the compositor if stale |

### Build & Installation

| Error | Root Cause | Solution |
|-------|-----------|----------|
| `BUILD FAILED` with Rust errors | Missing `aarch64-linux-android` target or NDK | Run `rustup target add aarch64-linux-android`, ensure NDK path is set in `.cargo/config.toml` |
| APK install fails with `INSTALL_FAILED_UPDATE_INCOMPATIBLE` | Existing install with different signature | `adb uninstall com.winland.server` then reinstall |
| `Failed to mount` chroot directories | Root not granted or missing busybox | Ensure root permission granted via Magisk/KernelSU. Check `logcat` for mount errors |
| Libc++ shared library not found | Missing C++ runtime in APK | Ensure `jniLibs` contains all required `.so` files |

### Audio

| Error | Root Cause | Solution |
|-------|-----------|----------|
| No audio from Linux apps | PulseAudio not connected to FIFO | Check `pulseaudio -k && pulseaudio --start` inside chroot. Verify FIFO pipe exists at configured path |
| Microphone not detected by Linux apps | Oboe mic FIFO not started | Ensure mic input is enabled in Android settings before starting the compositor |
| Audio crackling or stuttering | Buffer underrun | Increase audio buffer size in Oboe configuration |

### Chroot & Distro

| Error | Root Cause | Solution |
|-------|-----------|----------|
| Distro install stuck at "Downloading rootfs" | Network issue or slow connection | Check internet connection. The rootfs is ~1-2 GB, may take time |
| `chroot: cannot run command: No such file or directory` | Missing architecture compatibility | Ensure rootfs is ARM64. x86/x86_64 rootfs won't work on ARM64 devices |
| `exec format error` in chroot | Binary architecture mismatch | Same as above — verify `uname -m` inside chroot returns `aarch64` |
| LabWC fails to start | Missing compositor dependencies | Run `setup_ubuntu.sh` again to install required packages (labwc, wlroots, etc.) |

---

## Debug Commands

```bash
# Winland-specific logs
adb logcat -s WinlandServer NativeBridge SmithayRuntime Compositor

# Full log dump
adb logcat -d > winland-logs.txt

# Check GPU capabilities
adb shell dumpsys meminfo com.winland.server

# Memory info
adb shell dumpsys meminfo com.winland.server
```

---

## Reporting a Bug

Open an issue at [https://github.com/eirkkk/winland-Android/issues](https://github.com/eirkkk/winland-Android/issues) with:
1. Device model and Android version
2. Full `adb logcat -s WinlandServer NativeBridge` output
3. Steps to reproduce
4. Expected vs actual behavior
