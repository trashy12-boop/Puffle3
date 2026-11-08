/* MIT. Host replay ONLY. Never linked into the PS3 executable.
 * A frozen movie clock during events/update makes getTimer, Date and RNG seeds
 * reproducible. Outside those calls, benchmark measurements use real time. */
#define _GNU_SOURCE
#include <time.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/syscall.h>
static _Thread_local int enabled;
static _Thread_local uint64_t movie_ns;
void rp_benchmark_clock(uint64_t ns,int active){movie_ns=ns;enabled=active;}
int clock_gettime(clockid_t id,struct timespec *ts){
 if(enabled&&(id==CLOCK_MONOTONIC||id==CLOCK_REALTIME)){
  uint64_t n=movie_ns+(id==CLOCK_REALTIME?1000000000000000000ULL:1000000000000000ULL);
  ts->tv_sec=n/1000000000;ts->tv_nsec=n%1000000000;return 0;
 }
 return syscall(SYS_clock_gettime,id,ts);
}

/* Independent diagnostic clock: never freezes with Flash getTimer/Date. */
uint64_t rp_profile_clock_us(void){struct timespec t;syscall(SYS_clock_gettime,CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000000+t.tv_nsec/1000;}
