#[cfg(feature = "smithay_android")]
use std::io::Read;
#[cfg(feature = "smithay_android")]
use smithay::desktop::Window;
#[cfg(feature = "smithay_android")]
use smithay::output::Mode as OutputMode;
#[cfg(feature = "smithay_android")]
use smithay::utils::SERIAL_COUNTER;
#[cfg(feature = "smithay_android")]
use smithay::wayland::xwayland_shell::{XWaylandShellHandler, XWaylandShellState};
#[cfg(feature = "smithay_android")]
use smithay::xwayland::X11Wm;
#[cfg(feature = "smithay_android")]
use smithay::xwayland::XwmHandler;
#[cfg(feature = "smithay_android")]
use smithay::xwayland::xwm::{XwmId, ResizeEdge, Reorder, X11Surface, WmWindowType};
#[cfg(feature = "smithay_android")]
use crate::android::backend::wayland::seat::{AndroidSeatRuntime, GestureTarget};
#[cfg(feature = "smithay_android")]
use crate::android::backend::wayland::server::WaylandClientState;
#[cfg(feature = "smithay_android")]
use crate::android::backend::wayland::shell::WindowElement;

#[cfg(feature = "smithay_android")]
impl XWaylandShellHandler for AndroidSeatRuntime {
    fn xwayland_shell_state(&mut self) -> &mut XWaylandShellState {
        &mut self.xwayland_shell_state
    }

    fn surface_associated(
        &mut self,
        _xwm: XwmId,
        wl_surface: smithay::reexports::wayland_server::protocol::wl_surface::WlSurface,
        _window: X11Surface,
    ) {
        use smithay::reexports::wayland_server::Resource;
        if self.xwayland_client.is_none() {
            if let Some(client) = wl_surface.client() {
                self.xwayland_client = Some(client.clone());

                if let Some(data) = client.get_data::<WaylandClientState>() {
                    data.compositor_state.set_client_scale(1.0);
                    log::info!("XWayland: set client_scale=1.0");
                } else {
                    log::warn!("XWayland: WaylandClientState not found on XWayland client, scale stays default");
                }
            }
        }
        log::debug!("XWayland: X11 surface associated with wl_surface");
    }
}

#[cfg(feature = "smithay_android")]
impl XwmHandler for AndroidSeatRuntime {
    fn xwm_state(&mut self, _xwm: XwmId) -> &mut X11Wm {
        self.x11_wm.as_mut().expect(
            "xwm_state() called before x11_wm was initialized — call activate_passive_xwayland() first"
        )
    }

    fn new_window(&mut self, _xwm: XwmId, window: X11Surface) {
        log::debug!("XWayland: new_window id={}", window.window_id());
        let (sw, sh) = self.usable_screen_size();
        let default_w = (sw as f32 * 0.8) as i32;
        let default_h = (sh as f32 * 0.8) as i32;
        let offset_count = self.wl_to_window.len() as i32;
        let cascade = 30;
        let base_x = (sw - default_w) / 2;
        let base_y = self.reserved_top + 40;
        let fx = base_x + (offset_count % 10) * cascade;
        let fy = base_y + (offset_count % 10) * cascade;
        let rect = smithay::utils::Rectangle::new(
            (fx.max(0), fy.max(self.reserved_top)).into(),
            (default_w.max(100), default_h.max(100)).into(),
        );
        let _ = window.configure(rect);
    }

    fn new_override_redirect_window(&mut self, _xwm: XwmId, window: X11Surface) {
        if let Some(wl) = window.wl_surface() {
            if !self.unmanaged_surfaces.contains(&wl) {
                self.unmanaged_surfaces.push(wl);
            }
        }
        log::debug!("XWayland: new_override_redirect_window id={}", window.window_id());
    }

