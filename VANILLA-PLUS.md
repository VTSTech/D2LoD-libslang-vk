# Vanilla-Plus — VTSTech's custom Diablo II, built on Redivivus

This branch (repo default, `main`) is the **game-source line**: improvements over vanilla
Diablo II 1.14d made directly in the recovered source, on top of
[jaenster/redivivus](https://github.com/jaenster/redivivus) (`Game.exe`
recovered function-by-function as C++ that builds). It is kept deliberately
clean of the glide3x→Vulkan wrapper work — that lives in its own repository,
[Glide3xVK](https://github.com/VTSTech/Glide3xVK) (formerly the `Glide3xVK`
branch of this repo, when it was `D2LoD-libslang-vk`) and overlays on top of
whatever game exe is running (`d2.exe` upstream, `d2vp.exe` in this fork's releases). (Branch history: the
restructure on 2026-10-28 moved the former `vanilla-plus` branch to `main` and
renamed `libslang-vk` → `Glide3xVK`; the repository itself took its current
name `D2VP-QoL` on 2026-11-03 — old GitHub URLs redirect regardless.)

**Latest release: [`R124`](https://github.com/VTSTech/D2VP-QoL/releases/tag/R124)
("Vanilla+0.0.7-R124")** — one 7z with the client (boot banner self-IDs
`Vanilla+0.0.7-R124`: the **D3D fullscreen boot-crash fix**, field-verified
2026-11-06 — the build also carries the **eighth upstream sync**, Ghidra DB →
1948, and the R109–R122 wide-resolution arc: 1344x700 / 1366x768 / 1920x1080
slider stops with hosted-stretch fullscreen, plus the wide-view rendering
fixes), the Glide3xVK **0.4.8** wrapper pairing and launcher/debug scripts.
All releases: the
[releases page](https://github.com/VTSTech/D2VP-QoL/releases); per-release
change lists: [CHANGELOG.md](CHANGELOG.md).

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

## Status (2026-11-06)

- **Up to date with upstream.** Eight numbered syncs of `jaenster/redivivus:main`
  (first `R87`, latest the eighth: upstream `7df6ad0e..b8e3087f`, Ghidra DB
  → **1948**, 16 commits / 786 files, merged as `18a368ce` — 693 unreferenced
  functions defined, module-local symbols refiled into their using modules,
  **Duriel's death no longer crashes**) — vanilla-plus is
  ahead of upstream and never behind. At DB 1948 the upstream byte-match stands
  at 7,648 of 14,626 functions (52.3%) / 23.99% of bytes (`asm_matching/msvc8`).
- **The closed defect tracks stay closed** (HISTORY H1/H26): the glide crash
  series (fixes 1–13), sound (15–16; upstreamed as jaenster/redivivus PR #10 —
  drops work again and the Act 5 snow hang is gone, both upstream fixes) and
  the fullscreen-D3D recovery (17–27) are all field-verified. The 720p-era D3D perspective-toggle
  crash was root-fixed at `R98` (the flat-wall builder's `uint8_t` subtile
  iterator wraps after 12 subtiles — latent since the original decompilation).
- **The full smoke belt has passed twice on the 0.0.7 line**: on the R83 build
  ([#24](https://github.com/VTSTech/D2VP-QoL/issues/24), 2026-11-02) and again
  on R104 ([#28](https://github.com/VTSTech/D2VP-QoL/issues/28), 2026-11-04 —
  "R104 fully smoke tested. passed.", clearing the standing §L asks: the
  unit-lock re-checks, perspective-off, both-panels, the D3D toggle re-check).
  Boot → intro → title → character select → in game, Direct3D and Glide
  (incl. the native 1280x720 widescreen mode), sound end to end and the
  `-nointro` boot switch (R86) are all field-green; the build is field-played daily.
- **The video-options slider is six stops deep** — 640x480 / 800x600 / 1280x720
  retail, plus **1344x700** (R109 — retail's dormant mode 3, first exposure),
  **1366x768** (R112) and **1920x1080** (R115): probe-first fullscreen entry
  (the driver's mode list is consulted before any mode-set), hosted stops
  stretch to fill the host with the cursor mapped back into the logical frame,
  windowed `-w` clean at every stop (the R116 GDI-pitch shear fix), and the
  wide-view rendering campaign closed the black-edge/void family at ≥720p
  (R117–R122: edge-void band fill, the room walk extended to hop 4, `vpdiag`
  instrumentation in `d2boot.log`, the retail cull-rect re-fit) — the campaign
  closed on vpdiag evidence (fill firing at full stretch, zero silent drops).
  Glide pairing: wrapper **0.4.8** for the 1080p stop (0.4.7 adds 1366x768,
  0.4.6 knows 1344x700). The eighth-sync build re-verified in the field after
  the R124 D3D fullscreen fix (2026-11-06: "d3d fullscreen works again").
- **The Vanilla+ surface is eight features deep** — all opt-in, default OFF,
  all manageable in-game from the Vanilla+ Options page (`Esc → Options`;
  the page shipped with 0.0.6 on 2026-10-30 and grew to eight rows — Shared
  Stash R67, No Panel World Shift R101, Mana Burn Fix R108; see Features
  below): alt-key item-label toggle, portable saves, auto gold pickup,
  persistent globe text, item-level tooltips, the ten-page shared stash
  (field-proven R74/R82), No Panel World Shift (field-proven R95–R101) and
  the Mana Burn fix (R106, options row R108).
- **Releases**: `0.0.1` → `0.0.7` shipped 2026-10-27 → 2026-11-02; the 0.0.7
  line now advances by R-tag — `R67` (first 0.0.7) … **`R124`** (current
  latest; self-ID
  `Vanilla+0.0.7-R124`, pinned by line 1 of `d2boot.log`). GitHub CI
  (mingw-i686) builds every release tag and ships `d2s-<sha>.exe` (stripped) +
  `d2-map-<sha>.map` (linker map, load-bearing for crash symbolization).
- **Housekeeping**: the repo is renamed `D2LoD-libslang-vk` → **`D2VP-QoL`**
  (2026-11-03) and the wrapper has completed its spin-out to the
  [Glide3xVK](https://github.com/VTSTech/Glide3xVK) repo (latest wrapper tag
  0.4.8). The gate suite ([`tools/check-vanilla-plus.sh`](tools/check-vanilla-plus.sh))
  pins the whole Vanilla+ surface at **499/499**. `d2light-diag` / `d2drop-diag`
  are retired (R103 — lights and drops field-stable for many releases);
  `ShadowDiag` and the `[d2boot]` marks stay in-tree.
- **`d2boot.log` contract (since `f10b5ff`)**: boot milestones + `D2BOOT_Fail` /
  upload-halt crash triage + fix22 warn-once guards only — zero writes during
  normal play; the file appends across sessions so the pre-crash trail survives.
  A 12 s hangwatch dumps a symbolized report if the main loop ever stops beating.
- **Open work** (handoff.md §9): one open issue on GitHub — the Refilling
  Shrine XP buff ([#25](https://github.com/VTSTech/D2VP-QoL/issues/25),
  suspected wrong-shrine-effect application, triage pending). Recently closed:
  #26 (720p panel margins + `no_panel_shift` — cleared by the R104 full smoke
  pass), #18 (intermittent attribute-point crash), #22 (A1Q2 Bloodraven
  turn-in, fixed R85), #8 (necro minion skeleton tint, fixed R51/R50,
  field-verified R91). The `roster heal:` watch stays armed.

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

## Features (opt-in, config-gated)

Every feature ships OFF. With no `d2vp.ini` next to the exe the game is retail
1.14d byte-for-byte: missing file/section/key/garbage value = vanilla at every
touched site, keys are hot-re-read at most every 2 s, and a missing `d2vp.ini`
is auto-created with a documented all-defaults template (one attempt per
process, never overwrites, silently skipped if the directory is unwritable).
All eight also toggle in-game — **Esc → Options → "Vanilla+ Options"** (eight
rows since R108; the rows flip the same ini keys through one bridge, the panel
shows a flip immediately, server-side gates pick it up on their next 2 s poll).
The wider candidate slate (D2R backports + owner suggestions) is scoped in
[FEATURES.md](FEATURES.md); nothing lands without a config gate.

- **Alt-key item-label toggle** (D2R "Show Items: Toggle" parity) — each full
  Alt press flips ground-item labels on/off instead of hold-to-show. Latched
  labels survive clicks, walking and attacking; with labels latched on, hover
  and clicks use the normal selection machinery — attacks, interactions and
  movement work exactly as with labels off, and clicking an item's label or
  sprite picks the item up (vanilla hold-Alt keeps its retail click-through
  quirks untouched); overhead player names follow
  the toggle; entering a game starts with labels off. **Default OFF**: set
  `[features] alt_toggle=1` in `d2vp.ini`.
- **Portable saves** — single-player save files (`.d2s`/`.d2x`/`.key`/`.map`)
  are read and written from `<game folder>\save` (the folder `d2vp.ini` lives
  in, resolved once from the exe location) instead of the registry-resolved
  save path — retail reads a registry save-path value first, so a stale value
  from an old install pins saves to e.g. `F:\Diablo II\Save` forever; without
  one, retail falls back to the system Saved Games location. The save folder
  is created on demand; in portable mode the registry is never written and
  existing saves are NOT auto-copied — move your files over once to keep
  characters. All save-path consumers (character list, saves, realm lock,
  automap) share one choke point (`GetAndCreateSavePath`), so the single hook
  covers everything. **Default OFF**: set `[features] portable_save=1`.
- **Auto Gold Pickup** (D2R "Gold Pickup" gameplay-option parity) — with the gate
  on, gold piles inside pickup range are collected when you walk over them, via
  the game's own server-side pickup path. Never moves your character (the trigger
  fires only when BOTH of the server's instant-pick conditions hold — pickup
  distance and collision — so retail's walk-to-item behavior is structurally
  unreachable), never fires mid-attack or mid-cast (combat guard: neutral / town /
  walk / run only), and is gold only — ground items untouched, a full inventory
  leaves the pile where it is, gold never sits on the cursor. **Default OFF**:
  set `[features] auto_gold=1`.
- **Persistent Globe Text** — retail prints the Life/Mana values above the orbs
  when you hover them; with the gate on, that text is pinned on, no hover needed.
  Pure display: nothing is written, no input changes, no state changes, and
  retail's own "Show HP/MP Text" video-option toggle keeps working unchanged.
  **Default OFF**: set `[features] globe_persistent=1`.
- **Item Level Tooltips** (D2R 2.5 display-item-level parity) — with the gate
  on, every item tooltip's name line reads `Long Sword (12)`: the ilvl is
  appended to the item name in all three tooltip builders (identified,
  unidentified and set items), rendered in the name's own color. Bare digits —
  players know what `(N)` means (the label word shipped first and was dropped
  per owner preference, R55→R59). **Default OFF**: set `[features] item_ilvl=1`.
- **Shared Stash** — an outlined "Shared" tab on the stash panel over **ten**
  retail-shaped stash pages that every character on the PC shares: items and
  gold deposited there survive the character. `‹` / `›` arrows beside the label
  step one page at a time (Shared 1 … Shared 10); clicking the word returns to
  the personal page; one gold pool across all ten. Stored in its own
  `_d2vp_shared.sss` beside the saves (v2 header, ten pages — older shared
  files load with the missing pages empty); the save-flush design keeps
  character `.d2s` files retail-pure, with or without the key. Single-player
  v1. **Default OFF**: set `[features] shared_stash=1` (the Options row also
  busts the client stash-gate cache, so the tab follows the toggle the same
  frame). Plan: [SHARED-STASH-PLAN.md](SHARED-STASH-PLAN.md).
- **No Panel World Shift** — with one panel open at the wide layouts
  (1280x720 / 1344x700 / 1366x768) the world — terrain, units, effects — stays exactly
  where it was: the panel floats over an unmoved camera instead of the vanilla
  quarter-screen slide (units stay locked to the world via the R100
  border-neutralizer pass across all 14 unit-anchored consumers). The vanilla
  slide is kept at 640x480 / 800x600, where it still re-centers the action.
  Field-proven end to end (R95–R100, issue #26). **Default OFF**: set
  `[features] no_panel_shift=1` (Options row since R101).
- **Mana Burn fix** (D2R 2.4 parity) — repairs a bug that ships in retail
  1.14d: monsters with the Mana Burn affix drain 256x the intended mana with
  melee hits (512x in Hell), emptying a mana pool in one blow. With the gate
  on, a Mana Burn melee hit drains the designed amount instead — 66-100% of
  the monster's level damage (33-50% for Nightmare/Hell minions); Nightmare
  and Hell still drain more than Normal, exactly like D2R after its 2.4 fix
  (the difficulty scaling is retail behavior and is left alone). The fix is
  server-side, so it applies in single player and in TCP-IP games hosted
  from this exe. **Default OFF**: set `[features] mana_burn_fix=1` — or flip
  the **Mana Burn Fix** row on the Vanilla+ Options page (R108; same key,
  the panel shows the flip immediately and the server gate picks it up on
  its next 2 s poll).

## Build

```sh
cmake -S . -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-i686.cmake
cmake --build build          # -> build/d2.exe
```

Copy `d2.exe` next to the original `Game.exe` in a 1.14d install and run
`d2.exe -w` (releases ship this exe renamed to `d2vp.exe` — same binary).
`d2vp.exe -nointro` skips the two intro videos and the auto-played title
cinematics — boot goes straight to the title screen; in-game cutscenes are
unaffected. Works with the [Glide3xVK](https://github.com/VTSTech/Glide3xVK)
overlay: drop that overlay's `glide3x.dll` in the same folder as usual
(`-3dfx` for the wrapper route).

On Windows the same build runs natively — `tools/build-local-win11.ps1` sets
up MSYS2's i686 toolchain (first run: winget + pacman, one-time), then
configures, builds, runs the CI gates (32-bit PE, map) and strips a
`d2s-<id>.exe`:

```powershell
powershell -ExecutionPolicy Bypass -File tools\build-local-win11.ps1 -Clean -DeployTo "C:\Games\Diablo II"
```

More per platform: [docs/building.md](docs/building.md).

## Crash triage protocol

The glide-crash class that motivated the fix chain (fixes 1–13) is closed, but the
protocol stands for any new crash — a decompiler-reconstructed rebuild will keep
yielding single-wrong-type crashes upstream hasn't hit yet (see issues #18/#25 for
the live examples). To attribute a crash correctly, collect **both** reports from
one repro:

| Artifact | Comes from | Tells us |
|---|---|---|
| `d2crash-<date>-<time>.txt` | this d2vp.exe (upstream's symbolized report format, written next to the exe; a 12 s main-loop stall trips the hangwatch, which dumps one too) | the crashed module, function and offset **in game source terms** |
| `glide3x.log` | the glide3xvk wrapper | what the wrapper was doing (frame, preset, chain, controller state) and whether the faulting thread was in wrapper code |

Keep `d2exe.map` (the CI linker map renamed) beside the exe so reports
symbolize against the shipped build.

Decision rule:

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
   (17–27) is closed, and the 720p render family (issue #26, R92–R98) is
   root-fixed and field-proven; the full smoke belt has passed twice on this
   base (R83 #24, R104 #28). The crash protocol above handles whatever fires next.
2. **Compatibility pass** — **done.** The wrapper's whole feature set (window
   presets, shader chain, controller, base_res) is verified against the rebuilt
   exe by daily field play plus the two full smoke belts; deltas that surfaced
   (the 720p panel-margin family) landed as issue #26 and closed green.
3. **Vanilla-plus improvements** — **in flight**: eight features shipped (see
   Features above). Next slate candidates, scoped and ranked low-risk-first in
   [FEATURES.md](FEATURES.md): Z-key camera zoom, shift equipment-compare,
   clock/timer overlay, transparent HUD, monster HP bar, automap scale,
   quick-cast keys. Candidates are discussed per issue; nothing lands without
   a config gate.
4. **Keep current with upstream** — **standing cadence.** Eight numbered syncs
   so far, each per [docs/merge-upstream.md](docs/merge-upstream.md) with the
   full audit battery (gate suite, marker-placement and dead-store audits,
   refile-aliasing audit, merge-site syntax sweep); the fork is ahead of
   upstream and never behind.

## Build notes

- CI-first policy: the tree builds on GitHub CI (real `i686-w64-mingw32-g++`,
  `tools/check-build.sh`) — do not burn local cycles on full rebuilds. Every
  release tag ships artifacts: `d2s-<sha>.exe` (stripped, play/distribute) +
  `d2-map-<sha>.map` (linker map, load-bearing for crash symbolization).
- Every source change updates the `tools/check-vanilla-plus.sh` gate table
  (499 checks at R124); the build self-ID string (first `d2boot.log` line)
  bumps every code round so any shipped exe pins to its source round.
- Transfer: the agent sandbox has no push credentials — regular rounds travel
  as **overlay zips of changed files**, upstream merges as **git bundles**;
  user side: `git fetch <file>.bundle main && git merge --ff-only FETCH_HEAD`,
  then push. Overlay zips remain the fallback for file-level application to a
  non-git tree.
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
