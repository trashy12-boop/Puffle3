#include "profile.h"
#include <stdio.h>
#include <errno.h>
#include <string.h>

#define PATH_LIMIT 768
#define RECORD_LIMIT 1024
static uint64_t hash64(const char *s) { uint64_t v=UINT64_C(14695981039346656037); for (;*s;s++) v=(v^(unsigned char)*s)*UINT64_C(1099511628211); return v; }
static uint32_t hash32(const unsigned char *p,size_t n) { uint32_t v=2166136261u; while(n--) v=(v^*p++)*16777619u; return v; }
static void put16(unsigned char *p, unsigned v) { p[0]=(unsigned char)v; p[1]=(unsigned char)(v>>8); }
static unsigned get16(const unsigned char *p) { return (unsigned)p[0]|((unsigned)p[1]<<8); }
static void put32(unsigned char *p,uint32_t v) { for(unsigned i=0;i<4;i++) p[i]=(unsigned char)(v>>(i*8)); }
static uint32_t get32(const unsigned char *p) { return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24); }
static int path_for(char *out,size_t size,const char *dir,const char *game) {
    if (!out||!dir||!game||!*game || strlen(game)>512) return -1;
    int n=snprintf(out,size,"%s/%016llx.rpf",dir,(unsigned long long)hash64(game));
    return n<0 || (size_t)n>=size ? -1 : 0;
}
int rp_profile_save(const char *directory,const char *game,const RpProfile *p) {
    char path[PATH_LIMIT],tmp[PATH_LIMIT]; unsigned char b[RECORD_LIMIT]; size_t pos=0;
    if (!rp_profile_valid(p)||path_for(path,sizeof(path),directory,game)) return -1;
    int n=snprintf(tmp,sizeof(tmp),"%s.tmp",path); if(n<0||(size_t)n>=sizeof(tmp)) return -1;
    size_t glen=strlen(game); memcpy(b,"RPF2",4); pos=4; put16(b+pos,(unsigned)glen); pos+=2;
    memcpy(b+pos,game,glen); pos+=glen;
    for(int i=0;i<RP_BUTTONS;i++) for(int j=0;j<RP_BINDINGS;j++) { put16(b+pos,p->binding[i][j]); pos+=2; }
    put16(b+pos,p->mouse_speed); pos+=2; b[pos++]=p->deadzone; b[pos++]=p->acceleration; b[pos++]=p->mouse_stick; b[pos++]=p->renderer;
    put32(b+pos,hash32(b,pos)); pos+=4;
    FILE *f=fopen(tmp,"wb"); if(!f) return -1;
    bool ok=fwrite(b,1,pos,f)==pos && fflush(f)==0; if(fclose(f)!=0) ok=false;
    if(!ok) { remove(tmp); return -1; }
    if(rename(tmp,path)==0) return 0;
    /* LV2 rename does not replace an existing destination. Preserve the old
       record until the new record has been installed; load can recover .bak. */
    if(errno!=EEXIST) { remove(tmp); return -1; }
    char backup[PATH_LIMIT];
    n=snprintf(backup,sizeof(backup),"%s.bak",path);
    if(n<0 || (size_t)n>=sizeof(backup)) { remove(tmp); return -1; }
    remove(backup);
    if(rename(path,backup)!=0) { remove(tmp); return -1; }
    if(rename(tmp,path)!=0) { rename(backup,path); remove(tmp); return -1; }
    remove(backup);
    return 0;
}
int rp_profile_load(const char *directory,const char *game,RpProfile *p) {
    char path[PATH_LIMIT]; unsigned char b[RECORD_LIMIT];
    if(!p||path_for(path,sizeof(path),directory,game)) return -1;
    FILE *f=fopen(path,"rb");
    if(!f && errno==ENOENT) {
        char backup[PATH_LIMIT];
        int n=snprintf(backup,sizeof(backup),"%s.bak",path);
        if(n>=0 && (size_t)n<sizeof(backup)) f=fopen(backup,"rb");
    }
    if(!f) return -1;
    size_t len=fread(b,1,sizeof(b),f); bool extra=fgetc(f)!=EOF; bool bad=ferror(f)!=0; fclose(f);
    if(extra||bad||len<4) return -1;
    bool old_format=memcmp(b,"RPF1",4)==0;
    const size_t fields=RP_BUTTONS*RP_BINDINGS*2u+(old_format?5u:6u);
    if(extra||bad||len<6+fields+4||(!old_format && memcmp(b,"RPF2",4)!=0)) return -1;
    size_t glen=get16(b+4),pos=6; if(glen>512||len!=6+glen+fields+4||strlen(game)!=glen||memcmp(b+pos,game,glen)!=0) return -1;
    if(get32(b+len-4)!=hash32(b,len-4)) return -1;
    pos+=glen;
    RpProfile result; memset(&result,0,sizeof(result));
    for(int i=0;i<RP_BUTTONS;i++) for(int j=0;j<RP_BINDINGS;j++) { result.binding[i][j]=(uint16_t)get16(b+pos); pos+=2; }
    result.mouse_speed=(uint16_t)get16(b+pos); pos+=2; result.deadzone=b[pos++]; result.acceleration=b[pos++]; result.mouse_stick=b[pos++];
    result.renderer=old_format?0:b[pos++];
    if(!rp_profile_valid(&result)) return -1;
    *p=result; return 0;
}
