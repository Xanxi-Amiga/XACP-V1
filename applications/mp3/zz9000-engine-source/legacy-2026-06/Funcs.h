#include "engine/engine.h"

#include "Compiler.h"

extern char *Engine_getName(void);
extern char *Engine_getCapability(unsigned long Capability);
extern void Engine_start(void);
extern void Engine_stop(void);
extern void Engine_obtain(void);
extern void Engine_release(void);
extern unsigned long Engine_doOperation(REGD1(unsigned long Operation), REGA1(void* Data));