    fn map_window_request(&mut self, _xwm: XwmId, window: X11Surface) {
        let wl = window.wl_surface();
        let window_id = window.window_id();
        let _ = window.set_mapped(true);
        let is_dialog = matches!(window.window_type(), Some(WmWindowType::Dialog));
        if is_dialog {
            log::info!("XWayland: map_window_request id={} type=DIALOG — centered", window_id);
        }
        log::info!("XWayland: map_window_request granted id={}", window_id);

        if let Some(wl) = wl {
            if !self.wl_to_window.contains_key(&wl) {
                let (sw, sh) = self.usable_screen_size();
                let geometry = window.geometry();
                let (rect, pos) = if is_dialog {
                    let dw = geometry.size.w.max(100).min(sw);
                    let dh = geometry.size.h.max(100).min(sh);
                    let fx = (sw - dw) / 2;
                    let fy = self.reserved_top + ((sh - self.reserved_top - dh) / 2).max(0);
                    let rect = smithay::utils::Rectangle::new(
                        (fx.max(0), fy.max(self.reserved_top)).into(),
                        (dw, dh).into(),
                    );
                    (rect, (fx.max(0), fy.max(self.reserved_top)))
                } else if geometry.size.w == 0 || geometry.size.h == 0 {
                    let default_w = (sw as f32 * 0.8) as i32;
                    let default_h = (sh as f32 * 0.8) as i32;
                    let offset_count = self.wl_to_window.len() as i32;
                    let cascade = 30;
                    let base_x = (sw - default_w) / 2;
                    let base_y = self.reserved_top + 40;
                    let fx = base_x + (offset_count % 10) * cascade;
                    let fy = base_y + (offset_count % 10) * cascade;
                    let rect = smithay::utils::Rectangle::new(
                        (fx.max(0), fy.max(self.reserved_top)).into(),
                        (default_w.max(100), default_h.max(100)).into(),
                    );
                    (rect, (fx.max(0), fy.max(self.reserved_top)))
                } else {
                    (geometry, (geometry.loc.x, geometry.loc.y))
                };
                let _ = window.configure(rect);
                let x11_window = Window::new_x11_window(window);
                let elem = WindowElement(x11_window.clone());
                self.space.map_element(elem, pos, true);
                self.wl_to_window.insert(wl.clone(), x11_window);
            }
            self.focused_surface = Some(wl.clone());
            if let Some(keyboard) = self.keyboard.clone() {
                keyboard.set_focus(self, Some(wl), SERIAL_COUNTER.next_serial());
            }
        }
    }

    fn mapped_override_redirect_window(&mut self, _xwm: XwmId, window: X11Surface) {
        if let Some(wl) = window.wl_surface() {
            if !self.unmanaged_surfaces.contains(&wl) {
                self.unmanaged_surfaces.push(wl.clone());
            }
        }
    }

    fn unmapped_window(&mut self, _xwm: XwmId, window: X11Surface) {
        if let Some(wl) = window.wl_surface() {
            self.unmanaged_surfaces.retain(|s| s != &wl);
            if let Some(x11_window) = self.wl_to_window.remove(&wl) {
                self.space.unmap_elem(&WindowElement(x11_window));
            }
            self.minimized.remove(&wl);
            self.maximize_restore.remove(&wl);
            if self.gesture_surface.as_ref() == Some(&wl) {
                self.gesture_target = None;
                self.gesture_surface = None;
            }
            if self.focused_surface.as_ref() == Some(&wl) {
                self.focused_surface = None;
                let candidate = self.choose_focus_candidate();
                let _ = self.apply_focus_candidate("xwayland_unmapped", candidate);
            }
        }
        self.popups.cleanup();
        log::debug!("XWayland: unmapped_window id={}", window.window_id());
    }

    fn destroyed_window(&mut self, _xwm: XwmId, window: X11Surface) {
        if let Some(wl) = window.wl_surface() {
            self.unmanaged_surfaces.retain(|s| s != &wl);
            if let Some(x11_window) = self.wl_to_window.remove(&wl) {
                self.space.unmap_elem(&WindowElement(x11_window));
            }
            self.minimized.remove(&wl);
            self.maximize_restore.remove(&wl);
            if self.gesture_surface.as_ref() == Some(&wl) {
                self.gesture_target = None;
                self.gesture_surface = None;
            }
            if self.popup_grab_surface.as_ref() == Some(&wl) {
                self.popup_grab_active = false;
                self.popup_grab_surface = None;
            }
            if self.focused_surface.as_ref() == Some(&wl) {
                self.focused_surface = None;
                let candidate = self.choose_focus_candidate();
                let _ = self.apply_focus_candidate("xwayland_destroyed", candidate);
            }
        }
        self.popups.cleanup();
        log::debug!("XWayland: destroyed_window id={}", window.window_id());
    }

