/*
** C interface for engine access
**
** Created by: Thomas Wenzel  Sept 2021
*/

#include <exec/types.h>
#include <exec/memory.h>
#include <proto/exec.h>

// EngineAccess knows and requires iEngine.
#include "iEngine.h"

static void getMemory(struct SharedMemoryParameters *smp) {
	smp->NumFrames  = iEngine_getNumFrames();
	smp->DstBuffer  = iEngine_getOutBuffer();
	smp->SpecRawL   = iEngine_getSpecRawL();
	smp->SpecRawR   = iEngine_getSpecRawR();
	smp->EqualizerL = iEngine_getEqualizerL();
	smp->EqualizerR = iEngine_getEqualizerR();
}

/* Get Engine name
 * Return the name as string pointer. */
char* cEngine_getName(void) {
	return iEngine_get_name();
}
 
/* Get Engine capability.
 * [Capability] is one of the supported capability indexes.
 * Return the requested capability as string pointer. */
char* cEngine_getCapability(unsigned long capability) {
	return iEngine_get_capability(capability);
}

/* Start Engine, i.e. initialize all decoders. */
void cEngine_start(void) {
	iEngine_amp_start();
}

/* Stop Engine, i.e. uninitialize all decoders. */
void cEngine_stop(void) {
	iEngine_amp_stop();
}

/* Do Engine operation.
 * [Operation] is one of the supported operation indexes.
 * [Data] should point to a structure containing the operation data.
 * Return the result of the indivitual operation. */
unsigned long cEngine_doOperation(unsigned long Operation, void* Data) {
	unsigned long Result = 0;
	switch(Operation) {
		case ENGINEOP_GET_MEMORY:
			getMemory(Data);
		break;

		case ENGINEOP_INIT_FIFO:
			Result = iEngine_fifo_init(Data);		
		break;
		case ENGINEOP_EXIT_FIFO:
			Result = iEngine_fifo_exit(Data);		
		break;
		case ENGINEOP_CLEAR_FIFO:
			Result = iEngine_fifo_clear(Data);		
		break;
		case ENGINEOP_FILL_FIFO:
			Result = iEngine_fifo_fill(Data);		
		break;

		case ENGINEOP_INIT_EQUALIZER:
			Result = iEngine_equalizer_init(Data);		
		break;
		case ENGINEOP_EXIT_EQUALIZER:
			Result = iEngine_equalizer_exit(Data);		
		break;
		case ENGINEOP_CONFIG_EQUALIZER:
			Result = iEngine_equalizer_config(Data);		
		break;
		case ENGINEOP_SET_EQUALIZER:
			Result = iEngine_equalizer_set(Data);		
		break;
		case ENGINEOP_RUN_EQUALIZER:
			Result = iEngine_equalizer_run(Data);		
		break;

		case ENGINEOP_INIT_AMP:
			Result = iEngine_amp_init(Data);		
		break;
		case ENGINEOP_EXIT_AMP:
			Result = iEngine_amp_exit(Data);		
		break;
		case ENGINEOP_DECODE_AMP:
			Result = iEngine_amp_decode(Data); 	
		break;
	}
	return Result;
}

