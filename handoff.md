# main branch — handoff (0.0.6 RELEASED: tag `D2LoD-Vanilla-Plus-0.0.6` @ `0eb2655`, R45 field-verified incl. track F; open tracks: B — glide first-frame div0, crash report pending; C — unique/minion colors; docs line current through Rev 46)

*Rev 46 (2026-10-31, release + docs round — **0.0.6 is OUT**, zero source changes): owner verbatim: **"Great job. Released."** — tag **`D2LoD-Vanilla-Plus-0.0.6`** pushed at `0eb2655` ("Vanilla+0.0.6-R45"); the shipped exe self-IDs `Vanilla+0.0.6-R45`. **Track F FIELD-VERIFIED by the release field round** (owner: *"Skills Popout is fixed"*) — §9.3 CLOSED; tracks D and F now both closed, leaving the known-issue manifest at **track B** (glide `-3dfx` first-frame own-code div0, ~0.5 s in — `d2crash-20261030-211109.txt` still the one-shot data ask) and **track C** (unique monsters base colors + unique-pack minion skeletons gold, §9.1). Docs shipped this round: `RELEASE-NOTES-0.0.6-R45.md` (0.0.5 page format: Added/Fixed/Internals/Known issues/Upgrade notes) and a full **README.md rewrite** — the front page no longer carries upstream Redivivus' text; it now presents the fork (two-layer diagram, field status, the four gated features + Options page, install/build, the 0.0.1→0.0.6 release table, the two verification instruments, the two-artifact crash protocol, docs map, credits). `VANILLA-PLUS.md` latest-release pointer 0.0.5→0.0.6. Versioning of record unchanged: the tree stays **0.0.6** post-release, rounds tracked as `-Rxx`; this is a docs-only round (no Application.cpp bump, no gate re-pin; suite re-run **194/194**). Next code round takes **R46/R47** in Application.cpp + both version gates (§8 convention).

*Rev 45 (2026-10-31, field-fix round — **track F rooted and fixed**, self-ID bumped **`Vanilla+0.0.6-R45`**; **naming convention of record changed by owner directive**: the tree stays **0.0.6 until release**, rounds tracked as **`-Rxx`** — owner verbatim: *"this is still 0.0.6, it will be 0.0.6 until it releases. Revisions tracked with -Rxx"*; the owner's own commit `9a06341 "Vanilla+0.0.6-R44"` had already renamed the self-ID string but left both version gate regexes matching the old `0.0.7 (rev44)` form — re-pinned sandbox-side, 191/191, before this round's work). **Track F — skills flyout opens on the wrong side of the screen — ROOT-CAUGHT, same class as track D (merge casualty at the call sites, half-applied flip)**: `SPELLSEL_RenderAllSkillGrids` (`D2Client/UI/SpellSel.cpp`) draws each flyout grid in two passes — an **icon pass** (which also registers the click regions via `SPELLSEL_RegisterSkillSlot`) and a **tooltip pass** — and the Ghidra decomp-regen commits (`a91f7ac`/`aa93bfc`, post-site-5) scrambled the two icon-pass X anchors while BOTH tooltip passes kept the correct wide-mode anchors: right-hand grid icons drew at the swap grid's anchor `0x50 + VPRES_GetWideUiOffsetX()` (= x 320 at 1280x720 — LEFT half of the screen) instead of `ScreenSizeX - VPRES_GetWideUiOffsetX() - 0x80` (= x 912, above the hotbar), swap-grid icons drew at vanilla `0x50` (= x 80, off the centered panel) instead of `0x50 + offset` (= 320), and the `nStartX` store feeding the right grid had lost its offset term (dead store — the merge-port churn also swapped the if/else bodies and flipped `bIsRightHand`, leaving the correct anchors computable but unconsumed). Net field signature: icons + click regions on the left, tooltips on the right — the flyout "pops up on the wrong side". Fix restores the exact anchor expressions field-verified on the pre-regen tree (`git show 13aa82e~1` — the last known-good wide-mode state of this function, diff-verified sandbox-side: only decompiler branch-layout churn + R45 comments differ): icon passes re-paired with their tooltip passes; `nStartX` offset restored. **Retail no-op proof**: `VPRES_GetWideUiOffsetX()` = 0 at 640x480/800x600, so all three sites reduce to the vanilla expressions byte-identically. Y side untouched — `nCurY = ScreenSizeY - 0x56` is already bottom-anchored and correct. Click handler `INPUT_SpellSelectClick` is fully region-driven (no hardcoded X), so hit-testing follows the fix automatically. Gates: **194/194** — NEW anti-reflip rows: spell right-grid icons bar-anchored (`nStartX` consumers = 2), swap-grid icons ride panel (`0x50 + offset` = 2), absence gate holding the plain-`0x50` icon anchor out, and the spell-grid offset count re-pinned 4→5. Next: owner rebuild (this IS a source round), first d2boot.log mark must read `Vanilla+0.0.6-R45`; field asks — both hotbar skill buttons open their flyouts hugging the control panel above the skill buttons at 720p (icons, tooltips AND clicks on the same cells), 800x600 unchanged. Overlay: `vanilla-plus-0.0.6-R45-skills-flyout-fix.zip`.

