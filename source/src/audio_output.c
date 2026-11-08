/* MIT. Main-thread polling output; Rust never runs on an audio worker. */
#include "player.h"
#include <audio/audio.h>
#include <string.h>
#include <stdint.h>
static audioPortConfig config;
static u32 port,last_read;
static int ready,stopped,nonzero_reported;
extern void rp_log_write(const unsigned char*,size_t);
int rp_audio_init(void) {
    if(audioInit()!=0)return -1;
    audioPortParam p={AUDIO_PORT_2CH,AUDIO_BLOCK_16,0,1.0f};
    if(audioPortOpen(&p,&port)!=0){audioQuit();return -1;}
    if(audioGetPortConfig(port,&config)!=0){audioPortClose(port);audioQuit();return -1;}
    memset((void*)(uintptr_t)config.audioDataStart,0,config.portSize);
    last_read=(u32)*(volatile u64*)(uintptr_t)config.readIndex;
    ready=1;stopped=1;return 0;
}
void rp_audio_reset(void) {
    if(!ready)return;
    audioPortStop(port);stopped=1;
    memset((void*)(uintptr_t)config.audioDataStart,0,config.portSize);
    last_read=(u32)*(volatile u64*)(uintptr_t)config.readIndex;
}
void rp_audio_pause(int pause) {
    if(!ready)return;
    if(pause&&!stopped){audioPortStop(port);stopped=1;}
    else if(!pause&&stopped){audioPortStart(port);stopped=0;}
}
void rp_audio_pump(void *player) {
    if(!ready||stopped||!player)return;
    u32 read=(u32)*(volatile u64*)(uintptr_t)config.readIndex;
    for(u32 n=0;last_read!=read&&n<16;n++) {
        float *block=(float*)(uintptr_t)config.audioDataStart+last_read*512;
        rp_player_mix(player,block,512);
        if(!nonzero_reported){for(unsigned i=0;i<512;i++)if(block[i]>.001f||block[i]<-.001f){const char msg[]="AUDIO nonzero PCM reached PS3 output ring\n";rp_log_write((const unsigned char*)msg,sizeof(msg)-1);nonzero_reported=1;break;}}
        __asm__ volatile("sync":::"memory");
        last_read=(last_read+1)%16;
    }
}
void rp_audio_shutdown(void) {if(ready){audioPortStop(port);audioPortClose(port);audioQuit();ready=0;}}
