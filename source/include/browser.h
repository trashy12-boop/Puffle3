#ifndef RUFFLE_PS3_BROWSER_H
#define RUFFLE_PS3_BROWSER_H
#include <stddef.h>
#include <stdint.h>
#define RP_BROWSER_MAX 256
#define RP_PATH_MAX 768
#define RP_NAME_MAX 255
typedef struct { char name[RP_NAME_MAX+1]; int directory; uint64_t size; } RpEntry;
typedef struct { char root[RP_PATH_MAX], current[RP_PATH_MAX]; RpEntry items[RP_BROWSER_MAX]; unsigned count; int truncated; } RpBrowser;
int rp_browser_open(RpBrowser *b,const char *root);
int rp_browser_refresh(RpBrowser *b);
int rp_browser_enter(RpBrowser *b,unsigned index);
int rp_browser_parent(RpBrowser *b);
int rp_browser_path(const RpBrowser *b,unsigned index,char *out,size_t cap);
/* Header inspection only: does not decompress or validate the SWF body. */
int rp_swf_header(const char *path,uint32_t *declared_size,unsigned *version);
#endif
