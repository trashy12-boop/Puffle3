/* MIT. Collision-checked SharedObject records; one MiB maximum per object. */
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#define SOL_DIR "/dev_hdd0/ruffle/saves"
static uint64_t hash(const unsigned char *p,size_t n){uint64_t h=UINT64_C(14695981039346656037);while(n--)h=(h^*p++)*UINT64_C(1099511628211);return h;}
static int path_for(char *p,size_t cap,const unsigned char *k,size_t n){if(!k||!n||n>2048)return -1;int r=snprintf(p,cap,SOL_DIR "/%016llx.sol",(unsigned long long)hash(k,n));return r<0||(size_t)r>=cap?-1:0;}
int64_t rp_storage_read(const unsigned char *k,size_t n,unsigned char *out,size_t max){
    char path[128],backup[136];if(path_for(path,sizeof(path),k,n))return -1;
    FILE *f=fopen(path,"rb");if(!f&&errno==ENOENT){snprintf(backup,sizeof(backup),"%s.bak",path);f=fopen(backup,"rb");}if(!f)return -1;
    unsigned char h[8],key[2048];int64_t result=-1;
    if(fread(h,1,8,f)!=8||memcmp(h,"RPS1",4))goto done;
    size_t count=(size_t)h[4]|((size_t)h[5]<<8);if(count!=n||count>sizeof(key)||fread(key,1,count,f)!=count||memcmp(key,k,n))goto done;
    long start=ftell(f);if(start<0||fseek(f,0,SEEK_END))goto done;long end=ftell(f);
    if(end<start||end-start>1024*1024)goto done;
    result=end-start;
    if(out){if((uint64_t)result>max||fseek(f,start,SEEK_SET)||fread(out,1,(size_t)result,f)!=(size_t)result)result=-1;}
 done:fclose(f);return result;
}
int rp_storage_write(const unsigned char *k,size_t n,const unsigned char *v,size_t len){
    char path[128],tmp[136],backup[136];if(len>1024*1024||(!v&&len)||path_for(path,sizeof(path),k,n))return -1;
    mkdir(SOL_DIR,0777);snprintf(tmp,sizeof(tmp),"%s.tmp",path);snprintf(backup,sizeof(backup),"%s.bak",path);
    FILE *existing=fopen(path,"rb");if(existing){fclose(existing);if(rp_storage_read(k,n,NULL,0)<0)return -1;}
    unsigned char h[8]={'R','P','S','1',(unsigned char)n,(unsigned char)(n>>8),0,0};FILE *f=fopen(tmp,"wb");if(!f)return -1;
    int ok=fwrite(h,1,8,f)==8&&fwrite(k,1,n,f)==n&&fwrite(v,1,len,f)==len&&fflush(f)==0;if(fclose(f))ok=0;if(!ok){remove(tmp);return -1;}
    if(rename(tmp,path)==0)return 0;
    if(errno!=EEXIST){remove(tmp);return -1;}
    remove(backup);if(rename(path,backup)){remove(tmp);return -1;}
    if(rename(tmp,path)){rename(backup,path);remove(tmp);return -1;}
    remove(backup);return 0;
}
void rp_storage_remove(const unsigned char *k,size_t n){char path[128],backup[136];if(path_for(path,sizeof(path),k,n)||rp_storage_read(k,n,NULL,0)<0)return;remove(path);snprintf(backup,sizeof(backup),"%s.bak",path);remove(backup);}
