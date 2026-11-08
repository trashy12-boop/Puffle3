fn main(){
 println!("cargo:rerun-if-changed=benchmark/clock.c");
 if std::env::var("CARGO_CFG_TARGET_OS").as_deref()==Ok("linux"){
  cc::Build::new().file("benchmark/clock.c").compile("rp_benchmark_clock");
  if std::env::var_os("CARGO_FEATURE_HOST_RSX_PROBE").is_some(){
   for path in ["../src/rsx_renderer.c","../src/pixel_convert.c","benchmark/rsx_mock.c","../include/rsx_profile.h"]{println!("cargo:rerun-if-changed={path}");}
   let mut b=cc::Build::new();b.include("../vendor/rsxkit/include").include("../tests/mock").include("../include").file("../src/rsx_renderer.c").file("../src/pixel_convert.c").file("benchmark/rsx_mock.c");
   if std::env::var_os("CARGO_FEATURE_HOST_RSX_UNBATCHED").is_some(){b.define("RP_RSX_BATCHING","0");}
   b.compile("rp_rsx_probe");
  }
 }
}
