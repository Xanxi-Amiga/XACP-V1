
#include "engine/engine.h"

#ifndef _FIFO_H_
#define _FIFO_H_

#define FIFOSIZE 1152*20

struct Fifo {
	unsigned char *Buffer;
	unsigned long Size;
	unsigned long ReadIdx;
	unsigned long WriteIdx;
	long FillLevel;
	long Space;
};

unsigned long fifo_init(struct InitFifoParameters *ifp);
unsigned long fifo_exit(struct ExitFifoParameters *efp);
unsigned long fifo_clear(struct ClearFifoParameters *cfp);

#endif

