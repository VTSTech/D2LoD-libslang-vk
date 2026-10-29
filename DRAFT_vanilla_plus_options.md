# DRAFT_vanilla_plus_options.md — slate row 10 implementation draft (v2, 2026-10-29)

*Status: **IMPLEMENTED** (v2, 2026-10-29). Impl-round pins V1–V5 resolved
(§7); implementation landed — 11 retail files touched + 2 new TUs, sandbox
harness (verbatim-extract `g++ -fsyntax-only`) green, 12 grep gates green.
Remaining: real compile gate (owner Win11 llvm-mingw i686) + field loop V6.
Owner decisions (§6): **Route A** (runtime-synthesized DC6 label) + placement
**above "Previous Menu"** (row index 4 of the 6-row table). Charter:
VANILLA-PLUS.md. Slate: FEATURES.md row 10. Delivery: handoff §7.*

**Headline: the Esc-menu system is data-driven and the loader SKIPS rows
with an empty image path — so the new Options-page row needed ZERO loader
edits, the page mirrors the proven ControlConfig pattern (UI flag + font
text + own input batch), and the one novel piece (the synthesized label
DC6) runs the retail fixup over a retail-shaped raw image so the retail
free path tears it down unchanged.**

## 1. Verified anchor map

| # | Anchor | Evidence (scan 2026-10-29) | Grep gate (must hold at impl time) |
|---|---|---|---|
| A1 | **Options table** | `globals/D2Client.cpp:7304` `D2EscMenuItemStrc gaEscMenuOptionsTable[5]` (→ `[6]`); rows are plain initializers: type 0, path string, click callback (`&D2Client::UI::EscMenu::ESCMENU_*`), null init/draw/visibility. Item 3 = `CfgOptions` → `ESCMENU_ShowControlConfig`, item 4 = `Previous` → `ESCMENU_SwitchToMainMenu_alt` | `gaEscMenuOptionsTable\[5\]` in globals/D2Client.cpp → 1 |
| A2 | **Options header** | `globals/D2Client.cpp:7288-7294` `gaEscMenuHeaderOptions` = `{nItemCount 0x5, nVerticalSpacing 0x32, nRowHeight 0x27, nStarOffset 0x33, 0}` — count → `0x6` | `gaEscMenuHeaderOptions = \{` → 1 |
| A3 | **Loader skip** | `EscMenu.cpp:584` `if (...szLocalizedImagePath[0] != '\0')` wraps the ONLY file load; empty path ⇒ no load, **no halt**; `:614` init callback fires regardless. Our row: empty path + `fpInitCallback = &...ESCMENU_InitVanillaPlusLabel` sets `pDC6Main` synthetically. Zero loader edits | `szLocalizedImagePath\[0\] != '\\0'` in EscMenu.cpp → 1 (unchanged) |
| A4 | **Free path compatibility** | `EscMenu.cpp:717-720` frees non-null `pDC6Main` via `Core::Archive::DC6_FreeAndDelete` at shutdown; `Archive.cpp:113-120` = `CelCmp::DeleteCellFile(pDC6)` + `Fog::Memory::FreeClientMemory(pDC6)`; `CelCmp.cpp:1800-1844` DeleteCellFile frees the runtime block table (via the pixel-free callback + `D2_FREE`) — all driven off the DC6 struct itself | `DC6_FreeAndDelete` in Archive.cpp → 2 (def + 1 internal use) |
| A5 | **Fixup contract (synthesis entry)** | `CelCmp.cpp:1441-1483` `CELCMP_FixupPointersAndPrepare(pRaw, &pOut, szFile, nLine, -1, szName)`: requires `nFlags & 1` and `nVersion == 6` (when nExpectedFormat=-1); idempotent via `nFlags & 2`; allocates `directions*frames*0x2c` block table via `D2_ALLOC`, fixes the raw file-relative frame-offset table (`pDC6+1`, 4 B/entry) to absolute; `*ppDC6Out = pDC6` — **same buffer, no copy**. Synthesis = build a byte-valid raw DC6 file image (Fog-allocated like `D2FILE_AllocAndLoadD2File` output), `nFlags \|= 1`, call the retail fixup | `CELCMP_FixupPointersAndPrepare` in CelCmp.cpp → 1 def |
| A6 | **Type-0 draw (row rendering)** | `EscMenu.cpp:1025-1027` case 0: `IMAGE_DrawDC6ImageOnScreen(pDC6Main, ScreenSizeX/2, nItemBottomY, 1, DRAW_TRANSPARENT(+4 hovered), -1)`; `D2WinFont.cpp:904-948`: nParam==1 ⇒ sums ALL frame widths and centers the run on nPosX, then draws each frame advancing +0x100 — **synth must be 1 direction × 1 frame**; hovered rows draw mode +4 | `IMAGE_DrawDC6ImageOnScreen` in EscMenu.cpp → 4 call sites (types 0/1/2) |
| A7 | **Page precedent** | `EscMenu.cpp:1123-1130` `ESCMENU_ShowControlConfig`: `SetUIFlag(UI_ESCMENU, UI_RESET)` + `SetUIFlag(UI_CONFIG, UI_SET)` — a row click swaps to a fully separate text screen. Our `ESCMENU_ShowVanillaPlusOptions` mirrors it with `UI_VANILLAPLUS` | `UI_CONFIG, UI_SET` in EscMenu.cpp → 1 |
| A8 | **UI flag store — bounded, 4-site extension** | `d2_enums.h:14372` UI vars are `constexpr` ints 0..37 (30/38+ free numerically); `ui.cpp:1308-1316` `GetUIFlag` **halts above `UI_RECIPESCROLL` (37)**; `ui.cpp:1326` **hard assert `gnUIFlagCount == 0x26` (38)** — the store `GFX_bShowUIFlag` is a 38-slot array. Adding `UI_VANILLAPLUS = 38` requires: (a) struct member in the `GFX_bShowUIFlag` type, (b) `gnUIFlagCount` 0x26→0x27, (c) bound check `UI_RECIPESCROLL`→`UI_VANILLAPLUS`, (d) dismiss-mode table +1 entry (grep `gUIFlagDismissModeTable`) | `0x26` near `gnUIFlagCount` in ui.cpp → 1; `UI_RECIPESCROLL >= eUI` in ui.cpp → 1 |
| A9 | **Page body pattern** | `ControlConfig.cpp` — `D2WinFont::SetAndReturnLastFont(0xd)`, `DRAW_text0`/wide-string renderers with color codes, hit-rect rows (ControlConfig has no DC6 assets of its own beyond retail panel art); draw host = `Draw.cpp` UI dispatch keyed by flags (grep `DRAW_ControlConfig\|UI_CONFIG` in Draw.cpp); input = the same mouse/key dispatch that serves UI_CONFIG (pin exact sites at impl: `ui.cpp` input + `Draw.cpp` draw stage) | `SetAndReturnLastFont(0xd)` in ControlConfig.cpp ≥ 2 |
| A10 | **Ini write (first in tree)** | No `WritePrivateProfileString*` anywhere in D2Client/Fog/D2Win (reads only + `ALTTOGGLE_EnsureIni` template writer, `CreateFileA`-based). Our toggle rows use `WritePrivateProfileStringA("features", key, value, szIniPath)` — kernel32, already linked; ini path via the existing `ALTTOGGLE_BuildIniPath` export | `WritePrivateProfileString` repo count = 0 before impl |
| A11 | **Cache bust (2 s gates)** | Gate statics: `gbAltToggleMode`/`dwAltToggleLastCheck` + `gbAutoGoldMode`/`dwAutoGoldLastCheck` (D2Client.cpp), `gbPortableSaveMode`/`dwPortableSaveLastCheck` (Fog/Source/SFile.cpp). Export per-TU flushers: `D2VP_ConfigReloaded()` in D2Client.cpp (resets its two statics, calls `PORTABLESAVE_FlushConfig()`), `PORTABLESAVE_FlushConfig()` new export in Fog (layering OK: D2Client already calls Fog freely) | `dwAutoGoldLastCheck` in D2Client.cpp → 2 (read+write) |
| A12 | **Feature rows v1** | `alt_toggle`, `auto_gold`, `portable_save` — all `[features]` keys, all `0`-default; page shows `Label: On/Off` per key, click flips + writes + flushes. Slate rows gain one line each as they ship | — |

