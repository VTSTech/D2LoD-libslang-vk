# Changelog — D2LoD Vanilla+ (`d2vp.exe`)

All notable changes to the Vanilla+ client and the Glide3xVK wrapper, per shipped
release. Working rounds between releases carry build self-IDs `Vanilla+0.0.x-Ryy` — the
first `d2boot.log` line pins any shipped exe to its source round. The engineering-level
round journal lives in [`handoff.md`](handoff.md) (current) and
[`HISTORY.md`](HISTORY.md) (verbatim archive, H1–H40); per-round summaries for the
0.0.6-era releases are also on the
[GitHub releases page](https://github.com/VTSTech/D2LoD-libslang-vk/releases).

Started at handoff rev 110 (2026-11-02); the 0.0.1–0.0.6 entries are distilled from the
GitHub releases page and the H-records (the README release table this superseded was
retired in README trim #1, rev 111 — verbatim in [`HISTORY.md`](HISTORY.md) H32).

## [Unreleased]

- Nothing yet — the next round's entry goes here.

## [R103] — 2026-11-04 (diag retirement round — self-ID `Vanilla+0.0.7-R103`)

`d2light-diag` and `d2drop-diag` are retired — the lights and drops defects
they were built to pin have been field-stable for many releases (fix-8/13-era
tools; the gold-drop blue-channel question closed in 0.0.2). Removed:
`D2Client/Draw/LightDiag.{h,cpp}`, `D2Common/DATATBLS/DropDiag.{h,cpp}` and
the surviving hooks — the OBJECT_SetLightRGB probe block + include in
`D2Client.cpp` and the ground-grid / unit-sample probes + include in
`Draw.cpp` — both upstream TUs return to upstream-verbatim at those sites.
This round also unblocks the first post-R102 owner build: LightDiag.cpp was
the TU that failed it (it named `gnLightmapWorldMaxX/Y`, which upstream's
`962f88d5` per-TU restructure had made `static` inside LightMap.cpp).
`ShadowDiag` stays in-tree. Also removed the paste-ready
`docs/release-notes-R84-R88.md` (release notes live under the tagged GitHub
releases; README links dropped accordingly). No behavior change: the probes
were one-shot dumps, idle after their caps.

## [R102] — 2026-11-04 (the fifth upstream sync — upstream `94ec239d..cc23648d`; self-ID `Vanilla+0.0.7-R102`)

Merge round, no owner-visible behavior change intended: four upstream
commits (`b2886796` 36 more functions filed to their source files,
`0bcf07cc`/`cc23648d` msvc8 evidence bumps, `962f88d5` the structural one),
459 files, +93,760/−97,027, Ghidra DB → **1892**, byte-match now 6,493 of
14,642 functions (44.3%) / 20.54% of bytes.

### The restructure and what it moved

`962f88d5` ("define shared globals in their TU, declare them per TU") emptied
the aggregate `globals/*.cpp` definition dumps: `globals.h` shed ~2,231 lines
of externs, each definition moved to its owning TU, and per-TU `*.data.h`
headers now declare what other TUs name. That re-homed three Vanilla+ anchors,
all re-applied at the new sites and re-pinned by the gate suite:

- **The row-10 esc-menu table** — `gaEscMenuOptionsTable` is now a `static
  [6]` in `D2Client/UI/EscMenu.cpp` (was a `[6]` edit inside the old
  `globals/D2Client.cpp` dump): the Vanilla+ Options entry, the `0x6` header
  count, the `VanillaPlusOptions.h` include and the resolution-slider
  `UI_SLIDER_DrawResolutionValue` draw callback all moved with it.
- **The dismiss table + flag count** — `gUIFlagDismissModeTable[39]` (with the
  UI_CONFIG alias row) and `gnUIFlagCount = 0x27` now live in
  `D2Client/UI/ui.cpp`, declared in `ui.data.h` (grown to `[39]` to match);
  the struct grow (`anDismissMode[39]` + `BOOL VanillaPlus` @0x98) stayed in
  `globals.h` untouched.
- **The six class sound tables** — upstream itself moved Barbarian, Necromancer,
  Paladin, Sorceress, Assassin and Druid into `D2Client/UNIT/UnitSnd.cpp`
  (Amazon stays at its retail site in `globals.cpp`); all seven first values
  verified byte-identical to the R99-recovered forms, and the per-class value
  gates re-pointed accordingly.

Also absorbed: 14 fork-touched files whose only delta was line-ending churn
took upstream's side wholesale; the eight real consumer conflicts were the
include blocks (our `d2_boot.h` / `VanillaPlusOptions.h` includes kept beside
upstream's new `.data.h` includes); the palette counter's explicit 39-byte
zero-init re-applied in `D2WinPalette.cpp`; the stale fix-17
`IID_IDirectDrawGammaControl` orphan was correctly dropped — the QI riid is the
in-place blob bytes (`69C11C3E-B46B-11D1-AD7A-00C04FC29B4E`), byte-identical
fork and upstream, so the typed orphan (unreferenced, and carrying stale bytes)
is superseded, not lost.

Gates stayed at **380/380** — no checks added or removed, but a dozen gate
file-pointers re-pinned to the post-restructure homes (each annotated
`[R102 merge]` in the script). R98's flat-wall probe, the R100 unit-pan
consumers and the R101 Options row all still pinned green.

## [R101] — 2026-11-04 (issue #26 round 9: the Options-page row — `no_panel_shift` is no longer ini-only; self-ID `Vanilla+0.0.7-R101`)

Field ask against the R100 build (owner): "Great job, no_panel_shift is
working as intended now. Let's add the toggle to the Vanilla+ Options panel."

### The row

`Esc → Options → Vanilla+ Options` grows a seventh row, **No Panel World
Shift**, wired exactly like the R67 Shared Stash row: one `gaVanillaPlusRows`
entry riding the existing `D2VP_GetFeatureToggle` / `D2VP_SetFeatureToggle`
bridge. Nothing else in the toggle chain needed touching — the ini template
block shipped with the feature itself (round 6), and `D2VP_ConfigReloaded`
already busts `dwNoPanelShiftLastCheck`, so a flip in the panel re-reads the
key on the very next `PANELSHIFT_NoShiftWorld` poll: the world stays put the
same frame you flip On, and slides again the frame you flip Off. No ini edit
needed, ever.

### The layout

Seven feature rows overflow the page box at the R67 geometry: row 7 spans
boxTop+0x110..0x12a while the Previous Menu row sat at boxTop+0x124 — a 6 px
overlap. `VPPAGE_BACK_ROW_Y` moves 0x124 → 0x144, keeping the same one-row
0x1a gap above the back row; its bottom edge (0x15e) stays inside the
0x171-high box (0x13 margin). Hit-test, hover highlight and draw all share
`VPPAGE_RowTopY`, so the one constant moves all three in step.

The label documents the feature's own boundary by being inert where the
feature is: at 640x480 / 800x600 the row flips the key but
`PANELSHIFT_NoShiftWorld` stays false outside wide layouts (the vanilla slide
is kept, exactly as the ini template's comment has said since round 6).
`no_panel_shift=0` behavior unchanged everywhere. Gates 380/380 (377 + 3 new:
row present, seven rows, back-row geometry).

## [R100] — 2026-11-04 (issue #26 round 8: the unit-sprite pan follow-up — `no_panel_shift` keeps units at the border-0 camera; self-ID `Vanilla+0.0.7-R100`)

Field report against the R99 build (owner): "clean build successful.
no_panel_shift is still missing. with it set to 0 shifts normally, when set
to 1, player, npc, campfire get pushed into wrong locations."

### The decode — the R99 merge re-armed the unit-sprite pan

The R95 feature was verified green in Glide with perspective ON ("no
shifting is observed when opening a panel"), so the regression is real and
it came later: the upstream Ghidra 1864-1877 library rounds merged in R99
landed the **retail bodies** for the unit-sprite draw path
(`DRAW_WORLD_Unit_1`, `GFX_DrawUnitSelectionShape`, …). The R95-era copies
had no border handling at all (units rode the view rect, which the R95
border-0 view-rect fix already covers); the retail bodies compute the sprite
screen-x the vanilla way in the perspective branch:
`AdjustPerspectivePosition` + a hand-applied **±quarter pan keyed on the
REAL window border**. `PANELSHIFT_BorderForWorld` covered every terrain pass
(floor/wall/roof/shadow, weather, automap — R95) but not this path, so with
`no_panel_shift=1` the world stayed at the border-0 camera while players,
monsters, objects (campfires), their shadows, selection rings, hover text
and ground-item labels still panned one quarter sideways. `no_panel_shift=0`
stays correct because there the sprite pan matches the world pan.

### The fix — the unit path rides the same neutralizer

All 14 unit-anchored world-render consumers now route their border read
through `PANELSHIFT_BorderForWorld` (the 1/2→0 neutralizer; border 3 passes
through; real border untouched when the feature is off):

- `D2Client/Engine/Gfx.cpp` ×7 — unit sprites, state overlays, shadows
  (`GFX_DrawUnitShadow`, `DrawUnitShadows`), colored/selection shapes,
  `DRAW_WORLD_Unit_1`
- `D2Client/Engine/GfxUtil.cpp` ×1 — `GFXUTIL_GetUnitCofDirectionOnScreen`
- `D2Client/UI/Text.cpp` ×2 — `TEXT_DrawMonsterHoverText`,
  `TEXT_DrawObjectHoverText`
- `D2Client/UI/ShowItems.cpp` ×1 — ground-item label perspective anchor
  (the label cull keeps the real border, per charter)
- `D2Client/UI/ui.cpp` ×3 — mercenary HP bar, player name plates
  (`UI_DrawPlayerNameOverhead`), unit/warp tooltip anchor

UI/interaction consumers keep reading the real border (panel click rects,
`HitTestScreen`, label culls, mouse warps — charter rule). Gate suite: 5 new
R100 gates pin the call-site counts (377 total).

## [R98] — 2026-11-03 (issue #26 round 7: the D3D flat wall iterator wrap — ROOT CAUSE FIXED; self-ID `Vanilla+0.0.7-R98`)

The R97 guard fired on the first field pass — outcome (b), the mark IS the
root-cause data — and it came with one more artifact:

```
[334112828 ms] videoopt: perspective apply renderer=6 active=1 -> 0 [R96]
[334112828 ms] world: perspective now 0, view frame [R97]
[334112828 ms] d3d: perspective now 0, floor tile pass (panels=0 w=800 h=600) [R97]
[334112828 ms] d3d: subtile promote out-of-range idx=4097 max=1536 used=138 bufNull=0 subtile=1457C004 - healed to uncached [R97]
[334112828 ms] VEH: fatal exception 0xC0000005 at addr 00AEB792
[334112828 ms] VEH: AV READ at FF600060 (target of the faulting access)
```

### The decode — both numbers are structure-layout arithmetic

`idx=4097` and `FF600060` are exactly what a **+4-byte-shifted subtile
record** produces:

- `RENDERER_D3D_BuildWallTileVerticesNoPerspective` (the D3D flat wall
  builder) walks the tile's `pSubTiles` array by BYTE OFFSET: `_nParam4 += 0x14`
  for the 0x14-byte records. The decompiler typed that iterator `uint8_t` — it
  merged it with the dead `nParam4` argument slot; every sibling byte-offset
  iterator in the tree (glFloorCache/glSubtileCache node tables, Tilecmp, the
  SubTile loader, D2GfxHelper) is full-width `int`.
- A `uint8_t` accumulating 0x14 per iteration wraps at 256: after 12 subtiles
  (offset 0xF0) the 13th iteration reads at **+4**, and from there on every
  "record" is the real array shifted 4 bytes.
- The shifted record's `nTextureCacheIndex` (+4) lands on the real record's
  **DT1 `wFormat` word** (+8) → `idx=4097 = 0x1001` is a format word, not a
  cache slot. That is the word that halted line=200 in R96 (`nIndex >
  nMaxNodes`) and that the R97 guard then healed.
- The shifted record's `pPixelData` (+0x10) lands on the **next real record's
  `nOffsetX`/`nOffsetY` shorts** — a screen-offset pair, not a pointer. The
  heal fell into the alloc path (`D3D_DecompressRLESubtileToBuffer(buffer,
  pSubTile->pPixelData)`) and read through it: **AV READ at `0xFF600060` =
  `{nOffsetY=0xFF60 (-160), nOffsetX=0x0060 (+96)}` packed** — a textbook wall
  subtile offset pair. Lesson recorded: an out-of-range index word is proof
  the struct is not a valid subtile, so the R97 heal-then-reupload was itself
  unsafe for this class of poison (correct for a real subtile with a corrupted
  word, fatal for a fake record).

Why the toggle correlation is exact: perspective-ON walls
(`RenderWallTileWithPerspective`) and both shadow builders index
`pSubTiles[i]` properly — only the flat wall builder walks the raw byte
offset, and with the owner's registry defaulting perspective ON
(`videoopt: perspective load reg=1`) the flat wall pass runs for the first
time in the session on the first frame after a 1 → 0 flip. Tiles with ≤ 12
subtile records never wrapped; wider tiles did only when a shifted
pseudo-record's garbage coordinates passed the on-screen clip — which is why
this stayed latent since the original decompilation (`4b0154cd`) and why
earlier D3D perspective-OFF sessions lived.

### Fixed

- `RENDERER_D3D_BuildWallTileVerticesNoPerspective` (`Direct3D.cpp`): the
  byte-offset iterator is now `int nSubtileByteOffset` — the loop reads real
  records at any subtile count. Side effect: the silent wrong-record class the
  wrap could also produce (wrong-node MRU promotes, wrong-textured quads on
  tiles whose shifted format words happened to stay in range) is gone too.
- The R97 promote guards stay verbatim as defense in depth (dead-cache skip +
  out-of-range heal + the retail halts inside the promotes for unseen paths).
- Confirmation probe: tiles with **more than 12** subtile records — exactly
  the wrap population — log `d3d: flat wall subtiles=<n> tile=<p> [R98]`
  (first 8 per session).

### Field ask (SMOKE §L item 48)

On R98 in D3D: toggle Perspective both directions. Expected: **no death and no
`healed to uncached` marks**; a handful of `flat wall subtiles=` marks (n > 12)
is the confirmation data — send the bootlog tail either way. Any surviving
guard mark on R98 would be a second, independent corruption. With the toggle
surviving, finish the #26 D3D matrix (items 41–45): both directions, single
panels static, right margin clean, no bottom strip, both panels.

Verification: changed TUs compile clean under llvm-mingw i686 with the CMake
forced-include set (`scripts/compile_check_r98.sh`, 7-TU suite); gates
**364/364** (2 new rows: the widened iterator + the wide-tile probe; the
self-ID row re-pinned R97 → R98).

## [R97] — 2026-11-03 (issue #26 round 6: the D3D texture-cache promote guards; self-ID `Vanilla+0.0.7-R97`)

The R96 probes did their job on the first field pass. The owner reproduced the
D3D perspective-toggle death at **800x600** and sent the full bootlog tail plus
the Redivivus report:

```
[328963515 ms] world: first perspective frame enter (renderer=6) [R96]
[328963515 ms] d3d: first perspective DrawGroundTile (panels=0 w=800 h=600) [R96]
[328963750 ms] world: first perspective frame drawn [R96]
[328970062 ms] videoopt: perspective apply renderer=6 active=1 -> 0 [R96]
[328970062 ms] halt report: line=200 addr=00F0C70C
```

HALT line=200 at d2vp.exe+0x21C70C — "Fog's unrecoverable-error halt", stack
frames clustered in the D3D renderer region, no access violation, and every
R96 one-shot probe already spent (the death is a *second* perspective event of
the session, in exactly the blind spot the one-shot design left open).

### What line=200 names

Exactly two sites in the tree halt with the literal 200, both in D2Direct3D:
`D3DFLOOR_PromoteNodeToMRU` (`d3dFloorCache.cpp:221`) and
`D3DSUBTILE_PromoteNodeToMRU` (`d3dSubtileCache.cpp:216`) — the retail range
check `nIndex > nMaxNodes` on the 1-based texture-cache slot a floor tile or
wall subtile claims. The crash fired within the same 15 ms tick as the slider
apply, i.e. on the first world frame drawn with perspective OFF after ~6.5 s
of play with perspective ON — and the perspective-OFF wall/shadow builders
(`BuildWallTileVerticesNoPerspective` / `BuildShadowTileVertices`) are precisely
the promote callers that had not run all session. The frame-index arithmetic
that makes the flip the trigger is unchanged: with perspective ON the
perspective builders promoted the same cache slots all session without a
halt, so the out-of-range word is runtime state, not the visible code path.

### The audit (every link retail-clean)

- **Toggle path is client-side only**: `UI_SLIDER_ApplyPerspective` = registry
  save + `D2GFX_SetPerspective` (flag flip) + `SetViewRectangleSize` (rects +
  flags); no renderer callback, no cache invalidation, no device re-init.
- **Cache lifecycle is single-generation at 800x600**: the floor/subtile
  texture caches exist only on the 800-class device path
  (`RENDERER_D3D_AllocateTextureCaches` with `g_bPerspectiveMode`), are freed
  only via `RENDERER_D3D_FreeTextureCaches` (branch always paired with
  `Free(1)`, which invalidates every tile/subtile index), and
  `AllocateTextureCaches`' only call site is inside `RENDERER_D3D_InitializeDevice`,
  which halts (0x50f) if the device is still initialized. `ClearCaches()`
  (`Free(0)`, the non-invalidating free) runs only on the exit-game path.
- **List machinery is correct**: the D3D unlink operates through
  `D2InventoryGridStrc`/`D2UnitStrc` field aliases over the 0x18-byte node;
  the alias offsets were verified against the node layout (`pRightArmItem`/
  `eAnimMode` → +0x10 = `pNext`, `pLeftArmItem`/`pUnitData` → +0x14 = `pPrev`),
  and the tail-branch `eAnimMode.ePlayerMode = Death` store resolves to 0
  (`Death == 0`) — identical in effect to the clean Glide twin
  (`GLFLOOR_PromoteNodeToMRU` et al., which the owner toggles without incident).
- **Allocation is in-range by construction**: indices come from
  `EnsureAvailableNode` = `pNode->nIndex + 1` over a buffer of exactly
  `nMaxNodes` nodes; `nMaxNodes` changes only in `Initialize` (once) and
  `Free` (→0), both audited; upstream's last ground-tile rounds (77e87e02)
  never touched the cache files.
- R92–R96 D3D diffs are clipping-rect logic that compiles to the retail
  branches at 800x600 (`RENDERER_D3D_IsWideUiLayout()` false); the R95
  neutralizer passes border 0 through untouched (options menu = border 0).

Conclusion: the promote range halt is real, reachable, and its precondition
(a slot word beyond `nMaxNodes` while ≥2 nodes are live) cannot be produced by
any static path in the tree — it is runtime state the source cannot name from
here (sandbox has no Windows D3D). So R97 changes the failure mode from
process death to a logged, self-healing degradation, and re-arms the probes so
a repeat still tells us everything.

### Changed: both promote callers validate before promoting (`d3dFloorCache.cpp`)

- **Dead-cache guard**: if the floor/subtile cache struct is dead
  (`pNodeBuffer` null or `nMaxNodes <= 0`) the draw is skipped
  (`d3d: floor texture cache dead ... tile skipped [R97]` /
  `d3d: subtile texture cache dead ... subtile skipped [R97]`, first 8 hits
  logged) instead of walking a null buffer or spinning the eviction loop.
- **Range heal**: an out-of-range slot word is logged with full cache state
  (`d3d: floor promote out-of-range idx=<i> max=<m> used=<u> bufNull=<b>
  tile=<p> - healed to uncached [R97]`, subtile twin likewise), the tile's
  index word is reset to 0, and the ordinary alloc path runs — fresh node,
  texture re-upload, frame continues. Worst case is one tile/subtile
  re-uploading its texture; the retail halts inside the two
  `PromoteNodeToMRU` functions remain for any path the guards do not see.
- The guards are the *only* callers of both promotes (grep-verified), so the
  line=200 halt is unreachable through the guarded entry points.

### Added: re-armed transition probes (the R96 blind spot)

- `world: perspective now <0|1>, view frame [R97]` — ViewDraw2, once per
  perspective state change.
- `d3d: perspective now <0|1>, floor tile pass (panels=<p> w=<w> h=<h>) [R97]`
  — D3D `DrawGroundTile`, once per perspective state change.
- Together with the R96 one-shots every flip now gets a fresh bootlog bracket:
  a future death reports apply → transition → (guard mark or halt) with no
  spent-probe ambiguity, and the guard marks name `idx/max/used/bufNull/tile`
  — the exact numbers the root cause needs.

### Field ask (unchanged shape, one new cell)

Reproduce the D3D toggle death on R97 and send the bootlog tail. Outcomes:
(a) **no death** — the heal absorbed it; the `healed to uncached` marks (if
any) are the root-cause data, send them; (b) **death persists with a guard
mark** — the mark's numbers are the root-cause data; (c) **death persists with
no guard mark** — the death moved off the promote path; the transition marks
say where. Either way D3D perspective toggling should now survive long enough
to finish the #26 D3D verification matrix (blackness + no-shift, both
directions).

Verification: 7 changed/new TUs compile clean under llvm-mingw i686 with the
CMake forced-include set (`scripts/compile_check_r97.sh`); gates **362/362**
(6 new rows: 4 guard rows + 2 transition-probe rows; the self-ID row re-pinned
R96 → R97).

## [R96] — 2026-11-03 (issue #26 round 5: the D3D perspective-toggle triage probes; self-ID `Vanilla+0.0.7-R96`)

**Zero behavior change — a diagnostics-only round.** The R95 field pass came
back green in Glide ("I didn't realize you added a gate to the fix. In Glide
the fix appears to work. no shifting is observed when opening a panel ...
All blackness appears fixed in Glide as well" — `no_panel_shift=1` closes the
single-panel slide AND the last #26 blackness case in the field, i.e. issue
#26 is field-green in Glide) but carried one new report: "toggling perspective
mode in D3D crashes the game ... Glide can toggle."

### Why this round ships probes instead of a fix

The report's own data argues the crash is not the R95 feature: the owner's
first message predates their ini edit (`Everything still moves when a panel is
open` — with `no_panel_shift=1` the world cannot slide, and in Glide it does
not), so that D3D toggle death happened with **the R95 code inert** — options
menu open means border 0/3 exactly as retail, every `PANELSHIFT_*` call site
returns its retail value, and the same added functions demonstrably run in
Glide without incident. Static review agrees: the toggle flips
`bPerspectiveEnabled` (`UI_SLIDER_ApplyPerspective`) and the next frame re-enters
the ordinary D3D perspective paths; nothing in R92–R95 touches a
toggle-transition-specific site, and the D3D halt guards (`exit(-1)` on
inverted rects / halted dispatchers) are retail. The two candidate surfaces —
a latent first-perspective-frame fault in the D3D renderer, or the owner's
emulated-DDraw environment (their registry-corruption episode is SMOKE §L) —
cannot be separated from here: the sandbox has no Windows D3D.

### Added: five one-shot d2boot.log probes (the R60-probe pattern)

- `videoopt: perspective load reg=<v> capable=<c>` — every video-options
  open; a garbage registry value names the §L corruption class instantly.
- `videoopt: perspective apply renderer=<r> active=<a> -> <v>` — the slider
  release itself.
- `world: first perspective frame enter (renderer=<r>)` / `world: first
  perspective frame drawn` — the ViewDraw2 bracket around the first
  perspective world frame in the process.
- `d3d: first perspective DrawGroundTile (panels=<p> w=<w> h=<h>)` — first
  D3D floor tile drawn with perspective active; the `panels` value also
  confirms what the R95 neutralizer delivers to the renderer.

Read-out matrix in `SMOKE-CHECKLIST.md` §L: death with no apply mark = menu
plumbing; apply without the world enter mark = UI draw with perspective;
enter without drawn = inside the world pass; both marks = steady state. A
`d2crash-*.txt` next to the exe (or its documented absence —
VANILLA-PLUS.md two-artifact rule) remains the primary artifact.

Verification: 4 changed TUs + the 2 R95 feature TUs compile clean under
llvm-mingw i686 with the CMake forced-include set
(`scripts/compile_check_r96.sh`); gates **356/356** (5 new probe rows; the
self-ID row re-pinned R95 → R96). Field ask (§L): reproduce the D3D toggle
death once on this build and send (a) the newest `d2crash-*.txt` or the
statement that none was written, (b) the tail of `d2boot.log` with the probe
lines, (c) whether the death happens at the slider release or after closing
the menu, and (d) whether both toggle directions die.

## [R95] — 2026-11-03 (issue #26 round 4: the no-panel-shift feature; self-ID `Vanilla+0.0.7-R95`)

Track S follow-up and a new Vanilla+ option. The R94 field pass closed the
both-panels case — owner: "both panels open, no more blackness. however right
panel still has some, left has none" — and surfaced the retail behavior behind
the whole bug family: "when i open the left (character) panel the game screen
moves everything, the world, my player, npc's. to the right, and when i open
the right (inventory) panel, everything moves to the left. not sure if this is
how it is supposed to be. **Can we open the panel without moving anything?**
Both panels open it matches the game world before i opened a panel. that seems
good." A D3D cross-check confirmed the leftover right-margin blackness is
renderer-independent ("right side blackness occurs in D3D as well, otherwise,
both panels open is fixed in D3D as well") — it lives in the shared panned
perspective path, not in any one renderer.

### Answer to the field question: yes, the slide is vanilla — and it is why #26 exists

Retail slides the whole world a quarter screen sideways when exactly one
panel is open (`TypeOfUiOverlayOverWorld`: `MouseYRelatedB = ∓ScreenSizeX/4`)
so the action re-centers in the strip the panel leaves uncovered; both panels
open = no slide — exactly what the owner observed. The assumption underneath
is that the open panel **reaches the screen edge** (true at 640x480/800x600,
where the slide is free). At 1280x720 the 800-class UI floats 240 px inboard,
so the slide drags world out from under the visible outer margins — and the
perspective projection anchors on the player and expands from the screen
center, so the pan rides **half a dozen independent channels** (MouseYRelatedB,
the pView walk rect + nCenterX width derivation, the four roof/floor
light-grid half-screen adjusts, the weather clip switches, the automap border
offset, the renderer quarter offsets). Every R92–R94 artifact lived on that
panned path; the both-panels case renders clean precisely because border 3
never pans.

### Fixed / added (wide layouts 1280x720/1344x700 only — retail byte-identical)

- **New opt-in feature `no_panel_shift`** (d2vp.ini `[features]`, default 0 =
  vanilla slide, 2 s hot-read, template entry, options-cache bust): while 1,
  a single open panel takes the **border-0 world pipeline verbatim** — the
  world renders exactly like the field-proven both-panels-open case and the
  panel floats over it. The world does not move. This is the fix for the
  remaining perspective right-margin blackness by bypass: the panned
  perspective path is no longer reachable with a single panel on the wide
  stops.
- Implementation keeps the renderers untouched: `Path.cpp` keeps the border-0
  camera (MouseYRelatedB = 0, full unpanned view rect, no rcExtendedView
  widening, world clicking stays active) and every world-draw call site reads
  its border type through the new `PANELSHIFT_BorderForWorld` neutralizer —
  `Path.cpp` (pan), `Game/Draw.cpp` ×6 (the floor nWindowBorderType + the
  roof/underfloor/alpha/dWallPass/dShadowPass origins), `Draw/Weather.cpp` ×3
  (rain clip switches), `UI/Automap.cpp` ×1 (automap pan) — so the renderers
  receive 0 and take their own border-0 full-screen path. The R92–R94
  renderer guards stay live for the vanilla slide (`no_panel_shift=0`).
  UI/interaction consumers (panel click rects, label culls, selection
  guards, the R92 CUnit click rejection) keep reading the real border type.
- Retail 640x480/800x600: the wide-dims gate is false, every call site sees
  its retail value — byte-identical.

### Also noted this round (owner environment, not a build bug)

- **D3D "didn't go in game" episode**: builds as old as R83 (owner
  smoke-tested) also failed until the registry was restored to fresh 1.14d
  install values — a corrupted D2 video-options registry value (owner
  suspicion: one of the slider values), not a code regression. Recovery +
  watch item recorded in SMOKE-CHECKLIST §L. Vanilla+ itself writes no
  registry values (d2vp.ini lives next to the exe; portable_save explicitly
  avoids the registry).

Verification: all 6 changed TUs compile clean under llvm-mingw i686 with the
CMake forced-include set (`d2_platform.h` + `support/d2_hand.h`; clang link
stops at the known GCC-only `__gcclibcxx_demangle_callback` — unchanged since
R90b; the owner's MSYS2 build links it); gates **351/351** (10 new R95 pins:
the ini key/reader/template, the dims gate, the camera pan gate, the border-0
view rects, the 6 world-draw origins, the weather clips, the automap pan,
the cache bust); self-ID → `Vanilla+0.0.7-R95`. Field ask (at 1280x720, with
`no_panel_shift=1` in d2vp.ini — the ini hot-reloads every 2 s, so it can be
toggled live): (a) single panels — the world, player and NPCs stay **exactly
where they were** when the character screen or the inventory opens/closes, in
perspective mode on AND off; (b) the right-panel perspective margin — no black
strip beyond the panel (this was the last #26 blackness); (c) both panels —
unchanged (already green in R94); (d) `no_panel_shift=0` — the vanilla slide
returns (regression guard); (e) retail 640x480/800x600 sanity pass if
convenient. The automap, rain and click-to-move must all line up with the
static world.

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

Field result: **MOSTLY GREEN, one case left + a feature ask** — owner build +
720p: "both panels open, no more blackness. however right panel still has
some, left has none ... with perspective off, there is almost no blackness
with either panel ... the world/player/npc's still move thou ... when i open
the left (character) panel the game screen moves everything ... Can we open
the panel without moving anything?" and, on D3D: "right side blackness occurs
in D3D as well, otherwise, both panels open is fixed in D3D as well." So: the
border-3 world pass and the bottom-band fixes landed (both-panels green in
both perspective modes, both renderers); the perspective-ON right-margin
blackness survives in the shared panned perspective path — and the field ask
that closes it is the no-panel-shift feature, fixed in [R95].

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
(owner build + SMOKE) pending; per-release notes for R84–R88 live under the tagged
[GitHub releases](https://github.com/VTSTech/D2LoD-libslang-vk/releases).

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
