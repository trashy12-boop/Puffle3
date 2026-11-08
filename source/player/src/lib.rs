//! Native Ruffle bridge. All entry points must run on the same PPE thread.
mod renderer;
mod audio;
mod storage;
use ruffle_core::{Player,PlayerBuilder,FloatDuration,tag_utils::SwfMovieData,limits::ExecutionLimit,backend::log::LogBackend,events::*};
use std::{sync::{Arc,Mutex},ffi::c_void,time::Duration};
struct Log;
impl LogBackend for Log {fn avm_trace(&self,s:&str){println!("TRACE {s}")} fn avm_warning(&self,s:&str){println!("WARNING {s}")}}
struct Handle{#[cfg(target_os="ps3")] next_gc:std::cell::Cell<usize>,player:Arc<Mutex<Player>>,mixer:ruffle_core::backend::audio::AudioMixerProxy}
#[repr(C)]pub struct Event{pub kind:i32,pub code:u32,pub down:bool,pub x:f64,pub y:f64}
#[unsafe(no_mangle)]pub extern "C" fn rp_rust_probe()->u64 {let v=vec![11u64,22,33];println!("PS3 Rust ABI probe {v:?} {:?}",std::time::Instant::now());v.iter().sum()}
#[unsafe(no_mangle)]unsafe fn __getrandom_v03_custom(p:*mut u8,n:usize)->Result<(),getrandom::Error>{unsafe extern "C"{fn rp_random(p:*mut u8,n:usize)->i32;}if unsafe{rp_random(p,n)}==0{Ok(())}else{Err(getrandom::Error::UNSUPPORTED)}}
#[unsafe(no_mangle)]pub unsafe extern "C" fn rp_player_open(data:*const u8,len:usize,w:u32,h:u32,path:*const std::ffi::c_char)->*mut c_void {
 if data.is_null() || !(8..=16*1024*1024).contains(&len) || w==0||h==0||w>640||h>480{return std::ptr::null_mut();}
 if path.is_null(){return std::ptr::null_mut();}
 let path=unsafe{std::ffi::CStr::from_ptr(path)}.to_string_lossy();
 let bytes=unsafe{std::slice::from_raw_parts(data,len)};
 let declared=u32::from_le_bytes(bytes[4..8].try_into().unwrap());if !(8..=32*1024*1024).contains(&declared){return std::ptr::null_mut();}
 let movie=match SwfMovieData::from_data(bytes,format!("file://{path}"),None,None){Ok(m)=>m,Err(e)=>{println!("SWF ERROR {e}");return std::ptr::null_mut();}};
 let mixer=ruffle_core::backend::audio::AudioMixer::new(2,48000);let proxy=mixer.proxy();
 let player=PlayerBuilder::new().with_storage(Box::new(storage::Storage(path.into_owned()))).with_audio(audio::Audio{mixer}).with_movie(movie).with_renderer(renderer::SoftwareRenderer::new(w,h)).with_log(Log).with_autoplay(true).with_max_execution_duration(Duration::from_secs(15)).build();
 player.lock().unwrap().set_viewport_dimensions(ruffle_render::backend::ViewportDimensions{width:w,height:h,scale_factor:1.});
 println!("RUFFLE loaded SWF {len} bytes at {w}x{h}");Box::into_raw(Box::new(Handle{#[cfg(target_os="ps3")] next_gc:std::cell::Cell::new(96*1024*1024),player,mixer:proxy})).cast()
}
unsafe fn player<'a>(p:*mut c_void)->&'a Handle{unsafe{&*p.cast::<Handle>()}}
#[unsafe(no_mangle)]pub unsafe extern "C" fn rp_player_close(p:*mut c_void){if !p.is_null(){drop(unsafe{Box::from_raw(p.cast::<Handle>())});println!("RUFFLE player closed");}}
#[cfg(target_os="ps3")]
fn collect_under_pressure(handle:&Handle,p:&mut Player){
 unsafe extern "C"{fn rp_rust_bytes()->usize;}
 let before=unsafe{rp_rust_bytes()};
 if before<handle.next_gc.get(){return;}
 let start=std::time::Instant::now();
 if before>=108*1024*1024 {
  let renderer=(p.renderer_mut() as &mut dyn std::any::Any).downcast_mut::<renderer::SoftwareRenderer>().unwrap();
  renderer.trim_for_memory_pressure();
 }
 p.collect_for_memory_pressure();
 let after=unsafe{rp_rust_bytes()};
 // Hysteresis avoids completing a cycle every frame when assets dominate.
 // A high surviving set still retries before the observed native exhaustion.
 let growth=if after>=108*1024*1024{1024*1024}else{4*1024*1024};
 handle.next_gc.set((after+growth).clamp(96*1024*1024,116*1024*1024));
 println!("GC pressure before={} after={} duration_us={}",before,after,start.elapsed().as_micros());
}
#[unsafe(no_mangle)]pub unsafe extern "C" fn rp_player_tick(p:*mut c_void,ms:f64){if p.is_null()||!ms.is_finite()||ms<0.{return;}let handle=unsafe{player(p)};let mut p=handle.player.lock().unwrap();
 #[cfg(target_os="ps3")] collect_under_pressure(handle,&mut p);
 let mut limit=ExecutionLimit::none();if p.preload(&mut limit){
 #[cfg(target_os="ps3")] collect_under_pressure(handle,&mut p);
 p.tick(FloatDuration::from_millis(ms));}}
#[unsafe(no_mangle)]pub unsafe extern "C" fn rp_player_render(p:*mut c_void)->i32 {
 if p.is_null(){return 0;}
 let mut p=unsafe{player(p)}.player.lock().unwrap();
 if !p.needs_render(){return 0;}p.render();1
}
#[unsafe(no_mangle)]pub unsafe extern "C" fn rp_player_next_tick_us(p:*mut c_void)->u64 {
 if p.is_null(){return 1000;}
 unsafe{player(p)}.player.lock().unwrap().time_til_next_frame().as_micros().clamp(1000,16000) as u64
}
#[unsafe(no_mangle)]pub unsafe extern "C" fn rp_player_event(p:*mut c_void,e:*const Event){
 if p.is_null()||e.is_null(){return;}let e=unsafe{&*e};let mut p=unsafe{player(p)}.player.lock().unwrap();
 match e.kind{
  0=>{let key=key(e.code);p.handle_event(if e.down{PlayerEvent::KeyDown{key}}else{PlayerEvent::KeyUp{key}});if e.down{if let LogicalKey::Character(c)=key.logical_key{p.handle_event(PlayerEvent::TextInput{codepoint:c});}}},
  1=>{let button=if e.code==0{MouseButton::Left}else{MouseButton::Right};p.handle_event(if e.down{PlayerEvent::MouseDown{x:e.x,y:e.y,button,index:None}}else{PlayerEvent::MouseUp{x:e.x,y:e.y,button}});},
  2=>{p.handle_event(PlayerEvent::MouseMove{x:e.x,y:e.y});},_=>{}
 }
}
fn key(code:u32)->KeyDescriptor{
 let (physical_key,logical_key)=match code{
 32=>(PhysicalKey::Space,LogicalKey::Character(' ')),
 13=>(PhysicalKey::Enter,LogicalKey::Named(NamedKey::Enter)),
 16=>(PhysicalKey::ShiftLeft,LogicalKey::Named(NamedKey::Shift)),
 17=>(PhysicalKey::ControlLeft,LogicalKey::Named(NamedKey::Control)),
 18=>(PhysicalKey::AltLeft,LogicalKey::Named(NamedKey::Alt)),
 27=>(PhysicalKey::Escape,LogicalKey::Named(NamedKey::Escape)),
 37=>(PhysicalKey::ArrowLeft,LogicalKey::Named(NamedKey::ArrowLeft)),
 38=>(PhysicalKey::ArrowUp,LogicalKey::Named(NamedKey::ArrowUp)),
 39=>(PhysicalKey::ArrowRight,LogicalKey::Named(NamedKey::ArrowRight)),
 40=>(PhysicalKey::ArrowDown,LogicalKey::Named(NamedKey::ArrowDown)),
 65..=90=>{let keys=[PhysicalKey::KeyA,PhysicalKey::KeyB,PhysicalKey::KeyC,PhysicalKey::KeyD,PhysicalKey::KeyE,PhysicalKey::KeyF,PhysicalKey::KeyG,PhysicalKey::KeyH,PhysicalKey::KeyI,PhysicalKey::KeyJ,PhysicalKey::KeyK,PhysicalKey::KeyL,PhysicalKey::KeyM,PhysicalKey::KeyN,PhysicalKey::KeyO,PhysicalKey::KeyP,PhysicalKey::KeyQ,PhysicalKey::KeyR,PhysicalKey::KeyS,PhysicalKey::KeyT,PhysicalKey::KeyU,PhysicalKey::KeyV,PhysicalKey::KeyW,PhysicalKey::KeyX,PhysicalKey::KeyY,PhysicalKey::KeyZ];(keys[(code-65) as usize],LogicalKey::Character(char::from_u32(code+32).unwrap()))},
 _=>(PhysicalKey::Unknown,LogicalKey::Character('\0'))};
 KeyDescriptor{physical_key,logical_key,key_location:KeyLocation::Standard}
}

