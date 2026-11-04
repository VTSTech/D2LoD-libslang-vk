# Resolution site-5 audit: wide-layout UI consumers

Site 5 of the 1280x720 plan: audit every retail `D2GFX_GetResolutionMode()`
consumer, route the wide modes into the retail 800x600 UI class, and
re-center the exactly-800-px-wide retail UI on the wider playfield. Sites
1-4 made mode 4 exist end-to-end (registry slider stop -> ScreenSize -> D2GFX
table -> D3D device); this site makes the UI class follow.

Companion gates: `tools/check-vanilla-plus.sh` ("site-5:" rows, 25 gates).

## Mechanism

Two fork-owned primitives in `D2Client/UI/VanillaPlusCore.{h,cpp}`:

- `VPRES_IsWideLayoutMode(m)` - TRUE for modes 2 (retail 800x600), 3 (dormant
  1344x700, same UI class) and 4 (Vanilla Plus 1280x720). Single definition
  of "renders like retail 800x600".
- `VPRES_GetWideUiOffsetX()` - `(ScreenSizeX - 800) / 2`, clamped to 0 on
  narrower widths. 0 at modes 0/1/2 (retail math byte-identical), 240 at
  1280x720, 272 at 1344x700.

The retail 800-class UI is exactly 800 px wide (interface bar pieces span
0..800; side panels and border frames tile 0..800), so wide modes move the
whole UI as ONE RIGID UNIT to the horizontal center:

- `DRAW_UI` (UI/Draw.cpp) sets `gnPanelOffsetX = 80 + offset` for wide
  modes. Nearly every 800-class panel expression reads
  `gnPanelOffsetX + const` (left panels) or
  `ScreenSizeX - gnPanelOffsetX - const` (right panels), so ~100 sites in
  ui.cpp / Inv.cpp / SpellSel.cpp / Help.cpp re-center with no per-site edit.
- Left-anchored bar elements add the offset; `ScreenSizeX`-relative right
  anchors subtract it (orbs, orb hover/text, belt arrows, skill icons and
  grids, help art/text, warning arrows).
- `gnPanelOffsetY = -60` unchanged: the 800-class bottom margin is measured
  from ScreenSizeY, so 720p lands correctly above the re-centered bar.
- The view/overlay shifts in `TypeOfUiOverlayOverWorld` derive from
  `ScreenSizeX / 4`, so the camera keeps the same player-to-panel
  relationship at any width.
