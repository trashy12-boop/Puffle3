//! Exact destination-dependent memoization. Never treats alpha draws as opaque.
use std::collections::{HashMap,VecDeque};
use tiny_skia::{IntRect,Pixmap};
#[derive(Clone,Copy,Debug,Hash,PartialEq,Eq)]
pub(super) struct Key {pub path:u64,pub clip:u64,pub matrix:[u32;6],pub colors:[i16;8],pub bitmap_version:u64,pub width:u32,pub height:u32}
struct Entry {rect:IntRect,before:Vec<u8>,after:Vec<u8>,serial:u64}
pub(super) struct DrawCache {entries:HashMap<Key,Entry>,order:VecDeque<(Key,u64)>,serial:u64,bytes:usize,budget:usize,pub hits:u64,pub misses:u64,probation:Vec<Option<Key>>}
const BUDGET:usize=8*1024*1024;
impl DrawCache {
 pub fn new()->Self {Self{entries:HashMap::new(),order:VecDeque::new(),serial:0,bytes:0,budget:BUDGET,hits:0,misses:0,probation:vec![None;4096]}}
 #[cfg(test)] pub fn entries_len(&self)->usize{self.entries.len()}
 pub fn set_budget(&mut self,budget:usize){
  self.budget=budget;
  while self.bytes>budget {if !self.evict_oldest(){break}}
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
 fn evict_oldest(&mut self)->bool {
  while let Some((key,serial))=self.order.pop_front() {
   if self.entries.get(&key).is_some_and(|e|e.serial==serial) {
    let old=self.entries.remove(&key).unwrap();
    self.bytes-=old.before.len()+old.after.len();return true;
   }
  }
  false
 }
 pub fn insert(&mut self,key:Key,rect:IntRect,before:Vec<u8>,p:&Pixmap){
  // Keep insertion order unique when an existing operation meets a new background.
  if let Some(old)=self.entries.remove(&key){self.bytes-=old.before.len()+old.after.len();}
  let after=Self::snapshot(p,rect);let cost=before.len()+after.len();
  if cost>self.budget{return;}
  while self.bytes+cost>self.budget||self.entries.len()>=1024 {
   if !self.evict_oldest(){break}
  }
  self.serial=self.serial.checked_add(1).expect("draw cache serial exhausted");
  let serial=self.serial;
  self.bytes+=cost;self.order.push_back((key,serial));self.entries.insert(key,Entry{rect,before,after,serial});
  // Replacements leave stale queue records. Compact only once enough have
  // accumulated, bounding bookkeeping to 2048 records with amortized O(1)
  // replacement instead of scanning up to 1024 keys for every cache miss.
  if self.order.len()>2048 {
   self.order.retain(|(key,serial)|self.entries.get(key).is_some_and(|e|e.serial==*serial));
  }
 }
}
#[cfg(test)] mod tests {
 use super::*;
 fn key(path:u64)->Key{Key{path,clip:0,matrix:[0;6],colors:[0;8],bitmap_version:0,width:4,height:4}}
 #[test]fn repeated_replacements_keep_newest_entry_and_bounded_queue(){
  let mut c=DrawCache::new();let mut p=Pixmap::new(4,4).unwrap();let r=IntRect::from_xywh(0,0,4,4).unwrap();
  for i in 0..10_000 {p.data_mut().fill((i%255)as u8);c.insert(key(1),r,vec![0;64],&p);}
  p.data_mut().fill(0);assert!(c.replay(&key(1),&mut p));assert_eq!(p.data()[0],(9999%255)as u8);
  assert_eq!(c.bytes(),128);assert_eq!(c.entries_len(),1);assert!(c.order.len()<=2048);
  c.set_budget(0);assert_eq!(c.bytes(),0);assert_eq!(c.entries_len(),0);
 }
 #[test]fn stale_records_cannot_evict_replacement_out_of_order(){
  let mut c=DrawCache::new();let p=Pixmap::new(4,4).unwrap();let r=IntRect::from_xywh(0,0,4,4).unwrap();c.set_budget(256);
  for id in [1,2,1,3] {c.insert(key(id),r,vec![0;64],&p);}
  assert!(c.entries.contains_key(&key(1)));assert!(c.entries.contains_key(&key(3)));assert!(!c.entries.contains_key(&key(2)));
  assert_eq!(c.bytes(),256);
 }
}
