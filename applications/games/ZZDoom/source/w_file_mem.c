#include <string.h>
#include "w_file.h"
#include "z_zone.h"

extern void   *g_wad_ptr;
extern size_t  g_wad_size;

static wad_file_t *W_Mem_OpenFile(char *path);
static void        W_Mem_CloseFile(wad_file_t *wad);
static size_t      W_Mem_Read(wad_file_t *wad, unsigned int offset,
                               void *buffer, size_t len);

wad_file_class_t mem_wad_file = {
    W_Mem_OpenFile,
    W_Mem_CloseFile,
    W_Mem_Read,
};

static wad_file_t *W_Mem_OpenFile(char *path)
{
    wad_file_t *result;
    (void)path;
    /* Marker: W_Mem_OpenFile entered */
    { volatile unsigned int *f=(volatile unsigned int *)0x0470003CUL; *f=0xD008; }
    if(!g_wad_ptr||!g_wad_size){
        volatile unsigned int *f=(volatile unsigned int *)0x0470003CUL; *f=0xDEA8;
        return NULL;
    }
    result = Z_Malloc(sizeof(wad_file_t), PU_STATIC, 0);
    result->file_class = &mem_wad_file;
    result->mapped     = (byte*)g_wad_ptr;
    result->length     = (unsigned int)g_wad_size;
    ((volatile unsigned int*)0x04300000UL)[5]=0xA009UL;
    /* Marker: W_Mem_OpenFile OK */
    { volatile unsigned int *f=(volatile unsigned int *)0x0470003CUL; *f=0xD009; }
    return result;
}
static void W_Mem_CloseFile(wad_file_t *wad){(void)wad;}
static size_t W_Mem_Read(wad_file_t *wad, unsigned int offset,
                         void *buffer, size_t len)
{
    if(offset>=wad->length) return 0;
    if(offset+len>wad->length) len=wad->length-offset;
    memcpy(buffer,(byte*)wad->mapped+offset,len);
    return len;
}
