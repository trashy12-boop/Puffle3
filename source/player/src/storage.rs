//! Bounded persistent SharedObject storage, namespaced by the loaded SWF.
use ruffle_core::backend::storage::StorageBackend;
unsafe extern "C" {fn rp_storage_read(k:*const u8,n:usize,out:*mut u8,max:usize)->i64;fn rp_storage_write(k:*const u8,n:usize,v:*const u8,len:usize)->i32;fn rp_storage_remove(k:*const u8,n:usize);}
pub struct Storage(pub String);
impl Storage {fn key(&self,name:&str)->String{format!("{}\n{name}",self.0)}}
impl StorageBackend for Storage {
 fn get(&self,name:&str)->Option<Vec<u8>>{let k=self.key(name);let n=unsafe{rp_storage_read(k.as_ptr(),k.len(),std::ptr::null_mut(),0)};if !(0..=1024*1024).contains(&n){return None;}let mut v=vec![0;n as usize];if unsafe{rp_storage_read(k.as_ptr(),k.len(),v.as_mut_ptr(),v.len())}!=n{return None;}Some(v)}
 fn put(&mut self,name:&str,value:&[u8])->bool{let k=self.key(name);unsafe{rp_storage_write(k.as_ptr(),k.len(),value.as_ptr(),value.len())==0}}
 fn remove_key(&mut self,name:&str){let k=self.key(name);unsafe{rp_storage_remove(k.as_ptr(),k.len())}}
}
