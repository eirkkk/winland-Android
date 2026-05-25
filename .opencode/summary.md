## Goal
- Migrate Winland from manual surface tracking to smithay's `Space<WindowElement>` + `PopupManager` architecture, with HiDPI support, output registration, and zero warnings

## Constraints & Preferences
- Layer-shell and InputMethod-popup surfaces stay in a separate `unmanaged_surfaces: Vec<WlSurface>` outside the Space
- `HashMap<WlSurface, Window>` for reverse lookup from WlSurface to Window
- Must compile on first pass: `cargo build --target aarch64-linux-android --features smithay_android --lib`
- Keyboard-focus tracking (`focused_surface`) stays as `Option<WlSurface>`
- Popup grab is still tracked manually with `popup_grab_active` / `popup_grab_surface`
- APK must build successfully via `./gradlew assembleDebug`

## Progress
### Done
- Created `WindowElement(pub Window)` newtype implementing `SpaceElement`, `IsAlive`, `PartialEq`/`Eq`
- Rewrote `seat.rs` struct: `space`, `popups`, `wl_to_window`, `unmanaged_surfaces`
- Rewrote `handlers.rs`: `Space` + `PopupManager` integration
- Rewrote `x11.rs`: X11 windows in Space, override-redirect in `unmanaged_surfaces`
- Rewrote `input_router.rs`: focus cycling/`element_under` via Space, gesture relocate
- Rewrote `render_all`: iterates `space.elements()` + `unmanaged_surfaces`
- Output `map_output` in constructor + `update_output_mode` for enter/leave events
- Cleanup: `prune_dead_surfaces` cleans gesture/popup_grab state atomically
- **Space-based minimize/maximize**: `minimized: HashMap<WlSurface, Point>` + `maximize_restore: HashMap<WlSurface, Point>`; uses `space.unmap_elem()`/`space.map_element()` instead of HashSet + render checks
- **HiDPI Scale Factor**: `compute_dpi_scale()` helper (DPI = diagonal_px/diagonal_in/160, clamped 1.0–4.0); `Scale::Fractional(scale)` passed to `output.change_current_state()`; `FractionalScaleHandler::new_fractional_scale()` reads real output scale
- **Warning purge (3 files)**: Rewrote import blocks for `selection.rs`, `server.rs`, `x11.rs` — removed ~200 unused imports, dropping total warnings from ~297 to ~154

### In Progress
- Warning cleanup continues: `seat.rs` (50), `input_router.rs` (50), `handlers.rs` (42), `x11.rs` (7) still have unused imports and deprecation warnings

### Blocked
- Linking of binary targets (`uniffi-bindgen`, `winland-setup`) fails for `aarch64-linux-android` — library-only build with `--lib` is clean
- No Android device or emulator connected for runtime testing

## Key Decisions
- `WindowElement::bbox()` delegates to `Window::bbox()`; input-region check delegates to `Window::surface_under(point, WindowSurfaceType::ALL)`
- X11 override-redirect windows go to `unmanaged_surfaces`, not Space
- Output mapped at `(0,0)` in both constructor and `update_output_mode()`
- Cleanup is done atomically in destruction handlers
- **Minimize via Space unmap**: `space.unmap_elem()` + `space.map_element()` instead of HashSet
- **Maximize state as typed Point**: `Point<i32, Logical>` instead of `(f32,f32,f32,f32)` tuple
- **DPI-based scale**: `(diagonal_px / diagonal_in / 160).round().clamp(1.0, 4.0)`
- **Manual warning purge**: `cargo fix` doesn't apply with cross-compilation target; rewriting import blocks manually
- `Rectangle::from_loc_and_size` is deprecated — use `Rectangle::new(loc.into(), size.into())` instead

## Next Steps
1. Clean remaining ~154 unused-import warnings in `seat.rs`, `handlers.rs`, `input_router.rs`, `x11.rs`
2. Fix `Rectangle::from_loc_and_size` → `Rectangle::new` deprecation across all files
3. Deploy APK on Android device for runtime testing (windows, popups, input, enter/leave, minimize, maximize, HiDPI)

## Critical Context
- Smithay 0.5 vendored at `native/lib/smithay` with `desktop`, `wayland_frontend`, `xwayland`, `renderer_glow` features
- `cargo build --target aarch64-linux-android --features smithay_android --lib` is the clean library-only build command
- Release build with `--release` takes ~8 min (LTO disabled) vs debug ~2.5 min
- APK build: `./gradlew assembleDebug` with `ANDROID_HOME=/opt/android/sdk ANDROID_NDK=/opt/android/ndk`
- `/sdcard` accessible locally (environment runs inside Android/Termux)

## Relevant Files
- `shell.rs`: `WindowElement` newtype
- `seat.rs`: ~793 lines, Space+PopupManager+HiDPI+minimize/maximize
- `handlers.rs`: ~668 lines, Space handler migration
- `x11.rs`: ~280 lines, X11 handler + Space + minimize/maximize cleanup
- `input_router.rs`: ~919 lines, Space-based focus/gesture routing
- `selection.rs`: ~171 lines, minimal imports (3 file-level imports)
- `server.rs`: ~418 lines, minimal imports (11 file-level imports)
- `compositor.rs`: status line with `windows=`/`unmanaged=`
