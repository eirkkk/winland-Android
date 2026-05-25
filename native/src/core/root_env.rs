use std::fs;
use std::path::Path;
use std::ffi::CString;
use std::os::unix::io::AsRawFd;
use std::io;

pub struct NativeRoot;

impl NativeRoot {
    /// Enforce root identity (UID 0, GID 0)
    /// NOTE: This is now a NO-OP in the app process to prevent SECCOMP violations (SIGSYS).
    /// Identity, mounts, and ownership MUST be handled by the external `winland-setup` binary.
    pub fn enforce_root() -> Result<(), String> {
        log::warn!("NativeRoot: enforce_root called in app process context. Skipping to avoid SECCOMP violation.");
        Ok(())
    }

    /// Perform programmatic chroot and chdir
    pub fn enter_jail(rootfs: &Path) -> Result<(), String> {
        let rootfs_str = rootfs.to_string_lossy();
        let c_rootfs = CString::new(rootfs_str.as_ref()).map_err(|e| e.to_string())?;
        
        // chroot
        let ret_chroot = unsafe { libc::chroot(c_rootfs.as_ptr()) };
        if ret_chroot != 0 {
            return Err(format!("chroot failed: {}", io::Error::last_os_error()));
        }
        
        // chdir to /
        let c_root = CString::new("/").unwrap();
        let ret_chdir = unsafe { libc::chdir(c_root.as_ptr()) };
        if ret_chdir != 0 {
            return Err(format!("chdir / failed: {}", io::Error::last_os_error()));
        }

        log::info!("NativeRoot: successfully entered jail at {}", rootfs_str);
        Ok(())
    }

    /// Autonomous bind mounts for system folders
    /// NOTE: Now a NO-OP in the app process. Handled by winland-setup.
    pub fn mount_system_folders(_rootfs: &Path) -> Result<(), String> {
        log::warn!("NativeRoot: mount_system_folders skipped - Handled by external setup");
        Ok(())
    }

    /// Set unified SELinux context (Android 14 compatible)
    /// NOTE: Now a NO-OP in the app process. Handled by winland-setup.
    pub fn set_unified_context(path: &Path) -> Result<(), String> {
        log::warn!("NativeRoot: Skipping set_unified_context for {} - Handled by external setup", path.display());
        Ok(())
    }

    /// Redirect stdout and stderr to a log file programmatically
    pub fn redirect_stdio(log_path: &Path) -> Result<(), String> {
        // Create directory for log if needed
        if let Some(parent) = log_path.parent() {
            let _ = fs::create_dir_all(parent);
        }

        let file = fs::OpenOptions::new()
            .create(true)
            .write(true)
            .append(true)
            .open(log_path)
            .map_err(|e| format!("Failed to open log file {}: {}", log_path.display(), e))?;

        let fd = file.as_raw_fd();

        unsafe {
            if libc::dup2(fd, libc::STDOUT_FILENO) < 0 {
                return Err(format!("dup2 stdout failed: {}", io::Error::last_os_error()));
            }
            if libc::dup2(fd, libc::STDERR_FILENO) < 0 {
                return Err(format!("dup2 stderr failed: {}", io::Error::last_os_error()));
            }
        }

        log::info!("NativeRoot: stdout/stderr redirected to {}", log_path.display());
        Ok(())
    }

    /// Perform a native chown programmatically
    /// NOTE: Now a NO-OP in the app process. Handled by winland-setup.
    pub fn programmatic_chown(path: &Path, _uid: u32, _gid: u32) -> Result<(), String> {
        log::warn!("WinlandV2: Skipping chown for {} - Handled by external setup", path.display());
        Ok(())
    }
}
