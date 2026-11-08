/* MIT. Single-PPE Rust runtime services backed by PSL1GHT/newlib. */
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <malloc.h>
#include <stdio.h>
#include <sys/systime.h>
#include <lv2/system.h>
#include <sys/tty.h>
typedef struct {void *base;size_t size;} Header;
static size_t live_bytes,peak_bytes;
size_t rp_rust_bytes(void){return live_bytes;}
size_t rp_rust_peak(void){return peak_bytes;}
static void allocation_failure(size_t size,size_t align,const char *reason) {
    char message[320];u32 written=0;struct mallinfo heap=mallinfo();
    int n=snprintf(message,sizeof(message),"Puffle allocation failure: %s request=%llu align=%llu live=%llu peak=%llu arena=%u allocated=%u free=%u top=%u\n",reason,(unsigned long long)size,(unsigned long long)align,(unsigned long long)live_bytes,(unsigned long long)peak_bytes,(unsigned)heap.arena,(unsigned)heap.uordblks,(unsigned)heap.fordblks,(unsigned)heap.keepcost);
    if(n>0)sysTtyWrite(0,message,(u32)((size_t)n<sizeof(message)?(size_t)n:sizeof(message)-1),&written);
}
void *rp_alloc(size_t size,size_t align) {
    if(align<16)align=16;
    if(size>128u*1024*1024-live_bytes || align>1024u*1024){allocation_failure(size,align,"guard");return NULL;}
    if(size>SIZE_MAX-align-sizeof(Header))return NULL;
    void *base=malloc(size+align+sizeof(Header));if(!base){allocation_failure(size,align,"malloc");return NULL;}
    uintptr_t address=((uintptr_t)base+sizeof(Header)+align-1)&~(uintptr_t)(align-1);
    Header *h=(Header*)address-1;h->base=base;h->size=size;
    live_bytes+=size;if(live_bytes>peak_bytes)peak_bytes=live_bytes;
    return (void*)address;
}
void rp_free(void *p) {if(p){Header *h=(Header*)p-1;live_bytes-=h->size;free(h->base);}}
uint64_t rp_monotonic_us(void) { return sysGetSystemTime(); }
uint64_t rp_wall_us(void) { u64 s=0,n=0; sysGetCurrentTime(&s,&n); return s*1000000+n/1000; }
int rp_random(unsigned char *p,size_t n) { while(n) { size_t part=n>4096?4096:n; if(sysGetRandomNumber(p,part)!=0)return -1; p+=part;n-=part; } return 0; }
static FILE *core_log;
static char log_buffer[4096];
void rp_log_write(const unsigned char *p,size_t n) {
 if(!core_log){core_log=fopen("/dev_hdd0/ruffle/ruffle-core.log","a");if(core_log)setvbuf(core_log,log_buffer,_IOFBF,sizeof(log_buffer));}
 if(core_log)fwrite(p,1,n,core_log);
}
void rp_log_flush(void){if(core_log)fflush(core_log);}

uint64_t rp_profile_clock_us(void){return sysGetSystemTime();}
