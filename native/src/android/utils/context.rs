use crate::{
    core::config::{LocalConfig, ARCH_FS_ROOT, CONFIG_FILE, parse_config},
};
use jni::{objects::{JObject, JString}, JNIEnv};
use std::path::PathBuf;
use std::sync::RwLock;

#[derive(Debug, Clone)]
pub struct ApplicationContext {
    pub cache_dir: PathBuf,
    pub data_dir: PathBuf,
    pub native_library_dir: PathBuf,
    pub local_config: LocalConfig,
    pub permission_all_files_access: bool,
}

impl ApplicationContext {
    pub fn build(env: &mut JNIEnv, activity: &JObject) -> Result<(), String> {
        let cache_dir = Self::get_path(env, activity, "getCacheDir")?;
        let data_dir = Self::get_path(env, activity, "getFilesDir")?;
        let native_library_dir = Self::get_native_library_dir(env, activity)?;
        let full_config_path = format!("{}{}", ARCH_FS_ROOT, CONFIG_FILE);
        let local_config = parse_config(full_config_path);
        let permission_all_files_access = Self::is_all_files_access_granted(env);

        let mut context = APPLICATION_CONTEXT
            .write()
            .map_err(|_| "Failed to acquire application context write lock".to_string())?;
        *context = Some(ApplicationContext {
            cache_dir,
            data_dir,
            native_library_dir,
            local_config,
            permission_all_files_access,
        });
        if let Some(context) = context.as_ref() {
            log::info!("ApplicationContext initialized: {:?}", context);
        }

        Ok(())
    }

    fn get_path(env: &mut JNIEnv, activity: &JObject, method: &str) -> Result<PathBuf, String> {
        let path_obj = env
            .call_method(activity, method, "()Ljava/io/File;", &[])
            .map_err(|error| format!("{method}() failed: {error}"))?
            .l()
            .map_err(|error| format!("{method}() did not return a File: {error}"))?;
        let path_str = env
            .call_method(&path_obj, "getAbsolutePath", "()Ljava/lang/String;", &[])
            .map_err(|error| format!("{method}().getAbsolutePath() failed: {error}"))?
            .l()
            .map_err(|error| format!("{method}().getAbsolutePath() did not return a String: {error}"))?;
        let path: String = env
            .get_string(&JString::from(path_str))
            .map_err(|error| format!("{method}() path string decode failed: {error}"))?
            .into();
            
        // Force path unification to /data/data/
        let unified_path = path.replace("/data/user/0/", "/data/data/");
        log::info!("Unified path from {} to {}", path, unified_path);
        Ok(PathBuf::from(unified_path))
    }

    fn get_native_library_dir(env: &mut JNIEnv, activity: &JObject) -> Result<PathBuf, String> {
        let app_info = env
            .call_method(activity, "getApplicationInfo", "()Landroid/content/pm/ApplicationInfo;", &[])
            .map_err(|error| format!("getApplicationInfo() failed: {error}"))?
            .l()
            .map_err(|error| format!("getApplicationInfo() did not return ApplicationInfo: {error}"))?;
        let native_library_dir = env
            .get_field(app_info, "nativeLibraryDir", "Ljava/lang/String;")
            .map_err(|error| format!("ApplicationInfo.nativeLibraryDir lookup failed: {error}"))?
            .l()
            .map_err(|error| format!("ApplicationInfo.nativeLibraryDir was not a String: {error}"))?;
        let path: String = env
            .get_string(&JString::from(native_library_dir))
            .map_err(|error| format!("nativeLibraryDir string decode failed: {error}"))?
            .into();
        Ok(PathBuf::from(path))
    }

    fn is_all_files_access_granted(env: &mut JNIEnv) -> bool {
        env.call_static_method("android/os/Environment", "isExternalStorageManager", "()Z", &[])
            .and_then(|value| value.z())
            .unwrap_or(false)
    }
}

static APPLICATION_CONTEXT: RwLock<Option<ApplicationContext>> = RwLock::new(None);
pub fn get_application_context() -> ApplicationContext {
    APPLICATION_CONTEXT
        .read()
        .unwrap_or_else(|_| panic!("ApplicationContext lock poisoned"))
        .as_ref()
        .cloned()
        .expect("ApplicationContext not initialized (initWaylandConnection must be called first)")
}
