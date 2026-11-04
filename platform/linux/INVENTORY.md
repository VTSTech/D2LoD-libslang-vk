# INVENTORY.md — the M1 Win32-surface inventory and shim ledger (d2linux)

> Status: v1, 2026-11-03 — the M1 inventory pass (PLAN.md §6 M1, first bullet).
> Method: empirical. Every candidate translation unit was compiled with the
> linux forced-include chain (`d2_linux_compat.h` → platform-shadow
> `d2_platform.h` → `tools/vp_host_api.h` → `d2_linux_api.h`) under the i386
> charter flags, and the shim surface grew only at the named call sites the
> compile/link loop surfaced — the inventory-first rule (PLAN.md §7 R2:
> "shims grow only at named call sites; each step keeps CI green").

## 1. What the pass found

The decomp's Windows surface arrives through four doors:

1. **d2_platform.h's non-Windows branch** (the generator's own): scalar/handle
   typedefs and ~50 `extern "C"` Win32 declarations with no implementations —
   the M1 shim's job. It had never been compiled anywhere
   (tools/syntax-check.sh's comment: "one in-built inconsistency that has
   never compiled anywhere" — there is more than one; see §3).
2. **The fork's own host preludes** (`tools/vp_host_shim.h` +
   `tools/vp_host_api.h`, the syntax-check surface): the three glibc holes
   (FILE guards, calling conventions, `errno_t`) and the fork-TU Win32 fill-in
   (file/lstring/version/toolhelp/SEH/critical-section shapes). Both absorbed
   into the linux build by reference (vp_host_api.h) and by upgraded copy
   (vp_host_shim.h → d2_linux_compat.h, fidelity raised from
   declaration-enough to ABI-enough where the slice indexes the struct).
3. **The game TUs' own `#ifndef _WIN32` fallback blocks** (the generator's
   "provided by the Win32 SDK / CRT on Windows" markers): they expect the port
   to supply SDK types (`_devicemodeA`, `tagWNDCLASSA`, `WSAData`,
   `THREADENTRY32`, the wincrypt blob family, `FLOATING_SAVE_AREA`...) —
   supply list extracted by grepping every such block.
4. **MSVC CRT internals** the tree calls by name (`_time32`, `_itoa` family,
   `_strlwr/_strupr`, `strncpy_s`, `_localtime64_s`, `_wfopen`, the `CRT_*`
   x87 intrinsics, `_eh_vector_*_iterator_`, `_except_handler4`) — supplied
   by the PE line's own portable support files (d2_crt_impl.cpp /
   d2_crt_support.cpp, brought in with two guarded edits) plus the shim's CRT
   file.

## 2. The seam as built

```
game TUs (M1 slice, 44 TUs)
    |  -include d2_linux_compat.h      (glibc holes, conventions, SDK structs)
    |  -include d2_platform.h          (the platform shadow — see §3)
    |  -include tools/vp_host_api.h    (fork-curated Win32 fill-in)
    |  -include d2_linux_api.h         (the M1 surface: registry, winsock,
    |                                   threads, toolhelp, conversion, errors)
    v
platform/linux shims
    - d2_linux_shim_win32.cpp    (heap/locks/time/events/threads/toolhelp/
                                  module+memory queries over /proc, text
                                  conversion, errors, winsock over BSD, the
                                  fail-soft families)
    - d2_linux_shim_registry.cpp (the file-backed registry, ~/.config/d2linux)
    - d2_linux_shim_crt.cpp      (MSVC CRT bits, boot/trace twins, the VEH
                                  chain + unhandled filter + translator slots)
    - d2_linux_crash.cpp         (the crash twin: /proc module tables, EBP
                                  walk, dladdr symbolization, signal->CONTEXT,
                                  XDG known-folder)
    - d2_linux_globals_step1.cpp (STEP-1 ONLY: weak globals that yield to
                                  globals.cpp at step 2 — see §5)
    v
SDL3 (static, vendored release-3.4.18) + glibc (i386)
```

