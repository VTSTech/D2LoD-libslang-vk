<img width="1920" height="1080" alt="image" src="https://github.com/user-attachments/assets/dedbe5a2-7079-40d4-af2d-4eb948e975b2" />
<img width="1920" height="1080" alt="image" src="https://github.com/user-attachments/assets/73c54d57-8baf-44ae-a1ed-c73711978db3" />
<img width="1920" height="1080" alt="image" src="https://github.com/user-attachments/assets/815fae59-2e1a-4b76-a834-8ba2ea1893ba" />


# D2VP Vanilla+QoL

<a href="https://madeopenly.com/?t=325ab9ffa5494159843c9f9999b41dc3" title="Made Openly: Mostly AI">
  <img src="https://madeopenly.com/b/325ab9ffa5494159843c9f9999b41dc3.svg?l=mostly-ai&s=dark&z=small&v=robot&m=robot"
       width="88" height="31" alt="Made Openly: Mostly AI">
</a>

[![Release](https://img.shields.io/badge/release-0.0.7--R104-8f2b2b)](https://github.com/VTSTech/D2VP-QoL/releases/latest)
[![Gates](https://img.shields.io/badge/vanilla%2B%20gates-380%2F380-2ea043)](tools/check-vanilla-plus.sh)
[![Fork of](https://img.shields.io/badge/fork%20of-jaenster%2Fredivivus-2b5797)](https://github.com/jaenster/redivivus)
[![Discord](https://img.shields.io/badge/Discord-join%20the%20chat-5865F2?logo=discord&logoColor=white)](https://discord.gg/xQXpH5Nj5)

Diablo II: Lord of Destruction 1.14d, reverse-engineered into C++ that builds and runs —
with **Vanilla+** on top: a fix-and-QoL layer maintained by VTSTech directly in the
recovered source, plus a `glide3x.dll` that renders the game through Vulkan with a
libSlang shader chain. The wrapper is its own project and repository:
[Glide3xVK](https://github.com/VTSTech/Glide3xVK) (formerly a branch of this repo,
which was itself formerly `D2LoD-libslang-vk`).

The recovered game source comes from
[Redivivus](https://github.com/jaenster/redivivus) (`Game.exe` recovered
function-by-function; it does for the program host, renderers, sound, input, files and
network what [D2MOO](https://github.com/ThePhrozenKeep/D2MOO) does for the game
mechanics). This fork keeps that tree current with upstream and adds the Vanilla+
surface: crash fixes, widescreen UI support, opt-in quality-of-life features and a
settings page — nothing silently changes gameplay. Upstream builds `d2.exe`; this fork
modifies that source and ships the client as **`d2vp.exe`**.

## Scope

Single-player, made to study, preserve and play the game. Online play on Battle.net is
outside its scope: not designed, tested or supported for it. The repository holds no
game data — MPQs and CD keys come from your own 1.14d install.

## Features (opt-in, default OFF)

With no `d2vp.ini` next to `d2vp.exe`, the game behaves like retail 1.14d byte-for-byte.
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
| Item Level Tooltips | `[features] item_ilvl` | Appends ` (N)` to the item name on every item tooltip — e.g. `Long Sword (12)` — so an item's level reads at a glance. |
| Shared Stash | `[features] shared_stash` | An **outlined "Shared" tab** on the stash panel over **ten** retail-shaped pages that every character on the PC shares: items and gold deposited there survive the character. `‹` / `›` arrows beside the label step one page at a time (Shared 1 … Shared 10); clicking the word returns to the personal page; one gold pool across all ten. Stored in its own `_d2vp_shared.sss` beside the saves; older shared-stash files load with the missing pages empty — character `.d2s` files stay retail-compatible, with or without the key. |
| No Panel World Shift | `[features] no_panel_shift` | With one panel open at the wide layouts (1280x720 / 1344x700) the world — terrain, units, effects — stays exactly where it was: the panel floats over an unmoved camera instead of the vanilla quarter-screen slide. The vanilla slide is kept at 640x480 / 800x600, where it still re-centers the action. |

No balance changes, no save-format changes: `.d2s`/`.d2x` layouts stay untouched, and
every feature ships OFF. The wider candidate slate (D2R backports and owner suggestions)
is scoped in [FEATURES.md](FEATURES.md); nothing lands without a config gate.

## Status

The short form — per-area records: [`handoff.md`](handoff.md) §2 (field-status table);
per-release history: [`CHANGELOG.md`](CHANGELOG.md); round journals:
[`handoff.md`](handoff.md) (live) and [`HISTORY.md`](HISTORY.md) (verbatim archive).

- **Shipped: [`R98`](https://github.com/VTSTech/D2VP-QoL/releases/tag/R98)
  ("Vanilla+ 0.0.7-R98")** — issue #26 round 7: the D3D perspective-toggle crash
  rooted to the flat-wall builder's `uint8_t` subtile iterator (wraps after 12 subtiles
  and reads +4-shifted pseudo-records — latent since the original decompilation),
  iterator widened to `int`. Earlier 0.0.7-line tags:
  [`R91`](https://github.com/VTSTech/D2VP-QoL/releases/tag/R91) — docs +
  field-verify round: the third upstream sync field-run green and the necro-minion
  skeleton fix confirmed in the field ("Skeletons are finally white again");
  [`R88`](https://github.com/VTSTech/D2VP-QoL/releases/tag/R88) — the second
  upstream sync (Ghidra DB 1839 → 1851; gossip-table and SFX float supersessions);
  [`R87`](https://github.com/VTSTech/D2VP-QoL/releases/tag/R87) — the first
  upstream sync of the line (1806 → 1839, field-run green);
  [`R86`](https://github.com/VTSTech/D2VP-QoL/releases/tag/R86) — the
  `-nointro` boot switch ("R86 verified, -nointro works");
  [`R85`](https://github.com/VTSTech/D2VP-QoL/releases/tag/R85) — the A1Q2
  Bloodraven turn-in fix ([#22](https://github.com/VTSTech/D2VP-QoL/issues/22),
  closed by the owner 2026-11-02).
- **In the tree (R99–R104; the R104 build carried the full SMOKE belt green —
  [#28](https://github.com/VTSTech/D2VP-QoL/issues/28), 2026-11-04 — tags pending)**:
  the fourth upstream sync (R99 — the Ghidra 1864–1877 library rounds, 209 files) and
  the rounds that closed the loop it opened — R100 fixed the unit-sprite pan the new
  retail bodies re-armed under `no_panel_shift=1` (14 call sites routed through the
  border neutralizer; owner: "no_panel_shift is working as intended now"), R101 added
  the **No Panel World Shift** row to the Vanilla+ Options panel (the toggle is no
  longer ini-only), R102 took the fifth upstream sync (`94ec239d..cc23648d`, 459
  files, Ghidra DB → 1892; the byte-match table below is current at 6,493/14,642
  fns), R103 retired the `d2light-diag`/`d2drop-diag` probes and R104 swept the
  per-TU restructure fallout (R103/R104 zero owner-visible behavior change). Release
  notes live under the tagged
  [GitHub releases](https://github.com/VTSTech/D2VP-QoL/releases).
- **Field-verified**: boot → intro → title → character select → in game; Direct3D and
  Glide (`-3dfx`) rendering, incl. the native 1280x720 widescreen mode; sound end to
  end; the full SMOKE belt passed on the R83 build
  ([#24](https://github.com/VTSTech/D2VP-QoL/issues/24)) and again fully on the R104
  build ([#28](https://github.com/VTSTech/D2VP-QoL/issues/28), 2026-11-04 — incl. the
  D3D perspective-toggle re-check and the `no_panel_shift` §L matrix); the `-nointro`
  boot switch (R86) verified in-game 2026-11-02 — "R86 verified, -nointro works"; the
  `no_panel_shift` feature (R95 Glide pass: "All blackness appears fixed in Glide as
  well"; R100 unit lock: "working as intended now").
- **Open issues (4)**, triaged in [handoff.md](handoff.md) §9: Necro minion skeleton
  colors ([#8](https://github.com/VTSTech/D2VP-QoL/issues/8)) — fixed (R51),
  field-verified R91, owner close pending; intermittent
  attribute-point crash ([#18](https://github.com/VTSTech/D2VP-QoL/issues/18));
  Refilling Shrine XP buff ([#25](https://github.com/VTSTech/D2VP-QoL/issues/25));
  720p panel margins ([#26](https://github.com/VTSTech/D2VP-QoL/issues/26)) —
  the render fixes and `no_panel_shift` are field-proven (R92–R101), and the remaining
  asks (the D3D perspective-toggle re-check, the explicit both-panels pass, the R100
  unit-lock re-checks) rode the R104 full smoke belt green
  ([#28](https://github.com/VTSTech/D2VP-QoL/issues/28), 2026-11-04) — owner close
  pending. A1Q2 Bloodraven turn-in ([#22](https://github.com/VTSTech/D2VP-QoL/issues/22))
  CLOSED 2026-11-02 — the R85 fix field-proven.
- A crash writes a symbolised report (`d2crash-<date>-<time>.txt`) next to the
  executable; a 12 s hangwatch dumps one if the main loop ever stops beating. See
  [Crash reports](#crash-reports).

## Install

1. Grab the latest release 7z from the
   [releases page](https://github.com/VTSTech/D2VP-QoL/releases) and extract it
   into your Diablo II: LoD 1.14d folder (next to `Game.exe`).
2. Run `d2vp.exe -w`. That's the Vanilla+ client; `glide3x.dll` sits beside it for the
   wrapper route (`-3dfx`), launcher/debug scripts included.
3. Optional: `d2vp.exe -nointro` skips the two intro videos (the Blizzard / Blizzard
   North logo clips) and the auto-played title cinematics — boot goes straight to the
   title screen. In-game cutscenes are unaffected.

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

Naming: source and CI keep upstream's `d2.exe` / `d2s-<sha>.exe` names; each release
ships the client renamed **`d2vp.exe`** — the Vanilla+ build is distinct from upstream's
`d2.exe` at a glance.

The default `d2.exe` is ~300 MB **on purpose**: -O0 with full DWARF so the in-process
fault handler can write the *symbolised* `d2crash-*.txt` at crash time. `strip` removes
only the DWARF. Linking is fully static — no runtime DLLs needed beside it.

## Releases

Every shipped release with its change list: [`CHANGELOG.md`](CHANGELOG.md). Tag
downloads: the
[releases page](https://github.com/VTSTech/D2VP-QoL/releases). Round journals:
[`handoff.md`](handoff.md) (live) and [`HISTORY.md`](HISTORY.md) (verbatim archive).

## Verification

Two checks stand behind every release:

- **The Vanilla+ gate suite** — [`tools/check-vanilla-plus.sh`](tools/check-vanilla-plus.sh)
  pins the whole Vanilla+ surface (feature gates, 720p anchors, sound constants, boot
  telemetry) with **380 checks**, green at the current tip.
- **[`asm_matching/`](asm_matching/README.md)** (upstream) compiles the tree with Visual
  C++ 2005 SP1 and byte-compares each function against the 1.14d binary — a check on the
  decompilation itself: where the code comes out identical, the decompiled function was
  read correctly. Current counts live in its README and below.

At this commit, generated from version 1892 of the Ghidra database:

|Measure|Identical machine code|
|-|-|
|Functions|6,493 of 14,642 (44.3%)|
|Code|534,694 of 2,602,915 bytes (20.54%)|
|Instructions|1,780,082 of 2,602,915 bytes (68.4%), counting identical instructions in functions that still differ|

These leave out the C runtime that Visual C++ 2005 linked in statically (libcmt): the
rebuild links a runtime instead of reconstructing it. Code the compiler generates for C++
exceptions is left out as well. Over the whole binary, runtime
included: 7,050 of 15,711 functions (44.9%), 633,399 of
2,748,975 bytes (23.04%).

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="asm_matching/msvc8/evidence/byte-match-dark.svg">
  <img alt="The original's code split by how closely the rebuild matches it" src="asm_matching/msvc8/evidence/byte-match-light.svg">
</picture>

[BYTE-MATCHING.md](BYTE-MATCHING.md) explains what is compared, the kinds of match, and why
whole matching functions are a strict measure next to identical instructions.

## Crash reports

A decompiler-reconstructed rebuild will keep yielding single-wrong-type crashes upstream
hasn't hit yet. To attribute a crash correctly, collect **both** artifacts from one repro:

| Artifact | Comes from | Tells us |
| --- | --- | --- |
| `d2crash-<date>-<time>.txt` | `d2vp.exe` (symbolised, written next to the exe) | the crashed module, function and offset in game-source terms |
| `glide3x.log` | the glide3xvk wrapper | what the wrapper was doing (frame, preset, chain) and whether the faulting thread was in wrapper code |

Rule of thumb: faulting frame in `glide3xvk`/`vk_backend`/slang → wrapper issue (the
[Glide3xVK repo](https://github.com/VTSTech/Glide3xVK)); faulting frame in `D2*`/`Storm`/`Fog` with a d2crash report →
game-source issue, report upstream if it's a reconstruction bug. No d2crash file and a
silent death → suspect the boundary (exports, calling conventions, the driver swap).
Attach both files to an issue — a crash without both artifacts cannot be attributed.

[CONTRIBUTING.md](CONTRIBUTING.md) has the detail: which paths are generated and which are
hand-written, and which door a given finding goes through so that it survives.

With an AI agent you can read the Ghidra databases behind the tree (1.14d Windows and Mac)
through a read-only MCP server, and report such fixes with the evidence. In Claude Code:

```
claude plugin marketplace add jaenster/redivivus
claude plugin install redivivus@redivivus
```

The plugin brings the MCP server and the house rules; sign in with GitHub from `/mcp`. Other
agents: see https://redivivus.app/agent-setup/prompt.md. Corrections go in as proposals that are
checked against the databases, reviewed, and applied with you as the git author; see
[how proposals work](https://redivivus.app/about#contributing).

## Project documents

- [CHANGELOG.md](CHANGELOG.md) — the per-release change digest.
- [VANILLA-PLUS.md](VANILLA-PLUS.md) — the charter: layering, principles, shipped features, crash-triage protocol.
- [FEATURES.md](FEATURES.md) — the D2R-backport feature slate, ranked, with the house feature contract.
- [handoff.md](handoff.md) — the live work log: current revision, field status, open defect tracks.
- [HISTORY.md](HISTORY.md) — the archive of closed fix series and investigations.
- [PLAN.md](PLAN.md) — the wrapper/d2.exe integration plan (PLAN.md's "d2.exe" is the source line's own name; releases ship `d2vp.exe`).
- [docs/merge-upstream.md](docs/merge-upstream.md) — how upstream regenerations are merged through with the Vanilla+ surface intact.

## Credits

- The recovered source is [Redivivus](https://github.com/jaenster/redivivus) by jaenster
  and contributors — this fork's `d2vp.exe` is built from it. Much of the 1.14d
  reverse-engineering comes from [blizzhackers](https://blizzhackers.dev), dzik in
  particular; the byte-match oracle in `asm_matching/` is Niehztog's;
  [D2MOO](https://github.com/ThePhrozenKeep/D2MOO) supplied many of the structure
  layouts the work started from.
- Vanilla+, the Glide3xVK wrapper and this fork: VTSTech.

## License

MIT, see [LICENSE](LICENSE). Code under `thirdparty/` keeps its own license.

The `.cpp.map` files map every function and line back to its address in 1.14d `Game.exe`. They
are part of this work under the same license: if you use them, credit Redivivus and link here.

Diablo and Blizzard Entertainment are trademarks of Blizzard Entertainment, Inc. This
project is not affiliated with or endorsed by Blizzard.
