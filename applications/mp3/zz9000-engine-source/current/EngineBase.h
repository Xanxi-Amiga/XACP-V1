/*
** Base definition of Engine shared library
**
** Created by: Thomas Wenzel  Sept 2021
*/

#ifndef ENGINE_ENGINEBASE_H
#define ENGINE_ENGINEBASE_H

#ifdef   __MAXON__
#ifndef  EXEC_LIBRARIES_H
#include <exec/libraries.h>
#endif
#else
#ifndef  EXEC_LIBRARIES
#include <exec/libraries.h>
#endif /* EXEC_LIBRARIES_H */
#endif
#include <dos/dos.h>

struct EngineBase {
	struct Library         eb_LibNode;
	BPTR                   eb_SegList;
	struct ExecBase       *eb_SysBase;
	struct DosBase        *eb_DOSBase;
};

#endif /* ENGINE_ENGINEBASE_H */