## 2. Design

### D1 — The Options-page row (globals/D2Client.cpp + our TU)

- `gaEscMenuOptionsTable[5]` → `[6]`; new item inserted at **index 4**, above `Previous`:
  `{type 0, expansionOnly 0, y 0, szLocalizedImagePath = "" , fpVisibilityCheck = nullptr,
  fpClickCallback = &D2Client::UI::EscMenu::ESCMENU_ShowVanillaPlusOptions,
  fpInitCallback = &D2Client::UI::EscMenu::ESCMENU_InitVanillaPlusLabel,
  fpDrawCallback = nullptr, nSubImageCount 0, nImageIndex 0, bSelected 0, sub-paths "", ..., nullptr, {nullptr,...}}`.
- `gaEscMenuHeaderOptions.nItemCount` `0x5` → `0x6`.
- Callback decls in `D2Client.h` (globals/D2Client.cpp already includes it — A-includes verified; no new include needed), definitions in the new `D2Client/UI/VanillaPlusOptions.cpp`.
- Keyboard nav (up/down/enter) and mouse hover iterate `nItemCount` — our row participates free; selection star positions off `nYPosition` — free.

### D2 — Route A: the synthesized label DC6 (new TU, ~150 lines) — **RETIRED, see §9**

> Field QA 2026-10-29 replaced this route with **Route B** (fpDrawCallback text
> label + 8×8 blank placeholder). This section is kept as the design record;
> §9 documents why it was retired and what shipped instead.