The slice (build.cmake's `D2LINUX_GAME_SOURCES`, 44 TUs — the ledger grows
5-10 per step): PKWARE; Fog foundation (SMem, Memory, BitBuffer, File, Fog,
Save, Debug, DataArrays, ErrorManager, LogManager, FogString, Safesock,
AsyncData, Source/{SFile, SSComp}, 3rdparty/crashy + crashy/Report); Storm
foundation (SReg, SErr, SComp, SBig, SMem, SSTR, SSystem, SEVT, SLog, SFile,
WindowHandle, 3rdparty/zlib 9 TUs); both SSignature files; D2Hell's two CRT
support files; globals/Fog.cpp + globals/Storm.cpp.

## 3. The platform shadow (mk-platform-shadow.sh, 12 patches)

tools/syntax-check.sh's temp-shadow pattern, scaled to a real build and
regenerated at configure time so upstream d2_platform.h regens (the R-syncs)
re-apply it. The tree file is never edited; every patch sits inside
`#ifndef _WIN32`, so the PE path is untouched by construction:

1. `typedef void* PRTL_CRITICAL_SECTION_DEBUG;` dropped — the struct-typed
   SDK form stands (vp_host_api.h).
2. `struct CONTEXT { DWORD ContextFlags; };` stub dropped — the compat layer's
   `CONTEXT = _CONTEXT` alias + Fog/Debug.h's real body stand.
3-5. The IMAGE_DOS_HEADER/IMAGE_NT_HEADERS/IMAGE_SECTION_HEADER stubs dropped —
   d2_crash's PE walk needs the REAL x86 layouts (vp_host_api.h).
6. `_LARGE_INTEGER` completed with the `.u` member (the decomp reads
   `.u.LowPart`; the stub only spelled `.s`).
7. `struct CRITICAL_SECTION` retagged onto `_RTL_CRITICAL_SECTION` (the
   globals declare their locks with the SDK's tag; two identical types for
   one lock broke the pointer types).
