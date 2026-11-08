unsafe extern "C" { fn rp_random(p:*mut u8,n:usize)->i32; }
pub fn fill_bytes(b:&mut [u8]) { assert_eq!(unsafe {rp_random(b.as_mut_ptr(),b.len())},0,"PS3 random source failed"); }
