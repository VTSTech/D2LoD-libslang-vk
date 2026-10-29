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
