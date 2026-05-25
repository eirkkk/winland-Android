use std::env;
use std::process;
use uniffi_winland_core::android::utils::setup::native_run_setup;

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() < 4 {
        eprintln!("Usage: winland-setup <rootfs_dir> <files_dir> <app_uid>");
        process::exit(1);
    }

    let rootfs_dir = args[1].clone();
    let files_dir = args[2].clone();
    let app_uid: u32 = args[3].parse().unwrap_or_else(|_| {
        eprintln!("Error: app_uid must be a valid integer");
        process::exit(1);
    });

    println!("Winland OS: Starting native setup sequence...");
    println!("Rootfs: {}", rootfs_dir);
    println!("Files: {}", files_dir);
    println!("App UID: {}", app_uid);

    match native_run_setup(rootfs_dir, files_dir, app_uid) {
        Ok(_) => {
            println!("Winland OS: Setup completed successfully.");
            process::exit(0);
        }
        Err(e) => {
            eprintln!("Winland OS: Setup failed: {}", e);
            process::exit(2);
        }
    }
}
