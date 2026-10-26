# vanilla-plus — handoff for future work

*Written 2026-10-26 at the close of the glide-renderer defect series (fixes 1–13, PR #6 at 10 commits, all investigations closed and field-verified). Audience: the next engineer or agent picking up vanilla-plus work with zero conversation context. The master journal behind this doc is `/home/z/my-project/worklog.md` (rev-1 Tasks 1–22 were lost in the 2026-10-26 sandbox reset; the journal was re-initialized — its Task 1 is the remote-state verification this rev is based on); when this doc and the worklog disagree, the worklog wins.*

*Rev 2 (2026-10-26, later same day): §2 rewritten for the remote-repo state — the fix chain now lives on the public remote and the full fix-chain hash map is recorded; PR #6's verified live state moved to §9.1 (the updated body was never applied); §10 is now a build spec (prior tooling lost); §12's grep gates corrected against the actual in-tree marker text; §13 refreshed for the clean re-cloned sandbox. This file is committed in-tree at the tip (`03705cf`).*
*Rev 3 (2026-10-26, later still): upstream/main absorbed the PR #6/#8 content as the eef5f2c regeneration ("Monsters drop items again, Act 5 snow no longer hangs, Glide fixes", closes #6/#8, co-authored) and it was merged into vanilla-plus at cf36935. Resolution: D2Glide/*, Drop.cpp and Storm/_unnamespaced.cpp took upstream's canonical implementations of the same defects (field-verified on jaenster-main); Bnclient.cpp kept the d2boot-instrumented v5 chain; the globals [257] hunks kept our marker comments. The [glide fix 10] / [vanilla-plus fix 13] marker gates inside D2Glide/Renderer/Glide.cpp are RETIRED — upstream's file replaces those sites (perspective size table, struct-typed gGlideVertex0/D2GlideGroundVertexCursorStrc). The ShadowDiag OnShadowQuad hook was re-inserted into upstream's DrawShadowQuad (NoteWorldFrame + include survived the auto-merge); LightDiag and DropDiag hooks untouched. The falloff [257] gate in globals/globals.h still greps clean. The D2MEM_CopyRows char* link fix (2612a51) is superseded by upstream's own version of the same fix in eef5f2c.*

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
| CI | GitHub Actions on vanilla-plus | Ships `d2s-<sha>.exe` + `d2-map-<sha>.map` (linker map — load-bearing for crash symbolization). CI-first policy: never burn local cycles on i686 rebuilds. |

Root pattern behind all eleven PR defects: labels adjacent in the original `.data`/`.bss` become unrelated objects after relink; guessed array extents come back one short; live return values are dropped; a color word written through a float-typed alias converts instead of storing bits. **Any future "works in retail, wrong in rebuild" report should start from this pattern** — audit what the decompiler had to guess, not what the code says.

## 2. Branch / commit topology

- **The authoritative tree is now the public remote**: `origin = https://github.com/VTSTech/D2LoD-libslang-vk` (public repo, default branch `main`). **`vanilla-plus` carries the entire fix chain plus this file**; tip at rev-2 time = `03705cf` ("Create handoff.md"). A fresh clone is clean and complete — verified 2026-10-26: `[glide fix 9]` falloff `[257]` at `globals/globals.h:3094`, the fix-10 u32-store block at `D2Glide/Renderer/Glide.cpp:3522`, the `[vanilla-plus fix 13]` span block at `Glide.cpp:1221`, ShadowDiag + LightDiag v2 modules in tree, `Glide.cpp` = 165 953 B.
- Remote branches / tags (verified against origin 2026-10-26):
  - `vanilla-plus` = `03705cf` — the game-source line; everything below hangs off it.
  - `main` = `fd236ce` — pre-fix-chain baseline, ancestor of vanilla-plus.
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
  | — | `03705cf` | Create handoff.md | this file |

  (The PR #6 "commit N" numbering for this table lives in the collision table below.)
- CI (GitHub Actions `.github/workflows/build.yml`): green on `440c5f8` and `1a180d1` (runs completed 2026-10-26 UTC; 46 runs total at rev-2 time). No run observed for the docs-only `03705cf`. Still ships `d2s-<sha>.exe` + `d2-map-<sha>.map`.
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

## 8. Diagnostics on the tree (and their quirks)

- **ShadowDiag** (`D2Glide/Renderer/ShadowDiag.{h,cpp}` + 3 hooks in Glide.cpp: include, `DrawGroundTile` head `NoteWorldFrame()`, `DrawShadowQuad` submit-tail `OnShadowQuad`): arms at the first ground tile (in-world marker), captures the **first 24 in-world shadow quads**, writes `d2shadow-diag.txt` **once per session**. Record layout: unit/frame identity, DC6 block header, full `GlideTexInfoStrc` (nAspect@+0x34, fScale0/fTexS0/fTexT0/fTexS1@+0x20..+0x2c), mask crc32 + ASCII bitmap, 4 fan vertices (28-byte stride). Quirk: cache-hit records show stale mask crc/bitmap (see §5) — only first-decode records are trustworthy.
- **LightDiag v2** (`D2Client/Draw/LightDiag.{h,cpp}` + hooks in `LightMap.cpp` (AllocLightMap inputs), `D2Client.cpp:1802` (OBJECT_SetLightRGB with raw `objects.txt` row bytes 0x160..0x16F), `Draw.cpp`): dumps `d2light-diag.txt` **once at in-world frame 90** (v2 moved it from frame 2 so world-init object lights fit the budget). Decoder: `scripts/parse_light_diag.py` (asserts reader==txt, blue-zero scans, packed-vs-readback round-trip; exit 1 on any anomaly).
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
3. CI ships `d2s-<sha>.exe` (+ map).
4. User reproduces the scene through the logging wrapper — rebuild **and** retail, same scene — and/or captures diag dumps.
5. Uploads zips here → decode with §10 tooling → decide next step from data, not theory.
6. Update `pr6-updated-body.md` and append a worklog Task entry **every round** (the worklog is the institutional memory — this handoff is just its index).

## 12. Triage quick-paths (if a symptom returns)

| Symptom | First check |
|---|---|
| Black light cores / black unit silhouettes | falloff table still `[257]`? (`globals/globals.h:3094`) |
| Light radius / torch discoloration | (rev-3: marker retired by the cf36935 merge — the fix now lives upstream-typed) was: `grep -n "glide fix 10" D2Glide/Renderer/Glide.cpp` — exactly one block comment (~3522) naming the nine u32 stores; confirm the nine `*(uint32_t*)&pfVertexSlot[...]` writes beneath it. Missing = the fix-11-style regression again (apply §7 patch-first rules). (Rev-2: rev 1 said "expect 9" — that counted the stores, not marker lines.) |
| Every unit shadow is a solid blob / samples (0,0) | (rev-3: marker retired by the cf36935 merge) was: `grep -n "vanilla-plus fix 13" D2Glide/Renderer/Glide.cpp` — block comment at ~1221 (DrawShadowQuad). (Rev-2: the marker is `[vanilla-plus fix 13]`, not `glide fix 13` — rev 1's gate string returned 0 on a healthy tree.) Then a fresh `d2shadow-diag.txt` — aspect<0 records must show s 0..64/0..128, t 0..256 |
| `0xc1` halt at boot/menu | texture cache (defects 1–4); check promote bails in bootlog |
| Halt `0x112` at end-of-loading | `GLSUBTILE_EnsureCapacity` return capture (defect 8) |
| Black-triangle grid on title screen | fan slot order (defect 6) — do *not* blame the wrapper (that attribution was retracted) |
| Foreign-module crash in `d2boot.log` | crash-triage stack (Task 2/2-b): VEH skip paths now dump module+offset; symbolize via CI map |
| New "retail right, rebuild wrong" color issue | start from the §1 root pattern; audit decompiler guesses around the site; then capture (§6.1) |

## 13. Sandbox gotchas

- No `gh` CLI/token — PR body edits are handed to the user.
- `git apply` needs `--no-index` and `GIT_CEILING_DIRECTORIES=/home/z/my-project` (the clone has no clean parent boundary).
- Use `rg --hidden` for repo sweeps. The rev-2 clone is **clean** (`03705cf`, 0 dirty files) — a dirty worktree now means *new* local work: inspect it, don't blanket-revert.
- The sandbox resets between sessions (the 2026-10-26 reset lost the rev-1 worklog, scripts/, download zips, work/, upload/ — §2). Persist anything load-bearing in the repo itself; that is why this handoff is committed in-tree.
- Script Persistence Rule: scripts >10 lines go to `scripts/` and get edited in place on failure.
- User timezone America/Toronto; session language English; delivery zips always to `/home/z/my-project/download/`.
