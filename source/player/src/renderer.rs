//! CPU vector renderer, uploaded through RSX. Uses actual Ruffle command lists.
//! Unsupported: Stage3D, PixelBender, filters and exact Flash advanced blending.
use ruffle_render::{backend::*, bitmap::*, commands::*, matrix::Matrix,
    shape_utils::{DistilledShape,DrawPath,DrawCommand,FillRule}, transform::Transform};
use std::{any::Any,cell::RefCell,sync::{Arc,atomic::{AtomicU64,Ordering}}};
use tiny_skia as sk;
use swf::{Color,FillStyle};
#[path="draw_cache.rs"] mod draw_cache;
use draw_cache::{DrawCache,Key};
#[path="mask_cache.rs"] mod mask_cache;
#[cfg(any(test,any(all(target_os="ps3",feature="ps3-rsx"),feature="host-rsx-probe")))]
#[path="rsx_clip.rs"] mod rsx_clip;
use mask_cache::{Clip,MaskCache};
use std::cell::Cell;
#[derive(Clone,Copy,Debug,Default)] pub struct RenderStats {pub path_ns:u64,pub bitmap_ns:u64,pub path_calls:u64,pub cache_hits:u64,pub cache_misses:u64,pub cache_bytes:usize}

#[derive(Debug)] struct Shape(Vec<Path>, #[cfg(any(all(target_os="ps3",feature="ps3-rsx"),feature="host-rsx-probe"))] Option<rsx_backend::GpuShape>);
impl ShapeHandleImpl for Shape {}
#[derive(Debug)] struct Path { shader:RefCell<Option<Box<(swf::ColorTransform,sk::Shader<'static>)>>>, id:u64, geometry:sk::Path, style:FillStyle, bitmap:Option<BitmapHandle>, stroke:Option<sk::Stroke>, rule:sk::FillRule }
#[derive(Debug)] struct Image(RefCell<sk::Pixmap>,Cell<u64>,u64);
/* Never reuse an RSX texture identity when a bitmap's allocation is freed.
 * The generation counter (.1) invalidates textures updated in-place. */
static NEXT_BITMAP_ID: AtomicU64 = AtomicU64::new(1);
fn new_image(p:sk::Pixmap)->Image {
 Image(RefCell::new(p),Cell::new(0),NEXT_BITMAP_ID.fetch_add(1,Ordering::Relaxed))
}
impl BitmapHandleImpl for Image {}
#[derive(Debug)] struct Readback(Vec<u8>,u32);
impl SyncHandle for Readback {}
struct MaskFrame { saved:Option<sk::Pixmap>, parent:Option<Clip> }
pub struct SoftwareRenderer { dimensions:ViewportDimensions, pub pixels:sk::Pixmap, clip:Option<Clip>, mask_cache:MaskCache, masks:Vec<MaskFrame>, clearing:bool, cache:DrawCache, pub cache_enabled:bool, pub allow_gpu:bool, next_path:u64, #[cfg(any(all(target_os="ps3",feature="ps3-rsx"),feature="host-rsx-probe"))] gpu_tessellator:ruffle_render::tessellator::ShapeTessellator, pub stats:RenderStats, pub profile:bool, #[cfg(not(target_os="ps3"))] pub validate_cache:bool }
#[cfg(any(all(target_os="ps3",feature="ps3-rsx"),feature="host-rsx-probe"))]
#[path="rsx_backend.rs"] mod rsx_backend;
fn image(b:&BitmapHandle)->&Image { (b.0.as_ref() as &dyn Any).downcast_ref().unwrap() }
fn matrix(m:Matrix)->sk::Transform {sk::Transform::from_row(m.a,m.b,m.c,m.d,m.tx.to_pixels() as f32,m.ty.to_pixels() as f32)}
fn color(c:Color)->sk::Color {sk::Color::from_rgba8(c.r,c.g,c.b,c.a)}
fn bounds(w:u32,h:u32)->Option<sk::Rect>{sk::Rect::from_xywh(0.,0.,w as f32,h as f32)}
impl SoftwareRenderer {
 pub fn trim_for_memory_pressure(&mut self){self.cache.set_budget(2*1024*1024);}