#[unsafe(no_mangle)]pub unsafe extern "C" fn rp_player_mix(p:*mut c_void,out:*mut f32,n:usize){
 if p.is_null()||out.is_null()||n>8192{return;}unsafe{player(p)}.mixer.mix(unsafe{std::slice::from_raw_parts_mut(out,n)});
}

#[cfg(test)] mod tests {
 use super::*;
 static FRAME:Mutex<Vec<u8>>=Mutex::new(Vec::new());
 #[unsafe(export_name="rp_present")] extern "C" fn present(p:*const u8,w:u32,h:u32){*FRAME.lock().unwrap()=unsafe{std::slice::from_raw_parts(p,w as usize*h as usize*4)}.to_vec();}
 #[unsafe(export_name="rp_random")] extern "C" fn random(p:*mut u8,n:usize)->i32{getrandom::fill(unsafe{std::slice::from_raw_parts_mut(p,n)}).map(|_|0).unwrap_or(-1)}
 fn red_x()->usize {FRAME.lock().unwrap().chunks_exact(4).enumerate().filter(|(_,p)|p[0]>240&&p[1]<100&&p[2]<100).map(|(i,_)|i%320).min().expect("red vector square must render")}
 #[test] fn real_avm1_controller_moves_rendered_shape(){
  let bytes=include_bytes!("../../tests/fixtures/interactive.swf");
  unsafe{
   let p=rp_player_open(bytes.as_ptr(),bytes.len(),320,240,c"/test.swf".as_ptr());assert!(!p.is_null());
   for _ in 0..5 {rp_player_tick(p,1000./30.);player(p).player.lock().unwrap().collect_for_memory_pressure();}assert_eq!(rp_player_render(p),1);let before=red_x();assert_eq!(rp_player_render(p),0,"unchanged SWF image must be reused");rp_player_set_software(p,1);assert_eq!(rp_player_render(p),1,"renderer switch must redraw a static stage");rp_player_set_software(p,0);assert_eq!(rp_player_render(p),1);
   let e=Event{kind:0,code:90,down:true,x:0.,y:0.};rp_player_event(p,&e);
   for _ in 0..8 {rp_player_tick(p,1000./30.);player(p).player.lock().unwrap().collect_for_memory_pressure();}rp_player_render(p);let after=red_x();assert!(after>before,"key must move actual display object: {before} -> {after}");
   let e=Event{down:false,..e};rp_player_event(p,&e);rp_player_close(p);
  }
 }
 #[test] fn rejects_oversized_declared_movie(){let b=b"FWS\x08\xff\xff\xff\x7f";assert!(unsafe{rp_player_open(b.as_ptr(),b.len(),320,240,c"/bad.swf".as_ptr())}.is_null());}
}
#[cfg(test)]mod audio_tests {
 use super::*;
 #[test]fn embedded_pcm_reaches_real_ruffle_mixer(){
  let b=include_bytes!("../../tests/fixtures/tone.swf");
  unsafe{let p=rp_player_open(b.as_ptr(),b.len(),320,240,c"/test.swf".as_ptr());assert!(!p.is_null());rp_player_tick(p,100.);let mut samples=[0f32;4096];rp_player_mix(p,samples.as_mut_ptr(),samples.len());assert!(samples.iter().all(|x|x.is_finite()));let peak=samples.iter().copied().map(f32::abs).fold(0f32,f32::max);assert!(peak>0.05&&peak<1.,"PCM peak={peak}");rp_player_close(p);}
 }
}
#[cfg(test)]mod storage_test_hooks {
 use std::sync::Mutex;use std::collections::HashMap;use std::sync::LazyLock;
 static DATA:LazyLock<Mutex<HashMap<Vec<u8>,Vec<u8>>>>=LazyLock::new(||Mutex::new(HashMap::new()));
 #[unsafe(export_name="rp_storage_read")]extern "C" fn read(k:*const u8,n:usize,out:*mut u8,max:usize)->i64{let key=unsafe{std::slice::from_raw_parts(k,n)};let d=DATA.lock().unwrap();let Some(v)=d.get(key)else{return -1;};if !out.is_null(){if v.len()>max{return -1;}unsafe{std::ptr::copy_nonoverlapping(v.as_ptr(),out,v.len());}}v.len() as i64}
 #[unsafe(export_name="rp_storage_write")]extern "C" fn write(k:*const u8,n:usize,v:*const u8,len:usize)->i32{DATA.lock().unwrap().insert(unsafe{std::slice::from_raw_parts(k,n)}.to_vec(),unsafe{std::slice::from_raw_parts(v,len)}.to_vec());0}
 #[unsafe(export_name="rp_storage_remove")]extern "C" fn remove(k:*const u8,n:usize){DATA.lock().unwrap().remove(unsafe{std::slice::from_raw_parts(k,n)});}
}

