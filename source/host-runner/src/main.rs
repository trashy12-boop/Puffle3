//! Headless upstream Ruffle integration test, not a PS3 player.
use ruffle_core::backend::log::LogBackend;
use ruffle_core::events::{KeyDescriptor, KeyLocation, LogicalKey, MouseButton, NamedKey, PhysicalKey, PlayerEvent};
use ruffle_core::limits::ExecutionLimit;
use ruffle_core::{Player, PlayerBuilder};
use ruffle_core::tag_utils::SwfMovieData;
use ruffle_core::FloatDuration;
use std::io::Read;
use std::time::{Duration, Instant};

#[repr(C)]
struct RpEvent { kind: i32, code: u32, down: bool, x: f64, y: f64 }
unsafe extern "C" { fn rp_host_mapped_input_fixture(emit: extern "C" fn(*mut std::ffi::c_void, *const RpEvent), ctx: *mut std::ffi::c_void); }

// The C call is synchronous; ctx points to the locked Player until it returns.
// This deliberately covers only the smoke fixture's Space and mouse actions.
extern "C" fn mapped_event(ctx: *mut std::ffi::c_void, raw: *const RpEvent) {
    let (player, event) = unsafe { (&mut *(ctx as *mut Player), &*raw) };
    match event.kind {
        0 if event.code == 32 => {
            let key = KeyDescriptor { physical_key: PhysicalKey::Space, logical_key: LogicalKey::Character(' '), key_location: KeyLocation::Standard };
            player.handle_event(if event.down { PlayerEvent::KeyDown { key } } else { PlayerEvent::KeyUp { key } });
            if event.down { player.handle_event(PlayerEvent::TextInput { codepoint: ' ' }); }
        }
        1 => {
            let button = if event.code == 0 { MouseButton::Left } else { MouseButton::Right };
            player.handle_event(if event.down { PlayerEvent::MouseDown { x: event.x, y: event.y, button, index: None } } else { PlayerEvent::MouseUp { x: event.x, y: event.y, button } });
        }
        2 => { player.handle_event(PlayerEvent::MouseMove { x: event.x, y: event.y }); }
        _ => {}
    }
}

struct ConsoleLog;
impl LogBackend for ConsoleLog {
    fn avm_trace(&self, message: &str) { println!("{message}"); }
    fn avm_warning(&self, message: &str) { eprintln!("WARNING {message}"); }
}

fn main() {
    if let Err(error) = run() {
        eprintln!("error: {error}");
        std::process::exit(1);
    }
}

fn run() -> Result<(), Box<dyn std::error::Error>> {
    let mut args = std::env::args().skip(1);
    let path = args.next().ok_or("usage: ruffle-ps3-host-runner <file.swf> [frames] [--input-smoke]")?;
    let frames: usize = args.next().unwrap_or_else(|| "10".into()).parse()?;
    let input_smoke = match args.next().as_deref() { None => false, Some("--input-smoke") => true, _ => return Err("unknown option".into()) };
    if frames == 0 || frames > 100_000 || args.next().is_some() {
        return Err("frame count must be between 1 and 100000".into());
    }
    let mut data = Vec::new();
    std::fs::File::open(&path)?.take(64 * 1024 * 1024 + 1).read_to_end(&mut data)?;
    if data.len() > 64 * 1024 * 1024 { return Err("SWF exceeds the host runner's 64 MiB file limit".into()); }
    if data.len() < 8 { return Err("SWF header is truncated".into()); }
    let declared = u32::from_le_bytes([data[4], data[5], data[6], data[7]]);
    if !(8..=64 * 1024 * 1024).contains(&declared) { return Err("invalid or oversized declared SWF length".into()); }
    let movie = SwfMovieData::from_data(&data, format!("file:///{path}"), None, None)
        .map_err(|e| format!("invalid SWF: {e}"))?;
    let fps = movie.frame_rate().to_f64();
    if !fps.is_finite() || fps <= 0.0 || fps > 1000.0 { return Err("invalid SWF frame rate".into()); }
    let player = PlayerBuilder::new().with_log(ConsoleLog).with_movie(movie)
        .with_autoplay(true).with_max_execution_duration(Duration::from_secs(15)).build();
    let mut player = player.lock().map_err(|_| "player lock poisoned")?;
    if !player.preload(&mut ExecutionLimit::none()) { return Err("preload did not complete".into()); }
    let start = Instant::now();
    for frame in 0..frames {
        player.run_frame();
        player.update_timers(FloatDuration::from_millis(1000.0 / fps));
        player.audio_mut().tick();
        player.render();
        if frame == 0 && input_smoke {
            let key=KeyDescriptor { physical_key: PhysicalKey::Tab, logical_key: LogicalKey::Named(NamedKey::Tab), key_location: KeyLocation::Standard };
            player.handle_event(PlayerEvent::KeyDown {key});
            player.handle_event(PlayerEvent::TextInput {codepoint:'\t'});
            player.handle_event(PlayerEvent::KeyUp {key});
            unsafe { rp_host_mapped_input_fixture(mapped_event, (&mut *player as *mut Player).cast()); }
        }
    }
    eprintln!("HEADLESS frames={frames} swf_fps={fps:.3} elapsed_ms={:.3}", start.elapsed().as_secs_f64() * 1000.0);
    Ok(())
}