 pub fn new(w:u32,h:u32)->Self {Self {dimensions:ViewportDimensions{width:w,height:h,scale_factor:1.},pixels:sk::Pixmap::new(w,h).unwrap(),clip:None,mask_cache:MaskCache::new(),masks:Vec::new(),clearing:false,cache:DrawCache::new(),cache_enabled:true,allow_gpu:true,next_path:1,#[cfg(any(all(target_os="ps3",feature="ps3-rsx"),feature="host-rsx-probe"))]gpu_tessellator:ruffle_render::tessellator::ShapeTessellator::new(),stats:RenderStats::default(),profile:false,#[cfg(not(target_os="ps3"))]validate_cache:false}}
 fn paint_path(&mut self,p:&Path,t:&Transform) {
  if self.clearing || self.clip.as_ref().is_some_and(|c|c.bounds.is_none()) {return;}
  let start=self.profile.then(std::time::Instant::now);
  self.stats.path_calls+=1;
  let candidate=if self.cache_enabled&&self.masks.last().is_none_or(|m|m.saved.is_none()){self.cache_region(p,t)}else{None};
  if let Some((key,_))=&candidate {if self.cache.replay(key,&mut self.pixels){self.stats.cache_hits+=1;self.stats.cache_bytes=self.cache.bytes();if let Some(start)=start{self.stats.path_ns+=start.elapsed().as_nanos() as u64;}return;}self.stats.cache_misses+=1;}
  let candidate=candidate.filter(|(key,r)|r.width() as u64*r.height() as u64*4<16*1024 || self.cache.admit(key));
  let before=candidate.as_ref().map(|(_,r)|DrawCache::snapshot(&self.pixels,*r));
  self.paint_path_uncached(p,t);
  if let (Some((key,rect)),Some(before))=(candidate,before){self.cache.insert(key,rect,before,&self.pixels);}
  self.stats.cache_bytes=self.cache.bytes();
  if let Some(start)=start{self.stats.path_ns+=start.elapsed().as_nanos() as u64;}
 }
 fn cache_region(&self,p:&Path,t:&Transform)->Option<(Key,sk::IntRect)>{
  let m=matrix(t.matrix);let mut b=p.geometry.bounds();
  if let Some(stroke)=&p.stroke {
   // Enclose joins, square/round caps and tiny-skia's hairline expansion.
   let pad=(stroke.width*stroke.miter_limit.max(2.)).max(2.);
   b=b.outset(pad,pad)?;
  }
  let b=b.transform(m)?.outset(2.,2.)?;
  let left=b.left().floor().max(0.).min(self.pixels.width() as f32) as i32;
  let top=b.top().floor().max(0.).min(self.pixels.height() as f32) as i32;
  let right=b.right().ceil().max(0.).min(self.pixels.width() as f32) as i32;
  let bottom=b.bottom().ceil().max(0.).min(self.pixels.height() as f32) as i32;
  let (left,top,right,bottom)=if let Some(clip)=&self.clip {
   let b=clip.bounds?;(left.max(b.left()),top.max(b.top()),right.min(b.right()),bottom.min(b.bottom()))
  }else{(left,top,right,bottom)};
  let rect=sk::IntRect::from_ltrb(left,top,right,bottom)?;
  let bytes=rect.width() as u64*rect.height() as u64*4;
  if !(64..=512*1024).contains(&bytes){return None;}
  let c=t.color_transform;
  Some((Key{path:p.id,clip:self.clip.as_ref().map_or(0,|c|c.id),matrix:[m.sx.to_bits(),m.ky.to_bits(),m.kx.to_bits(),m.sy.to_bits(),m.tx.to_bits(),m.ty.to_bits()],colors:[c.r_multiply.get(),c.g_multiply.get(),c.b_multiply.get(),c.a_multiply.get(),c.r_add,c.g_add,c.b_add,c.a_add],bitmap_version:p.bitmap.as_ref().map_or(0,|b|image(b).1.get()),width:self.pixels.width(),height:self.pixels.height()},rect))
 }
 fn paint_path_uncached(&mut self,p:&Path,t:&Transform) {
  if self.clearing {return;}
  let ct=&t.color_transform;
  let mut paint=sk::Paint::default();paint.anti_alias=true;
  let bitmap=p.bitmap.as_ref().map(|b|image(b).0.borrow());
  let drawing_mask=self.masks.last().is_some_and(|m|m.saved.is_some());
  if drawing_mask {paint.set_color_rgba8(255,255,255,255);} else {
   match &p.style {
    FillStyle::Color(c)=>paint.set_color(color(ct**c)),
    FillStyle::LinearGradient(g)|FillStyle::RadialGradient(g)|FillStyle::FocalGradient{gradient:g,..}=> {
     // Local-space gradient shaders do not depend on the display-list matrix.
     // Keep one color generation per immutable path; tiny-skia shares stop
     // storage across the temporary paint clones made by its rasterizer.
     let mut cached=p.shader.borrow_mut();
     if cached.as_ref().is_none_or(|entry|entry.0!=*ct) {
     let stops=g.records.iter().map(|r|sk::GradientStop::new(r.ratio as f32/255.,color(ct*r.color))).collect();
     let spread=match g.spread {swf::GradientSpread::Pad=>sk::SpreadMode::Pad,swf::GradientSpread::Repeat=>sk::SpreadMode::Repeat,_=>sk::SpreadMode::Reflect};
     let m:Matrix=g.matrix.into();
     // Gradients use coordinates in twips, whereas cached paths use pixels.
     let gm=sk::Transform::from_row(m.a/20.,m.b/20.,m.c/20.,m.d/20.,m.tx.to_pixels() as f32,m.ty.to_pixels() as f32);
     let shader=if matches!(p.style,FillStyle::LinearGradient(_)) {sk::LinearGradient::new(sk::Point::from_xy(-16384.,0.),sk::Point::from_xy(16384.,0.),stops,spread,gm)} else {sk::RadialGradient::new(sk::Point::from_xy(0.,0.),sk::Point::from_xy(0.,0.),16384.,stops,spread,gm)};
     if let Some(s)=shader {*cached=Some(Box::new((*ct,s)));} else{return;}
     }
     paint.shader=cached.as_ref().unwrap().1.clone();
    }
    FillStyle::Bitmap{matrix:m,is_repeating,is_smoothed,..}=> {
     let Some(ref b)=bitmap else{return;};let m:Matrix=(*m).into();
     let bm=sk::Transform::from_row(m.a/20.,m.b/20.,m.c/20.,m.d/20.,m.tx.to_pixels() as f32,m.ty.to_pixels() as f32);
     paint.shader=sk::Pattern::new(b.as_ref(),if *is_repeating {sk::SpreadMode::Repeat}else{sk::SpreadMode::Pad},if *is_smoothed {sk::FilterQuality::Bilinear}else{sk::FilterQuality::Nearest},ct.a_multiply.to_f32().clamp(0.,1.),bm);
    }
   }
  }
  if let Some(stroke)=&p.stroke {self.pixels.stroke_path(&p.geometry,&paint,stroke,matrix(t.matrix),self.clip.as_ref().map(|c|c.pixels.as_ref()));}
  else {self.pixels.fill_path(&p.geometry,&paint,p.rule,matrix(t.matrix),self.clip.as_ref().map(|c|c.pixels.as_ref()));}
 }
 fn execute(&mut self,c:CommandList) {c.execute(self)}
 fn rect(&mut self,c:Color,m:Matrix,stroke:bool) {if self.clearing{return;} let mut paint=sk::Paint::default();paint.set_color(color(c));let path=sk::PathBuilder::from_rect(bounds(1,1).unwrap());if stroke {self.pixels.stroke_path(&path,&paint,&sk::Stroke::default(),matrix(m),self.clip.as_ref().map(|c|c.pixels.as_ref()));}else{self.pixels.fill_path(&path,&paint,sk::FillRule::Winding,matrix(m),self.clip.as_ref().map(|c|c.pixels.as_ref()));}}
}
impl CommandHandler for SoftwareRenderer {
 fn render_shape(&mut self,s:ShapeHandle,t:Transform){let s=(s.0.as_ref() as &dyn Any).downcast_ref::<Shape>().unwrap();for p in &s.0 {self.paint_path(p,&t)}}
 fn render_bitmap(&mut self,b:BitmapHandle,mut t:Transform,smooth:bool,snap:PixelSnapping,r:PixelRegion){
  if self.clearing{return;}let started=self.profile.then(std::time::Instant::now);snap.apply(&mut t.matrix);
  let b=image(&b).0.borrow();
  let copy=bitmap_pixels(&b,&t.color_transform);
  let mut paint=sk::Paint::default();paint.shader=sk::Pattern::new(copy.as_ref().as_ref(),sk::SpreadMode::Pad,if smooth{sk::FilterQuality::Bilinear}else{sk::FilterQuality::Nearest},1.,sk::Transform::from_translate(-(r.x_min as f32),-(r.y_min as f32)));
  if let Some(rect)=bounds(r.width(),r.height()){self.pixels.fill_rect(rect,&paint,matrix(t.matrix),self.clip.as_ref().map(|c|c.pixels.as_ref()));}if let Some(start)=started{self.stats.bitmap_ns+=start.elapsed().as_nanos() as u64;}
 }
 fn render_stage3d(&mut self,_:BitmapHandle,_:Transform){}
 fn draw_rect(&mut self,c:Color,m:Matrix){self.rect(c,m,false)}
 fn draw_line_rect(&mut self,c:Color,m:Matrix){self.rect(c,m,true)}
 fn draw_line(&mut self,c:Color,m:Matrix){if self.clearing{return;} let mut pb=sk::PathBuilder::new();pb.move_to(0.,0.);pb.line_to(1.,0.);if let Some(path)=pb.finish(){let mut paint=sk::Paint::default();paint.set_color(color(c));self.pixels.stroke_path(&path,&paint,&sk::Stroke::default(),matrix(m),self.clip.as_ref().map(|c|c.pixels.as_ref()));}}
 fn push_mask(&mut self){let blank=sk::Pixmap::new(self.pixels.width(),self.pixels.height()).unwrap();let saved=std::mem::replace(&mut self.pixels,blank);self.masks.push(MaskFrame{saved:Some(saved),parent:self.clip.take()});}
 fn activate_mask(&mut self){if let Some(m)=self.masks.last_mut(){let mut clip=sk::Mask::from_pixmap(self.pixels.as_ref(),sk::MaskType::Alpha);if let Some(parent)=&m.parent {for (v,p) in clip.data_mut().iter_mut().zip(parent.pixels.data()){*v=(*v as u32**p as u32/255) as u8;}}if let Some(saved)=m.saved.take(){self.pixels=saved;}self.clip=Some(self.mask_cache.intern(clip));}}
 fn deactivate_mask(&mut self){self.clearing=true;}
 fn pop_mask(&mut self){if let Some(m)=self.masks.pop(){self.clip=m.parent;}self.clearing=false;}
 fn blend(&mut self,c:CommandList,_:RenderBlendMode){self.execute(c)}
 fn render_alpha_mask(&mut self,c:CommandList,m:CommandList){self.push_mask();self.execute(m);self.activate_mask();self.execute(c);self.pop_mask();}
}

use std::{borrow::Cow,num::NonZeroU32};
// Ordinary sprites borrow decoded pixels; transformed sprites keep the full
// color operation, including additive alpha, unchanged.
pub(crate) fn bitmap_pixels<'a>(b:&'a sk::Pixmap,ct:&swf::ColorTransform)->Cow<'a,sk::Pixmap> {
 if *ct==swf::ColorTransform::IDENTITY {return Cow::Borrowed(b);}
 Cow::Owned(transform_bitmap(b,ct))
}
pub(crate) fn transform_bitmap(b:&sk::Pixmap,ct:&swf::ColorTransform)->sk::Pixmap {
  let mut copy=b.clone();
  for px in copy.data_mut().chunks_exact_mut(4) {
   let a=px[3] as u32;
   let raw=if a==0 {Color{r:0,g:0,b:0,a:0}}else{Color{r:(px[0] as u32*255/a).min(255) as u8,g:(px[1] as u32*255/a).min(255) as u8,b:(px[2] as u32*255/a).min(255) as u8,a:px[3]}};
   let c=ct*raw;px[0]=(c.r as u32*c.a as u32/255) as u8;px[1]=(c.g as u32*c.a as u32/255) as u8;px[2]=(c.b as u32*c.a as u32/255) as u8;px[3]=c.a;
  }
 copy
}

use ruffle_render::{error::Error,quality::StageQuality,pixel_bender::{PixelBenderShader,PixelBenderShaderHandle},pixel_bender_support::PixelBenderShaderArgument};
unsafe extern "C" {fn rp_present(p:*const u8,w:u32,h:u32);}
impl SoftwareRenderer {
 fn offscreen(&mut self,h:BitmapHandle,c:CommandList,clear:Color){
  let (w,height)={let p=image(&h).0.borrow();(p.width(),p.height())};
  let Some(mut target)=sk::Pixmap::new(w,height)else{return;};target.fill(color(clear));
  let saved=std::mem::replace(&mut self.pixels,target);let clip=self.clip.take();let masks=std::mem::take(&mut self.masks);let clearing=self.clearing;self.clearing=false;
  self.execute(c);let rendered=std::mem::replace(&mut self.pixels,saved);*image(&h).0.borrow_mut()=rendered;image(&h).1.set(image(&h).1.get().wrapping_add(1));self.clip=clip;self.masks=masks;self.clearing=clearing;
 }
}
impl RenderBackend for SoftwareRenderer {
    fn is_offscreen_supported(&self)->bool { true }
    fn viewport_dimensions(&self) -> ViewportDimensions {
        self.dimensions
    }
    fn set_viewport_dimensions(&mut self, dimensions: ViewportDimensions) {
        self.dimensions = dimensions;
    }
    fn register_shape(&mut self,shape:DistilledShape,source:&dyn BitmapSource)->ShapeHandle {
 #[cfg(any(all(target_os="ps3",feature="ps3-rsx"),feature="host-rsx-probe"))]
 let mesh=if rsx_backend::available(){Some(self.gpu_tessellator.tessellate_shape_with_scale(shape.clone(),source,2.0))}else{None};
 let mut paths=Vec::new();
 for path in shape.paths {
  let (commands,style,stroke,rule,closed)=match path {
   DrawPath::Fill{style,commands,winding_rule}=>(commands,style.clone(),None,match winding_rule {FillRule::EvenOdd=>sk::FillRule::EvenOdd,_=>sk::FillRule::Winding},true),
   DrawPath::Stroke{style,commands,is_closed}=>{let mut st=sk::Stroke::default();st.width=(style.width().to_pixels() as f32).max(1.);st.line_cap=match style.start_cap(){swf::LineCapStyle::Round=>sk::LineCap::Round,swf::LineCapStyle::Square=>sk::LineCap::Square,_=>sk::LineCap::Butt};(commands,style.fill_style().clone(),Some(st),sk::FillRule::Winding,is_closed)}
  };
  let bitmap=if let FillStyle::Bitmap{id,..}=&style {source.bitmap_handle(*id,self)}else{None};
  let mut pb=sk::PathBuilder::new();let mut has=false;
  for c in commands { match c {
   DrawCommand::MoveTo(p)=>{if has && closed {pb.close();}pb.move_to(p.x.to_pixels() as f32,p.y.to_pixels() as f32);has=true;},
   DrawCommand::LineTo(p)=>pb.line_to(p.x.to_pixels() as f32,p.y.to_pixels() as f32),
   DrawCommand::QuadraticCurveTo{control:c,anchor:p}=>pb.quad_to(c.x.to_pixels() as f32,c.y.to_pixels() as f32,p.x.to_pixels() as f32,p.y.to_pixels() as f32),
   DrawCommand::CubicCurveTo{control_a:a,control_b:b,anchor:p}=>pb.cubic_to(a.x.to_pixels() as f32,a.y.to_pixels() as f32,b.x.to_pixels() as f32,b.y.to_pixels() as f32,p.x.to_pixels() as f32,p.y.to_pixels() as f32)
  }}
  if closed {pb.close();}if let Some(geometry)=pb.finish(){let id=self.next_path;self.next_path+=1;paths.push(Path{shader:RefCell::new(None),id,geometry,style,bitmap,stroke,rule});}
 }
 #[cfg(any(all(target_os="ps3",feature="ps3-rsx"),feature="host-rsx-probe"))]
 let shape={let gpu=mesh.map(|m|rsx_backend::GpuShape::new(m,&paths));Shape(paths,gpu)};
 #[cfg(not(any(all(target_os="ps3",feature="ps3-rsx"),feature="host-rsx-probe")))]
 let shape=Shape(paths);
 ShapeHandle(Arc::new(shape))
 }

