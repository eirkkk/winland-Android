// NOTE: Audio init stubbed for Android JNI build.
// Audio goes through PulseAudio in the Linux chroot over TCP.
pub fn init() {
    log::info!("Audio: stub — audio handled by PulseAudio in chroot.");
}
