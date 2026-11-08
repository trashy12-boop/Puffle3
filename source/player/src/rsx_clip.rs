//! Conservative integer rectangle masks for the RSX scissor path.
use ruffle_render::matrix::Matrix;
use tiny_skia::{Path,PathSegment};
#[derive(Clone,Copy,Debug,PartialEq,Eq)]
pub(super) struct Rect {pub left:i32,pub top:i32,pub right:i32,pub bottom:i32}
impl Rect {
 pub const FULL:Self=Self{left:0,top:0,right:320,bottom:240};
 pub const EMPTY:Self=Self{left:0,top:0,right:0,bottom:0};
 pub fn intersect(self,other:Self)->Self {Self{left:self.left.max(other.left),top:self.top.max(other.top),right:self.right.min(other.right),bottom:self.bottom.min(other.bottom)}}
 pub fn empty(self)->bool {self.left>=self.right||self.top>=self.bottom}
}
#[derive(Clone,Copy,Debug,PartialEq,Eq)]pub(super) enum Phase {Draw,Build,Erase}
struct Frame {parent:Rect,mask:Option<Rect>,phase:Phase}
pub(super) struct Masks {stack:Vec<Frame>,pub clip:Rect}
impl Masks {
 pub fn new()->Self {Self{stack:Vec::new(),clip:Rect::FULL}}
 pub fn phase(&self)->Phase {self.stack.last().map_or(Phase::Draw,|f|f.phase)}
 pub fn push(&mut self)->Result<(),u32>{
  if self.phase()!=Phase::Draw||self.stack.len()>=32{return Err(2)}
  self.stack.try_reserve(1).map_err(|_|2u32)?;
  self.stack.push(Frame{parent:self.clip,mask:None,phase:Phase::Build});Ok(())
 }
 pub fn rectangle(&mut self,r:Rect)->Result<(),u32>{
  let f=self.stack.last_mut().ok_or(2u32)?;
  if f.phase!=Phase::Build||f.mask.is_some(){return Err(2)}
  f.mask=Some(r);Ok(())
 }
 pub fn activate(&mut self)->Result<(),u32>{
  let f=self.stack.last_mut().ok_or(2u32)?;if f.phase!=Phase::Build{return Err(2)}
  self.clip=f.parent.intersect(f.mask.unwrap_or(Rect::EMPTY));f.phase=Phase::Draw;Ok(())
 }
 pub fn deactivate(&mut self)->Result<(),u32>{
  let f=self.stack.last_mut().ok_or(2u32)?;if f.phase!=Phase::Draw{return Err(2)}
  f.phase=Phase::Erase;Ok(())
 }
 pub fn pop(&mut self)->Result<(),u32>{
  if self.phase()!=Phase::Erase{return Err(2)}
  self.clip=self.stack.pop().ok_or(2u32)?.parent;Ok(())
 }
 pub fn balanced(&self)->bool {self.stack.is_empty()}
}
fn transformed(m:Matrix,x:f32,y:f32)->[f32;2]{[m.a*x+m.c*y+m.tx.to_pixels()as f32,m.b*x+m.d*y+m.ty.to_pixels()as f32]}
fn four_corners(p:&[[f32;2]])->Option<Rect>{
 if p.len()!=4||p.iter().flatten().any(|v|!v.is_finite()||v.fract()!=0.||v.abs()>32768.){return None}
 let left=p.iter().map(|p|p[0]as i32).min()?;let right=p.iter().map(|p|p[0]as i32).max()?;
 let top=p.iter().map(|p|p[1]as i32).min()?;let bottom=p.iter().map(|p|p[1]as i32).max()?;
 if left==right||top==bottom{return None}
 for i in 0..4 {
  let a=p[i];let b=p[(i+1)%4];
  if (a[0]==b[0])==(a[1]==b[1]){return None}
  if !(a[0]==left as f32||a[0]==right as f32)||!(a[1]==top as f32||a[1]==bottom as f32){return None}
  if p[..i].contains(&a){return None}
 }
 Some(Rect{left,top,right,bottom}.intersect(Rect::FULL))
}
pub(super) fn unit_rectangle(m:Matrix)->Option<Rect>{four_corners(&[[0.,0.],[1.,0.],[1.,1.],[0.,1.]].map(|p|transformed(m,p[0],p[1])))}
pub(super) fn path_rectangle(path:&Path,m:Matrix)->Option<Rect>{
 let mut points=[[0.;2];5];let mut n=0;let mut began=false;let mut ended=false;
 for segment in path.segments(){
  let p=match segment {
   PathSegment::MoveTo(p)=>{if began{return None}began=true;p},
   PathSegment::LineTo(p)=>{if !began||ended{return None}p},
   PathSegment::Close=>{ended=true;continue},
   _=>return None,
  };
  if n==points.len(){return None}points[n]=transformed(m,p.x,p.y);n+=1;
 }
 if n==5&&points[4]==points[0]{n-=1}
 four_corners(&points[..n])
}
#[cfg(test)]mod tests {
 use super::*;
 #[test]fn rectangles_allow_reflection_and_clip_to_viewport(){
  assert_eq!(unit_rectangle(Matrix::scale(400.,300.)),Some(Rect::FULL));
  let mut m=Matrix::scale(-40.,60.);m.tx=swf::Twips::from_pixels(50.);
  assert_eq!(unit_rectangle(m),Some(Rect{left:10,top:0,right:50,bottom:60}));
 }
 #[test]fn fractional_edges_and_shear_are_rejected(){
  assert!(unit_rectangle(Matrix::scale(10.5,20.)).is_none());
  let mut m=Matrix::scale(20.,20.);m.c=4.;assert!(unit_rectangle(m).is_none());
 }
 #[test]fn curves_and_multiple_contours_are_rejected(){
  let mut b=tiny_skia::PathBuilder::new();b.move_to(0.,0.);b.cubic_to(0.,0.,20.,0.,20.,20.);b.close();
  assert!(path_rectangle(&b.finish().unwrap(),Matrix::IDENTITY).is_none());
  let mut b=tiny_skia::PathBuilder::new();b.push_rect(tiny_skia::Rect::from_xywh(0.,0.,20.,20.).unwrap());b.push_rect(tiny_skia::Rect::from_xywh(2.,2.,5.,5.).unwrap());
  assert!(path_rectangle(&b.finish().unwrap(),Matrix::IDENTITY).is_none());
 }
 #[test]fn nested_masks_intersect_and_restore(){
  let a=Rect{left:10,top:20,right:80,bottom:90};let b=Rect{left:30,top:0,right:100,bottom:40};
  let mut m=Masks::new();m.push().unwrap();m.rectangle(a).unwrap();m.activate().unwrap();assert_eq!(m.clip,a);
  m.push().unwrap();m.rectangle(b).unwrap();m.activate().unwrap();assert_eq!(m.clip,a.intersect(b));
  m.deactivate().unwrap();assert_eq!(m.phase(),Phase::Erase);m.pop().unwrap();assert_eq!(m.clip,a);
  m.deactivate().unwrap();m.pop().unwrap();assert_eq!(m.clip,Rect::FULL);assert!(m.balanced());
 }
 #[test]fn malformed_and_multi_draw_masks_use_fallback(){
  let mut m=Masks::new();assert!(m.activate().is_err());m.push().unwrap();m.rectangle(Rect::FULL).unwrap();assert!(m.rectangle(Rect::FULL).is_err());assert!(m.push().is_err());assert!(!m.balanced());
 }
 #[test]fn empty_mask_remains_empty_and_cannot_escape_parent(){
  let mut m=Masks::new();m.push().unwrap();m.activate().unwrap();assert!(m.clip.empty());
  m.push().unwrap();m.rectangle(Rect::FULL).unwrap();m.activate().unwrap();assert!(m.clip.empty());
 }
}
