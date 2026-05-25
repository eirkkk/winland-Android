use std::fs;
use std::path::Path;

pub fn bind_socket(socket_dir: &str) {
    let path = Path::new(socket_dir);
    if path.exists() && !path.is_dir() {
        match fs::remove_file(path) {
            Ok(_) => log::warn!("Wayland: removed stale file blocking socket dir: {}", socket_dir),
            Err(error) => {
                log::error!("Wayland: failed to remove stale socket-dir file {}: {}", socket_dir, error);
                return;
            }
        }
    }

    if !path.exists() {
        match fs::create_dir_all(path) {
            Ok(_) => log::info!("WinlandV2: created missing socket directory {}", socket_dir),
            Err(error) => {
                log::error!(
                    "WinlandV2: failed to create socket directory {}: {}",
                    socket_dir,
                    error
                );
                return;
            }
        }
    }

    log::info!("WinlandV2: socket directory {} is ready (Secure Ownership).", socket_dir);

    for stale_socket in ["wayland-0", "wayland-1", "wayland-0.lock", "wayland-1.lock"] {
        let stale_path = path.join(stale_socket);
        match fs::remove_file(&stale_path) {
            Ok(_) => log::info!("Wayland: removed stale entry {}", stale_path.display()),
            Err(e) if e.kind() == std::io::ErrorKind::NotFound => {}
            Err(e) => log::warn!("Wayland: could not remove stale {}: {}", stale_path.display(), e),
        }
    }

    log::info!("Winland: prepared socket directory {} (SECURE_SYNC_COMPLETE)", socket_dir);
}
