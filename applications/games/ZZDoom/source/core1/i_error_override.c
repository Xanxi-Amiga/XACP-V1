/*
 * i_error_override.c - bare-metal i_system replacement
 * Official Julia shared table.
 * ASCII only.
 */
#include "doomtype.h"
#include "i_system.h"
#include <stdarg.h>

#define SHARED_ADDR 0x04300000UL
static volatile unsigned int *sh=(volatile unsigned int*)SHARED_ADDR;

#define SH_STATUS 1
#define SH_CMD    3
#define SH_DIAG   5
#define SH_ERROR  6
#define SH_ERR4   7

#define DIAG(v) do{ sh[SH_DIAG]=(v); __asm__ volatile("dsb":::"memory"); }while(0)

static void wfe_stop(void)
{ sh[SH_CMD]=0; sh[SH_STATUS]=0xFF; while(1){__asm__ volatile("wfe");} }

void I_Error(char *error, ...)
{
    if(error&&error[0])
        sh[SH_ERR4]=((unsigned int)(unsigned char)error[0]<<24)|
                    ((unsigned int)(unsigned char)error[1]<<16)|
                    ((unsigned int)(unsigned char)error[2]<<8)|
                     (unsigned int)(unsigned char)error[3];
    sh[SH_ERROR]=0xEEEEEEEEUL;
    sh[SH_STATUS]=0xEEUL;
    while(1){__asm__ volatile("wfe");}
}

void I_Quit(void)
{
    extern void mmu_disable(void);
    extern void dcache_clean_range(unsigned int,unsigned int);
    dcache_clean_range(0x04300000UL, 256);
    sh[SH_CMD]=0;
    sh[SH_STATUS]=0xFF;
    mmu_disable();
    while(1){__asm__ volatile("wfe");}
}
void I_AtExit(atexit_func_t f, boolean r){(void)f;(void)r;}
boolean I_ConsoleStdout(void){return false;}
void I_PrintBanner(char*t){(void)t;}
void I_PrintDivider(void){}
void I_PrintStartupBanner(char*g){(void)g;}
void I_Tactile(int o,int f,int t){(void)o;(void)f;(void)t;}
boolean I_GetMemoryValue(unsigned int o,void*v,int s){(void)o;(void)v;(void)s;return false;}

#define ZONE_BASE 0x05200000UL
#define ZONE_SIZE (6*1024*1024)
byte *I_ZoneBase(int *size){ *size=ZONE_SIZE; return (byte*)ZONE_BASE; }
