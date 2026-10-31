# D2LoD Vanilla+

[![Release](https://img.shields.io/badge/release-D2LoD--Vanilla--Plus--0.0.6-8f2b2b)](https://github.com/VTSTech/D2LoD-libslang-vk/releases/tag/D2LoD-Vanilla-Plus-0.0.6)
[![Gates](https://img.shields.io/badge/vanilla%2B%20gates-201%2F201-2ea043)](tools/check-vanilla-plus.sh)
[![Fork of](https://img.shields.io/badge/fork%20of-jaenster%2Fredivivus-2b5797)](https://github.com/jaenster/redivivus)

Diablo II: Lord of Destruction 1.14d, reverse-engineered into C++ that builds and runs —
with **Vanilla+** on top: a fix-and-QoL layer maintained by VTSTech directly in the
recovered source, plus a `glide3x.dll` that renders the game through Vulkan with a
libSlang shader chain.

The recovered game source comes from
[Redivivus](https://github.com/jaenster/redivivus) (`Game.exe` recovered
function-by-function; it does for the program host, renderers, sound, input, files and
network what [D2MOO](https://github.com/ThePhrozenKeep/D2MOO) does for the game
mechanics). This fork keeps that tree current with upstream and adds the Vanilla+
surface: crash fixes, widescreen UI support, opt-in quality-of-life features and a
settings page — nothing silently changes gameplay.

## Scope

Single-player, made to study, preserve and play the game. Online play on Battle.net is
outside its scope: not designed, tested or supported for it. The repository holds no
game data — MPQs and CD keys come from your own 1.14d install.

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

The wrapper never touches `Game.exe` and this branch never touches the wrapper; each can
be fixed, replaced and released independently. The releases bundle both as one drop-in
7z.

## Status

- **Current release: [`D2LoD-Vanilla-Plus-0.0.6`](https://github.com/VTSTech/D2LoD-libslang-vk/releases/tag/D2LoD-Vanilla-Plus-0.0.6)** — rebuilt `d2.exe` + Glide3xVK 0.4.6. The
  first `d2boot.log` line self-identifies the build (`Vanilla+0.0.6-R45`), so a shipped
  exe can always be pinned to its source round.
- Works, field-verified: boot → intro → title → character select → in game; Direct3D
  fullscreen and windowed at 640x480, 800x600 and the native **1280x720** widescreen
  mode; **Glide (`-3dfx`) through the glide3xvk wrapper** — fixed in the R46 round,
  root cause + fix documented in
  [#11](https://github.com/VTSTech/D2LoD-libslang-vk/issues/11); sound (intro video,
  title music, UI and in-game SFX); the Vanilla+ Options page; inventory, skill
  flyouts and dialogs aligned at 720p; **Auto Gold Pickup** (`[features] auto_gold`)
  — restored in the R47 round (a merge-port casualty: the walk-over trigger had
  been re-parented inside the help-menu UI block), root cause + fix documented in
  [#10](https://github.com/VTSTech/D2LoD-libslang-vk/issues/10).
- Not finished. One known issue remains:
  - **Unique monsters render in base colors, and unique-pack minion skeletons render
    gold.** One mechanism family (the palette-transform roll), under investigation.
- A crash writes a symbolised report (`d2crash-<date>-<time>.txt`) next to the
  executable; a 12 s hangwatch dumps one if the main loop ever stops beating. See
  [Crash reports](#crash-reports).

## Features (opt-in, default OFF)

With no `d2vp.ini` next to `d2.exe`, the game behaves like retail 1.14d byte-for-byte.
Missing ini/section/key/garbage value = vanilla at every touched site; keys are hot-re-read
at most every 2 s, and a missing `d2vp.ini` is auto-created with a documented all-defaults
template. Everything below can also be toggled in-game: **Esc → Options → "Vanilla+
Options"**.

| Feature | `d2vp.ini` key | What it does |
| --- | --- | --- |
| Show Items Toggle | `[features] alt_toggle` | Each Alt press flips ground-item labels on/off (D2R "Show Items: Toggle" parity). Latched labels survive clicks; clicking an item's label or sprite picks it up. |
| Portable saves | `[features] portable_save` | Saves read/write `<game folder>\save` instead of the registry-resolved path. Registry untouched; move your `.d2s`/`.d2x`/`.key`/`.map` files over once. |
| Auto Gold Pickup | `[features] auto_gold` | Gold piles inside pickup range are collected when you walk over them, via the game's own server-side pickup path. Gold only, never moves your character, never fires mid-attack. |
| Persistent Globe Text | `[features] globe_persistent` | Retail's above-orb Life/Mana readout stays visible without hovering. Pure display; retail's own HP/MP-text options keep working. |

No balance changes, no save-format changes: `.d2s`/`.d2x` layouts stay untouched, and
every feature ships OFF. The wider candidate slate (D2R backports and owner suggestions)
is scoped in [FEATURES.md](FEATURES.md); nothing lands without a config gate.

## Install

1. Grab the latest release 7z from the
   [releases page](https://github.com/VTSTech/D2LoD-libslang-vk/releases) and extract it
   into your Diablo II: LoD 1.14d folder (next to `Game.exe`).
2. Run `d2.exe -w`. That's the rebuilt client; `glide3x.dll` sits beside it for the
   wrapper route (`-3dfx`), launcher/debug scripts included.

Upgrading: extract over the previous install — saves, realm logins and `d2vp.ini` carry
over.

## Build

You need your own Diablo II: LoD 1.14 installation; this repository holds no game data.
With a mingw-w64 cross compiler for 32-bit Windows, CMake and Ninja:

```sh
cmake -S . -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-i686.cmake
cmake --build build          # -> build/d2.exe
```

On Windows the same build runs natively — `tools/build-local-win11.ps1` sets up MSYS2's
i686 toolchain (one-time), then configures, builds, runs the CI gates and strips a
`d2s-<id>.exe`:

```powershell
powershell -ExecutionPolicy Bypass -File tools\build-local-win11.ps1 -Clean -DeployTo "C:\Games\Diablo II"
```

GitHub CI (mingw-i686) builds every release tag and ships `d2s-<sha>.exe` (stripped,
play/distribute) plus `d2-map-<sha>.map` (linker map, load-bearing for crash
symbolisation). More per platform: [docs/building.md](docs/building.md).

The default `d2.exe` is ~300 MB **on purpose**: -O0 with full DWARF so the in-process
fault handler can write the *symbolised* `d2crash-*.txt` at crash time. `strip` removes
only the DWARF. Linking is fully static — no runtime DLLs needed beside it.

## Releases

| Tag | Highlights |
| --- | --- |
| [`0.0.1`](https://github.com/VTSTech/D2LoD-libslang-vk/releases/tag/D2LoD-Vanilla-Plus-0.0.1) | First public release. Glide-renderer defect series closed (fixes 1–13), drops fixed, sound fixed. |
| [`0.0.2`](https://github.com/VTSTech/D2LoD-libslang-vk/releases/tag/D2LoD-Vanilla-Plus-0.0.2) | First opt-in feature: the alt-key item-label toggle, with `d2vp.ini` auto-creation. |
| [`0.0.3`](https://github.com/VTSTech/D2LoD-libslang-vk/releases/tag/D2LoD-Vanilla-Plus-0.0.3) | Portable saves, alt-toggle click-through fix, quiet diagnostics. |
| [`0.0.4`](https://github.com/VTSTech/D2LoD-libslang-vk/releases/tag/D2LoD-Vanilla-Plus-0.0.4) | Auto Gold Pickup; globe hover values. |
| [`0.0.5`](https://github.com/VTSTech/D2LoD-libslang-vk/releases/tag/D2LoD-Vanilla-Plus-0.0.5) | Per-class player sounds restored (77 values byte-matched from the retail exe); full-inventory pickup crash and walk-click mis-pickup fixed; Persistent Globe Text. |
| [`0.0.6`](https://github.com/VTSTech/D2LoD-libslang-vk/releases/tag/D2LoD-Vanilla-Plus-0.0.6) | The 720p release: native 1280x720 mode; loading-screen freeze fixed; sound constants repaired; Options page visible; inventory, skill flyouts and dialogs aligned at 720p. |

Between tags, work is tracked as revision rounds (`-Rxx`); the shipped 0.0.6 build
self-IDs `Vanilla+0.0.6-R45`.

## Verification

Two checks stand behind every release:

- **The Vanilla+ gate suite** — [`tools/check-vanilla-plus.sh`](tools/check-vanilla-plus.sh)
  pins the whole Vanilla+ surface (feature gates, 720p anchors, sound constants, boot
  telemetry) with **201 checks**, green at the current tip.
- **[`asm_matching/`](asm_matching/README.md)** (upstream) compiles the tree with Visual
  C++ 2005 SP1 and byte-compares each function against the 1.14d binary — the check on
  the decompilation itself. Current counts live in its README.

## Crash reports

A decompiler-reconstructed rebuild will keep yielding single-wrong-type crashes upstream
hasn't hit yet. To attribute a crash correctly, collect **both** artifacts from one repro:

| Artifact | Comes from | Tells us |
| --- | --- | --- |
| `d2crash-<date>-<time>.txt` | `d2.exe` (symbolised, written next to the exe) | the crashed module, function and offset in game-source terms |
| `glide3x.log` | the glide3xvk wrapper | what the wrapper was doing (frame, preset, chain) and whether the faulting thread was in wrapper code |

Rule of thumb: faulting frame in `glide3xvk`/`vk_backend`/slang → wrapper issue (the
`Glide3xVK` branch); faulting frame in `D2*`/`Storm`/`Fog` with a d2crash report →
game-source issue, report upstream if it's a reconstruction bug. No d2crash file and a
silent death → suspect the boundary (exports, calling conventions, the driver swap).
Attach both files to an issue — a crash without both artifacts cannot be attributed.

## Project documents

- [VANILLA-PLUS.md](VANILLA-PLUS.md) — the charter: layering, principles, shipped features, crash-triage protocol.
- [FEATURES.md](FEATURES.md) — the D2R-backport feature slate, ranked, with the house feature contract.
- [handoff.md](handoff.md) — the live work log: current revision, field status, open defect tracks.
- [HISTORY.md](HISTORY.md) — the archive of closed fix series and investigations.
- [PLAN.md](PLAN.md) — the wrapper/d2.exe integration plan.
- [docs/merge-upstream.md](docs/merge-upstream.md) — how upstream regenerations are merged through with the Vanilla+ surface intact.

## Credits

- The recovered source is [Redivivus](https://github.com/jaenster/redivivus) by jaenster
  and contributors — this fork's `d2.exe` is built from it. Much of the 1.14d
  reverse-engineering comes from [blizzhackers](https://blizzhackers.dev), dzik in
  particular; the byte-match oracle in `asm_matching/` is Niehztog's;
  [D2MOO](https://github.com/ThePhrozenKeep/D2MOO) supplied many of the structure
  layouts the work started from.
- Vanilla+, the Glide3xVK wrapper and this fork: VTSTech.

Diablo and Blizzard Entertainment are trademarks of Blizzard Entertainment, Inc. This
project is not affiliated with or endorsed by Blizzard.
