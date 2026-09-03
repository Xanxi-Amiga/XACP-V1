/*
** Library startup-code and function table definition
**
** Based on CLib37x by Andreas R. Kleinert
*/

#define __USE_SYSBASE

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/libraries.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/initializers.h>

#include <proto/exec.h>

#include "Compiler.h"
#include "EngineBase.h"
#include "Funcs.h"


extern ULONG __stdargs L_OpenLibs(struct EngineBase *EngineBase);
extern void  __stdargs L_CloseLibs(void);

struct EngineBase *InitLib(REGA6(struct ExecBase *sysbase), REGA0(BPTR seglist), REGD0(struct EngineBase *eb));
struct EngineBase *OpenLib(REGA6(struct EngineBase *EngineBase));
BPTR CloseLib(REGA6(struct EngineBase *EngineBase));
BPTR ExpungeLib(REGA6(struct EngineBase *EngineBase));
ULONG ExtFuncLib(void);


/* ----------------------------------------------------------------------------------------
   ! LibStart:
   !
   ! If someone tries to start a library as an executable, it must return (LONG) -1
   ! as result. That's what we are doing here.
   ---------------------------------------------------------------------------------------- */

LONG LibStart(void) {
	return(-1);
}


/* ----------------------------------------------------------------------------------------
   ! Function and Data Tables:
   !
   ! The function and data tables have been placed here for traditional reasons.
   ! Placing the RomTag structure before (-> LibInit.c) would also be a good idea,
   ! but it depends on whether you would like to keep the "version" stuff separately.
   ---------------------------------------------------------------------------------------- */

extern APTR FuncTab [];
extern struct MyDataInit DataTab;
// extern DataTab; /* DICE fix */
                                  /* Instead you may place ROMTag + Datatab directly, here */
                                  /* (see LibInit.c). This may fix "Installer" version     */
                                  /* checking problems, too - try it.                      */

struct InitTable {                       /* do not change */
	ULONG              LibBaseSize;
	APTR              *FunctionTable;
	struct MyDataInit *DataTable;
	APTR               InitLibTable;
} InitTab = {
	(ULONG)               sizeof(struct EngineBase),
	(APTR              *) &FuncTab[0],
	(struct MyDataInit *) &DataTab,
	(APTR)                InitLib
};

APTR FuncTab [] = {
	(APTR)OpenLib,
	(APTR)CloseLib,
	(APTR)ExpungeLib,
	(APTR)ExtFuncLib,

	(APTR)Engine_getName,
	(APTR)Engine_getCapability,
	(APTR)Engine_obtain,
	(APTR)Engine_release,
	(APTR)Engine_doOperation,

	(APTR)((LONG)-1)
};


extern struct EngineBase *EngineBase;

/* ----------------------------------------------------------------------------------------
   ! InitLib:
   !
   ! This one is single-threaded by the Ramlib process. Theoretically you can do, what
   ! you like here, since you have full exclusive control over all the library code and data.
   ! But due to some bugs in Ramlib V37-40, you can easily cause a deadlock when opening
   ! certain libraries here (which open other libraries, that open other libraries, that...)
   !
   ---------------------------------------------------------------------------------------- */

struct EngineBase * InitLib(REGA6(struct ExecBase *sysbase), REGA0(BPTR seglist), REGD0(struct EngineBase *eb)) {
	EngineBase = eb;
	ULONG negsize, possize, fullsize;
	UBYTE *negptr = (UBYTE *) EngineBase;

	EngineBase->eb_SysBase = sysbase;
	EngineBase->eb_SegList = seglist;


	if(L_OpenLibs(EngineBase)) {
		Engine_start();
		return(EngineBase);
	}

	L_CloseLibs();


	negsize  = EngineBase->eb_LibNode.lib_NegSize;
	possize  = EngineBase->eb_LibNode.lib_PosSize;
	fullsize = negsize + possize;
	negptr  -= negsize;

	FreeMem(negptr, fullsize);
	return NULL;
}

