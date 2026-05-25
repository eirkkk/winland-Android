# Fix Rendering & Window Management

## Change A: Client-Side Decorations
**File**: `native/src/android/backend/wayland/handlers.rs:458-483`

Replace all three `XdgDecorationHandler` methods:

```rust
impl XdgDecorationHandler for AndroidSeatRuntime {
    fn new_decoration(&mut self, toplevel: ToplevelSurface) {
        toplevel.with_pending_state(|state| {
            state.decoration_mode = Some(Mode::ClientSide);
        });
        toplevel.send_configure();
    }
    fn request_mode(&mut self, toplevel: ToplevelSurface, _mode: Mode) {
        toplevel.with_pending_state(|state| {
            state.decoration_mode = Some(Mode::ClientSide);
        });
        toplevel.send_configure();
    }
    fn unset_mode(&mut self, toplevel: ToplevelSurface) {
        toplevel.with_pending_state(|state| {
            state.decoration_mode = Some(Mode::ClientSide);
        });
        toplevel.send_configure();
    }
}
```

## Change B: Don't force Maximized on new toplevels
**File**: `native/src/android/backend/wayland/handlers.rs:316-345`

In `new_toplevel`: Remove `state.states.set(xdg_toplevel::State::Maximized)` and `state.size`/`state.bounds`. Keep only `Activated`:

```rust
fn new_toplevel(&mut self, surface: ToplevelSurface) {
    surface.with_pending_state(|state| {
        state.states.set(xdg_toplevel::State::Activated);
    });
    surface.send_configure();
    // ... rest unchanged
}
```

**File**: `native/src/android/backend/wayland/handlers.rs:376-385`

In `unmaximize_request`: Actually clear Maximized state instead of re-setting it:

```rust
fn unmaximize_request(&mut self, surface: ToplevelSurface) {
    surface.with_pending_state(|state| {
        state.states.unset(xdg_toplevel::State::Maximized);
        state.size = None;
    });
    surface.send_configure();
}
```

## Change C: Fix color swizzle in direct framebuffer path
**File**: `native/src/android/backend/smithay_backend.rs:74-93`

Replace the row copy loop with an R/B-swapped version:

```rust
for y in 0..copy_height {
    let src_row = y * width;
    let dst_y = start_y + y;
    let dst_row_offset = (dst_y * stride_in_bytes + start_x * bytes_per_pixel) as usize;

    for x in 0..copy_width {
        let si = (src_row + x) as usize * 4;
        let di = dst_row_offset + x as usize * 4;
        // Wayland SHM ARGB8888 = BGRA in memory: [B, G, R, A]
        // ANativeWindow expects RGBA_8888:        [R, G, B, A]
        *dst_ptr.add(di + 0) = src_ptr[si + 2]; // R ← B
        *dst_ptr.add(di + 1) = src_ptr[si + 1]; // G ← G
        *dst_ptr.add(di + 2) = src_ptr[si + 0]; // B ← R
        *dst_ptr.add(di + 3) = src_ptr[si + 3]; // A ← A
    }
}
```

## Change D+E: Multi-surface compositing (single GLES pass)

### D1. Add `composite_multi()` to `smithay_backend.rs`

New function that renders multiple surfaces in one GLES pass:

```rust
pub fn composite_multi(surfaces: &[(Vec<u8>, i32, i32, i32, i32)]) {
    let mut guard = match state().lock() {
        Ok(g) => g,
        Err(_) => return,
    };

    let (display, surface, context) = match (guard.egl_display, guard.egl_surface, guard.egl_context) {
        (Some(d), Some(s), Some(c)) => (d, s, c),
        _ => return,
    };

    unsafe {
        if eglMakeCurrent(display, surface, surface, context) == egl::FALSE {
            return;
        }

        let sw = guard.surface_size.0 as f32;
        let sh = guard.surface_size.1 as f32;
        if sw <= 0.0 || sh <= 0.0 { return; }

        gl::Viewport(0, 0, sw as i32, sh as i32);
        gl::ClearColor(0.12, 0.12, 0.18, 1.0);
        gl::Clear(gl::COLOR_BUFFER_BIT);
        gl::Enable(gl::BLEND);
        gl::BlendFunc(gl::SRC_ALPHA, gl::ONE_MINUS_SRC_ALPHA);
        gl::PixelStorei(gl::UNPACK_ALIGNMENT, 1);
    }

    // Use existing shader/texture from guard
    // For each surface, upload and draw
    for (pixels, x, y, w, h) in surfaces {
        // ... existing per-surface GL texture upload + quad draw code
    }

    unsafe {
        eglSwapBuffers(display, surface);
        let _ = eglMakeCurrent(display, egl::NO_SURFACE, egl::NO_SURFACE, egl::NO_CONTEXT);
        guard.frames_presented += 1;
    }
}
```

### D2. Modify `render_all()` in `seat.rs`

Change to collect all surfaces and call `composite_multi()` once:

```rust
pub(crate) fn render_all(&mut self) {
    if !engine_timing::is_rendering_active() {
        return;
    }

    let mut surfaces_data: Vec<(Vec<u8>, i32, i32, i32, i32)> = Vec::new();

    // Collect active surfaces
    for s in &self.surfaces {
        // ... existing buffer extraction logic ...
    }

    // Composite all surfaces in a single pass
    if !surfaces_data.is_empty() {
        crate::android::backend::smithay_backend::composite_multi(&surfaces_data);
    }
}
```

## Verification
1. `cargo build --release --target aarch64-linux-android --features smithay_android --jobs 4`
2. `cp target/aarch64-linux-android/release/libuniffi_winland_core.so ../app/src/main/jniLibs/arm64-v8a/`
3. `cd .. && ./gradlew assembleDebug`
4. `cp app/build/outputs/apk/debug/app-debug.apk /sdcard/winland.apk`
