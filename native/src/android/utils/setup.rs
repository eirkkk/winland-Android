use std::sync::atomic::{AtomicBool, Ordering};
use std::path::Path;
use std::fs;
use std::ffi::CString;
use std::thread;
use std::time::Duration;
use log::{info, error, warn};

static IS_SETTING_UP: AtomicBool = AtomicBool::new(false);

pub fn native_run_setup(rootfs: String, files_dir: String, app_uid: u32) -> Result<(), String> {
    if IS_SETTING_UP.swap(true, Ordering::SeqCst) {
        return Err("Setup is already in progress".to_string());
    }

    let result = (|| -> Result<(), String> {
        info!("WinlandNative: Performing pre-mount disk sync...");
        unsafe { libc::sync() };

        info!("WinlandNative: Starting native environment setup at {}", rootfs);
        
        let rootfs_path = Path::new(&rootfs);
        let files_path = Path::new(&files_dir);

        // 1. Ensure internal tmp directory exists
        let internal_tmp = files_path.join("tmp");
        if !internal_tmp.exists() {
            fs::create_dir_all(&internal_tmp).map_err(|e| format!("Failed to create internal tmp: {e}"))?;
        }

        // 2. Perform Mounts in Hierarchical Order
        info!("WinlandNative: Mounting system filesystems...");

        // A. /dev (Bind) - MUST BE FIRST as sub-mounts depend on it
        mount_safe("/dev", &rootfs_path.join("dev"), "", libc::MS_BIND | libc::MS_REC, "")?;

        // B. Procfs
        mount_safe("proc", &rootfs_path.join("proc"), "proc", 0, "")?;

        // C. Sysfs
        mount_safe("sysfs", &rootfs_path.join("sys"), "sysfs", 0, "")?;

        // D. DevPts (Bind) - Depends on /dev
        mount_safe("/dev/pts", &rootfs_path.join("dev/pts"), "", libc::MS_BIND, "")?;

        // E. Shm (Tmpfs) - Depends on /dev
        mount_safe("tmpfs", &rootfs_path.join("dev/shm"), "tmpfs", libc::MS_NOSUID | libc::MS_NODEV, "size=64M,mode=777")?;

        // F. Binderfs (Virtual Binder Filesystem) - REQUIRED for am/pm commands
        let binderfs_host = Path::new("/dev/binderfs");
        if binderfs_host.exists() {
            mount_safe("/dev/binderfs", &rootfs_path.join("dev/binderfs"), "binder", 0, "")?;
            
            // Create symlinks for common binder nodes
            let binder_nodes = ["binder", "hwbinder", "vndbinder"];
            for node in &binder_nodes {
                let target = format!("/dev/binderfs/{}", node);
                let link = rootfs_path.join(format!("dev/{}", node));
                if let Err(e) = create_symlink(&link, &target) {
                    warn!("WinlandNative: Could not create binder symlink {}: {}", node, e);
                }
            }
        }

        // G. Tmp (Bind from filesDir/tmp to ensure reliability)
        mount_safe(internal_tmp.to_str().unwrap(), &rootfs_path.join("tmp"), "", libc::MS_BIND, "")?;

        // 3. Apply Secure Ownership and SELinux Contexts
        info!("WinlandNative: Applying secure ownership and SELinux contexts...");
        let context = "u:object_r:app_data_file:s0";

        // A. Internal Tmp (Socket Dir)
        apply_secure_ownership(&internal_tmp, 0, app_uid, 0o770)?;
        apply_selinux_context(&internal_tmp, context)?;

        // B. Rootfs Tmp (Bind Point)
        apply_secure_ownership(&rootfs_path.join("tmp"), 0, app_uid, 0o770)?;
        apply_selinux_context(&rootfs_path.join("tmp"), context)?;

        // C. Run/VarRun directories
        let run_dir = rootfs_path.join("run");
        let var_run_dir = rootfs_path.join("var/run");
        for dir in &[&run_dir, &var_run_dir] {
            if dir.exists() {
                apply_secure_ownership(dir, 0, app_uid, 0o770)?;
                apply_selinux_context(dir, context)?;
            }
        }

        // D. Context Recursive for rest of system
        apply_context_recursive(rootfs_path, context)?;

        // 4. Create Compatibility Symlinks for Kali NetHunter & Legacy Pathing
        info!("WinlandNative: Creating compatibility symlinks for Kali NetHunter...");
        create_compatibility_symlinks(rootfs_path, context)?;

        info!("WinlandNative: Native environment setup completed successfully.");
        Ok(())
    })();

    IS_SETTING_UP.store(false, Ordering::SeqCst);
    result
}

