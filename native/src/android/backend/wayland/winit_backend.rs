use ndk::native_window::NativeWindow;
use std::ptr::NonNull;

pub struct AndroidNativeSurface {
    pub native_window: NativeWindow,
}

impl AndroidNativeSurface {
    pub fn new(native_window: NativeWindow) -> Self {
        Self { native_window }
    }
}

pub struct WinitGraphicsBackend {
    pub surface: AndroidNativeSurface,
}

impl WinitGraphicsBackend {
    pub fn bind_raw_surface(native_window: *mut ndk_sys::ANativeWindow) -> Self {
        log::info!("EGL: Binding raw ANativeWindow surface...");
        let nw = unsafe { NativeWindow::from_ptr(NonNull::new(native_window).expect("Null NativeWindow pointer")) };
        Self {
            surface: AndroidNativeSurface::new(nw),
        }
    }
}

pub fn create_egl_display() {
    log::info!("EGL: Initializing Android EGL display...");
}