    fn configure_request(
        &mut self,
        _xwm: XwmId,
        window: X11Surface,
        x: Option<i32>,
        y: Option<i32>,
        w: Option<u32>,
        h: Option<u32>,
        _reorder: Option<Reorder>,
    ) {
        let (sw, sh) = self.usable_screen_size();
        let default_w = (sw as f32 * 0.8) as i32;
        let default_h = (sh as f32 * 0.8) as i32;
        let (fw, fh) = (
            w.map(|v| v as i32).unwrap_or(default_w),
            h.map(|v| v as i32).unwrap_or(default_h),
        );

        // Compositor is authoritative for position of already-mapped windows.
        // Ignore client-requested x/y; use current compositor position instead.
        let (fx, fy) = if let Some(wl) = window.wl_surface() {
            if let Some(x11_window) = self.wl_to_window.get(&wl) {
                let loc = self.space.element_location(&WindowElement(x11_window.clone()));
                loc.map(|l| (l.x, l.y))
                    .unwrap_or((x.unwrap_or((sw - fw) / 2), y.unwrap_or(self.reserved_top + 40)))
            } else {
                (x.unwrap_or((sw - fw) / 2), y.unwrap_or(self.reserved_top + 40))
            }
        } else {
            (x.unwrap_or((sw - fw) / 2), y.unwrap_or(self.reserved_top + 40))
        };

        let rect = smithay::utils::Rectangle::new(
            (fx.max(0), fy.max(self.reserved_top)).into(),
            (fw.max(100), fh.max(100)).into(),
        );
        let _ = window.configure(rect);
    }

    fn configure_notify(
        &mut self,
        _xwm: XwmId,
        window: X11Surface,
        geometry: smithay::utils::Rectangle<i32, smithay::utils::Logical>,
        _above: Option<smithay::xwayland::xwm::X11Window>,
    ) {
        if let Some(wl) = window.wl_surface() {
            if let Some(x11_window) = self.wl_to_window.get(&wl) {
                self.space.relocate_element(
                    &WindowElement(x11_window.clone()),
                    (geometry.loc.x, geometry.loc.y),
                );
            }
        }
        log::debug!("XWayland: configure_notify id={} geometry={}x{}+{}+{}",
            window.window_id(), geometry.size.w, geometry.size.h,
            geometry.loc.x, geometry.loc.y);
    }

    fn resize_request(
        &mut self,
        _xwm: XwmId,
        window: X11Surface,
        _button: u32,
        resize_edge: ResizeEdge,
    ) {
        if let Some(wl) = window.wl_surface() {
            self.gesture_target = Some(GestureTarget::Resize(resize_edge));
            self.gesture_surface = Some(wl);
        }
        log::debug!("XWayland: resize_request id={} edge={:?}", window.window_id(), resize_edge);
    }

    fn move_request(&mut self, _xwm: XwmId, window: X11Surface, _button: u32) {
        if let Some(wl) = window.wl_surface() {
            self.gesture_target = Some(GestureTarget::Move);
            self.gesture_surface = Some(wl);
        }
        log::debug!("XWayland: move_request id={}", window.window_id());
    }

    fn maximize_request(&mut self, _xwm: XwmId, window: X11Surface) {
        let (w, h) = self.usable_screen_size();
        let rect = smithay::utils::Rectangle::new((0, self.reserved_top).into(), (w, h).into());
        let _ = window.configure(rect);
        let _ = window.set_maximized(true);
    }

    fn unmaximize_request(&mut self, _xwm: XwmId, window: X11Surface) {
        let _ = window.set_maximized(false);
    }

    fn fullscreen_request(&mut self, _xwm: XwmId, window: X11Surface) {
        let mode = self.output.current_mode().unwrap_or(OutputMode {
            size: (1080, 1920).into(),
            refresh: 60000,
        });
        let rect = smithay::utils::Rectangle::new((0, 0).into(), (mode.size.w, mode.size.h).into());
        let _ = window.configure(rect);
        let _ = window.set_fullscreen(true);
    }

    fn unfullscreen_request(&mut self, _xwm: XwmId, window: X11Surface) {
        let _ = window.set_fullscreen(false);
    }

