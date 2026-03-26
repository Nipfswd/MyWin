/*++

Copyright (c) 1989-1993  Noahssoft Corporation

Module Name:

    stubs.c

Abstract:

    This module implements bug check and system shutdown code.

Author:

    Noah Juopperi (noahju) 30-Aug-2025

Environment:

    Kernel mode only.

Revision History:

--*/

#include "ki.h"
#define NOEXTAPI
#include "wdbgexts.h"
#include <inbv.h>
#include <hdlsblk.h>
#include <hdlsterm.h>
//
//
//

extern KDDEBUGGER_DATA64 KdDebuggerDataBlock;

extern PVOID ExPoolCodeStart;
extern PVOID ExPoolCodeEnd;
extern PVOID MmPoolCodeStart;
extern PVOID MmPoolCodeEnd;
extern PVOID MmPteCodeStart;
extern PVOID MmPteCodeEnd;

extern PWD_HANDLER ExpWdHandler;
extern PVOID       ExpWdHandlerContext;

#if defined(_AMD64_)

#define PROGRAM_COUNTER(_trapframe) ((_trapframe)->Rip)

#elif defined(_X86_)

#define PROGRAM_COUNTER(_trapframe) ((_trapframe)->Eip)

#elif defined(_IA64_)

#define PROGRAM_COUNTER(_trapframe) ((_trapframe)->StIIP)

#else

#error "no target architecture"

#endif

//
// Define forward referenced prototypes.
//

VOID
KiScanBugCheckCallbackList (
    VOID
    );