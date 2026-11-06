# main branch — handoff (rev 135, 2026-11-06 — the docs-restructure round, no code changes; self-ID stays **`Vanilla+0.0.8-R127`**, gates **510/510** (+1 docs-budget gate, §8). Build state: release line **0.0.8**, rounds tracked as `-Rxx`; upstream synced to Ghidra **1948** (eight numbered syncs — details H39/H40 + the R105/R123 entries in CHANGELOG/H41); **the owner is running the R127 win11 local build now**. NEWEST code round = **R127, the vpdiag retirement** — owner field-green on R126: "Great job, works. Let's retire the vpdiag probes". The R120 wide-view instrumentation left whole (the `VPDIAG_WalkStats` struct + externs, the per-frame scratch + `VPDIAG_ResetFrame` + the throttled dumper + both call sites, the collector's per-call stats + scene snapshot, the lightmap clamp counters, Wall2's diag-only include) — zero behavior change: every removed line was a counter increment, a dump call or a declaration, and the wide-view fixes the probes shipped with stay (R117 fill, R118/R121 walk + hop-4, R121 un-gate + snapshot triggers, R122 cull re-fit). 14 `r120:` pins retired, +6 `r127:` tombstones pin the absence; no field ask — `d2boot.log` line 1 stays the boot sanity check. PREVIOUS rounds, one line each (full records: CHANGELOG + **H41/H42**): **R126** the shield-compare slot fix (same-kind slot pick when both arm cells are occupied — shield/quiver hover → off-hand, weapon hover → primary, per retail's own auto-equip convention; release line rolled 0.0.7 → 0.0.8; FIELD-VERIFIED "Great job, works"); **R125** the Shift Item Compare feature (hold-Shift equipped-item compare tooltip, D2R parity, Options row 9, shop stock included); **R124** the D3D fullscreen boot-crash fix (the eighth-sync merge split `UploadTextureToCache`'s node-base spelling; FIELD-VERIFIED "d3d fullscreen works again"); **R123** the eighth upstream sync (merge-only); **R117–R122** the wide-view rendering campaign. **Docs restructure this rev** (owner: "Some documents are intended for humans. Those need to be more concise. Some are intended for Agents. those can be verbose. The handoff.md should maintain a limit of 16kb. HISTORY.md can grow unbounded. All plan's are for Agents. README and CHANGELOG are for humans."): README/CHANGELOG/SMOKE re-cut concise, plans moved to `plans/`, the pre-restructure texts archived verbatim (CHANGELOG → **H41**, this file → **H42**, SMOKE → **H43**); the doc policy is §8's first bullet.

## 1. Branch topology (owner-corrected, 2026-10-31; repo renamed + wrapper spun out, 2026-11-03)