fn mount_safe(source: &str, target: &Path, fstype: &str, flags: u64, data: &str) -> Result<(), String> {
    if is_already_mounted(target) {
        warn!("WinlandNative: {} is already mounted, skipping.", target.display());
        return Ok(());
    }

    // Force create target directory before mounting
    if let Err(e) = fs::create_dir_all(target) {
        warn!("WinlandNative: Warning creating directory {}: {}", target.display(), e);
    }

    let c_source = CString::new(source).map_err(|_| "Invalid source path")?;
    let c_target = CString::new(target.to_str().ok_or("Invalid target path")?).map_err(|_| "Invalid target path")?;
    let c_fstype = CString::new(fstype).map_err(|_| "Invalid fstype")?;
    let c_data = CString::new(data).map_err(|_| "Invalid data")?;

    let max_retries = 3;
    let mut last_error = String::new();

    for attempt in 1..=max_retries {
        unsafe {
            let ret = libc::mount(
                c_source.as_ptr(),
                c_target.as_ptr(),
                if fstype.is_empty() { std::ptr::null() } else { c_fstype.as_ptr() },
                flags as libc::c_ulong,
                if data.is_empty() { std::ptr::null() } else { c_data.as_ptr() as *const libc::c_void },
            );

            if ret == 0 {
                info!("WinlandNative: Successfully mounted {} on {} (Attempt {})", source, target.display(), attempt);
                return Ok(());
            } else {
                let err = std::io::Error::last_os_error();
                last_error = format!("{}", err);
                
                // If it's the last attempt, don't sleep
                if attempt < max_retries {
                    warn!("WinlandNative: Mount {} failed (Attempt {}): {}. Retrying in 200ms...", source, attempt, err);
                    thread::sleep(Duration::from_millis(200));
                    
                    // Re-ensure directory exists (just in case)
                    let _ = fs::create_dir_all(target);
                }
            }
        }
    }

    error!("WinlandNative: Final mount failure for {} on {}: {}", source, target.display(), last_error);
    Err(format!("Mount failed for {}: {}", source, last_error))
}

fn is_already_mounted(target: &Path) -> bool {
    let target_str = match target.to_str() {
        Some(s) => s,
        None => return false,
    };

    if let Ok(mounts) = fs::read_to_string("/proc/mounts") {
        for line in mounts.lines() {
            let parts: Vec<&str> = line.split_whitespace().collect();
            if parts.len() >= 2 && parts[1] == target_str {
                return true;
            }
        }
    }
    false
}

fn apply_context_recursive(path: &Path, context: &str) -> Result<(), String> {
    // For now, we apply to the main directory and key subdirectories to avoid massive performance hit
    // and potentially breaking mounts that shouldn't have app_data_file context (like /proc)
    
    let targets = ["", "root", "usr", "etc", "var", "tmp", "run", "home"];
    
    for t in targets {
        let p = if t.is_empty() { path.to_path_buf() } else { path.join(t) };
        if p.exists() {
            apply_selinux_context(&p, context)?;
        }
    }
    
    Ok(())
}

fn apply_selinux_context(path: &Path, context: &str) -> Result<(), String> {
    let c_path = CString::new(path.to_str().ok_or("Invalid path")?).map_err(|_| "Invalid path")?;
    let c_name = CString::new("security.selinux").unwrap();
    let c_value = CString::new(context).unwrap();

    unsafe {
        let ret = libc::lsetxattr(
            c_path.as_ptr(),
            c_name.as_ptr(),
            c_value.as_ptr() as *const libc::c_void,
            c_value.as_bytes().len(),
            0,
        );

        if ret == 0 {
            Ok(())
        } else {
            let err = std::io::Error::last_os_error();
            // We warn instead of error here because some filesystems might not support xattrs
            warn!("WinlandNative: Could not set SELinux context on {}: {}", path.display(), err);
            Ok(())
        }
    }
}

fn create_compatibility_symlinks(rootfs_path: &Path, context: &str) -> Result<(), String> {
    // A. Link inside Chroot: /nh -> /
    // This allows absolute paths like /nh/bin/kali to work inside the chroot.
    let internal_nh = rootfs_path.join("nh");
    if let Err(e) = create_symlink(&internal_nh, ".") {
         warn!("WinlandNative: Could not create internal /nh link: {}", e);
    } else {
         let _ = apply_selinux_context(&internal_nh, context);
    }

    // B. Link outside Chroot (Global Legacy): /data/local/nh -> rootfs_path
    // This allows external tools/scripts to find our chroot in the standard NetHunter location.
    let global_nh = Path::new("/data/local/nh");
    if let Err(e) = create_symlink(global_nh, rootfs_path.to_str().unwrap()) {
         warn!("WinlandNative: Could not create global /data/local/nh link (check root): {}", e);
    } else {
         let _ = apply_selinux_context(global_nh, context);
    }

    Ok(())
}

fn apply_secure_ownership(path: &Path, uid: u32, gid: u32, mode: u32) -> Result<(), String> {
    let c_path = CString::new(path.to_str().ok_or("Invalid path")?).map_err(|_| "Invalid path")?;

    unsafe {
        // 1. CHOWN (root:app_uid)
        if libc::chown(c_path.as_ptr(), uid as libc::uid_t, gid as libc::gid_t) != 0 {
            let err = std::io::Error::last_os_error();
            error!("WinlandNative: chown failed for {}: {}", path.display(), err);
            return Err(format!("chown failed for {}: {}", path.display(), err));
        }

        // 2. CHMOD (0o770)
        if libc::chmod(c_path.as_ptr(), mode as libc::mode_t) != 0 {
            let err = std::io::Error::last_os_error();
            error!("WinlandNative: chmod failed for {}: {}", path.display(), err);
            return Err(format!("chmod failed for {}: {}", path.display(), err));
        }
    }

    info!("WinlandNative: Applied secure ownership (uid={}, gid={}, mode=0o{:o}) to {}", uid, gid, mode, path.display());
    Ok(())
}

fn create_symlink(link: &Path, target: &str) -> std::io::Result<()> {
    use std::os::unix::fs::symlink;
    
    if link.exists() || fs::read_link(link).is_ok() {
        let _ = fs::remove_file(link);
    }
    
    symlink(target, link)
}
