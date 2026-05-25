pub mod root_env;
pub mod config {
    pub const ARCH_FS_ROOT: &str = "/data/data/com.winland.server/files/rootfs";
    pub const CONFIG_FILE: &str = "/etc/winland.conf";

    #[derive(Debug, Clone, Default)]
    pub struct LocalConfig;

    pub fn parse_config(_path: String) -> LocalConfig {
        LocalConfig
    }
}