#[cfg(test)]mod performance_tests;

/// Deterministic host replay only: advance exactly one Flash frame, independent
/// of benchmark execution speed. Native scheduling continues to use tick().
#[cfg(not(target_os="ps3"))]
pub unsafe fn benchmark_frame(p:*mut c_void,ms:f64){
 let mut p=unsafe{player(p)}.player.lock().unwrap();
 p.preload(&mut ExecutionLimit::none());p.run_frame();
 p.update_timers(FloatDuration::from_millis(ms));p.audio_mut().tick();
}
#[cfg(not(target_os="ps3"))]
pub unsafe fn benchmark_renderer(p:*mut c_void,cache:bool,profile:bool)->renderer::RenderStats {
 let mut p=unsafe{player(p)}.player.lock().unwrap();
 let r=(p.renderer_mut() as &mut dyn std::any::Any).downcast_mut::<renderer::SoftwareRenderer>().unwrap();
 r.cache_enabled=cache;r.profile=profile;std::mem::take(&mut r.stats)
}

#[repr(C)]pub struct CacheStats {pub paths:u64,pub hits:u64,pub misses:u64,pub bytes:u64}
#[unsafe(no_mangle)]pub unsafe extern "C" fn rp_player_cache_stats(p:*mut c_void,out:*mut CacheStats){
 if p.is_null()||out.is_null(){return;}
 let mut p=unsafe{player(p)}.player.lock().unwrap();
 let r=(p.renderer_mut() as &mut dyn std::any::Any).downcast_mut::<renderer::SoftwareRenderer>().unwrap();
 unsafe{*out=CacheStats{paths:r.stats.path_calls,hits:r.stats.cache_hits,misses:r.stats.cache_misses,bytes:r.stats.cache_bytes as u64};}
 r.stats=renderer::RenderStats::default();
}