8. `typedef uint16_t WCHAR` → `typedef wchar_t WCHAR` (the build spells
   -fshort-wchar; MSVC's 16-bit wchar_t is part of the ABI the decomp models,
   and under GCC's flag the types are still distinct).
9. `typedef void* HMODULE/HINSTANCE` → the DECLARE_HANDLE form (Fog/Debug.cpp
   dereferences HMODULE for the `MZ` check, exactly like the SDK shape).
10. `DWORD_PTR/ULONG_PTR` per-arch: `unsigned long` on i386 (MSVC's own 32-bit
    spelling — same TYPE as DWORD, which the decomp's `DWORD*` → `ULONG_PTR*`
    arguments rely on), `uintptr_t` on the x86-64 skeleton.
11. The two stray non-Windows SDK declaration groups (CreateEventW /
    WaitForMultipleObjects / SendMessageA and GetFileSize..timeGetTime) and
    12. the IsBad*/CreateEventA/window block — wrapped in `extern "C"` (the
    game TUs' references and the shim's definitions must agree on one
    linkage; the SDK's own is C).

## 4. The guarded decomp edits (rule 1: #ifdef-guarded, PE-identical)

| File | Edit | Why |
|---|---|---|
| Bnclient/BnSend.h | `struct __time32_t;` forward decl stood down under `D2_LINUX_TIME32_SUPPLIED` | the elaborated decl never parsed anywhere (gcc: "using typedef-name after struct"); the compat prelude supplies the real typedef |
| support/d2_crt_impl.cpp | `<windows.h>` guarded; the `_MSC_VER` CRT-filter block also compiles under `D2LINUX_GAME` | the filters are stored, never dispatched (its own comment) — the same empty body is their whole linux behavior |
| support/d2_crt_support.cpp | the `d2_CRT_Floor` asm label spells `CRT_Floor` under `D2LINUX_GAME` | ELF carries no user-label underscore |
| D2Net/in_addr.h | the simple regen body stood down under `D2_LINUX_SDK_IN_ADDR` | Safesock.cpp's regenerated bodies reference the SDK-faithful union members (`_union_1226`); the compat layer supplies that shape |
| Fog/Src/Safesock.h | the winsock fallback types stood down under `D2_LINUX_WINSOCK_SUPPLIED` | they clash with the glibc types already in scope (timeval via bits/types, fd_set via sys/select); the api header supplies the winsock shapes |

## 5. The step-1 weak globals (d2_linux_globals_step1.cpp)

globals/globals.cpp (the 39K-line central-data TU) embeds the game's command
dispatch tables — its closure is the whole client, so it joins at step 2.
The foundation slice externs a few of its definitions; they are weak-defined
here (verbatim content) and yield automatically to the strong ones when step 2
brings globals.cpp in: ExceptionList, the s_* error texts, the bit-mask and
zlib tables, z_errmsg, MISC_OperatorDeleteShim (free → Fog pools at step 2),
the D2Client GS-handler thunk (stored, never dispatched), and the two language
probes (default-language answers until the strtable layer joins).

## 6. Shim honesty table (every family has named call sites)

| Family | Body | Named sites |
|---|---|---|
| Heap (HeapAlloc/HeapFree/GetProcessHeap) | glibc malloc | Fog pool reservations |
| Critical sections | pthread_mutex in the RTL storage (24 = 24 on i386, static_assert) | Fog/Storm locks |
| Time (GetTickCount/QPC/QPF/Sleep/GetLocalTime/timeGetTime/SystemTimeToFileTime) | CLOCK_MONOTONIC, civil-date math | SBig, LogManager, SMem |
| Registry (RegOpenKeyExA family, 10 entry points) | file-backed store, ~/.config/d2linux/registry.ini, flush-through | SReg.cpp, Application boot |
| Events (CreateEvent*/Set/Reset/WaitForSingleObject) | pthread cond vars, in-process (cross-process names = M5) | crash-dump handshake |
| Threads (CreateThread/Open/Suspend/Resume/Priority) | pthread + tids; SIGSTOP/SIGCONT for suspend | Application crash path, LogManager |
| Toolhelp (snapshot + Thread/Process/Module walks) | /proc/self/task, /proc, dl_iterate_phdr | Fog/File, Fog/Debug, crashy |
| Memory status + VirtualQuery + GetSystemInfo | sysinfo, /proc/self/maps, sysconf | Fog/Memory OOM path, stack walker |
| Module info (GetModuleHandle/FileName/Information) | /proc/self/exe + maps | Fog/Debug symbolizer |
| Text conversion (MultiByteToWideChar/WideCharToMultiByte) | hand-rolled UTF-8/UTF-16 codec, BMP-complete | Fog/File, Debug, Application |
| Errors (FormatMessageA, Get/SetLastError, WSAGetLastError) | code-family tables + TSD | Fog/File, SErr |
| Winsock (socket/connect/send/recv/closesocket/ioctlsocket/select + fd_set) | BSD fds; the winsock fd_set shape macro-mapped; select translates to poll(2) | Safesock client path |
| CreateFileA/ReadFile/WriteFile/SetFilePointer/GetFileSize/FlushFileBuffers/CreateFileMappingA/MapViewOfFile | open(2) + a two-entry fd table + mmap | SFile save paths |
| Fail-soft (service control, CreateProcess, window fns, LoadLibrary, WinVerifyTrust/crypt32, FindResource) | the same failures the PE handles without the feature | Application boot, SFile signature probe |

## 7. Verification (this round)

- sandbox: the full slice compiles i386 and links — `d2linux`, ELF 32-bit,
  statically linked (the smoke run of the i386 binary is CI's ring; this
  container's seccomp refuses static-i386 glibc startup — even a hello world).
- sandbox: the `native` build (skeleton + shim layer, host arch) configures,
  builds and passes `d2linux --smoke` headless.
- sandbox: PE gates **380/380** on the shared-TU diff (tools/check-vanilla-plus.sh).
- CI (this push): `linux-native` flips to `-DD2LINUX_ARCH=i386` + multilib,
  builds and smokes the charter binary (build.yml).

## 8. Step 2 (next slice, toward M2)

The boot chain: globals/globals.cpp + D2Launch + D2Client (ClientModeInGame,
WindowHandle, the GAME::Game core) + D2Multi + D2Common — the AppMode table
Fog/Engine/Application.cpp's boot dispatch references, and the DEBUG_* crash
functions migrate from the weak step-1 file to Application.cpp's strong
bodies (or stay linux-native, by the same weak rule). Then M2's Storm/MPQ
wiring onto the M0 presentation surface.
