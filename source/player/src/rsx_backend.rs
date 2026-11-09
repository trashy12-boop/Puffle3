//! Experimental RSX rasterizer. Unsupported command lists use the CPU backend.
use super::{image, Path, Shape};
use super::rsx_clip::{self,Masks,Phase,Rect};
use ruffle_render::{
    bitmap::{BitmapHandle,PixelRegion,PixelSnapping},
    commands::{Command,CommandList,RenderBlendMode},
    matrix::Matrix,
    tessellator::{DrawType,Mesh,Gradient},
    shape_utils::GradientType,
    transform::Transform,
};
use std::{any::Any,collections::HashMap,fmt};
use swf::{BlendMode,Color,FillStyle,GradientSpread,GradientInterpolation};

#[repr(C)]
struct Vertex {pos:[f32;4],st:[f32;2],rgba:[u8;4],pad:u32}
unsafe extern "C" {
 fn rp_rsx_ready()->i32;
 fn rp_rsx_reason(reason:u32);
 fn rp_rsx_begin(rgb:u32)->i32;
 fn rp_rsx_mesh(v:*const Vertex,n:u32,p:*const u8,w:u32,h:u32,premul:i32,wrap:u8,filter:u8,gradient:u8,focal:f32,mult:*const f32,add:*const f32,texture_id:u64,version:u64)->i32;
 fn rp_rsx_end(ok:i32);
 fn rp_rsx_clip(left:i32,top:i32,right:i32,bottom:i32)->i32;
}
pub(super) fn available()->bool {unsafe{rp_rsx_ready()!=0}}

