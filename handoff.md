# main branch — handoff (0.0.7 line current: the first 0.0.7 build = the R66 content state (tag `R67`, self-ID `Vanilla+0.0.7-R66`, release notes covering R59→R66), the R67→R77 block shipped 2026-11-01 as tag `R77` @ `866883e` ("Vanilla+ 0.0.7 (R77)"), later 0.0.7-line tags `R78`–`R83`; per-release summaries now also in **CHANGELOG.md** (rev 110); tip `63e020a3` = tag **`R84`** — **`Vanilla+0.0.7-R84`**, the item-tooltip zero fix ([issue #16](https://github.com/VTSTech/D2LoD-libslang-vk/issues/16)) — FIELD-VERIFIED + PUBLISHED (owner, 2026-11-02; SMOKE §J items 37-38 proven); self-ID `Vanilla+0.0.7-R84` (`D2Client/Engine/Application.cpp:66`), gates **311/311** (`tools/check-vanilla-plus.sh`); upstream synced to Ghidra 1806 (merges 3d645e7 + 19f9791 + d6d4b53 → 6c552f9 → ea0b84d4 → f518fda0 → f427f962) — every merge field-run green, the merge belt full-smoke-proven on the R83 pass (owner, 2026-11-02, [issue #24](https://github.com/VTSTech/D2LoD-libslang-vk/issues/24)); ONE open defect: track C minion-skeleton color ([issue #8](https://github.com/VTSTech/D2LoD-libslang-vk/issues/8)) — ROOT-CAUGHT v2 + FIXED R51, field-verify pending (§9.1); shared stash FIELD-PROVEN end to end + ten pages (R82), SMOKE §H items 25/27 owed; the roster-heal corruptor watch ARMED (§6); round-by-round records: revs ≤ 91 → HISTORY.md H29/H30; the rev-109 status header + revs 92–109 + the closed §9.5–§9.7 track records → **H31** (trim #8, rev 110); revs 110-111 → **H33** (trim #9, rev 112); docs line current through **Rev 112**)