    fn render_offscreen(&mut self,h:BitmapHandle,c:CommandList,_:StageQuality,_:PixelRegion)->Option<Box<dyn SyncHandle>> {
 self.offscreen(h.clone(),c,Color::TRANSPARENT);let p=image(&h).0.borrow();Some(Box::new(Readback(p.data().to_vec(),p.width())))
 }

    fn submit_frame(&mut self,clear:Color,commands:CommandList,cache_entries:Vec<BitmapCacheEntry>) {
 // Bitmap-cache entries must be rendered before any consumer draws them.
 // Previously their mere presence bypassed RSX for the entire scene, even
 // if the final command list contained only GPU-supported operations.
 #[cfg(any(all(target_os="ps3",feature="ps3-rsx"),feature="host-rsx-probe"))]
 let had_offscreen_cache = !cache_entries.is_empty();
 for c in cache_entries {self.offscreen(c.handle,c.commands,c.clear);}
 #[cfg(any(all(target_os="ps3",feature="ps3-rsx"),feature="host-rsx-probe"))]
 {
  if had_offscreen_cache { rsx_backend::note_cache(); }
  if self.allow_gpu && rsx_backend::render_frame(clear,&commands) {return;}
 }
 self.pixels.fill(color(clear));self.clip=None;self.masks.clear();self.clearing=false;
 #[cfg(not(target_os="ps3"))]
 let reference=if self.validate_cache {
  let enabled=self.cache_enabled;self.cache_enabled=false;
  let stats=self.stats;self.execute(commands.clone());self.stats=stats;
  let pixels=self.pixels.data().to_vec();self.cache_enabled=enabled;
  self.pixels.fill(color(clear));self.clip=None;self.masks.clear();self.clearing=false;Some(pixels)
 }else{None};
 self.execute(commands);
 #[cfg(not(target_os="ps3"))]
 if let Some(reference)=reference {assert_eq!(reference,self.pixels.data(),"cached frame differs from identical uncached command list");}
 unsafe {rp_present(self.pixels.data().as_ptr(),self.pixels.width(),self.pixels.height());}
 }
    fn register_bitmap(&mut self,b:Bitmap<'_>)->Result<BitmapHandle,Error> {
 if b.width()>4096||b.height()>4096||b.width() as u64*b.height() as u64>4*1024*1024{return Err(Error::Unimplemented("bitmap exceeds PS3 limit".into()));}
 let b=b.to_rgba();let p=sk::Pixmap::from_vec(b.data().to_vec(),sk::IntSize::from_wh(b.width(),b.height()).ok_or_else(||Error::Unimplemented("empty bitmap".into()))?).ok_or_else(||Error::Unimplemented("bitmap allocation".into()))?;
 Ok(BitmapHandle(Arc::new(new_image(p))))
 }

