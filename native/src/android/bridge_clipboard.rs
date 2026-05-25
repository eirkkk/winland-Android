use jni::JNIEnv;
use jni::objects::{JClass, JString};

#[no_mangle]
/// # Safety
///
/// Called from the JVM when the Android clipboard changes (Wayland→Android feedback or
/// explicit Android→Wayland push). Routes text into the Smithay data-device selection
/// so Wayland clients can paste from it.
pub unsafe extern "system" fn Java_com_winland_server_NativeBridge_updateClipboard(
    mut env: JNIEnv,
    _class: JClass,
    text: JString,
) {
    let input: String = env.get_string(&text).expect("Couldn't get java string!").into();
    log::info!("Bridge: updateClipboard Android->Wayland len={}", input.len());
    #[cfg(feature = "smithay_android")]
    crate::android::command_channel::send_command(crate::android::command_channel::JniCommand::UpdateClipboard { text: input });
}

#[no_mangle]
/// # Safety
///
/// Called from the JVM when the Android clipboard changes.
/// Injects text as a server-side Wayland `wl_data_device` selection so clients can paste.
pub unsafe extern "system" fn Java_com_winland_server_NativeBridge_sendClipboardTextToWayland(
    mut env: JNIEnv,
    _class: JClass,
    text: JString,
) {
    let input: String = env.get_string(&text).expect("Couldn't get java string!").into();
    log::info!("Bridge: sendClipboardTextToWayland len={}", input.len());
    #[cfg(feature = "smithay_android")]
    crate::android::command_channel::send_command(crate::android::command_channel::JniCommand::UpdateClipboard { text: input });
}