use std::sync::atomic::{AtomicU64,Ordering};
static MESH_BEFORE:AtomicU64=AtomicU64::new(0);
static MESH_AFTER:AtomicU64=AtomicU64::new(0);
static MESH_SHAPES:AtomicU64=AtomicU64::new(0);
pub(super) fn memory()->[u64;3]{[MESH_BEFORE.load(Ordering::Relaxed),MESH_AFTER.load(Ordering::Relaxed),MESH_SHAPES.load(Ordering::Relaxed)]}
enum Indices {Small(Box<[u16]>),Large(Box<[u32]>)}
impl Indices {
 fn len(&self)->usize{match self{Self::Small(v)=>v.len(),Self::Large(v)=>v.len()}}
 fn get(&self,i:usize)->usize{match self{Self::Small(v)=>v[i]as usize,Self::Large(v)=>v[i]as usize}}
}
struct CompactDraw {draw_type:DrawType,vertices:Box<[ruffle_render::tessellator::Vertex]>,indices:Indices}
struct CompactMesh {draws:Box<[CompactDraw]>,gradients:Vec<Gradient>}
pub(super) struct GpuShape {mesh:CompactMesh,bytes_before:u64,bytes_after:u64,bitmaps:HashMap<u16,BitmapHandle>,gradients:Vec<Option<Vec<u8>>>,gradient_ids:Vec<u64>}
impl fmt::Debug for GpuShape {
 fn fmt(&self,f:&mut fmt::Formatter<'_>)->fmt::Result {f.debug_struct("GpuShape").field("draws",&self.mesh.draws.len()).finish()}
}
impl GpuShape {
 pub(super) fn new(mesh:Mesh,paths:&[Path])->Self {
  let mut bitmaps=HashMap::new();
  for p in paths {if let (FillStyle::Bitmap{id,..},Some(b))=(&p.style,&p.bitmap){bitmaps.insert(*id,b.clone());}}
  let gradients:Vec<_>=mesh.gradients.iter().map(gradient).collect();
  // Share the bitmap identity allocator: IDs cannot alias live bitmap resources.
  // Immutable gradient bytes need no revision changes; cache eviction handles shape death.
  let gradient_ids=gradients.iter().map(|g|if g.is_some(){super::NEXT_BITMAP_ID.fetch_add(1,std::sync::atomic::Ordering::Relaxed)}else{0}).collect();
  let mut bytes_before=0u64;let mut bytes_after=0u64;
  let draws=mesh.draws.into_iter().map(|d|{
   bytes_before+=(d.vertices.capacity()*std::mem::size_of::<ruffle_render::tessellator::Vertex>()+d.indices.capacity()*4)as u64;
   let small=d.indices.iter().all(|&i|i<=u16::MAX as u32);
   let indices=if small {Indices::Small(d.indices.iter().map(|&i|i as u16).collect::<Vec<_>>().into_boxed_slice())}else{Indices::Large(d.indices.into_boxed_slice())};
   let vertices=d.vertices.into_boxed_slice();
   bytes_after+=(vertices.len()*std::mem::size_of::<ruffle_render::tessellator::Vertex>()+indices.len()*if small{2}else{4})as u64;
   CompactDraw{draw_type:d.draw_type,vertices,indices}
  }).collect::<Vec<_>>().into_boxed_slice();
  MESH_BEFORE.fetch_add(bytes_before,Ordering::Relaxed);MESH_AFTER.fetch_add(bytes_after,Ordering::Relaxed);MESH_SHAPES.fetch_add(1,Ordering::Relaxed);
  let mesh=CompactMesh{draws,gradients:mesh.gradients};
  Self{mesh,bytes_before,bytes_after,bitmaps,gradients,gradient_ids}
 }
 fn supported(&self)->bool {
  self.mesh.draws.iter().all(|d|match &d.draw_type{
   DrawType::Color=>true,
   DrawType::Bitmap(_)=>true,
   DrawType::Gradient{gradient,..}=>self.gradients.get(*gradient).is_some_and(Option::is_some),
  })
 }
}
impl Drop for GpuShape {
 fn drop(&mut self){MESH_BEFORE.fetch_sub(self.bytes_before,Ordering::Relaxed);MESH_AFTER.fetch_sub(self.bytes_after,Ordering::Relaxed);MESH_SHAPES.fetch_sub(1,Ordering::Relaxed);}
}
fn gradient(g:&Gradient)->Option<Vec<u8>> {
 if g.interpolation!=GradientInterpolation::Rgb{return None}
 let mut data=vec![0u8;256*4];
 if g.records.is_empty(){return Some(data)}
 let mut last=0;
 for t in 0..256usize {
  while last+1<g.records.len() && t>g.records[last+1].ratio as usize {last+=1}
  let next=(last+1).min(g.records.len()-1);
  let a=&g.records[last];let b=&g.records[next];
  let f=if t<=a.ratio as usize || a.ratio==b.ratio {0.0} else if t>b.ratio as usize {1.0} else {(t as f32-a.ratio as f32)/(b.ratio as f32-a.ratio as f32)};
  for (i,(x,y)) in [(a.color.r,b.color.r),(a.color.g,b.color.g),(a.color.b,b.color.b),(a.color.a,b.color.a)].into_iter().enumerate() {
   data[t*4+i]=(x as f32+(y as f32-x as f32)*f) as u8;
  }
 }
 Some(data)
}
fn shape(h:&ruffle_render::backend::ShapeHandle)->&Shape {
 (h.0.as_ref() as &dyn Any).downcast_ref::<Shape>().unwrap()
}
fn draw_reason(c:&Command)->u32 {
 match c {
  Command::RenderShape{shape:h,transform:t}=>{
   if t.perspective_projection.is_some(){7}else if let Some(g)=shape(h).1.as_ref(){
    if g.supported(){0}else if g.mesh.draws.iter().any(|d|matches!(&d.draw_type,DrawType::Gradient{gradient,..} if g.gradients.get(*gradient).is_none_or(Option::is_none))){1}else{5}
   }else{6}
  },
  Command::RenderBitmap{transform:t,..}=>if t.perspective_projection.is_none(){0}else{7},
  Command::DrawRect{..}=>0,
  Command::DrawLine{..}|Command::DrawLineRect{..}=>4,
  Command::RenderStage3D{..}=>8,
  Command::Blend(..)=>3,
  _=>5,
 }
}
fn mask_rectangle(c:&Command,alpha:bool)->Option<Rect>{
 match c {
  Command::DrawRect{color,matrix}=>if alpha&&color.a!=255{None}else{rsx_clip::unit_rectangle(*matrix)},
  Command::RenderShape{shape:h,transform:t}=>{
   if t.perspective_projection.is_some(){return None}
   let paths=&shape(h).0;if paths.len()!=1||paths[0].stroke.is_some(){return None}
   if alpha&&(!matches!(paths[0].style,FillStyle::Color(c)if c.a==255)||t.color_transform.a_multiply.get()!=256||t.color_transform.a_add!=0){return None}
   rsx_clip::path_rectangle(&paths[0].geometry,t.matrix)
  },
  Command::Blend(list,RenderBlendMode::Builtin(BlendMode::Normal))=>single_alpha_rectangle(list,alpha),
  _=>None,
 }
}
fn single_alpha_rectangle(list:&CommandList,alpha:bool)->Option<Rect>{
 if list.commands.is_empty(){return Some(Rect::EMPTY)}
 if list.commands.len()!=1{return None}
 mask_rectangle(&list.commands[0],alpha)
}
struct Planned<'a>{command:&'a Command,clip:Rect}
fn plan_list<'a>(list:&'a CommandList,masks:&mut Masks,out:&mut Vec<Planned<'a>>)->Result<(),u32>{
 for command in &list.commands {
  match command {
   Command::PushMask=>masks.push()?,
   Command::ActivateMask=>masks.activate()?,
   Command::DeactivateMask=>masks.deactivate()?,
   Command::PopMask=>masks.pop()?,
   Command::Blend(list,RenderBlendMode::Builtin(BlendMode::Normal))=>plan_list(list,masks,out)?,
   Command::RenderAlphaMask{maskee_commands,mask_commands}=>{
    if masks.phase()!=Phase::Draw{return Err(2)}
    let rect=single_alpha_rectangle(mask_commands,true).ok_or(2u32)?;
    masks.push()?;masks.rectangle(rect)?;masks.activate()?;plan_list(maskee_commands,masks,out)?;masks.deactivate()?;masks.pop()?;
   },
   _=>match masks.phase(){
    Phase::Erase=>if !matches!(command,Command::RenderShape{..}|Command::RenderBitmap{..}|Command::DrawRect{..}|Command::DrawLine{..}|Command::DrawLineRect{..}){return Err(2)},
    Phase::Build=>{
     let rectangle=mask_rectangle(command,false);
     #[cfg(any(feature="host-rsx-probe",feature="profile-vm"))]
     if rectangle.is_none(){probe_mask(command);}
     masks.rectangle(rectangle.ok_or(2u32)?)?;
    },
    Phase::Draw=>{
     let reason=draw_reason(command);if reason!=0{return Err(reason)}
     out.try_reserve(1).map_err(|_|5u32)?;
     out.push(Planned{command,clip:masks.clip});
    },
   },
  }
 }
 Ok(())
}
fn plan_frame(list:&CommandList)->Result<Vec<Planned<'_>>,u32>{
 let mut masks=Masks::new();let mut out=Vec::new();plan_list(list,&mut masks,&mut out)?;
 if !masks.balanced(){return Err(2)}Ok(out)
}
pub(super) fn note_cache(){unsafe{rp_rsx_reason(4)}}

