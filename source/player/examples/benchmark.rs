//! Full-render, audio-enabled deterministic replay; never measures console FPS.
#[cfg(feature = "profile-alloc")]
#[path = "../benchmark/allocation_profile.rs"]
mod allocation_profile;
#[cfg(feature = "profile-alloc")]
#[global_allocator]
static ALLOCATOR: allocation_profile::CountingAllocator = allocation_profile::CountingAllocator;
use ruffle_ps3_player::*;
use std::{sync::Mutex,time::Instant,ffi::CString};
static FRAME:Mutex<Vec<u8>>=Mutex::new(Vec::new());
#[unsafe(no_mangle)] extern "C" fn rp_present(p:*const u8,w:u32,h:u32){let mut f=FRAME.lock().unwrap();f.clear();f.extend_from_slice(unsafe{std::slice::from_raw_parts(p,(w*h*4) as usize)});}
#[unsafe(no_mangle)] extern "C" fn rp_random(p:*mut u8,n:usize)->i32{getrandom::fill(unsafe{std::slice::from_raw_parts_mut(p,n)}).map(|_|0).unwrap_or(-1)}
#[unsafe(no_mangle)] extern "C" fn rp_storage_read(_: *const u8,_:usize,_:*mut u8,_:usize)->i64{-1}
#[unsafe(no_mangle)] extern "C" fn rp_storage_write(_: *const u8,_:usize,_:*const u8,_:usize)->i32{0}
#[unsafe(no_mangle)] extern "C" fn rp_storage_remove(_: *const u8,_:usize){}
unsafe extern "C" {fn rp_benchmark_clock(ns:u64,active:i32);}
fn main(){unsafe{run()}}
unsafe fn run(){
 let a:Vec<String>=std::env::args().collect();assert!(a.len()>=5,"benchmark SWF FRAMES cache|scalar OUTPUT_PREFIX [replay.txt]");
 #[cfg(feature="host-rsx-probe")] rsx_probe::init();
 let bytes=std::fs::read(&a[1]).unwrap();let frames:usize=a[2].parse().unwrap();let cache=a[3]!="scalar";let prefix=&a[4];
 let movie=ruffle_core::tag_utils::SwfMovieData::from_data(&bytes,"file:///benchmark.swf".into(),None,None).unwrap();let fps=movie.frame_rate().to_f64();
 let path=CString::new(a[1].clone()).unwrap();unsafe{rp_benchmark_clock(0,1)};let p=unsafe{rp_player_open(bytes.as_ptr(),bytes.len(),320,240,path.as_ptr())};unsafe{rp_benchmark_clock(0,0)};assert!(!p.is_null());unsafe{benchmark_renderer(p,cache,true);benchmark_verify(p,a[3]=="verify")};
 let events=if a.len()>5{std::fs::read_to_string(&a[5]).unwrap()}else{String::new()};
 let events:Vec<Vec<&str>>=events.lines().filter(|l|!l.trim().is_empty()&&!l.starts_with('#')).map(|l|l.split_whitespace().collect()).collect();
 let mut out=String::from("frame,logic_ms,render_ms,audio_ms,path_ms,path_calls,hits,misses,cache_bytes\n");
 #[cfg(feature="host-rsx-probe")] let mut rsx_file={use std::io::Write;let mut f=std::fs::File::create(format!("{prefix}-rsx.csv")).unwrap();writeln!(f,"frame,accepted,input_draws,vertices,uploads,reused,upload_bytes,failed_draws,batches,merged,mask_fallback,offscreen_entries,stream_hash").unwrap();f};
 #[cfg(feature="profile-vm")] let mut vm_file={use std::io::Write;let mut f=std::fs::File::create(format!("{prefix}-vm.csv")).unwrap();writeln!(f,"frame,vm_us,collision_us,gc_us,opcodes,gets,sets,calls,hit_tests,sim_frames,vm_entries").unwrap();f};
 #[cfg(feature="profile-alloc")] let mut alloc_file={use std::io::Write;let mut f=std::fs::File::create(format!("{prefix}-alloc.csv")).unwrap();writeln!(f,"frame,logic_alloc_requests,logic_requested_bytes,live_requested_bytes,peak_requested_bytes").unwrap();f};
 let mut totals=[0f64;4];let mut pcm=vec![0f32;((48000./fps) as usize)*2];let mut peak=0f32;
 for frame in 0..frames{
  let movie_ns=(frame as f64*1e9/fps) as u64;unsafe{rp_benchmark_clock(movie_ns,1)};
  for e in &events{if e[0].parse::<usize>().unwrap()!=frame{continue;}let event=Event{kind:e[1].parse().unwrap(),code:e[2].parse().unwrap(),down:e[3]=="1",x:e[4].parse().unwrap(),y:e[5].parse().unwrap()};unsafe{rp_player_event(p,&event)};}
  unsafe{rp_benchmark_clock(movie_ns,0)};
  #[cfg(feature="profile-alloc")] let allocations_before=allocation_profile::snapshot();
  let start=Instant::now();unsafe{rp_benchmark_clock(movie_ns,1);benchmark_frame(p,1000./fps);rp_benchmark_clock(movie_ns,0)};let tick=start.elapsed().as_secs_f64()*1000.;
  #[cfg(feature="profile-alloc")] {use std::io::Write;let a=allocation_profile::snapshot();writeln!(alloc_file,"{frame},{},{},{},{}",a[0]-allocations_before[0],a[1]-allocations_before[1],a[2],a[3]).unwrap();}
  let start=Instant::now();unsafe{rp_player_render(p)};let render=start.elapsed().as_secs_f64()*1000.;
  #[cfg(feature="host-rsx-probe")] rsx_probe::frame(&mut rsx_file,frame);
  let start=Instant::now();for chunk in pcm.chunks_mut(8192){unsafe{rp_player_mix(p,chunk.as_mut_ptr(),chunk.len())}}let audio=start.elapsed().as_secs_f64()*1000.;
  peak=pcm.iter().copied().map(f32::abs).fold(peak,f32::max);
  #[cfg(feature="profile-vm")] {use std::io::Write;let mut s=VmStats::default();unsafe{rp_player_vm_stats(&mut s)};writeln!(vm_file,"{frame},{},{},{},{},{},{},{},{},{},{}",s.vm_us,s.collision_us,s.gc_us,s.opcodes,s.gets,s.sets,s.calls,s.hit_tests,s.frames,s.vm_entries).unwrap();}
  let stats=unsafe{benchmark_renderer(p,cache,true)};let path_ms=stats.path_ns as f64/1e6;
  for (total,value) in totals.iter_mut().zip([tick,render,audio,path_ms]){*total+=value;}
  out.push_str(&format!("{frame},{tick:.6},{render:.6},{audio:.6},{path_ms:.6},{},{},{},{}\n",stats.path_calls,stats.cache_hits,stats.cache_misses,stats.cache_bytes));
  if frame%30==0||frame+1==frames{std::fs::write(format!("{prefix}-{frame:04}.rgba"),&*FRAME.lock().unwrap()).unwrap();}
 }
 std::fs::write(format!("{prefix}.csv"),out).unwrap();eprintln!("HOST_FULL_RENDER frames={frames} declared_fps={fps} logic_ms={:.3} render_ms={:.3} audio_ms={:.3} path_ms={:.3} pcm_peak={peak}",totals[0],totals[1],totals[2],totals[3]);unsafe{rp_player_close(p)};
 #[cfg(feature="host-rsx-probe")] rsx_probe::report();
}

// Real C submission path with a host command recorder; never measures GPU FPS.
#[cfg(feature="host-rsx-probe")]
mod rsx_probe {
 use std::io::Write;
 unsafe extern "C" {fn rp_rsx_init()->i32;fn rp_rsx_shutdown();fn rp_rsx_profile_snapshot(p:*mut u64);fn rp_rsx_reason_stats(p:*mut u64);fn rp_rsx_current()->i32;fn rp_probe_stream_hash()->u64;}
 pub fn init(){assert_ne!(unsafe{rp_rsx_init()},0)}
 pub fn frame(file:&mut std::fs::File,frame:usize){let mut s=[0u64;15];let mut r=[0u64;9];unsafe{rp_rsx_profile_snapshot(s.as_mut_ptr());rp_rsx_reason_stats(r.as_mut_ptr());}writeln!(file,"{frame},{},{},{},{},{},{},{},{},{},{},{},{}",unsafe{rp_rsx_current()},s[0],s[1],s[2],s[3],s[4],s[8],s[13],s[14],r[2],r[4],unsafe{rp_probe_stream_hash()}).unwrap();}
 pub fn report(){unsafe{rp_rsx_shutdown()};}
}
