# PLAN.md — d2linux: the native Linux host track (D2LoD-libslang-vk)

> Status: v6, 2026-10-31 — **this plan is the native Linux port (branch `linux`)**. It supersedes the
> libslang-vk integration plan that lived here (v1–v5): that workflow is closed — the wrapper line
> `Glide3xVK` finished at **0.4.5** (`3a52553`), P1–P3 delivered and shipped inside the Vanilla+
> releases through **`D2LoD-Vanilla-Plus-0.0.6`**, and its record lives in `VANILLA-PLUS.md`,
> `HISTORY.md` and `handoff.md`. Track charter: `docs/linux-port.md`. Live task list: `handoff.md`
> §9. Journal: `/home/z/my-project/worklog.md`. Owner: VTSTech. Agent session log: `handoff.md` Rev.
>
> **M0 is FIELD-PROVEN** (owner Ubuntu VM, build `c8e8d54`): banner + `--smoke` green, the animated
> pattern live, and after the libdecor build fix plain `./d2linux` shows titlebar + resize on GNOME
> Wayland. CI on this branch is the **linux-native job only** — the mingw-i686 PE job was retired by
> the owner; the PE gate is the owner's local Win11 llvm-mingw build + the manual gate suite (240/240).

---

## 0. Branching & git workflow

