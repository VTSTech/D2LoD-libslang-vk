# Vanilla-Plus — VTSTech's custom Diablo II, built on Redivivus

This branch (repo default, `main`) is the **game-source line**: improvements over vanilla
Diablo II 1.14d made directly in the recovered source, on top of
[jaenster/redivivus](https://github.com/jaenster/redivivus) (`Game.exe`
recovered function-by-function as C++ that builds). It is kept deliberately
clean of the glide3x→Vulkan wrapper work — that lives in its own repository,
[Glide3xVK](https://github.com/VTSTech/Glide3xVK) — and overlays on top of
whatever game exe is running (`d2.exe` upstream, `d2vp.exe` in this fork's releases).
(Branch history: the 2026-10-28 restructure moved the former `vanilla-plus` branch to
`main`; the repository took its current name `D2VP-QoL` on 2026-11-03 — old GitHub URLs
redirect regardless.)

**Current release: `Vanilla+0.0.8-R127`** — one 7z with the client (the Glide3xVK
**0.4.8** wrapper pairing and launcher/debug scripts). The feature list lives in
[README.md](README.md); per-release changes in [CHANGELOG.md](CHANGELOG.md); the live
work log in [handoff.md](handoff.md). All releases: the
[releases page](https://github.com/VTSTech/D2VP-QoL/releases).

## The two layers

```
+----------------------------------------------------------+
|  d2vp.exe (this branch: redivivus source + vanilla-plus  |
|            fixes/improvements, built with mingw i686)    |
+----------------------------------------------------------+
|  glide3x.dll                                             |
|  (Glide3xVK repo: glide3xvk wrapper — Vulkan render,     |
|   slang shader chain, window presets, controller, ...)   |
+----------------------------------------------------------+
|  your own Diablo II: LoD 1.14d install (MPQs + keys)     |
+----------------------------------------------------------+
```

The wrapper never touches Game.exe and this branch never touches the wrapper;
each can be fixed, replaced and released independently.

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
   user's own 1.14d install. Single-player focus; online play on Battle.net is
   out of scope — not designed, tested or supported for it.

## The config contract

Every feature ships OFF. With no `d2vp.ini` next to the exe the game is retail
1.14d byte-for-byte: missing file/section/key/garbage value = vanilla at every
touched site, keys are hot-re-read at most every 2 s, and a missing `d2vp.ini`
is auto-created with a documented all-defaults template (one attempt per
process, never overwrites, silently skipped if the directory is unwritable).
All features also toggle in-game — **Esc → Options → "Vanilla+ Options"** (the
rows flip the same ini keys through one bridge; the panel shows a flip
immediately, server-side gates pick it up on their next 2 s poll). The feature
table: [README.md](README.md). The candidate slate + the feature contract:
[FEATURES.md](FEATURES.md).

## Crash triage protocol

The glide-crash class that motivated the original fix chain (fixes 1–13) is closed, but
the protocol stands for any new crash — a decompiler-reconstructed rebuild will keep
yielding single-wrong-type crashes upstream hasn't hit yet (issues #18/#25 are the live
examples). To attribute a crash correctly, collect **both** reports from
one repro:

| Artifact | Comes from | Tells us |
|---|---|---|
| `d2crash-<date>-<time>.txt` | this d2vp.exe (symbolized report, written next to the exe; a 12 s main-loop stall trips the hangwatch, which dumps one too) | the crashed module, function and offset **in game source terms** |
| `glide3x.log` | the glide3xvk wrapper | what the wrapper was doing (frame, preset, chain, controller state) and whether the faulting thread was in wrapper code |

Keep `d2exe.map` (the CI linker map renamed) beside the exe so reports
symbolize against the shipped build. Decision rule:

- faulting frame in `glide3xvk` / `vk_backend` / slang chain -> wrapper issue,
  fix in the [Glide3xVK repo](https://github.com/VTSTech/Glide3xVK);
- faulting frame in `D2*` / `Storm` / `Fog` source with a d2crash report ->
  game-source issue: minimize the repro, check upstream issues, then either
  report upstream (reconstruction bug) or fix here (vanilla-plus behavior);
- no `d2crash-*.txt` written and the process dies silently -> suspect the
  boundary: exports, calling conventions, or the glide driver swap itself.

Attach both files to a new issue on the fork; a crash without both artifacts
cannot be attributed.

## Roadmap

1. **Stable base** — **done.** The glide crash class is burned down (fixes
   1–13), sound (15–16) and drops (14) are fixed, the fullscreen-D3D recovery
   (17–27) is closed, and the 720p render family (issue #26) is root-fixed and
   field-proven; the full smoke belt has passed twice on this base (R83 #24,
   R104 #28). The crash protocol above handles whatever fires next.
2. **Compatibility pass** — **done.** The wrapper's whole feature set (window
   presets, shader chain, controller, base_res) is verified against the rebuilt
   exe by daily field play plus the two full smoke belts.
3. **Vanilla-plus improvements** — **in flight**: nine features shipped (see
   README). Next slate candidates, scoped and ranked low-risk-first in
   [FEATURES.md](FEATURES.md). Candidates are discussed per issue; nothing lands
   without a config gate.
4. **Keep current with upstream** — **standing cadence.** Eight numbered syncs
   so far (Ghidra DB → 1948), each per
   [docs/merge-upstream.md](docs/merge-upstream.md) with the full audit
   battery (gate suite, marker-placement and dead-store audits, refile-aliasing
   audit, merge-site syntax sweep); the fork is ahead of upstream and never behind.

## Build

```sh
cmake -S . -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-i686.cmake
cmake --build build          # -> build/d2.exe
```

Copy `d2.exe` next to the original `Game.exe` in a 1.14d install and run
`d2.exe -w` (releases ship this exe renamed to `d2vp.exe` — same binary).
Works with the [Glide3xVK](https://github.com/VTSTech/Glide3xVK) overlay: drop
that overlay's `glide3x.dll` in the same folder (`-3dfx` for the wrapper
route). On Windows: `tools/build-local-win11.ps1` (MSYS2 i686 toolchain,
configure → build → gates → strip). More per platform:
[docs/building.md](docs/building.md).

## Build notes

- CI-first policy: the tree builds on GitHub CI (real `i686-w64-mingw32-g++`,
  `tools/check-build.sh`) — do not burn local cycles on full rebuilds. Every
  release tag ships artifacts: `d2s-<sha>.exe` (stripped, play/distribute) +
  `d2-map-<sha>.map` (linker map, load-bearing for crash symbolization).
- Every source change updates the `tools/check-vanilla-plus.sh` gate table
  (510 checks at R127); the build self-ID string (first `d2boot.log` line)
  bumps every code round so any shipped exe pins to its source round.
- Transfer: the agent sandbox has no push credentials — regular rounds travel
  as **overlay zips of changed files**, upstream merges as **git bundles**;
  user side: `git fetch <file>.bundle main && git merge --ff-only FETCH_HEAD`,
  then push. Overlay zips remain the fallback for file-level application to a
  non-git tree.
- Local recon (llvm-mingw / clang 23): CMake configure is clean with
  `cmake/mingw-i686.cmake` as-is. Compiling trips on two **clang-specific**
  quirks gcc does not have, so CI is unaffected: (1) `_EXCEPTION_DISPOSITION`
  typedef redefinitions — clang's mingw-w64 headers already declare the type;
  (2) `threadlocinfo` unknown type in `d2_platform.h`. Both only matter for a
  local clang build.
- exe size: the default d2.exe is ~300 MB **on purpose**. Upstream builds
  -O0 with full DWARF (`-g`) so the in-process fault handler
  (support/d2_crash.cpp + vendored libbacktrace) can write the SYMBOLIZED
  `d2crash-*.txt` at crash time. Linking is fully static, so no mingw runtime
  DLLs are needed beside it. Keep the fat exe as the crash-triage instrument;
  `strip` removes only the DWARF (the CI job uploads both).
