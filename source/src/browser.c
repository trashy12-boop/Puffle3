#include "browser.h"
#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int swf_name(const char *s) { size_t n=strlen(s); return n>=4 && s[n-4]=='.' && tolower((unsigned char)s[n-3])=='s' && tolower((unsigned char)s[n-2])=='w' && tolower((unsigned char)s[n-1])=='f'; }
static int compare(const void *a,const void *b) {
    const RpEntry *x=a,*y=b; if(x->directory!=y->directory) return y->directory-x->directory;
    const unsigned char *p=(const unsigned char*)x->name,*q=(const unsigned char*)y->name;
    while(*p&&*q&&tolower(*p)==tolower(*q)) { p++; q++; } int v=tolower(*p)-tolower(*q); return v?v:strcmp(x->name,y->name);
}
int rp_browser_open(RpBrowser *b,const char *root) {
    if(!b||!root||!*root||strlen(root)>=RP_PATH_MAX) return -1;
    memset(b,0,sizeof(*b));
    strcpy(b->root,root); size_t n=strlen(b->root); while(n>1&&b->root[n-1]=='/') b->root[--n]=0;
    strcpy(b->current,b->root); return rp_browser_refresh(b);
}
int rp_browser_path(const RpBrowser *b,unsigned index,char *out,size_t cap) {
    if(!b||index>=b->count||!out||!cap) return -1;
    int n=snprintf(out,cap,"%s/%s",b->current,b->items[index].name);
    return n<0||(size_t)n>=cap ? -1 : 0;
}
int rp_browser_refresh(RpBrowser *b) {
    if(!b) return -1;
    DIR *d=opendir(b->current); if(!d) { b->count=0; return -1; }
    b->count=0; b->truncated=0; struct dirent *ent;
    while((ent=readdir(d))!=NULL) {
        if(ent->d_name[0]=='.' && (!ent->d_name[1]||(ent->d_name[1]=='.'&&!ent->d_name[2]))) continue;
        size_t len=strlen(ent->d_name); if(!len||len>RP_NAME_MAX||strchr(ent->d_name,'/')) continue;
        char path[RP_PATH_MAX]; int n=snprintf(path,sizeof(path),"%s/%s",b->current,ent->d_name); if(n<0||(size_t)n>=sizeof(path)) continue;
        struct stat st; if(stat(path,&st)!=0) continue;
        int is_dir=S_ISDIR(st.st_mode); if(!is_dir && (!S_ISREG(st.st_mode)||!swf_name(ent->d_name))) continue;
        if(b->count==RP_BROWSER_MAX) { b->truncated=1; continue; }
        RpEntry *e=&b->items[b->count++]; strcpy(e->name,ent->d_name); e->directory=is_dir; e->size=(uint64_t)st.st_size;
    }
    closedir(d); qsort(b->items,b->count,sizeof(b->items[0]),compare); return 0;
}
int rp_browser_enter(RpBrowser *b,unsigned index) {
    if(!b||index>=b->count||!b->items[index].directory) return -1;
    char next[RP_PATH_MAX],old[RP_PATH_MAX]; if(rp_browser_path(b,index,next,sizeof(next))) return -1;
    strcpy(old,b->current); strcpy(b->current,next); if(rp_browser_refresh(b)==0) return 0; strcpy(b->current,old); rp_browser_refresh(b); return -1;
}
int rp_browser_parent(RpBrowser *b) {
    if(!b||!strcmp(b->current,b->root)) return -1;
    char *slash=strrchr(b->current,'/'); if(!slash) return -1;
    size_t rootlen=strlen(b->root); if((size_t)(slash-b->current)<rootlen) return -1; *slash=0; return rp_browser_refresh(b);
}
int rp_swf_header(const char *path,uint32_t *size,unsigned *version) {
    unsigned char b[8]; FILE *f=fopen(path,"rb"); if(!f)return -1; size_t n=fread(b,1,8,f); fclose(f);
    if(n!=8||(b[0]!='F'&&b[0]!='C'&&b[0]!='Z')||b[1]!='W'||b[2]!='S'||b[3]==0) return -1;
    uint32_t length=(uint32_t)b[4]|((uint32_t)b[5]<<8)|((uint32_t)b[6]<<16)|((uint32_t)b[7]<<24);
    if(length<8 || length>64u*1024u*1024u) return -1;
    if(size)*size=length;
    if(version)*version=b[3];
    return 0;
}
