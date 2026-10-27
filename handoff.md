# vanilla-plus — handoff for future work

*Rev 9 (2026-10-28): **the next Ghidra export landed and is MERGED.** Upstream force-pushed: `eef5f2c` was rewritten as `3f6da07` (tree-identical — metadata only) and **`ad964f3`** landed on top (450 files, +181k: Ancients/Diablo/Act II overlay crash fixes, Esc closes speech boxes, **unnamed data tables restored**, D2_GLOBAL_GUARD/D2_STATIC_GUARD guard-zone infrastructure, symbolised crash reports). Merged as **`8030526`** per the §3ter plan; supersession map in §3ter-bis, updated gates in §12. **Fixes 17/19/20/23 and glide defects 3/7/8 are superseded upstream; fixes 18/21/22/24a re-ported; defect 2/4 guards kept.** New field round needed: rebuild + fullscreen-D3D boot + glide sanity (§9 item 1). Prior rev-8 state: glide series (1–14) + sound series (15–16) closed and field-verified; PR #10 open; release `D2LoD-Vanilla-Plus-0.0.1` built from `4857c30`. Historical narratives: **[`HISTORY.md`](HISTORY.md)** (H1–H17). Companion journal: `/home/z/my-project/worklog.md` — when this doc and the worklog disagree, the worklog wins.*

---

*Rev 8 (2026-10-27): **upstream wind-down.** The `Fixes #N` gap on PR #10 is closed (user added the line). **PR #6 (glide) was closed by jaenster** with a per-defect verdict: confirmed defects were fixed **in the Ghidra database** and ship with the next export (§3ter) — the PR-body-regeneration task is MOOT. New headline watch item: **the next upstream Ghidra export** (merge plan + fixes-2/4 re-verification there). State: the glide series (fixes 1–14) and the sound series (fixes 15–16, defects 12+13) are closed and field-verified; the sound fixes are upstreamed as **jaenster/redivivus PR #10** (open, merge-clean, `Fixes #N` in place); the first public release **`D2LoD-Vanilla-Plus-0.0.1`** is out, built from `4857c30`. Docs-only commits after it: `5546a00` (rev 7), `a95bdad` (PLAN v4). Rev-7 restructure note: everything historical — closed glide-era and sound-era narratives, retired infrastructure, deep diagnostic specs, closed investigations, archived micro-items, full delivery texts — lives in **[`HISTORY.md`](HISTORY.md)** (H1–H8). Revision history rev 1–7 verbatim: HISTORY.md §H1. Companion journal: `/home/z/my-project/worklog.md` — when this doc and the worklog disagree, the worklog wins.*

---

## 1. What this project is

Vanilla-plus is the **game-source line** of VTSTech/D2LoD-libslang-vk: the recovered Diablo II 1.14d source rebuilt with gcc (`i686-w64-mingw32`, `-O0` + DWARF fat exe — both intentional, see the branch charter `VANILLA-PLUS.md`). The defect series this handoff closes was one thing: **decompiler-reconstructed code lost facts the original binary encoded implicitly**, and those losses only became visible once the code was relinked by a different compiler.

| Component | What it is | Role |
|---|---|---|
| `Game.exe` | Blizzard **1.14d retail, original** | **Source of truth.** Capture reference for every A/B. |
| `d2.exe` / `d2s.exe` | the gcc rebuild (d2s = stripped artifact) | The thing being fixed. |
| `D2Glide.dll` | rebuilt glide renderer (in-tree `D2Glide/`) | Battlefield of fixes 1–13 (HISTORY H1). |
| `glide3xvk` wrapper | logging glide3x ICD (0.4.5, branch `libslang-vk`) | Measurement instrument: budget-limited `parr#`/`vc0#`/`drawshape#`/`fan#` dumps; vertex color bytes 0–2 = RGB (R = LSB); `grConstantColorValue` = `0xRRGGBBAA`. |
| CI | **RETIRED** | All builds run locally on the user's Win11 box via `tools/build-local-win11.ps1` (HISTORY H2). |

**Root pattern behind every defect so far:** labels adjacent in the original `.data`/`.bss` become unrelated objects after relink; guessed array extents come back one short; live return values are dropped; a color word written through a float-typed alias converts instead of storing bits; FP constants arrive from Ghidra as IEEE-754 bit patterns. **Any future "works in retail, wrong in rebuild" report should start from this pattern** — audit what the decompiler had to guess, not what the code says. **Rule for every future port: a 16-hex-digit literal in Ghidra pseudocode is a double BIT PATTERN — decode it (`struct.unpack('<d', struct.pack('<Q', n))`), never paste it.**

## 2. Branch / commit topology (current)