    fn update_texture(&mut self,h:&BitmapHandle,b:Bitmap<'_>,_:PixelRegion)->Result<(),Error> {
 let b=b.to_rgba();let mut p=image(h).0.borrow_mut();if b.width()!=p.width()||b.height()!=p.height(){return Err(Error::Unimplemented("bitmap dimensions".into()));}p.data_mut().copy_from_slice(b.data());image(h).1.set(image(h).1.get().wrapping_add(1));Ok(())
 }

    fn create_context3d(
        &mut self,
        _profile: Context3DProfile,
    ) -> Result<Box<dyn Context3D>, Error> {
        Err(Error::Unimplemented("createContext3D".into()))
    }

    fn debug_info(&self) -> Cow<'static, str> {
        Cow::Borrowed("PS3 CPU vector renderer / RSX presentation")
    }

    fn name(&self) -> &'static str {
        "ps3-software"
    }

    fn set_quality(&mut self, _quality: StageQuality) {}

    fn run_pixelbender_shader(
        &mut self,
        _shader: PixelBenderShaderHandle,
        _arguments: &[PixelBenderShaderArgument],
        _target: &PixelBenderTarget,
    ) -> Result<PixelBenderOutput, Error> {
        Err(Error::Unimplemented("Pixel bender shader".into()))
    }

    fn resolve_sync_handle(&mut self,h:Box<dyn SyncHandle>,read:RgbaBufRead)->Result<(),Error> {
 let p=(&*h as &dyn Any).downcast_ref::<Readback>().ok_or_else(||Error::Unimplemented("readback".into()))?;read(&p.0,p.1);Ok(())
 }

    fn compile_pixelbender_shader(
        &mut self,
        _shader: PixelBenderShader,
    ) -> Result<PixelBenderShaderHandle, Error> {
        Err(Error::Unimplemented(
            "Pixel bender shader compilation".into(),
        ))
    }

    fn create_empty_texture(&mut self,w:NonZeroU32,h:NonZeroU32)->Result<BitmapHandle,Error> {
 if w.get()>4096||h.get()>4096||w.get() as u64*h.get() as u64>4*1024*1024{return Err(Error::Unimplemented("texture exceeds PS3 limit".into()));}
 sk::Pixmap::new(w.get(),h.get()).map(|p|BitmapHandle(Arc::new(new_image(p)))).ok_or_else(||Error::Unimplemented("texture allocation".into()))
 }
}