`ESCMENU_InitVanillaPlusLabel(pItem, pMsg)`: if `pItem->pDC6Main` already set → return
(idempotent). Else build the raw image:

1. Rasterize `L"Vanilla+ Options"` with the font-0xd glyph source (glyph metrics via
   `FONT_GetCharWidthCallback`/`D2WINFONT_LookupGlyphEntry`; pixel source = the font's
   glyph bitmaps — impl-round pin: how the wide-string renderer fetches glyph blocks,
   A9). Height ≤ 39 px (nRowHeight A2), silver/white pixels + 1-px dark shadow, so it
   reads like the row art; exact palette indices chosen at impl from the font draw
   path (field-QA-tunable constant in one place).
2. Encode a standard DC6 frame (RLE, 1 direction × 1 frame), wrap in a 24-byte DC6
   file header (`nVersion 6`, `nFlags |= 1`, `nDirections 1`, `nFrames 1`, file-relative
   frame-offset table at +0x18) — byte-shaped exactly like a file-loaded image (A5).
3. Allocate via the same Fog client allocator `D2FILE_AllocAndLoadD2File` uses (pin the
   exact alloc call at impl from Fog/Archive.cpp), run
   `CELCMP_FixupPointersAndPrepare(pRaw, &pItem->pDC6Main, __FILE__, __LINE__, -1, "vanilla-plus options label")`.
4. Free path needs ZERO special-casing: shutdown runs the retail `DC6_FreeAndDelete`
   over it (A4) — same allocator, same shape, same fixup ⇒ byte-compatible lifetime.

Fallback if glyph-bitmap extraction proves messy at impl: rasterize through
`D2GFX_DrawImage` of glyph frames into a retained surface — still code-only; decide at
impl, contract (steps 2–3) unchanged.

### D3 — The Vanilla+ page (mirror ControlConfig)

- New flag `UI_VANILLAPLUS = 38` (d2_enums.h) + the 4-site store extension (A8):
  struct member, `gnUIFlagCount` 0x27, `GetUIFlag` bound, dismiss-table entry
  (`1` = close-on-Esc, matching UI_CONFIG's row semantics — pin UI_CONFIG's dismiss
  value at impl).
- `ESCMENU_ShowVanillaPlusOptions`: `SetUIFlag(UI_ESCMENU, UI_RESET)` +
  `SetUIFlag(UI_VANILLAPLUS, UI_SET)` (A7 verbatim shape).
- `DRAW_VanillaPlusOptions()` registered in the Draw.cpp UI stage next to
  ControlConfig's draw (pin site at impl): title line, one row per feature key
  (`Alt-Show Items: On/Off`, `Auto Gold Pickup: On/Off`, `Portable Saves: On/Off`),
  hover highlight, click = toggle row; a `Previous Menu` row back to
  `SetUIFlag(UI_VANILLAPLUS, UI_RESET) + SetUIFlag(UI_ESCMENU, UI_SET)` + restore the
  options table pointers (mirror `ESCMENU_SwitchToOptionsMenu` shape, A7).
- Input handlers mirror ControlConfig's registration (pin the dispatch table site).
  Field-QA revision: **no WM_MOUSEMOVE (0x200) entry** — see §9 (the always-on
  `gUIFlagGeneralInit` batch owns 0x200; a chain-breaking 0x200 handler froze the
  cursor).
- Toggle action: read current (`GetPrivateProfileIntA` via the feature gates' own
  accessors where exported — else direct read), flip, `WritePrivateProfileStringA`
  (A10), then `D2VP_ConfigReloaded()` (A11) — effect is same-frame for the gates, ≤
  1 frame for auto_gold (its tick gates on the flag), portable-save takes effect on
  the next save-path resolution (documented in-row? no — keep the ini template as the
  doc; the page toggles the same keys the template documents).
- Default rows: the three shipped keys; page renders gracefully with more/fewer keys
  (static row table in the TU — adding a slate feature = +1 table line, like the ini
  template block).

### D4 — What is explicitly NOT changed

