# 720p branch — handoff for future work

*Rev 28 (2026-10-30, handoff refocused on the 720p effort — owner: "Let's rework the current handoff.md to focus just on 720p work. that is purpose of this branch"): the pre-720p content — revs 14–27 and the standing vanilla-plus sections (§1–§13) — moved verbatim to HISTORY **H24** (third offload, same pattern as H20 at rev 16); nothing rewritten, nothing dropped. Everything below is the 720p work: what this branch is, what landed, the live defect (in-game UI vertical misalignment at 720p, root-caused in-tree), the fix plan, and the field checklist. Engineering record: [`docs/resolution-widelayout-audit.md`](docs/resolution-widelayout-audit.md); gates: `tools/check-vanilla-plus.sh` ("site-5:" + "resfix:" rows).*

## 1. What this branch is

The `720p` branch adds a fourth stop to the in-game VIDEO OPTIONS Resolution row — **Vanilla Plus 1280x720** (resolution mode 4) — and renders the retail 800x600 UI class on it. The UI is *not* widened or relaid out: the exactly-800-px-wide retail interface (800CtrlPnl7 bar art, side panels, 800BorderFrame, and the `nScreenMode=1` D2Common layout tables inventory.txt / belts.txt) moves as ONE RIGID UNIT to the horizontal center of the playfield. The two fork-owned primitives (VanillaPlusCore.{h,cpp}) are:

- `VPRES_IsWideLayoutMode(m)` — TRUE for modes 2 (retail 800x600), 3 (dormant 1344x700, same UI class) and 4 (1280x720).
- `VPRES_GetWideUiOffsetX()` — `(ScreenSizeX - 800) / 2`, clamped ≥ 0. 0 at modes 0/1/2 (retail math byte-identical), **240 at 1280x720**, 272 at 1344x700.

`DRAW_UI` (UI/Draw.cpp :2064–2080) sets `gnPanelOffsetX = 80 + offset`, `gnPanelOffsetY = -60` per frame; bottom-anchored expressions (`gnPanelOffsetY - const + ScreenSizeY`, bar pieces at `ScreenSizeY`) follow the screen height automatically. DC6 draws extend **upward** from the y argument (Rev 19 postmortem in H20), so bar art anchored at `ScreenSizeY` renders bottom-of-screen at any height.

## 2. What landed (commits on `720p`)

| commit | what |
| --- | --- |
| `492644a` | sites 1–2: mode 4 exists end-to-end — ClientModeInGame equality guard, D2GFX mode/size table, D3D device first cut, MiniPanel tweak |
| `ddb2dbc` | Resolution slider row in VIDEO OPTIONS (EscMenu + Storm/_unnamespaced callbacks, Route-B "1280x720" label via blank-placeholder DC6 + font pick), VPRES primitives, site-5 gate stubs |
| `4ee3211` | site 5: the wide-layout audit implementation — gnPanelOffset ride in DRAW_UI, ~100 X-shift sites (ui.cpp / Inv.cpp / Panel.cpp / SpellSel.cpp / Help.cpp / QuestLog.cpp / ListBox), inventory.txt coord shifts + wide coord-cache key, audit doc, 25 "site-5:" gates |
| `0c52c70` | renderer dispatch fixes after the first field test — D3D modes 2/3/4 take the fully-initialized perspective=1 path; GDI windowed learns mode 4 (DIB 1280x720 + scale width); DDraw size tables extended (mode 3 = 1344x700, mode 4 = 1280x720) + 1280 render bounds; 5 "resfix:" gates |

## 3. Field status (2026-10-30)

Owner, first in-game round at 720p: **"it just started displaying at 720p. there is still a lot of UI and Menu alignment issues. I do have it built already."** Screenshot (windowed GDI): VIDEO OPTIONS centered and readable at 1280x720; world renders; bottom interface bar correctly bottom-anchored and horizontally centered; **belt potions floating one bar-height above the bar**; owner: **"same issue occurs with inventory panel"**. Diagnosis below — the X half of the site-5 shift works; the Y half is missing where Y is *not* bottom-anchored.

## 4. Live defect: the vertical half of the shift (root-caused in-tree)

Site 5's mechanism re-centers X only. Two Y anchoring styles coexist in the 800-class code, and they disagree at 720p:

- **Bottom-anchored** (`gnPanelOffsetY - const + ScreenSizeY`, bar/orb art at `ScreenSizeY`): self-adapts — everything shifts down by `ScreenSizeY - 600` (+120 at 720p). No action needed.
- **800-absolute table Y and top-anchored raw constants**: stay at their 800x600 values → land exactly one height-delta too high.