#[cfg(test)] mod tests {
 use super::*;
 #[test] fn nested_mask_clips_and_pop_restores_parent(){
  let mut r=SoftwareRenderer::new(16,16);
  let rect=|x:f32,w:f32|{let mut m=Matrix::scale(w,16.);m.tx=swf::Twips::from_pixels(x as f64);m};
  r.push_mask();r.draw_rect(Color::from_rgb(0xffffff,255),rect(0.,8.));r.activate_mask();
  r.push_mask();r.draw_rect(Color::from_rgb(0xffffff,255),rect(4.,8.));r.activate_mask();
  r.draw_rect(Color::from_rgb(0xff0000,255),rect(0.,16.));r.deactivate_mask();r.pop_mask();
  assert_eq!(r.pixels.pixel(2,2).unwrap().alpha(),0);assert_eq!(r.pixels.pixel(6,2).unwrap().red(),255);assert_eq!(r.pixels.pixel(10,2).unwrap().alpha(),0);
  r.draw_rect(Color::from_rgb(0x00ff00,255),rect(0.,2.));assert_eq!(r.pixels.pixel(1,2).unwrap().green(),255);
 }
}
#[cfg(test)]mod offscreen_tests {
 use super::*;
 #[test]fn bitmap_readback_contains_offscreen_render_and_restores_stage(){
  let mut r=SoftwareRenderer::new(16,16);
  let bitmap=r.create_empty_texture(NonZeroU32::new(8).unwrap(),NonZeroU32::new(8).unwrap()).unwrap();
  let mut c=CommandList::new();c.draw_rect(Color::from_rgb(0x123456,255),Matrix::scale(8.,8.));
  let sync=r.render_offscreen(bitmap,c,StageQuality::Low,PixelRegion::for_whole_size(8,8)).unwrap();
  r.resolve_sync_handle(sync,Box::new(|bytes,width|{assert_eq!(width,8);assert_eq!(&bytes[..4],&[0x12,0x34,0x56,255]);})).unwrap();
  assert_eq!((r.pixels.width(),r.pixels.height()),(16,16));
 }
}