| Branch | Role |
|---|---|
| [`main`](https://github.com/VTSTech/D2LoD-libslang-vk) | **The d2.exe source line (default branch).** Byte-match charter (retail 1.14d Game.exe), Vanilla+ R-rounds (`Vanilla+0.0.6-R59` self-ID), mingw-w64 i686 PE builds. Built locally on the owner's Win11 box (`tools/build-local-win11.ps1`) — the standing "full build is the owner's gate" convention — and verified by the manual `tools/check-vanilla-plus.sh` suite (240/240). **Untouched by this plan**; the linux track must never break it. |
| [`linux`](https://github.com/VTSTech/D2LoD-libslang-vk/tree/linux) | **Active development line — this plan.** `d2linux`, the native ELF host. Milestone ladder in §5. |
| [`Glide3xVK`](https://github.com/VTSTech/D2LoD-libslang-vk/tree/Glide3xVK) | The glide3x→Vulkan→libslang-vk wrapper line — complete at 0.4.5, shipped; maintenance only. The linux track does not depend on it (a native slang chain is a post-M5 stretch, §5). |
| `pr2/…`–`pr5/…` | Upstream-PR lines to jaenster/redivivus (sound series = upstream PR #10, open). |

Rules:

- Every phase below commits to `linux` only. The two tracks share the decompiled game source;
  **any decomp translation-unit change made for Linux is `#ifdef`-guarded and must leave the PE
  build byte-identical** (refereed by the PE gates + the owner's Win11 build; PE-line files
  `FEATURES.md`, `SMOKE-CHECKLIST.md`, `check-vanilla-plus.sh`, `Application.cpp` self-ID are
  not touched for Linux-only work).
- **Delivery convention: overlay zips.** The sandbox holds no GitHub credentials — changes ship
  as `download/<name>-overlay.zip` with a `README-FIRST.txt`; the owner applies them as their own
  commits. No `.patch` files.
- **Exec-bit hygiene:** git modes are commit properties, invisible from a Windows checkout; zip
  round-trips cannot carry them. The canonical CI call is mode-immune —
  `bash tools/check-build-linux.sh …` (build.yml). Mode audits go by
  `git ls-tree HEAD tools/` (want `100755` for shell gates; 2026-10-31 audit: the bit never
  landed on the canonical line — tree shows `100644` throughout; the bash prefix carries CI, so
  this is cosmetic, not a defect).

---

## 1. Goal

Run Diablo II: LoD 1.14d (Redivivus recovered source) as a **native Linux ELF binary that *is*
the game host** — no Windows, no Wine, no PE. Concretely: `./d2linux` on an Ubuntu/GNOME Wayland
session opens a decorated, resizable window and plays the game (single player first), with
D2's own presentation model intact: 8-bit paletted surfaces composited by the host, retail
22.05 kHz S16-stereo sound shape, and the game's 4:3 logical size letterboxed to the desktop.

Non-goals for this track: Battle.net (upstream limitation), byte-matching anything (impossible
for a port — verification is behavioral), changing the PE line's charter, and re-porting the
glide3x wrapper (the shader-preset chain is a stretch goal only after M5).

## 2. Where we are (baseline, 2026-10-31)

| Item | Value |
|---|---|
| Artifact | `build-linux/d2linux`, native ELF **i386 with the M1 slice** (the `native` skeleton remains for shim work), ~15.4 MB static-i386 / ~6.4 MB static-x86-64, static SDL3 |
| Toolchain | host gcc/clang + vendored SDL3 submodule `external/SDL` pinned **release-3.4.18** (gitlink `829a65d`) |
| Arch policy | `D2LINUX_ARCH` default **i386** (`-m32`, what game code requires, needs multilib); `native` escape used by M0/CI/sandbox |
| CI | one job, `linux-native` (ubuntu-24.04): full X11/Wayland dev set → configure → build → `--smoke` verdict, via `tools/check-build-linux.sh` (clean-clone mirror: temp clone → submodule init → configure → build → smoke) |
| Field status | **M0 field-proven** on the owner's Ubuntu VM (Wayland/GNOME): banner `d2linux scaffold c8e8d54`, `d2linux smoke OK (3 frames, dummy video/audio)`, animated INDEX8 pattern, titlebar + resize without parameters after the libdecor build fix |
| Sandbox | no GitHub creds (overlay-zip delivery), no X11 dev libraries — configure via `SDL_UNIX_CONSOLE_BUILD=ON` console path; full configure+build+smoke end-to-end is proven |

How M0 got there — three CI defect rounds, each root-caused **in-source** (not flakiness):

1. **exit 126 / Permission denied** — the scaffold recorded the gate scripts as git mode
   `100644`; the exec bit is a commit property and cannot be set from a Windows checkout.
   Fixed for CI by the mode-immune `bash` prefix (content fix), documented for later canonical
   recording.
2. **SDL3 XTEST configure fatal** — vendored `external/SDL/cmake/sdlchecks.cmake` treats a
   missing X11 sub-feature as fatal (first one: XTEST). `libxtst-dev` was the only gap vs
   SDL3's README-linux build-dep set (XRender arrives via `libxcursor-dev`; SDL3 vendors its
   Wayland protocols; pulse/jack/KMSDRM disable non-fatally).
3. **Bare window on GNOME Wayland** — `SDL_VIDEODRIVER=x11` showed a titlebar, plain Wayland
   did not → `SDL_WAYLAND_LIBDECOR` is a **soft build-time dep**: without `libdecor-0-dev` SDL3
   compiles with no decoration support at all (WARNING-only), and Mutter offers no server-side
   xdg-decoration fallback (KDE does). Fixed by adding `libdecor-0-dev` to the runner apt set.

## 3. The seam: the game still assumes Win32; the host replaces the platform layer

The decompiled tree is Windows-PE-shaped by construction. The port seam is the same one the
PE line's own layering already draws — D2 speaks DirectDraw/DirectSound/Winsock/registry/SEH
to the outside world, and `d2_platform.h` models the 32-bit MSVC x86 ABI it was recovered
against (byte-annotated struct offsets carry embedded 4-byte pointers — on x86-64 every
pointer-bearing struct changes layout and game state corrupts; hence the i386 charter, §5/§7).

The M0 skeleton already proves the presentation half of the seam: **INDEX8 palettized staging
surface → palette lookup → XRGB8888 streaming texture upload → letterboxed logical present** is
exactly DirectDraw's surface/palette model carried onto SDL3's renderer, and SDL3's
device-stream audio maps DirectSound mixing directly. What remains is the shim work of M1–M3
(§6), not a renderer redesign.

Prior art for the shape: the same house pattern as the wrapper line — a self-contained
host binary, fail-soft subsystems, a headless self-test (`--smoke`) as the CI verdict.

## 4. Architecture

```
+------------------------------------------------------------------+
| d2linux (native ELF; i386 game code + host shims from M1)        |
|                                                                  |
|  game TUs (Redivivus decomp, #ifdef D2LINUX_GUARDED edits)       |
|      |  DirectDraw / DirectSound / Winsock / registry / SEH       |
|      v                                                           |
|  platform/linux shims (this track's code)                        |
|    - video:  INDEX8 surface + palette -> streaming texture       |
|              -> letterboxed logical present (M0-proven path)     |
|    - audio:  22.05 kHz S16-stereo device stream (M0: silence)    |
|    - M1+:    sockets, signals+backtrace, file-backed registry,   |
|              input, CRT odds and ends                            |
|      |                                                           |
|      v                                                           |
|  SDL3 (static, vendored release-3.4.18)                          |
|    Wayland + libdecor / X11 / ALSA backends dlopen'd at runtime  |
+------------------------------------------------------------------+
```

Key properties:

- **Self-contained binary.** Static SDL3 (`SDL_SHARED OFF` → `SDL3::SDL3-static`), same
  self-contained ethos as the PE line's `-static` link; backends are dlopen'd at runtime, so
  no -dev packages are needed to *run* it (GNOME Wayland needs `libdecor-0-0` +
  `libdecor-0-plugin-1-gtk` installed for decorations; `SDL_VIDEODRIVER=x11` is the escape hatch).
- **Fail-soft subsystems.** Audio absence is non-fatal (M0 ships silence); video failure is
  fatal with a diagnosed reason; `--smoke` forces the SDL `dummy` drivers so the self-test runs
  display-less anywhere (CI, WSL, containers).
- **The pattern producer is scaffolding.** `d2_linux_video.cpp`'s animated moiré exists to prove
  the present pipeline; M2 replaces it with the game's composed 8-bit frame. Nothing in the
  shim API assumes it stays.
- **Zero PE drift.** `platform/linux/build.cmake` routes only on `UNIX AND NOT APPLE` and
  `return()`s before the PE target graph; the Windows guard is untouched for Win32 builds.

## 5. Milestone ladder

| M | Scope | Status |
|---|---|---|
| **M0 — scaffold** | SDL3 host skeleton: window + renderer, INDEX8 staging surface + palette + streaming-texture present, letterboxed logical presentation, 22.05 kHz S16-stereo silence feeder, d2boot-style self-ID banner, headless `--smoke`; CI job `linux-native` | **FIELD-PROVEN** (owner VM `c8e8d54`; pattern + smoke + Wayland titlebar/resize) |
| **M1 — game logic compiles on Linux i386** | The shim layer (§6): Winsock→BSD sockets, SEH/VEH→signals (the crash twin over dladdr; the ELF libbacktrace config is its own later round), registry→file-backed (`~/.config/d2linux/registry.ini`), remaining CRT bits; the slice list grew to 44 TUs (Fog/Storm foundation + PKWARE — platform/linux/INVENTORY.md); CI switched to `-DD2LINUX_ARCH=i386` with multilib; PE gates re-run 380/380 on the shared-TU diff | **sandbox-green** (compiles + links i386, native smoke green, gates green); CI verifies the i386 smoke on this push |
| **M2 — title screen offline** | MPQ loading via the vendored Storm, one renderer path onto the M0 presentation surface (pattern producer retired), input shim; cinematics stubbed | |
| **M3 — playable offline** | Single player end to end; DirectDraw/D2Sound shims complete; palette-ramp lighting verified visually | |
| **M4 — codecs** | binkw32→ffmpeg-based shim (long pole), smackw32→libsmacker, ijl11→libjpeg | |
| **M5 — networking** | Full Winsock surface, Bnclient paths | |
| stretch | Native slang preset chain (the wrapper line's payoff, linux-side), packaging (AppImage/deb), 720p+ UI follow-ups | |

## 6. Work breakdown

### M1 — i386 game logic + shim layer *(next; the meat)*
- Inventory pass: enumerate the Win32 surface the first playable slice actually needs
  (sockets touched? registry keys read at boot? which SEH sites?) before writing shims —
  the decomp's `d2_platform.h` and the PE line's `support/` code are the map.
- Socket shim: Winsock → BSD sockets (handles are fds; `closesocket`→`close`,
  `WSAGetLastError`→`errno` mapping table).
- Exception shim: SEH/VEH → signal handlers + `libbacktrace` for the elf crash path
  (the PE's `d2crash-*.txt` reporter gets a linux twin).
- Registry shim: file-backed store under `~/.config/d2linux/` (or `-registry` switch);
  video-mode keys are the known first readers.
- CRT bits: whatever the game TUs actually reference beyond what glibc provides
  (`-m32` + glibc covers most; inventory first, no speculative shims).
- CMake: grow the source list incrementally (5–10 TUs per step, each step keeps
  `--smoke` and the PE gates green); CI flips to `i386` + `gcc-multilib g++-multilib`.
- Exit: the chosen game TUs compile and link i386, `d2linux --smoke` still green, PE
  gates still 240/240 on the shared-TU diff.

### M2 — title screen offline
- Storm/MPQ: vendored Storm opens the MPQs from the game dir; file-API shim routes the
  game's opens through it; missing-MPQ is a clean diagnosed error, not a crash.
- Renderer: the game's composed 8-bit frame lands in the M0 INDEX8 surface — palette-ramp
  and all; the pattern producer is deleted.
- Input: keyboard + mouse through SDL events into the game's input path; window close = quit.
- Cinematics: stubbed to skip.
- Exit: title screen renders, main menu navigable, character select visible.

### M3 — playable offline
- Complete the DirectDraw surface semantics the game really uses (locks, blits, palette
  ramps) and the D2Sound device-stream path (mixer fed from game, not silence).
- Exit: a single-player session — menu → new character → Act 1 outdoors/indoors → combat →
  save/exit — visually correct against the PE build on the same save, ~60 fps.

### M4 — codecs
- binkw32 → ffmpeg-based shim (long pole — schedule it as its own round), smackw32 →
  libsmacker, ijl11 → libjpeg.
- Exit: intro and in-game cinematics play.

### M5 — networking
- Full Winsock surface; Bnclient paths compile and link (open play / TCP as the target;
  Battle.net stays out of scope).
- Exit: LAN/TCP open game joins between the linux build and the PE build.

## 7. Risk register

| # | Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|---|
| R1 | 64-bit build corrupts game state (pointer-bearing struct layouts) | certain if tried | High | The i386 charter — `D2LINUX_ARCH=i386` default, `-m32` probe FATALs with the multilib hint; `native` exists only for the M0 skeleton/CI |
| R2 | MSVC-isms run deeper than inventoried (SEH scope, `__declspec`, inline asm) | Med | Med | Inventory-first rule (§6 M1); shims grow only at named call sites; each step keeps CI green |
| R3 | SDL3 backend drift (hard vs soft configure deps) | — | — | Retired as a live risk: all three defect rounds root-caused in-source; the apt set + hard/soft table live in `docs/linux-port.md` and build.yml comments; libdecor field-proven |
| R4 | Linux-only edit regresses the PE line | Low | High | `#ifdef` discipline (PE-identical when the guard is off), PE files untouchable (rule 2), referee = manual 240/240 gate suite + owner's Win11 build; CI on this branch carries no PE job by design |
| R5 | Multilib toolchain absent on a builder | Low | Med | CI installs `gcc-multilib g++-multilib` at M1; `D2LINUX_ARCH=native` remains the no-multilib escape for skeleton work |
| R6 | Audio fidelity (device-stream vs DirectSound mixing semantics) | Med | Med | Fail-soft until M3; M3's exit criteria include a listening pass on real hardware |
| R7 | CI runtime creep (temp clone + submodule init per run) | Low | Low | Shallow clone + `cancel-in-progress` concurrency already in build.yml; the smoke verdict runs in minutes today |
| R8 | git mode drift from Windows-side commits (exec bits silently lost) | certain (observed) | Low | Cosmetic by construction: CI calls gates through `bash`; mode audits by `git ls-tree` only (§0); no Windows-side re-chmod rituals |

## 8. Test plan

- **Sandbox (agent):** configure + build against the vendored SDL via the console path
  (`SDL_UNIX_CONSOLE_BUILD=ON` — no X11 dev libs in the container), `d2linux --smoke`
  headless, diff review + anchor greps, overlay-zip round-trips with `git ls-tree` mode
  checks. No GitHub push; the owner applies overlays.
- **CI (`linux-native`, ubuntu-24.04):** the automated gate — full X11/Wayland dev set,
  real configure (both backends detected), build, `--smoke` verdict grep. This is the only
  CI job on the branch; PR/paths-ignore rules unchanged from the house workflow.
- **Owner's Ubuntu VM (authoritative field pass):** real Wayland/GNOME session — banner,
  smoke, pattern, decorations, resize, ESC quit. Runtime needs: none beyond
  `libdecor-0-0` + `libdecor-0-plugin-1-gtk` on GNOME (static SDL3 dlopens the rest);
  `SDL_VIDEODRIVER=x11` as the escape hatch. Windows owner note: the skeleton runs under
  WSL2/WSLg; `--smoke` runs anywhere.
- **PE gates (shared-source referee):** `tools/check-vanilla-plus.sh` manual suite (240/240)
  re-run on every round that touches a shared TU; the owner's local Win11 llvm-mingw build
  stays the PE compile gate.

## 9. References

- `docs/linux-port.md` — track charter: mission split, i386 rationale, SDL3 rationale, the
  milestone ladder, build/runtime requirements, track rules (read this first).
- `handoff.md` — Rev journal (Rev 71 = scaffold, Rev 72 = M0 field-proven + CI shape change,
  Rev 73 = this plan), live task list §9. `HISTORY.md` / `VANILLA-PLUS.md` — the PE + wrapper
  record (closed lines).
- `.github/workflows/build.yml` — the `linux-native` job (apt set with per-package rationale
  comments, bash-prefixed gate call, concurrency rule).
- `tools/check-build-linux.sh` — clean-clone CI mirror (temp clone → submodule init →
  configure → build → smoke); `tools/check-vanilla-plus.sh` — the PE gate suite.
- `platform/linux/` — `build.cmake` (routing + arch probe + SDL3 resolution), `d2_linux.h`,
  `d2_linux_main.cpp` (banner, `--smoke`), `d2_linux_video.cpp` (present pipeline),
  `d2_linux_audio.cpp` (silence feeder).
- SDL3: vendored submodule `external/SDL` @ `829a65d` (release-3.4.18); its
  `cmake/sdlchecks.cmake` is the ground truth for hard-vs-soft configure deps;
  `README-linux.md` is the build-dep checklist.
- House precedents: chocolate-doom-vk / DevilutionX-libslang-vk (host-binary pattern);
  the Glide3xVK wrapper (self-contained static-link ethos, fail-soft subsystems, self-test
  as CI verdict).
