/*
** C interface for engine access
**
** Created by: Thomas Wenzel  Sept 2021
*/

#ifndef ENGINE_H
#define ENGINE_H

#define __USE_SYSBASE        // perhaps only recognized by SAS/C

#include <exec/types.h>
#include <exec/memory.h>
#include <proto/exec.h>

/* Get Engine name
 * Return the name as string pointer. */
char* cEngine_getName(void);

/* Get Engine capability.
 * [Capability] is one of the supported capability indexes.
 * Return the requested capability as string pointer. */
char* cEngine_getCapability(unsigned long capability);

/* Start Engine, i.e. initialize all decoders. */
void cEngine_start(void);

/* Stop Engine, i.e. uninitialize all decoders. */
void cEngine_stop(void);

/* Do Engine operation.
 * [Operation] is one of the supported operation indexes.
 * [Data] should point to a structure containing the operation data.
 * Return the result of the indivitual operation. */
unsigned long cEngine_doOperation(unsigned long Operation, void* Data);

#endif

