//! Bounded interning of exact mask contents. IDs are never reused.
use std::{collections::VecDeque,sync::Arc};
use tiny_skia::Mask;
#[derive(Clone)]
pub(super) struct Clip {pub id:u64,pub pixels:Arc<Mask>,pub bounds:Option<tiny_skia::IntRect>}
pub(super) struct MaskCache {entries:VecDeque<(u64,Clip)>,next:u64,bytes:usize}
const BUDGET:usize=512*1024;
fn hash(mask:&Mask)->u64 {
 let mut h=(u64::from(mask.width())<<32)|u64::from(mask.height());
 let mut chunks=mask.data().chunks_exact(8);
 for c in &mut chunks {h=h.rotate_left(7)^u64::from_ne_bytes(c.try_into().unwrap());}
 for &v in chunks.remainder(){h=h.rotate_left(7)^u64::from(v);}
 h
}
impl MaskCache {
 pub fn new()->Self{Self{entries:VecDeque::new(),next:1,bytes:0}}
 pub fn intern(&mut self,mask:Mask)->Clip {
  let h=hash(&mask);
  if let Some(index)=self.entries.iter().position(|(key,c)|*key==h&&c.pixels.width()==mask.width()&&c.pixels.height()==mask.height()&&c.pixels.data()==mask.data()) {
   let entry=self.entries.remove(index).unwrap();let clip=entry.1.clone();self.entries.push_back(entry);return clip;
  }
  // Bounds include every nonzero coverage sample, including antialiased
  // edges. Restrict snapshots only; preserve the original per-pixel mask.
  let mut left=mask.width();let mut top=mask.height();let mut right=0;let mut bottom=0;
  for (y,row) in mask.data().chunks_exact(mask.width() as usize).enumerate() {
   if let Some(x)=row.iter().position(|&v|v!=0) {
    left=left.min(x as u32);top=top.min(y as u32);
    right=right.max(row.iter().rposition(|&v|v!=0).unwrap() as u32+1);bottom=y as u32+1;
   }
  }
  let bounds=tiny_skia::IntRect::from_ltrb(left as i32,top as i32,right as i32,bottom as i32);
  let clip=Clip{id:self.next,pixels:Arc::new(mask),bounds};self.next=self.next.checked_add(1).expect("mask identity exhausted");
  let bytes=clip.pixels.data().len();
  if bytes<=BUDGET {
   while self.entries.len()>=4||self.bytes+bytes>BUDGET {if let Some((_,old))=self.entries.pop_front(){self.bytes-=old.pixels.data().len();}else{break}}
   self.bytes+=bytes;self.entries.push_back((h,clip.clone()));
  }
  clip
 }
}
#[cfg(test)]mod tests {
 use super::*;
 #[test]fn exact_masks_reuse_identity_and_evicted_ids_are_not_recycled(){
  let mut c=MaskCache::new();let a=c.intern(Mask::new(4,4).unwrap());assert_eq!(a.id,c.intern(Mask::new(4,4).unwrap()).id);
  for value in 1..6 {let mut m=Mask::new(4,4).unwrap();m.data_mut().fill(value);c.intern(m);}
  let b=c.intern(Mask::new(4,4).unwrap());assert_ne!(a.id,b.id);assert_eq!(a.pixels.data(),b.pixels.data());assert!(c.entries.len()<=4&&c.bytes<=BUDGET);
 }
 #[test]fn bounds_include_low_alpha_edges_and_empty_masks(){
  let mut c=MaskCache::new();assert!(c.intern(Mask::new(8,8).unwrap()).bounds.is_none());
  let mut m=Mask::new(8,8).unwrap();m.data_mut()[2*8+3]=1;m.data_mut()[4*8+5]=255;
  assert_eq!(c.intern(m).bounds,tiny_skia::IntRect::from_ltrb(3,2,6,5));
 }
 #[test]fn hash_collision_does_not_alias_different_masks(){
  let a=Mask::new(4,4).unwrap();let mut b=Mask::new(4,4).unwrap();b.data_mut()[..8].copy_from_slice(&1u64.to_ne_bytes());b.data_mut()[8..].copy_from_slice(&1u64.rotate_left(7).to_ne_bytes());assert_eq!(hash(&a),hash(&b));
  let mut c=MaskCache::new();assert_ne!(c.intern(a).id,c.intern(b).id);
 }
}