/* ----------------------------------------------------------------------------------------
   ! OpenLib:
   !
   ! This one is enclosed within a Forbid/Permit pair by Exec V37-40. Since a Wait() call
   ! would break this Forbid/Permit(), you are not allowed to start any operations that
   ! may cause a Wait() during their processing. It's possible, that future OS versions
   ! won't turn the multi-tasking off, but instead use semaphore protection for this
   ! function.
   !
   ! Currently you only can bypass this restriction by supplying your own semaphore
   ! mechanism.
   ---------------------------------------------------------------------------------------- */

struct EngineBase * OpenLib(REGA6(struct EngineBase *EngineBase)) {
	EngineBase->eb_LibNode.lib_OpenCnt++;

	EngineBase->eb_LibNode.lib_Flags &= ~LIBF_DELEXP;

	return EngineBase;
}

/* ----------------------------------------------------------------------------------------
   ! CloseLib:
   !
   ! This one is enclosed within a Forbid/Permit pair by Exec V37-40. Since a Wait() call
   ! would break this Forbid/Permit(), you are not allowed to start any operations that
   ! may cause a Wait() during their processing. It's possible, that future OS versions
   ! won't turn the multi-tasking off, but instead use semaphore protection for this
   ! function.
   !
   ! Currently you only can bypass this restriction by supplying your own semaphore
   ! mechanism.
   ---------------------------------------------------------------------------------------- */

BPTR CloseLib(REGA6(struct EngineBase *EngineBase)) {
	EngineBase->eb_LibNode.lib_OpenCnt--;

	if(!EngineBase->eb_LibNode.lib_OpenCnt) {
		if(EngineBase->eb_LibNode.lib_Flags & LIBF_DELEXP) {
			return ExpungeLib(EngineBase);
		}
	}

	return NULL;
}

/* ----------------------------------------------------------------------------------------
   ! ExpungeLib:
   !
   ! This one is enclosed within a Forbid/Permit pair by Exec V37-40. Since a Wait() call
   ! would break this Forbid/Permit(), you are not allowed to start any operations that
   ! may cause a Wait() during their processing. It's possible, that future OS versions
   ! won't turn the multi-tasking off, but instead use semaphore protection for this
   ! function.
   !
   ! Currently you only could bypass this restriction by supplying your own semaphore
   ! mechanism - but since expunging can't be done twice, one should avoid it here.
   ---------------------------------------------------------------------------------------- */

BPTR ExpungeLib(REGA6(struct EngineBase *eb)) {
	struct EngineBase *EngineBase = eb;
	BPTR seglist;

	if(!EngineBase->eb_LibNode.lib_OpenCnt) {
		ULONG negsize, possize, fullsize;
		UBYTE *negptr = (UBYTE *) EngineBase;

		seglist = EngineBase->eb_SegList;

		Remove((struct Node *)EngineBase);

		Engine_stop();
		L_CloseLibs();

		negsize  = EngineBase->eb_LibNode.lib_NegSize;
		possize  = EngineBase->eb_LibNode.lib_PosSize;
		fullsize = negsize + possize;
		negptr  -= negsize;

		FreeMem(negptr, fullsize);

		return seglist;
	}

	EngineBase->eb_LibNode.lib_Flags |= LIBF_DELEXP;

	return NULL;
}

/* ----------------------------------------------------------------------------------------
   ! ExtFunct:
   !
   ! This one is enclosed within a Forbid/Permit pair by Exec V37-40. Since a Wait() call
   ! would break this Forbid/Permit(), you are not allowed to start any operations that
   ! may cause a Wait() during their processing. It's possible, that future OS versions
   ! won't turn the multi-tasking off, but instead use semaphore protection for this
   ! function.
   !
   ! Currently you only can bypass this restriction by supplying your own semaphore
   ! mechanism - but since this function currently is unused, you should not touch
   ! it, either.
   ---------------------------------------------------------------------------------------- */

ULONG ExtFuncLib(void) {
	return NULL;
}

struct EngineBase *EngineBase = NULL;

