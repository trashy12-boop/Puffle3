//! Exact destination-dependent memoization. Never treats alpha draws as opaque.
use std::collections::{HashMap,VecDeque};
use tiny_skia::{IntRect,Pixmap};
#[derive(Clone,Copy,Debug,Hash,PartialEq,Eq)]
pub(super) struct Key {pub path:u64,pub clip:u64,pub matrix:[u32;6],pub colors:[i16;8],pub bitmap_version:u64,pub width:u32,pub height:u32}
struct Entry {rect:IntRect,before:Vec<u8>,after:Vec<u8>}
pub(super) struct DrawCache {entries:HashMap<Key,Entry>,order:VecDeque<Key>,bytes:usize,budget:usize,pub hits:u64,pub misses:u64,probation:Vec<Option<Key>>}
const BUDGET:usize=8*1024*1024;
impl DrawCache {
 pub fn new()->Self {Self{entries:HashMap::new(),order:VecDeque::new(),bytes:0,budget:BUDGET,hits:0,misses:0,probation:vec![None;4096]}}
 #[cfg(test)] pub fn entries_len(&self)->usize{self.entries.len()}
 pub fn set_budget(&mut self,budget:usize){
  self.budget=budget;
  while self.bytes>budget {let Some(k)=self.order.pop_front()else{break};if let Some(old)=self.entries.remove(&k){self.bytes-=old.before.len()+old.after.len();}}
 }
 pub fn bytes(&self)->usize {self.bytes}
 pub fn replay(&mut self,k:&Key,p:&mut Pixmap)->bool {
  if let Some(e)=self.entries.get(k) {
   let stride=p.width() as usize*4;let n=e.rect.width() as usize*4;
   let equal=e.before.chunks_exact(n).enumerate().all(|(row,src)|{
    let offset=(e.rect.top() as usize+row)*stride+e.rect.left() as usize*4;
    &p.data()[offset..offset+n]==src
   });
   if equal {
    for (row,src) in e.after.chunks_exact(n).enumerate(){let offset=(e.rect.top() as usize+row)*stride+e.rect.left() as usize*4;p.data_mut()[offset..offset+n].copy_from_slice(src);}
    self.hits+=1;return true;
   }
  }
  self.misses+=1;false
 }
 // A one-off transform or morph should not allocate two pixel snapshots
 // and evict useful repeated draws. Require a second observation before
 // insertion; exact key equality keeps collisions a performance issue only.
 pub fn admit(&mut self,k:&Key)->bool {
  let mut hash=k.path^k.clip.rotate_left(19);
  for v in k.matrix {hash=hash.wrapping_mul(0x9e3779b185ebca87)^u64::from(v);}
  // Float encodings put most translation entropy in the high bits. Mix it
  // down before selecting a slot, or moving sprites collide systematically.
  hash=(hash^(hash>>30)).wrapping_mul(0xbf58476d1ce4e5b9);
  hash=(hash^(hash>>27)).wrapping_mul(0x94d049bb133111eb);
  let slot=((hash^(hash>>31)) as usize)&4095;
  if self.probation[slot].as_ref()==Some(k) {true}
  else {self.probation[slot]=Some(*k);false}
 }
 pub fn snapshot(p:&Pixmap,rect:IntRect)->Vec<u8>{
  let stride=p.width() as usize*4;let n=rect.width() as usize*4;let mut out=Vec::with_capacity(n*rect.height() as usize);
  for row in rect.top()..rect.bottom(){let offset=row as usize*stride+rect.left() as usize*4;out.extend_from_slice(&p.data()[offset..offset+n]);}out
 }
 pub fn insert(&mut self,key:Key,rect:IntRect,before:Vec<u8>,p:&Pixmap){
  // Keep insertion order unique when an existing operation meets a new background.
  if let Some(old)=self.entries.remove(&key){self.bytes-=old.before.len()+old.after.len();self.order.retain(|k|*k!=key);}
  let after=Self::snapshot(p,rect);let cost=before.len()+after.len();
  if cost>self.budget{return;}
  while self.bytes+cost>self.budget||self.entries.len()>=1024 {
   let Some(k)=self.order.pop_front()else{break};if let Some(old)=self.entries.remove(&k){self.bytes-=old.before.len()+old.after.len();}
  }
  self.bytes+=cost;self.order.push_back(key);self.entries.insert(key,Entry{rect,before,after});
 }
}
