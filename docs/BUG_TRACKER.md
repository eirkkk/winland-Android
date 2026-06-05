# Bug Tracker — Winland Compositor

## Legend
- 🔴 **Critical** — causes crash, hang, or data loss
- 🟠 **Major** — breaks a feature or makes it unusable
- 🟡 **Minor** — cosmetic, edge-case, or rare
- 🔵 **Enhancement** — not a bug but missing functionality
- ⚪ **Fixed** — resolved, kept for reference

---

## 1. Rendering

| # | Severity | Title | Area | Status |
|---|----------|-------|------|--------|
| R1 | 🔴 | `unsafe { std::slice::from_raw_parts(ptr, len) }` in SHM read loop — no bounds check across 4 call sites | `seat.rs:917,988,1029,1138` | ⚪ Fixed |
| R2 | 🟠 | Non-SHM buffers silently skipped (EGL buffers) — no fallback path | `seat.rs:912` | Open |
| R3 | 🟠 | No damage tracking: entire output composited every frame even if nothing changed | `seat.rs:871` | Open |
| R4 | 🟠 | GLES program compiled on every `bind_native_window` call — no reuse across surface recreations | `smithay_backend.rs:28-51` | Open |
| R5 | 🟡 | `surface_scale` applied to NDC placement but texture is already uploaded at full buffer resolution — possible pixel mismatch | `smithay_backend.rs:97-100` | Open |
| R6 | 🟡 | XWayland SHM buffer alpha forced to 255 via pixel loop every frame — works but wasteful, could use `glBlendFunc(GL_ONE, GL_ZERO)` per-texture instead | `seat.rs:954-958` | ⚪ Fixed |
| R7 | 🟡 | No partial-update / dirty-region optimization — full buffer copy on every frame | `seat.rs:924-935` | Open |

---

## 2. Wayland Protocol

| # | Severity | Title | Area | Status |
|---|----------|-------|------|--------|
| W1 | 🟠 | `lock().unwrap()` on `XdgToplevelSurfaceData` — will panic on poisoned mutex | `handlers.rs:383,544,559` | ⚪ Fixed |
| W2 | 🟠 | `WaylandClientState` might not be set when first client handler fires — silent skip | `handlers.rs:236` | ⚪ Fixed |
| W3 | 🟡 | Selection/Clipboard uses raw `from_raw_fd` with no ownership tracking — FD leak risk | `selection.rs:56,105` | Open |
| W4 | 🟡 | `SERIAL_COUNTER.next_serial()` called ad-hoc without protocol-phase ordering guarantees | multiple | Open |
| W5 | 🟡 | Popup dismissal uses `element_under(point)` — may misfire on overlapping popups | `input_router.rs:204` | Open |
| W6 | 🔵 | No `wl_data_device_manager` drag-and-drop protocol handling | Open |
| W7 | 🔵 | No `zwp_relative_pointer_manager` — needed for camera/3D viewport look | Open |
| W8 | 🔵 | No `zwp_pointer_constraints` locking — only read via `with_pointer_constraint` in Mouse mode | `input_router.rs:908-910` | Open |

---

## 3. XWayland

