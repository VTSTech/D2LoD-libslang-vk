// platform/linux/d2_linux_globals_step1.cpp — [d2linux M1, STEP-1 ONLY]
//
// Weak stand-ins for the handful of globals/globals.cpp definitions the M1
// foundation slice externs. globals.cpp (the 39K-line central-data TU) embeds
// the game's command dispatch tables, whose closure is the whole client — it
// joins at step 2 (build.cmake's slice ledger), and its STRONG definitions
// then take over from these weak ones automatically. Nothing here changes
// behavior that step 2 would redo:
//   - ExceptionList: the tree's global SEH list head (the PE links its
//     frames onto it and never dispatches).
//   - the s_* constants: the decomp's literal error texts (verbatim content
//     from globals.cpp).
//   - the bit-mask/zlib tables: the canonical table contents globals.cpp
//     also carries (1<<i, ~(1<<i), the standard zlib length-code bits).
//   - MISC_OperatorDeleteShim: routes to free; step 2's strong body routes
//     into the Fog pools instead — the weak link flips with no call-site
//     change.
//   - __GSHandlerCheck_EH_6c9f58 / the language probes: stored-not-called or
//     default-answer, the same never-dispatched contract support/
//     d2_crt_impl.cpp documents for the CRT_ExceptionFilter family.
//
// PE safety: compiles only in the linux build (build.cmake).

#include <cstdint>
#include <cstdlib>

#include "platform/linux/d2_linux_compat.h"
#include "Fog/Engine/Application.h"

extern "C" {

// globals.cpp:38051 — the SEH list head the crash path stores onto.
__attribute__((weak)) void* ExceptionList = (void*)0x00000000;

// globals.cpp:38039 — the fault path's thread self-identity.
__attribute__((weak)) uint32_t dwCurrentThreadId = 0;

// globals.cpp:373/409 — the bit-mask tables (BitMask / BitMaskOr).
__attribute__((weak)) uint32_t gadwBitMasks[32] = {
    0x1, 0x2, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80,
    0x100, 0x200, 0x400, 0x800, 0x1000, 0x2000, 0x4000, 0x8000,
    0x10000, 0x20000, 0x40000, 0x80000, 0x100000, 0x200000, 0x400000, 0x800000,
    0x1000000, 0x2000000, 0x4000000, 0x8000000, 0x10000000, 0x20000000, 0x40000000, 0x80000000
};
__attribute__((weak)) uint32_t gadwInvBitMasks[32] = {
    0xfffffffe, 0xfffffffd, 0xfffffffb, 0xfffffff7, 0xffffffef, 0xffffffdf, 0xffffffbf, 0xffffff7f,
    0xfffffeff, 0xfffffdff, 0xfffffbff, 0xfffff7ff, 0xffffefff, 0xffffdfff, 0xffffbfff, 0xffff7fff,
    0xfffeffff, 0xfffdffff, 0xfffbffff, 0xfff7ffff, 0xffefffff, 0xffdfffff, 0xffbfffff, 0xff7fffff,
    0xfeffffff, 0xfdffffff, 0xfbffffff, 0xf7ffffff, 0xefffffff, 0xdfffffff, 0xbfffffff, 0x7fffffff
};

// globals.cpp:482+ — zlib's length-code tables (the standard RFC 1951 shapes
// the vendored zlib cpp set references).
__attribute__((weak)) int gaZlibExtraBlBits[19] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 3, 7
};

// globals.cpp:1975 — UTF-8 lead-byte marks.
__attribute__((weak)) int gaUTF8LeadByteMarks[8] = { 0x0, 0x0, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0x0 };

// zlib's message table (zutil.h's extern; the vendored cpp set references it
// and carries no definition — upstream zlib keeps it in zutil.c).
extern const char* const z_errmsg[10] __attribute__((weak)) = {
    "need dictionary", "stream end", "", "file error", "stream error",
    "data error", "insufficient memory", "buffer error", "incompatible version", "",
};

}  // extern "C"

// The error-text constants the slice externs (globals.cpp's definitions,
// verbatim content; extern char arrays so the weak attribute is public).
#define D2_WEAK_STR(name, value) \
    extern char name[] __attribute__((weak)) = value

