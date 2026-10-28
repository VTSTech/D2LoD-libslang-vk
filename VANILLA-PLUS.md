# Vanilla-Plus — VTSTech's custom Diablo II, built on Redivivus

This branch (repo default, `main`) is the **game-source line**: improvements over vanilla
Diablo II 1.14d made directly in the recovered source, on top of
[jaenster/redivivus](https://github.com/jaenster/redivivus) (`Game.exe`
recovered function-by-function as C++ that builds). It is kept deliberately
clean of the glide3x→Vulkan wrapper work — that lives on the `Glide3xVK`
branch of [VTSTech/D2LoD-libslang-vk](https://github.com/VTSTech/D2LoD-libslang-vk)
and overlays on top of whatever `d2.exe` is running. (Branch history: the
restructure on 2026-10-28 moved the former `vanilla-plus` branch to `main` and
renamed `libslang-vk` → `Glide3xVK`.)

**Latest release: [`D2LoD-Vanilla-Plus-0.0.3`](https://github.com/VTSTech/D2LoD-libslang-vk/releases/tag/D2LoD-Vanilla-Plus-0.0.3)**
(2026-10-28, from `569fc5c`) — one 7z with the client, the Glide3xVK 0.4.5 wrapper and
launcher/debug scripts.

## The two layers

```
+----------------------------------------------------------+
|  d2.exe   (this branch: redivivus source + vanilla-plus  |
|            fixes/improvements, built with mingw i686)    |
+----------------------------------------------------------+
|  glide3x.dll                                             |
|  (Glide3xVK branch: glide3xvk wrapper — Vulkan render,   |
|   slang shader chain, window presets, controller, ...)   |
+----------------------------------------------------------+
|  your own Diablo II: LoD 1.14d install (MPQs + keys)     |
+----------------------------------------------------------+
```

The wrapper never touches Game.exe and this branch never touches the wrapper;
each can be fixed, replaced and released independently.

## Status (2026-10-28)

- **Up to date with upstream.** `jaenster/redivivus:main` (`ad964f3`, the export-scale
  commit: Ancients/Diablo/Act II overlay crash fixes, Esc closes speech boxes, unnamed
  data tables restored, guard zones) is merged (`8030526`); vanilla-plus is ahead of
  upstream and never behind.
- **The glide defect series (fixes 1–13) is closed and field-verified** (HISTORY H1):
  launcher crash, title-screen black triangles, end-of-loading halt, black light cores
  and unit silhouettes, light-radius discoloration, shadow blobbing — all gone. Root
  causes and evidence: `handoff.md` §3–§4.
- **Sound is fixed** (fixes 15–16: title/in-game music and NPC/monster/player SFX;
  upstreamed as jaenster/redivivus PR #10). **Drops work again and the Act 5 snow hang
  is gone** (upstream).
- **The fullscreen-D3D recovery (fixes 17–27) is closed and field-verified** (HISTORY
  H9–H18): the Error 25 boot failure, the texture-pipeline boot halts, black ground,
  diagonal bars, and wrong UI text hues are all gone under any act palette.
- **First opt-in feature shipped**: the alt-key item-label toggle — press Alt once to
  flip ground-item labels (D2R "Show Items: Toggle" parity). While labels are
  latched on, clicking still attacks, moves and interacts normally, and clicking an
  item's label or sprite picks the item up. Gated behind
  `[features] alt_toggle` in `d2vp.ini`, which the client auto-creates with documented
  defaults; default off = vanilla byte-for-byte (see Features below).
- **Releases**: `D2LoD-Vanilla-Plus-0.0.1` (`4857c30`) → `D2LoD-Vanilla-Plus-0.0.2`
  (`9cf9f69`) → **`D2LoD-Vanilla-Plus-0.0.3` (`569fc5c`, 2026-10-28)**. CI (mingw-i686)
  is back in service and green on the release tags.
- **0.0.3 ships**: portable saves (`[features] portable_save`), the
  alt-toggle click-through fix (latched labels no longer eat clicks), and quiet
  diagnostics — the fix-26/27 `pal26*` probe family is retired (it made
  `d2boot.log` grow during play), so `d2boot.log` only gains boot marks and
  failure records, never gameplay output. Boot banner self-identifies
  `vanilla-plus 0.0.3` in the log header.
- Diagnostics (`ShadowDiag`, `LightDiag v2`, `DropDiag v2.1`, `[d2boot]` boot marks, D3D
  upload-halt dumps) stay in-tree, zero cost when idle, armed for the next regression.
  **`d2boot.log` contract (since `f10b5ff`)**: boot milestones + `D2BOOT_Fail` /
  upload-halt crash triage + fix22 warn-once guards only — zero writes during
  normal play; the file appends across sessions so the pre-crash trail survives.

## Principles

1. **Vanilla behavior is the default.** Improvements are opt-in, config-gated
   or bug-fixes; nothing silently changes gameplay or balance.
2. **Fixes first, features second.** The recovered source still crashes in
   places (reconstruction artifacts). Every crash fixed is the top priority —
   upstream's rule holds: each root cause usually takes a whole class of
   crashes with it.
3. **Fixes belong upstream where possible.** A crash that comes from a wrong
   type or lost value in the reconstruction should be reported upstream
   (issue + `d2crash-*.txt`) so it survives regeneration. Only
   vanilla-plus-specific behavior (new toggles, QoL) lives only here.
4. **No game data, no Blizzard binaries.** MPQs and CD keys come from the
   user's own 1.14d install. Single-player focus; Battle.net is out of scope
   until upstream recovers it.

## Features (opt-in, config-gated)

- **Alt-key item-label toggle** (D2R "Show Items: Toggle" parity) — each full
  Alt press flips ground-item labels on/off instead of hold-to-show. Latched
  labels survive clicks, walking and attacking; with labels latched on, hover
  and clicks use the normal selection machinery — attacks, interactions and
  movement work exactly as with labels off, and clicking an item's label or
  sprite picks the item up (vanilla hold-Alt keeps its retail click-through
  quirks untouched); overhead player names follow
  the toggle; entering a game starts with labels off. **Default OFF**: set
  `[features] alt_toggle=1` in `d2vp.ini` next to `d2.exe` (missing
  file/section/key/garbage value = vanilla hold-Alt behavior byte-for-byte;
  the key is hot-re-read at most every 2 s). A missing `d2vp.ini` is created
  at startup with a documented all-defaults template — one attempt per
  process, never overwrites an existing file, silently skipped if the
  directory is unwritable (delete it and a fresh copy returns next start).
- **Portable saves** — single-player save files (`.d2s`/`.d2x`/`.key`/`.map`)
  are read and written from `<game folder>\save` (the folder `d2vp.ini` lives
  in, resolved once from the exe location) instead of the registry-resolved
  save path — retail reads a registry save-path value first, so a stale value
  from an old install pins saves to e.g. `F:\Diablo II\Save` forever; without
  one, retail falls back to the system Saved Games location. **Default OFF**:
  set `[features] portable_save=1` in `d2vp.ini` (hot-re-read at most every
  2 s). The save folder is created on demand; in portable mode the registry is
  never written (no migration pass, retail values untouched) and existing
  saves are NOT auto-copied — move your `.d2s`/`.d2x`/`.key`/`.map` files over
  once to keep characters (the ini template says the same). Missing
  ini/section/key or a garbage value = vanilla resolution byte-for-byte. All
  save-path consumers (character list, saves, realm lock, automap) share one
  choke point (`GetAndCreateSavePath`), so the single hook covers everything.

## Build

```sh
cmake -S . -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-i686.cmake
cmake --build build          # -> build/d2.exe
```

Copy `d2.exe` next to the original `Game.exe` in a 1.14d install and run
`d2.exe -w`. Works with the `Glide3xVK` overlay: drop that overlay's
`glide3x.dll` in the same folder as usual.

On Windows the same build runs natively - `tools/win11-build.ps1` sets up
MSYS2's i686 toolchain (first run: winget + pacman, one-time), then
configures, builds, runs the CI gates (32-bit PE, map) and strips a
`d2s-<id>.exe`:

```powershell
powershell -ExecutionPolicy Bypass -File tools\win11-build.ps1 -Clean -DeployTo "C:\Games\Diablo II"
```

## Crash triage protocol

The glide-crash class that motivated the fix chain (fixes 1–13) is closed, but the
protocol stands for any new crash — a decompiler-reconstructed rebuild will keep
yielding single-wrong-type crashes upstream hasn't hit yet. To attribute a crash
correctly, collect **both** reports from one repro:

| Artifact | Comes from | Tells us |
|---|---|---|
| `d2crash-<date>-<time>.txt` | this d2.exe (upstream's symbolized report, written next to the exe) | the crashed module, function and offset **in game source terms** |
| `glide3x.log` | the glide3xvk wrapper | what the wrapper was doing (frame, preset, chain, controller state) and whether the faulting thread was in wrapper code |

Decision rule:

- faulting frame in `glide3xvk` / `vk_backend` / slang chain -> wrapper issue,
  fix on `Glide3xVK`;
- faulting frame in `D2*` / `Storm` / `Fog` source with a d2crash report ->
  game-source issue: minimize the repro, check upstream issues, then either
  report upstream (reconstruction bug) or fix here (vanilla-plus behavior);
- no `d2crash-*.txt` written and the process dies silently -> suspect the
  boundary: exports, calling conventions, or the glide driver swap itself.

Attach both files to a new issue on the fork; a crash without both artifacts
cannot be attributed.

## Roadmap

1. **Stable base** — **done.** The glide crash class is burned down (fixes 1–13), sound
   (15–16) and drops (14) are fixed, and the fullscreen-D3D recovery (17–27) is closed;
   two public releases shipped on that base (`0.0.1`, `0.0.2`). The gauntlet now runs
   crash-free on `d2.exe` + `glide3xvk` together; the crash protocol above handles
   whatever fires next.
2. **Compatibility pass** — confirm the wrapper's whole feature set (window
   presets, shader chain, controller, base_res) against the rebuilt exe and
   note any game-side differences vs original Game.exe. (0.0.2 bundles Glide3xVK 0.4.5
   and is field-played daily; remaining deltas land as issues.)
3. **Vanilla-plus improvements** — **started**: the alt-key item-label toggle
   (config-gated, default off) shipped in 0.0.2; the portable-save option
   (`[features] portable_save`) landed post-0.0.2. Candidates discussed per
   issue; nothing lands without a config gate.

## Build notes

- CI-first policy: the tree builds on GitHub CI (real `i686-w64-mingw32-g++`,
  `tools/check-build.sh`) — do not burn local cycles on full rebuilds. Every push
  ships artifacts: `d2s-<sha>.exe` (stripped, play/distribute) + `d2-map-<sha>.map`
  (linker map, load-bearing for crash symbolization). (CI was retired 2026-10-27 per
  HISTORY H2 and re-enabled 2026-10-28 — green on the 0.0.2 tag.)
- Transfer: the agent sandbox has no push credentials, so fixes travel as **git
  bundles** — user side: `git fetch <file>.bundle main && git merge
  --ff-only FETCH_HEAD`, then push. Overlay zips remain the fallback for
  file-level application to a non-git tree.
- Local recon (llvm-mingw / clang 23): CMake configure is clean with
  `cmake/mingw-i686.cmake` as-is. Compiling trips on two **clang-specific**
  quirks gcc does not have, so CI is unaffected: (1) `_EXCEPTION_DISPOSITION`
  typedef redefinitions — clang's mingw-w64 headers already declare the type,
  the decompiled `typedef int` versions conflict (8 sites incl. d2_platform.h
  pattern); (2) `threadlocinfo` unknown type in `d2_platform.h` (gcc exposes
  it via libstdc++ internals clang's libc++ names differently). Both only
  matter if someone wants a local clang build later.
- exe size: the default d2.exe is ~300 MB **on purpose**. Upstream builds
  -O0 with full DWARF (`-g`) so the in-process fault handler
  (support/d2_crash.cpp + vendored libbacktrace) can write the SYMBOLIZED
  `d2crash-*.txt` at crash time, and explicitly warns that optimizing
  decompiled code which may lean on undefined behaviour is "an experiment,
  not a release". Linking is fully static (`-static -static-libgcc
  -static-libstdc++`), so no mingw runtime DLLs are needed beside it. The
  original 1.14d Game.exe is ~6 MB; -O0 code bloat plus DWARF for tens of
  thousands of functions is the rest. Keep the fat exe as the crash-triage
  instrument; `strip` removes only the DWARF (the CI job uploads both),
  it does not shrink the -O0 code itself.
