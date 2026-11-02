# Changelog — D2LoD Vanilla+ (`d2vp.exe`)

All notable changes to the Vanilla+ client and the Glide3xVK wrapper, per shipped
release. Working rounds between releases carry build self-IDs `Vanilla+0.0.x-Ryy` — the
first `d2boot.log` line pins any shipped exe to its source round. The engineering-level
round journal lives in [`handoff.md`](handoff.md) (current) and
[`HISTORY.md`](HISTORY.md) (verbatim archive, H1–H36); per-round summaries for the
0.0.6-era releases are also on the
[GitHub releases page](https://github.com/VTSTech/D2LoD-libslang-vk/releases).

Started at handoff rev 110 (2026-11-02); the 0.0.1–0.0.6 entries are distilled from the
GitHub releases page and the H-records (the README release table this superseded was
retired in README trim #1, rev 111 — verbatim in [`HISTORY.md`](HISTORY.md) H32).

## [Unreleased]

### Fixed

- A1Q2 "Sisters' Burial Grounds" turn-in: Blood Raven killed but the Kashya
  reward (rogue mercenary) could not be claimed
  ([#22](https://github.com/VTSTech/D2LoD-libslang-vk/issues/22)) — the same
  truncated-gossip-table regen defect as
  [#13](https://github.com/VTSTech/D2LoD-libslang-vk/issues/13): the A1Q2
  gossip table was emitted singular (accept line only) while the handler reads
  slot 3 whenever RewardAwaits is set, so the turn-in line never existed and
  the reward gate was never asked. Table widened to 5 slots (slot 3 = turn-in
  line 0x5c, pinned by the quest's own gate; flavor slots left empty rather
  than invented), Kashya chain marks added. Gates 311 → 320; self-ID
  `Vanilla+0.0.7-R85`; **field-proven — the owner closed the issue 2026-11-02**
  (state completed, same day). Track record: HISTORY.md H35–H36.

## [R84] — 2026-11-02 ([`R84`](https://github.com/VTSTech/D2LoD-libslang-vk/releases/tag/R84) @ `63e020a3`, self-ID `Vanilla+0.0.7-R84`)

The item-tooltip zero fix ([#16](https://github.com/VTSTech/D2LoD-libslang-vk/issues/16)),
built and field-verified by the owner the same day; SMOKE §J items 37–38 field-proven
at tag.

### Fixed

- Item tooltips printed "+0 cold damage" / "0 poison damage over 0 seconds"
  whenever a roll's range collapsed to a single value
  ([#16](https://github.com/VTSTech/D2LoD-libslang-vk/issues/16)) — the single-value
  elemental/poison branches of `SKILLDESC_BuildStatDescription` called
  `UNICODE_FormatWideString` with no varargs (a latent retail-1.14d bug the fork's build
  layout makes visible). Tooltip-only: item stats and generation untouched. Gates
  308 → 311; SMOKE §J items 37–38 field-proven on the owner's R84 verify pass.

## [0.0.7] — 2026-11-01/02

The first 0.0.7 build is the R66 content state (tag `R67`, self-ID `Vanilla+0.0.7-R66`,
release notes covering R59→R66); the R67→R77 block shipped as tag
[`R77`](https://github.com/VTSTech/D2LoD-libslang-vk/releases/tag/R77)
("Vanilla+ 0.0.7 (R77)", 2026-11-01); further 0.0.7-line tags `R78`–`R84` mark the
upstream-sync and fix rounds. Gate suite 204 (0.0.6) → 283 (first 0.0.7 build) →
311 (R84).

### Added

- **Shared Stash** (SP): a "Shared" tab on the stash panel over retail-shaped pages every
  character on the PC shares — items and gold deposit/withdraw, one gold pool, own
  `_d2vp_shared.sss` pool file; character `.d2s` files stay retail-compatible. Round 1
  (one page) coded R63–R66 and first shipped in 0.0.7; field-proven end to end
  (mechanism blind-tested R65, visible tab R66 — "Great job. I see it. It works.", 720p
  check R74). The R67–R71 tab-polish round was rolled back by owner decision (`59ff645`)
  and rebuilt on the R66 tab as R75: outlined label + a **second page** + `‹`/`›` pager
  arrows (`.sss` v2; v1 files read as page 1); R82 grew it to **ten pages** — the v2
  format widened in place (same `VPS2` magic + version, header 28 → 92 bytes; pre-R82
  files load with the missing pages empty; one gold pool across all ten).
- **Item Level Tooltips** (`[features] item_ilvl`, R55–R59, field-proven: "Great job.
  iLvl spacing appears fixed."): appends ` (N)` to the item name on every item tooltip.
  Coded on the 0.0.6 line; first shipped in a 0.0.7 build.
- **Vanilla+ Options page**: every feature also toggles in-game (Esc → Options →
  "Vanilla+ Options"); the Shared Stash row added R67 (same `D2VP_*` bridge).
- The smoke checklist ships as an interactive GitHub issue template
  (`.github/ISSUE_TEMPLATE/smoke-test.md`, generated from `SMOKE-CHECKLIST.md` by
  `tools/gen-smoke-issue-template.py`) — one Smoke Test issue per build run from R83 on.

### Fixed

- Waypoint panel row icons rendered ~120 px above their buttons at 720p
  ([#21](https://github.com/VTSTech/D2LoD-libslang-vk/issues/21), R81) — the two
  `waygateicons` draws were the only site-6 panel misses left in the wide-Y retrofit.
- Video-options 1280x720 label floated a label-width past its column and ~4 px above the
  neighbor rows ([#23](https://github.com/VTSTech/D2LoD-libslang-vk/issues/23), R83) —
  now right-aligned to the value column on the neighbors' ink line.
- Alt-toggle stale selection: standing on a latched label and walk-clicking far away
  picked the item up
  ([#15](https://github.com/VTSTech/D2LoD-libslang-vk/issues/15), R58) — root-caught v2
  (the Rev-23 fix was dead code). Coded on the 0.0.6 line; first shipped in a 0.0.7
  build.
- Stash tab geometry: the pager's `‹` tip rode the retail gold-amount text (R76 nudge,
  R77 lift into the panel's top-right band).

### Changed

- **Upstream syncs** to Ghidra 1792 (merge `6c552f9` + the R73/R74 compile-fix rounds —
  the merge-site syntax sweep `scripts/merge_site_syntax_sweep.sh` now guards the merge
  surface), 1794 (`ea0b84d4`), 1801 (`f518fda0`), 1806 (`f427f962`); byte-match 6,029 of
  14,642 functions (41.2%) at the 1806 merge. Every merge field-run green
  ("Builds. Runs."), and the full SMOKE belt passed on the R83 build
  ([#24](https://github.com/VTSTech/D2LoD-libslang-vk/issues/24), 2026-11-02 — the first
  full-belt field pass on a 0.0.7 build).
- `tools/syntax-check.sh` host syntax net added (R71) so a text gate can never again be
  the last line of defense; README restructured and the shipped client renamed
  `d2vp.exe` across the docs (R74/R96).

## [0.0.6] — 2026-10-30 ([`D2LoD-Vanilla-Plus-0.0.6`](https://github.com/VTSTech/D2LoD-libslang-vk/releases/tag/D2LoD-Vanilla-Plus-0.0.6) @ `0eb2655`, self-ID `Vanilla+0.0.6-R45`)

The 720p release: native 1280x720 widescreen mode; loading-screen freeze fixed; sound
constants repaired; the Vanilla+ Options page visible; inventory, skill flyouts and
dialogs aligned at 720p. Post-release, the owner kept shipping 0.0.6-line repair rounds
as tags `R47`–`R62`; all of it folded into the first 0.0.7 build.

### Fixed (post-release 0.0.6-line rounds)

- Glide `-3dfx` wrapper route crashed on the first frame (R46, field-verified
  "ok Glide i working now",
  [#11](https://github.com/VTSTech/D2LoD-libslang-vk/issues/11)).
- Auto Gold Pickup restored (R47,
  [#10](https://github.com/VTSTech/D2LoD-libslang-vk/issues/10)) — gold piles inside
  pickup range collected via the game's own server-side pickup path.
- 720p stash deposit/withdraw gold dialog misaligned (R48,
  [#12](https://github.com/VTSTech/D2LoD-libslang-vk/issues/12)) — the VP riders had
  resolved into the allocation-failure branch (a new wrong-branch merge-casualty class;
  `scripts/rev48_wrong_branch_audit.py` added).
- Unique monsters rendered base colors instead of their unique palettes (R49,
  field-verified, [#9](https://github.com/VTSTech/D2LoD-libslang-vk/issues/9)).
- Necro minion skeletons rendered gold — two inverted-polarity palette writes (roster
  pet-recolor R50 + spawn-time fallback R51,
  [#8](https://github.com/VTSTech/D2LoD-libslang-vk/issues/8); field verify still
  pending — the one open defect).
- 720p belt hover popup collapsed on the next mouse event (R52,
  [#14](https://github.com/VTSTech/D2LoD-libslang-vk/issues/14)).
- A1Q1 Den of Evil never completed: the reward gossip entry was missing from the tree
  data (R54, [#13](https://github.com/VTSTech/D2LoD-libslang-vk/issues/13)).
- Alt-toggle stale selection (R58,
  [#15](https://github.com/VTSTech/D2LoD-libslang-vk/issues/15)) — see 0.0.7.
- Save-and-re-enter crash (R60 probes + R61 fix,
  [#19](https://github.com/VTSTech/D2LoD-libslang-vk/issues/19)) — the VP Resolution row
  mutated the static esc-menu table; loader guards degrade to marks + draw-site NULL
  guard. Field-verified ("R61 Confirmed fixes #19").

### Synced

- Upstream to Ghidra 1783 (R62, merges `3d645e7` + `19f9791` + `d6d4b53` + two
  compile-fix rounds) — field-run green and the full SMOKE checklist passed on the same
  build (owner, 2026-11-01).

## [0.0.5] — 2026-10-29 ([`D2LoD-Vanilla-Plus-0.0.5`](https://github.com/VTSTech/D2LoD-libslang-vk/releases/tag/D2LoD-Vanilla-Plus-0.0.5))

Per-class player sounds restored (77 values byte-matched from the retail exe);
full-inventory pickup crash and walk-click mis-pickup fixed; **Persistent Globe Text**
(the 0.0.4 globe-hover readout reworked to retail's own always-on above-orb display).

## [0.0.4] — 2026-10-29 ([`D2LoD-Vanilla-Plus-0.0.4`](https://github.com/VTSTech/D2LoD-libslang-vk/releases/tag/D2LoD-Vanilla-Plus-0.0.4))

Auto Gold Pickup; globe hover values.

## [0.0.3] — 2026-10-28 ([`D2LoD-Vanilla-Plus-0.0.3`](https://github.com/VTSTech/D2LoD-libslang-vk/releases/tag/D2LoD-Vanilla-Plus-0.0.3))

Portable saves (`[features] portable_save` — saves read/write `<game folder>\save`,
registry untouched); alt-toggle click-through fix; diagnostics quieted (probe
retirement).

## [0.0.2] — 2026-10-28 ([`D2LoD-Vanilla-Plus-0.0.2`](https://github.com/VTSTech/D2LoD-libslang-vk/releases/tag/D2LoD-Vanilla-Plus-0.0.2))

First opt-in feature: the alt-key item-label toggle (`[features] alt_toggle`), with
self-provisioning `d2vp.ini`.

## [0.0.1] — 2026-10-27 ([`D2LoD-Vanilla-Plus-0.0.1`](https://github.com/VTSTech/D2LoD-libslang-vk/releases/tag/D2LoD-Vanilla-Plus-0.0.1))

First public release. Glide-renderer defect series closed (fixes 1–13), item drops
fixed, sound fixed.

## Wrapper tags

`0.3.1`, `0.4.1`–`0.4.6` (the Glide3xVK branch) — `glide3x.dll` wrapper versions; the
current release 7z ships wrapper 0.4.6.
