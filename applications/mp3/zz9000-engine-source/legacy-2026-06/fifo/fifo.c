
#include "engine/engine.h"
#include "fifo.h"
#include "hton.h"
#include "../MyStdlib.h"

unsigned long fifo_init(struct InitFifoParameters *ifp) {
	struct Fifo *itsFifo;

	if(itsFifo = my_malloc(sizeof(struct Fifo))) {
		ifp->Size          = HTONL(FIFOSIZE);
		itsFifo->Buffer    = my_malloc(FIFOSIZE);
		itsFifo->Size      = FIFOSIZE;
		itsFifo->ReadIdx   = 0;
		itsFifo->WriteIdx  = 0;
		itsFifo->FillLevel = 0;
		itsFifo->Space     = FIFOSIZE;
		return (unsigned long)itsFifo;
	}
	ifp->Size = HTONL(0);
	return HTONL(0);
}

unsigned long fifo_exit(struct ExitFifoParameters *efp) {
	struct Fifo *itsFifo = efp->itsFifo;
	if(itsFifo) {
		if(itsFifo->Buffer) my_free(itsFifo->Buffer);
		my_free(itsFifo);
	}
	return HTONL(0);
}

unsigned long fifo_clear(struct ClearFifoParameters *cfp) {
	struct Fifo *itsFifo = cfp->itsFifo;

	if(itsFifo) {
		cfp->Size		   = HTONL(FIFOSIZE);
		itsFifo->Size      = FIFOSIZE;
		itsFifo->ReadIdx   = 0;
		itsFifo->WriteIdx  = 0;
		itsFifo->FillLevel = 0;
		itsFifo->Space     = FIFOSIZE;
	}
	else {
		cfp->Size = HTONL(0);
	}
	return HTONL(0);
}

