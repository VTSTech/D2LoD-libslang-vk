// Startup tracing. The binary is -mwindows, so there is no console and printf
// goes nowhere. This writes to both OutputDebugStringA (visible under
// WINEDEBUG=+debugstr) and an unbuffered file, so a hang still leaves a trail.
#pragma once

extern "C" void D2Trace(const char* pszWhere);

// Same, with arguments. Deliberately wsprintfA rather than snprintf: it is in
// user32, needs no CRT state, and so still works while the error handler is
// unwinding. Only the %-specifiers wsprintfA supports are safe here.
extern "C" void D2TraceFmt(const char* pszFormat, ...);

// Write a crash report for a Fog halt (support/d2_crash.cpp): the line, the
// call-site address and the halting thread's stack. Declared here because the
// generated ErrorManager.cpp already includes this header.
extern "C" void D2CRASH_ReportHalt(int nLine, unsigned nAddress);

// Last-chance report for an unhandled exception (support/d2_crash.cpp): Fog's
// top-level filter calls it right before it gives up on the exception, so a
// fatal fault in a DLL-own thread (no game frames on the stack) still gets a
// full report with the module list. void* is _EXCEPTION_POINTERS*, kept as
// pointer-to-void so this header stays free of windows.h.
extern "C" void D2CRASH_ReportUnhandled(void* pExceptionPointers);
extern "C" int D2ProbeReadTileClipYMin(void);
extern "C" void D2ProbeArmClipGuard(void);
extern "C" void D2ProbeArmClipHwbpSelf(void);
extern "C" void* D2ProbeWrapThreadStart(void* pfnStart, void* pArg, void** ppArgOut);
extern "C" void D2ProbeCheckBuffer(const char* pszWho, const void* pBuf, unsigned nLen, void* pRet);
extern "C" void D2ProbeArmClipHwbp(void);

#define D2TRACE(where) D2Trace(where)

extern "C" void D2TRACE_WatchWrite(void* pAddress);

extern unsigned g_anClientPktCount[256];
extern unsigned g_anServerPktCount[256];