| # | Severity | Title | Area | Status |
|---|----------|-------|------|--------|
| X0 | 🟠 | X11→Wayland clipboard bridge broken: `send_selection` reads instead of writes, `new_selection` not implemented | `x11.rs:453-490` | ⚪ Fixed |
| X1 | 🔴 | XWayland surface rendered as ARGB with alpha=0 → transparent through GLES blend | `seat.rs:954` ✅ fix | ⚪ Fixed |
| X2 | 🟠 | `x11.configure()` sent during move/resize deltas causes double-render (GIMP commits stale buffer) | `input_router.rs:308-315` ✅ removed | ⚪ Fixed |
| X3 | 🟠 | `X11Wm::start_wm` failure kills XWayland but runtime continues — zombie state | `server.rs:271` | Open |
| X4 | 🟠 | XWayland socket retry logic (`retries < 5`) can race with slow X11 server startup | `server.rs:245-258` | Open |
| X5 | 🟡 | `set_hidden(true/false)` called on X11 surface but no visual indicator of minimized state | `seat.rs:684-686` | Open |
| X6 | 🟡 | No `xwm_id` tracking across multiple XWayland instances — assumes single x11_wm | `x11.rs:55-58` | Open |
| X7 | 🟡 | `unmapped_window` drops gesture state but doesn't verify gesture_surface matches | `x11.rs:144-146` | Open |
| X8 | 🟡 | `configure_request` handler may fight with compositor's move/resize (dual-position authority) | `x11.rs` | ⚪ Fixed |
| X9 | 🔵 | No `_NET_WM_STATE_FULLSCREEN` → fullscreen X11 apps not supported | x11.rs | ⚪ Fixed |
| X10 | 🔵 | No `_NET_WM_WINDOW_TYPE_DIALOG` → dialogs not centered/pinned | x11.rs | ⚪ Fixed |

---

## 4. Touch Input

| # | Severity | Title | Area | Status |
|---|----------|-------|------|--------|
| T1 | 🟠 | Gesture detection uses `self.focused_surface` which is `None` on first touch → gesture missed | `input_router.rs:220` ✅ fixed | ⚪ Fixed |
| T2 | 🟠 | Titlebar touch zone (24px above window) not visible — user has no way to discover window operations | `input_router.rs:231` | Open |
| T3 | 🟡 | Multi-touch: only tracks centroids, individual finger tracking lost after gesture starts | `input.rs:44-46` | Open |
| T4 | 🟡 | `primary_touch_id` set but never cleared on gesture timeout/cancel | `input_router.rs` | Open |
| T5 | 🟡 | Close (8-18px), Minimize (24-34px), Maximize (40-50px) zones are hardcoded pixel values — not DPI-aware | `input_router.rs:232-248` | Open |
| T6 | 🔵 | No two-finger right-click emulation | Open |
| T7 | 🔵 | No long-press right-click emulation (only long-press drag in trackpad mode) | Open |

---

## 5. Trackpad

| # | Severity | Title | Area | Status |
|---|----------|-------|------|--------|
| P1 | 🟠 | Trackpad mode receives ABSOLUTE touch coordinates but interprets them as RELATIVE — extrapolation mismatch | `input_router.rs:394-420` | Open |
| P2 | 🟠 | Long-press drag (350ms threshold) sends `ButtonState::Pressed` but misses release on finger lift | `input_router.rs:431-445` | Open |
| P3 | 🟡 | Trackpad move uses `abs(raw_dx) < 0.5` noise gate — may reject slow/fine cursor movements | `input_router.rs:421-423` | Open |
| P4 | 🟡 | Trackpad tap detection tracks finger count but never actually sends click on tap (only on long press) | `input_router.rs` | Open |
| P5 | 🟡 | Sensitivity applied linearly without acceleration curve — feels sluggish at low speed, jerky at high speed | `input_router.rs` | Open |
| P6 | 🔵 | No two-finger scroll in trackpad mode (only multi-touch gesture scroll in Touch mode) | `input_router.rs` | Open |

---

## 6. Mouse Pointers

| # | Severity | Title | Area | Status |
|---|----------|-------|------|--------|
| M1 | 🟠 | Dual cursor: Wayland software cursor + XWayland cursor both rendered simultaneously | `seat.rs:1079-1161` ✅ skip on XWayland focus | ⚪ Fixed |
| M2 | 🟡 | Cursor hotspot is read from `CursorImageSurfaceData` but may be (0,0) for clients that don't set it | `seat.rs:1106-1107` | Open |
| M3 | 🟡 | Surface cursor rendered even when cursor image is `Hidden` — `Hidden` only skips named fallback | `seat.rs:1088` | Open |
| M4 | 🟡 | Fallback cursor (16×16 bitmap arrow) is hardcoded — no theme support | `seat.rs:1184-1222` | Open |
| M5 | 🔵 | No `wl_cursor_shape` / `cursor-shape-v1` protocol support — always renders named as fallback | Open |
| M6 | 🔵 | No hidpi/scale-aware cursor rendering — surface cursor rendered at native resolution, may appear small | `seat.rs` | Open |