- **Repository**: **`D2VP-QoL`** (renamed from `D2LoD-libslang-vk` 2026-11-03; old URLs redirect). **All Glide DLL work happens in the wrapper's own repository: [`VTSTech/Glide3xVK`](https://github.com/VTSTech/Glide3xVK)**.
- **`main`** — the only integration branch. The `720p` suite (merge `fec99ff`) and the vanilla-plus line are absorbed. Build self-ID **`Vanilla+0.0.8-R127`** (`Game/Main.cpp:1078`). Gates **510/510** (`tools/check-vanilla-plus.sh`). Work happens here.
- **`720p`** — retired, read-only archive (its handoff era: **H25**).
- **PR branches** (`pr1..pr5`, `jaenster-main`) — upstreamed already; **not integration targets; do not merge from them.** Read-only references for fix specs (e.g. `pr4/sound-except-intro` = upstream PR #10, the sound trio).
- Full pre-720p history: HISTORY.md H1–H24 (H3 = the first sound-defect series record; H17 = the supersession precedent; H21 = fix-chain ledger rows 1–27).

## 2. Field status (collapsed current-state table at rev 135 — the per-round rows live verbatim in H40/H42)

| Area | Status |
| --- | --- |
| Boot → intro → title → char select → in game; D3D fullscreen/windowed; Glide `-3dfx`; sound end to end; `-nointro` (R86) | **WORKING** — field-verified; the full SMOKE belt green twice (R83 [#24](https://github.com/VTSTech/D2VP-QoL/issues/24), R104 [#28](https://github.com/VTSTech/D2VP-QoL/issues/28)) |
| Resolution ladder: 640x480 / 800x600 / 1280x720 retail + **1344x700 / 1366x768 / 1920x1080** (R109–R115), hosted stretch, windowed `-w` (R116 fix) | **WORKING** — field-proven R109–R115 ("Great job. works" at each stop); the explicit 1366x768-windowed eyeball still owed (§7) |
| Wide-view rendering campaign (R117–R122: band fill, hop-4 walk, cull-rect re-fit) | **CLOSED** — proven on vpdiag evidence; the probes retired R127 (zero behavior change) |
| D3D fullscreen boot crash (R124) + shift-compare shield slot (R126) | **FIXED — FIELD-VERIFIED 2026-11-06** ("d3d fullscreen works again" / "Great job, works") |
| Vanilla+ features (nine, all opt-in + Options rows) | **SHIPPED + FIELD-PROVEN** (shared stash ten pages R82; `no_panel_shift` R95–R101; mana burn R106–R108; shift compare R125/R126) |
| Upstream sync | Ghidra **1948** (R123, the eighth); fork ahead, never behind |

## 3–5. Closed defect tracks A (sound) / B (glide) + re-litigation ledger — ARCHIVED

Track A sound (field-verified rev42), track B glide (R46, issue #11) and the §5 ledger moved verbatim to **HISTORY.md H26** at rev 58. Both tracks FIELD-VERIFIED — no open work; section numbers stay reserved so §6–§9 cross-references hold. The glide known-issue step is **H26 §4.3 step 4**.

## 6. Standing watches (carry-overs, unchanged)

- `roster heal:` in d2boot.log → upstream roster corruptor alive (rev35/36 stack; context H25). A cap mark now means REAL corruption, not a benign shape (R60 re-scope, issue #19 audit); the rev34–36 immunization contained the 2026-10-31 hit. If it recurs, send the log (hangwatch + `d2exe.map` as usual).
- Track J follow-up (gossip-data recovery) — RESOLVED at the R88 merge (upstream recovered the retail tables verbatim); the R53/R54/R85 `quest:` marks stay as field diagnostics.
- `s2c dispatch guard:` / repeating `s2c pump: drained` → packet-path pathology (rev34 hardening; H25 §8).
- hangwatch dumps at 12 s stalls; keep `d2exe.map` beside d2vp.exe for symbolization.
- d2boot.log first mark must read the current self-ID string — pins the exe before any analysis.
- SetPan right-branch negation deviation (stereo position only; H3 watch item).

## 7. Field checklist — the still-owed items

- **Issue [#25](https://github.com/VTSTech/D2VP-QoL/issues/25)** (track R): Refilling Shrine XP buff — suspected wrong-shrine-effect application; triage pending.
- **Issue [#18](https://github.com/VTSTech/D2VP-QoL/issues/18)** (track Q): intermittent attribute-point crash — never triaged; reproduce with `d2exe.map` beside the exe, collect `d2crash-*.txt`.
- **Owner closes owed**: #26 (track S — field-proven R92–R101, cleared by the R104 full smoke pass) and #8 (track C — field-verified R91, record §9.1).
- **R116 owed**: the explicit windowed (`-w`) eyeball at 1366x768 (frame straight, no shear at the window edges) — `-w` boots clean on the R123/R124 field builds; plus the formal six-stop switch-ladder walk (the 1080p stop is field-proven by use).
- **Cosmetic label follow-ups** (from #26): overhead icons, alt-labels, the border-3 guards.
- Closed tracks (records in their H-entries): C uniques R49 [#9], C skeletons R51 (§9.1), I belt hover R52 (H27), J Den turn-in R54 (H28), M save-re-enter R61 [#19] (H31), P Bloodraven R85 [#22] (H35/H36).

## 8. Conventions (condensed; full historical texts in H8/H42)

- **Docs policy (owner, 2026-11-06)** — two lanes: **human** = README.md + CHANGELOG.md (concise; CHANGELOG = one short entry per release) + SMOKE-CHECKLIST.md (live checks only; closed-round history → HISTORY); **agent** = verbose-OK: handoff.md, HISTORY.md (unbounded), `plans/*` (all plans), FEATURES.md (the slate), docs/*. **handoff.md has a hard 16 KB budget, gate-enforced** (`tools/check-vanilla-plus.sh`): when it grows past the cap, offload the oldest-again material verbatim into the next HISTORY.md H-record (the trim-N pattern, H25–H43) and re-run the gate. CHANGELOG entries stay short — the engineering detail goes in the handoff rev line and, when it trims out, HISTORY.
- **Overlay workflow**: sandbox edits the tree → overlay **zip of changed files** (regular rounds; bundles only for upstream merges) → owner applies as own commits → llvm-mingw i686 Win11 build (MSYS2 MINGW32). The full build is the owner's gate.
- **VP-BEGIN/VP-END markers** on every fork edit in upstream files; fork-only TUs (`VanillaPlusCore.{h,cpp}` at `D2Client/UI/`) keep upstream files free of new code.
- Every source change updates the `tools/check-vanilla-plus.sh` gate table. (The owner-side fork-surface budget/allowlist check is RETIRED rev43.)
- Line endings: **merged main is LF-normalized** (rev41 finding) — patchers detect and preserve each file's ending, never mix.
- **Version self-ID** (`Game/Main.cpp:1078`) bumps every code round so d2boot.log pins the build; docs-only rounds do NOT bump it. The suite carries a single self-ID row, repinned every round; the build check itself is manual: d2boot.log line 1 (SMOKE-CHECKLIST item 0).
- **Sandbox**: llvm-mingw i686 cross-toolchain available since R90b — no apt; **the owner's MSYS2 build stays the compile gate of record.** Per-round changed-TU compile checks must pass the CMake forced-include set (`-include d2_platform.h -include support/d2_hand.h`).
- Diagnostic channels: `d2boot.log` (`[d2boot]` marks, permanent), hangwatch stall dumps, the game log (`DirectSound error:` lines), `glide3x.log` (wrapper side).
- Full history: H1–H24 (pre-720p), H25 (720p era), H26 (0.0.6 post-release era), H27/H28 (tracks I/J), H29–H31 (rev offloads 57–109), H32 (README offload), H33–H40 (trims #8–#16: the R84-era fixes through the R89–R101 arc), **H41 (CHANGELOG full text, pre-restructure), H42 (handoff pre-trim #17), H43 (SMOKE pre-restructure)**.

## 9. Known-issue manifest (rev-135 status: TWO open — tracks Q/R; track S + C FIELD-PROVEN, owner closes pending; closed rows live in their H-records)

| # | Owner symptom (verbatim) | Track |
| --- | --- | --- |
| 12 | "**Bug** 'Refilling Shrine' gives XP buff" ([issue #25](https://github.com/VTSTech/D2VP-QoL/issues/25) — owner, 2026-11-02) | R — suspected wrong-shrine-effect application (XP belongs to a different shrine type); triage pending |
| 13 | "**Crash** Intermittent crash when applying attribute points" ([issue #18](https://github.com/VTSTech/D2VP-QoL/issues/18) — owner, 2026-10-31) | Q — never triaged; intermittent — reproduce with `d2exe.map` beside the exe, collect `d2crash-*.txt` |

Open: **rows 12, 13** (issues #25, #18 — tracks R, Q). Row 11 (issue #26, track S — field-proven R92–R101, the §L standing asks cleared in the R104 full smoke pass) and row 3 (issue #8, track C) lift to history at the owner's close.

### 9.1 Track C — minion skeletons gold — FIXED R51, FIELD-VERIFIED R91 ("Skeletons are finally white again") — issue #8

A baseline transcription defect (`44e3518`) with two inverted-polarity write sites — the roster pet-recolor `(uint32_t)!bHostile` (R50) and the spawn-time `CreateObject` `Utrans==0xFF` fallback (R51, the write that sticks for SP summons); both now write the raw hostility flag. Full record verbatim: **H31** (original H26 Revs 55/56). Lifts to history at the owner's close.