- `origin` = https://github.com/VTSTech/D2LoD-libslang-vk (public fork, default branch `main` = pre-fix-chain baseline `fd236ce`). **`vanilla-plus` carries the entire fix chain plus this file.** Tip at rev-8 time = **`a95bdad`** (docs-only on top of **`4857c30`** "cleanup diag", 2026-10-27 — the build of public release **`D2LoD-Vanilla-Plus-0.0.1`**; asset `D2-LoD-Vanilla-Plus-0.0.1.7z`; notes: Redivivus + audio fixes + Glide3xVK 0.4.5; known issue: **fullscreen Direct3D does not work** — diagnosed 2026-10-28 → fix 17 (Ghidra truncated IID_IDirectDrawGammaControl to 1 byte, see ledger) + fix 18 (gamma QI still refused on the field box's emulated DD7 even with the correct IID → init now degrades gracefully instead of aborting, see ledger) + fix 19 (pixel-format template + adapter-GUID extent truncations, see ledger); `-3dfx` fullscreen OK, `-w` works in all modes).
- `upstream` = https://github.com/jaenster/redivivus — **`upstream/main` = `ad964f3`** (2026-10-28). History: 10 commits, linear; vanilla-plus forked at `1b24c75`; `eef5f2c` (the tree merged at `cf36935`) was rewritten as `3f6da07` **tree-identical** (metadata-only rebase), then **`ad964f3`** (+181k, the export-scale commit) landed. Vanilla-plus merge state: `8030526` = `08cddd2` (fix 24) + `ad964f3`.
- **Upstream PR #10** (the sound fixes): "Sound: only the intro video had audio — …", head `VTSTech:pr4/sound-except-intro` (tip **`b0c250c`**, the 3 `git am`-ed patches `5c75b10`/`e614f5e`/`b0c250c` on `eef5f2c`) → base `upstream main` @ `eef5f2c`, 3 commits / 3 files, `mergeable_state: clean` at filing (2026-10-27). **`Fixes #N` added 2026-10-27 (rev 8)** — merging now auto-closes the companion issue. Watch: maintainer review on the `SetPan`/bit-pattern details.
- Active branches of record: `vanilla-plus` (game-source line), `main` (fork baseline), `pr2/glide-renderer-fixes` (= `31acdac`, mirror of the **closed** PR #6 line — historical), `pr3/mobs-drop-items`, `pr4/sound-except-intro` (= 4857c30 content on `eef5f2c` + the 3 sound commits, tip `b0c250c` — the PR #10 head), `libslang-vk` (glide3xvk wrapper line, tags 0.3.1–0.4.5). Retired branches and the twin-commit story: HISTORY H2.
- **Full fix-chain hash map** (kept as the fix-chain record; the export merge it fed is DONE — verdicts per fix in §3ter-bis, narrative in HISTORY H17):

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
  | 9 | `642711a` | glide fix 9 | falloff `[257]` — CI build `d2s-642711a`; **reference base for fix-10-era forensics** |
  | 10 | `6feb86f` | glide fix 10 | ground-argb u32 stores |
  | 11 | `b478f2b` | glide fix 11 | ShadowDiag (diagnostic) |
  | 12 | `7e07380` | restore glide fix 10 | fix-11 drop-in regression restore |
  | 13s | `440c5f8` | vanilla-plus fix 13 | shadow spans — CI build `d2s-440c5f8`, field-verified |
  | 13l | `1a180d1` | glide fix 13 | LightDiag v2 (diagnostic) |
  | — | `03705cf` / `2612a51` / `cf36935` / `0fa2003` / `45c6c2d` | rev 1–4 docs, CopyRows link, **merge eef5f2c**, rev 3, merge-fallout port | merge-era row (details HISTORY H1) |
  | — | `b2411ec` | fix (jaenster-main) | sibling line: CopyRows fix + MANIFEST.txt removal |
  | — | `d217e04`…`5931e15` | docs + build.yml ×5 | CI hardening churn → CI retired |
  | — | `eb16281`…`fcd50d8` | sounddiag v1–v5 | client sound instrumentation (retired r10, HISTORY H4) |
  | 15 | `04cea73` | vanilla-plus fix 15 | sound hwnd (Application.cpp GetHwnd) + `-3dfx` contrast clamp + build-script consolidation |
  | — | `4031fa7` | R6 fix (title-music root cause) | defect 12 (HISTORY H3) |
  | — | `e269cde` | handoff | this file, rev 5 |
  | 16 | `9c8beb9` | vanilla-plus fix 16 | defect 13: CalculateVolumeAndPan out params read back through float storage → SFX volume 0 → att=-10000 |
  | — | `4857c30` | cleanup diag | sounddiag/viddiag retirement (r10, 12 files −140 lines); `d2_boot` infra kept (HISTORY H4) |
  | 17 | (pending commit) | vanilla-plus fix 17 | fullscreen D3D: restore 16-byte IID_IDirectDrawGammaControl — export kept 1 byte (`0x3e`) → primary-surface QI DDERR_INVALIDPARAMS → retail "Error 25" on every non-3dfx fullscreen boot (FOG log 2026-10-27; HISTORY H9) |
  | 18 | (pending commit) | vanilla-plus fix 18 | fullscreen D3D round 2: with the restored IID the gamma QI STILL failed on the field box (DDERR_INVALIDPARAMS again, FOG log 13:12) → gamma QI + GetGammaRamp are now non-fatal (log "- continuing without hardware gamma", g_pDDGammaControl stays null; consumers already null-guarded at Direct3D.cpp :833 / :719) — deliberate robustness deviation from retail, which aborts (HISTORY H10) |
  | 19 | (pending commit) | vanilla-plus fix 19 | fullscreen D3D round 3: post-fix-18 boot crashed (C0000005 read-of-0) in RENDERER_D3D_CreateFontTextures+0x219 (symbolized via user's d2exe.map) — `gsD3DTextureDescTemplate` was exported as a 4-byte scalar (dwSize only) but read as the full 32-byte DDPIXELFORMAT by both texture-creation helpers → garbage pixel format → every D3D texture CreateSurface failed → ignored return at :3234 → Lock through null; restored as A1R5G5B5[8] (values corroborated by D3D_EnumTextureFormatsCallback + the g_Palette16Bit packer). Same-family: `gsD3DAdapterGuid` was 1 byte but the registry GUID load writes 16 → spilled into GlobalGreen/Blue/Red (D3D vertex-lighting tints) → restored [16] (HISTORY H11). Rev 2 (field build broke): the 19b extent change itself broke the compile — the bare `&gsD3DAdapterGuid` at :1853 was `uint8_t*` against the old scalar but `uint8_t(*)[16]` against the array, no implicit conversion to `LPBYTE` → fix 19c explicit `(LPBYTE)` cast (audited: all other 19a/19b use sites already used explicit &-casts); same pass fix 19d closed the sibling under-extent — `gD3DAdapterInfoBuffer` was 1 byte but the `SRegLoadString(...,"DeviceName",...,0x100)` at :1844 caps at 0x100 and retail writes the value (`SaveDeviceConfigToRegistry` :585-606) → restored `[0x100]` |
  | 20 | (pending commit) | vanilla-plus fix 20 | fullscreen D3D round 4: post-fix-19 boot PLAYS the intro video + music fullscreen (`CreateWindowWithChecks -> 1` — the original wall retired) then clean `HALT line=215 (0xd7)` ~5s in = first logo boundary = first DC6-sprite upload — the export rendered the three sprite-cache structs (image 0x970eac / node-buffer labels 0x970ec4) as two PHANTOM function-statics and `RENDERER_DIRECT3D_UploadTextureToCache` computed `pCache = &DAT_00970eac + class*0x1c` off them while `InitializeTextureCachePool` seeds `gD3DGenericTextureCache.aCaches[0..2]` → unseeded struct (garbage nMaxNodes, null pFreeHead) → `D3DTEXCACHE_AllocateNodeFromFreeList` halt 0xd7; upload path now addresses the real pools (class 1/2/3 → aCaches[0/1/2], the ClearCacheNodes mapping) (HISTORY H12) |
  | 21 | (pending commit) | vanilla-plus fix 21 | fullscreen D3D round 5: post-fix-20 first sprite upload now clean-halts `HALT line=365 (0x16d)` = `RENDERER_DIRECT3D_UploadTextureToCache` Direct3D.cpp:2521 — freshly-popped cache node's `pSurface` (+0x10) is null while every statically-reachable path (seeder `InitializeTextureCachePool` :5039, allocator, eviction) provably leaves allocated nodes surfaced — the only surviving route is `pCache = &aCaches[class-1]` with a GARBAGE class slot: the per-frame descriptor header (0x2c/block, slots `[class, nodeIdx]` at `mode*8`) comes from a non-zeroing/recycled pool, and the D3D upload's exact-zero test (unlike glide fix 3's range test) let garbage ≠1..3 skip the size recompute and walk the cache array into the packed struct's `apSpriteTextures`/`apSpriteSurfaces` (aCaches[3+] adjacency lottery: pointer-shaped pFreeHead → no 0xd7, bogus node read → null pSurface). Fix = port the field-verified glide guards: fix-3 mirror (class slot <1 or >3 → zero + recompute) + fix-2 mirror (stale nodeIdx validated against node backpointers +4/+8 and the pool bound before the reuse deref). Cross-renderer lesson: the D3D upload path never executed until this week, so the glide series' fixes 2/3/4 family was never audited against it (HISTORY H13) |
  | 22 | (pending commit) | vanilla-plus fix 22 | fullscreen D3D round 6: post-fix-21 build `d2s-0604b96` (hash rotated — overlay confirmed in; halt site shifted +0x99 with the added guard code, as expected) STILL clean-halts `HALT line=365 (0x16d)` at the same first-sprite-upload spot. Round-9 audit re-verified EVERY read-side premise by direct reading: seeder :5070-5123 surfaces every free-list node and truncates coherently on mid-seed failure; `CreateSurfaceWithTexture` :2238 cannot return TRUE with a null out-param; alloc/evict/invalidate never clear node +0x10; `d3dTextureCache.h` layouts internally consistent (packed set struct adjacency CONFIRMED correct at class 1..3); descriptor region = the memset 0x2c block from `FixupPointersAndPrepare`, `pbSrcPixel` = `*(block+0x18)` = descriptor base. Conclusion: halt 0x16d is unreachable through READ paths post-fix-21 → fix 22 (a) closes the family's one PROVEN unbounded WRITE — `D3D_PixelDataFreeCallback_ClearCacheNodes` validated each [class, idx] pair with only a signed >=1 and wrote `aCaches[class-1].pNodeBuffer[idx-1]+0x4 = 0` with NO class range check / NO idx upper bound / NO owner match; garbage pairs (pairs 0/4 are never written by the upload; DCC combined-blob frames carry 0x23-byte headers in non-memset memory per H13 footnote b) wild-wrote past the node buffer or cleared a FOREIGN node's backpointer (the round-8 audit-noted residual) — now hardened with the fix-21 discipline (range check + node+4 must name the descriptor being freed; legit frees byte-identical; first-of-shape reported to boot log); (b) adds `RENDERER_D3D_UploadHaltDump` before ALL 12 halt sites in UploadTextureToCache (pool ptr/max/used/freeHead/nodes + full node dump + apSpriteTex lines, via D2BOOT_MarkFmt) so any recurrence names its failing premise from the log alone; (c) boot banner `vanilla-plus fixset through fix 22` (Application.cpp) so field logs self-identify builds regardless of commit-hash rotation. Glide's twin callback deliberately untouched (field-verified working renderer; asymmetric hardening noted) (HISTORY H14) |
  | 23 | (pending commit) | vanilla-plus fix 23 | fullscreen D3D round 7: post-fix-22 build (fixset banner present) STILL `HALT line=365 (0x16d)` at the first big sprite upload — but this time the fix-22 dump FIRED and decided the round from the log alone: pool fully coherent (max=178, used=7, freeHead=nodes+7*0x20, freeTail−nodes=177*0x20 exact, node[7] pos=6 prev=node[5] next=0) and the dumped node is HEALTHY — tile/mip just written by this call, surf=0E0B8A18 tex=0FA87324 seeded, only pal=0 = the caller's NULL palette. A halt that tests node+0x10==0 cannot fire while the same node's +0x10 prints non-null ⇒ the check and the dump read DIFFERENT addresses. ROOT CAUSE: C pointer-scaling — the check added the raw byte offset (idx*0x20−0x10) to a `D3DTextureCacheNodeStrc*` (stride 0x20), so the true address was base+(idx*0x20−0x10)×0x20: idx 1..6 land on other seeded nodes' nIndex (non-zero ⇒ six silent false-passes), idx=7 lands at base+0x1A00 = 0x3E0 bytes PAST the 178×0x20=0x1620 buffer ⇒ reads heap zero ⇒ "null surface" halt on a fully seeded node. Field log used=7/node=7 is the arithmetic's own prediction. Same disease at the reuse-path stored-palette compare (:2603, never matched ⇒ reuse never fired) and the SetTexture source read (:2605, would hand SetTexture a garbage pointer on the first mode-4 re-upload). Glide compiles the IDENTICAL transcription byte-exact because D2Client/Renderer/Glide.h:36 types `pNodes` as `uint8_t*` while d3dTextureCache.h types the base as the node struct — one header typing difference is why -3dfx survived the same decompiler output and D3D never cleared the first big sprite; retroactively explains round 9 (halt at the 7th class-1 allocation, no dump yet) and demotes the fix-22 wild-write theory to hardened-but-unproven sibling (kept: defense in depth, byte-identical for legit frees). Fix = all three sites rewritten to the byte-exact `(uint8_t*)pCacheNodeBase + (*piVar2 - 1) * 0x20 + OFF` form the fix-21 validation already uses (OFF 0xC/0x10/0x14); banner → fixset through fix 23 (HISTORY H15) |
  | 24 | (pending commit) | vanilla-plus fix 24 | fullscreen D3D round 8 (first PLAYABLE round — fix 23 cleared the first-sprite wall; title/char-select/in-game all reached): two color defects. (1) GROUND ALL BLACK (fullscreen D3D only): `DrawGroundTile` is the provider's ONLY `DrawIndexedPrimitiveVB` call site, and the Ghidra export had split the retail index buffer into THREE under-extent phantom function-statics — a 1-byte `gpD3DGroundTileVertexBuffer` that the draw read 130 bytes of index WORDs from, a 2-byte `gaD3DGroundTileIndexBuffer` that the prefill overran by 129 bytes, and `gdwRendererMode = 0x50000` (literally WORDs [0,5] — the first half of the low-quality 4-word fan [0,5,35,28]). Prefill filled one label, the draw read another ⇒ garbage indices ⇒ D3D drew nothing where the floor should be, while sprites (DrawPrimitive, no index list) and the GDI windowed path were immune. Fix = real shared arrays `g_aD3DGroundTileStripIndices[65]` (5 bands × (12 pair words [c,c+6] + 1 trailing duplicate = the odd-parity band join; puDestIndex was uint16_t* and local_9c[60] confirms 13-word bands)) + `g_aD3DGroundTileFanIndices[4] = {0,5,35,28}` ([0,5] verbatim from the retail dword, [35,28] = the remaining two per-frame-updated vertices in diamond-perimeter fan order). (2) "Expansion Character" green→purple on char select: closed the latent-queue gnPaletteIndexCounter extent defect (Task-2/3 recon, glide sighting) — declared `uint32_t` (4B) but `D2WINPAL_LoadPaletteFiles` writes 40B (9 PALETTEENTRY + 1 manual triple, D2WinPalette.cpp:163-174) ⇒ 36B overrun into `gPaletteAct1[0..35]` (gcc adjacency, globals/D2Win.cpp:251272-3), overwriting act1 plane-0 remap entries 0-8 with text-color RGB triples read as remap indices. Declared `uint8_t[40]`; table-slot 0x45 consumers unaffected (same verbatim bytes). If the purple persists past this fix, the next lens is the D3D glyph-texel mapping (glide's format-5 palette-free texel vs D3D's `g_Palette16Bit[rawidx]`) — the D3D text path is retail-identical code, so a residual defect would be data/state, not logic. Banner → fixset through fix 24 (HISTORY H16) |

- **Naming collision — read before confusing yourself.** Three numbering systems circulate:

  | "fix N" (fix-chain) | PR #6 "commit N" | defect (PR body) | What it is |
  |---|---|---|---|
  | fixes 1–7 | commits 1–4 (batch, 6 files, +466/−79) | 1–8 | texture cache (1–4), fan vertices (5–7), subtile halt (8) |
  | fix 8 | commit 5 | — (diagnostic) | LightDiag v1 |
  | fix 9 | commit 6 | 9 | falloff table `[256]`→`[257]` |
  | fix 10 | commit 7 | 10 | ground-argb u32 stores (+21/−9) |
  | fix 11 | commit 8 | — (diagnostic) | ShadowDiag |
  | fix 12 | *no PR change* | — | vanilla-plus-only restore of fix-10 delta |
  | fix 13 (shadowspan) | commit 9 | 11 | DrawShadowQuad portrait spans (+29/−4) |
  | fix 13 (lightcolordiag) | commit 10 | — (diagnostic) | LightDiag v2 |

  Yes, "fix 13" names two different overlays (`pr2-glide-fix13-shadowspan-overlay.zip` / `pr2-glide-fix13-lightcolordiag-overlay.zip`).
- Note: pr2's Glide.cpp text has **drifted from vanilla-plus's** (jaenster revision, ~2441 lines). Overlay scripts therefore assert *per-anchor* uniqueness, never whole-file equality, across the two lines.

## 3. The eleven defects (root causes, one line each) — the fix-chain record (upstream verdict: §3ter)

1. **Phantom texture-cache structs** — function-local statics `gGlideSmallTextureCacheBase`/`gnGlideState` were `.data` labels adjacent to `gGlideSmallTextureCache[3]`; rebuilt they're unrelated 4-byte objects → every upload addressed a phantom → `0xc1` halt (`-3dfx` launcher crash, issue #5).
2. **1-based size class as 0-based index** — class 3 indexed one `0x20` struct past `cache[3]`. Clamped + decremented.
3. **`GLTEXCACHE_EnsureCapacity` result discarded** — caller stored an uninitialized local as the 1-based node index. Now returns the index.
4. **Stale sprite-header slots splice free-list nodes onto the active list** — recycled headers inherited stale node indices; promote walked the node's own links without `nUsedCount++` → cache drained → `0xc1` with a clean-looking free list. Fixed by backpointer/mode validation.
5. **Scattered fan-vertex globals → giant clipped triangle** — one `4 × 0x1c` vertex block decompiled into ~20 globals; linker scatters them. All quad/tile submits now use contiguous local blocks with every field filled.
6. **Fan slot order + portrait spans OOB** — decompiler names offset one fan slot (hardware order `v0=TL, v1=BL, v2=BR, v3=TR`); portrait (negative-aspect) spans indexed backward out of `gaPerspectiveImageSizeTable`. Negative classes compute spans from pow2 dims (`nTexW/H·2^(8−lod)`).
7. **Wall/shadow tile fans** — 8 scattered "vertex globals"; `oow/s/t` presets were `InitPerspectiveTables` function-local statics, unreachable after rebuild; `ApplyColorLighting` writes landed on unknown `.bss`. Both `DrawWallTileSimple`/`DrawShadowTileSimple` now submit contiguous blocks with full-texture mapping.
8. **Wall-subtile cache halt** — `GLSUBTILE_EnsureCapacity` decompiled as `void`, dropping its live 1-based index → `ERROR_Halt 0x112` at end-of-loading. Restored the `short` return.
9. **Lightmap falloff table truncated** — `gnLightmapInterpolationState` declared `[256]`, live index 0x100 → saturated cells multiply rgb by 0 → **black light cores / black unit silhouettes**. Two-line fix: `[257]`.
10. **Ground-tile vertex colors through a `(float)` cast** — stores the *f32 bit pattern* of the intended PARGB (`0xff808080` → `0x4f7f8080`). Fix: 9 stores via `*(uint32_t*)&`.
11. **Shadow-quad portrait spans** — `DrawShadowQuad` read spans with the negative aspect → every unit shadow sampled texel (0,0). Fix: mirror fix 6's span block. Caught by the ShadowDiag capture after static analysis *passed* the path.

### 3bis. Sound series — status block only (full record: HISTORY H3)

- **Defect 12 (title/in-game music silent)** — CLOSED, fix `4031fa7` (R6): Ghidra FP bit-pattern constants decoded in `D2SoundSmp.cpp` clamps + `-2000` dB scale restore. Field-verified.
- **Defect 13 (NPC/monster/player SFX silent)** — CLOSED, fix 16 `9c8beb9`: `SOUND_UpdateChannelParams` reads `SOUNDSMP_CalculateVolumeAndPan`'s integer out params as `uint32_t` (was float-storage punning → subnormal truncation → vol 0 → att=-10000). Music channels skip that block — the observed music-vs-SFX split. Field-verified.
- **Upstreamed** as jaenster/redivivus **PR #10** (3 commits, self-contained comments; upstream additionally lacked the `-2000.0` scale fix). **SMP layer exonerated against retail decompiles — do not re-audit.** sounddiag retired (HISTORY H4).

### 3ter. Upstream verdict on the glide defects — PR #6 closed by jaenster (2026-10-27)

jaenster reviewed the glide defects against 1.14d and fixed the confirmed ones **in the Ghidra database** rather than merging the C++ — the next regen emits the right code (fixes at the source of truth, no regen conflicts). His verdict, mapped to the §3 defect numbering:

| Defects | Verdict (jaenster, 2026-10-27) |
|---|---|
| 1, 6/11, 9, 10 | **Confirmed, fixed in Ghidra**: the vertex block and the texture caches as one object; the span table as one `int[14]`; the falloff table with 257 entries; the ground vertex colour as an integer store. |
| 3, 5, 7, 8 | **Already fixed** in Ghidra after the tree the PR was worked from (`eef5f2c`). |
| 2 | **Rejected**: 1.14d uses the 1-based size class directly; the `− 1` only undid a rebase from the same patch. |
| 4 | **Rejected**: 1.14d has no slot check there — the original's behaviour is kept. |

Implications for vanilla-plus:

- The hand fixes for defects 1, 3, 5, 6/7/8, 9, 10/11 will be **superseded (or conflict) when the next upstream export lands**. Merge plan: take upstream's regen, drop the matching hand fixes, re-run the §12 gate set + the §11 field loop.
- **Defects 2 and 4: vanilla-plus keeps its fixes until proven unnecessary.** jaenster's verdicts describe the Ghidra *source*; the crashes those two fixes address are relink-layout phenomena (adjacency/scattering after relink) — a retail-faithful regen can still relink differently. Re-verify empirically post-export before dropping.
- jaenster invited **issues with retail captures** if Glide misbehaves post-export — the logging-wrapper dumps + the ShadowDiag/LightDiag capture method are the standing asset for that.
- PR #6 closure kills the body-regeneration task and the pr2-branch advancement decision; `pr2/glide-renderer-fixes` stays as a historical mirror.

### 3ter-bis. The ad964f3 merge — supersession map (rev 9)

Resolution discipline: our side = `eef5f2c` + our fixes; their side = `eef5f2c` + `ad964f3` (because `3f6da07` ≡ `eef5f2c`), so every hand-merge ran a 3-way with **`eef5f2c` as base** — conflicts mark exactly where our fixes meet upstream's re-typing. 85 files conflicted; 70 taken wholesale (66 untouched + 4 superseded), 9 auto-merged clean, 7 by hand, `Direct3D.cpp` re-ported onto upstream's tree (`scripts/port_d3d_fixes.py`).

| Our fix | Verdict after ad964f3 | Disposition |
|---|---|---|
| 1 (phantom texture-cache structs) | upstream re-typed (in `eef5f2c`, absorbed at `cf36935`) | gone (superseded) |
| 2 / 4 (defects 2+4: class-slot + stale-nodeIdx guards) | **REJECTED upstream, still relink-relevant** | **KEPT** — glide side in `Glide.cpp`, D3D side re-ported as fix 21; §3ter rule: keep until empirically proven unnecessary |
| 3 / 8 (EnsureCapacity results; defects 3+8) | upstream re-typed the returns (`int`) and the miss path | `glTextureCache.cpp` / `glSubtileCache.*` / `glBlocks.cpp` taken from upstream |
| 5, 6, 7, 9, 10, 11, 13 (glide defects 5–11) | upstream typings (in `eef5f2c`) | absorbed at `cf36935`; diagnostics (ShadowDiag) kept |
| 15 / 16 (sound, defects 12+13) | upstream never fixed | **KEPT** (`Glide.cpp` clamp auto-merged; `D2SoundSmp.cpp` got upstream's D2_STATIC_GUARDs + our R6 note; `Sound.cpp` fix-16 block intact) |
| 17 (truncated gamma IID) | **superseded**: IID lives as the first 16 bytes of the 272-byte `gszRendererD3DDeviceInitializationCode` blob | upstream kept |
| 18 (gamma fatal → degrade) | **still needed** — upstream QI/ramp failures remain fatal | **re-ported** onto upstream's blob-based QI (consumers null-guarded at :720/:834) |
| 19a/b/d (template[8], GUID, DeviceName buffer) | **superseded**: upstream typed all three (`[8]` with our exact values; `GUID`; `[264]`) | upstream kept |
| 20 (phantom cache statics) | **superseded**: upstream's `pCache = (cache − 0x1c + class·0x1c)` addresses the real pools | upstream kept |
| 21 (class-slot + nodeIdx guards) | **still needed** — upstream still trusts both slots | **re-ported** (markers ×2) |
| 22 (upload-halt dump + hardened free callback) | **still needed** — upstream's 11 halt sites un-instrumented; callback still only signed-checks | **re-ported** (dump def + 12 calls; `D3DTEXCACHE_ClearCacheNodeSlot`) |
| 23 (byte-exact node addressing) | **superseded**: upstream's int-arithmetic (`iCacheNodeIdx − 0x10 + base`) is byte-exact by construction | upstream kept (the `(uint8_t*)pCacheNodeBase` forms are GONE — do not re-add) |
| 24a (ground index arrays) | **still needed** — upstream still has the under-extent phantom triad (now guarded) | **re-ported** (`g_aD3DGroundTileStripIndices[65]` / `g_aD3DGroundTileFanIndices[4]`) |
| 24b (gnPaletteIndexCounter extent) | **superseded, and OUR 40 was off by one**: the loader writes 9 PALETTEENTRY (36B) + a 3-byte RGB triple = **39 B** (the peFlags 4th byte is never written) | upstream's `uint8_t[39]` + guard kept; our comment preserved at the def |
| vanilla-plus-only: CERT_INFO fix, DropDiag/LightDiag/d2boot, banner, tooling, docs | untouched upstream | all survived (14/14 gates) |

New upstream capabilities now in-tree (adopt, don't fight): **guard zones** (`D2_GLOBAL_GUARD`/`D2_STATIC_GUARD` from `d2_platform.h`; `D2LOUD_GuardPump` walks them; a hit logs `D2GUARD <owner> ...` to the d2loud report; `D2_GUARD_HALT=1` makes it fatal; `D2_GUARD_WATCH=<owner>` arms a write watch) — the 39-vs-40 palette question is exactly what the guard would have caught live; **frame groups** (`__frame0_t` where a callee reaches past stack locals); **unnamed data tables restored** (+181k — first full-content rebuild of the .data section; a build from this merge is the first real test).

## 4. Verification status (merge not yet field-verified; fix chain was all green as of rev 8)

- Fixes 1–8 field-verified (launcher crash gone; title/select/loading render; in-world tiles, rain/fire/glow correct; end-of-loading halt gone).
- Fix 9 field-verified on `d2s-642711a` (silhouettes gone under night/storm).
- Fix 10 (+12 restore) field-verified (light radius / torch colors match retail).
- Fix 13 field-verified on `d2s-440c5f8` ("Shadows appear fixed"), validated by the second `d2shadow-diag.txt` capture (24/24 records reproduce the span law `pow2dim·2^(8−lod)`; wrapper `vc0#` shadow stream byte-identical retail-vs-rebuild).
- Lighting path proven channel-complete via LightDiag v2 (details HISTORY H5); user: "tbh the lights look fine."
- Fixes 15–16 (sound) field-verified; release `0.0.1` built from `4857c30`.

## 5. Closed investigations

→ **HISTORY.md H5** (do-not-reopen list with the evidence trail). The one standing rule from it: **the ShadowDiag capture caught what static analysis could not** — trust captures over audits.

## 6. Methodology — what actually worked (keep doing this)

1. **Evidence-first per the charter**: never author a fix from static reading alone when a capture can pin the stage (ShadowDiag is the flagship; LightDiag v1 turned "units render black" into a two-line fix).
2. **Retail A/B through the logging wrapper** — same wrapper build, same scene, diff the streams. Budget-limited dumps mean early-frame evidence only.
3. **Byte-level forensics over narrative**: file sizes, `cmp`, f32-bit-pattern math, crc groups, per-record parser asserts.
4. **One-shot client diagnostics with a decision tree shipped in the overlay** — the dump should discriminate between candidate stages in a single build.
5. **QA gauntlet for every delivery** (§7) — non-negotiable.

## 7. Delivery conventions (condensed; full texts + fix-11 lesson: HISTORY H8)

- **Patch-first, always.** Full-file drop-ins are fallback only, and must be cut from the tip the user actually has (base-state grep gate printed in the delivery doc). The fix-11 regression (HISTORY H8) is why.
- **QA gauntlet**: apply round-trip byte-identical on every supported base; negative test refuses cleanly on a fixed tree; unique anchors per edit site; `g++ -fsyntax-only` for new files; builders persisted as scripts.
- **Routes**: git bundle (history matters) → `git bundle create download/<name>.bundle origin/vanilla-plus..vanilla-plus`, verify in a scratch repo at the *fetched* prerequisite tip. Overlay zip (file-level) → only files actually changed + MANIFEST (base commit, sha256s, grep gates, steps); tmpfiles.org **60-min expiry**, post the extracted `/dl/` token link, byte-verify. format-patch (PR-style, proven on PR #10) → commits in a scratch worktree at the target base, `git format-patch`, `git apply --check` QA on pristine base, zip + `MANIFEST.sha256`.
- Sandbox cannot push — the user applies/commits/pushes on their tree (`F:\GAMES\Diablo II`), commits styled `vanilla-plus fix N: ...` / upstream plain-subject style.

## 8. Diagnostics on the tree (condensed; deep specs: HISTORY H4)

- **ShadowDiag** — in-tree (`D2Glide/Renderer/ShadowDiag.{h,cpp}` + 3 hooks in Glide.cpp: include, `DrawGroundTile` head `NoteWorldFrame()`, `DrawShadowQuad` tail `OnShadowQuad`). Gate: `grep -c "ShadowDiag" D2Glide/Renderer/Glide.cpp` → 3.
- **LightDiag v2** — in-tree (`D2Client/Draw/LightDiag.{h,cpp}` + hooks in `LightMap.cpp`, `D2Client.cpp:1802`, `Draw.cpp`); dumps at in-world frame 90.
- **DropDiag** — hooks in `Drop.cpp` (include + `NoteRollLookup`/`NoteItemGen`/`NoteItemSpawnAudit`). Gate: `grep -c "pSpawnRoomFound" D2Game/ITEMS/Items/Drop.cpp` → 3.
- **sounddiag/viddiag — RETIRED** (r10, `4857c30`; mark formats of record: HISTORY H4). Re-cut probes if sound symptoms return — don't reinvent.
- **`support/d2_boot.{h,cpp}` stays**: permanent `[d2boot]` boot/crash marks are load-bearing (12 files).
- All idle diag modules are zero-behavior-change; strip-or-env-gate is **offered, not requested** (keep §12 gates accurate if touched).

## 9. Current / upcoming tasks (the live list)

1. **Upstream PR #10 watch** (jaenster/redivivus, sound fixes). `Fixes #N` is in place (rev 8) — merging auto-closes the companion issue. Watch for maintainer review on the `SetPan`/bit-pattern details.
2. **ad964f3 merge field round (the rev-9 headline)**: merge `8030526` is gated green but NOT field-verified. Loop: user rebuilds (`tools/build-local-win11.ps1`), then (a) `-3dfx` boot = glide regression check (§3ter: post-export Glide misbehaviour → new upstream issue with retail captures); (b) fullscreen D3D boot = fixes 18/21/22/24a re-port check (banner reads "fixset through fix 24"; if a halt recurs, read the `upload-halt` lines FIRST — fix 23's old gate is RETIRED, upstream's int-arithmetic node math is byte-exact by construction, so a healthy dump + fired halt now means audit the NEW arithmetic, not re-add the old form); (c) ground renders + "Expansion Character" green = 24a/24b (extent now 39B); (d) watch the new `D2GUARD` lines in d2loud reports — upstream's guard zones are live and will name any extent defect the merge couldn't see; (e) gameplay spot-checks for ad964f3's own fixes (Ancients/Diablo/Act II overlays, Esc closing speech boxes, item drops).
3. **Task C (gameplay, unparked — awaiting user go)**: Z-key camera zoom + Shift equipment-compare. Loop per §11.
4. **Offered, not requested**: (a) `-3dfx` Contrast/SetGamma boot-halt clamp as an upstream patch (fix exists in fix 15, `04cea73`; PR #10 is sound-only, so this would be a separate proposal — hold or propose post-export); (b) **fullscreen Direct3D regression** — fix 17 (truncated IID) + fix 18 (graceful gamma degradation) + fix 19 (pixel-format template + adapter-GUID extents, HISTORY H9/H10/H11) + fix 20 (phantom cache statics, H12) + fix 21 (sprite-header slot guards ported from the glide series, H13) + fix 22 (free-callback wild-write hardening + upload halt-site self-diagnostics + fixset boot banner, H14) + fix 23 (typed-pointer scaling in the D3D upload's node addressing — three byte-exact rewrites; the fix-22 dump caught it, H15) + fix 24 (ground-tile index-buffer phantom statics → real shared arrays; gnPaletteIndexCounter true 40B extent closing the gPaletteAct1[0..35] clobber, H16); field verification pending (round-8 field: PLAYABLE — title/char-select/in-game reached; residual: purple "Expansion Character" + black ground, both addressed by fix 24); (c) ShadowDiag/LightDiag/DropDiag strip-or-env-gate.
5. **Watch item**: `SOUNDSMP_SetPan` right-branch negation vs retail vtable+0x40 (stereo position only; identify the retail owner before touching — HISTORY H3).

## 10. Tooling

→ **HISTORY.md H7** (build-spec table; scripts were lost in the 2026-10-26 sandbox reset — recreate on demand per the Script Persistence Rule). Sandbox currently holds `scripts/` from the rev-4+ era only.

## 11. Field-validation loop (how work gets done)

1. Author fix/diagnostic in the sandbox → package per §7 → deliver from `download/`.
2. User applies on their Windows tree (`F:\GAMES\Diablo II`), commits, pushes.
3. User builds locally on Win11 (`tools/build-local-win11.ps1`) — CI retired (HISTORY H2).
4. User reproduces the scene through the logging wrapper — rebuild **and** retail, same scene — and/or captures diag dumps.
5. Uploads logs/zips here → decode → decide next step from data, not theory.
6. Append a worklog Task entry **every round** (the worklog is the institutional memory — this handoff is just its index). Public artifacts worth keeping: releases on the fork, issues/PRs upstream.

## 12. Triage quick-paths (if a symptom returns)

| Symptom | First check |
|---|---|
| Black light cores / black unit silhouettes | falloff table still `[257]`? (`globals/globals.h:3075`) |
| Light radius / torch discoloration | `grep -c "vanilla-plus fix 3"` → 1 (zero-and-recompute at ~1042) and `grep -c "vanilla-plus fix 2"` → 1 (ported stale-slot validation at ~1071-1098) in `D2Glide/Renderer/Glide.cpp`. Missing = a fix-11-style regression (§7 routes). |
| Every unit shadow is a solid blob / samples (0,0) | `grep -c "ShadowDiag" D2Glide/Renderer/Glide.cpp` → 3 and `grep -c "D2Client/D2GfxDataStrc.h" D2Glide/Renderer/ShadowDiag.cpp` → 1. Then a fresh `d2shadow-diag.txt` — aspect<0 records must show s 0..64/0..128, t 0..256 |
| Drops never broadcast / ground items vanish | `grep -c "pSpawnRoomFound" D2Game/ITEMS/Items/Drop.cpp` → 3 (upstream canonical fix 14) + DropDiag hooks intact. A `pResolvedSpawnRoom` reference = the 0fa2003 regression (deleted in 45c6c2d). |
| `0xc1` halt at boot/menu | texture cache (defects 1–4); check promote bails in bootlog; the fix-2 validation block must run in the upload path (`if (nNewCacheNode)` at ~1092) |
| Clean HALT `0x164/0x165/0x16d/0x16e/0x171/0x172/0x19a/0x19b/0x1b4/0x1b5/0x1c0` (D3D sprite upload) | fix-21 guards present? `grep -c "vanilla-plus fix 21" D2Direct3D/Renderer/Direct3D.cpp` → 2 (class-slot range guard ~:2481 + nodeIdx backpointer validation ~:2513). Missing = a fix-11-style regression. The 11 codes are all `UploadTextureToCache` branches (H12 footnote) — the halt code names the branch. **Fix 22 makes the wall self-reporting**: the `upload-halt <code>: ...` lines in d2boot.log (emitted just before the halt) carry mode/class/node/pool + the failing node's full contents — read them FIRST, then the halt code. Sanity anchors: pool max ≈ hundreds and nodes != 0 means seeded; a node with surf=00000000 inside a seeded pool = the round-9 contradiction, escalate with the dump. No `upload-halt` lines at all = a pre-fix-22 build (check for the `vanilla-plus fixset through fix 22` banner; local builds do not always rotate the commit hash). `fix22: free-callback skipped ... (was a wild write)` lines = the hardened callback catching live garbage pairs — include them in the report. **Fix 23 closes the 0x16d root cause** (typed-pointer scaling, H15) — **SUPERSEDED at the ad964f3 merge**: upstream's re-typed upload computes node addresses in INT arithmetic (`iCacheNodeIdx − 0x10 + base`, base = `aCaches[class−1].pNodeBuffer` via the `(cache − 0x4 + class·0x1c)` int read), which is byte-exact by construction; the old `grep "vanilla-plus fix 23" → 3` gate is RETIRED (0 hits is CORRECT now). A fired halt with a HEALTHY dump now means: audit upstream's NEW arithmetic for the failing premise before suspecting memory. **Fix 24 quick-paths**: BLACK ground/floor (D3D) = index-buffer statics — gate `grep -c "g_aD3DGroundTileStripIndices" D2Direct3D/Renderer/Direct3D.cpp` → 4 (def + comment refs + prefill + draw; re-ported at `8030526`); wrong-hue UI sprites/text = check the palette-buffer extents first (`gnPaletteIndexCounter` now **uint8_t[39]** — the true extent, 36B PALETTEENTRYs + 3B manual triple; upstream form + guard; if a `D2GUARD gnPaletteIndexCounter` line ever fires, the loader writes past 39 and the field analysis must be redone). |
| Halt `0x112` at end-of-loading | wall-subtile cache (defect 8): **gate RETIRED at the ad964f3 merge** — upstream re-typed `GLSUBTILE_EnsureCapacity` to return the live index; the old `nTexCacheIdx = (short int)nNodeIdx0` grep no longer applies. If 0x112 returns, audit upstream's `GLBLOCKS_LoadWallSubtileTexture` miss path first. |
| Black-triangle grid on title screen | fan slot order (defect 6) — do *not* blame the wrapper (attribution retracted) |
| Foreign-module crash in `d2boot.log` | crash-triage stack: VEH skip paths dump module+offset; symbolize via linker map |
| New "retail right, rebuild wrong" color issue | start from the §1 root pattern; audit decompiler guesses around the site; then capture (§6.1) |
| Title/in-game music silent (again) | defect 12 relapse: `rg '\(double\)0x[0-9a-f]{16}' D2Sound/Src/D2SoundSmp.cpp` must be empty. Probe marks retired — re-cut before assuming (HISTORY H4). |
| NPC/monster/player SFX silent, music fine | fix 16 relapse: `grep -c "vanilla-plus fix 16" D2Client/GAME/Sound.cpp` → 1 (the `uint32_t nCalcVolume / nCalcPan` block; a `(uint32_t*)&fFalloffMin` pun there = bug is back). SMP layer stays exonerated — do not re-audit. |

## 13. Sandbox gotchas

- No `gh` CLI/token — PR/issue body edits are handed to the user.
- `git apply` needs `--no-index` and `GIT_CEILING_DIRECTORIES=/home/z/my-project` (the clone has no clean parent boundary); note `git apply --no-index` OUTSIDE a repo silently SKIPS hunks with exit 0 — use a real scratch repo for QA.
- A dirty worktree means *new* local work: inspect it, don't blanket-revert.
- The sandbox resets between sessions — persist anything load-bearing in the repo itself (that is why this handoff and HISTORY.md are committed in-tree).
- Script Persistence Rule: scripts >10 lines go to `scripts/` and get edited in place on failure.
- **No mingw toolchain in the sandbox and no apt rights** — constants-level fixes are verified by diff review + curve math, not by compiling; the user's local Win11 build is the compile gate.
- tmpfiles.org links expire in **60 minutes**; always post the extracted `/dl/` token link and byte-verify.
- User timezone America/Toronto; session language English; delivery zips always to `/home/z/my-project/download/`.
