use jni::JNIEnv;
use jni::objects::JClass;

#[no_mangle]
/// # Safety
///
/// Called from the JVM when the Android activity goes to background (onStop) or
/// when the Surface is destroyed (surfaceDestroyed). Freezes Wayland compositor
/// rendering to save power and CPU, and destroys the EGL surface so the
/// compositor thread won't attempt to render into a stale/dead ANativeWindow
/// BufferQueue (avoids "BufferQueue abandoned" errors on resume).
pub unsafe extern "system" fn Java_com_winland_server_NativeBridge_suspendRendering(
    _env: JNIEnv,
    _class: JClass,
) {
    log::info!("Bridge: suspendRendering");
    #[cfg(feature = "smithay_android")]
    {
        crate::android::backend::wayland::engine_timing::set_rendering_active(false);
        // Fire-and-forget: destroy EGL surface and release ANativeWindow on
        // the compositor thread so stale BufferQueue won't be used for rendering.
        // If the surface/context was already destroyed, this is a no-op.
        crate::android::command_channel::send_command(
            crate::android::command_channel::JniCommand::SuspendRendering,
        );
    }
}

#[no_mangle]
/// # Safety
///
/// Called from the JVM when the Android activity returns to foreground (onResume).
/// Resumes Wayland compositor rendering.
pub unsafe extern "system" fn Java_com_winland_server_NativeBridge_resumeRendering(
    _env: JNIEnv,
    _class: JClass,
) {
    log::info!("Bridge: resumeRendering");
    #[cfg(feature = "smithay_android")]
    crate::android::backend::wayland::engine_timing::set_rendering_active(true);
}