fn point(m:Matrix,x:f32,y:f32)->[f32;4] {
 [m.a*x+m.c*y+m.tx.to_pixels() as f32,m.b*x+m.d*y+m.ty.to_pixels() as f32,0.0,1.0]
}
fn color(c:Color)->[u8;4] {[c.r,c.g,c.b,c.a]}
fn uv(m:&[[f32;3];3],x:f32,y:f32)->[f32;2] {
 [m[0][0]*x+m[1][0]*y+m[2][0],m[0][1]*x+m[1][1]*y+m[2][1]]
}
fn submit(v:&[Vertex],p:Option<(&[u8],u32,u32,bool)>,wrap:u8,smooth:bool,gradient:u8,focal:f32,ct:Option<&swf::ColorTransform>,identity:Option<(u64,u64)>)->bool {
 if v.is_empty(){return true;}
 if v.len()>u32::MAX as usize{return false;}
 let (pixels,w,h,premul)=p.map_or((std::ptr::null(),0,0,false),|(p,w,h,pre)|(p.as_ptr(),w,h,pre));
 let (m,a)=ct.map_or(([1.0;4],[0.0;4]),|c|(c.mult_rgba_normalized(),c.add_rgba_normalized()));
 let (id,revision)=identity.unwrap_or((0,0));
 unsafe{rp_rsx_mesh(v.as_ptr(),v.len() as u32,pixels,w,h,premul as i32,wrap,if smooth{0}else{1},gradient,focal,m.as_ptr(),a.as_ptr(),id,revision)!=0}
}
fn draw_shape(h:&ruffle_render::backend::ShapeHandle,t:&Transform,verts:&mut Vec<Vertex>)->bool {
 let Some(gpu)=shape(h).1.as_ref() else{return false};
 for draw in &gpu.mesh.draws {
  // RSX consumes the copied data synchronously before returning from submit.
  // Reuse this staging Vec for every draw in the frame to avoid hot-path heap
  // allocation, even for shapes with thousands of triangles.
  verts.clear();
  verts.reserve(draw.indices.len());
  let matrix=match &draw.draw_type {DrawType::Bitmap(b)=>Some(&b.matrix),DrawType::Gradient{matrix,..}=>Some(matrix),_=>None};
  for slot in 0..draw.indices.len() {
   let i=draw.indices.get(slot);
   let Some(v)=draw.vertices.get(i) else{return false};
   let c=if matrix.is_some(){Color::WHITE}else{&t.color_transform*v.color};
   let mut st=matrix.map_or([0.0,0.0],|m|uv(m,v.x,v.y));
   if let DrawType::Gradient{gradient,..}=&draw.draw_type {if gpu.mesh.gradients[*gradient].gradient_type==GradientType::Linear {st[1]=0.5;}}
   verts.push(Vertex{pos:point(t.matrix,v.x,v.y),st,rgba:color(c),pad:0});
  }
  match &draw.draw_type {
   DrawType::Color=>if !submit(&verts,None,0,true,0,0.0,None,None){return false},
   DrawType::Bitmap(b)=>{
    // The software path also omits a fill when its bitmap could not be resolved.
    let Some(handle)=gpu.bitmaps.get(&b.bitmap_id) else{continue};
    let pix=image(handle).0.borrow();
    if !submit(&verts,Some((pix.data(),pix.width(),pix.height(),true)),if b.is_repeating{1}else{0},b.is_smoothed,0,0.0,Some(&t.color_transform),Some((image(handle).2,image(handle).1.get()))){return false;}
   }
   DrawType::Gradient{gradient,..}=>{
    let Some(Some(p))=gpu.gradients.get(*gradient) else{return false};
    let wrap=match gpu.mesh.gradients[*gradient].repeat_mode {GradientSpread::Pad=>0,GradientSpread::Repeat=>1,GradientSpread::Reflect=>2};
    let g=&gpu.mesh.gradients[*gradient];
    let kind=match g.gradient_type {GradientType::Linear=>0,GradientType::Radial=>1,GradientType::Focal=>2};
    if !submit(&verts,Some((p,256,1,false)),wrap,true,kind,g.focal_point.to_f32().clamp(-0.98,0.98),Some(&t.color_transform),Some((gpu.gradient_ids[*gradient],0))){return false;}
   }
  }
 }
 true
}
fn draw_bitmap(b:&BitmapHandle,t:&Transform,smooth:bool,snap:PixelSnapping,r:PixelRegion)->bool {
 let mut m=t.matrix;snap.apply(&mut m);
 let pix=image(b).0.borrow();let w=pix.width() as f32;let h=pix.height() as f32;
 if w==0.0 || h==0.0 || r.width()==0 || r.height()==0{return true}
 let x=r.width() as f32;let y=r.height() as f32;
 let points=[[0.0,0.0],[x,0.0],[x,y],[0.0,y]];
 let tex=[[r.x_min as f32/w,r.y_min as f32/h],[r.x_max as f32/w,r.y_min as f32/h],[r.x_max as f32/w,r.y_max as f32/h],[r.x_min as f32/w,r.y_max as f32/h]];
 let v=[0usize,1,2,0,2,3].map(|i|Vertex{pos:point(m,points[i][0],points[i][1]),st:tex[i],rgba:[255;4],pad:0});
 submit(&v,Some((pix.data(),pix.width(),pix.height(),true)),0,smooth,0,0.0,Some(&t.color_transform),Some((image(b).2,image(b).1.get())))
}
fn draw_rect(c:Color,m:Matrix)->bool {
 let p=[[0.0,0.0],[1.0,0.0],[1.0,1.0],[0.0,1.0]];
 let v=[0usize,1,2,0,2,3].map(|i|Vertex{pos:point(m,p[i][0],p[i][1]),st:[0.0,0.0],rgba:color(c),pad:0});
 submit(&v,None,0,true,0,0.0,None,None)
}
fn execute(plan:&[Planned<'_>],scratch:&mut Vec<Vertex>)->bool {
 let mut clip=Rect::FULL;
 for draw in plan {
  if draw.clip.empty(){continue}
  if draw.clip!=clip {
   let r=draw.clip;if unsafe{rp_rsx_clip(r.left,r.top,r.right,r.bottom)}==0{return false}clip=r;
  }
  let ok=match draw.command {
   Command::RenderShape{shape,transform}=>draw_shape(shape,transform,scratch),
   Command::RenderBitmap{bitmap,transform,smoothing,pixel_snapping,region}=>draw_bitmap(bitmap,transform,*smoothing,*pixel_snapping,*region),
   Command::DrawRect{color,matrix}=>draw_rect(*color,*matrix),
   _=>false,
  };
  if !ok{return false}
 }
 true
}
pub(super) fn render_frame(clear:Color,list:&CommandList)->bool {
 // Validate the entire list before beginning GPU work; unsupported masks keep
 // the established full software fallback and never leave a partial frame.
 let plan=match plan_frame(list){Ok(plan)=>plan,Err(why)=>{unsafe{rp_rsx_reason(why)};return false}};
 let rgb=(u32::from(clear.r)<<16)|(u32::from(clear.g)<<8)|u32::from(clear.b);
 if unsafe{rp_rsx_begin(rgb)}==0{return false}
 let mut scratch=Vec::new();let ok=execute(&plan,&mut scratch);
 unsafe{rp_rsx_end(ok as i32)};
 #[cfg(feature="host-rsx-probe")] {let _=ok;false}
 #[cfg(not(feature="host-rsx-probe"))] {ok}
}

#[cfg(any(feature="host-rsx-probe",feature="profile-vm"))]
fn probe_mask(c:&Command){
 static SEEN:std::sync::Mutex<Vec<String>>=std::sync::Mutex::new(Vec::new());
 let mut seen=SEEN.lock().unwrap();if seen.len()>=20{return}
 let description=match c {
  Command::RenderShape{shape:h,transform:t}=>format!("paths={} transform={:?} geometry={:?}",shape(h).0.len(),t.matrix,shape(h).0.iter().map(|p|(p.geometry.bounds(),p.geometry.segments().count(),p.stroke.is_some())).collect::<Vec<_>>()),
  Command::Blend(list,mode)=>format!("blend={mode:?} length={} children={:?}",list.commands.len(),list.commands.iter().map(|c|match c{Command::RenderShape{..}=>"shape",Command::Blend(..)=>"blend",Command::RenderBitmap{..}=>"bitmap",_=>"other"}).collect::<Vec<_>>()),
  Command::RenderBitmap{..}=>"bitmap".into(),
  Command::DrawRect{matrix,color}=>format!("rect matrix={matrix:?} color={color:?}"),
  _=>"other".into(),
 };
 if !seen.contains(&description){println!("MASK_PROBE {description}");seen.push(description);}
}

#[cfg(test)] mod compact_tests {
 use super::*;
 #[test]fn indices_preserve_the_u16_boundary_and_exact_vertices(){
  for (indices,small)in [(vec![0,65535,1],true),(vec![0,65536,1],false)] {
   let mut vertices=vec![ruffle_render::tessellator::Vertex{x:1.25,y:-2.5,color:Color::WHITE};65537];
   vertices[65536].x=123.125;
   let original=indices.clone();
   let mesh=Mesh{draws:vec![ruffle_render::tessellator::Draw{draw_type:DrawType::Color,vertices,indices,mask_index_count:3}],gradients:vec![]};
   let g=GpuShape::new(mesh,&[]);let d=&g.mesh.draws[0];
   assert_eq!(matches!(&d.indices,Indices::Small(_)),small);
   for(i,v)in original.iter().enumerate(){assert_eq!(d.indices.get(i),*v as usize);}
   assert_eq!(d.vertices[65536].x.to_bits(),123.125f32.to_bits());assert_eq!(d.vertices[0].y.to_bits(),(-2.5f32).to_bits());
   assert!(g.bytes_after<=g.bytes_before);
  }
 }
}
