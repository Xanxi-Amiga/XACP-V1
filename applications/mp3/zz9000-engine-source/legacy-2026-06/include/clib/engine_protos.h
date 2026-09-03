#ifndef CLIB_ENGINE_PROTOS_H
#define CLIB_ENGINE_PROTOS_H

#ifndef ENGINE_ENGINE_H
#include <engine/engine.h>
#endif /* ENGINE_ENGINE_H */

/* Get Engine name
 * Return the name as string pointer. */
char *Engine_getName(void);

/* Get Engine capability.
 * [Capability] is one of the supported capability indexes.
 * Return the requested capability as string pointer. */
char *Engine_getCapability(unsigned long capability);

/* Obtain shared memory access semaphore. */
void Engine_obtain(void);

/* Release shared memory access semaphore. */
void Engine_release(void);

/* Do Engine operation.
 * [Operation] is one of the supported operation indexes.
 * [Data] should point to a structure containing the operation data.
 * Return the result of the indivitual operation. */
unsigned long Engine_doOperation(unsigned long Operation, void* Data);

#endif /* CLIB_ENGINE_PROTOS_H */

