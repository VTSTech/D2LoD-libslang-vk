# Release notes — R84 → R88

One section per shipped tag, chronological. Each section is the paste text for its
GitHub release (tag link + commit + self-ID match what shipped). Dates are the
owner's local dates. The engineering-level records live in [`CHANGELOG.md`](../CHANGELOG.md)
(per-release digest), [`handoff.md`](../handoff.md) (rev 118, live) and
[`HISTORY.md`](../HISTORY.md) H39 (the R87/R88 sync arcs).

---

## R84 — Vanilla+ 0.0.7-R84 — 2026-11-02 — tag [`R84`](https://github.com/VTSTech/D2LoD-libslang-vk/releases/tag/R84) @ `63e020a3`

**The item-tooltip zero fix ([#16](https://github.com/VTSTech/D2LoD-libslang-vk/issues/16)).**

Items could drop with a mod line reading **"0 poison damage over 0 seconds"**, and
'of Frost' gear could show **"+0 Cold Damage"**: the item-stat tooltip formatter's
single-value elemental/poison branches called the wide-string formatter **without the
numeric varargs** (a latent retail-1.14d bug the fork's build layout makes visible), so
any roll whose range collapsed to min==max printed stack residue instead of the value
('of Frost' weapons roll `cold-max` 1..2 with `cold-min` fixed at 1 — every drop that
rolled a 1 collapsed; poison collapses through the length rounding). Display-side only —
item stats and generation were never wrong.

- **Field-verified** by the owner the same day (SMOKE §J items 37–38 proven at tag).
- Gates 308 → 311; self-ID `Vanilla+0.0.7-R84` (d2boot.log line 1 pins the exe).

## R85 — Vanilla+ 0.0.7-R85 — 2026-11-02 — tag [`R85`](https://github.com/VTSTech/D2LoD-libslang-vk/releases/tag/R85) @ `71f4772f`

**The A1Q2 "Sisters' Burial Grounds" turn-in fix ([#22](https://github.com/VTSTech/D2LoD-libslang-vk/issues/22)).**

Blood Raven killed, but Kashya's reward (the rogue mercenary) could not be claimed:
the A1Q2 gossip table was emitted singular (accept line only) while the handler reads
slot 3 whenever RewardAwaits is set — the same truncated-gossip-table regen defect as
[#13](https://github.com/VTSTech/D2LoD-libslang-vk/issues/13). The turn-in line never
existed, so the reward gate was never asked. Table widened to 5 slots (slot 3 = turn-in
line `0x5c`, pinned by the quest's own gate; flavor slots left empty rather than
invented) + 7 Kashya chain marks for one-log diagnosis.

- **Field-proven** — the owner closed the issue 2026-11-02, the same day.
- Gates 311 → 320; self-ID `Vanilla+0.0.7-R85`.
- R88 note: upstream later recovered the retail-exact **6-slot** table (with the flavor
  lines the fork had left empty); the hand-build was retired at R88, notes kept.

## R86 — Vanilla+ 0.0.7-R86 — 2026-11-02 — tag [`R86`](https://github.com/VTSTech/D2LoD-libslang-vk/releases/tag/R86) @ `3ca2da40`

**The `-nointro` boot switch.**

`d2vp.exe -nointro` skips the boot intro videos — the Blizzard / Blizzard North logo
.biks that play on every launch plus the auto-played title/first-run cinematics — by
never entering cutscene mode: both boot/title entries take the retail begin-failed path
straight to the next screen. Writes a `[vp] cmdline -nointro` mark to `d2boot.log`.
In-game quest cutscenes are unaffected; boot without the switch is byte-for-byte
vanilla.

- **Field-verified** 2026-11-02 — "R86 verified, -nointro works" (SMOKE §K items 39–40).
- Gates 320 → 326; self-ID `Vanilla+0.0.7-R86`.

## R87 — Vanilla+ 0.0.7-R87 — 2026-11-02 — tag [`R87`](https://github.com/VTSTech/D2LoD-libslang-vk/releases/tag/R87) @ `486b936c`

**The first upstream sync of the 0.0.7 line** — merge `b2937091`: upstream 18 commits,
Ghidra DB 1806 → 1839, byte match 6,029 → 6,232 of 14,642 functions (18.40% → 19.50% of
bytes). Vanilla+ intact on top.

What came in:

- Ghidra 1807–1813 regen rounds (packet/roster/monster/subtile parameters,
  SkillDesc/OOG types, zero-init/signed compares) and generator rounds (loop gate +
  signed division, folded-store arm order, range tests as `lo <= x && x <= hi`).
- The VC8 harness + CI deps (upstream PR #32); dtchitt issue #37–#76 names; names for
  3,500 variables/fields + Storm PowerPC names + the new D2Hell file.
- Real behavior fixes: roster party ids `uint16_t` throughout (joining a game no longer
  hangs in the party-grouping loop); three value-bug fixes (wide char literal, byte-mask
  select, Win32 calls shadowed by thunks) with VC8 near-miss type fixes.
- Merge-side latent fix: `DRAW_ShouldDrawEdges` had returned 0 unconditionally since the
  0.0.6 merge port — wide modes never drew level edges; caught and restored during
  resolution.

- **Field-run green** — "Finally, builds, runs, no regressions." Gates 326/326.

## R88 — Vanilla+ 0.0.7-R88 — merge `1d401785`; tag after field verify

**The second upstream sync** — upstream 9 commits (`c8445e73..bc47c6bc`), Ghidra DB
1839 → 1851, byte match 6,232 → 6,242 of 14,642 functions (19.50% → 19.54% of bytes);
22 conflicts / 27 hunks resolved, Vanilla+ intact on top.

Synced:

- Types/prototypes from Commit-Will's, Niehztog's and Nugwa628's proposals; the third
  naming round with Mac cross-check types (`CLIENTMODE_WaitForQServerConnection`,
  `MAINMENU_OnPressSkipEmailAddress`, `Objects_InitFnNN_<role>`, `StaticInit_gpsz*`).
- `QUESTLOG_HitTestCloseButton` returns bool; affix rollers take `bSpawnableOnly`, not
  `bIsSuffix`; `fnGetDatabaseFileTime` is `__fastcall(FILETIME *)`;
  `D2Sha1ContextStrc` is 0x60 bytes.

**The supersession ledger opens** — upstream recovered the retail forms of two fork
fixes; the fork takes them ("upstream wins") and keeps the root-cause notes in code:

- **The R54/R85 gossip hand-tables** ([#13](https://github.com/VTSTech/D2LoD-libslang-vk/issues/13)
  / [#22](https://github.com/VTSTech/D2LoD-libslang-vk/issues/22)): upstream's
  `bc47c6bc` restored the retail tables verbatim — A1Q1 8 / A1Q2 6 / A2Q1 9 slots —
  plus the flavor string ids the fork had left empty rather than invent. All 39
  `aNpcGossipData` externs are now sized retail-exact (28 were unsized pre-merge): the
  ~17-quest singular-table watch (track J) resolves wholesale.
- **vanilla-plus fix 16** (Defect 13: SFX silent, music OK): upstream's
  `D2FloatIntUnion` is the same root cause in the retail-faithful form (`.fValue`
  store, integer `.nValue` read-back).
- **Sound constants**: upstream's exact decoded decimals
  (`9.999999747378752e-05` = the float32 `0.0001` promotion) replace the fork's
  spellings; the `-2000.0` centibel diagnosis stays (upstream ships the same scale).

Checked and **not** superseded: the issue #11 `[glide fix 11]` hook — still fork-side
(re-planted at the R87 merge); the glide3x.dll wrapper ships from the `Glide3xVK`
branch, which upstream does not carry.

- Gates: 10 stale expectations updated to the superseding state with R87-merge notes
  (epsilon ×4, the `-2000.0` cast, gossip sizes ×6, Bloodraven `0x5c` ×2 — retail
  carries bMenu 0 and bMenu 2 variants); self-ID row pinned `Vanilla+0.0.7-R88`.
  Suite **326/326**.
- Status: in the tree, **field verify pending** — owner build + SMOKE, then the tag.

---

### Update mechanics (owner, Windows side)

    git fetch <bundle> main && git merge --ff-only FETCH_HEAD
    git ls-files -z | xargs touch   :: before building (ninja mtime guard)
    bash tools/check-vanilla-plus.sh   :: expect 326/326
    d2boot.log line 1 pins the build: Vanilla+0.0.7-R88