#[cfg(not(target_os="ps3"))]
pub unsafe fn benchmark_verify(p:*mut c_void,enabled:bool){
 let mut p=unsafe{player(p)}.player.lock().unwrap();
 let r=(p.renderer_mut() as &mut dyn std::any::Any).downcast_mut::<renderer::SoftwareRenderer>().unwrap();r.validate_cache=enabled;
}

/// Switch rasterizers without changing VM state or timing. Mark the stage dirty
/// so paused/static scenes are refreshed on the next render.
#[unsafe(no_mangle)]pub unsafe extern "C" fn rp_player_set_software(p:*mut c_void,software:i32){
 if p.is_null(){return;}
 let mut p=unsafe{player(p)}.player.lock().unwrap();
 let r=(p.renderer_mut() as &mut dyn std::any::Any).downcast_mut::<renderer::SoftwareRenderer>().unwrap();
 r.allow_gpu=software==0;
 let dimensions=p.renderer_mut().viewport_dimensions();
 p.set_viewport_dimensions(dimensions);
}

/// Optional diagnostic counters; VM and collision times overlap.
#[repr(C)]#[derive(Default)]pub struct VmStats {
 pub vm_us:u64,pub collision_us:u64,pub gc_us:u64,pub opcodes:u64,
 pub gets:u64,pub sets:u64,pub calls:u64,pub hit_tests:u64,pub frames:u64,pub vm_entries:u64,
}
#[unsafe(no_mangle)]pub unsafe extern "C" fn rp_player_vm_stats(out:*mut VmStats)->i32 {
 if out.is_null(){return 0;}
 #[cfg(feature="profile-vm")] {
  let s=ruffle_core::puffle_profile::take();
  unsafe{*out=VmStats{vm_us:s.vm_us,collision_us:s.collision_us,gc_us:s.gc_us,opcodes:s.opcodes,gets:s.gets,sets:s.sets,calls:s.calls,hit_tests:s.hit_tests,frames:s.frames,vm_entries:s.vm_entries}};1
 }
 #[cfg(not(feature="profile-vm"))] {unsafe{*out=VmStats::default()};0}
}

/// Live vertex/index buffer payload, before and after lossless packing.
#[unsafe(no_mangle)]pub unsafe extern "C" fn rp_player_mesh_stats(p:*mut c_void,out:*mut u64){
 if p.is_null()||out.is_null(){return;}
 let mut p=unsafe{player(p)}.player.lock().unwrap();
 let r=(p.renderer_mut()as &mut dyn std::any::Any).downcast_mut::<renderer::SoftwareRenderer>().unwrap();
 let values=r.mesh_memory();unsafe{std::ptr::copy_nonoverlapping(values.as_ptr(),out,3)};
}
