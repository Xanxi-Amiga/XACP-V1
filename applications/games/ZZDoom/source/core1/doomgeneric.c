#include <stdio.h>

#include "m_argv.h"

#include "doomgeneric.h"

pixel_t* DG_ScreenBuffer = NULL;

void M_FindResponseFile(void);
void D_DoomMain (void);


void doomgeneric_Create(int argc, char **argv)
{
    volatile unsigned int *shared = (volatile unsigned int *)0x04300000UL;

	// save arguments
    myargc = argc;
    myargv = argv;

	M_FindResponseFile();

	DG_ScreenBuffer = malloc(DOOMGENERIC_RESX * DOOMGENERIC_RESY * 4);
    shared[5] = (DG_ScreenBuffer != NULL) ? 0xA006UL : 0xDEADUL;

	DG_Init();
    shared[5] = 0xA00AUL;

	D_DoomMain ();
}