    fn active_window_request(
        &mut self,
        _xwm: XwmId,
        window: X11Surface,
        _timestamp: u32,
        _currently_active_window: Option<X11Surface>,
    ) {
        if let Some(wl) = window.wl_surface() {
            let old_focused = self.focused_surface.clone();
            if old_focused.as_ref() != Some(&wl) {
                if let Some(old_surface) = &old_focused {
                    if let Some(old_w) = self.wl_to_window.get(old_surface) {
                        old_w.set_activated(false);
                    }
                }
            }
            self.focused_surface = Some(wl.clone());
            if let Some(x11_window) = self.wl_to_window.get(&wl) {
                x11_window.set_activated(true);
            }
            if let Some(keyboard) = self.keyboard.clone() {
                keyboard.set_focus(self, Some(wl), SERIAL_COUNTER.next_serial());
            }
            log::debug!("XWayland: active_window_request id={}", window.window_id());
        }
    }

    fn allow_selection_access(
        &mut self,
        _xwm: XwmId,
        _selection: smithay::wayland::selection::SelectionTarget,
    ) -> bool {
        true
    }

    fn send_selection(
        &mut self,
        _xwm: XwmId,
        _selection: smithay::wayland::selection::SelectionTarget,
        _mime_type: String,
        fd: std::os::unix::io::OwnedFd,
    ) {
        let mut file = std::fs::File::from(fd);
        let mut text = String::new();
        if file.read_to_string(&mut text).is_ok() && !text.is_empty() {
            if let Ok(mut guard) = self.clipboard_text.lock() {
                *guard = text;
            }
        }
    }

    fn disconnected(&mut self, _xwm: XwmId) {
        let surfaces: Vec<_> = self.wl_to_window.keys().cloned().collect();
        for wl in &surfaces {
            if let Some(x11_window) = self.wl_to_window.remove(wl) {
                self.space.unmap_elem(&WindowElement(x11_window));
            }
            self.minimized.remove(wl);
            self.maximize_restore.remove(wl);
            self.unmanaged_surfaces.retain(|s| s != wl);
        }
        self.gesture_target = None;
        self.gesture_surface = None;
        self.focused_surface = None;
        log::warn!("XWayland: X11 WM disconnected, cleaned up {} surfaces", surfaces.len());
    }

    fn property_notify(&mut self, _xwm: XwmId, window: X11Surface, property: smithay::xwayland::xwm::WmWindowProperty) {
        let _ = (window.clone(), property);
        log::debug!("XWayland: property_notify id={} property={:?}", window.window_id(), property);
    }

    fn minimize_request(&mut self, _xwm: XwmId, window: X11Surface) {
        if let Some(wl) = window.wl_surface() {
            self.toggle_minimize(wl.clone());
        }
    }

    fn unminimize_request(&mut self, _xwm: XwmId, window: X11Surface) {
        if let Some(wl) = window.wl_surface() {
            if self.minimized.contains_key(&wl) {
                self.toggle_minimize(wl.clone());
            }
        }
    }

    fn above_request(&mut self, _xwm: XwmId, window: X11Surface) {
        if let Some(wl) = window.wl_surface() {
            if let Some(x11_window) = self.wl_to_window.get(&wl) {
                let loc = self.space.element_location(&WindowElement(x11_window.clone()));
                self.space.relocate_element(&WindowElement(x11_window.clone()), loc.unwrap_or_else(|| smithay::utils::Point::from((0, self.reserved_top))));
            }
        }
        log::debug!("XWayland: above_request id={}", window.window_id());
    }

    fn below_request(&mut self, _xwm: XwmId, window: X11Surface) {
        if let Some(wl) = window.wl_surface() {
            if let Some(x11_window) = self.wl_to_window.get(&wl) {
                if let Some(loc) = self.space.element_location(&WindowElement(x11_window.clone())) {
                    self.space.relocate_element(&WindowElement(x11_window.clone()), (loc.x, self.reserved_top + 1000));
                }
            }
        }
        log::debug!("XWayland: below_request id={}", window.window_id());
    }

    fn ping_acked(&mut self, _xwm: XwmId, window: X11Surface, _timestamp: u32) {
        log::debug!("XWayland: ping_acked id={}", window.window_id());
    }

    fn sync_request_timeout(&mut self, _xwm: XwmId, window: X11Surface) {
        log::warn!("XWayland: sync timeout id={}", window.window_id());
    }
}
