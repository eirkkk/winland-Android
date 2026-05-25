use std::path::Path;

pub struct DistroManager {
    base_path: String,
}

impl DistroManager {
    pub fn new(base_path: &str) -> Self {
        Self {
            base_path: base_path.to_string(),
        }
    }


    pub fn prepare_chroot(&self, name: &str) -> Result<(), String> {
        let root_path = Path::new(&self.base_path).join("distros").join(name);
        
        log::info!("DistroManager: preparation for {} complete. Note: Mounts must be handled EXTERNALLY by winland-setup.", root_path.display());
        Ok(())
    }
}
