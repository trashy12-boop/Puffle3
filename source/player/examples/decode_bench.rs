//! Isolated AVM1 parser workload; not a game or console FPS benchmark.
use std::{alloc::{GlobalAlloc,Layout,System},sync::atomic::{AtomicU64,Ordering},time::Instant,hint::black_box};
use swf::avm1::read::Reader;
struct Count; static ALLOCS:AtomicU64=AtomicU64::new(0);
unsafe impl GlobalAlloc for Count {
 unsafe fn alloc(&self,l:Layout)->*mut u8 {ALLOCS.fetch_add(1,Ordering::Relaxed);unsafe{System.alloc(l)}}
 unsafe fn dealloc(&self,p:*mut u8,l:Layout){unsafe{System.dealloc(p,l)}}
}
#[global_allocator]static A:Count=Count;
fn main(){
 for count in [0,1,2,3,32,128] {
  let length=count*5;let mut bytes=vec![0x96,length as u8,(length>>8) as u8];
  for i in 0..count {bytes.push(7);bytes.extend_from_slice(&(i as i32-17).to_le_bytes());}
  bytes.push(0x17);let mut reader=Reader::new(&bytes,8);
  let swf::avm1::types::Action::Push(push)=reader.read_action().unwrap() else {panic!()};
  assert_eq!(push.values.len(),count);assert_eq!(push.values.spilled(),count>2);
  for (i,value) in push.values.iter().enumerate(){assert_eq!(*value,swf::avm1::types::Value::Int(i as i32-17));}
  assert_eq!(reader.get_ref(),[0x17]);
 }
 println!("Push storage boundaries 0/1/2/3/32/128 values: passed");
 let bytes=[0x96,4,0,8,0,4,1];let n=2_000_000;
 for _ in 0..3 {
  ALLOCS.store(0,Ordering::Relaxed);let start=Instant::now();
  for _ in 0..n {let mut r=Reader::new(black_box(&bytes),8);black_box(r.read_action().unwrap());}
  println!("mode=inline-storage instructions={n} ms={:.3} allocations={}",start.elapsed().as_secs_f64()*1000.,ALLOCS.load(Ordering::Relaxed));
 }
}
