#[cfg(feature = "smithay_android")]
use std::io::Write;
#[cfg(feature = "smithay_android")]
use smithay::input::Seat;
#[cfg(feature = "smithay_android")]
use smithay::wayland::selection::{SelectionHandler, SelectionSource, SelectionTarget};
#[cfg(feature = "smithay_android")]
use crate::android::backend::wayland::seat::AndroidSeatRuntime;

#[cfg(feature = "smithay_android")]
impl SelectionHandler for AndroidSeatRuntime {
    type SelectionUserData = String;

    fn new_selection(
        &mut self,
        ty: SelectionTarget,
        source: Option<SelectionSource>,
        seat: Seat<Self>,
    ) {
        if ty != SelectionTarget::Clipboard && ty != SelectionTarget::Primary {
            return;
        }

        let Some(source) = source else {
            return;
        };

        let text_mime = "text/plain".to_string();
        let utf8_mime = "text/plain;charset=utf-8".to_string();
        let mime = if source.mime_types().contains(&utf8_mime) {
            utf8_mime
        } else if source.mime_types().contains(&text_mime) {
            text_mime
        } else {
            return;
        };

        let (read_fd, write_fd) = match nix::unistd::pipe2(nix::fcntl::OFlag::O_CLOEXEC) {
            Ok(fds) => fds,
            Err(e) => {
                log::warn!("Clipboard: failed to create pipe: {}", e);
                return;
            }
        };

        use smithay::wayland::selection::data_device::request_data_device_client_selection;
        if let Err(e) = request_data_device_client_selection::<AndroidSeatRuntime>(&seat, mime, write_fd) {
            log::warn!("Clipboard: request_data_device_client_selection failed: {:?}", e);
            return;
        }

        let clipboard = self.clipboard_text.clone();
        std::thread::spawn(move || {
            use std::io::Read;
            let mut file = std::fs::File::from(read_fd);
            let mut buf = String::new();
            if file.read_to_string(&mut buf).is_ok() {
                let text = buf.trim_end_matches('\0').to_string();
                if !text.is_empty() {
                    log::debug!("Clipboard: Wayland client set selection len={}", text.len());
                    if let Ok(mut guard) = clipboard.lock() {
                        *guard = text.clone();
                    }
                    let Some(vm) = crate::java_vm() else { return; };
                    let result = vm.attach_current_thread_permanently().and_then(|mut env| {
                        let jstr = match env.new_string(&text) {
                            Ok(s) => s,
                            Err(_) => return Ok(()),
                        };
                        env.call_static_method(
                            "com/winland/server/NativeBridge",
                            "onWaylandClipboardChanged",
                            "(Ljava/lang/String;)V",
                            &[jni::objects::JValue::Object(jstr.as_ref())],
                        )?;
                        Ok(())
                    });
                    if let Err(e) = result {
                        log::warn!("Clipboard: JNI onWaylandClipboardChanged failed: {}", e);
                    }
                }
            }
        });
    }

    fn send_selection(
        &mut self,
        ty: SelectionTarget,
        mime_type: String,
        fd: std::os::unix::io::OwnedFd,
        _seat: Seat<Self>,
        user_data: &Self::SelectionUserData,
    ) {
        if ty != SelectionTarget::Clipboard && ty != SelectionTarget::Primary {
            return;
        }

        if mime_type.starts_with("text/plain") || mime_type == "UTF8_STRING" || mime_type == "STRING" || mime_type == "TEXT" {
            let text = user_data.clone();
            std::thread::spawn(move || {
                let mut file = std::fs::File::from(fd);
                let _ = file.write_all(text.as_bytes());
            });
        }
    }
}