No D2Game/D2Common edits. No loader edits (A3). No retail row content changes — the
options table gains one row, all five retail rows byte-identical initializers. No
menu draw/input path changes beyond the flag-store extension (A8), which is
predicate-false for every retail screen. No new assets shipped; no MPQ edits; the
only new win32 call is `WritePrivateProfileStringA`. `alt_toggle`-latch machinery,
`CUnit.cpp` hit-tests, ShowItems: untouched. With no `[features]` changes made in the
page, behavior is identical to today's build; the page itself is reachable regardless
of gate states (it is a settings surface, not a gated feature — charter note below).

## 3. Semantics table

| Situation | Behavior | Owner |
|---|---|---|
| Options page renders | 6 rows; ours reads "Vanilla+ Options" between CfgOptions and Previous | our row |
| Row clicked | Options page swaps to the Vanilla+ page (ControlConfig-style) | our callback |
| Toggle clicked | ini written, caches flushed, rows re-render same frame | our page |
| Esc on page | page closes back to Options (dismiss-table entry) | A8(d) |
| Game exit / ESCMENU_Shutdown | synthesized DC6 freed by the retail free path | A4/A5 |
| `auto_gold=0` etc. untouched | page reachable but values untouched — page is UI, gates unchanged | charter note |
| Classic (no expansion) | row renders like any row (`nExpansionOnly 0`); page identical | our row |

**Charter note (owner awareness, no sign-off blocker):** the settings page is
*input UI*, not a gated feature — the menu entry itself is always present, while
every feature it toggles stays opt-in with `0`-defaults. Principle 1 (default OFF =
vanilla) applies to the *features*, and the page changes no behavior unless clicked.
If the owner prefers the row itself gated (e.g. hidden unless `[features] options_menu=1`),
that is a one-line visibility callback (A1 row struct supports `fpVisibilityCheck`) —
default: always visible (it is how the features get discovered).

## 4. Impl-round verify items (V1–V6)

- **V1** Pin `CELCMP_GetDC6BlockFromGfxData` + block-field population (width/height
  reach the draw path — CelCmp.cpp) — determines the exact raw frame sub-header the
  synthesized data must carry (A5/A6).
- **V2** Pin the glyph pixel source: how the wide-string renderer (D2WinFont.cpp
  ~:60 comment "Draws each character glyph from font DC6") obtains glyph bitmaps;
  font-0xd glyph DC6 access or per-glyph blit into a scratch buffer.
- **V3** Pin the exact Fog allocator `D2FILE_AllocAndLoadD2File` uses for the raw
  image buffer (Fog/Archive.cpp) — synth buffer must free-compat (A4/A5).
- **V4** Pin ControlConfig's draw + input registration sites in Draw.cpp / ui.cpp
  (A9) — our TU hooks the same two dispatch points.
- **V5** Pin `gUIFlagDismissModeTable` shape + UI_CONFIG's entry value (A8d).
- **V6** Field: Esc → Options shows the row between Configure Keys and Previous
  Menu; click opens the page; toggles apply same-frame (auto_gold within ≤1 s;
  alt_toggle next Alt press; portable_save next save); Esc returns to Options;
  retail rows unchanged; game exit clean (no crash in shutdown — the synth DC6
  frees); `Vanilla+ Options` label renders centered and legible in 640×480 and
  800×600.

## 5. TU list, QA, delivery

- **Touched TUs (6 + 2 new)**:
  - `globals/D2Client.cpp` — options header count + table [5]→[6] with the new row (A1/A2).
  - `d2_enums.h` — `UI_VANILLAPLUS = 38` (A8).
  - `D2Client/UI/ui.cpp` — flag-count assert, GetUIFlag bound, dismiss table (A8).
  - `GFX_bShowUIFlag`'s header (pin: Draw.h or globals.h — impl round) — +1 member (A8a).
  - `D2Client/UI/Draw.cpp` — page draw dispatch line (A9).
  - `D2Client/D2Client.cpp` — `D2VP_ConfigReloaded()` + 2 gate-static resets + page
    ini-write helper (A10/A11); `D2Client/D2Client.h` — callback + flusher decls.
  - **NEW** `D2Client/UI/VanillaPlusOptions.{h,cpp}` — synthesis + page + callbacks.
  - `Fog/Source/SFile.cpp` — `PORTABLESAVE_FlushConfig()` export (+Fog header decl).
- **QA**: syntax round via the signature-mirror harness pattern (auto_gold
  precedent); real gate = per-TU llvm-mingw i686 + full build (Win11/CI); field
  loop V6; A/B: no ini interaction ⇒ byte-identical behavior to current build
  (the row + page are additive UI; every feature default stays `0`).
- **Land**: ledger row 34, ini template comment unchanged (page toggles the same
  documented keys), release note at next tag.

## 6. Owner decisions (2026-10-29) — recorded

1. **Label route**: **A** — runtime-synthesized DC6 via the retail fixup; zero
   shipped assets, zero loader edits (A3 skip + A5 contract).