- D2Common layout tables (inventory.txt / belts.txt `nScreenMode=1`
  variants, keyed by site-1's `nScreenMode = 1`) hold 800-absolute screen
  coordinates: `InitializeiInventoryPages` shifts every fetched rect's X
  bounds (`VP_SHIFT_InvPageCoords/GridCoords/BodyCoords`), and the coord
  cache key adds a wide-flag so an in-game 800<->1280 switch refetches.
  Belt rects from Belts.txt are re-centered at their Panel.cpp consumers.

## Consumer disposition (audit table)

| Consumer | Disposition |
| --- | --- |
| D2Win/Src/D2WinMain.cpp:747,788 | none - fully table-driven via `D2GFX_GetWindowSizeByResolutionMode` (site-2 table has mode 4) |
| D2Client/ClientModeInGame.cpp:390 | none - equality guard only |
| D2Client/D2Client.cpp:1399,1411,1439 (cutscene restore) | none - passes the current mode back through the D2GFX table |
| D2Client/D2Client.cpp `DRAW_ShouldDrawEdges` | wide classifier (level edges render like 800) |
| D2Client/GAME/Draw.cpp:452 (DrawEdges) | wide classifier |
| D2Client/GAME/View.cpp:733 (summit cloud row) | wide classifier (extra row drawn like 800) |
| D2Client/UI/ui.cpp automap overlay margins (:1124) | wide classifier (800 margin array) |
| D2Client/UI/ui.cpp stat/skill "+" buttons (20 gates: down/up handlers, draw + tooltips) | wide classifier - 800 input model is count-based, anchors are screen-center-relative, already dynamic |
| D2Client/UI/ui.cpp `INPUT_HighResLeftPanelClick` | wide classifier |
| D2Client/UI/Panel.cpp `PANEL_DrawCharacterInfo` (CtrlPnl7 interface bar) | new wide branch: same `Panel\800CtrlPnl7` pieces at 800-space offsets 0/165/293/421/549/683 + offset; retail 640/800 branches untouched |
| D2Client/UI/Panel.cpp orbs (health/mana), orb HP/MP text + hover regions | offset added (subtract on right anchors) |
| D2Client/UI/Panel.cpp skill icons, belt-full arrows, belt rects/highlights/hotkeys/hover, belt hit tests | offset added; expanded-belt hit test compares virtual-800 mouse X |
| D2Client/UI/Panel.cpp `DRAW_UI_BorderLeft/Right` | wide gate + offset (frames ride the panels) |
| D2Client/UI/Panel.cpp run/stamina/menu/minipanel/exp-bar | none - screen-center-anchored, self-adapting |
| D2Client/UI/Draw.cpp `DRAW_UI` | gnPanelOffset re-anchor block (see Mechanism) |
| D2Client/UI/SpellSel.cpp skill grids | offset on both grid bases (render IS the hit data) |
| D2Client/UI/Help.cpp | wide classifier + all xpac help art/text X shifted (border, bullets, close button draw + hit tests) |
| D2Client/UI/QuestLog.cpp | wide classifier (draw-position variants + panel-bottom margins) |
| D2Client/UI/Inv.cpp | wide classifier (`INV_HandleHighResDropClick`) + table-rect shifts + cache key |
| D2Client/UI/ListBox/ListBoxImplementation.cpp:878 | wide classifier (800 row metrics) |
| D2Client/UI/Waypoint.cpp (act tabs, row icons, open/close button, row texts, cancel tooltip, header) | wide classifier + Y offset on every top-anchored 800-space Y (input inversely virtualized); the row icons joined at R81 — the site-6 retrofit missed the two `waygateicons` draws and they rode retail Y at 720p ([#21](https://github.com/VTSTech/D2VP-QoL/issues/21)) |

## Watch-items (in-game verification)

- Main menus (D2Win/Fog side) at 1280x720 are out of D2Client's scope; the
  in-game UI is fully re-centered, the front-end may need its own pass.
- Quest log window internals use `gaQuestLogPanelRects` variants plus
  `(ScreenSizeX - 640) / 2` shifts; the variants follow the classifier, but
  pixel positions per border-type combination need eyeballing at 720p.
- `Draw_EquipmentNeedToRepairBody` warning arrows stay screen-edge-anchored
  (free-floating indicators, not bar art) - acceptable, verify visually.
- Summit clouds horizontal tiling covers 1024 px per strip; possible
  right-edge gap at 1344x700 existed in retail already; verify at 720p.
- Cutscene round-trips (`BeginCutSceneAtResolutionMode0` ->
  `SetResolutionModeWithEndCutScene`) restore via the site-2 table; verify
  a cutscene played at 720p returns to 720p.
- `Game.cpp` cutscene hooks that force modes 0/2 (pre-existing behavior)
  still need a play-through to see whether the configured mode should be
  restored instead.
- Glide path: the wrapper must accept 1280x720 from `grSstWinOpen` as-is
  (glide branch owner's side of the interface contract).

## Renderer dispatch fixes (post-field-test, 2026-10-30)

First in-game test of the 1280x720 stop exposed three renderer-side
defects the static audit had routed around (the client-side chain worked;
the device/buffer side did not follow). All three are the same disease:
per-renderer dispatch tables that retail only ever sized for modes 0-2,
with the dormant modes unreachable in practice.

1. **D2Direct3D fullscreen: black screen, audio alive.** Mode 4 took the
   `goto setup_device` branch (perspective=0) that retail only reached via
   dormant registry modes 1/3 - which its own else-branch clamped back to
   800x600, so the path was never actually exercised. The perspective=0
   branch of `RENDERER_D3D_AllocateTextureCaches` never initializes the
   floor cache, subtile cache, font system or
   `gnD3DTextureCacheBlockSize/Count` (the ground-tile VB init then divides
   by zero), so the world renders nothing while the game loop and sound
   keep running. Fix: modes 2/3/4 fall through to the same fully
   initialized perspective=1 device path as retail 800x600
   (Direct3D.cpp `RENDERER_D3D_InitializeDevice`).

2. **GDI windowed (-w): 800x600 content in the resized 1280x720 window.**
   The window resized correctly (D2GFX_ResizeWindow -> site-2 table), but
   `RENDERER_WINDOWED_AllocateDIBSection` clamped mode 4 to 800x600 and
   `RENDERER_WINDOWED_UpdateScaleFactor` kept the 800-wide render bound ->
   player right of center (640 px into an 800 px view), garbled interface
   bar (drawn below y=600), inventory corner sliver at the right edge
   (panel anchored at ScreenSizeX-width = 480..1280, clipped at 800). Fix:
   both functions learn mode 4 = 1280x720 / 1280-wide (Windowed.cpp).

3. **DDraw: out-of-bounds table read.** `InternalOpenWindow` indexed
   `gdwDirectDrawPaletteSizes[4]` / `gdwDirectDrawSurfaceWidths[4]` (both
   `[4]` arrays, index = requested mode) - mode 4 read past the arrays and
   mode 3 read the {0} sentinel. Fix: extended both tables to five entries
   (mode 3 = 1344x700 as documented, mode 4 = 0x500 x 0x2d0) and taught
   `RENDERER_DDRAW_SetHorizontalBounds` the full-width 1344/1280 render
   bounds (DirectDraw.cpp).

Glide needs no client-side change: `D2Glide` forwards the resolution mode
to the glide DLL (grSstWinOpen receives 1280x720) per the interface
contract with the glide branch.

Tripwires: `tools/check-vanilla-plus.sh` "resfix:" gates (5).

## Video-options label anchor (issue #23, R83)

The Route-B visible `1280x720` label (`EscMenu.cpp UI_SLIDER_DrawResolutionValue`,
the text stand-in for the stop-2 slot no MPQ asset exists for) sat wrong at
every resolution once it drew: it LEFT-anchored at the value-column X
(`ScreenSizeX/2 + 0xe6`) where `DRAW_EscMenu` draws the row's value images
with alignment 2 - cell RIGHT edge on that anchor - so the text ran a full
label width (~100 px) past the `HIGH`/`ON` column, and it drew at
`row bottom - 2`, which put its ink ~4 px above the neighbors' (screenshot-
measured at 720p, issue #23). Two engine conventions explain both halves:

- `IMAGE_DrawDC6ImageOnScreen` alignment 2 = right edge anchor (the function
  subtracts the summed frame width); the value DC6s bake ~4 px of transparent
  padding under their ink, so their glyphs end at `row bottom - 4`.
- `D2WinFont::DRAW_text`'s y anchors the glyph CELL bottom (DC6 frames draw
  upward from y - the convention `VANILLAPLUS_DrawRowLabel` documents); the
  picked font's digit ink sits ~6 px above that anchor.

Fix: right-align the text by subtracting the rendered advance width
(`DRAW_text0`, same metric the renderer advances by) and draw at
`row bottom + 2` (`RESLABEL_DESCENDER_COMP`) so the ink lands at
`row bottom - 4`, on the neighbors' baseline. The band math itself was
already correct (it reproduced the retail row geometry exactly - the
misanchor was purely in the text call). Resolution-independent: 800x600
picks up the same corrected placement with the same relative geometry.
Gates: "resolution: label right-align (#23)" + "resolution: label descender
comp (#23)".