*Rev 112 (2026-11-02, docs round — owner: "R84 fix for #16 verified. R84 published": tag **R84** @ `63e020a3`; SMOKE §J items 37-38 FIELD-PROVEN — the item-tooltip zero fix SHIPPED; §2 R84 row closed, §7 asks now the standing §H 25/27 only; CHANGELOG [Unreleased] R84 entry → **[R84]** released section; README badge/status → R84; trim #9: revs 110-111 → **H33**; zero source changes, self-ID `Vanilla+0.0.7-R84`, gates 311/311): docs-only round.*

## 1. Branch topology (owner-corrected, 2026-10-31)

- **`main`** — the only integration branch. The `720p` suite (merge `fec99ff`) and the vanilla-plus line are absorbed. Build self-ID **`Vanilla+0.0.7-R84`** (`D2Client/Engine/Application.cpp:66`; release line 0.0.7, rounds tracked as `-Rxx` — owner directive R45); gates **311/311** (`tools/check-vanilla-plus.sh`). Work happens here.
- **`720p`** — retired, read-only archive. Its handoff era (revs 28–39) lives in **HISTORY.md H25**.
- **PR branches** (`pr1..pr5`, `jaenster-main`) — sent upstream (jaenster/redivivus) already; **not integration targets; do not merge from them.** They remain useful as read-only references for fix specs (e.g. `pr4/sound-except-intro` = upstream PR #10, the sound trio).
- **`Glide3xVK`** — the glide3x.dll wrapper branch. **All Glide DLL work happens there.** `D2Glide/` + `D2Client/Renderer/Glide.cpp` in main are the game-side glide renderer client and stay in main.
- Full pre-720p history: HISTORY.md H1–H24 (H3 = the first sound-defect series record; H17 = the earlier supersession precedent; H21 = fix-chain ledger rows 1–27).

## 2. Field status (current at rev 111, 2026-11-02 — the 2026-10-31 table this replaces is verbatim in H31)

| Area | Status |
| --- | --- |
| Boot → intro → title → char select → in game; D3D fullscreen/windowed (640x480 / 800x600 / 1280x720); Glide `-3dfx`; sound end to end | **WORKING** — field-verified (per-area records H26; glide fix [issue #11](https://github.com/VTSTech/D2LoD-libslang-vk/issues/11), auto-gold [#10](https://github.com/VTSTech/D2LoD-libslang-vk/issues/10), uniques [#9](https://github.com/VTSTech/D2LoD-libslang-vk/issues/9)) |
| 720p UI: inventory, skill flyouts, gold dialogs, belt hover, waypoint icons (R81), video-options labels (R83) | **WORKING** — the full SMOKE belt passed on the R83 build (owner, 2026-11-02, [issue #24](https://github.com/VTSTech/D2LoD-libslang-vk/issues/24)) |
| Track C — minion skeletons gold ([issue #8](https://github.com/VTSTech/D2LoD-libslang-vk/issues/8)) | **ROOT-CAUGHT v2 + FIXED R51 — field-verify pending** — the ONE open defect (§9.1) |
| Shared stash (track N) | **FIELD-PROVEN end to end** (R65/R66/R74) + ten pages (R82); SMOKE §H items 25/27 owed |
| Upstream sync Ghidra 1783 → 1806 (R62, R72–R80) | **FIELD-RUN green per merge**; the merge belt full-smoke-proven on the R83 pass |
| R84 — item-tooltip zero fix ([issue #16](https://github.com/VTSTech/D2LoD-libslang-vk/issues/16)) | **SHIPPED — tag R84 @ 63e020a3, FIELD-VERIFIED** (owner, 2026-11-02; SMOKE §J items 37-38 proven) |

## 3–5. Closed defect tracks A (sound) / B (glide) + re-litigation ledger — ARCHIVED

Track A sound (field-verified rev42, "Sound works."), track B glide (R46, issue #11, "ok Glide i working now") and the §5 re-litigation ledger moved verbatim to **HISTORY.md H26** at rev 58 (owner: "trim the handoff, move things to historical that are done"). Live cross-reference kept for the checklist: the glide known-issue step is **H26 §4.3 step 4**. Both tracks are FIELD-VERIFIED — no open work; the section numbers 3–5 stay reserved so §6–§9 cross-references keep pointing at the same places.

## 6. Standing watches (carry-overs, unchanged)

- `roster heal:` in d2boot.log → upstream roster corruptor alive (rev35/36 stack; context H25). **R60 re-scope (issue #19 audit)**: the regroup/flatten walks are provably correct on clean data (head-match splice present since the port) — a cap mark now means REAL corruption, not a benign shape; R60 adds the exit/join `roster: clear lists` / `roster state:` probes so one log also shows whether the state was stale across sessions. **WATCH HIT 2026-10-31 (owner's R54 field log, SP)**: two marks during load, right after the 0x5B join (party=-1) — `regroup iterations walk hit the 64-node cap` + `sort-party-lists outer walk hit the 64-node cap`, both `corrupt chain, degraded`. First live sighting on the 0.0.6-era build; the rev34–36 immunization contained it (world entered, session completed normally, no symptom reported). The two marks name the walks for the next narrowing pass; if it recurs, send the log (hangwatch + `d2exe.map` beside d2.exe as usual).
- Track J follow-up (gossip-data recovery): ~18 quests have singular `aNpcGossipData` tables while their handlers use slot ≥ 1 (A1Q0, A1Q2, A1Q6, A1Q7, A2Q0, A2Q7, A3Q0, A3Q5, A3Q6, A4Q1, A4Q3, A4Q4, A5Q1, Quest37/38/39/40) — their flavor/reward lines are absent from the tree data; recovery needs the vanilla binary's `.data` or an upstream reference (owner's call). Symptom shape if one bites: the issue #13 silent-no-reward shape for another quest; the R53/R54 `quest:` marks name the site in one log.
- `s2c dispatch guard:` / repeating `s2c pump: drained` → packet-path pathology (rev34 hardening; H25 §8).
- hangwatch dumps at 12 s stalls; keep `d2exe.map` beside d2vp.exe for symbolization.
- d2boot.log first mark must read the current self-ID string — pins the exe before any analysis.
- SetPan right-branch negation deviation (stereo position only; H3 watch item).

## 7. Field checklist (re-litigation pass on main)

- **Pending from the 720p era (H25 §6)**: waypoint / quest log / party / help deep-pass; sibling dialogs (trade gold, cube UI, NPC menus, EscMenu); in-game 800↔720 switch (coord-cache refetch); 800x600 byte-parity + 640x480 untouched.
- **Sound — title music + in-game audio field-VERIFIED rev42 ("Sound works.")**; keep the fine-grain pass as regression watch: UI clicks, NPC speech, footsteps, quest fanfares, pickup-fail voice, music transitions, volume-slider behavior.
- **Glide**: H26 §4.3 step 4 (known issue #1).
- **Track C (§9.1)**: uniques FIELD-VERIFIED R49 (issue #9 CLOSED); skeleton symptom ROOT-CAUGHT v2 + FIXED R51 (roster recolor polarity R50 + spawn polarity R51) — field-verify pending.
- **Track I (§9.5, [issue #14](https://github.com/VTSTech/D2LoD-libslang-vk/issues/14))**: CONFIRMED on R52 (owner: "R52, Hover belt confirmed fix") — CLOSED; arc record in H27, public writeup `ISSUE-14-CLOSE-COMMENT.md`.
- **Track J (§9.6, [issue #13](https://github.com/VTSTech/D2LoD-libslang-vk/issues/13))**: A1Q1 Den of Evil turn-in — CONFIRMED on R54 (owner: "Great Job. Works."; the 7-step chain ending `quest: den reward GRANTED` + the skill point, first try) — CLOSED; arc record in H28, public writeup `ISSUE-13-CLOSE-COMMENT.md`. Follow-up watch moved to §6 (~18-quest gossip-data recovery, owner's call).
- **Track M (§9.7, [issue #19](https://github.com/VTSTech/D2LoD-libslang-vk/issues/19))**: save-and-re-enter crash — ROOT-CAUGHT + FIXED R61, FIELD-VERIFIED R61 — CLOSED (owner: "R61 Confirmed fixes #19"; VP Resolution row table mutation; SMOKE §G items 18-20 field-proven).
- **Upstream-sync chain + the R84 round (the current arc, [SMOKE-CHECKLIST.md](SMOKE-CHECKLIST.md) §I/§J)**: the merge belt rode along on the R83 field pass, so the R62–R80 sync chain (Ghidra 1783 → 1792 → 1794 → 1801 → 1806) is full-smoke-proven end to end ([issue #24](https://github.com/VTSTech/D2LoD-libslang-vk/issues/24)); §J items 37-38 CLOSED (field-proven on the owner's R84 verify pass — tag R84); remaining standing asks = §H items 25/27 (`.d2s`-in-retail, degrade drill).

## 8. Conventions (condensed, unchanged)

- **Overlay workflow**: sandbox edits the tree → overlay **zip of changed files** (owner preference, not .patch) → owner applies as own commits → llvm-mingw i686 Win11 build (`tools/build-local-win11.cmd`; MSYS2 MINGW32 `cmake --build build -j 8`, ~501 targets). The full build is the owner's gate.
- **VP-BEGIN/VP-END markers** on every fork edit in upstream files; fork-only TUs (`VanillaPlusCore.{h,cpp}` at `D2Client/UI/`) keep upstream files free of new code.
- Every source change updates the `tools/check-vanilla-plus.sh` gate table. (The owner-side `tools/check-fork-surface.sh --update` step is RETIRED rev43 — the fork-surface budget/allowlist was deleted; main never sends PRs, clean branches off main carry PR hygiene.)
- Line endings: **merged main is LF-normalized** (rev41 finding; only overlay-era leftovers such as `D2Net/SRC/Client.cpp` are CRLF) — patchers detect and preserve each file's ending, never mix.
- **Version self-ID** (`Application.cpp:66`) bumps every code round so d2boot.log pins the build — the **0.0.6 cut ships as `vanilla-plus 0.0.6 (rev41)`** (field-confirmed); the docs-only cut round does NOT bump it, and the next code round takes 0.0.7 + the next rev number (final naming owner's call). The gate suite does NOT pin the self-ID (Rev 58 — gates are for features and fixes); the build check is manual: d2boot.log line 1 (SMOKE-CHECKLIST item 0).
- Sandbox: **no mingw toolchain, no apt** — constants-level fixes are verified by diff review + curve math; the owner's local build is the compile gate.
- Diagnostic channels: `d2boot.log` (`[d2boot]` marks, permanent), hangwatch stall dumps, the game log (`DirectSound error:` lines), `glide3x.log` (wrapper side), `d2loud-*.txt` (LOUD builds only). Sandbox tooling of record: `scripts/rev41_sound_fp_decode.py` (ending-aware patcher), `scripts/rev41_curve_check.py` (volume-curve harness).
- Full history: HISTORY.md H1–H24 (pre-720p), **H25** (the 720p era, incl. the freeze-era reading guide referenced by the `d2boot:` gate comments), **H26** (the 0.0.6 post-release era: revs 40–56 + closed tracks A/B/D/F/G/H + the C-uniques half of track C), **H27** (track I belt hover), **H28** (track J Den turn-in), **H29** (the pre-R61 round-by-round record, revs 57-70), **H30** (the pre-R74 round-by-round record, revs 71-91), **H31** (trim #8, rev 110), **H32** (README trim #1, rev 111), **H33** (trim #9: revs 110-111, rev 112). Public writeups: the close comments on [issue #14](https://github.com/VTSTech/D2LoD-libslang-vk/issues/14) and [issue #13](https://github.com/VTSTech/D2LoD-libslang-vk/issues/13); per-release summaries on the GitHub releases page (0.0.6-era) and in **CHANGELOG.md** (the per-release digest, rev 110).

## 9. Known-issue manifest (opened at the 0.0.6 cut; closed rows collapsed to pointers at rev 110)

| # | Owner symptom (verbatim) | Track |
| --- | --- | --- |
| 3 | "Minion Skeletons are Gold" (issue #8, 2026-10-29) | C — **ROOT-CAUGHT v2 + FIXED R51, field-verify pending** (TWO inverted-polarity write sites of the same baseline defect: roster pet-recolor `!bHostile` fixed R50; spawn-time `CreateObject` `Utrans==0xFF` fallback `!uMercType` fixed R51 — SP summons are never roster-recolored, so the spawn value is the one that sticks; fix = raw hostility flag at both sites) |
| 8 | "[720p] Hover Belt Slots do not stay open" ([issue #14](https://github.com/VTSTech/D2LoD-libslang-vk/issues/14) — owner field report, 2026-10-31) | I (§9.5) — **FIELD-VERIFIED R52 — CLOSED** — record H27 + H31 |
| 9 | "A1Q1 Den of Evil does not complete after killing all monsters in Den of Evil" ([issue #13](https://github.com/VTSTech/D2LoD-libslang-vk/issues/13) — owner, 2026-10-31) | J (§9.6) — **FIELD-VERIFIED R54 — CLOSED** — record H28 + H31 |
| 10 | "Saving a game and then trying to enter a game again crashes the game" ([issue #19](https://github.com/VTSTech/D2LoD-libslang-vk/issues/19) — owner, 2026-10-31) | M (§9.7) — **ROOT-CAUGHT + FIXED R61 — FIELD-VERIFIED — CLOSED** — record H31 |

Closed rows 1/2/4/5/6/7/8/9/10 (tracks B, C-uniques, D, F, G, H, I, J, M) → rows 1-9 archived in HISTORY.md H26 / H27 / H28 (offloaded at revs 58/59/64); row 10 + the closed-row full texts archived in **H31** (rev 110 — the §9.7 archive ritual). Open: **#3 (issue #8) only**.

### 9.1 Track C — minion skeletons gold — **ROOT-CAUGHT v2 + FIXED R51, field-verify pending (issue #8)** — the uniques half is CLOSED (R49, issue #9, record in H26)

- **Status**: a baseline transcription defect (`44e3518`) with TWO inverted-polarity write sites — the roster pet-recolor `(uint32_t)!bHostile` (R50; event-driven, inert for SP summons) and the spawn-time `CreateObject` `Utrans==0xFF` fallback `(uint32_t)!uMercType` (R51; the write that sticks for SP summons, packet path via `SCmd.cpp:523`). Both sites now write the raw hostility flag; gates `roster: pet recolor polarity (R50)` + `spawn: ownership palshift polarity (R51)`. Full root-cause records verbatim: **H26** Revs 55/56 + the §9.1 copy in **H31**.
- **Next discriminator (if R51 still field-fails)**: the palshift.dat entry parse itself (loader table order vs retail) — force idx 2 (TransLvl path) on a skeleton and compare its render against the idx 0/1 renders. Mechanism map + the closed uniques record: H26 §9.1 closed half.

### 9.5 Track I — (720p) belt hover popup — FIELD-VERIFIED R52 — CLOSED ([issue #14](https://github.com/VTSTech/D2LoD-libslang-vk/issues/14)) — record: HISTORY.md H27, verbatim copy H31

### 9.6 Track J — A1Q1 Den of Evil does not complete — FIELD-VERIFIED R54 — CLOSED ([issue #13](https://github.com/VTSTech/D2LoD-libslang-vk/issues/13)) — record: HISTORY.md H28, verbatim copy H31

### 9.7 Track M — save a game, re-enter crashes — ROOT-CAUGHT + FIXED R61, FIELD-VERIFIED — CLOSED ([issue #19](https://github.com/VTSTech/D2LoD-libslang-vk/issues/19)) — record: HISTORY.md H31 (archived at rev 110)