2. **Placement**: **index 4** of the Options table — directly above
   "Previous Menu" (`Previous` becomes index 5).

## 7. Impl-round pins (V1–V5 resolved, 2026-10-29)

- **V1 (block/frame-header reconciliation)** — `DC6Block` IS the DC6 frame
  header: nFlip@+0x00 (=1), **width@+0x04, height@+0x08**, offsets@+0x0C/
  +0x10, allocSize@+0x14, **patch slot@+0x18** (fixup writes the block-row
  pointer there; `DeleteCellFile` frees through it), length@+0x1C,
  **RLE data@+0x20**. Draw path: renderer `DrawImage` →
  `SPRITECACHE_GetOrLoadSprite` → **bottom-anchored** blit (`offsetY+posY-
  height < winH` cull math — hence EscMenu's `nItemBottomY`); the RLE decoder
  (`FASTCMP_DecompressRLEWithAlpha`, FastCmp.cpp:1742) halts if pitch <
  width. Synth contract: 1 dir × 1 frame, offsets 0, retail RLE tokens
  (0x01–0x7F literal, 0x81–0xFF transparent 1–127, **0x80 end-of-row**).
- **V2 (glyph pixel source)** — glyphs live in the **font DC6 itself**, one
  frame per glyph: `D2WINFONT_DrawWideStringWithDrawMode` sets
  `sGfxData.pDC6 = *gppDC6FontCache` and per char looks up the .tbl entry
  (`D2WINFONT_LookupGlyphEntry`) for **frame idx = *(uint16_t*)(entry+6)**,
  **advance = entry[1]**. Those cache globals are TU-internal (not
  exported), so the synth loads its OWN font copy via
  `D2WINFONT_BuildFontPath(0xd)` + `D2WinArchive::IMAGE_LoadDC6`, decodes
  frames with its own RLE decoder, composes at a running pen x (advance
  parity), keeps the glyphs' own palette indices (renders as in-game white
  text, unshifted like retail row art), and frees the copy with
  `IMAGE_FreeImage`. Fallback: an 8×8 blank image keeps the row drawable
  if synthesis ever fails (type-0 draw halts on null pDC6Main).
- **V3 (allocator)** — `D2FILE_AllocAndLoadD2File` = `D2_ALLOC(dwFileSize +
  800)` (Fog/Archive.cpp:56) = `AllocClientMemory`; free =
  `DC6_FreeAndDelete` = `DeleteCellFile` + `FreeClientMemory` — the synth's
  raw buffer uses the same pair, so the retail free path needs zero
  special-casing (verified against CelCmp.cpp:1800).
- **V4 (dispatch sites)** — SET: `SetUiFlagInit` case UI_CONFIG =
  `CONTROLCONFIG_OnToggleUI(1)` + `DRAW_RegisterDrawHandlers()`; our case
  registers our own `gaVanillaPlusInit[4]` batch (mousemove/mousedown/
  mouseup/Esc-keydown; cube/panel style, self-contained). RESET: the
  unregister switch gets a matching case. Draw: `DRAW_UI` gains
  `if (GFX_bShowUIFlag.VanillaPlus) VanillaPlus::DRAW_VanillaPlusOptions();`
  next to the ConfigControls dispatch. Back-navigation =
  `SetUIFlag(UI_VANILLAPLUS, UI_RESET)` + `ESCMENU_ShowMenu(0, 1)` — the
  exact shape of retail `INPUT_ConfigCancel` minus the binding revert.
- **V5 (dismiss machinery — deeper than A8 anticipated)** — `SetUIFlag`
  itself calls `UI_HandleKeyInputAndDismiss(type, eUI)` (Draw.cpp:1293), so
  **every** SetUIFlag with flag 38 reads `anDismissMode[38]` off every
  ACTIVE row ⇒ the array **must** grow `[38]→[39]` — and because every row
  is an aggregate initializer, the new column zero-fills to "ignore" with
  **zero row-initializer edits** (globals.h one-token change). Extension
  landed: `anDismissMode[39]`, `gUIFlagDismissModeTable[39]` with entry 38 =
  UI_CONFIG's row (`&gaUIFlagDescs[9]` — identical modal semantics; config
  opens/closes exactly like our page should), `gnUIFlagCount 0x26→0x27` +
  **6 asserts** (5× Draw.cpp, 1× ui.cpp), GetUIFlag + SetUIFlag bounds
  `UI_RECIPESCROLL → UI_VANILLAPLUS`, `D2UIFlagStrc.VanillaPlus` member
  appended last (keeps `(&GFX_bShowUIFlag.Game)[38]` in enum order).
  Left untouched on purpose: `gaEscMenuUIFlags[38]` + the two
  `uTableFlags < 38` loops in `ESCMENU_ShowMenu`/`ESCMENU_RestoreUIState` —
  flag 38 is never active during those dances (the page closes itself
  before any menu transition; `ESCMENU_ShowMenu(0,1)` passes bHideUI=0).

