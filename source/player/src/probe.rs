#[unsafe(no_mangle)] pub extern "C" fn rp_rust_probe()->u64 {
 let v=vec![11u64,22,33];
 println!("PS3 Rust std: vector={v:?} clock={:?}",std::time::Instant::now());
 v.iter().sum()
}
