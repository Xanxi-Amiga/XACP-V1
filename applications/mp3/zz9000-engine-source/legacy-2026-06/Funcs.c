
/*
** Engine library functions implementation
**
** Created by: Thomas Wenzel  Sept 2021
*/

#include <exec/types.h>
#include <exec/memory.h>
#include <proto/exec.h>

#include "Compiler.h"

// Funcs knows EngineAccess.
#include "EngineAccess.h"

static struct SignalSemaphore EngineSemaphore;

 /* Please note, that &EngineBase always resides in register __a6 as well,
    but if we don't need it, we need not reference it here.

    Also note, that registers a0, a1, d0, d1 always are scratch registers,
    so you usually should only *pass* parameters there, but make a copy
    directly after entering the function. To avoid problems of kind
    "implementation defined behaviour", you should make a copy of A6 too,
    when it is actually used.
  */
char* Engine_getName(void) {
	return cEngine_getName();
}
 
char* Engine_getCapability(REGD1(unsigned long Capability)) {
	return cEngine_getCapability(Capability);
}

void Engine_start(void) {
	InitSemaphore(&EngineSemaphore);
	cEngine_start();
}

void Engine_stop(void) {
	cEngine_stop();
}

/* Obtain shared memory access semaphore. */
void Engine_obtain(void) {
	ObtainSemaphore(&EngineSemaphore);
}

/* Release shared memory access semaphore. */
void Engine_release(void) {
	ReleaseSemaphore(&EngineSemaphore);
}

unsigned long Engine_doOperation(REGD1(unsigned long Operation), REGA1(void* Data)){
	return cEngine_doOperation(Operation, Data);
}