## 8. Implementation record (2026-10-29)

- **Touched (11 + 2 new)**: `d2_enums.h` (flag 38 + using),
  `globals/globals.h` (anDismissMode[39], VanillaPlus member, table extern),
  `globals/D2Client.cpp` (dismiss table +1 aliasing entry, gnUIFlagCount
  0x27, options header 0x6, table [6] + new row — the five retail rows
  byte-identical), `D2Client/UI/ui.cpp` (assert + GetUIFlag bound),
  `D2Client/UI/Draw.cpp` (5 asserts, SetUIFlag bound, SET/RESET cases,
  DRAW_UI dispatch, include), `D2Client/D2Client.cpp` (D2VP_Get/Set/
  ConfigReloaded bridge — first `WritePrivateProfileStringA` in tree),
  `D2Client/D2Client.h` (decls), `Fog/Source/SFile.{cpp,h}`
  (`PORTABLESAVE_FlushConfig`). **NEW** `D2Client/UI/VanillaPlusOptions.{h,cpp}`
  (synthesis + page + handlers; CMake GLOB_RECURSE picks it up).
- **Verification**: extracted-verbatim harness
  (`scripts/build_vanilla_plus_harness.sh` → `g++ -fsyntax-only`) over the
  real TU body + real bridge block + real row initializer against
  line-referenced stub mirrors — green; caught one real typo pre-build
  (`nFrameIndex` → `anFrameIndex`). 12 grep gates green (counts above).
- **Field QA tunables (one place)**: `VPLABEL_HEIGHT`/`VPLABEL_FONT_ID`,
  page box/row constants in VanillaPlusOptions.cpp §tunables.
- **Remaining**: real gate = owner llvm-mingw i686 full build + field loop
  V6 (label legibility/placement, toggle same-frame effect, Esc round-trip,
  clean shutdown free).

## 9. Field-QA revision (rev C, 2026-10-29) — Route A retired → Route B

First field build (owner Win11 llvm-mingw i686, overlay
`d2vp-overlay-vanilla-plus-options-20261029-rev2.zip`): **compiles and runs, but
the "Vanilla+ Options" row label is invisible while the row still opens the
page, and the cursor locks while the page is open.** Both root-caused in the
tree; both fixed in the same overlay round (rev 3).

### 9.1 Bug 1 — invisible label (Route A retired)