#[cfg(test)]mod cache_tests {
 use super::*;
 fn path(id:u64,stroke:bool)->Path{
  let mut pb=sk::PathBuilder::new();pb.move_to(2.,3.);pb.cubic_to(29.,-9.,-10.,30.,25.,24.);pb.line_to(2.,3.);pb.close();
  Path{shader:RefCell::new(None),id,geometry:pb.finish().unwrap(),style:FillStyle::Color(Color::from_rgba(0x72b5d299)),bitmap:None,stroke:stroke.then(||sk::Stroke{width:2.7,miter_limit:8.,line_cap:sk::LineCap::Square,..Default::default()}),rule:sk::FillRule::EvenOdd}
 }
 #[test]fn cached_paths_match_background_transform_color_and_mask_changes(){
  let mut fast=SoftwareRenderer::new(96,64);let mut reference=SoftwareRenderer::new(96,64);reference.cache_enabled=false;
  for frame in 0..120 {
   let bg=sk::Color::from_rgba8((frame/8) as u8,31,61,255);fast.pixels.fill(bg);reference.pixels.fill(bg);
   for id in 1..=6 {
    let p=path(id,id%2==0);let mut t=Transform::default();t.matrix=Matrix::scale(0.13+(id as f32)*0.27,0.18+(id as f32)*0.21);
    t.matrix.b=0.12;t.matrix.c=-0.35;t.matrix.tx=swf::Twips::from_pixels((frame/11+id as i32*8) as f64);t.matrix.ty=swf::Twips::from_pixels(4.);
    t.color_transform.r_add=(frame/17) as i16;
    for r in [&mut fast,&mut reference]{r.paint_path(&p,&t);}
   }
   assert_eq!(fast.pixels.data(),reference.pixels.data(),"frame {frame}");
  }
  assert!(fast.stats.cache_hits>100,"cache must be exercised");
  for r in [&mut fast,&mut reference]{r.push_mask();r.draw_rect(Color::WHITE,Matrix::scale(40.,64.));r.activate_mask();r.paint_path(&path(90,false),&Transform::default());r.deactivate_mask();r.pop_mask();}
  assert_eq!(fast.pixels.data(),reference.pixels.data());
 }
 #[test]fn masked_cache_reuses_exact_masks_and_handles_nested_alpha_changes(){
  let mut fast=SoftwareRenderer::new(64,64);let mut reference=SoftwareRenderer::new(64,64);reference.cache_enabled=false;
  for frame in 0..24 {
   let side=(frame/3)%2;let alpha=if frame<12{255}else{127};
   for r in [&mut fast,&mut reference]{
    r.pixels.fill(sk::Color::BLACK);
    r.push_mask();let mut m=Matrix::scale(24.,64.);m.tx=swf::Twips::from_pixels((side*16)as f64);r.draw_rect(Color::from_rgb(0xffffff,alpha),m);r.activate_mask();
    r.push_mask();r.draw_rect(Color::WHITE,Matrix::scale(64.,40.));r.activate_mask();
    r.paint_path(&path(901,true),&Transform::default());r.deactivate_mask();r.pop_mask();r.deactivate_mask();r.pop_mask();
   }
   assert_eq!(fast.pixels.data(),reference.pixels.data(),"masked frame {frame}");
  }
  assert!(fast.stats.cache_hits>=8,"masked repeated draws must actually replay");
 }
 #[test]fn mutable_bitmap_fill_invalidates_cached_draw(){
  let mut fast=SoftwareRenderer::new(48,48);let mut reference=SoftwareRenderer::new(48,48);reference.cache_enabled=false;
  let mut handles=Vec::new();
  for r in [&mut fast,&mut reference]{handles.push(r.create_empty_texture(NonZeroU32::new(8).unwrap(),NonZeroU32::new(8).unwrap()).unwrap());}
  for frame in 0..12{
   for (index,r) in [&mut fast,&mut reference].into_iter().enumerate(){
    let h=&handles[index];if frame==0||frame==6 {let rgba=if frame==0{[230,20,0,255]}else{[0,20,230,255]};r.update_texture(h,Bitmap::new(8,8,BitmapFormat::Rgba,rgba.repeat(64)),PixelRegion::for_whole_size(8,8)).unwrap();}
    let mut p=path(300,false);p.style=FillStyle::Bitmap{id:1,matrix:swf::Matrix{a:swf::Fixed16::from_f32(20.),d:swf::Fixed16::from_f32(20.),..Default::default()},is_repeating:true,is_smoothed:true};p.bitmap=Some(h.clone());r.pixels.fill(sk::Color::BLACK);r.paint_path(&p,&Transform::default());
   }
   assert_eq!(fast.pixels.data(),reference.pixels.data(),"bitmap frame {frame}");
  }
  assert!(fast.stats.cache_hits>=8);
 }
 #[test]fn narrow_fractional_mask_cache_matches_uncached_pixels(){
  let mut fast=SoftwareRenderer::new(64,64);let mut reference=SoftwareRenderer::new(64,64);reference.cache_enabled=false;
  for frame in 0..8 {
   for r in [&mut fast,&mut reference] {
    r.pixels.fill(sk::Color::from_rgba8(20,40,60,255));
    r.push_mask();let mut m=Matrix::scale(3.7,40.2);m.tx=swf::Twips::from_pixels(10.35);
    r.draw_rect(Color::from_rgb(0xffffff,133),m);r.activate_mask();
    r.paint_path(&path(902+frame%2,frame%2==0),&Transform::default());r.deactivate_mask();r.pop_mask();
   }
   assert_eq!(fast.pixels.data(),reference.pixels.data());
  }
  assert!(fast.cache.bytes()<64*64*8);
 }
 #[test]fn cache_eviction_stays_within_memory_and_entry_limits(){
  let mut r=SoftwareRenderer::new(320,240);
  for id in 1..2500 {let mut t=Transform::default();t.matrix=Matrix::scale(4.,4.);r.paint_path(&path(id,false),&t);}
  assert!(r.cache.bytes()<=8*1024*1024);assert!(r.cache.entries_len()<=1024);
  r.trim_for_memory_pressure();assert!(r.cache.bytes()<=2*1024*1024);
  for id in 2500..2700 {let mut t=Transform::default();t.matrix=Matrix::scale(4.,4.);r.paint_path(&path(id,false),&t);}
  assert!(r.cache.bytes()<=2*1024*1024);
 }
}

impl SoftwareRenderer {
 pub fn mesh_memory(&self)->[u64;3] {
  #[cfg(any(all(target_os="ps3",feature="ps3-rsx"),feature="host-rsx-probe"))] {rsx_backend::memory()}
  #[cfg(not(any(all(target_os="ps3",feature="ps3-rsx"),feature="host-rsx-probe")))] {[0;3]}
 }
}
