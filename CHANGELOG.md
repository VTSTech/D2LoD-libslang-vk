# Changelog — D2LoD Vanilla+ (`d2vp.exe`)

All notable changes to the Vanilla+ client and the Glide3xVK wrapper, per shipped
release. Working rounds between releases carry build self-IDs `Vanilla+0.0.x-Ryy` — the
first `d2boot.log` line pins any shipped exe to its source round. The engineering-level
round journal lives in [`handoff.md`](handoff.md) (current) and
[`HISTORY.md`](HISTORY.md) (verbatim archive, H1–H39); per-round summaries for the
0.0.6-era releases are also on the
[GitHub releases page](https://github.com/VTSTech/D2LoD-libslang-vk/releases).

Started at handoff rev 110 (2026-11-02); the 0.0.1–0.0.6 entries are distilled from the
GitHub releases page and the H-records (the README release table this superseded was
retired in README trim #1, rev 111 — verbatim in [`HISTORY.md`](HISTORY.md) H32).

## [Unreleased]

- Nothing yet — the next round's entry goes here.

## [R94] — 2026-11-03 (issue #26 fix round 3; self-ID `Vanilla+0.0.7-R94`)

Track S follow-up. The R93 field pass came back **partially fixed** — owner:
"with it off. both right and left individually are good. both panels, still
black. there is a little bit of black on the bottom of the screen with
perspective mode off. with perspective mode on, left side is ok, right side
still has black triangles at edge. same black outside panels with both
panels." Round 3 found the two remaining members of the family: the world
draw pass itself is skipped for border 3, and the panel-cull family has
**vertical** (bottom-band) members nobody had touched.

### Root causes (all confirmed against the R93 field report)

1. **The whole world draw is skipped with both panels open.** `Game.cpp`
   calls `ViewDraw2` — the underfloor/floors/shadows/walls/roofs pass — only
   when `eBorderType != 3`. Retail can afford the skip: at 640x480/800x600
   the two panels cover the window edge to edge, so the world is invisible
   and the pass is a pure fill-rate save. The 800-class UI floats 240 px
   inboard at 1280x720, so the outer strips still show world — and with the
   pass never running they rendered as the untouched black backbuffer, on
   **every** renderer, with perspective on **and** off ("both panels, still
   black", "same black outside panels with both panels"). R93's border-3
   scene-list build in `GAMEVIEW_PrepareScene` was the right fix pointed at
   the wrong layer — it fed a pass that never ran.
2. **The bottom-band vertical cuts.** The same retail assumptions run
   vertically: the non-perspective wall/shadow builders reject any subtile
   anchored at `y >= h-0xf` (Direct3D `BuildShadowTileVertices` /
   `BuildWallTileVerticesNoPerspective`, Glide `DrawShadowTileSimple` /
   `DrawWallTileSimple`), and the perspective cull rects reserve the bottom
   `0x2f` px for the retail full-width bottom HUD (D3D
   `CalculateViewportRect`, Glide `CalculateScreenBounds`). At 640x480/800x600
   the bottom HUD covers those bands; at 1280x720 the 800-class HUD floats
   240 px inboard, the bottom corners show world, and the cuts rendered as a
   thin black strip along the screen bottom ("a little bit of black on the
   bottom ... with perspective mode off" — the `h-0xf` anchor cut is only
   live in the non-perspective paths, which is why the strip reports with
   perspective off). The perspective rect's reserved band also ate the
   shadow/wall geometry in the bottom corners — the likely shape of the
   perspective-ON right-margin triangles (the perspective near-field
   expansion magnifies the band at the screen edges).

### Fixed (wide layouts 1280x720/1344x700 only — retail modes byte-identical)

- **`D2Client/GAME/Game/Game.cpp`** — border 3 runs `ViewDraw2` on wide
  layouts (retail keeps the skip). The pass gates underfloor/shadows/roofs on
  the WALL2 pass mask that R93's scene-list build now sets, so both fixes
  land together as the both-panels world render.
- **Bottom band opens on wide dims** (same 1280x720/1344x700 test as the
  R92/R93 guards; the HUD/panels overdraw their own rects afterwards, and
  D3D/Glide clip the overdraw at rasterization): `D2Direct3D`
  `BuildShadowTileVertices` + `BuildWallTileVerticesNoPerspective` (`h-0xf`
  → `h+0x20`) and `CalculateViewportRect` (bottom `h-0x2f` → `h+0x20`);
  `D2Glide` `DrawShadowTileSimple` + `DrawWallTileSimple` (`h-0xf` →
  `h+0x20`) and `CalculateScreenBounds` (bottom `h-0x2f` → `h+0x20`).

Still deferred (cosmetic, unchanged from R92/R93): overhead party icons /
unit-name labels / alt-item labels in the margins under border 3
(`ui.cpp:1060-family`, `ShowItems.cpp` half-screen rejects), the border-3
right-click world-interaction guard (`Game.cpp` input path), and the software
renderers' bottom-band clears/clip table (`FillYBufferFast`, DDraw/GDI memset
of `h-0x2f` rows — the owner's D3D build never reaches them).

Verification: all 4 changed TUs compile clean under llvm-mingw i686 (clang
link stops at the known GCC-only `__gcclibcxx_demangle_callback` — unchanged
since R90b; the owner's MSYS2 build links it); gates **341/341** (3 new R94
pins: border3 world draw + the D3D/Glide bottom-band triplets); self-ID →
`Vanilla+0.0.7-R94`. Field ask: at 1280x720 — (a) both panels open: **both**
outer margins must render the world (no black band, no triangles), units in
the margins clickable; (b) the screen bottom (no panels, one panel, both): no
black strip at the bottom edge with perspective off, and the bottom corners
render world with perspective on; (c) single panels in perspective mode: both
margins clean. Retail 640x480/800x600 byte-identical.

## [R93] — 2026-11-03 (issue #26 fix round 2; self-ID `Vanilla+0.0.7-R93`)

Track S follow-up. The R92 field pass came back **half-fixed** — owner: "The
left side of the screen now has no more blackness when a panel is open, but the
right side still does. when both panels are open, it is still black on all
sides." Round 2 found the three members of the panel-cull family R92 missed.

### Root causes (all confirmed against the R92 field report)

1. **D3D defaults perspective OFF.** R92 fixed the *perspective* cull rect
   (`CalculateViewportRect`), but with the Perspective video option off (the
   default) wall/shadow tiles go through the **non-perspective** vertex
   builders — `BuildWallTileVerticesNoPerspective` / `BuildShadowTileVertices`
   — which carry their own half-screen cuts (`case 1 → right cut at w/2`,
   `case 2 → left cut at w/2-0x20`). That is the live path on the owner's
   build: with the inventory open the right margin still lost every wall,
   shadow and structure (the jagged "black triangles"); the left margin was
   inside the retail clip all along, which is why it looked fixed.
2. **Both panels open = the scene is never built.**
   `GAMEVIEW_PrepareScene` skips `WALL2_BuildSceneDrawList` entirely for
   border type 3 — a retail fill-rate shortcut that is exact only when the two
   panels cover the window (640x480/800x600). At 1280x720 the 800-class UI
   floats 240 px inboard, the outer strips still show world, and the skip
   rendered them as pure black backbuffer ("both panels … black on all
   sides") on **every** renderer.
3. **R92's case-2 walk flip broke the software wall pan.** In the
   non-perspective passes the wall draw position is `pView.left - nCenterX +
   tile anchor` (`Draw.cpp` dWallPass / WALL2 alpha / shadow passes) —
   `pView.left` is the **pan origin**, not just a walk bound. R92 flipped the
   case-2 view-rect left edge from `+q` to `-q`, which would have drawn
   software-path walls 640 px off the floor with the left panel open. (It is
   also why the left margin appeared fixed in the owner's R92 test: the
   misplaced wall span covered it.)

### Fixed (wide layouts 1280x720/1344x700 only — retail modes byte-identical)

- **Every non-perspective half-screen tile clip opens its margin side** on
  wide backbuffer dims (same 1280x720/1344x700 test as the R92 renderer
  guards; the panel art overdraws its own rect afterwards, so the full-width
  clip is safe): `D2Direct3D` `BuildShadowTileVertices` +
  `BuildWallTileVerticesNoPerspective`; `D2Glide`
  `CalculateScreenBounds` (non-perspective branch) + `DrawShadowTileSimple` +
  `DrawWallTileSimple`; `D2DDraw` and `D2Gdi` `DrawShadowTile` /
  `DrawTransWallTile` / `DrawWallTile`.
- **`D2Client/GAME/View.cpp`** — border 3 builds the scene draw list on wide
  layouts (retail keeps the skip: byte-identical + fill rate).
- **`D2Client/UI/Path.cpp`** case 2 — the wide view rect keeps the retail
  `+q` **left** anchor (the software wall pan stays correct) and rides the
  extra span on the **right** edge (`w+3q`, same `w+2q` width the WALL2 grid
  column count and rcCull derive from); the floor-side left-margin coverage
  still rides the `rcExtendedView.left` widening R92 added.

Verification: all 7 changed TUs compile clean under llvm-mingw i686 (the
clang link stops at the known GCC-only `__gcclibcxx_demangle_callback` symbol
— unchanged since R90b; the owner's MSYS2 build links it); gates **338/338**
(6 new R93 pins + the site-5 summit-clouds classifier count 1 → 2); self-ID →
`Vanilla+0.0.7-R93`. Field ask: same test as R92 — at 1280x720 open the
inventory alone, a left panel alone, then both: **both** outer margins must
render the world with no black band and no jagged triangles in every state,
units in the margins clickable.

Field result: **PARTIAL** — owner build + 720p test: "with it off. both right
and left individually are good. both panels, still black. there is a little
bit of black on the bottom of the screen with perspective mode off. with
perspective mode on, left side is ok, right side still has black triangles at
edge. same black outside panels with both panels." The non-perspective fixes
landed (both single panels good with perspective off); three cases remain —
the border-3 **world draw** pass is still skipped (the R93 scene-list build
fed a pass that never ran), the bottom-band vertical cuts (builders cut
wall/shadow anchors at `h-0xf`; the perspective rects reserve the bottom
`0x2f` px for the retail full-width HUD), and the perspective-ON right-margin
triangles — fixed in [R94].

## [R92] — 2026-11-03 (issue #26 fix round; self-ID `Vanilla+0.0.7-R92`)

Track S — "[720p] With only Left or Right UI Panel open, Black triangles on that
side of screen. Both panels. all black outside of panel" — root-caught and fixed.

### Root cause

Every panel-open adjustment in the world pipeline is a fraction of the **window
width**: the tile walk narrows to the free half (`Path.cpp` view rect), the
perspective renderer culls a centered half and re-pans the world by a quarter
window (`RENDERER_D3D_CalculateViewportRect` / `RENDERER_GLIDE_CalculateScreenBounds`
+ the `±ScreenSizeX/4` light-grid/sprite pans), the unit-click reject halves the
screen (`CUnit.cpp`), and the warp fade / weather clamps follow. That math is
tuned for 640x480/800x600, where the open 800-class panel reaches the screen
edge. The Vanilla Plus wide modes reuse the 800-class UI as one rigid 800 px
unit **centered** on the playfield (240 px margins at 1280x720), so the panel
edges move inboard (screen x 640/1040) while every cull still assumes the
edge-hugging retail geometry — the 240 px strip beyond the open panel is culled
to the black backbuffer ("all black outside of panel"), and the tile/wall cut
lines leave jagged unrendered edges ("black triangles"). Longstanding 720p
behavior, not a regression: the R87→R89 syncs touched none of these files.

### Fixed (wide layouts 1280x720/1344x700 only — retail modes byte-identical)

- `D2Client/UI/Path.cpp` — the tile walk keeps the full panned span
  (`[-q, w+q]`) and `rcExtendedView` extends to match, so the walk reaches the
  outer-margin tiles; the world pan (`MouseYRelatedB`) is untouched — the
  mouse→world mapping rides the same offset.
- `D2Direct3D/Renderer/Direct3D.cpp` + `D2Glide/Renderer/Glide.cpp` — the
  perspective cull rect keeps the inner-side quarter cut (lands exactly on the
  screen edge after the pan) and opens the margin side (`[q, w+q]` / `[-q, w-q]`).
  The renderers test their own backbuffer dims (no cross-DLL imports).
- `D2Client/UNIT/CUnit.cpp` — unit clicks are rejected only on the actual
  400 px panel rectangle, not the whole covered half; units in the margins are
  clickable again (with one or both panels open).
- `D2Client/D2Client.cpp` — the town-portal fade spans the full width.
- `D2Client/Draw/Weather.cpp` — rain/particle clamps and the dark rain sheet
  drop the half-screen clip (the bubble pan stays, consistent with the world).

Known follow-ups (cosmetic, deferred to the next round): overhead party icons /
unit-name labels use the same half-screen rejects in a few UI paths
(`ui.cpp`, `Draw.cpp` alt-items under border 3), so labels in the margins may
lag one round behind the fixed geometry.

Verification: all 7 changed TUs compile under llvm-mingw; gates re-run
**332/332** (6 new issue-#26 pins + the site-5 client classifier count 1 → 2);
self-ID → `Vanilla+0.0.7-R92`. Field ask: at 1280x720 open each panel alone and
both — the outer margins must render the world (no black strip, no triangles),
units there clickable, rain reaching the margins.

Field result: **PARTIAL** — owner build + 720p test: "The left side of the
screen now has no more blackness when a panel is open, but the right side still
does. when both panels are open, it is still black on all sides." Three family
members missed (the non-perspective half-cuts, the border-3 scene skip, the
case-2 pan-anchor flip) — fixed in [R93].

## [R91] — 2026-11-03 (docs + field-verify round; self-ID `Vanilla+0.0.7-R91`)

The field-verify round for the R90/R90b line — the owner's MSYS2 MINGW32 build of
R90b went green and the game runs: **"Skeletons are finally white again."** One
report, two closures:

- **R90/R90b field verification** — both R90b build blockers (the stale
  `D2UnitFindDataStrc.h` include and the `RENDERER_D3D_DrawGlyph` splice) confirmed
  fixed on the owner's Windows build; the third upstream sync (Ghidra DB 1855) is
  field-run green. Tags `R90` @ `fff470d7` and `R90b` @ `3726f3ce` cut.
- **Track C / issue #8 RESOLVED in the field** — necro minion skeletons spawn and
  stay white: the R50 roster-recolor polarity, the R51 spawn-polarity write, and the
  R89 component-model hardening (plus the `0x16b`/`0x16c` palshift polarity force)
  all confirmed together on one field build. GitHub close pending the owner; tag
  `R89` @ `4014ef65` cut.

Docs-only round otherwise: self-ID → `Vanilla+0.0.7-R91` (`Application.cpp:66`),
gate self-id row re-pinned, gates **326/326**, SMOKE-CHECKLIST known-fail note
retired to a standing regression check, handoff rev 121. Delivery note: regular
rounds return to the overlay-zip workflow (owner applies as own commits) — git
bundles are reserved for upstream-merge rounds.

## [R90] — 2026-11-03 (merge `77e87e02`, self-ID `Vanilla+0.0.7-R90`; FIELD-RUN GREEN 2026-11-03 — tags `R90` @ `fff470d7` + `R90b` @ `3726f3ce`)

The third upstream sync of the 0.0.7 line: upstream `bc47c6bc..57380e21` (2 commits,
Ghidra DB 1851 → 1855) merged with the Vanilla+ surface intact. Byte match 6,242 →
6,273 of 14,642 functions (42.84% of fns), 19.51% of bytes. Conflict surface: 166
upstream-changed files, 3 conflicts (9 hunks). The supersession ledger grows by one
entry: upstream rebuilt the D3D ground-tile index buffers in retail form, superseding
the fork's fixes 24 and 25.

### Synced

- Upstream `03877005` — tables, float vertices, remainders, loops (160 files, the
  broad regen sweep to 6,273 matching fns); `57380e21` — monster palette path and
  neighbour-anchored stack addresses.
- `D2Game/D2UnitFindDataStrc.h` deleted upstream (structs relocated to
  `D2Common/Unit.h` + `D2Common/Units/UnitFinds.h`); the fork's delta was EOL-only,
  so the deletion was accepted wholesale.
- README keeps the fork's two-gate description and takes upstream's Ghidra DB 1855.
- `SkillNec.cpp` carries both sides: upstream's regen changes plus the R89 component
  model — verified zero `szCharName` component writes remain.

### Superseded (upstream wins; fork notes kept in code)

- **vanilla-plus fix 24 (D3D fullscreen black ground) + fix 25 (perspective floor
  diagonal bars)** — upstream `03877005`/`57380e21` rebuilt `DrawGroundTile` and
  `InitGroundTileCache` with the retail index-buffer model: the stack
  `aStripIndices[60]` prefill copied into a properly-extent
  `gpD3DGroundTileVertexBuffer`, and the low-quality fan as the static
  `gdwRendererMode[4] = {0,5,0x23,0x1e}` (byte-match verified — the retail fan's
  4th vertex is 0x1e=30, not the fork's hand-derived 28). The fork statics are
  retired; a dated supersession note sits at the old site. Fixes 18/22/27 in the
  same file were checked and are NOT superseded.
- Gate re-pins (R90-merge notes): R51 spawn palshift polarity raw=1 → raw=2 (R89's
  skeleton-class site is a second good site, not a regression); the self-id row
  re-pinned to the current string.

### Build fixes (R90b — owner field-build report, 2026-11-03)

The owner's first Windows build of R90 failed; the fix round also caught a second
blocker their `-j8` build had not reached yet:

- `D2Client/UI/VanillaPlusCore.cpp:86` still included the deleted
  `D2Game/D2UnitFindDataStrc.h` — the R90 merge accepted the deletion wholesale but
  missed the kitchen-sink include block. No VP code uses the struct; the relocated
  definition ships via the already-present `D2Common/Unit.h` (line 82). Include
  dropped.
- `D2Direct3D/Renderer/Direct3D.cpp` `RENDERER_D3D_DrawGlyph`: the R90-merge
  resolution had spliced the fork's out-of-range remainder block back in — it
  references the float globals (`gfD3DFontVertex*`) that the upstream regen deleted
  (GCC-real: undeclared identifiers, array-assign errors). Replaced with upstream's
  clean `return (int)g_pFontTextureBuffer[0]` tail — semantics identical to the
  fork's old `>= 0x60` early return; dated merge note in place.
- Cross-verified in the sandbox with llvm-mingw: whole-tree include audit (1,100
  files, 14,915 quoted includes → 0 unresolvable after the fix); all objects
  compile; `d2.exe` links (pei-i386). The only sandbox-link delta vs GCC is
  `__gcclibcxx_demangle_callback` (libstdc++-internal, GCC-only — expected under
  clang, present in the owner's MSYS2 GCC); the `Fog/Engine/Application.cpp`
  fastcall-init diagnostics are a clang canonicalization quirk that reproduces
  identically on the fork-R89 parent — GCC-real: none.
- Self-ID → `Vanilla+0.0.7-R90b`; gates re-run **326/326** (self-id row re-pinned).
- **Field-verified 2026-11-03** — see [R91]: the owner's MSYS2 build of R90b went
  green and the game runs ("Skeletons are finally white again"; issue #8 resolved
  in the field).

## [R89] — 2026-11-02 (owner-applied @ `4014ef65` = tag `R89`; FIELD-VERIFIED 2026-11-03 riding the R90b field run)

The monster-components data-model fix, owner-applied on the fork:

- Raised-skeleton/monster component bytes now write to the monster unit-data union
  (`pUnitDataMonster->Components[0x10]`, `->pAiGeneral`) instead of overlaying
  `pUnitDataPlayer->szCharName[4..]`; null guards added
  (`SKILLNEC_SetSkeletonComponentsByMasteryLvl`, `D2Game/UNIT/Monster.cpp`).
- Skeleton classes `0x16b`/`0x16c` get their palshift.dat ownership polarity forced
  from the live hostility check regardless of `byUtrans`
  (`D2Client/UNIT/CUnit.cpp`) — issue #8 hardening on top of the R51 fix.

## [R88] — 2026-11-02 (merge `1d401785`, self-ID `Vanilla+0.0.7-R88`; tag pending the owner's field verify)

The second upstream sync of the 0.0.7 line: upstream `c8445e73..bc47c6bc` (9 commits,
Ghidra DB 1839 → 1851) merged with the Vanilla+ surface intact. Byte match 6,232 →
6,242 of 14,642 functions, 19.50% → 19.54% of bytes. Conflict surface: 462
upstream-changed files, 55 two-sided, 22 conflicts (27 hunks). This release OPENS THE
SUPERSESSION LEDGER — upstream recovered the retail forms of two fork fixes, and the
fork takes them ("upstream wins"), keeping the root-cause notes in code. Field verify
(owner build + SMOKE) pending; per-release notes for R84–R88 live in
[`docs/release-notes-R84-R88.md`](docs/release-notes-R84-R88.md).

### Synced

- Types and prototypes from Commit-Will's, Niehztog's and Nugwa628's proposals; the
  third naming round with Mac cross-check types (`CLIENTMODE_WaitForQServerConnection`,
  `CLIENTMODE_WaitForServerShutdown`, `MAINMENU_OnPressSkipEmailAddress`,
  `Objects_InitFnNN_<role>`, `StaticInit_gpsz*`; 6,237 fns / 19.53% at that round);
  `QUESTLOG_HitTestCloseButton` returns bool; affix rollers take `bSpawnableOnly`,
  not `bIsSuffix`; `fnGetDatabaseFileTime` is `__fastcall(FILETIME *)`;
  `D2Sha1ContextStrc` is 0x60 bytes; quest-gossip, sound and missiles value fixes.

### Superseded (upstream wins; fork notes kept in code)

- **vanilla-plus fix 16** (Defect 13: SFX silent, music OK) — upstream's
  `D2FloatIntUnion` (store `.fValue`, read back `.nValue`) is the same root cause in
  the retail-faithful form; the fix-16 comment in `D2Client/GAME/Sound.cpp` now
  carries the supersession note.
- **The R54/R85 gossip hand-tables** ([#13](https://github.com/VTSTech/D2LoD-libslang-vk/issues/13)
  / [#22](https://github.com/VTSTech/D2LoD-libslang-vk/issues/22)): upstream
  recovered the retail tables verbatim — A1Q1 5→8, A1Q2 5→6, A2Q1 5→9 slots — plus
  the flavor string ids the fork had left empty rather than invent. All 39
  `aNpcGossipData` externs are now sized retail-exact (28 were unsized pre-merge),
  which RESOLVES the track-J watch (the ~17 singular-table quests). The
  `VP-BEGIN [gossip-table-fix history]` notes keep the lineage; the R53/R54/R85
  quest marks stay as field diagnostics.
- **Sound constants**: upstream's exact decoded decimals
  (`9.999999747378752e-05` = the float32 `0.0001` promotion) replace the fork's
  spellings; the `-2000.0` centibel diagnosis comment is preserved (upstream ships
  the same scale — fix 10 co-adopted, not superseded).
- Checked at this merge and NOT superseded: the issue #11 `[glide fix 11]` hook
  (still fork-side, re-planted at the R87 merge; the glide3x.dll wrapper ships from
  the `Glide3xVK` branch, which upstream does not carry).

### Changed

- Gates: 10 stale expectations updated to the superseding state with R87-merge notes
  (the epsilon quartet, the `-2000.0` `(long double)` cast, gossip sizes ×6,
  Bloodraven turn-in `0x5c` now expected ×2 — retail carries bMenu 0 and bMenu 2
  variants); the self-ID row repinned R87 → R88. Suite **326/326**.
- `D2GFX.cpp`: upstream's `uVar1` signed/unsigned model taken in
  `D2GFX_InitializeLineBuffer` (the fork-side comparison rewrite would have read an
  uninitialized variable).

## [R87] — 2026-11-02 ([`R87`](https://github.com/VTSTech/D2LoD-libslang-vk/releases/tag/R87) @ `486b936c`, self-ID `Vanilla+0.0.7-R87`)

The first upstream sync of the 0.0.7 line: merge `b2937091` (upstream
`55016505..c8445e73`, 18 commits, Ghidra DB 1806 → 1839). Byte match 6,029 → 6,232 of
14,642 functions, 18.40% → 19.50% of bytes. **Field-run green** — the owner: "Finally,
builds, runs, no regressions." — and shipped as tag `R87` the same day.

### Synced

- Ghidra 1807–1809 packet/roster/monster/subtile parameter regen and byte-writer
  void returns; generator rounds (loop gate + signed division, folded-store arm
  order, range tests as `lo <= x && x <= hi`); Ghidra 1810 SkillDesc/OOG types;
  Ghidra 1811–1813 zero-init and signed compares; the VC8 harness (vtable
  `__thiscall` casts, per-unit include manifests, response-file path) + CI deps
  (upstream PR #32); dtchitt issue #37–#76 names; roster party ids `uint16_t`
  throughout (joining a game no longer hangs in the party-grouping loop); names for
  3,500 variables/fields + Storm PowerPC names + the D2Hell file; three value-bug
  fixes (wide char literal, byte-mask select, Win32 calls shadowed by thunks) with
  VC8 near-miss type fixes.
- Merge-side latent fix: `DRAW_ShouldDrawEdges` had returned 0 unconditionally since
  the 0.0.6 merge port — wide modes never drew level edges; caught and restored
  during resolution.
- Conflict surface: 1,113 upstream-changed files, 136 two-sided, 86 unmerged paths;
  the D2Hell rename cluster resolved onto upstream's layout with the `[d2boot]`
  CreateProcess marks re-planted; every other fork surface (wide-Y offsets, `VPRES`
  classifiers, D3D fix-22/24/25/27, the sound decoded doubles) re-planted. Gates: 2
  rows updated (the R84 tooltip varargs pin for upstream's typed renames; the
  fork-api include pin replaced by a `D2Hell.cpp` CreateProcess tripwire). Suite
  **326/326**.

## [R86] — 2026-11-02 ([`R86`](https://github.com/VTSTech/D2LoD-libslang-vk/releases/tag/R86) @ `3ca2da40`, self-ID `Vanilla+0.0.7-R86`)

### Added

- `-nointro` command-line switch: skips the boot intro videos — the Blizzard and
  Blizzard North logo .biks that play on every launch plus the auto-played
  title/first-run cinematics — by never entering cutscene mode (both boot/title
  entries take the retail begin-failed path straight to the next screen). The
  parse writes a `[vp] cmdline -nointro` mark to `d2boot.log`; in-game quest
  cutscenes are unaffected and boot without the switch is unchanged. Gates
  320 → 326. **Field-verified — the owner tested in-game 2026-11-02** ("R86
  verified, -nointro works"; SMOKE §K items 39–40 proven); shipped as tag `R86`
  the same day.

## [R85] — 2026-11-02 ([`R85`](https://github.com/VTSTech/D2LoD-libslang-vk/releases/tag/R85) @ `71f4772f`, self-ID `Vanilla+0.0.7-R85`)

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
  than invented), Kashya chain marks added. Gates 311 → 320;
  **field-proven — the owner closed the issue 2026-11-02** (state completed,
  same day). Track record: HISTORY.md H35–H36. (The hand-built table was
  superseded at R88 by upstream's retail-exact table — see [R88].)

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
("Vanilla+ 0.0.7 (R77)", 2026-11-01); further 0.0.7-line tags `R78`–`R86` mark the
fix rounds and `R87`/`R88` the upstream-sync rounds (the R88 tag follows the field
verify). Gate suite 204 (0.0.6) → 283 (first 0.0.7 build) →
311 (R84) → 326 (R86; unchanged through R88).

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