*Rev 44 (2026-10-31, field-fix round — **track D + track E rooted and fixed**, self-ID bumped **`vanilla-plus 0.0.7 (rev44)`**): owner field round delivered the glide log, the 720p inventory screenshot, and the options-menu report. **Track B — stale-DLL era CLOSED**: glide3xvk 0.4.6 (64916d0) loads into d2s-18ed945.exe, Vulkan swapchain 800x600 up (RX 560, FIFO), slang chain ready, first frame's state sets flow — then **562 ms after provider init an own-code integer divide-by-zero** (0xC0000094, eip 007671B2 = RVA `0x3D71B2` over base 00390000), last glide calls = chroma/chromakeyValue (first chroma'd image draw). Tree sweep CLEARED the obvious suspects: `RENDERER_GLIDE_BuildPerspectiveLUT` (`D2Glide/Renderer/Glide.cpp:3081`) clamps to 2999 so `nClampedZ-3000` ranges [-5999,-1] and never zeroes; every other integer division in D2Glide divides a constant; the float inversions cannot raise 0xC0000094. **The ask: `d2crash-20261030-211109.txt`** (14 frames, d2exe.map-resolved — names the site in one shot). **Track D — ROOT-CAUGHT, a merge casualty at the call sites**: main's `InitializeiInventoryPages` ran every `VP_SHIFT_*` call BEFORE its paired `Get*Dimensions` fetch; the TokTbls fetchers are pure overwrites (`D2Common/DATATBLS/TokTbls.cpp:806–894`), so all 40 table-rect sets (page/grid/body across inventory, hireling, trade 1/2, bank, big-bank, cube, guild vault, trophy, npc shop) stayed vanilla-800x600-absolute while the panel art rides the bottom-anchored ui.cpp machinery — measured off the owner's 720p shot: art grid cells at client x 657..947 = exactly 417+240, HUD/orbs correctly shifted, but the two throwing potions rendered at vanilla-screen grid cols 8-9 row 1, i.e. 240 left/120 above their cells, over the paperdoll. `git diff 720p main --ignore-cr-at-eol` shows the flip pair-by-pair (720p had fetch→shift) — **the rev42 "machinery intact and identical" verdict covered the shift function bodies, not the call order; correction of record.** Fix: `scripts/rev44_inv_shift_order.py` swapped all 40 pairs back to fetch→shift (asserts: 40 swaps, VP_SHIFT_Inv=43, fetches=40, per-pair target match). **Track E — ROOT-CAUGHT, latent since rev 18**: the row-10 page draw sat INSIDE the `GFX_bShowUIFlag.ConfigControls` guard in `DRAW_UI` (`Draw.cpp:541`); `ESCMENU_ShowVanillaPlusOptions` sets ONLY UI_VANILLAPLUS (ConfigControls stays clear), so the nested check never evaluated — owner's field report pins it exactly (row label visible, click closes the menu, game world shows, page invisible, Esc round-trips via the registered input batch); the rev C/D/E field rounds only QA'd the row label, so the page had never been exercised in the field. Fix: dispatch hoisted to a SIBLING of the ConfigControls guard. **Rev43 verified in-tree** (fork-allowlist retirement complete: `tools/check-fork-surface.sh` + `tools/fork-surface.allowlist` gone; `vanilla-plus-rev43-ci-cleanup.zip` + `REV43-APPLY.txt` delivered last round). Gates: **191/191** — version gate re-pinned to 0.0.7 (rev44); NEW `site-5: fetch->shift pair order` (awk per-pair order check — a future merge cannot silently re-flip); NEW row-10 draw-call + dispatch-hoisted rows. Next: owner rebuild (this IS a source round), d2boot.log first mark must read `0.0.7 (rev44)`; field asks — 720p inventory aligned (items in cells, clicks on cells), options page opens/draws/toggles/Esc, **track B crash report**, track C data asks (§9.1). Overlay: `vanilla-plus-rev44-inventory-options-fixes.zip`.

