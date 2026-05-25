use std::env;
use std::fs;
use std::path::PathBuf;

fn main() {
    println!("cargo:rerun-if-changed=build.rs");
    println!("cargo:rerun-if-changed=../libxkbcommon/build-android/libxkbcommon.so");
    println!("cargo:rerun-if-changed=src/android/sensor_shim.c");

    cc::Build::new()
        .file("src/android/sensor_shim.c")
        .compile("sensor_shim");

    let smithay_android_enabled = env::var_os("CARGO_FEATURE_SMITHAY_ANDROID").is_some();
    let target_os = env::var("CARGO_CFG_TARGET_OS").unwrap_or_default();
    let target_arch = env::var("CARGO_CFG_TARGET_ARCH").unwrap_or_default();

    if !smithay_android_enabled || target_os != "android" || target_arch != "aarch64" {
        return;
    }

    let manifest_dir = PathBuf::from(env::var("CARGO_MANIFEST_DIR").expect("CARGO_MANIFEST_DIR missing"));

    // Primary source: the library we built from source in libxkbcommon/build-android/
    // Fallback: patches dir (alternative build location)
    let xkbcommon_candidates = [
        manifest_dir.join("../libxkbcommon/build-android/libxkbcommon.so"),
        manifest_dir.join("../patches/build-libxkbcommon/output/lib/libxkbcommon.so"),
    ];

    let xkbcommon_lib = xkbcommon_candidates
        .iter()
        .find_map(|candidate| {
            let p = candidate.canonicalize().ok()?;
            // Must be a real non-empty file
            if p.metadata().map(|m| m.len() > 0).unwrap_or(false) {
                Some(p)
            } else {
                None
            }
        })
        .unwrap_or_else(|| {
            panic!(
                "smithay_android requires a non-empty libxkbcommon.so; \
                 build it first with: cd libxkbcommon && meson setup build-android --cross-file android-arm64.ini && ninja -C build-android"
            )
        });

    let xkbcommon_dir = xkbcommon_lib
        .parent()
        .expect("libxkbcommon.so must have a parent directory");

    // Link against libxkbcommon.so as a shared library dependency.
    // The symbols (xkb_keymap_new_from_names etc.) are referenced as undefined
    // externals in the produced cdylib and must be resolved at runtime from
    // the copy packaged in jniLibs.
    println!("cargo:rustc-link-search=native={}", xkbcommon_dir.display());
    println!("cargo:rustc-link-lib=dylib=xkbcommon");
    // Work around occasional Android NDK ld.lld crash on aarch64 during relocation scanning.
    println!("cargo:rustc-cdylib-link-arg=-Wl,--threads=1");

    // Copy the built libxkbcommon.so into jniLibs so Gradle packages it in the APK.
    let packaged_jni_dir = manifest_dir.join("../app/src/main/jniLibs/arm64-v8a");
    if let Err(error) = fs::create_dir_all(&packaged_jni_dir) {
        panic!("failed to create JNI output directory: {error}");
    }

    let dest = packaged_jni_dir.join("libxkbcommon.so");
    // Guard: only copy when source != destination (avoids truncating the file)
    if xkbcommon_lib != dest {
        if let Err(error) = fs::copy(&xkbcommon_lib, &dest) {
            panic!("failed to copy {xkbcommon_lib:?} -> {dest:?}: {error}");
        }
        eprintln!("cargo:warning=Copied libxkbcommon.so ({} bytes) -> jniLibs/arm64-v8a/",
            dest.metadata().map(|m| m.len()).unwrap_or(0));
    }
}
