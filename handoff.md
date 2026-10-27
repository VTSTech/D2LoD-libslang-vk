# vanilla-plus — handoff for future work

*Written 2026-10-26 at the close of the glide-renderer defect series (fixes 1–13, PR #6 at 10 commits, all investigations closed and field-verified). Audience: the next engineer or agent picking up vanilla-plus work with zero conversation context. The master journal behind this doc is `/home/z/my-project/worklog.md` (rev-1 Tasks 1–22 were lost in the 2026-10-26 sandbox reset; the journal was re-initialized — its Task 1 is the remote-state verification this rev is based on); when this doc and the worklog disagree, the worklog wins.*

*Rev 2 (2026-10-26, later same day): §2 rewritten for the remote-repo state — the fix chain now lives on the public remote and the full fix-chain hash map is recorded; PR #6's verified live state moved to §9.1 (the updated body was never applied); §10 is now a build spec (prior tooling lost); §12's grep gates corrected against the actual in-tree marker text; §13 refreshed for the clean re-cloned sandbox. This file is committed in-tree at the tip (`03705cf`).*
*Rev 3 (2026-10-26, later still): upstream/main absorbed the PR #6/#8 content as the eef5f2c regeneration ("Monsters drop items again, Act 5 snow no longer hangs, Glide fixes", closes #6/#8, co-authored) and it was merged into vanilla-plus at cf36935. Resolution: D2Glide/*, Drop.cpp and Storm/_unnamespaced.cpp took upstream's canonical implementations of the same defects (field-verified on jaenster-main); Bnclient.cpp kept the d2boot-instrumented v5 chain; the globals [257] hunks kept our marker comments. The [glide fix 10] / [vanilla-plus fix 13] marker gates inside D2Glide/Renderer/Glide.cpp are RETIRED — upstream's file replaces those sites (perspective size table, struct-typed gGlideVertex0/D2GlideGroundVertexCursorStrc). The ShadowDiag OnShadowQuad hook was re-inserted into upstream's DrawShadowQuad (NoteWorldFrame + include survived the auto-merge); LightDiag and DropDiag hooks untouched. The falloff [257] gate in globals/globals.h still greps clean. The D2MEM_CopyRows char* link fix (2612a51) is superseded by upstream's own version of the same fix in eef5f2c.*
*Rev 4 (2026-10-26): the merge's CI fallout is fixed and the delivery model changed. The first post-merge push (0fa2003) failed check-build with 18 compile errors — four roots, all "re-inserted hooks referenced pre-merge names": Drop.cpp's redundant fix-14 tail (deleted; upstream's pSpawnRoomFound-after-memset IS canonical fix 14), Glide.cpp's fix-2 stale-slot validation + fix-3 guard/decrement (ported to upstream's gGlideVertex0.aTextureCache[] holder with the raw 1-based class; node+8 backpointer / node+0xc mode offsets verified against upstream's upload writes (- 0x14/- 0x10) and eviction reads in glTextureCache.cpp:258-261, so fix-2 semantics are unchanged; the guard+decrement is retired — the zero-and-recompute above it already pins the class to 1..3), glBlocks.cpp's fix-7 capture (upstream's re-typed miss path is canonical: nNodeIdx0/nTexCacheIdx), and ShadowDiag.cpp's incomplete D2GfxDataStrc (upstream moved the struct to D2Client/D2GfxDataStrc.h — included directly now). Fixed as 45c6c2d, scratch-verified; CI run in progress at write time. Fixes now travel as GIT BUNDLES (§7bis) since the sandbox still cannot push. Sibling line: jaenster-main got its own CI split (d2-<sha> fat + d2-stripped-<sha>, 9e0ae75) and the CopyRows fix + MANIFEST removal landed there as b2411ec.*
*Rev 5 (2026-10-27): the sound-defect series (§3bis is new). CI is RETIRED — chronic toolchain-install timeouts, a snapshot-corruption failure and 15-minute step caps made it unusable after the merge era; the user reverted build.yml to the last working version (5931e15) and all builds now run locally on their Win11 box (`tools/build-local-win11.ps1`, consolidated by fix 15). Delivery route changed to single-purpose overlay zips of only the files actually changed, uploaded to tmpfiles.org with tokenized `/dl/` links + byte verification (§7ter). Two sound defects: (12, CLOSED) the title/in-game music mute = Ghidra IEEE-754 bit-pattern constants transcribed as integer literals in D2SoundSmp.cpp, pinning every DDA buffer at DSBVOLUME_MIN — fixed d86ba98 = user's 4031fa7, field-verified; (13, OPEN) NPC speech + monster/player SFX silent — the client-side volume chain feeds SetVolume ≤ 0, the SMP layer is EXONERATED against the user's retail decompiles of all four FUN_005165f0 callers, and round-7 probes (dc1d051, sandbox-local) await the user's runtime log. sounddiag stays in-tree until defect 13 closes, then retires. `-3dfx` boot halt (registry "Contrast" out of [0,100] → FOG halt via SetGamma) closed in fix 15 with the sound-hwnd fix.*

---

## 1. What this project is

Vanilla-plus is the **game-source line** of VTSTech/D2LoD-libslang-vk: the recovered Diablo II 1.14d source rebuilt with gcc (`i686-w64-mingw32`, `-O0` + DWARF fat exe — both intentional, see the branch charter `VANILLA-PLUS.md` in the repo). The defect series this handoff closes was one thing: **the glide renderer (`D2Glide`) and parts of the client, decompiler-reconstructed, lost facts the original binary encoded implicitly**, and those losses only became visible once the code was relinked by a different compiler.

Stack topology — keep this straight, it cost us one reversed A/B attribution (Task 18):

| Component | What it is | Role in debugging |
|---|---|---|
| `Game.exe` | Blizzard **1.14d retail, original** | **Source of truth.** Capture reference for every A/B. |
| `d2.exe` / `d2s.exe` | the gcc rebuild (d2s = CI-stripped artifact) | The thing being fixed. |
| `D2Glide.dll` | rebuilt glide renderer (in-tree `D2Glide/`) | Main battlefield of fixes 1–13. |
| `glide3xvk` wrapper | logging glide3x ICD (0.4.5, work/wrapper/glide3xvk; module 535f2aa) | The measurement instrument: budget-limited dumps of `parr#` (ground strips), `vc0#` (shadow-class quads), `drawshape#`/`fan#` (menu quads). Vertex color bytes 0–2 = RGB (R = LSB), byte 3 alpha unused for opaque; `grConstantColorValue` = `0xRRGGBBAA`. |
| CI | GitHub Actions on vanilla-plus | **RETIRED (rev 5)** — chronic toolchain-install failures killed it (§2); builds are local on the user's Win11 box via `tools/build-local-win11.ps1`. (Historically it shipped `d2s-<sha>.exe` + `d2-map-<sha>.map`; the fat local build still produces the map for crash symbolization.) |

Root pattern behind all eleven PR defects: labels adjacent in the original `.data`/`.bss` become unrelated objects after relink; guessed array extents come back one short; live return values are dropped; a color word written through a float-typed alias converts instead of storing bits. **Any future "works in retail, wrong in rebuild" report should start from this pattern** — audit what the decompiler had to guess, not what the code says.

## 2. Branch / commit topology

- **The authoritative tree is now the public remote**: `origin = https://github.com/VTSTech/D2LoD-libslang-vk` (public repo, default branch `main`). **`vanilla-plus` carries the entire fix chain plus this file**; tip at rev-4 time = `45c6c2d` ("fix: port fix-2/3/7/14 leftovers to upstream's re-typed tree"); tip at rev-5 time = **origin `4031fa7`** ("R6 fix (title-music root cause)", 2026-10-26), sandbox local `dc1d051`. A fresh clone is clean and complete — verified 2026-10-26: `[glide fix 9]` falloff `[257]` at `globals/globals.h:3075`, the fix-2 stale-slot validation ported block at `D2Glide/Renderer/Glide.cpp:1071-1098`, ShadowDiag + LightDiag v2 modules in tree, `Glide.cpp` = 141 830 B (upstream's re-typing shrank it from the rev-2 165 953 B).
- Remote branches / tags (verified against origin 2026-10-26):
  - `vanilla-plus` = `4031fa7` (rev 5) — the game-source line; everything below hangs off it. Sandbox local sits at `dc1d051`: its `d86ba98` is a **content-twin** of origin's `4031fa7` (`git diff d86ba98 4031fa7` is empty — the user committed the same fix themselves), plus diag-only `dc1d051`. Reconcile by cherry-pick/diff of `dc1d051` only — a merge would drag in the duplicate twin.
  - `main` = `fd236ce` — pre-fix-chain baseline, ancestor of vanilla-plus.
  - `upstream/main` (jaenster/redivivus) = `eef5f2c` — the regeneration; merged into vanilla-plus at `cf36935`.
  - `jaenster-main` = `b2411ec` — clean upstream-tracking line with its own CI artifact split (d2 fat + d2-stripped); the CopyRows link fix + MANIFEST.txt removal landed there as b2411ec.
  - `pr2/glide-renderer-fixes` = `31acdac` ("vanilla-plus fix 13") — in-repo mirror of the PR #6 line; its own revised text (drift note below).
  - `libslang-vk` = `3a52553` (tip titled `0.4.5`) — the **glide3xvk wrapper line** (§1's logging ICD; module `535f2aa` is its parent). Tags `0.3.1`, `0.4.2`–`0.4.4` sit on this lineage.
  - `PR1-Bnclient-signer-verify-crash-fix` = `449d0a2` — early crash-triage-era side branch off `1b24c75` (tag `0.4.1`, a shared early commit); **not merged** into vanilla-plus.
- **Full fix-chain hash map** — previously only in the user's local `git log`, now durable (vanilla-plus first-parent, all 2026-10-25):

  | fix | commit | subject | note |
  |---|---|---|---|
  | 1 | `fdfdd17` | vanilla-plus fix | texture-cache phantom structs |
  | 2 | `870031f` | vanilla-plus fix 2 | 1-based size class |
  | 3 | `179dcf9` | vanilla-plus fix 3 | EnsureCapacity result |
  | 4 | `6878d9d` | vanilla-plus fix 4 | stale sprite-header slots |
  | 5 | `d4deaa3` + `9884942` | vanilla-plus fix 5 (two commits) | scattered fan vertices |
  | 6 | `fa1fa3a` | vanilla-plus fix 6 | fan slot order + portrait spans |
  | 7 | `77fd651` | glide fix 7 | wall/shadow tile fans |
  | — | `b4c705e` | Update build.yml | CI tweak, no code |
  | 8 | `c2512b2` | glide fix 8 | LightDiag v1 (diagnostic) |
  | 9 | `642711a` | glide fix 9 | falloff `[257]` — CI build `d2s-642711a`; **reference base for fix-10-era forensics** (Glide.cpp = 163 371 B here) |
  | 10 | `6feb86f` | glide fix 10 | ground-argb u32 stores |
  | 11 | `b478f2b` | glide fix 11 | ShadowDiag (diagnostic) |
  | 12 | `7e07380` | restore glide fix 10 | fix-11 drop-in regression restore |
  | 13s | `440c5f8` | vanilla-plus fix 13 | shadow spans — CI build `d2s-440c5f8`, field-verified |
  | 13l | `1a180d1` | glide fix 13 | LightDiag v2 (diagnostic) |
  | — | `03705cf` | Create handoff.md | this file (rev 1/2) |
  | — | `2612a51` | fix: D2MEM_CopyRows definition must match its char* declaration (link) | link fix, vanilla-plus; superseded by upstream's own version inside eef5f2c |
  | — | `cf36935` | Merge upstream/main (eef5f2c) | the 424-file regeneration merge; resolution policy in the commit message |
  | — | `0fa2003` | docs: handoff rev 3 | this file, rev 3 |
  | — | `45c6c2d` | fix: port fix-2/3/7/14 leftovers to upstream's re-typed tree | CI fallout of the merge (18 errors) — rev 4 |
  | — | `b2411ec` | fix (jaenster-main) | sibling line: CopyRows fix + MANIFEST.txt removal |
  | — | `d217e04` | docs | handoff rev 4 + PLAN/README/VANILLA-PLUS refresh |
  | — | `c2562da`…`5931e15` | Update build.yml (×5) | CI hardening churn → CI retired (rev 5); user reverted to the last working build.yml |
  | — | `eb16281` / `145a428` | sounddiag (v1/v2) | client sound instrumentation begins |
  | — | `0856cb9` / `2520c21` / `c0c2d6f` | build files | CMake / d2_platform.h tweaks |
  | — | `0a4af03` | sounddiag 3 | round-3 DDA-pipeline probes |
  | — | `91f28f3` | cmake | build ID: `git describe` → `rev-parse` (bare `d2s-<hash>`) |
  | 15 | `04cea73` | vanilla-plus fix 15 | sound hwnd (Application.cpp GetHwnd) + `-3dfx` contrast clamp (Glide.cpp SetGamma) + build-script consolidation (`tools/build-local-win11.ps1`) |
  | — | `fdcf198` | sounddiag v4 | round-4 probes |
  | — | `fcd50d8` | sounddiag v5 | SetVolume full-call logger + GetVolume readbacks |
  | — | `4031fa7` | R6 fix (title-music root cause) | = sandbox `d86ba98`; FP bit-pattern constants (§3bis defect 12) |
  | — | `dc1d051` | sounddiag round 7 | **sandbox-local only**: unit-sound volume-chain probes (§3bis defect 13) |

  (Rows fixes 1–13l date from 2026-10-25 on the pre-merge first-parent chain; the dated rows below them are the 2026-10-26/27 merge era. The PR #6 "commit N" numbering for this table lives in the collision table below.)
- **CI RETIRED (rev 5).** History: red on `0fa2003` (the merge fallout, fixed in `45c6c2d`), then the hardening series (`c2562da`…`5931e15`) fought 15-minute apt step caps, toolchain-path failures and a corrupted snapshot — and lost. The user reverted `build.yml` to the last working version (`5931e15`) and parked CI; the workflow file is historical, not load-bearing. **All builds run locally on the user's Win11 box via `tools/build-local-win11.ps1`** (a copy ships as `download/win11-build.ps1`).
- **Sandbox reset (2026-10-26, after rev 1 was written)**: the prior session's local artifacts are gone — worklog Tasks 1–22, `scripts/` overlay builders + diag decoders, `download/pr6-updated-body.md`, `work/` forensic areas, `upload/` zips. The worklog was re-initialized; §10's tooling table is now a *build spec*, not an inventory. Rev-1's forensics-copy notes (HEAD `c607f7f`, ~1137 dirty files) are historical only — `c607f7f` is simply the last pre-fix-chain commit (the bundle-merge point).
- **`pr2/glide-renderer-fixes` / PR #6** (on `jaenster/redivivus`): verified live state in §9.1. Key facts: PR head = `c65fa93` = the pr2-line `vanilla-plus fix 6` commit; **the live PR body is still the original five-defect text** — the finalized 11-defect body was never applied, and its source file was lost in the sandbox reset (§9.1).
- Note: pr2's Glide.cpp text has **drifted from vanilla-plus's** (jaenster revision: reworded comments, ~2441 lines). Overlay scripts therefore assert *per-anchor* uniqueness, never whole-file equality, across the two lines.

### Naming collision — read before confusing yourself

Three numbering systems are in circulation:

| "fix N" (overlay/fix-chain) | PR #6 "commit N" | defect (PR body) | What it is |
|---|---|---|---|
| fixes 1–7 | commits 1–4 (batch, 6 files, +466/−79) | 1–8 | texture cache (1–4), fan vertices (5–7), subtile halt (8) |
| fix 8 | commit 5 | — (diagnostic) | LightDiag v1, `d2light-diag.txt` at world entry |
| fix 9 | commit 6 | 9 | falloff table `[256]`→`[257]` |
| fix 10 | commit 7 | 10 | ground-argb u32 stores (+21/−9) |
| fix 11 | commit 8 | — (diagnostic) | ShadowDiag, `d2shadow-diag.txt` (first 24 shadow quads) |
| fix 12 | *no PR change* | — | vanilla-plus-only restore of fix-10 delta (fix-11 drop-in regression) |
| fix 13 (shadowspan) | commit 9 | 11 | DrawShadowQuad portrait spans (+29/−4) |
| fix 13 (lightcolordiag) | commit 10 | — (diagnostic) | LightDiag v2, `d2light-diag.txt` at in-world frame 90 |

Yes, "fix 13" names two different overlays. The zips are `pr2-glide-fix13-shadowspan-overlay.zip` and `pr2-glide-fix13-lightcolordiag-overlay.zip`.

## 3. The eleven defects (root causes, one line each)

Full detail with evidence in `pr6-updated-body.md` §"The eleven defects".

1. **Phantom texture-cache structs** — function-local statics `gGlideSmallTextureCacheBase`/`gnGlideState` were `.data` labels adjacent to `gGlideSmallTextureCache[3]`; rebuilt they're unrelated 4-byte objects → every upload addressed a phantom → `0xc1` halt (`-3dfx` launcher crash, issue #5).
2. **1-based size class as 0-based index** — class 3 indexed one `0x20` struct past `cache[3]`. Clamped + decremented.
3. **`GLTEXCACHE_EnsureCapacity` result discarded** — caller stored an uninitialized local as the 1-based node index. Now returns the index.
4. **Stale sprite-header slots splice free-list nodes onto the active list** — recycled headers inherited stale node indices; promote walked the node's own links without `nUsedCount++` → cache drained → `0xc1` with a clean-looking free list. Fixed by backpointer/mode validation.
5. **Scattered fan-vertex globals → giant clipped triangle** — one `4 × 0x1c` vertex block decompiled into ~20 globals; linker scatters them. All quad/tile submits now use contiguous local blocks with every field filled (the decompiled write-set had relied on `.data` residue for v1.x / v2.oow/s/t / v3.x/y).
6. **Fan slot order + portrait spans OOB** — decompiler names were offset one fan slot (hardware order `v0=TL, v1=BL, v2=BR, v3=TR`); portrait (negative-aspect) spans indexed backward out of `gaPerspectiveImageSizeTable` (neighbors in original `.data` made the reads land on usable values; rebuild reads `.bss` zeros). Negative classes compute spans from pow2 dims (`nTexW/H·2^(8−lod)`).
7. **Wall/shadow tile fans** — 8 scattered "vertex globals" (one held an X in a name ending `YMax`); `oow/s/t` presets were `InitPerspectiveTables` function-local statics, unreachable after rebuild; the four `ApplyColorLighting` writes landed on unknown `.bss` via `+8/+36/+64/+92` pointer arithmetic. Both `DrawWallTileSimple`/`DrawShadowTileSimple` now submit contiguous blocks with full-texture mapping.
8. **Wall-subtile cache halt** — `GLSUBTILE_EnsureCapacity` decompiled as `void`, dropping its live 1-based index; caller stored an unassigned local → first in-world wall draw hit `nNodeIdx0 < 0` → `ERROR_Halt 0x112` at end-of-loading. Restored the `short` return (mirror of `GLFLOOR_EnsureCapacity`).
9. **Lightmap falloff table truncated** — `gnLightmapInterpolationState` declared `[256]`, but `VIEW_Init` writes 2..0x100 and `AddLightToCell` reads `newIntensity+1` with saturation at 0xff → live index 0x100 → saturated cells multiply rgb by 0 → **black light cores / black unit silhouettes**. Two-line fix: `[257]`.
10. **Ground-tile vertex colors through a `(float)` cast** — `pfVertexSlot[7k+1] = (float)(...)` stores the *f32 bit pattern* of the intended PARGB (`0xff808080` → `0x4f7f8080`: red pinned to 0x7F, green←red, blue rounded). Invisible on uniform ambient gray; visible as light-radius/torch discoloration. Fix: 9 stores via `*(uint32_t*)&` (K ∈ {1,8,0xf,0x16,0x1d,0x24,0x2b,0x32,0x39}).
11. **Shadow-quad portrait spans** — `DrawShadowQuad` (which fix 6 left spelled out — its comment even flags it) read spans with the negative aspect → every portrait shadow submitted s=t=0 on all four vertices → **every unit shadow sampled texel (0,0)**. Fix: mirror fix 6's span block (portrait classes from pow2 dims, non-negative keep raw table reads). Caught by the ShadowDiag capture after static analysis *passed* the path — see §6 lesson.

## 3bis. The sound-defect series (2026-10-26/27, post-glide era)

Two defects after the eleven; retail 1.14d Game.exe's sound code (the user's Ghidra decompiles) is the reference throughout.

**Defect 12 — title/in-game music silent (CLOSED, field-verified).** Intro-video (Bink) audio was always audible — separate path. Every DDA/SFile music buffer sat at DSBVOLUME_MIN (-10000) from its first feed. Root cause: **Ghidra prints double constants as IEEE-754 bit patterns, and the port transcribed them as hex *integer* literals.** `0x3f1a36e2e0000000` is the double `0.0001` (the clamp epsilon — as an integer ≈7.3e19), `0x409f400000000000` is `2000.0` (the centibel scale); pasted as integers, `SOUNDSMP_ClampVolume`'s `value <= min + eps` fired on every input and every conversion returned -10000. The R5 scale fix (-2000.0) sat *behind* that clamp and never executed — which is why the fcd50d8/ce32a18 rounds changed nothing. Fixed five literals in `D2Sound/Src/D2SoundSmp.cpp` (two ClampVolume epsilons; ClampAttenuation epsilon, bottom bound, divisor — the old bottom bound `(double)-0x3f3c780000000000` actually decodes to -0.0004344, a third casualty; `-10000.0` is the retail curve's real bottom). Commit `d86ba98` = user's `4031fa7` "R6 fix". Field: "Music works now! Both at title and in game."

Retail volume-curve reference (user-supplied FUN_005165f0), now reproduced exactly: `value ≤ min+0.0001 → -10000`; `value ≥ max-0.0001 → 0`; else `-2000·log10((max+min)/value)` centibels. Verified: 255→0, 110→-730, 0→-10000; `SOUNDSMP_ClampAttenuation` (10^(att/2000)) is its exact inverse. DirectSound: volume -10000 = absolute mute, 0 = full; hr 0 = SetVolume succeeded.

> **Rule for every future port: a 16-hex-digit literal in Ghidra pseudocode is a double BIT PATTERN — decode it (`struct.unpack('<d', struct.pack('<Q', n))`), never paste it.** The `(double)0x<16hex>` idiom existed only in D2SoundSmp.cpp (grep-verified R6), but audit every newly recovered file the same way.

**Defect 13 — NPC speech + monster/player SFX silent (OPEN).** Post-fix symptom: "Still no NPC Speech or Monster/Player sounds." R6 log ground truth: Warriv speech **does** stream+play (`DATA\LOCAL\SFX\common\warriv\war_goodday.wav`, dda PLAY#3) but post-open GetVolume = -10000 → the **client-side** volume chain writes silence before `OpenAndStreamFile`. Music is unaffected (own volume source). Client chain: `SOUND_PlaySound` (D2Client/Sound/Sound.cpp:5441, request) → tick → `SOUND_PlaySoundWithPanning` (GAME/Sound.cpp:634) → `SOUND_UpdateChannelParams` (GAME/Sound.cpp:410, volume) → SetVolume/OpenAndStreamFile.

**The SMP layer is EXONERATED (2026-10-27).** User supplied retail decompiles of all four FUN_005165f0 callers — FUN_005157b0 SmpSetVolume, FUN_00515890 SmpSetPan, FUN_00515a90 SmpSetEaxObstruction, FUN_00515b40 SmpSetEaxRoomLevel — plus the complete call-site list (5 sites inside the same four functions). The port matches retail on every axis: halt IDs (0x2cb/0x2ef/0x345/0x359), `D2SoundSmpStrc` offsets (pDSBuffer@0x00, pEaxInterface@0x10, dwFlags byte@0x34 bit 2, nVolume@0x38, fEaxObstruction float@0x3c, size 0x60), float literals (0x437f0000=255.0f, 0x42fe0000=127.0f, 0x3f800000=1.0f), the master multiply `gnSoundGlobalVolume × vol / 255` with store-to-nVolume, the quality-flag==2 attenuation skip, and EAX property IDs 7 / 1 / 5+4 with 4-byte payloads. **Do not re-audit the SMP layer.**

Known open deviation (stereo position only, NOT a silence cause): our `SOUNDSMP_SetPan` right-branch negates (`SetPan((int)-fVar1)`); retail as-decompiled passes the raw (negative) conversion result to vtable+0x40. Retail's slot may be a wrapper Ghidra flattened — identify the retail owner before touching; do not "fix" blind.

R7 probes (sandbox `dc1d051`, delivered as overlay) discriminate the remaining suspects: `req:` SOUND_PlaySound request params; `updchan:` desc+0x1c/0x20/0x38 + falloff min/max + final volume; `playsfx loadfail|nochannel|wavfail` early exits; `fadein:`/`fadevol:` fade ramp. Candidate roots: desc+0x1c volume base (header says nPriority — field-mapping conflict), desc+0x20 read as float EAX obstruction (header says nUnitDistance int) → `SetEaxObstruction` → multiplied as (1-obstruction) inside SetVolume, callers passing nVolume=0, fade-in zeroing desc+0x1c before play with a stuck ramp. **Next action: obtain the R7 in-game log (the first paste arrived empty), read the marks, fix the client chain, then retire sounddiag (§8).**

## 4. Verification status (all green as of this handoff)

- Fixes 1–8: field-verified (launcher crash gone; title/select/loading render; in-world tiles, rain/fire/glow correct; end-of-loading halt gone).
- Fix 9: field-verified on `d2s-642711a` (silhouettes gone under night/storm).
- Fix 10 (+12 restore): field-verified (light radius / torch colors match retail).
- Fix 13 / commit 9: field-verified on `d2s-440c5f8` ("Shadows appear fixed"), and validated by the second `d2shadow-diag.txt` capture — 24/24 records reproduce the span law `pow2dim·2^(8−lod)`, and the wrapper `vc0#` shadow stream is byte-identical retail-vs-rebuild.
- LightDiag v2 capture (Task 22) decoded clean: reader chain == raw `objects.txt` bytes at 0x164..0x166 for 16/16 object lights (torch `ff/ec/b0`, blue `64/64/ff`); lightmap minimums R=fd G=f5 **B=d8** (no blue-zero anywhere); 16/16 unit samples round-trip packed-vs-cell, fix-8 signature `0xffffff80` reproducing. **The last open item (one gold-drop quad tinting `0xffc7b300` vs retail `0xffc7b377`, blue channel) is closed** as a cosmetic property of that sprite's own pass — the entire client lighting path is proven channel-complete. User's field words: "tbh the lights look fine."

## 5. Closed investigations — do not re-open without new evidence

- **Character-shadow artifacting** → defect 11 (closed via ShadowDiag capture, commit 9). Static audits had exonerated geometry/S/T/blend/decode-machinery/dispatch/cache — the capture caught what static analysis could not.
- **Gold-tint blue channel** → closed Task 22 (see §4). Glide side AND client lighting both exonerated by data.
- **Stale/zero mask content in shadow-diag captures** → capture-point artifact, explained: `UploadTextureToCache`'s cache-hit path early-outs *without* touching `gpGlideSpriteBuffer` (Glide.cpp ~1117–1123), so draw-time reads of the shared decode buffer show the previous sprite's content on cache hits. Not a render bug.
- **Wrapper `grConstantColorValue` garbage dwords** → memory-layout noise, never reach a draw, both sides.
- **Ground-strip A/B attribution reversal** (Task 18): retail `parr` = `0xff808080` raw; the `0x4f7f…` family = the *rebuild's*. Retail is the source of truth; defect-9's client-side evidence was independent of the earlier wrong attribution.
- **Group A/B split in shadow texinfo fields** (perspective-branch vs non-perspective-branch span fields) → both branches self-consistent, rects match retail; documented in verify_shadow_geom.py output, parked.

## 6. Methodology — what actually worked (keep doing this)

1. **Evidence-first per the charter**: never author a fix from static reading alone when a capture can pin the stage. The ShadowDiag capture is the flagship example: every static layer passed; the diag found the bug in one build. The LightDiag v1 capture turned "units render black, cause unknown" into a two-line fix.
2. **Retail A/B through the logging wrapper** — same wrapper build, same scene, diff the streams (`scripts/diff_glide_logs.py`, `cmp_log_colors.py`). Budget-limited dumps mean early-frame evidence only; pick captures accordingly.
3. **Byte-level forensics over narrative**: file sizes, `cmp`, f32-bit-pattern math (`f32_bits(f32(0xff808080)) = 0x4f7f8080` was the defect-10 smoking gun), crc groups, per-record asserts in a parser.
4. **One-shot client diagnostics with a decision tree shipped in the OVERLAY.md** — the dump should discriminate between candidate stages in a single build (LightDiag v2 was built exactly to split txt-parse / reader-chain / lightmap-accumulation).
5. **QA gauntlet for every overlay** (non-negotiable, see §7).

## 7. Delivery conventions (the overlay model)

Deliverable = zip in `/home/z/my-project/download/`, structure:

```
OVERLAY-PR2-FIXN.md      root cause, evidence table, self-diagnosis greps,
                         decision tree, branch matrix (vanilla-plus vs pr2)
patches/PATCH.diff       normalized prefixes (--- a/... +++ b/...), minimal hunks
drop-in-pr2/             full files for pr2 tip (fallback route)
drop-in-vanilla-plus/    full files for vanilla-plus tip (fallback route)
```

QA gauntlet (all must be green before shipping):
- `git apply --no-index` round-trip **byte-identical** on *every* supported base (use `GIT_CEILING_DIRECTORIES=/home/z/my-project`).
- **Negative test**: the patch must *refuse cleanly* on an already-fixed tree (`error: patch failed: <file>:<line>`).
- Unique-anchor asserts for every edit site, on every base.
- New C++ files: `g++ -fsyntax-only -Wall -Wextra` against stub headers.
- Builders are persisted scripts (`scripts/make_fixN_overlay.py`) — edit in place on failure, never regenerate (Script Persistence Rule).

**The fix-11 lesson, burned in:** the fix-11 zip's vanilla-plus drop-in was cut from the pre-fix-10 tip, so the user's whole-file copy reverted nine u32 stores and the light-radius discoloration returned in the next build (root-caused byte-level in Task 20; fixed by fix 12). Rules going forward:
- **Patch-first.** Full-file drop-ins are fallback only.
- Drop-ins must be cut **from the tip the user actually has** — verify with a base-state grep gate printed in the OVERLAY.md (e.g. `grep -c "glide fix 10" D2Glide/Renderer/Glide.cpp` → 0 = regressed, ≥1 = intact; same pattern for any future fix marker).
- Deltas touching the same file must be QA'd order-independent (fix-10 delta and fix-11 hooks touch disjoint lines; apply either order, byte-identical both ways).

### §7bis. Git bundles — the primary delivery route (rev 4)

The sandbox has no push credentials, but whole commits transfer cleanly as git bundles —
stronger than overlay zips (history, authorship, and the ability to fast-forward arrive
intact):

```
# sandbox side (after verifying local state and fetching origin):
git bundle create download/<repo>-vanilla-plus-<tip7>-bundle-<date>.bundle origin/vanilla-plus..vanilla-plus

# user side (on vanilla-plus):
git fetch <file>.bundle vanilla-plus && git merge --ff-only FETCH_HEAD && git push origin vanilla-plus
```

QA for a bundle (all green before shipping):
- `git bundle verify` in the sandbox clone AND in a scratch repo initialized at exactly
  the prerequisite tip (fetch that tip from the clone, never the branch head being delivered).
- In the scratch repo: `git fetch <bundle> vanilla-plus` + `git merge --ff-only FETCH_HEAD`
  must fast-forward cleanly, then content greps for every fix the bundle claims to carry.
- The bundle's prerequisite is the user's pushed tip — check `git rev-parse origin/vanilla-plus`
  AFTER a fresh `git fetch origin`, not from a stale remote-tracking ref (bit once: scratch
  was built at 9389e07 while the user was already at 0fa2003).
- Record bundle path + prerequisite + verification in the worklog.

Overlay zips (§7 above) remain the fallback for file-level application to a non-git tree.

### §7ter. tmpfiles.org overlay zips — the working delivery route (rev 5)

Since the CI retirement the loop is: **user builds locally** (win11-build.ps1), sandbox delivers **overlay zips of only the files actually changed** — never a stale whole-file overlay (the user's tree carries their own commits; cut every delivery from a fresh `git fetch origin`):

```
zip layout:  <changed files at repo-relative paths> + MANIFEST.txt
MANIFEST:    base commit the files were cut from, per-file sha256, base-state grep gates,
             apply/build steps, acceptance criteria
```

Upload to tmpfiles.org (**60-minute expiry**). The URL curl receives is an **HTML interstitial** — extract the tokenized `/dl/<token>` link embedded in that page, then **byte-verify**: `curl -o /tmp/f <dl-url> && cmp /tmp/f <local-file>` and record the sha256. Post the `/dl/` link (never the interstitial URL). Everything lives in `/home/z/my-project/download/` first.

Git bundles (§7bis) remain correct when history matters; the sound series' 1–2-file deltas all traveled as tmpfiles zips (`d86ba98-titlemusic-overlay.zip`, `dc1d051-sfxdiag-overlay.zip`, …).

## 8. Diagnostics on the tree (and their quirks)

- **ShadowDiag** (`D2Glide/Renderer/ShadowDiag.{h,cpp}` + 3 hooks in Glide.cpp: include, `DrawGroundTile` head `NoteWorldFrame()`, `DrawShadowQuad` submit-tail `OnShadowQuad`): arms at the first ground tile (in-world marker), captures the **first 24 in-world shadow quads**, writes `d2shadow-diag.txt` **once per session**. Record layout: unit/frame identity, DC6 block header, full `GlideTexInfoStrc` (nAspect@+0x34, fScale0/fTexS0/fTexT0/fTexS1@+0x20..+0x2c), mask crc32 + ASCII bitmap, 4 fan vertices (28-byte stride). Quirk: cache-hit records show stale mask crc/bitmap (see §5) — only first-decode records are trustworthy.
- **LightDiag v2** (`D2Client/Draw/LightDiag.{h,cpp}` + hooks in `LightMap.cpp` (AllocLightMap inputs), `D2Client.cpp:1802` (OBJECT_SetLightRGB with raw `objects.txt` row bytes 0x160..0x16F), `Draw.cpp`): dumps `d2light-diag.txt` **once at in-world frame 90** (v2 moved it from frame 2 so world-init object lights fit the budget). Decoder: `scripts/parse_light_diag.py` (asserts reader==txt, blue-zero scans, packed-vs-readback round-trip; exit 1 on any anomaly).
- **sounddiag (sound-series instrument, rev 5)** — `D2BOOT_MarkFmt("[sounddiag] …")` lines into `d2boot.log`. Generations: v1/v2 boot+device chain (eb16281/145a428) → v3 DDA pipeline (0a4af03) → v4 (fdcf198) → v5 SetVolume full-call logger + post-open/post-reset GetVolume readbacks (fcd50d8) → round 7 unit-sound volume-chain probes (dc1d051, sandbox-local). Marks of record: `SetVolume#N: vol= glob= lin= att= buf= hr= from=` (vol = channel volume 0–255, lin = master×vol/255, att = centibels handed to DirectSound), `feed:`, `global volume -> N`, `stream OPEN`, and R7's `req:` / `updchan:` / `playsfx loadfail|nochannel|wavfail` / `fadein:` / `fadevol:`. **Retirement promised once defect 13 closes** — strip every `[sounddiag]` mark plus the `support/d2_boot.h` includes that exist only for them, then re-grep the tree clean. The glide-era viddiag marks in Glide.cpp SetGamma (fix 15) follow the same fate.
- Both are zero-behavior-change when idle and cheap (one file write per session each). They have served their purpose; a future cleanup commit may strip or env-gate them — **offered to the user, not yet requested**. If you strip them, strip all hook lines *and* the modules, and keep the grep gates in the PR body accurate.
- CI linker maps (`d2-map-<sha>.map`) + `scripts/resolve_map*.py` = crash address → symbol (the fat-exe DWARF map survives strip; offsets stay valid).

## 9. Open / parked items (nothing urgent, nothing hidden)

1. **PR #6 merge watch — real work outstanding (verified 2026-10-26 via GitHub API).** State **open**, not merged; head `pr2/glide-renderer-fixes` @ `c65fa93` (the pr2-line fix-6 commit), base `main`, 3 commits / 3 files / +448/−76, `mergeable_state: unstable`, last updated 2026-10-25T20:26Z. **The live body is still the original five-defect text** — the finalized 11-defect/10-commit body was never applied, and `download/pr6-updated-body.md` was lost in the sandbox reset, so the body must be **regenerated first** (source material: §2's hash map + collision table, §3's eleven defects, §4 verification status) before `gh pr edit 6 --repo jaenster/redivivus --body-file ...` (no gh CLI/token in this sandbox — the user runs it or edits on the web). Also decide whether the PR head should be advanced past fix 6: fixes 7–13 exist on `vanilla-plus` and the pr2 mirror (`31acdac`) but not as PR-visible commits.
2. **Diag-hook cleanup commit** — see §8; pending user decision.
3. **Garbage-dword `grConstantColorValue` upper bytes** — memory-layout noise; ignore unless it ever reaches a draw.
4. **Second-order shadow-capture oddities** — same-frame different-crc decodes (class 36 frame 1/10 pairs), empty frame-0 decodes (class 37 frame 0 = 0/4096 vs frame 1 = 1051). Now interpretable with correct spans if anyone cares.
5. **Blood-pool saturation baseline** — parked from early triage, never picked up.
6. **Ground-tile v10 constants** (`gaGlideGroundTileVertices` `.data` item) — noted during fix-8 era, never needed.
7. **`Glide.cpp:1077` uninit-hardening note** — minor, optional.
8. **`nfpInitialize` FOG Error** — parked since crash-triage days.
9. **Wrapper `shdiag` A/B patch** — planned during fix-11, obsoleted by the client diag; only write it if a new shadow-class issue appears that client-side capture can't see.
10. **Defect 13 — SFX silence** (§3bis): blocked on the user's round-7 log; then the client-chain fix, then sounddiag retirement (§8).
11. **SetPan right-branch negation** (§3bis): needs the retail vtable+0x40 owner identified before any change; stereo position only, not audibility.
12. **Task C (gameplay, parked)**: Z-key camera zoom + Shift equipment-compare — user requested, awaiting their go once the sound series closes.

## 10. Tooling inventory (`/home/z/my-project/scripts/`)

| Script | Purpose |
|---|---|
| `make_fix{7..13}_overlay.py` | overlay builders (fix-13 is shadowspan; lightcolordiag builder is also `make_fix13_overlay.py` — second run documented in worklog Task 21) |
| `fix{10,11,12,13}_manifest.md` | OVERLAY.md text sources |
| `parse_shadow_diag.py`, `verify_shadow_geom.py` | d2shadow-diag decoders + span-law verifier |
| `parse_light_diag.py` | d2light-diag **v2** decoder with channel asserts (Task 22) |
| `diag_decode.py` | d2light-diag v1 decoder |
| `diff_glide_logs.py`, `cmp_log_colors.py` | wrapper log A/B |
| `analyze_lightradius_log.py` | `parr#` ground-strip color extraction |
| `vertex_scatter_audit.py` | defect-5 vertex-block audit |
| `resolve_map*.py` | CI linker-map symbolization |

**Rev-2 note:** every script above was lost in the 2026-10-26 sandbox reset — the table is now a *build spec* (recreate on demand per the Script Persistence Rule; the dump formats survive in §8 and in the in-tree modules `ShadowDiag.{h,cpp}` / `LightDiag.{h,cpp}`). Likewise `work/` (fix11regress, fix12, fix11diag, shadow-verif, light-verif, logdiff*, wrapper/…) and `upload/` start empty in a fresh sandbox; re-create extraction areas as needed.

## 11. Field-validation loop (how work gets done)

1. Author fix/diagnostic → overlay zip via the conventions in §7 → deliver from `download/`.
2. User applies on their Windows tree (`F:\GAMES\Diablo II`), commits `vanilla-plus fix N: ...`, pushes.
3. User builds locally on Win11 (`tools/build-local-win11.ps1`) — CI retired (§2).
4. User reproduces the scene through the logging wrapper — rebuild **and** retail, same scene — and/or captures diag dumps.
5. Uploads zips here → decode with §10 tooling → decide next step from data, not theory.
6. Update `pr6-updated-body.md` and append a worklog Task entry **every round** (the worklog is the institutional memory — this handoff is just its index).

## 12. Triage quick-paths (if a symptom returns)

| Symptom | First check |
|---|---|
| Black light cores / black unit silhouettes | falloff table still `[257]`? (`globals/globals.h:3075` — rev 4: line moved from 3094 by the eef5f2c merge) |
| Light radius / torch discoloration | (rev 4: the u32 stores are upstream's re-typed code now; our survivors in `D2Glide/Renderer/Glide.cpp`) `grep -c "vanilla-plus fix 3"` → 1 (zero-and-recompute at ~1042, guards the garbage size class) and `grep -c "vanilla-plus fix 2"` → 1 (ported stale-slot validation at ~1071-1098). Missing = a fix-11-style regression again (§7bis bundle or §7 patch route). |
| Every unit shadow is a solid blob / samples (0,0) | (rev 4: fix-13 marker retired — upstream's DrawShadowQuad owns the spans) gates: `grep -c "ShadowDiag" D2Glide/Renderer/Glide.cpp` → 3 (include + NoteWorldFrame + OnShadowQuad hook) and `grep -c "D2Client/D2GfxDataStrc.h" D2Glide/Renderer/ShadowDiag.cpp` → 1 (the 45c6c2d include — missing it is a compile error, not silent). Then a fresh `d2shadow-diag.txt` — aspect<0 records must show s 0..64/0..128, t 0..256 |
| Drops never broadcast / ground items vanish | `grep -c "pSpawnRoomFound" D2Game/ITEMS/Items/Drop.cpp` → 3 (upstream's canonical fix 14: found after FindBestSpotToSpawnItem, assigned to sItemGenCtx.pDrlgRoom AFTER the memset) + DropDiag hooks intact (include + `NoteRollLookup`/`NoteItemGen`/`NoteItemSpawnAudit`). A `pResolvedSpawnRoom` reference = the 0fa2003 regression (deleted in 45c6c2d). |
| `0xc1` halt at boot/menu | texture cache (defects 1–4); check promote bails in bootlog; rev-4 gate: the fix-2 validation block runs in the upload path (`if (nNewCacheNode)` at ~1092) — its absence reopens the stale-slot drain |
| Halt `0x112` at end-of-loading | wall-subtile cache capture (defect 8): `grep -c "nTexCacheIdx = (short int)nNodeIdx0" D2Glide/glBlocks.cpp` → 1 (upstream's canonical fix 7; a bare `nCacheSlot` there = the 0fa2003 regression) |
| Black-triangle grid on title screen | fan slot order (defect 6) — do *not* blame the wrapper (that attribution was retracted) |
| Foreign-module crash in `d2boot.log` | crash-triage stack (Task 2/2-b): VEH skip paths now dump module+offset; symbolize via CI map |
| New "retail right, rebuild wrong" color issue | start from the §1 root pattern; audit decompiler guesses around the site; then capture (§6.1) |
| Title/in-game music silent (again) | defect 12 relapse: `rg '\(double\)0x[0-9a-f]{16}' D2Sound/Src/D2SoundSmp.cpp` must be empty; read `SetVolume#N: att=` — att=-10000 while lin>0 = the clamp bug is back |
| NPC/monster/player SFX silent, music fine | defect 13 (§3bis): read `[sounddiag] req:/updchan:/fadein:` marks in d2boot.log — `updchan … vol<=0` = client volume chain; `playsfx loadfail/nochannel/wavfail` = early exit. SMP layer is exonerated — do not re-audit it |

## 13. Sandbox gotchas

- No `gh` CLI/token — PR body edits are handed to the user.
- `git apply` needs `--no-index` and `GIT_CEILING_DIRECTORIES=/home/z/my-project` (the clone has no clean parent boundary).
- Use `rg --hidden` for repo sweeps. The rev-2 clone is **clean** (`03705cf`, 0 dirty files) — a dirty worktree now means *new* local work: inspect it, don't blanket-revert.
- The sandbox resets between sessions (the 2026-10-26 reset lost the rev-1 worklog, scripts/, download zips, work/, upload/ — §2). Persist anything load-bearing in the repo itself; that is why this handoff is committed in-tree. (Rev 4: the re-initialized worklog now carries Tasks 1–6 — jaenster-main CI split, overlay zips, the CopyRows fix, the eef5f2c merge, first bundle delivery, and the merge-fallout fix 45c6c2d; scripts/ holds `make_overlay_zip.sh`, `verify_copyrows_fix.sh`, `resolve_merge.py`.)
- Script Persistence Rule: scripts >10 lines go to `scripts/` and get edited in place on failure.
- **No mingw toolchain in the sandbox and no apt rights** (installs fail/time out) — constants-level fixes are verified by diff review + curve math, not by compiling; the user's local Win11 build is the compile gate.
- Sound testing must run against a **1.14d** install (a mid-series 1.12A reinstall + registry clean reset the audio-slider registry values the code reads — `HKCU\Software\Blizzard Entertainment\Diablo II`; it also field-confirmed the `-3dfx` contrast-registry halt).
- tmpfiles.org links expire in **60 minutes**; always post the extracted `/dl/` token link and byte-verify (§7ter).
- User timezone America/Toronto; session language English; delivery zips always to `/home/z/my-project/download/`.