*Rev 43 (2026-10-31, CI/tooling round — **fork-allowlist checks retired**, zero source changes): owner directive verbatim: **"lets remove the fork-allowlist checks from the yml and scripts. this branch wont be sending PR's we'll create clean branches for that like we have already been doing - we can keep the vanilla-plus checks. they are informative."** Removed: `.github/workflows/build.yml` — the upstream remote add/fetch, the `tools/check-fork-surface.sh` invocation, and `fetch-depth: 0` (which existed only for the upstream whole-diff; step renamed "Vanilla+ merge gates (features intact)"; `check-build.sh` verified shallow-safe — it clones locally); **deleted `tools/check-fork-surface.sh` + `tools/fork-surface.allowlist`** (the 888-line budget file; deletions cannot ride an overlay zip — `REV43-APPLY.txt` orders the owner-side `git rm`); `tools/check-vanilla-plus.sh` header (companion-script mention dropped, VP-marker note kept); `docs/merge-upstream.md` (step-4 audit line, the `check-fork-surface FAIL` bullet, the whole-diff-budget bullet, "same two gates" → "same gate", and the upstream-citizen section now records the policy: **PRs are cut from clean branches off main, never from this branch's working state**). Kept: the full vanilla-plus gate suite — informative, re-run this round **188/188** — and the `pull_request:` triggers (harmless). Version self-ID unchanged: `vanilla-plus 0.0.6 (rev41)` is the shipped cut; no source files changed, no rebuild needed. Overlay zip: `vanilla-plus-rev43-ci-cleanup.zip`.

*Rev 42 (2026-10-31, **0.0.6 cut round** — handoff-only, zero source changes): owner field report verbatim: **"Great job. Sound works. 720p works. Still some issues. But let's cut 0.0.6 here."** The rev41 sound port is **FIELD-VERIFIED** — defect-12 decode + fix 16 + the hwnd fix all confirmed working on main's build; 720p re-confirmed. **0.0.6 = the build that self-IDs `vanilla-plus 0.0.6 (rev41)`** (first d2boot.log mark pins it); the string is intentionally NOT bumped this round so the shipped exe and the tree stay identical. Known-issue manifest at cut — four open tracks: **(1) Glide `-3dfx` does not boot** — track B (§4) unchanged, still waiting on `glide3x.log` + `d2boot.log` + deployed-DLL provenance; **(2) unique monsters render base colors** and **(3) unique-pack minion skeletons render gold** — new **track C (§9.1)**, one mechanism family; **(4) at 1280x720 the inventory is misaligned** — new **track D (§9.2)**. Re-litigation finding of record for C/D (this round, via content-only `git diff main 720p --ignore-cr-at-eol`): **not merge casualties** — the PalShift.cpp / Pallete.cpp / MonUnique.cpp diffs are decompiler-noise (guard inversions, `n > 0` ↔ `0 < n`, include order) and main's Inv.cpp is NEWER upstream code than the 720p-era file (trade-page Right/top/bottom/left split, `gbInvOverlayFlag`, `init_npc_coords` path) with the site-5/6 VP machinery intact and identical — `fec99ff` dropped nothing here. Seeds in §9 — C: hunt a defect-12-style constant-pin on the unique RandTransform index (`PALSHIFT_GetMonsterTransform` PalShift.cpp:307, `PALSHIFT_LoadMonsterPalShift` :209, draw-time `D2GFX_DrawShiftedImage` D2GFX.cpp:972 GLOBALPALLETE plumbing; the static `Monster.cpp:1238–1298` TransLvl path is structurally sane and NOT the suspect); D: audit every coordinate write in Inv.cpp's newer upstream paths for a paired `VP_SHIFT_*` call (43 shift vs 40 fetch sites counted — near-paired, so the miss is likely OUTSIDE `InitializeiInventoryPages`, e.g. the NPC-inventory block or trade splits), ask the owner which sub-panel misaligns, add a `VP_SHIFT_`-count gate row. Gates unchanged **188/188**. No new overlay; docs ride `vanilla-plus-0.0.6-cut-docs.zip`.

*Rev 41 (2026-10-31, sound-fix port round — "lets see if PR4 can fix sound in main"): ported upstream PR #10 commit 2 (`e614f5e`) onto merged main — the defect-12 FP-constant decode the merge port dropped. Pre-state verified first: main's `ClampVolume`/`ClampAttenuation` are **byte-identical to pr4's pre-patch state** (function-body diff), so the port is a clean transform; patches 1 (hwnd) and 3 (fix 16) confirmed already in main. **Landed (sandbox, uncommitted)**: `D2Sound/Src/D2SoundSmp.cpp` (6 literal decodes + two self-contained comments — verbatim pr4 text, one em-dash adapted to ASCII `--`; **no VP markers**: the sound series follows the fix-16/upstream-PR-10 convention), `D2Client/Engine/Application.cpp` (self-ID bump **`vanilla-plus 0.0.6 (rev41)`**), `tools/check-vanilla-plus.sh` (+5 sound gates incl. an absence gate holding the raw literals out; version gate re-pinned to 0.0.6 (rev41)). Toolchain-free verification: function-body diff vs pr4-fixed = code-identical; curve harness `scripts/rev41_curve_check.py` **ALL PASS** (255→0, 110→−730 raw −730.295, 0→−10000; monotonic over 0..255; inverse round-trip ≤ 1.1e-16; pre-fix integer-cast eps 4.547e18 pins every input to −10000 = the field signature). Gates: **188/188**. Finding of record: **merged main is LF-normalized** (only overlay-era leftovers such as `D2Net/SRC/Client.cpp` are CRLF) — the old "all source files CRLF" convention described the pre-merge tree; patchers must detect and preserve per-file endings (`scripts/rev41_sound_fp_decode.py` does). Remaining: **owner build + field round** — d2boot.log first mark must read `0.0.6 (rev41)`; expect title music + UI audio per the defect-12 field test ("Music works now! Both at title and in game."), then SFX/NPC speech (fix 16 already in main). If still silent: §3.4 discriminator (`dsoundInit=?`) and re-cut sounddiag marks (H4). After committing: fork-surface allowlist refresh owner-side. Overlay zip: `vanilla-plus-rev41-sound-fp-decode.zip`.

*Rev 40 (2026-10-31, handoff-refocus round — **handoff-only, zero source changes**): owner retired the `720p` branch (absorbed into `main` by merge `fec99ff`; PR branches all sent upstream). Owner status verbatim: **"Current status: D3D FS/Window booting. normal+720p res. Sound on Intro Video. No sound at/after Title Screen."** and **"the merge and port work is done so far as it compiles green and boots now. But the individual fixes vanilla-plus implemented have not been tested or re-litigated yet."** All 720p/crash-era handoff content (revs 28–39 + §1–§9) moved verbatim to **HISTORY.md H25**. This round verified in-tree facts for the two open tracks: **(A) SOUND — the merge port (`d6affdd`) dropped the defect-12 fix**: main's `D2Sound/Src/D2SoundSmp.cpp` again carries the raw IEEE-754 hex-literal casts (:769/:774/:785/:790/:793/:797) that were decoded and field-verified on the vanilla-plus line (H3 Defect 12, "Music works now! Both at title and in game.") and are still fixed on both `720p` and upstream `pr4/sound-except-intro` — as integer casts they are ≈4.54e18, not 0.0001, so every game-engine volume pins to DSBVOLUME_MIN (-10000) while Bink intro audio (own DirectSound) survives: exactly the field signature. Fix spec in §3.3 (constant decode only — do not re-audit the SMP layer, H3 exoneration). Fix 16 and the hwnd fix DID survive the merge. **(B) GLIDE — game-side site-7 verified intact** (size table, grSstWinOpen enums 16/17, perspective path; gates green); the DLL is built on branch `Glide3xVK` where the resolution table is absorbed in-source; prime suspect is a stale/unpatched deployed glide3x.dll; needs `glide3x.log` + `d2boot.log`. Gates re-run sandbox-side on merged main: **183/183**. No new overlay this round; the first code round on main bumps the self-ID (§8).*

## 1. Branch topology (owner-corrected, 2026-10-31)

- **`main`** — the only integration branch. The `720p` suite (merge `fec99ff`) and the vanilla-plus line are absorbed. Build self-ID **`Vanilla+0.0.6-R45`** (`D2Client/Engine/Application.cpp:66`; stays 0.0.6 until release, rounds tracked as `-Rxx` — owner directive R45); gates **194/194** (`tools/check-vanilla-plus.sh`). Work happens here.
- **`720p`** — retired, read-only archive. Its handoff era (revs 28–39) lives in **HISTORY.md H25**.
- **PR branches** (`pr1..pr5`, `jaenster-main`) — sent upstream (jaenster/redivivus) already; **not integration targets; do not merge from them.** They remain useful as read-only references for fix specs (e.g. `pr4/sound-except-intro` = upstream PR #10, the sound trio).
- **`Glide3xVK`** — the glide3x.dll wrapper branch. **All Glide DLL work happens there.** `D2Glide/` + `D2Client/Renderer/Glide.cpp` in main are the game-side glide renderer client and stay in main.
- Full pre-720p history: HISTORY.md H1–H24 (H3 = the first sound-defect series record; H17 = the earlier supersession precedent; H21 = fix-chain ledger rows 1–27).

## 2. Field status (owner, 2026-10-31)

| Area | Status |
| --- | --- |
| Boot → Intro → Title → Char Select → In game | WORKING |
| D3D fullscreen + windowed, 800x600 and 1280x720 | WORKING |
| Intro video sound | WORKING (Bink path — explains why "sound" is split) |
| Sound at/after title screen | **WORKING** (field-verified rev42 — track A §3 CLOSED) |
| Glide (`-3dfx`) boot | **NOT WORKING** → track B (§4) — known issue #1 at the 0.0.6 cut |
| Unique monster colors | **NOT WORKING** (base colors, no unique tint) → track C (§9.1) — known issue #2 |
| Unique-pack minion colors | **NOT WORKING** (minion skeletons render gold) → track C (§9.1) — known issue #3 |
| Inventory at 1280x720 | **WORKING** (field-verified R45 — owner verbatim: *"Inventory is aligned now"*; track D §9.2 CLOSED) |
| Skills flyout (hotbar skill-button click) | **WORKING** (field-verified R46 — owner verbatim: *"Skills Popout is fixed"*; shipped in tag `D2LoD-Vanilla-Plus-0.0.6`) — track F §9.3 CLOSED |

Sound is now FIELD-VERIFIED on main's build (defect-12 decode + fix 16 + hwnd fix); the rest of the re-litigation ledger (§5) and the §7 checklist still stand. The two NOT-WORKING rows above are the entire post-release known-issue manifest (§9): tracks D and F both closed (R45 fix, R46 field confirmation, shipped in tag `D2LoD-Vanilla-Plus-0.0.6`).

## 3. Defect track A — Sound silent at/after title screen — **CLOSED (field-verified rev42)**

Kept as the record of the defect class (IEEE-754 bit-pattern mistranscription) and the fix spec; §9.1 hunts the same class elsewhere.

### 3.0 The two audio paths (why intro works and everything after doesn't)

1. **Intro video (Bink)**: `BinkSetSoundSystem(BinkOpenDirectSound, 0)` (`D2Client/D2Client.cpp:4033`) — Bink opens its **own** DirectSound instance, independent of the game engine. Field-confirmed working.
2. **Game engine**: `SOUND_Initialize` at boot (`D2Client/Engine/Application.cpp:170–179`, real-hwnd fix already in) → `D2Sound/Sound.cpp` up to 3× `SOUND_InitDirectSound` attempts (EAX/3D downgrades) → on success: primary buffer play, `SOUNDSMP_CreateSoundThread`, `SOUND_InitMusicBuffer` (`SFileDdaInitialize` — the DDA music stream), `SOUNDSMP_SetGlobalVolume(0xff)`. Title music = DDA stream (`D2SoundSmp` → `SFileDdaBeginEx`); UI/game SFX = the sfx channels (`D2Client/GAME/Sound.cpp`). **Every volume→attenuation conversion funnels through `SOUNDSMP_ClampVolume` / `SOUNDSMP_ClampAttenuation` (`D2Sound/Src/D2SoundSmp.cpp`).**

### 3.1 Prime suspect (verified in-tree this round): the merge dropped the defect-12 fix

- main's `D2Sound/Src/D2SoundSmp.cpp` carries the raw hex-literal casts:
  - `ClampVolume` epsilons: :769, :774 — `(long double)(double)0x3f1a36e2e0000000`
  - `ClampVolume` scale: :785 — `(long double)(double)-0x3f60c00000000000` (decodes to −0.00341796875; retail scale is −2000.0)
  - `ClampAttenuation`: :790 epsilon, :793 bottom bound `(double)-0x3f3c780000000000` (decodes to −0.0004344), :797 divisor `(double)0x409f400000000000` (retail: 2000.0)
- In C++ `0x3f1a36e2e0000000` is the **integer** ≈4.54e18 (Ghidra printed the double `0.0001` as an IEEE-754 bit pattern and the port transcribed it literally), so `value <= min + eps` fires on **every** input → every conversion returns −10000.0 → **every engine buffer pinned at DSBVOLUME_MIN**. The intro video is immune (separate path). Field signature matches defect 12 exactly.
- **Provenance (verified via git)**: `720p` HAS the decoded constants (8 sites); upstream `pr4/sound-except-intro` HAS them (PR #10 commit 2); main's last change to the file is `d6affdd "merge port"` — **the fix did not survive the merge**. First confirmed casualty of the owner's "fixes not re-litigated" caveat; precedent is H17 (ad964f3 superseded 6 D3D/glide hand fixes).
- Why no gate caught it: the 183-row gate suite covers site-5/6/7 UI+glide and `d2boot-fix:` rows but has **zero sound rows**. Add sound gate rows when the fix lands (§5 rule).

### 3.2 What DID survive the merge (do not redo)

- **fix 16** — `D2Client/GAME/Sound.cpp:523–539` reads `SOUNDSMP_CalculateVolumeAndPan` out params as integers (`[vanilla-plus fix 16]` comment in-tree; H3 Defect 13, field-verified "The sounds works now. NPC's Monsters, Player. All working.").
- **the hwnd fix** — `Application.cpp:170–179` passes the real window handle (`[sound fix]` comment in-tree).
- **d2boot sound marks** — `Application.cpp:175–177`: `SOUND_Initialize enter (hwnd=...)` / `SOUND_Initialize done (dsoundInit=%d eax=%d)`.

### 3.3 The fix (verbatim spec from H3 Defect 12 / upstream PR #10 commit 2) — **LANDED rev41, FIELD-VERIFIED rev42** (code identical to pr4-fixed; curve harness ALL PASS; gates 188/188; owner: "Sound works.")

In `D2Sound/Src/D2SoundSmp.cpp`, decode five literals and restore the scale — nothing else:

| line | now | becomes |
| --- | --- | --- |
| :769 | `(long double)(double)0x3f1a36e2e0000000` | `(long double)0.0001` |
| :774 | `(long double)(double)0x3f1a36e2e0000000` | `(long double)0.0001` |
| :785 | `(float)(fVar1 * (long double)(double)-0x3f60c00000000000)` | `(float)(fVar1 * -2000.0)` |
| :790 | `(long double)(double)0x3f1a36e2e0000000` | `(long double)0.0001` |
| :793 | `(long double)(double)-0x3f3c780000000000` | `(long double)-10000.0` |
| :797 | `(long double)(double)0x409f400000000000` | `(long double)2000.0` |

Curve of record (retail FUN_005165f0, user-supplied): `value ≤ min+0.0001 → −10000`; `value ≥ max−0.0001 → 0`; else `−2000·log10((max+min)/value)` centibels; `SOUNDSMP_ClampAttenuation` (10^(att/2000)) is its exact inverse. Verified: 255→0, 110→−730, 0→−10000. DirectSound: −10000 = absolute mute, 0 = full. Keep the comments self-contained (upstream-clean wording, the pr4 commit comments are the model). Overlay zip over main, CRLF byte-exact, `VP-BEGIN/VP-END` markers (upstream file), gates for the decoded tokens (§8).

### 3.4 Discriminator first — read d2boot.log before touching code

- `SOUND_Initialize done (dsoundInit=1 ...)` → engine alive → the ClampVolume pinning is fully consistent; land §3.3.
- `dsoundInit=0` → engine dead, different track: the game log then contains `DirectSound error: ...` lines (`SOUND_InitDirectSound` logs every failing call — DirectSoundCreate / SetCooperativeLevel / CreateSoundBuffer / GetCaps / SetFormat / EAX probe) naming the exact failing call; fix that instead.
- If after §3.3 music works but SFX stay silent → re-walk the fix-16 chain with the sounddiag marks **re-cut, not reinvented** (mark formats of record survive in HISTORY.md H4/sounddiag: `SetVolume#N: vol= glob= lin= att= buf= hr= from=`, `updchan:`, `calc:`, `fadein:`/`fadevol:`).

### 3.5 Do-not list

- **Do NOT re-audit the SMP layer** — H3 exoneration: halt IDs, struct offsets, float literals, master multiply, EAX property IDs all match retail ("Do not re-audit the SMP layer").
- **Do NOT touch `support/d2_loud.cpp` for sound issues** — it is the LOUD diagnostic build's bounds/guard/canary handlers, not audio (name trap).
- **Do NOT chase the SetPan right-branch negation** as a silence cause — carried watch item (H3): stereo position only.
- **Do NOT merge PR branches to "get the fix"** — owner: they are upstreamed; §3.3 above is the complete record.

## 4. Defect track B — Glide not booting

### 4.1 Architecture split (owner-corrected)

- **Game side (main)**: `D2Glide/` + `D2Client/Renderer/Glide.cpp` — loads glide3x.dll via `GetProcAddress("_grSstWinOpen@28", ...)` etc. Stays in main.
- **DLL side (branch `Glide3xVK`)**: the glide3x.dll wrapper — **all DLL work happens there**, including the resolution table.

### 4.2 Verified in-tree this round (main, game side)

- Site-7 intact: size table mode 4 = 1280x720 / mode 3 = dormant 1344x700 (`D2Glide/Renderer/Glide.cpp:132–133, 597–610`); `grSstWinOpen` resolution enums 16/17 (:605/:609); modes 2/3/4 take the perspective=1 init path (gates `resfix: glide*` green within 183/183).
- Wrapper contract on `Glide3xVK`: `src/g3x_types.h` `g3x_resolution_size` carries `{1280,720}`=16 and `{1344,700}`=17 **in-source** (the old `glide3xvk-720p-resolutions.patch` is absorbed — no standalone .patch file exists anymore); `src/glide_exports.cpp:130–131` honors enums ≤17. The soft-fail signature ("unknown resolution 16" in glide3x.log + 640x480 surface) belongs to wrapper builds **older** than that absorption.
- Zero renderer files changed between the "Glide works!" field confirmation (0.0.4 era, rev31/round-4, H25) and now — the failure mode this run is unknown (black screen? soft-fail? crash?).

### 4.3 Triage plan (next owner round)

1. Owner supplies: `glide3x.log` + `d2boot.log` from a `-3dfx` run, and which glide3x.dll build is deployed (date/provenance).
2. `unknown resolution 16` in glide3x.log → **stale DLL**: rebuild glide3x.dll from `Glide3xVK` HEAD and redeploy; game-side needs nothing.
3. DLL current and still failing → `d2boot.log` names the provider-init stage; a 12 s hangwatch dump names the wedged function (`d2exe.map` beside d2.exe for symbolization).
4. Field pass afterwards per H25 §6 glide bullets: world visible (not black) at the 1280x720 stop, fps overlay reads "Video: Glide", cutscenes resize 640x480 first (retail behavior), 800x600 stop byte-identical.

## 5. Re-litigation ledger — the individual fixes on main

- **Status after the 0.0.6 field round**: the sound trio (defect-12 decode, fix 16, hwnd fix) is **field-verified** on main's build; `D2SoundSmp.cpp` was the first proven drop (healed rev41) and its gate rows now exist. Still unverified: the site-5/6/7 + d2boot-fix surfaces beyond the rev30/32 confirmations (§7 pass), and tracks C/D (§9) — which this round's diffs show are NEW findings, not drops.
- **Method**: for each fix file in the ledger (H21 rows 1–27 + the 720p-era rounds archived in H25), diff main against the ref that carried the fix: `720p` for the site/gold/roster-era work, `origin/pr4/sound-except-intro` for the sound trio (other PR branches are upstream copies of their series). Watch for H17-style supersession ("upstream's re-typed tree supersedes N hand fixes").
- **Standing rule going forward**: every fix that lands on main adds a gate row (grep for the fixed token) so a future merge cannot silently drop it. The sound constant gates (§3.3) were added in rev41 — 5 rows, suite 188 (incl. an absence gate pinning the raw literals out).
- **Field re-pass**: §7 checklist on main's build, D3D first, then glide.

## 6. Standing watches (carry-overs, unchanged)

- `roster heal:` in d2boot.log → upstream roster corruptor alive (rev35/36 stack; context H25).
- `s2c dispatch guard:` / repeating `s2c pump: drained` → packet-path pathology (rev34 hardening; H25 §8).
- hangwatch dumps at 12 s stalls; keep `d2exe.map` beside d2.exe for symbolization.
- d2boot.log first mark must read the current self-ID string — pins the exe before any analysis.
- SetPan right-branch negation deviation (stereo position only; H3 watch item).

## 7. Field checklist (re-litigation pass on main)

- **Pending from the 720p era (H25 §6)**: waypoint / quest log / party / help deep-pass; sibling dialogs (trade gold, cube UI, NPC menus, EscMenu); in-game 800↔720 switch (coord-cache refetch); 800x600 byte-parity + 640x480 untouched.
- **Sound — title music + in-game audio field-VERIFIED rev42 ("Sound works.")**; keep the fine-grain pass as regression watch: UI clicks, NPC speech, footsteps, quest fanfares, pickup-fail voice, music transitions, volume-slider behavior.
- **Glide**: §4.3 step 4 (known issue #1).
- **Track C (§9.1)**: unique/champion/minion colors vs retail on the same pack.
- **Track D (§9.2)**: 720p screenshot naming the misaligned inventory sub-panel; 800x600 parity unchanged.

## 8. Conventions (condensed, unchanged)

- **Overlay workflow**: sandbox edits the tree → overlay **zip of changed files** (owner preference, not .patch) → owner applies as own commits → llvm-mingw i686 Win11 build (`tools/build-local-win11.cmd`; MSYS2 MINGW32 `cmake --build build -j 8`, ~501 targets). The full build is the owner's gate.
- **VP-BEGIN/VP-END markers** on every fork edit in upstream files; fork-only TUs (`VanillaPlusCore.{h,cpp}` at `D2Client/UI/`) keep upstream files free of new code.
- Every source change updates the `tools/check-vanilla-plus.sh` gate table. (The owner-side `tools/check-fork-surface.sh --update` step is RETIRED rev43 — the fork-surface budget/allowlist was deleted; main never sends PRs, clean branches off main carry PR hygiene.)
- Line endings: **merged main is LF-normalized** (rev41 finding; only overlay-era leftovers such as `D2Net/SRC/Client.cpp` are CRLF) — patchers detect and preserve each file's ending, never mix.
- **Version self-ID** (`Application.cpp:66`) bumps every code round so d2boot.log pins the build — the **0.0.6 cut ships as `vanilla-plus 0.0.6 (rev41)`** (field-confirmed); the docs-only cut round does NOT bump it, and the next code round takes 0.0.7 + the next rev number (final naming owner's call).
- Sandbox: **no mingw toolchain, no apt** — constants-level fixes are verified by diff review + curve math; the owner's local build is the compile gate.
- Diagnostic channels: `d2boot.log` (`[d2boot]` marks, permanent), hangwatch stall dumps, the game log (`DirectSound error:` lines), `glide3x.log` (wrapper side), `d2loud-*.txt` (LOUD builds only). Sandbox tooling of record: `scripts/rev41_sound_fp_decode.py` (ending-aware patcher), `scripts/rev41_curve_check.py` (volume-curve harness).
- Full history: HISTORY.md H1–H24 (pre-720p) and **H25** (the 720p era, incl. the freeze-era reading guide referenced by the `d2boot:` gate comments).

## 9. Known-issue manifest at the 0.0.6 cut (owner, 2026-10-31)

| # | Owner symptom (verbatim) | Track |
| --- | --- | --- |
| 1 | "No Glide" | B (§4) — UPDATED rev44: glide BOOTS (0.4.6), first-frame own-code int div-by-zero; crash report pending |
| 2 | "Unique Monsters are not Unique Colors" | C (§9.1) |
| 3 | "Minion Skeletons are Gold" | C (§9.1) |
| 4 | "(720p) Inventory not aligned" | D (§9.2) — **FIELD-VERIFIED R45 (owner: "Inventory is aligned now") — CLOSED** |
| 5 | "skills button in hotbar — the menu that pops up is on the wrong side of the screen" | F (§9.3) — **FIELD-VERIFIED R46 (owner: "Skills Popout is fixed") — CLOSED** |

### 9.1 Track C — unique monsters uncolored / minion skeletons gold

- **Not a dropped fix** (verified rev42): main-vs-720p content-only diffs of `PalShift.cpp` / `Pallete.cpp` / `MonUnique.cpp` are decompiler-noise (guard inversions, `n > 0` ↔ `0 < n`, include order); no color fix exists anywhere in the H21 ledger. This is first-time work on upstream behavior.
- **Mechanism map (where retail colors live)**:
  - Static per-class palettes: `D2Client/UNIT/Monster.cpp:1238–1298` — default `TransLvl+2` (clamped <8), Andariel/Duriel (`0x9c`/`0xd3`) take difficulty palettes 3/4, `0x14d` → 5; structurally sane on main, **not** the suspect.
  - Random unique transforms (the gold/silver/red rolls): `PALSHIFT_GetMonsterTransform` (`D2Client/GAME/PalShift.cpp:307`) — note the `case 0/1: nClassId - 0x16bU` early-out and the `nPalShiftIndex - 8U` bound over 0x1d entries; `PALSHIFT_LoadMonsterPalShift` (:209) reads palshift.txt rows by monster code; `PALSHIFT_InitPalShiftTable` (:275).
  - Draw-time application: `D2GFX_DrawShiftedImage(..., GLOBALPALLETE)` (`D2gfx/D2GFX.cpp:972`) → renderer `nfpDrawShiftedImage`; unit→index plumbing via `CUnit::SetPalShiftIndex`/getter (`D2Client/UNIT/CUnit.cpp`).
  - Unique seed → transform roll: `MonUnique.cpp` (`sNewSeed.nSeedLow % 100`); master→minion inheritance is the shared mechanism behind symptoms #2 and #3 — if the master's roll is lost but minions read a default, you get exactly "uniques plain, minions gold".
- **Pattern to hunt first (defect-12 precedent)**: a Ghidra transcription artifact pinning the transform index to one constant — "everything gold" is what a pinned index looks like. Audit the index math as constants/int-casts before touching logic.
- **Cheap first data ask**: one 800x600 D3D screenshot of a unique pack + whether ALL uniques are affected or act/class-specific; note whether champion (blue/orange) naming still works (same pack-generation path).
- **When the track opens**: sweep the PR branches (`jaenster-main`, `pr2`) for color-related fixes before writing anything; every fix lands with a gate row (§5 rule).

### 9.2 Track D — (720p) inventory misaligned — **FIELD-VERIFIED R45 — CLOSED**

- **Root cause (rev44)**: the fec99ff merge port placed every `VP_SHIFT_*` call BEFORE its paired `Get*Dimensions` fetch in `InitializeiInventoryPages` (and the `init_npc_coords` NPC block). The fetchers are pure overwrites (`TokTbls.cpp:806–894` — every field assigned from the inventory.txt row), so the shift landed on stale data and was clobbered: **all 40 table-rect sets stayed vanilla-800x600-absolute**. The panel art is drawn bottom-anchored by separate ui.cpp machinery, so the art moved (+240,+120 at 1280x720) while items, body slots, and hit-rects did not — measured off the owner's 720p screenshot: art grid cells at client x 657..947 (= 417+240, exact), HUD + orbs correctly shifted, potions drawn at vanilla-screen grid cols 8-9 row 1 over the paperdoll (240 left / 120 up of their cells).
- **Provenance**: `git diff 720p main --ignore-cr-at-eol` shows the flip pair-by-pair (720p = fetch→shift). The rev42 "site-5/6 machinery intact and identical" verdict examined the shift function bodies, not the call order — corrected here. (Also corrects this section's earlier "240/60" figure: the wide-UI Y offset at 720p is **120** = `ScreenSizeY-600`.)
- **Fix**: `scripts/rev44_inv_shift_order.py` — swaps all 40 pairs to fetch→shift; asserts counts (43 shift / 40 fetch / 40 calls) and per-pair target match. Gate: `site-5: fetch->shift pair order` (awk order check, ok=40/bad=0).
- **Field-verify ask**: 720p, open inventory with items in the grid — items must sit in their art cells; clicks land on the cells (hit-rects share the same structs); equipped items sit on their body-slot recesses. Same for vendor/stash/cube/trade pages if exercised.
- **R45**: owner confirmed — *"Inventory is aligned now"*. Track D CLOSED.

### 9.3 Track F — skills flyout opens on the wrong side of the screen — **FIELD-VERIFIED R46 — CLOSED**

- **Owner symptom (R45, verbatim)**: *"When I click the skills button in hotbar the menu the pops up is on the wrong side of the screen"* — reported right after the track-D field confirmation, i.e. at 1280x720 on the R44 build.
- **Root cause (R45)**: same class as track D — merge casualty at the call sites, half-applied flip. `SPELLSEL_RenderAllSkillGrids` (`D2Client/UI/SpellSel.cpp`) draws each flyout in an icon pass (which also registers the click regions via `SPELLSEL_RegisterSkillSlot`) and a tooltip pass. The Ghidra decomp-regen commits (`a91f7ac`/`aa93bfc`, post-site-5) scrambled the two icon-pass X anchors while both tooltip passes kept the correct wide-mode anchors:
  - right-hand grid icons: drew at the swap grid's anchor `0x50 + VPRES_GetWideUiOffsetX()` (x 320 at 720p — LEFT half) instead of `ScreenSizeX - VPRES_GetWideUiOffsetX() - 0x80` (x 912 — above the hotbar);
  - swap-grid icons: drew at vanilla `0x50` (x 80, off the centered panel) instead of `0x50 + offset` (x 320);
  - the `nStartX` store feeding the right grid had lost its offset term (dead store — the regen also swapped the if/else bodies and flipped `bIsRightHand`, so the correct anchors were computed but never consumed).
  - Net field signature: icons + click regions on the left, tooltips on the right — the flyout "pops up on the wrong side".
- **Fix (R45)**: icon passes re-paired with their tooltip passes; `nStartX` offset restored. Anchor expressions are byte-identical to the last field-verified wide-mode state of this function (`git show 13aa82e~1`, pre-decomp-regen). Retail no-op: `VPRES_GetWideUiOffsetX()` = 0 at 640x480/800x600. Y side untouched (`ScreenSizeY - 0x56` already bottom-anchored). Click handler `INPUT_SpellSelectClick` is region-driven — hit-testing follows automatically.
- **Gates (R45)**: spell right-grid icons bar-anchored (`nStartX` consumers = 2), swap-grid icons ride panel (`0x50 + offset` = 2), absence gate on the plain-`0x50` icon anchor, spell-grid offset count re-pinned 4→5. Suite: 194/194.
- **Field-verify ask**: 720p — click BOTH hotbar skill buttons: each flyout must hug the control panel above the skill buttons (icons, tooltips and clicks on the same cells); swap the left/right bindings and re-check both grids. 800x600: unchanged behavior.
- **R46**: owner confirmed — *"Skills Popout is fixed"*; the R45 fix shipped in release tag `D2LoD-Vanilla-Plus-0.0.6` (`0eb2655`). Track F CLOSED.
