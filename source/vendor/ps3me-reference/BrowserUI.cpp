// SPDX-License-Identifier: GPL-2.0-only
#include "BrowserUI.hpp"
#include "MenuLanguage.hpp"
#include "MenuPrompt.hpp"
#include "MenuAnimation.hpp"
#include <SDL_ttf.h>
#include <SDL_image.h>
#include <SDL_gfxPrimitives.h>
#include <zip.h>
#include <map>
#include <cmath>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>
namespace {
const char home[]="/dev_hdd0/game/PS3ME0001/USRDIR";
// Vertical strip of the 1280x720 menu that the library rows can touch. The
// static chrome (title, path line, message and controller prompts) lives
// outside it and is only repainted when it actually changes.
const int bandTop=143;
const int bandBottom=588;
struct Item{std::string title,vendor;SDL_Surface* icon;SDL_Surface* preview;std::map<int,SDL_Surface*> scaled;bool dir;Item():icon(0),preview(0),dir(false){}};
struct CachedText{SDL_Surface* surface;int width;CachedText():surface(0),width(0){}};
std::map<std::string,Item> cache;
std::map<std::string,CachedText> textCache;
SDL_Surface* background=0;SDL_Surface* base=0;SDL_Surface* highlight=0;
TTF_Font *big=0,*normal=0,*small=0;
std::string basePath,baseMessage;
std::vector<std::string> baseNames;
int baseWidth=0,baseHeight=0;
bool baseValid=false;
BrowserBand lastBand={0,720};
float animatedSelection=0.0f;
unsigned int lastFrameMs=0;
int lastSelectionTarget=-1;
bool animationStarted=false;
void ensureUi() {
    if(background) return;
    TTF_Init();
    std::string f=std::string(home)+"/ui/DejaVuSans.ttf";
    big=TTF_OpenFont(f.c_str(),32);normal=TTF_OpenFont(f.c_str(),23);small=TTF_OpenFont(f.c_str(),17);
    background=SDL_CreateRGBSurface(SDL_SWSURFACE,1280,720,32,0xff0000,0xff00,0xff,0);
    highlight=SDL_CreateRGBSurface(SDL_SWSURFACE,624,95,32,0xff0000,0xff00,0xff,0);
    if(highlight)SDL_FillRect(highlight,0,SDL_MapRGB(highlight->format,220,235,255));
    if(background)for(int y=0;y<720;y++)
        hlineRGBA(background,0,1279,y,23+y*17/720,64-y*45/720,91-y*26/720,255);
    if(background)for(int layer=0;layer<3;layer++)for(int x=0;x<1280;x++){
        int y=360+layer*40+(int)(35*std::sin(x/510.f+layer*.8f)+12*std::sin(x/200.f+layer));
        vlineRGBA(background,x,y,std::min(719,y+150),210,225,255,9);
        pixelRGBA(background,x,y,220,240,255,50);
    }
}
void text(SDL_Surface*,int,int,std::string,TTF_Font*,int);
void promptLabel(SDL_Surface* s, int x, int y, const char* id, const char* label) {
    int pw=ps3me::drawPrompt(s,x,y,id,24);
    text(s,x+pw+10,y+3,label,small,178);
}

std::string trim(std::string s){size_t a=s.find_first_not_of(" \t\r\n"),b=s.find_last_not_of(" \t\r\n");return a==std::string::npos?"":s.substr(a,b-a+1);}
std::vector<char> entry(struct zip* z,const char* name,int max){struct zip_stat st;std::vector<char>b;if(zip_stat(z,name,0,&st)||st.size<=0||st.size>max)return b;struct zip_file* f=zip_fopen(z,name,0);if(!f)return b;b.resize(st.size);size_t got=0;while(got<b.size()){ssize_t n=zip_fread(f,b.data()+got,b.size()-got);if(n<=0)break;got+=n;}zip_fclose(f);if(got!=b.size())b.clear();return b;}
Item load(const std::string& p,const std::string& name){Item i;i.title=name;struct stat st;if(!stat(p.c_str(),&st)&&S_ISDIR(st.st_mode)){i.dir=true;i.vendor="Folder";return i;}int err=0;struct zip* z=zip_open(p.c_str(),0,&err);if(!z)return i;auto b=entry(z,"META-INF/MANIFEST.MF",65536);std::map<std::string,std::string> m;std::string s(b.begin(),b.end()),key;size_t a=0;while(a<s.size()){size_t e=s.find('\n',a);std::string line=s.substr(a,e-a);if(!line.empty()&&line.back()=='\r')line.pop_back();if(!line.empty()&&line[0]==' '&&!key.empty())m[key]+=line.substr(1);else{size_t c=line.find(':');key.clear();if(c!=std::string::npos){key=line.substr(0,c);m[key]=trim(line.substr(c+1));}}if(e==std::string::npos)break;a=e+1;}if(!m["MIDlet-Name"].empty())i.title=m["MIDlet-Name"];i.vendor=m["MIDlet-Vendor"];std::string icon=m["MIDlet-Icon"];if(icon.empty()){std::string first=m["MIDlet-1"];size_t c=first.find(','),d=c==std::string::npos?c:first.find(',',c+1);if(d!=std::string::npos)icon=trim(first.substr(c+1,d-c-1));}while(!icon.empty()&&icon[0]=='/')icon.erase(0,1);if(icon.find("..") ==std::string::npos&&icon.size()<512){auto png=entry(z,icon.c_str(),1048576);if(png.size()>=24&&!std::memcmp(png.data(),"\x89PNG\r\n\x1a\n",8)){const unsigned char* v=(const unsigned char*)png.data();unsigned w=(unsigned(v[16])<<24)|(v[17]<<16)|(v[18]<<8)|v[19],h=(unsigned(v[20])<<24)|(v[21]<<16)|(v[22]<<8)|v[23];if(w&&h&&w<=512&&h<=512){SDL_RWops* rw=SDL_RWFromConstMem(png.data(),png.size());if(rw){SDL_Surface* raw=IMG_Load_RW(rw,1);if(raw){SDL_Surface* format=SDL_CreateRGBSurface(SDL_SWSURFACE,1,1,32,0xff0000,0xff00,0xff,0xff000000);if(format){i.icon=SDL_ConvertSurface(raw,format->format,SDL_SWSURFACE);SDL_FreeSurface(format);}SDL_FreeSurface(raw);}}}}}zip_close(z);return i;}
void freeItem(Item& i){
    if(i.icon)SDL_FreeSurface(i.icon);
    if(i.preview)SDL_FreeSurface(i.preview);
    for(std::map<int,SDL_Surface*>::iterator s=i.scaled.begin();s!=i.scaled.end();++s)if(s->second)SDL_FreeSurface(s->second);
    i.scaled.clear();
}
void clearTextCache(){for(std::map<std::string,CachedText>::iterator it=textCache.begin();it!=textCache.end();++it)if(it->second.surface)SDL_FreeSurface(it->second.surface);textCache.clear();}
void text(SDL_Surface* s,int x,int y,std::string v,TTF_Font* font,int width=650){
    if(!font){stringRGBA(s,x,y,v.substr(0,width/8).c_str(),240,245,255,255);return;}
    // Text surfaces are retained, and so is the measured width, because SDL_ttf
    // re-renders and re-measures every glyph on each call. Doing that for every
    // label on every animation frame was most of the menu's per-frame cost.
    char prefix[48];::snprintf(prefix,sizeof(prefix),"%p|%d|",(void*)font,width);
    std::string mapKey=std::string(prefix)+v;
    std::map<std::string,CachedText>::iterator found=textCache.find(mapKey);
    if(found!=textCache.end()){
        if(found->second.surface){SDL_Rect r={(Sint16)x,(Sint16)y,0,0};SDL_BlitSurface(found->second.surface,0,s,&r);}
        return;
    }
    CachedText held;
    std::string shown=v;
    if(!shown.empty()){
        TTF_SizeUTF8(font,shown.c_str(),&held.width,0);
        bool clipped=false;
        while(held.width>width&&!shown.empty()){
            size_t pos=shown.size()-1;while(pos&&((unsigned char)shown[pos]&0xc0)==0x80)--pos;shown.resize(pos);clipped=true;
            TTF_SizeUTF8(font,(shown+"...").c_str(),&held.width,0);
        }
        if(clipped)shown+="...";
        SDL_Color c={235,243,255,0};held.surface=TTF_RenderUTF8_Blended(font,shown.c_str(),c);
    }
    if(textCache.size()>=192)clearTextCache();
    textCache[mapKey]=held;
    if(held.surface){SDL_Rect r={(Sint16)x,(Sint16)y,0,0};SDL_BlitSurface(held.surface,0,s,&r);}
}
SDL_Surface* makeScaled(SDL_Surface* source,int n){
    if(!source||n<=0)return 0;
    SDL_Surface* scaled=SDL_CreateRGBSurface(SDL_SWSURFACE,n,n,32,0xff0000,0xff00,0xff,0xff000000);
    if(!scaled)return 0;
    SDL_FillRect(scaled,0,0);
    double scale=std::min(n/(double)source->w,n/(double)source->h);
    SDL_Rect target={(Sint16)((n-source->w*scale)/2),(Sint16)((n-source->h*scale)/2),(Uint16)(source->w*scale),(Uint16)(source->h*scale)};
    if(SDL_SoftStretch(source,0,scaled,&target)!=0){SDL_FreeSurface(scaled);return 0;}
    return scaled;
}
// The focused row grows continuously from 40 to 70 px while it slides. Rounding
// that size to a 5 px ladder keeps every frame on a cached bitmap instead of
// resampling the source icon for each row on each frame.
int iconLadder(int n){return n<30||n>92?n:((n+2)/5)*5;}
SDL_Surface* scaledIcon(Item& i,int n){
    if(n<=0)return 0;
    std::map<int,SDL_Surface*>::iterator found=i.scaled.find(n);
    if(found!=i.scaled.end())return found->second;
    // Bound the per-item cache; 40..70 px in 5 px steps is ten distinct sizes.
    if(i.scaled.size()>=12){
        for(std::map<int,SDL_Surface*>::iterator s=i.scaled.begin();s!=i.scaled.end();++s)if(s->second)SDL_FreeSurface(s->second);
        i.scaled.clear();
    }
    SDL_Surface* made=makeScaled(i.icon,n);
    i.scaled[n]=made;
    return made;
}
void icon(SDL_Surface* s,Item& i,int x,int y,int n){
    if(i.icon){
        // The large details-panel image is static while selected, so cache it.
        if(n>=160){
            if(!i.preview)i.preview=makeScaled(i.icon,176);
            if(i.preview){SDL_Rect dest={(Sint16)x,(Sint16)y,0,0};SDL_BlitSurface(i.preview,0,s,&dest);return;}
        }
        SDL_Surface* scaled=scaledIcon(i,iconLadder(n));
        if(scaled){SDL_Rect dest={(Sint16)x,(Sint16)y,0,0};SDL_BlitSurface(scaled,0,s,&dest);return;}
    }
    roundedBoxRGBA(s,x+n/4,y,x+n*3/4,y+n,5,120,175,220,200);
    boxRGBA(s,x+n/4+4,y+5,x+n*3/4-4,y+n*3/5,30,60,90,255);
}
// Everything that does not move with the selection. Rendered once into a
// retained surface so an animation frame only has to repaint the row band.
void drawChrome(SDL_Surface* t,const std::string& path,const std::vector<std::string>& names,const std::string& message){
    if(background)SDL_BlitSurface(background,0,t,0);else SDL_FillRect(t,0,0);
    text(t,76,38,"PS3ME",big,400);
    text(t,255,48,ps3me::tr("Jogos","Games"),normal,280);
    text(t,990,48,"01.23 / J2ME",small,245);
    text(t,80,108,path,small,1120);
    hlineRGBA(t,80,1198,142,210,235,255,40);
    if(names.empty())text(t,92,302,ps3me::tr("Nenhum jogo aqui. SELECT: atualizar","No games here. SELECT: refresh"),normal,970);
    text(t,80,595,message,small,1120);
    hlineRGBA(t,80,1198,628,220,240,255,35);
    promptLabel(t,80,644,"X",ps3me::tr("Abrir","Open"));
    promptLabel(t,270,644,"O",ps3me::tr("Voltar","Back"));
    promptLabel(t,450,644,"SQUARE",ps3me::tr("Configurações","Settings"));
    promptLabel(t,705,644,"SELECT",ps3me::tr("Atualizar","Refresh"));
    promptLabel(t,965,644,"START",ps3me::tr("Sair","Exit"));
    promptLabel(t,80,681,"L1","USB");
    promptLabel(t,270,681,"R1","HDD");
    promptLabel(t,450,681,"TRIANGLE",ps3me::tr("Resolução","Resolution"));
}
bool chromeIsCurrent(const std::string& path,const std::vector<std::string>& names,int w,int h,const std::string& message){
    return baseValid&&basePath==path&&baseMessage==message&&baseWidth==w&&baseHeight==h&&baseNames==names;
}
bool ensureChrome(const std::string& path,const std::vector<std::string>& names,int w,int h,const std::string& message){
    if(!chromeIsCurrent(path,names,w,h,message)){
        if(!base)base=SDL_CreateRGBSurface(SDL_SWSURFACE,1280,720,32,0xff0000,0xff00,0xff,0);
        if(!base)return false;
        drawChrome(base,path,names,message);
        basePath=path;baseMessage=message;baseNames=names;baseWidth=w;baseHeight=h;
        baseValid=true;
    }
    return true;
}
}
void ps3me_browser_clear(){for(std::map<std::string,Item>::iterator i=cache.begin();i!=cache.end();++i)freeItem(i->second);cache.clear();clearTextCache();}
void ps3me_browser_invalidate(){baseValid=false;lastBand.top=0;lastBand.bottom=720;}
void ps3me_browser_reset_animation(){animationStarted=false;lastFrameMs=0;lastSelectionTarget=-1;}
bool ps3me_browser_animation_active(int selected){return !animationStarted||std::fabs(animatedSelection-(float)selected)>0.001f;}
BrowserBand ps3me_browser_last_band(){return lastBand;}
void ps3me_browser_draw_loading(SDL_Surface* surface,const std::string& path){
    ensureUi();
    if(background)SDL_BlitSurface(background,0,surface,0);else SDL_FillRect(surface,0,0);
    text(surface,76,38,"PS3ME",big,450);
    text(surface,76,300,ps3me::tr("Carregando jogo...","Loading game..."),big,1000);
    std::string filename=path.substr(path.find_last_of('/')==std::string::npos?0:path.find_last_of('/')+1);
    text(surface,80,355,filename,normal,1080);
    hlineRGBA(surface,80,1198,630,210,235,255,40);
}
void ps3me_browser_draw(SDL_Surface* s,const std::string& path,const std::vector<std::string>& names,int selected,int w,int h,const std::string& message){
    ensureUi();
    const bool chromeChanged=!chromeIsCurrent(path,names,w,h,message);
    // Salsa64 navigation: position eases at 14/s rather than snapping by one item.
    // Use elapsed time, not frame count: handles 30/60 FPS and slow frames.
    unsigned int now=SDL_GetTicks();
    if(!animationStarted){animatedSelection=(float)selected;animationStarted=true;lastFrameMs=now;lastSelectionTarget=selected;}
    // An idle menu is not redrawn. When a new selection arrives, restart the
    // animation clock here so the first movement frame does not inherit seconds
    // of idle time and jump most of the way to the target.
    if(selected!=lastSelectionTarget){lastSelectionTarget=selected;lastFrameMs=now;}
    float dt=(float)(now-lastFrameMs)/1000.0f;
    lastFrameMs=now;
    animatedSelection=ps3me::easeMenuSelection(animatedSelection,selected,dt);
    // A frame that only moves the selection repaints the row band; the whole
    // surface is rebuilt only when the chrome itself changed or no retained
    // surface is available on this console.
    if(!ensureChrome(path,names,w,h,message)){
        drawChrome(s,path,names,message);
        lastBand.top=0;lastBand.bottom=720;
    }else if(chromeChanged){
        SDL_BlitSurface(base,0,s,0);
        lastBand.top=0;lastBand.bottom=720;
    }else{
        SDL_Rect from={0,(Sint16)bandTop,1280,(Uint16)(bandBottom-bandTop)};
        SDL_Rect to={0,(Sint16)bandTop,0,0};
        SDL_BlitSurface(base,&from,s,&to);
        lastBand.top=bandTop;lastBand.bottom=bandBottom;
    }
    if(cache.size()>32)ps3me_browser_clear();
    SDL_Rect clip={0,(Sint16)bandTop,1280,(Uint16)(bandBottom-bandTop)};
    SDL_Rect previousClip;
    SDL_GetClipRect(s,&previousClip);
    SDL_SetClipRect(s,&clip);
    int start=std::max(0,(int)std::floor(animatedSelection)-4);
    int stop=std::min((int)names.size(),(int)std::ceil(animatedSelection)+6);
    for(int index=start;index<stop;++index){
        float d=(float)index-animatedSelection;
        float ad=std::fabs(d);
        float yOff=ad<=1.0f?ad*94.0f:94.0f+(ad-1.0f)*54.0f;
        int y=302+(int)((d<0?-1.0f:1.0f)*yOff);
        if(y<150||y>550)continue;
        const std::string p=path+"/"+names[index];
        if(!cache.count(p))cache[p]=load(p,names[index]);
        Item& i=cache[p];
        bool focused=index==selected;
        // Emphasize the visible center during the slide, just as Salsa64 does.
        float focus=1.0f-std::min(1.0f,ad);
        focus=focus*focus*(3.0f-2.0f*focus);
        if(focus>0.01f){
            // Pre-blended tint blitted with surface alpha; the per-pixel
            // blended rectangle cost several milliseconds per frame.
            if(highlight){
                SDL_Rect dest={(Sint16)72,(Sint16)(y-13),0,0};
                SDL_SetAlpha(highlight,SDL_SRCALPHA,(Uint8)(16.0f*focus));
                SDL_BlitSurface(highlight,0,s,&dest);
            }else boxRGBA(s,72,y-13,695,y+81,220,235,255,(Uint8)(16.0f*focus));
        }
        int n=40+(int)(30.0f*focus);
        icon(s,i,92,y,n);
        text(s,190,y,i.title,focus>0.45f?big:normal,480);
        if(focused&&ad<0.35f){
            text(s,193,y+44,i.dir?ps3me::tr("Pasta","Folder"):i.vendor,small,450);
            if(!i.dir){
                icon(s,i,886,224,176);
                text(s,795,431,i.title,normal,385);
                text(s,795,473,i.vendor,small,385);
                char dims[96];
                ::snprintf(dims,sizeof(dims),ps3me::tr("%d x %d / Toque em Controles","%d x %d / Touch in Controls"),w,h);
                text(s,795,520,dims,small,385);
            }
        }
    }
    SDL_SetClipRect(s,&previousClip);
}
