<img width="1920" height="1080" alt="image" src="https://github.com/user-attachments/assets/bd850c32-2a98-477f-bda9-a22e41d18bbd" />
<img width="1920" height="1080" alt="image" src="https://github.com/user-attachments/assets/999da238-d2f1-4c83-9a01-910daeea65a2" />
<img width="1920" height="1080" alt="image" src="https://github.com/user-attachments/assets/fb46e254-b1ce-44b7-a59c-0736af6ce05d" />

# D2VP Vanilla+QoL

<a href="https://madeopenly.com/?t=325ab9ffa5494159843c9f9999b41dc3" title="Made Openly: Mostly AI">
  <img src="https://madeopenly.com/b/325ab9ffa5494159843c9f9999b41dc3.svg?l=mostly-ai&s=dark&z=small&v=robot&m=robot"
       width="88" height="31" alt="Made Openly: Mostly AI">
</a>

[![Release](https://img.shields.io/badge/release-0.0.8--R128-8f2b2b)](https://github.com/VTSTech/D2VP-QoL/releases/latest)
[![Gates](https://img.shields.io/badge/vanilla%2B%20gates-519%2F519-2ea043)](tools/check-vanilla-plus.sh)
[![Fork of](https://img.shields.io/badge/fork%20of-jaenster%2Fredivivus-2b5797)](https://github.com/jaenster/redivivus)
[![Discord](https://img.shields.io/badge/Discord-join%20the%20chat-5865F2?logo=discord&logoColor=white)](https://discord.gg/xQXpH5Nj5)

Diablo II: Lord of Destruction 1.14d, recovered as C++ that builds and runs — with
**Vanilla+** on top: a fix-and-QoL layer maintained by VTSTech directly in the source.
Crash fixes, widescreen UI support, opt-in quality-of-life features and a settings page —
nothing silently changes gameplay. The client ships as **`d2vp.exe`**; rendering through
Vulkan comes from the separate [Glide3xVK](https://github.com/VTSTech/Glide3xVK) project
(`glide3x.dll` with a libSlang shader chain).

The tree is organised by the module names the game itself uses (`D2Client`, `D2Common`,
`D2Game`, `D2Win`, `D2gfx`, `Fog`, ...). Names, types and structure are reconstructions:
some functions still read like a decompiler wrote them, because one did.
[CONTRIBUTING.md](CONTRIBUTING.md) lists which paths are generated and which are
hand-written.

## Scope

Single-player, made to study, preserve and play the game. Online play on Battle.net is
outside its scope: not designed, tested or supported for it. The repository holds no
game data — MPQs and CD keys come from your own 1.14d install.

## Features (opt-in, default OFF)

With no `d2vp.ini` next to `d2vp.exe`, the game behaves exactly like retail 1.14d
(missing ini/section/key = vanilla; keys hot-re-read every ~2 s; a missing ini is
auto-created with a documented all-defaults template). Everything below also toggles
in-game: **Esc → Options → "Vanilla+ Options"**.

| Feature | `d2vp.ini` key | What it does |
| --- | --- | --- |
| Show Items Toggle | `[features] alt_toggle` | Each Alt press flips ground-item labels on/off (D2R parity); latched labels survive clicks, and clicking a label or sprite picks the item up. |
| Portable saves | `[features] portable_save` | Saves read/write `<game folder>\save` instead of the registry-resolved path; registry untouched. |
| Auto Gold Pickup | `[features] auto_gold` | Gold piles in pickup range are collected when you walk over them, via the game's own server-side pickup path. Gold only; never moves your character. |
| Persistent Globe Text | `[features] globe_persistent` | Retail's above-orb Life/Mana readout stays visible without hovering. Pure display. |
| Item Level Tooltips | `[features] item_ilvl` | Appends ` (N)` to the item name on every item tooltip. |
| Shared Stash | `[features] shared_stash` | An outlined "Shared" tab over **ten** retail-shaped stash pages every character on the PC shares (items + one gold pool survive the character), with `‹`/`›` pager arrows; stored in its own `_d2vp_shared.sss` — character `.d2s` files stay retail-compatible. |
| No Panel World Shift | `[features] no_panel_shift` | With one panel open at the wide layouts the world stays exactly where it was — the panel floats over an unmoved camera instead of the vanilla quarter-screen slide. |
| Mana Burn Fix | `[features] mana_burn_fix` | Repairs retail's Mana Burn bug (256x the intended mana drain, 512x in Hell) to the designed 66–100% of monster-level damage, matching D2R post-2.4. |
| Shift Item Compare | `[features] shift_compare` | Hold **Shift** over an equippable item and a second tooltip draws beside it: the item you have equipped for that body slot (D2R hover-compare parity) — inventory, stash, cube, trade, vendor; shield/quiver hovers compare the off-hand, weapons the primary. |

No balance changes, no save-format changes: `.d2s`/`.d2x` layouts stay untouched, and
every feature ships OFF. The wider candidate slate is scoped in
[FEATURES.md](FEATURES.md); nothing lands without a config gate.

## Status

Current: **`Vanilla+0.0.8-R128`** — shipped as tag
**[`R128`](https://github.com/VTSTech/D2VP-QoL/releases/tag/R128)**, 2026-11-06 (line 1
of `d2boot.log` pins any build).

- **Shipped** (short form — per-release detail: [CHANGELOG.md](CHANGELOG.md)): the
  **Gheed gamble crash fix** (R128, [#32](https://github.com/VTSTech/D2VP-QoL/issues/32),
  field-verified), the **Shift Item Compare** feature with its shield-slot fix
  (R125/R126, field-verified), the **D3D fullscreen boot-crash fix** (R124,
  field-verified), the **R109–R122 wide-resolution arc** — the 1344x700 / 1366x768 /
  1920x1080 video-options stops with hosted-stretch fullscreen — and the **Mana Burn
  Fix** (R106–R108). Earlier tags on the
  [releases page](https://github.com/VTSTech/D2VP-QoL/releases).
- **Field-verified**: boot → intro → title → character select → in game; Direct3D and
  Glide (`-3dfx`) rendering; sound end to end; the six-stop resolution ladder; the full
  smoke belt passed on the R83 and R104 builds
  ([#24](https://github.com/VTSTech/D2VP-QoL/issues/24),
  [#28](https://github.com/VTSTech/D2VP-QoL/issues/28)); `d2vp.exe` is field-played
  daily.
- **Open issues (4)**: the 1080p D3D-windowed crash
  ([#31](https://github.com/VTSTech/D2VP-QoL/issues/31), filed 2026-11-06 — fullscreen
  is fine), the post-Countess crash ([#29](https://github.com/VTSTech/D2VP-QoL/issues/29),
  filed 2026-11-06), the Refilling Shrine XP buff
  ([#25](https://github.com/VTSTech/D2VP-QoL/issues/25)) and the intermittent
  attribute-point crash ([#18](https://github.com/VTSTech/D2VP-QoL/issues/18)) — triage
  pending. Recently closed: #32 (Gheed gamble crash, fixed R128), #26 (720p panel
  margins + `no_panel_shift`), #22 (Bloodraven turn-in), #8 (necro skeleton tint).
- A crash writes a symbolised report (`d2crash-<date>-<time>.txt`) next to the
  executable; a 12 s hangwatch dumps one if the main loop ever stops beating. See
  [Crash reports](#crash-reports).

## Install

1. Grab the latest release 7z from the
   [releases page](https://github.com/VTSTech/D2VP-QoL/releases) and extract it
   into your Diablo II: LoD 1.14d folder (next to `Game.exe`).
2. Run `d2vp.exe -w`. That's the Vanilla+ client; `glide3x.dll` sits beside it for the
   wrapper route (`-3dfx`), launcher/debug scripts included.
3. Optional: `d2vp.exe -nointro` skips the two intro videos and the auto-played title
   cinematic — boot goes straight to the title screen. In-game cutscenes are unaffected.

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

- [docs/building.md](docs/building.md): getting the game, the toolchain per platform.

The default build is ~300 MB **on purpose**: `-O0` with full DWARF so the in-process
fault handler can write a *symbolised* `d2crash-*.txt` at crash time; `strip` removes
only the DWARF. Linking is fully static. GitHub CI (mingw-i686) builds every release tag
and ships `d2s-<sha>.exe` (stripped, play/distribute) plus `d2-map-<sha>.map` (the
linker map, load-bearing for crash symbolisation). Source and CI keep the `d2.exe` /
`d2s-<sha>.exe` names; each release ships the client renamed **`d2vp.exe`**.

## Known issues

- **When an NPC has quest news, the scrolling text box does not appear.** The quest itself
  advances normally; only the dialog is missing.
- **It can still crash.** Rarely now, but it is not ruled out — and each one fixed has
  taken a class of others with it. A crash writes a report with a symbolised stack trace
  to `d2crash-<date>-<time>.txt` next to the executable; please attach it to an
  [issue](https://github.com/VTSTech/D2VP-QoL/issues) in this repository.

## Crash reports

To attribute a crash correctly, collect **both** artifacts from one repro:

| Artifact | Comes from | Tells us |
| --- | --- | --- |
| `d2crash-<date>-<time>.txt` | `d2vp.exe` (symbolised, written next to the exe) | the crashed module, function and offset in game-source terms |
| `glide3x.log` | the glide3xvk wrapper | what the wrapper was doing (frame, preset, chain) and whether the faulting thread was in wrapper code |

Rule of thumb: faulting frame in `glide3xvk`/`vk_backend`/slang → wrapper issue (the
[Glide3xVK repo](https://github.com/VTSTech/Glide3xVK)); faulting frame in
`D2*`/`Storm`/`Fog` with a d2crash report → issue in this repository. Attach both files
to an issue — a crash without both artifacts cannot be attributed.
[CONTRIBUTING.md](CONTRIBUTING.md) has the detail, including the routing for a root
cause that turns out to be a reconstruction bug.

## Project documents

**For humans:** [CHANGELOG.md](CHANGELOG.md) (per-release changes, short) ·
[VANILLA-PLUS.md](VANILLA-PLUS.md) (the charter: layering, principles, crash-triage
protocol) · [SMOKE-CHECKLIST.md](SMOKE-CHECKLIST.md) (the in-game test pass).
**For agents (verbose by design):** [FEATURES.md](FEATURES.md) (the feature slate + house
feature contract) · [handoff.md](handoff.md) (live work log) ·
[HISTORY.md](HISTORY.md) (the verbatim archive) · [plans/](plans/) ·
[docs/merge-upstream.md](docs/merge-upstream.md) · [docs/building.md](docs/building.md).

## Contributing

Changes are welcome as pull requests or patches against a release tag. Run
[`tools/check-vanilla-plus.sh`](tools/check-vanilla-plus.sh) before opening a PR — it is
the suite CI enforces, and green is the bar.

[CONTRIBUTING.md](CONTRIBUTING.md) routes every kind of finding: Vanilla+ improvements
belong in this repository; fixes to the recovered source itself belong upstream at
[Redivivus](https://github.com/jaenster/redivivus), and fixes landed there arrive here at
the next sync.

## Credits

- The source this fork builds on is [Redivivus](https://github.com/jaenster/redivivus)
  by jaenster and contributors. Much of the 1.14d reverse-engineering comes from
  [blizzhackers](https://blizzhackers.dev), dzik in particular;
  [D2MOO](https://github.com/ThePhrozenKeep/D2MOO) supplied many of the structure
  layouts the work started from.
- Vanilla+, the Glide3xVK wrapper and this fork: VTSTech.

## License

MIT, see [LICENSE](LICENSE). Code under `thirdparty/` keeps its own license.

Diablo and Blizzard Entertainment are trademarks of Blizzard Entertainment, Inc. This
project is not affiliated with or endorsed by Blizzard.
