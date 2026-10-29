// Minimal Windows shim for HOST-SIDE Linux builds of the frame TU
// (spike/raster_regress.cpp). Not shipped, not used by the DLL — the DLL
// builds under real MinGW headers. Only the symbols g3x_frame.cpp pulls in
// transitively (vk_backend.h's <windows.h> include) or directly (the
// present-seam hotkey polls).
#pragma once

#ifndef _WIN32

typedef unsigned long    DWORD;
typedef unsigned short   WORD;
typedef unsigned char    BYTE;
typedef int              BOOL;
typedef unsigned int     UINT;
typedef void*            HWND;
typedef void*            HINSTANCE;
typedef void*            HANDLE;
typedef unsigned short   SHORT;
typedef unsigned long long ULONGLONG;

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

#define VK_SCROLL 0x91

inline SHORT       GetAsyncKeyState(int /*vkey*/)  { return 0; }
inline ULONGLONG   GetTickCount64()                { return 0; }
inline HWND        GetForegroundWindow()           { return nullptr; }

#endif // _WIN32