D2_WEAK_STR(s_Unknown_006cc780, "Unknown");
D2_WEAK_STR(s_Do_you_wish_to_terminate_the_appl_006cf1f4, "Do you wish to terminate the application?");
D2_WEAK_STR(s_Press_OK_to_terminate_the_applica_006cf220, "Press OK to terminate the application.");
D2_WEAK_STR(s_ERROR___u__0x_08x__006cf2e0, "ERROR: %u (0x%08x)\n\n");
D2_WEAK_STR(s_Expr___s_006cf250, "Expr: %s");
D2_WEAK_STR(s_File___s_006cf1e8, "File: %s");
D2_WEAK_STR(s_File___s_Line___d__006cf284, "File: %s  Line: %d");
D2_WEAK_STR(s_Function___s__006cf274, "Function: %s");
D2_WEAK_STR(s_Handle___s__006cf25c, "Handle: %s");
D2_WEAK_STR(s_Object___s__006cf268, "Object: %s");
D2_WEAK_STR(s_Program___s__006cf298, "Program: %s");
D2_WEAK_STR(s_This_application_has_encountered__006cf2a8, "This application has encountered a critical error:\n");
D2_WEAK_STR(s__s_006cf248, "%s");
D2_WEAK_STR(s_client_006cc920, "client");
D2_WEAK_STR(s_expand_006cc8f8, "expand");
D2_WEAK_STR(s_launcher_006cc900, "launcher");
D2_WEAK_STR(s_modstate0_006cc928, "modstate0");
D2_WEAK_STR(s_multiplayer_006cc90c, "multiplayer");
D2_WEAK_STR(s_server_006cc918, "server");
D2_WEAK_STR(s_AMD_006cdeb8, "AuthenticAMD");
D2_WEAK_STR(s_Cyrix_006cdeb0, "CyrixInstead");
D2_WEAK_STR(s_Intel_006cdebc, "GenuineIntel");
D2_WEAK_STR(s_PKWARE_Data_Compression_Library_f_0074be60, "PKWARE Data Compression Library for Win32");
D2_WEAK_STR(s_Assertion_Failure_006ccb6c, "Assertion Failure");
D2_WEAK_STR(s_Deadlock_006ccb34, "Deadlock");
D2_WEAK_STR(s_English_006cc81c, "English");
D2_WEAK_STR(s_English_with_Korean_Chat_006cc788, "English (Korean Chat)");
D2_WEAK_STR(s_File_Read_Error_006ccb94, "File Read Error");
D2_WEAK_STR(s_File_Write_Error_006ccb80, "File Write Error");
D2_WEAK_STR(s_French_006cc804, "French");
D2_WEAK_STR(s_German_006cc80c, "German");
D2_WEAK_STR(s_Halt_006ccb64, "Halt");
D2_WEAK_STR(s_Italian_006cc7f0, "Italian");
D2_WEAK_STR(s_Japanese_006cc7e4, "Japanese");
D2_WEAK_STR(s_Korean_006cc7dc, "Korean");
D2_WEAK_STR(s_Missing_Dll_006ccb50, "Missing Dll");
D2_WEAK_STR(s_No_Error_006ccbc8, "No Error");
D2_WEAK_STR(s_Out_of_Memory_Error_006ccba4, "Out of Memory Error");
D2_WEAK_STR(s_Polish_006cc7ac, "Polish");
D2_WEAK_STR(s_Portuguese_006cc7f8, "Portuguese");
D2_WEAK_STR(s_Registry_Error_006ccb40, "Registry Error");
D2_WEAK_STR(s_Russian_006cc7a4, "Russian");
D2_WEAK_STR(s_Simplified_Chinese_006cc7c8, "Simplified Chinese");
D2_WEAK_STR(s_Spanish_006cc814, "Spanish");
D2_WEAK_STR(s_Traditional_Chinese_006cc7b4, "Traditional Chinese");
D2_WEAK_STR(s_Unknown_Error_006ccbb8, "Unknown Error");
D2_WEAK_STR(s_Warning_006ccb5c, "Warning");

namespace Fog { namespace Engine { namespace Application {

// The allocation shim's step-1 body: the plain CRT heap. Step 2's strong
// Application.cpp body routes into the Fog pools instead — the weak link
// flips without any call-site change.
__attribute__((weak)) void MISC_OperatorDeleteShim(void* pMemory) { free(pMemory); }

}}}  // namespace Fog::Engine::Application

namespace D2Lang { namespace StrTable { namespace strtable {

// The language probes SErr/SLog's error reporting calls. Step 1 has no
// language table (strtable.cpp's closure is the Unicode/kamap/MPQ layer —
// step 2's slice); the honest weak bodies answer "default language", which
// is what the PE build answers with an empty language registry value.
__attribute__((weak)) uint32_t MISC_GetLanguageCode() { return 0; }
__attribute__((weak)) uint32_t STRTABLE_GetRegistryLanguage() { return 0; }

}}}  // namespace D2Lang::StrTable::strtable

namespace D2Client { namespace WindowHandle {

// The /GS EH-check thunk SBig.cpp stores by address (globals.h's handler
// table). Never dispatched, like the CRT_ExceptionFilter family — an empty
// body is its whole behavior (support/d2_crt_impl.cpp's documented contract).
__attribute__((weak)) void __GSHandlerCheck_EH_6c9f58(EHExceptionRecord*, EHRegistrationNode*, CONTEXT*, void*) {}

}}  // namespace D2Client::WindowHandle
