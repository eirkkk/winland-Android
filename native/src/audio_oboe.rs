use std::fs::File;
use std::fs::OpenOptions;
use std::io::Read;
use std::io::Write;
use std::path::Path;
use std::sync::atomic::AtomicBool;
use std::sync::atomic::Ordering;
use std::thread;
use std::time::Duration;

use oboe::AudioInputStreamSync;
use oboe::AudioOutputStreamSync;
use oboe::AudioStream;
use oboe::AudioStreamBase;
use oboe::AudioStreamBuilder;
use oboe::AudioStreamSafe;
use oboe::AudioStreamSync;
use oboe::PerformanceMode;

const PLAYBACK_FIFO: &str = "/data/data/com.winland.server/files/tmp/audio_bridge/fifo";
const MIC_FIFO: &str = "/data/data/com.winland.server/files/tmp/audio_bridge/mic_fifo";
const SAMPLE_RATE: i32 = 44100;

static INITIALIZED: AtomicBool = AtomicBool::new(false);
static PLAYING: AtomicBool = AtomicBool::new(false);
static RECORDING: AtomicBool = AtomicBool::new(false);
static THREADS_SPAWNED: AtomicBool = AtomicBool::new(false);

pub fn init() {
    if INITIALIZED.swap(true, Ordering::SeqCst) {
        return;
    }
    log::info!("Oboe audio engine: initializing");

    if let Some(parent) = Path::new(MIC_FIFO).parent() {
        let _ = std::fs::create_dir_all(parent);
    }

    log::info!("Oboe audio engine initialized");
}

pub fn start_playback() {
    if PLAYING.swap(true, Ordering::SeqCst) {
        return;
    }
    spawn_threads();
}

pub fn stop_playback() {
    PLAYING.store(false, Ordering::SeqCst);
}

pub fn start_recording() {
    RECORDING.store(true, Ordering::SeqCst);
    spawn_threads();
}

pub fn stop_recording() {
    RECORDING.store(false, Ordering::SeqCst);
}

pub fn is_recording() -> bool {
    RECORDING.load(Ordering::Relaxed)
}

pub fn close() {
    stop_playback();
    stop_recording();
}

fn spawn_threads() {
    if THREADS_SPAWNED.swap(true, Ordering::SeqCst) {
        return;
    }
    thread::spawn(playback_thread);
    thread::spawn(recording_thread);
}

fn create_playback_stream() -> oboe::Result<AudioStreamSync<oboe::Output, (i16, oboe::Stereo)>> {
    AudioStreamBuilder::default()
        .set_output()
        .set_stereo()
        .set_i16()
        .set_sample_rate(SAMPLE_RATE)
        .set_performance_mode(PerformanceMode::LowLatency)
        .open_stream()
}

fn playback_thread() {
    log::info!("Oboe playback thread: started");

    let mut stream = match create_playback_stream() {
        Ok(s) => s,
        Err(e) => {
            log::error!("Oboe playback: failed to create stream: {}", e);
            PLAYING.store(false, Ordering::SeqCst);
            return;
        }
    };
    if let Err(e) = stream.start() {
        log::error!("Oboe playback: failed to start stream: {}", e);
        PLAYING.store(false, Ordering::SeqCst);
        return;
    }

    let burst = stream.get_frames_per_burst() as usize;
    let channels = stream.get_channel_count() as usize;
    let buf_size = burst * channels;
    let mut byte_buf = vec![0u8; buf_size * 2];

    while PLAYING.load(Ordering::Relaxed) {
        match File::open(PLAYBACK_FIFO) {
            Ok(mut fifo) => {
                log::info!("Oboe playback: connected to FIFO");
                while PLAYING.load(Ordering::Relaxed) {
                    match fifo.read(&mut byte_buf) {
                        Ok(0) => {
                            log::info!("Oboe playback: FIFO EOF, reconnecting...");
                            break;
                        }
                        Ok(n) => {
                            let frame_count = n / 4;
                            if frame_count > 0 {
                                let stereo_data = unsafe {
                                    std::slice::from_raw_parts(
                                        byte_buf.as_ptr() as *const (i16, i16),
                                        frame_count,
                                    )
                                };
                                let timeout_ns =
                                    std::time::Duration::from_millis(500).as_nanos() as i64;
                                if let Err(e) = stream.write(stereo_data, timeout_ns) {
                                    log::error!("Oboe playback: write error: {}", e);
                                    break;
                                }
                            }
                        }
                        Err(e) => {
                            log::error!("Oboe playback: FIFO read error: {}", e);
                            break;
                        }
                    }
                }
            }
            Err(e) => {
                log::debug!("Oboe playback: waiting for FIFO: {}", e);
                thread::sleep(std::time::Duration::from_millis(500));
            }
        }
    }

    let _ = stream.stop();
    log::info!("Oboe playback thread: stopped");
}

fn create_recording_stream() -> oboe::Result<AudioStreamSync<oboe::Input, (i16, oboe::Mono)>> {
    AudioStreamBuilder::default()
        .set_input()
        .set_mono()
        .set_i16()
        .set_sample_rate(SAMPLE_RATE)
        .set_performance_mode(PerformanceMode::LowLatency)
        .open_stream()
}

fn recording_thread() {
    log::info!("Oboe recording thread: started");

    loop {
        // Wait for recording flag
        while !RECORDING.load(Ordering::Relaxed) {
            thread::sleep(Duration::from_millis(500));
        }

        // Open FIFO (will retry until chroot creates it)
        let mut fifo = match OpenOptions::new().write(true).open(MIC_FIFO) {
            Ok(f) => {
                log::info!("Oboe recording: FIFO opened");
                f
            }
            Err(e) => {
                log::debug!("Oboe recording: waiting for FIFO: {}", e);
                thread::sleep(Duration::from_secs(1));
                continue;
            }
        };

        // Create and start AAudio stream
        let mut stream = match create_recording_stream() {
            Ok(s) => s,
            Err(e) => {
                log::error!("Oboe recording: failed to create stream: {}", e);
                continue;
            }
        };
        if let Err(e) = stream.start() {
            log::error!("Oboe recording: failed to start stream: {}", e);
            continue;
        }
        log::info!("Oboe recording: AAudio stream started");

        let burst = stream.get_frames_per_burst() as usize;
        let channels = stream.get_channel_count() as usize;
        let buf_size = burst * channels;
        let mut byte_buf = vec![0u8; buf_size * 2];

        // Record while flag is set
        while RECORDING.load(Ordering::Relaxed) {
            let i16_slice = unsafe {
                std::slice::from_raw_parts_mut(
                    byte_buf.as_mut_ptr() as *mut i16,
                    buf_size,
                )
            };
            let timeout_ns = Duration::from_millis(500).as_nanos() as i64;
            match stream.read(i16_slice, timeout_ns) {
                Ok(frames_read) if frames_read > 0 => {
                    let bytes_to_write = frames_read as usize * channels * 2;
                    if let Err(e) = fifo.write_all(&byte_buf[..bytes_to_write]) {
                        log::error!("Oboe recording: FIFO write error: {}", e);
                        break;
                    }
                }
                Ok(_) => {}
                Err(e) => {
                    log::error!("Oboe recording: read error: {}", e);
                    break;
                }
            }
        }

        // Stop AAudio and close FIFO
        log::info!("Oboe recording: stopping stream");
        let _ = stream.stop();
        drop(fifo);
    }
}