---

## 7. Windows

| # | Severity | Title | Area | Status |
|---|----------|-------|------|--------|
| N1 | 🟡 | `WindowElement::wl_surface()` returns `Cow<WlSurface>` — the cloned owned surface may mismatch after remap | `shell.rs:28-62` | Open |
| N2 | 🟡 | `wl_to_window` map holds `Window` clones; `Window` contains `Arc<WindowInner>` but `X11Surface` inside may become stale | `x11.rs:119` | Open |
| N3 | 🟡 | Unmanaged surfaces rendered at fixed `(0, reserved_top)` — tooltips/popups may appear at wrong position | `seat.rs:1061` | Open |
| N4 | 🔵 | No `wl_subsurface` handling for XDG popups with parent-relative positioning (only absolute via PopupManager) | `seat.rs:966-1003` | Open |

---

## 8. Window Management

| # | Severity | Title | Area | Status |
|---|----------|-------|------|--------|
| WM1 | 🟠 | `x11.configure()` on gesture completion (our fix) may race with client's own ConfigureRequest — position reset | `input_router.rs:376-390` | ⚪ Fixed |
| WM2 | 🟡 | `set_activate(true/false)` called but no visual active/inactive state rendered (e.g., titlebar color change) | `shell.rs:44-46` | Open |
| WM3 | 🟡 | Focus candidate selection in `unmapped_window` may pick wrong window when multiple remain | `x11.rs:150` | Open |
| WM4 | 🟡 | Maximize restores to saved position but does not verify position is still on-screen | `seat.rs:703-704` | Open |
| WM5 | 🟡 | Minimize only tracks position; no animation/transition → window disappears instantly | `seat.rs:669-694` | Open |
| WM6 | 🟡 | Resize edge detection (20px) may overlap with titlebar zone (24px) at window corners | `input_router.rs:257-258` | Open |
| WM7 | 🔵 | No keyboard shortcut support (Alt+F4 close, Alt+Tab switch, etc.) | Open |
| WM8 | 🔵 | No snap/tiling (Win+Left, Win+Right) | Open |
| WM9 | 🔵 | No window menu / right-click titlebar context menu | Open |

---

## 9. Performance & Stability

| # | Severity | Title | Area | Status |
|---|----------|-------|------|--------|
| S1 | 🔴 | Full buffer copy (`Vec::extend_from_slice`) for every surface every frame — O(W×H×N) memcpy per frame | `seat.rs:929` | Open |
| S2 | 🟠 | No frame scheduling — `render_all` called eagerly on every input event, may over-render | `input_router.rs:367` | Open |
| S3 | 🟡 | `eglMakeCurrent` called on every `composite_multi` and immediately unbound — expensive context thrash | `smithay_backend.rs:74,128-129` | Open |
| S4 | 🟡 | `render_sender` channel unbounded — if compositor thread falls behind, memory grows without bound | `seat.rs:242` | Open |

---

## 10. Window Operations — Gesture Zone Layout

Reference for the titlebar touch zones in `dispatch_touch_down`:

```
x: fx──fx+8──fx+18──fx+24──fx+34──fx+40──fx+50──────────fx+fw
y: fy-24 ┌─────────────────────────────────────────────────┐
         │ [C]  │ [─]  │ [□]  │  (drag zone)              │  ← touch zone
y: fy    └─────────────────────────────────────────────────┘
         │           X11 window content                    │
y: fy+fh └─────────────────────────────────────────────────┘

[C] = Close (8-18px)
[─] = Minimize (24-34px)  
[□] = Maximize (40-50px)
Drag zone = any touch in y∈[fy-24, fy] outside button zones
Resize zone = 20px from any edge (bottom-left, bottom-right, bottom, left, right)
```

All values are hardcoded pixels — not DPI/scale-aware. The zone is entirely invisible (no rendered titlebar).
