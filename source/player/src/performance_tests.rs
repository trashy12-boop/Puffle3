use super::*;
use std::{borrow::Cow,hint::black_box,time::Instant};
use tiny_skia as sk;
use ruffle_render::{backend::RenderBackend,commands::CommandHandler,bitmap::PixelSnapping,matrix::Matrix,transform::Transform};

#[test]
fn unchanged_bitmap_borrows_pixels_and_keeps_premultiplied_alpha(){
 let b=sk::Pixmap::from_vec(vec![23,47,63,127],sk::IntSize::from_wh(1,1).unwrap()).unwrap();
 let p=renderer::bitmap_pixels(&b,&swf::ColorTransform::IDENTITY);
 assert!(matches!(p,Cow::Borrowed(_)));assert_eq!(p.data(),b.data());
}

#[test]
fn bitmap_fast_path_and_texture_updates_render_correctly(){
 let mut r=renderer::SoftwareRenderer::new(16,16);
 let bytes=vec![255u8;8*8*4];
 let b=r.register_bitmap(ruffle_render::bitmap::Bitmap::new(8,8,ruffle_render::bitmap::BitmapFormat::Rgba,bytes)).unwrap();
 let mut t=Transform::default();t.matrix=Matrix::scale(2.,2.);
 r.render_bitmap(b.clone(),t.clone(),true,PixelSnapping::Never,ruffle_render::bitmap::PixelRegion::for_whole_size(8,8));
 assert_eq!(r.pixels.pixel(6,6).unwrap().red(),255);
 let green=[0,255,0,255].repeat(64);
 r.update_texture(&b,ruffle_render::bitmap::Bitmap::new(8,8,ruffle_render::bitmap::BitmapFormat::Rgba,green),ruffle_render::bitmap::PixelRegion::for_whole_size(8,8)).unwrap();
 r.render_bitmap(b,t,false,PixelSnapping::Never,ruffle_render::bitmap::PixelRegion::for_whole_size(8,8));
 assert_eq!(r.pixels.pixel(6,6).unwrap().red(),0);assert_eq!(r.pixels.pixel(6,6).unwrap().green(),255);
}

#[test]
#[ignore = "release-only synthetic renderer benchmark, not console game FPS"]
fn bitmap_workload_benchmark(){
 // The source is larger than its on-screen sprite. This exposes the old
 // unconditional scan of every source texel, even outside the drawn area.
 let mut b=sk::Pixmap::new(1024,1024).unwrap();b.fill(sk::Color::from_rgba8(20,80,150,255));
 let ct=swf::ColorTransform::IDENTITY;
 let mut out=sk::Pixmap::new(320,240).unwrap();let iterations=200;
 let draw=|out:&mut sk::Pixmap,p:&sk::Pixmap|{
  let mut paint=sk::PixmapPaint::default();paint.quality=sk::FilterQuality::Bilinear;
  out.draw_pixmap(0,0,p.as_ref(),&paint,sk::Transform::from_scale(0.0625,0.0625),None);
 };
 let start=Instant::now();for _ in 0..iterations{let p=renderer::transform_bitmap(black_box(&b),black_box(&ct));draw(&mut out,&p);black_box(out.data());}let old=start.elapsed();
 let reference=out.data().to_vec();out.fill(sk::Color::TRANSPARENT);
 let start=Instant::now();for _ in 0..iterations{let p=renderer::bitmap_pixels(black_box(&b),black_box(&ct));draw(&mut out,&p);black_box(out.data());}let new=start.elapsed();
 assert_eq!(reference,out.data());
 println!("HOST SYNTHETIC bitmap draws={iterations} source=1024x1024 sprite=64x64 old_ms={:.3} new_ms={:.3} ratio={:.2}x eliminated_temp_bytes_per_draw={}",old.as_secs_f64()*1000.,new.as_secs_f64()*1000.,old.as_secs_f64()/new.as_secs_f64(),b.data().len());
}