CONFIRMED — belt (the floating potions): every Belts.txt consumer in Panel.cpp shifts X and leaves Y raw. Item draw `DrawItemGfx(pItem, aRectItem[0] + offset, aRectItem[2])` (:2045), green/red slot highlights (:2024, :2041, helper :789), hotkey text (:2071), and the belt hit test `PANEL_GetPanelLayoutHeight` (:1471–1494 — Y pair never offset, so clicks target the phantom 800-space slots too). Bar art sits at 624..720; belt rects stay at ~545..604.

CONFIRMED — inventory: `VP_SHIFT_InvPageCoords / InvGridCoords / InvBodyCoords` (Inv.cpp :140–156) shift X bounds only; the panel art is bottom-anchored (4-tile `gnPanelOffsetY - 0xe0/-0x30 + ScreenSizeY` pattern, e.g. ui.cpp :5045–5054) and moves +120 → grid/body/page rects sit 120 px above their art. Same for hireling / trade / bank / cube / stash / npc-shop page+grid coords fetched in `InitializeiInventoryPages` (:159–286).

SAME CLASS, needs the same offset — top-anchored raw-constant sites (eyeball to confirm): 800BorderFrame pieces at raw Y `0x3f/0xfd/0x1e4/0x229` (Panel.cpp :1435–1443, :1460–1468); hireling name/stat texts (`0xd6 - gnPanelOffsetY` :5618, label rows :5729–5758, stat rows :5873–5875); trade window texts (`0x1b/0xf2 - gnPanelOffsetY` :9230–9276); stash gold text (:8881–8886); weapon-swap tooltip (`0x15 - gnPanelOffsetY` :8384); recipe scroll text (`0x82 - gnPanelOffsetY` :6727).

## 5. Fix plan (next round)

1. New primitive `VPRES_GetWideUiOffsetY()` in VanillaPlusCore.{h,cpp}, mirroring the X one: `ScreenSizeY - 600`, clamped ≥ 0 — 0 at modes 0/1/2 (byte-identical retail), +100 at mode 3, +120 at mode 4. Do **not** touch `gnPanelOffsetY` — bottom-anchored expressions already adapt; double-shifting is the failure mode.
2. Table rects: add the Y offset in `VP_SHIFT_InvPageCoords/GridCoords/BodyCoords` (Y members — grids consume `nTop` as the Y base) and at every Belts.txt consumer in Panel.cpp (Y pair `aRect[2]/aRect[3]`, i.e. nTop/nBottom — draw sites AND the hit test, or potions render right but click wrong).
3. Top-anchored constants: add the offset at the border-frame and text sites listed in §4.
4. Trap — coord cache key: `nCoordPageKey = nScreenMode + 2 * (0 < VPRES_GetWideUiOffsetX())` (Inv.cpp :175) cannot tell mode 3 (272/100) from mode 4 (240/120) — an in-game 3↔4 switch serves stale shifted coords. Key on both offsets (or the mode) while touching this.
5. Gates: add "site-6:" rows (Y primitive defined; VPRES_GetWideUiOffsetY counts in Inv.cpp / Panel.cpp); keep site-5/resfix counts green; refresh fork-surface.allowlist if touched-file hashes change.
6. Design note: default keeps retail's bottom margin (panels hug the bar) per the audit doc. If the owner prefers vertically-centered panels instead, the delta becomes `(ScreenSizeY - 600) / 2` — one constant, same call sites; decide before implementing.

## 6. Field checklist for the fix round (720p, windowed `-w` + fullscreen D3D)

- Belt: potions inside bar slots; click/use per slot; place-with-cursor; hotkeys 1–4 highlight; expansion rows (hover) stack above the in-bar row; belt-full arrows.
- Inventory: drop items in each grid corner; every body/equipment slot; gold drop button + coin draw; hireling panel (name, stat rows, slots); weapon-swap boxes.
- Other panels: stash / trade / cube / bank pages, waypoint, quest log (per border-type), help screen art+text, NPC shop tabs/buttons.
- Watch-items from the audit: esc/front-end menus at 720p, repair warning arrows, summit cloud tiling, cutscene mode round-trip.
- Regression: 800x600 byte-parity (same checklist — every offset must be 0 there) and 640x480 untouched.

## 7. Conventions (condensed)

Overlay workflow (sandbox edits the tree; owner applies as their own commits via llvm-mingw i686 Win11 build — `tools/build-local-win11.cmd`; the full build is the owner's gate). `VP-BEGIN/VP-END` markers on every fork edit in upstream files; fork-only TUs (VanillaPlusCore) keep upstream files free of new code. Every source change updates `tools/check-vanilla-plus.sh` gates + `fork-surface.allowlist`. Full pre-720p history: HISTORY H1–H24 (H24 = the offloaded handoff revs 14–27 + §1–§13).