The Route A synthesis fell into its blank-8×8 fallback in the field (that
fallback is the invisible-but-clickable row the owner saw; it also proved the
placeholder's byte shape renders safely — evidence reused by Route B). Route A
carried three independent defects that made it wrong even on success:

1. **Row order**: DC6 RLE streams are **bottom-up** — `FASTCMP_DecompressRLEWithAlpha`
   (D2CMP/SRC/FastCmp.cpp:1742) walks the destination with `nNegPitch =
   -nDstPitch`, so encoded row 0 = the image's bottom scanline. The Route A
   encoder emitted rows top-down ⇒ the label would have rendered vertically
   flipped.
2. **Palette**: glyph frames are drawn through `D2GFX_DrawShiftedImage` with the
   font's palette-shift index (`D2WINFONT_DrawWideString`, D2WinFont.cpp:424–426);
   Route A copied raw glyph indices into a plain `DrawImage` sprite — unshifted
   colors at best.
3. **Buffer**: `VPLABEL_BuildDc6` encoded into a 1240-byte stack scratch
   (`aEncoded[VPPAGE_BOX_WIDTH * 2]`) while a 0x100-wide × 0x27-tall text raster
   encodes to ~2 KB (worst case far more) — a stack-smash hidden behind the
   fallback path.

**Route B (shipped)**: the retail esc-menu loop already invokes a per-row
`fpDrawCallback` every frame (`ESCMENU_DrawMenu`, EscMenu.cpp:1001–1004, before
the row's image draw). The row now sets `fpDrawCallback =
&VanillaPlus::VANILLAPLUS_DrawRowLabel`, which renders the label with `DRAW_text`
(font 0xd — the ControlConfig row font, retail-proven) centered at
`ScreenSizeX/2`, baseline at the row band bottom minus `VPLABEL_BASELINE_INSET`
(DC6 draws extend upward from y, so y = baseline; same convention
ControlConfig's row renderer uses). `fpInitCallback` now only seeds the
field-proven 8×8 **blank placeholder** DC6 (hard-coded 16-byte RLE stream — per
row `0x88` transparent-run + `0x80` end-of-row — no runtime encoder, no scratch
buffer), which satisfies the unconditional type-0 image draw with zero visible
pixels. Allocator + `CELCMP_FixupPointersAndPrepare` shape unchanged ⇒ retail
free path (`DC6_FreeAndDelete`) still needs zero special-casing. Charter impact:
none — still zero shipped assets, zero loader edits; the label now renders
through the engine's own text path, so localization-ready and always legible.

### 9.2 Bug 2 — locked cursor (0x200 handler removed)

The page's input batch registered a `WM_MOUSEMOVE` (0x200) handler ending in
`SMsgBreakHandlerChain`. Handlers registered later run first in the client
chain, so every move while the page was open **broke the chain before
`ui::UI_HandleMousePositionUpdate`** (the always-on `gUIFlagGeneralInit` batch,
globals/D2Client.cpp:2691–2707) — the one handler that updates `MouseX/MouseY`
and refreshes OS-cursor visibility (`UpdateOSCursorVisibilty(1)`) on every move.
With position frozen and the OS cursor hidden by the normal gameplay path, the
on-screen cursor stops tracking = "locked". Neither `ESCMENU` nor
`ControlConfig` registers a 0x200 handler (ControlConfig's batch =
`gaUIFlagInitTable + 5`: 0x201/0x202/0x204/0x205 + keydowns only).

**Fix (shipped)**: batch is 3 entries — 0x201 down, 0x1b Esc, 0x202 up. Hover
highlight is derived per frame in `DRAW_VanillaPlusOptions` from
`ui::GetMouseX/GetMouseY` (the ESC menu and ControlConfig do exactly this).
Down/up coordinate source is the message payload (`pMsg->uLParam`), which the
handlers already used. Down/up handlers keep the retail modal idiom (bHandled=1
+ break — byte-matches `INPUT_ConfigMouseDown`, ui.cpp:3496–3498).

### 9.3 Page text placement correction (same round)

Same baseline convention applied to the page itself: `DRAW_text`'s y is the
glyph **baseline** (bottom-anchored DC6), so row labels/values/Previous Menu
now anchor at `row bottom − VPPAGE_TEXT_BASELINE_INSET` (was `row top + 2`,
which would have floated the ink above its row). Title keeps ControlConfig's
exact anchor (`boxTop + 0x1e`).

### 9.4 Verification

Sandbox: extracted-verbatim harness re-run green (row initializer extraction
also re-type-checks the new `fpDrawCallback` pointer against the verbatim
`D2EscMenuItemStrc`). Real gate: owner llvm-mingw i686 rebuild, then field
re-check: label visible + legible, no cursor lock, toggle round-trip, Esc
round-trip, clean shutdown free (the placeholder exercises the same retail
free path).

## 10. Field-QA revision (rev D, 2026-10-29) — label size fix

Field round 2 on the rev-3 overlay: **"label shows, but the text is ~10% the
size of the neighboring rows; cursor unlocked"** — Route B itself confirmed
healthy (draw hook, placeholder, input batch all good), one residual defect:
the label font.

Root cause: `VPLABEL_FONT_ID` was 0xd = `aFontNames[13]` = **FontInGameChat**
(the chat console font, ~10 px cell) — correct for the page, which mirrors
ControlConfig (drawn entirely in FontInGameChat), but wrong beside the esc-menu
rows: the retail neighbors (`SoundOptions` / `VideoOptions` /
`AutoMapOptions` / `CfgOptions` / `Previous`) are DC6 label *images* in the
big menu-font family — the same look as the esc-menu main items. Chat-size
text next to those images reads as a small fraction of their height.

Fix: the row label switches to font 2 = `aFontNames[2]` = **Font30** (30 px
cell), the family those label images belong to. Geometry: the 0x27 band fits
the 30 px cell with `VPLABEL_BASELINE_INSET` 8 → 4 (cell spans
rowTop+5 .. rowTop+35, 'p' descender included — `DRAW_text` y is the glyph
cell bottom, descent included). Blast-radius guard: the page keeps 0xd via a
new dedicated constant `VPPAGE_FONT_ID` — the constants are deliberately split
so the two surfaces can't drag each other's metrics (the page's 0x1a row bands
cannot fit a 30 px cell; unifying them would have broken the page while fixing
the row).

In-tree evidence trail: `aFontNames[14]` (globals/D2Win.cpp) maps 13 →
FontInGameChat and 2 → Font30; `SetAndReturnLastFont(2)` lazy-loads
`DATA\LOCAL\Font\<lang>\Font30.dc6/.tbl` through the standard font cache (no
new machinery, every locale ships Font30). Bonus finding while auditing the
retail draw: `IMAGE_DrawDC6ImageOnScreen`'s 4th argument is an ALIGN MODE
(1 = center the frames' total width on x, 2 = right-anchor), **not** a frame
index — so the 1-frame 8x8 placeholder is safe by construction (type-0 rows
draw it centered and transparent).

### 10.1 Verification

Sandbox: extracted-verbatim harness re-run green (the font constants live
inside the extracted namespace body). Real gate: owner llvm-mingw i686
rebuild, then field round 3: label height visually matches the neighboring
rows (640x480 and 800x600), vertical centering acceptable, the Vanilla+ page
still renders ControlConfig-styled (unchanged), and every round-2 behavior
stays green (live cursor, toggle round-trips, Esc round-trip, clean shutdown).

## 11. Field-QA revision (rev E, 2026-10-29) — label size, measure-don't-guess

Field round 3 on the rev-4 overlay: **"that's a lot closer. improved. but
still only 50% size"** — the Font30 switch moved the label from ~10% to ~50%
of the retail rows' height, direction confirmed, magnitude still short. Two
font datapoints now exist (FontInGameChat ≈ 10%, Font30 ≈ 50%), and both are
eyeball estimates against baked label images whose pixel height is MPQ data
the sandbox cannot inspect. A third constant guess would repeat the same
cycle; revE replaces the guess with a measurement.

Design: `VPLABEL_FONT_ID` (constant) is replaced by a runtime resolution that
runs once per process, on the row's first draw (`VPLABEL_DecideMetrics`):

1. **Override first** — `[ui] label_font = <aFontNames index>` in `d2vp.ini`
   (same file as the `[features]` gates; path resolved by a mirror of
   `ALTTOGGLE_BuildIniPath` since that one is static in D2Client.cpp).
   Values outside 0..13 are ignored. This is the escape hatch that turns any
   residual size mismatch into a one-line ini edit instead of an overlay
   round-trip.
2. **Measure the retail neighbor** — row [5] "Previous" (`pDC6Main`, loaded
   unconditionally at menu image load; no expansion/visibility gates, and it
   is the row directly below ours, i.e. exactly what the eye compares
   against; fallback row [0] "SoundOptions"). Its frame-0 height comes
   through the retail accessors (`CELCMP_GetDC6BlockFromGfxData` +
   `GetDC6Height`) — the same path every type-0 row draw takes.
3. **Probe candidates** — for each of {7 = Font24, 2 = Font30, 3 = Font42}
   (ascending), force the standard lazy load via `SetAndReturnLastFont`,
   then read the 'S' glyph's frame height through the retail lookup chain
   (`gpfnFontGetCharWidth` → frame number → `GetDC6Height`), the identical
   glyph→frame lookup `D2WINFONT_DrawWideString` performs for every drawn
   character. Pick the font whose capital height best matches the measured
   image height; ties resolve to the larger font (the retail labels are the
   big menu family — when in doubt, go bigger). Fallback if nothing
   measures: Font42 (largest menu font).

Metric note: capital height is the comparison because "Previous" has no
descenders — its baked image height IS its capital extent (+1–2 px of bake
padding, the same bias for every candidate, so the argmin is stable).
`VPLABEL_BASELINE_INSET` stays 4 (rev4's field-accepted value): glyph frames
are bottom-anchored, so a taller font extends upward from the same anchor and
the accepted placement carries over; 'p' descenders may reach a few px into
the inter-row gap, which the retail images (bottom-anchored at the band
bottom) also do. Every API the resolution touches is retail-proven and was
already exercised by three field rounds (font lazy load, glyph lookup, DC6
block read, ini profile read via the bridge's own pattern).

Failure containment: a failed font load leaves the cache entry null and
`FONT_GetCharWidthCallback` would index `pGlyphs + ch` against garbage — the
probe checks `pFontDC6 && pGlyphs` before touching either. All guards return
0 and the resolver degrades to the next candidate or the Font42 fallback;
no path can crash or render a null frame, and the page (`VPPAGE_FONT_ID` =
0xd) is untouched.

### 11.1 Verification

Sandbox: extracted-verbatim harness re-run green with the extended stub set
(`gaFontCache`/`gpfnFontGetCharWidth`/`lstrcatA` added — GetPrivateProfileIntA,
`D2GfxDataStrc`, the CelCmp accessors and `SetAndReturnLastFont` were already
mirrored). Real gate: owner llvm-mingw i686 rebuild, then field round 4:
label height visually matches the neighboring rows (if it still reads short,
set `[ui] label_font = 3` in d2vp.ini and restart the game — that forces the
biggest font and, if even that is short, the next rev goes back to
synthesizing a DC6 at the measured height, now with confirmed numbers); page
unchanged; all round-2/3 behaviors stay green.
