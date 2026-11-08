use crate::alloc::Layout;
unsafe extern "C" { fn rp_alloc(size: usize, align: usize) -> *mut u8; fn rp_free(p: *mut u8); }
pub unsafe fn alloc(l: Layout) -> *mut u8 { unsafe { rp_alloc(l.size(),l.align()) } }
pub unsafe fn dealloc(p: *mut u8, _: Layout) { unsafe { rp_free(p) } }
pub unsafe fn realloc(p:*mut u8,l:Layout,n:usize)->*mut u8 { unsafe { super::realloc_fallback(p,l,n) } }
pub unsafe fn alloc_zeroed(l:Layout)->*mut u8 { let p=unsafe{alloc(l)}; if !p.is_null(){unsafe{p.write_bytes(0,l.size())}} p }
