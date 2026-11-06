# Changelog — D2LoD Vanilla+ (`d2vp.exe`)

All notable changes to the Vanilla+ client and the Glide3xVK wrapper, per shipped
release. Working rounds between releases carry build self-IDs `Vanilla+0.0.x-Ryy` — the
first `d2boot.log` line pins any shipped exe to its source round. The engineering-level
round journal lives in [`handoff.md`](handoff.md) (current) and
[`HISTORY.md`](HISTORY.md) (verbatim archive, H1–H40); per-round summaries for the
0.0.6-era releases are also on the
[GitHub releases page](https://github.com/VTSTech/D2VP-QoL/releases).

Started at handoff rev 110 (2026-11-02); the 0.0.1–0.0.6 entries are distilled from the
GitHub releases page and the H-records (the README release table this superseded was
retired in README trim #1, rev 111 — verbatim in [`HISTORY.md`](HISTORY.md) H32).

## [Unreleased]

- **vanilla-plus (R126): Shift-compare shield-slot fix — the first field catch on the R125 compare, plus the release-line roll 0.0.7 → 0.0.8** — owner on the R125 build: "Great job it works. Only thing I notice, if i try to compare a shield, it compares to my primary hand weapon, not my offhand shield. i know they can go in either slot, pretty sure everyone uses left slot for weapon and right slot for shield." Root cause is in the DATA, not the compose path: vanilla `ItemTypes.txt` gives every weapon class AND the Shield row the **same** body pair (`body1=rarm`=4, `body2=larm`=5` — verified against the 1.13 data file), so R125's "first occupied of {bl1,bl2}" resolver tried bodyloc 4 — the primary weapon box — first for shield hovers too, and the equipped 1H weapon won. The rule the §2 draft wrote down ("shields/ammo 5, straight bl1") was wrong about which slot the txt names first. Fix (`SHIFTCOMPARE_ResolveSlot`, one function, VanillaPlusCore.cpp): when **both** arm cells are occupied, the same-kind slot wins — a shield or quiver hover tries the off-hand (bodyloc 5) first, a weapon hover the primary (bodyloc 4); the shield detector is `ITEMS_IsA(item, ITEMTYPE_AnyShield)` (the same aggregate-type call retail's own `Panel.cpp:1252` uses; quivers via `ITEMTYPE_IsQuiver` on the hovered item's type row). This mirrors how retail itself resolves the identical ambiguity in its auto-equip (`ItemMode.cpp` `choose_body_loc`'s type-conflict check: occupied kind conflicts → the paired slot). One arm occupied = that item, regardless of kind (the only possible comparison — unchanged); the 2H handedness-2 rule (describe from slot 4, the paired-cell convention) is untouched; rings {6,7} untouched; the compose/restore round trip untouched. Weapon hovers behave exactly as R125 (bl1=4 IS the weapon box for them) — the fix only re-routes the shield/quiver branch. Gates: +1 `shiftcmp:` pin ("shield-kind slot pick" — `ITEMTYPE_AnyShield` in the resolver, so a future merge dropping the discriminator trips the gate) → **517/517**. self-ID `Vanilla+0.0.7-R125` → **`Vanilla+0.0.8-R126`** (owner: "bump to 0.0.8 for R126" — the release line rolls with the first post-R125 field round). Verification: `tools/check-vanilla-plus.sh` 517/517; sandbox syntax probe of both touched TUs under the build's own flag set — error digest IDENTICAL to pre-round HEAD (16 normalized lines, all pre-existing host-shim noise: the Bnclient `__time32_t` class in VanillaPlusCore.cpp, the registry/D2Hell.h shim class in Game/Main.cpp). Docs: FEATURES row 0b + §0b (the wrong "shields are straight bl1" slot-rules sentence corrected), SMOKE §C-12c shield leg rewritten, README feature row, SHIFT-COMPARE-PLAN status, handoff, CHANGELOG (this entry). Field re-test owed (owner): hover the equipped-setup shield → the shield's box; hover the weapon → the weapon's box; a quiver with a bow → the quiver; dual-wield barbarian → the hovered-kind matching hand.

- **vanilla-plus (R125): Shift Item Compare — the hold-Shift equipped-item compare tooltip (FEATURES.md slate row 0b, the code round of [SHIFT-COMPARE-PLAN.md](SHIFT-COMPARE-PLAN.md))** — D2R hover-compare parity: with `[features] shift_compare=1`, holding **Shift** (the key bound to retail cmd 0x24, `CMD_SetClickModifier`, default VK_SHIFT — a keybind remap is honored, read per frame with retail's own `UpdateMouseModifierKeyState` poll idiom, Game.cpp:1193-1221 — no CmdTbl entry, no latch, no keyup hook, deliberately not the alt_toggle shape) while hovering an equippable item in a panel draws a **second tooltip** — the equipped item for the matching body slot — alongside the hovered item's tooltip. Side-by-side viewing only, no stat-delta lines — that is D2R's own compare scope (diablo2.io launch-era statement in the plan's Sources). **Owner decisions 2026-11-06**: side-by-side second box (the plan's recommendation), hold trigger, row label "Shift Item Compare", and **SHOP STOCK INCLUDED** — a delta from the plan's draft v1, which skipped the vendor-stock narrow tooltip; the capture fires immediately after Inv.cpp's single `LoadItemDesc` publish (:9422), the one point both builder shapes (the full builder :8532-8827 and the shop-narrow name+price branch :8828-8899) publish through, so vendor-stock hovers compare too. **Slot resolve** (the plan's §2 rules, tree-verified before coding): the hovered item's intended slot comes from its ItemTypes row (`ITEM_GetTxtItemTypes_BodyLoc1andBodyLoc2`, bl1/bl2) — never from the item instance (`GetBodyLocation` is only set once equipped); rings (bl1=6/bl2=7) compare against the first occupied of {6,7} (deterministic, retail's first-fit auto-equip order); weapons are 2H-aware — first occupied of {bl1,bl2}, and when `INVENTORY_GetWeaponHandedness` reports 2 (a 2H occupying both arms) the compare describes slot 4's item, the same paired-cell convention the equipped-slot hover path uses (`INV_UpdateBodySlotUnderCursor`); shields/ammo 5, armor 3, helm 1, boots 9, gloves 10, belts 8, amulet 2 are straight bl1→bl2 lookups; both empty (potion, charm, jewel) = no box; hovering the paperdoll never compares (`gbInvBodySlotUnderCursorExists`); empty target slot = no box. **Mechanics** (the item_ilvl lesson: hook, don't refactor): a VP block right after the :9422 call invokes the core hook `SHIFTCOMPARE_CompareForHover()` (VanillaPlusCore.cpp) — save the minimal global set (`pItemToDescribe` + `g_nCurrentInventoryPage` + the primary's published `DisplayedTooltip` struct), force the page to 0 (which makes the :8532 narrow-selector's `page - 1U > 8` term TRUE → the FULL builder runs, and the :8783-8787 vendor price append is skipped, so the compare box is the plain equipped-item tooltip everywhere, shop included — the §9 checklist pinned the exact gate reads first; `gpInvItemUnderCursor` is a hover FLAG that stays set, not an item pointer), swap `pItemToDescribe` to the equipped item, call `LoadItemDesc(pPlayer, 1)` (bUsePlayer=1: requirement colors check against the player), capture the composed tooltip into the second channel together with the primary's published rect (a capture-time snapshot, so the Alt-labels path wiping the primary between capture and draw — ShowItems.cpp :309/:413/:420, running between the panel draws and Draw.cpp:739 — cannot distort the side-by-side placement), then restore the globals AND the primary's `DisplayedTooltip` so the hovered item's box draws unchanged. **Equipped set items** ride LoadItemDesc's own quality-5 route into `ITEM_BuildSetItemTooltip` (:8540-8546), so the compare box carries the full set-bonus lines — the plan's §9 item 5 asked for a decision between accepting a gap or routing through the set builder; the tree already routes, no gap. **The second channel** (`gVpCompareTooltip`, `TEXT_CompareTooltipSetAttributes`, `DRAW_CompareTooltipUnderCursor`) lives in D2Win/Src/D2WinFont.cpp beside the file-local metric statics — the draw is a VP-marked clone of `Draw_UnitNameUnderCursor` (the font-downsize loop, the X/bottom clamp chain, the translucent-black `DRAW_LESSTRANSPARENT` background, `D2WINFONT_DrawWideString` with embedded 0xFF color codes, the `SetAndReturnLastFont` save/restore) minus the progress-bar branch (no `nLoadProgress` field — structurally unreachable), placed LEFT of the primary with a 4 px gap (wide enough that two adjacent translucent boxes never visually merge), falling back RIGHT of the primary when that leaves the screen, bottoms aligned — and it self-clears, so nothing outlives the frame; Draw.cpp calls it right after the primary draw (:739), before the stored tooltip (:741). **Options page row 9** shipped in the same round (the R101/R108 row-growth pattern): `gaVanillaPlusRows[9]` with label `Shift Item Compare`, `VPP_ROW_COUNT` 8→9, `VPP_ROW_BACK` 8→9, `VPPAGE_BACK_ROW_Y` 0x164→0x184, `VPPAGE_BOX_HEIGHT` 0x191→0x1b1 (at 640x480 the box bottom is 456 < 480), layout-math comment rewritten; the write bridge busts `dwShiftCompareLastCheck` (D2VP_ConfigReloaded), so a flip applies on the next hover frame. **ini template**: documented `shift_compare=0` block in `ALTTOGGLE_WriteIniTemplate`. **Gates**: +19 `shiftcmp:` pins (gate accessor, ini key + template key, bust pair, the cmd-0x24 poll pair, slot resolver, hook fn, page-0 compose guard, Inv.cpp call site, capture/draw/struct trio, font decls, Draw.cpp pipeline call, paperdoll exclusion, options row + the three re-pinned page constants), 2 R108-era options shape pins retired (superseded by the R125 pins) → **516/516**. self-ID `Vanilla+0.0.7-R124` → `-R125`. **Verification**: the plan's §9 tree-verify pass ran BEFORE coding (every §3 anchor re-pinned at source level; most exact, several within the ±2-line drift budget); sandbox syntax probe of the five touched TUs under the build's own flag set (-std=c++17 -fms-extensions + the forced-include chain) — all parse clean, error digest IDENTICAL to pre-round HEAD (2 isspace-class in D2WinFont.cpp, 33 registry-shim-class in Game/Main.cpp, both pre-existing host-SDK gaps); the llvm-mingw build gate stays the compile gate of record, and the plan's §8 field matrix (owner) is pending. Docs: FEATURES §0b, SMOKE §B/§C-12c + issue template regenerated, VANILLA-PLUS, README, handoff. D3D fullscreen boot crash fix — the eighth-sync merge split the texture-cache node-base spelling across a refile boundary** — owner field report on the R123 build: "D3D fullscreen does not boot. boots in -w, crashes on 1080p. Glide still works in all modes/resolutions", with the d2boot.log capture: pre-setup fully green (`provider init -> 1`, `InitPerspective -> 1`, `CreateWindowWithChecks -> 1`), then `setup ok -> entering mode loop` → ~1s → `VEH: fatal exception 0xC0000005 … own-code fault … AV READ at 00000010` — the fault is AFTER init, in the first title-screen frames, D3D-only (Glide/DDraw/GDI untouched: `-w` boots, Glide green everywhere). Root cause is NOT the old war-story classes (fix 24 black-ground, R109 mode-set ladder, R95 perspective toggle — all verified intact post-merge): it is a **merge resolution defect in `RENDERER_DIRECT3D_UploadTextureToCache`** (the per-frame D3D texture-cache upload). Upstream's eighth-sync file-symbols refile turned the texture-cache node-buffer bases at retail `0x00970ec4` into a function-local static (`INT_00970ec4`) addressed as `(&INT_00970ec4)[iMipLevel * 7]` — a spelling that aliases the real storage (`&gD3DGenericTextureCache - 4 + iMipLevel * 28`, i.e. `&pCache(iMipLevel)->pNodeBuffer`, `pNodeBuffer` @ 0x18 in the 0x1C-byte `D3DTextureCacheStrc`) **only at retail data addresses**; in a rebuilt exe the static lives wherever the linker puts it, so `(&INT_00970ec4)[iMipLevel * 7]` reads past the 4-byte static into unrelated .bss and yields a garbage node-array pointer. The merge took upstream's spelling for three of the function's four base sites (`SetTexture` slot load + the two allocation-site loads) but kept the fourth (the palette-identity invalidation check) on the fork's retail-relative form — one hunk, two spellings, and the resolver dropped the refile on the `if`-condition line. Result: allocator writes (`D3DTEXCACHE_InitializeTextureCachePool`'s `pCache->pNodeBuffer = _Dst`) landed in one region while the per-frame palette check and `SetTexture` indexed the other — first texture upload succeeds (empty cache, node 0 skips the check), the first RE-upload of the same sprite class reads its palette-identity pointer through the wrong region and dereferences a near-null base → `AV READ at 0x10`. Fix (one function, `D2Direct3D/Renderer/Direct3D.cpp`): all four base sites back on the fork's build-independent relative form `((int*)((uint8_t*)&gD3DGenericTextureCache - 0x4))[iMipLevel * 7]` (field-proven R74..R122; retail-equivalent to `&pCache->pNodeBuffer` by struct arithmetic, so the byte-match shape is unchanged in spirit), upstream's now-dead statics `DAT_00970eac`/`INT_00970ec4` dropped, dated VP note on the function. **New instrument**: `scripts/refile_aliasing_audit.py` — sweeps all 220 refile-marked TUs for the three defect shapes (scalar refiled static indexed as an array; refile static + old global-relative spelling of the same storage in one TU; "also labelled" retail names still referenced elsewhere) — repo-wide CLEAN after the fix, i.e. this was the only refile-aliasing site the eighth sync created. Gates unchanged **499/499** (no row counted the statics); syntax-check error digest identical pre/post on the touched TU (pre-existing D2Hell.h host-shim noise only). self-ID `Vanilla+0.0.7-R123` → `-R124`. llvm-mingw build gate + D3D field re-test pending (owner; expected: D3D fullscreen boots to the title screen at every stop — 640/800/720/1344-host/1366-host/1080 — Glide matrix unchanged, `-w` unchanged).
- **vanilla-plus (R123): the eighth upstream sync — Ghidra 1908 → 1948, merge-only round** — upstream `jaenster/redivivus` `7df6ad0e..b8e3087f`, 16 commits, 786 files (3 evidence rounds; `1608ccb3` defines 693 unreferenced functions + recorded VC8 forms; `4853ab5e` Ghidra 1934–36 control-flow forms + wrapping signed arithmetic; `7d30a7d8` **refiles module-local symbols into their using modules** — the `D2Client/Renderer/Direct3D.cpp` wrapper block (CheckCutScene/EndScene2/SetPaletteTable/…) moves to `D2Direct3D/Renderer/Direct3D.cpp` and `D2Client/Net/MCP.{cpp,h}` fold into their consumers; `309d8175` byte-matches initialised data against 1.14d; `3f5d10c8` Ghidra 1947 generator round; `f0f24ecb` InitPerspective identifier + NPC trade roll; `cebffb6e` two quest functions return nothing (the A1Q1 gossip call now rides inside the reward arm); `810e339a` **Duriel's death no longer crashes**; `f3402e90` d2_patch under VC8; `636e78e0` CCmd.h include case; `b8e3087f` check-build grows an include-case gate). **Prep**: one normalization commit (217 fork-touched files CRLF → LF, handoff §8) cut the raw trial surface 48 files/78 hunks → 26 real sites/57 hunks — 22 of the raw conflicts were pure line-ending noise and auto-merged away. **Fork resolution**: the R112 hosted-stretch `EndScene2` hook refiled WITH its function into `D2Direct3D/Renderer/Direct3D.cpp` (gates "stretch: endscene2 guard" re-pinned to the new TU via the transplant marker, "d3d present helper" 5 → 7 — the count gate caught the hook missing on the first pass and the re-plant followed); `RENDERER_D3D_InitializeDevice` keeps the fork body whole (mode-size lookup + 800×600-class if-chain + R110–R115 ladder + fix 18 gamma degradation — upstream's switch/hardcode shapes would regress the wide stops; the `resfix:` gate pins the if-chain); R93 panel-split guards re-planted onto upstream's switch forms in D3D/DDraw/GDI (issue #26 comment markers kept for the gates); `Party.cpp`'s `UI_HandlePartyButtonTooltips` keeps the fork body whole (16 site-6 terms vs upstream's zero-term switch refactor — merging both shapes had produced a double `draw_last_text_line` label); everywhere else upstream's inlined shapes win with the `VPRES_GetWideUiOffsetY()` terms re-planted + trailing VP markers (QuestLog/Inv/ui/SpellSel site-5/site-6; SpellSel's R45 `nStartX` spell restored after the gate flagged the over-inlining); SFile's `SFILE_CalculateBufferOffset` takes upstream's `__thiscall` (header+def consistent, no fork callers); D2SoundSmp takes upstream's plain `log10` spelling (fix-10 comment kept); Roster rev36 walk caps kept on upstream's retail if/else (flatten tail/party + roster-other recycle); A1Q1's R53 marks re-planted at the reshaped else-arm call site (gate count 3 intact); CMakeLists takes `-fwrapv` inside both LEAN branches; README keeps the fork Verification section (Ghidra version absorbed 1908 → 1948); VanillaPlusCore's two includes track upstream's refiled headers. **Instruments**: gate re-pins 2 rows (endscene2 TU move, spell thunk anchor 0 → 1 with reasons) → **499/499**; include-path audit clean after the two refiled-header fixes (0 dangling, 0 case-only — covers upstream's new include-case gate class); rev48 branch audit 0 dead stores; d2boot visibility ALL VISIBLE; merge-site syntax sweep caught the ui.cpp tooltip-var class (re-expressed on upstream's inline shape) — the D3D DEVMODE residuals proven environmental (identical on the fork body with only the refiled includes swapped; the owner's windows.h build declares them). self-ID `Vanilla+0.0.7-R122` → `-R123`.

- **vanilla-plus (R122): the R121-WIP log verdict — cull rect re-fit; the last bottom band was retail's own `ScreenSizeY - 0x2f`** — owner shipped the R121-build vpdiag capture (1080p, both modes, one perspective flip) and it is the cleanest log of the campaign: every R121 fix proved in — all dumps `vpdiag[1]` with `edge=0` (the DrawEdges=0 un-gate works: phase 1 = the fill path), `fire=1248` = all four fired bits L|R|T|B with `st=12432` stamps/frame (the bands firing at their full 4×28×111 worst case), `f:n=22-29` with `qf=0 of=0` (the hop-4 walk emitting more rooms than R120's 18-26, zero silent drops), `wb` real in both modes, `anch=` at the player — and STILL `gaps=0/51..82/0/0`: the bottom band survived intact. The discriminator is line 3's `px=` against line 1's `rcv=`: the drawn-floor px AABB hugs the rect on every edge in both modes (persp `px=[-119 2024 -109 1024]` inside `rcv=[-125 -110 2030 1033]`; ortho `px=[-64 1981 -80 1000]` inside `rcv=[-80 -80 2000 1033]`) — the walk and the fill deliver world far past the screen (culled-anchor extremes `cx=[-6784 8656]`), and the retail rect refuses it. `SetViewRectangleSize` (View.cpp) builds the rect as `SetRect(-0x50, -0x50, W+0x50, H-0x2f)` plus persp deltas — the bottom margin is **−47 px: the rect stops above the backbuffer by design** (the 800×600 panel-era wedge; at 1080p that is a guaranteed dead band, and with tile quantization the drawn AABB stops at 1000-1028 → the measured 51-82 px). `DrawFloorTile`'s cull and the wall2 scene's rect test kill every tile anchored past it — including R117's own fill stamps, which is why `fire=1248` could not close the gap. Fix: one block in `SetViewRectangleSize` — at the wide classifier the rect is re-fit to the live backbuffer (`left=top=-0xc0`, `right=W+0xc0`, `bottom=H+0xc0`; covers retail's widest per-edge margin, persp left −0x7d, with slack for the perspective warp; the wall2 scene pass reads the same rect, so bottom walls un-cull with the floors). 640×480/800×600 keep the retail rect byte-identical. Gates +4 `r122:` pins, site-5 summit-clouds classifier count 2→3 (the new arm in View.cpp), self-ID `Vanilla+0.0.7-R121` → `-R122` → **499/499**. Field expectations: `rcv=` in the next 1080p log should read `[-192 -192 2112 1272]` in BOTH modes, `gaps=` should collapse to `0/0/0/0` (bare parity noise at most), and the drawn AABB stops hugging the rect at the bottom; if the perspective top corners still void with `gaps=0` there, it is wall-shaped past the emitted room set — the R122 manifest names the wall-side lever for R123.

- **vanilla-plus (R121): the R120 log verdicts — the band fill finally runs at wide stops, the walk reaches the hop-4 diagonal, and the fill triggers read snapshot bounds** — owner shipped the R120 vpdiag capture (1080p, ortho hold, then perspective toggle). Four verdicts, four fixes. **Verdict 1 — the R117 fill never ran in the field**: `edge=0` on every line in BOTH modes (`persp=1 edge=0` after the R96 toggle marks) — `bPerspectiveHasEffectOnSprite` computes from `pLevelTxt->DrawEdges` (Draw.cpp:917), A1 outdoor is 0, and the retail flag gated the whole fill+bounds block: all dumps `vpdiag[0]`, `fire=0000`, `st=0`, `anch=(0,0)`, `wb` at sentinels. The R118 "half fix" was the wide walk alone; the band fill was dead code in the exact test area. Fix: with `edge=0`, `DRAW_UpdateTileVisibilityBounds` still grows the bounds and the pass falls through to the fill when `VPRES_IsWideLayoutMode` holds — retail modes keep the retail exit byte-identical, the flag itself is untouched (no other retail consumer moves). **Verdict 2 — the ortho bottom void measured**: `gaps=0/52..86/0/0` every frame — drawn floor anchors stop 52-86 px short of the bottom edge while the left/top/right anchors sit at/beyond their edges. **Verdict 3 — the shortfall is the hop-4 diagonal**: iso projection puts the 1080p bottom screen corner at ~(5828,5748); the depth-3 walk union `a=[5520 5400 5800 5680]` stops 30-70 coords short in x and y — the corner room is one BFS edge past the collector. Fix: both walk call sites depth 3->4 (rooms the DRLG never materialized at hop 4 simply yield nothing — the probe is free). **Verdict 4 — the 74% lightmap misses are benign**: `LIGHTMAP_InitFromPlayerAndRooms` pre-fills the 48x48 ambient grid with the player room's ambient (the R118 FEATURES note), so out-of-window samples get ambient, not black — the same value retail samples; the counter stays as a drift alarm, not a defect. **Bonus fix — trigger bounds now snapshot**: each band's span walks BOTH world axes, so a band's own stamps grow the live members mid-fill; the four triggers read them live, letting an earlier band's y-growth push a later trigger's bound past the reach and silently skip that band (left fires, grows maxY, bottom under-fires). All four conditions now read `nTrigMin/MaxX/Y` captured before the first stamp. Gates +6 `r121:` pins, `r118:` walk-site pins re-locked to `, 4,`, both `VPRES_IsWideLayoutMode` count gates 2->4 (the new arms), self-ID `Vanilla+0.0.7-R121` → **495/495**. Field expectations: vpdiag now emits phase-1 lines with `wb` real, `fire` bits set, `st` in the thousands, and the bottom gap at/near 0; if the perspective top corner still voids with the floor covered, the next lever is wall-side (hop-4 walls or a wall equivalent of the fill).

- **vanilla-plus (R120): wide-view diagnostics — vpdiag lines in d2boot.log, zero behavior change** — owner after R119: "same behavior, blackness on top with perspective, blackness on bottom without" and "Is there anything we could add to d2boot.log to help diagnose the issue?". The two residuals have four candidate mechanisms (BFS walk short on an edge, the retail fixed-pixel `rcExtendedView` cull eating drawn world, the R117 fill never firing, the ambient-lightmap 48x48 grid clamping drawn tiles dark) and the two field modes discriminate between them only by which edge hurts — so R120 instruments all four and prints the discriminating numbers instead of guessing: the floor pass dumps three `vpdiag` lines every 32nd floor frame (~2x/second) plus a forced dump on every perspective flip. **Line 1** `vpdiag[N]: persp= edge= scr=WxH rcv=[l t r b] lm=(x,y)` — dump phase, perspective active + the floor pass's edge flag, backbuffer, the retail cull rect, lightmap grid origin. **Line 2** `vpdiag: wb=[minX maxX minY maxY] wanch=(x,y) f:n= a=[...] h2= h3= qf= of= s:n= qf= of=` — the floor tile bounds the fill triggers compare (sentinels in ortho = `DRAW_UpdateTileVisibilityBounds` never grew them), the walk anchor in game coords, the R118/R119 collector's real output for the floor pass (emitted rooms, hop-2/3 mix, room-union AABB, 512-queue and 256-out-cap drops) and the scene pass's drops. **Line 3** `vpdiag: seen= drw= rc= flg= cx=[...] cy=[...] px=[...] st= fire=abcd anch=(...) lm=miss/samples gaps=T/B/L/R` — tile counters, culled anchors' screen extremes, drawn-floor px AABB, edge stamps, the four fill fired bits, fill anchor, lightmap clamp rate, per-edge px gaps between the drawn AABB and the backbuffer. Field protocol: capture d2boot.log at 1080p in BOTH modes standing where the blackness shows, plus one perspective ON->OFF->ON toggle in place (the flip forces a dump pair); read map in handoff. Implementation: shared zero-behavior counters (`VPDIAG_WalkStats` + externs in Draw.h; Wall2.cpp collector and View.cpp `DRAW_CoordIsInCoord` only increment; Draw.cpp owns reset + the throttled dumper), per-frame scratch resets at floor-pass entry with sentinels, int ++/min-max on already-walking paths (no allocation, no per-tile branch beyond the counters). Gates +14 `r120:` pins — drop-counter pins match the increment (`\+\+`) after the gate caught reset+increment double-counting, clamp gate pins both sampler increments — self-ID `Vanilla+0.0.7-R119` → `-R120` → **489/489**. Field: owner d2boot.log capture owed; the fix round is R121 with the numbers in hand.

- **vanilla-plus (R119): the wide-view reach unit fix — R118's prune square was 5x too small; R117's fill triggers under-fired** — owner after R118: "ok that half fixed it, i don't see any blackness on the bottom or sides, but there is still blackness on the top edge and top corners", then the mode split that cracked it: "Previous report was about perspective mode, without perspective mode it's the opposite issue, rolling black triangle at bottom". Both residuals were ONE bug: DRLG room coords (`D2DrlgRoomCoordsStrc.dwXStart`, the `DRAW_UpdateTileVisibilityBounds` bounds, `UNIT_CenterXCoordsDiv5 * 5`) live in **game coords** (5 per subtile — retail itself compares `UNIT_CenterXCoordsDiv5 * 5` against `dwXStart`, Wall2.cpp:748), but R118's reach constant computed the screen half-reach in **subtiles** (`ScreenSizeX/32 + ScreenSizeY/16 + 10 + 80` = 217 at 1080p — the right NUMBER in the wrong SPACE): the prune square covered 43 subtiles, not the 127+ a 1080p view reaches, so hop-2/3 rooms only patched a ~700px band past the retail 3x3 block — "half fixed" (bottom/sides = the block's own coverage in big-room outdoor areas). The same subtile-scaled number fed R117's four fill open-bound triggers (`nAnchorXRaw - nMinTileX` are coord deltas), so the band fill under-fired whenever the drawn bounds sat 43–127 subtiles out — the rolling ortho bottom triangle (where the 3x3 block ends over small rooms) and the perspective top edge/corners (the warp-loaded rows past the walked square). Fix: reach computed in coords at both call sites (`ScreenSizeX * 5 / 32 + ScreenSizeY * 5 / 16 + 450` = 1087 coords at 1080p = the intended 217 subtiles), the four fill triggers compare `nFillReachCoords = nFillReach * 5` (stamp depth/span stay subtile-derived — dividing by 5 was always correct there), the BFS queue widens 128→512 and both out caps 96→256 so the now-real room coverage can't silently drop entries, and the collector's params renamed to the space they always operated in (`nCenterX/Y`, `nReach`) so the next reader can't repeat the mixup. The rev-145 suspicion of a 5x origin bug at the Wall2 call site (`UNIT_CenterXCoordsDiv5 * 5`) is retracted — that expression is retail's own. Gates +6 `r119:` pins + self-id roll → **475/475**, self-ID `Vanilla+0.0.7-R118` → `-R119`.
- **vanilla-plus (R118): the wide-view room walk — the black rim at the screen edges in every mode ≥720p (+ the R117 boot incident root-caused and fenced)** — owner after deleting the R117 harness and rebuilding: "ok that hypothesis was correct the test .cpp in /scripts/ broke the entire build. i deleted it and built again, runs now. The blackness at the edges is still there. and is present in all video modes 720p and up. 1080p sees way beyond 720p's border and yet when i switch to 720p it has a blackness at the edges. This seems like some kind of render of view limit, can we extend it?" — then ruling out lighting: "its definitely not the light radius, i am in the center of the screen and the light radius stops way way before the edge of the screen where the blackness is". Two records, one round. **(1) The R117 boot incident, on the record**: the root `CMakeLists.txt` globs `*.cpp` TREE-WIDE (`file(GLOB_RECURSE D2_SOURCES ... "${CMAKE_SOURCE_DIR}/*.cpp")`), and R117 shipped its host geometric proof INSIDE that glob — `scripts/test_r117_fill.cpp` carries its own `int main()` plus stub globals, so the "host harness" silently compiled INTO d2.exe (the exe is `WIN32_EXECUTABLE`, entry `WinMain@16`, so the stray `main` links clean and nothing detects it); the owner's build died right after the `static-init ok` boot mark, before `WinMain`, in every renderer, with nothing in any log — exactly the ctor-window signature. The fork filter (`vanilla-plus/cmake/sources.cmake`) excluded only `build/`/`thirdparty/libbacktrace/`/`asm_matching/`; it now literal-prefix-excludes **`scripts/`** too, so nothing parked there can reach the game link again (the R117 harness itself is gone — owner deleted it). **(2) The actual blackness — the owner's "render of view limit" was right**: not the light grid (`LIGHTMAP_InitFromPlayerAndRooms` pre-fills the WHOLE 48×48 ambient grid — 8×8 subtiles per cell, origin = player cell − 0x18 — with the player room's ambient color, so cells past the drawn rooms are lit, not black) and not R117's fill (it stamps `DrawEdges` level boundaries only). The real limit: **both render walks stop ONE hop out** — `DrawFloorByViewAndFloor` loops exactly `DRLGROOM_GetAdjacentRoomsList` (the player's room + direct neighbors = the 3×3 room block), and `WALL2_BuildSceneDrawList` walks the same set through `CallRoomCallback`, which is a literal `for` over `pRoom->ppRoomList` — so floors, walls, roofs, units and shadows ALL stop at the 3×3 block and the cleared backbuffer shows through as the black rim past it. Retail never needed more: an 800x600 view reaches ~25 subtiles while a 40×40-subtile room block reaches ±60–100 — always covered; the wide stops reach 85 (1280x720) to ~127 (1920x1080) subtiles — rim at 720p, most of the screen void at 1080p, exactly "1080p sees way beyond 720p's border". The rooms past the block exist: `InitNearRooms` materializes the RoomEx graph THREE hops out (hop-1 rooms ride priority 0, hop-2 rooms get `DRLGROOMEX_ActivateRoomEx` with tile data, hop-3 demote-in) — the draw walks were the only thing that stopped short. Fix (two files + the insurance): new **`WALL2_CollectRoomsBeyondAdjacency`** (Wall2.cpp, declared in Wall2.h, shared by both passes) — BFS over `DRLGROOM_GetAdjacentRoomsList` to depth 3, hop ≤ 1 walked-but-never-emitted (the retail set), candidates culled against a world-space square (center = the player's room center, reach = `W/32 + H/16 + 10 + 80` subtiles), linear-scan dedupe, 96-room cap; the **floor pass** (Draw.cpp, after the retail room loop) draws the hop-2/3 rooms through `DRAW_DrawWideViewRoomFloors` — the retail two-pass shape verbatim (pass = `(nFlags >> 0xe) & 7` tested against 1 then 2, `pParam` 1 only for pass 1 when the level has FloorFilter, same `nX/nY` screen base and `roomCoords + pos*5` world anchors); the **wall2 scene pass** feeds the same rooms to the existing `WALL2_CollectRoomTilesAndUnits`, whose per-room `rcCull` rect, per-tile screen rect, `WALL2_IsInDrawGrid` cull and the 3000-entry draw-list cap bound the cost (a fully off-screen room costs one rect test). Side benefit: the extra visible tiles grow `nMin/MaxTileX/Y` through `DRAW_UpdateTileVisibilityBounds`, so R117's open-bound tests now fire at REAL level boundaries instead of the 3×3 block edge — the mid-map flat-edge-tile overdraw window closes. Both call sites gated `VPRES_IsWideLayoutMode` — 640x480/800x600 never reach the new code (byte-identical); the wall2 grid already scales with the view (`(viewW/160 + 11)` × `(viewH/80)`-derived dim). Gates: +6 `r118:` pins (collector symbol, both call sites, both wide gates, the two-pass helper) + self-id roll `Vanilla+0.0.7-R117` → `-R118`. Field re-test owed (owner; expected: no black rim at any wide stop in normal play — world, walls and monsters render out to the screen edges; R117's edge fill appears only at true map boundaries; watch FPS at 1080p in dense areas (hop-2/3 rooms add visible tiles); if a far corner still voids at 1080p that is the DRLG materialization depth (3 RoomEx hops) — the next knob, not this round).
- **vanilla-plus (R117): resolution-scaled edge-void fill — the level-boundary black wedges at the wide stops** — owner at the new 1920x1080 stop (PERSPECTIVE on): "there's some blackness at the very edge. Is there any way to fix that?". Pixel-measured the field screenshot first: **0px solid-black band on every screen edge** (top/bottom/left/right contiguous near-fully-black bands all measure 0; only the top-left corner is 58% dead black) — not a present/letterbox/stretch artifact. It is the **D2 level-boundary void**: `DrawFloorByViewAndFloor` (D2Client/GAME/Draw.cpp) draws only the current room + adjacent rooms (`DRLGROOM_GetAdjacentRoomsList`), and retail's void skirt — `DrawSubTile` → `DRAW_DrawEdgeFloorTile` stamping the act edge tile (`GetTileDataFromAct` = `&pDrlgAct->sTileCache`) past the drawn tile bounds — is **ONE 3-tile strip per open side** on the player row, behind **30/33 + 25/23 subtile triggers sized for 800x600**, whose view reaches ~25 subtiles past the drawn bounds. A 1920x1080 view reaches (1920/16 + 1080/8)/2 ≈ **127 subtiles** into the void, so near any level boundary the void shows as black wedges at the screen edges (worst at corners). Decompile trap worth the record: the `bIsPerspective` term in the skirt's enable gate is the **PANEL border type**, not the perspective flag — the var is reassigned to `PANELSHIFT_BorderForWorld` six lines earlier — so the skirt already ran in perspective; it was simply far too small. Fix (one file, `D2Client/GAME/Draw.cpp`): replace the strip with a **band fill scaled from the live backbuffer** — reach = `W/32 + H/16 + 10` subtiles (screen half-reach in world coordinates + parity margin), depth = `reach/5 + 1` tiles stamped beyond the open bound, span = `reach*2/5 + 1` tiles stamped each way along it; four open-side bands (left/right ride anchorY, top/bottom ride anchorX); bounds/anchors captured to locals BEFORE the first stamp (`DrawFloorTile` → `DRAW_UpdateTileVisibilityBounds` grows the bounds mid-loop with the fill's own tiles); off-screen stamps cost one `CoordsMapToScreen` + `DrawFloorTile`'s `rcExtendedView` cull and nothing else, so the per-frame cost is bounded by the visible fill area (worst host-measured case 12,432 stamps when all four sides sit open on a sliver). Gate shape unchanged: `DrawEdges` levels only, VPRES wide stops only (800x600 never reaches the code — retail byte-identical there), border-0 world view (PANELSHIFT-neutralized), **ortho + perspective both**. Host geometric proof (`scripts/test_r117_fill.cpp`, g++ -Wall -Wextra clean): at every wide stop (1280x720 / 1344x700 / 1366x768 / 1920x1080) the stamped rectangle covers **100% of the on-screen void diamond at any camera parity** (corner, SE, odd-parity and all-sides-open cases, ~295k sampled void px at 1080p, zero uncovered); interior frames stamp **0**. Gates: +4 `r117:` pins (reach/depth/span formulas, 4 band call sites) + self-id roll → **462/462**, self-ID `Vanilla+0.0.7-R116` → `-R117`. Field re-test owed (owner; expected: no black wedges at screen edges near level boundaries at 1080p with PERSPECTIVE on — the void fills with the act's edge tile; watch for repeated-edge-tile patches over real floor in dense room graphs and FPS near boundaries).
- **vanilla-plus (R116): windowed 1366x768 diagonal shear fix — the GDI DIB renderer rode the logical width as scanline pitch** — owner, on the R114-era build while the R115 1080p build was compiling: "in D3D windowed mode 1366x768 everything is distorted and slanted". First fact pinned: **`-w` never reaches the D3D renderer** — `Game/Main.cpp` maps `bWINDOW → RENDERER_Windowed` (the GDI DIB renderer) ahead of every `bD3D` check, so "D3D windowed" has always been this software path (retail shape: `CreateDIBSection` 8bpp + `StretchBlt`, Y-buffer row table via `nfpFillYBufferTable`). Root cause: GDI DWORD-aligns every 8bpp scanline, so the DIB's real stride is `(width + 3) & ~3` — **1368 at the 1366-wide stop, exactly width at every other stop** (640/800/1280/1344/1920 are all 4-aligned, which is why no other stop ever sheared and the defect stayed invisible since R112 shipped mode 5 — windowed was only ever field-tested fullscreen). The renderer handed `nfpFillYBufferTable` the logical **width**, and the DirectDraw windowed renderer proves the parameter is a pitch (it passes its real `ddSurfDesc.lPitch`): the Y-buffer row table, `D2GFX_FillYBufferFast`'s `gRenderBufferPitch` (all tile drawing), the five `CELCMP_DrawCelFrame*` `nDstPitch` slots, and every raw memset/pixel/line/smacker address in `D2Gdi/Renderer/Windowed.cpp` drifted **2px per row — the bottom row lands 1,534px off**, the frame shears diagonally and wraps: "distorted and slanted". Fix (one file, `D2Gdi/Renderer/Windowed.cpp`): new `RENDERER_WINDOWED_nDIBPitch = (width + 3) & ~3` set at DIB allocation and used at **all 24 raw-buffer sites** — the `FillYBufferTable` call, `GetBackBuffer`'s source row advance (dest stays packed `width*3`), the four `CELCMP` pitch slots (clip slots stay logical — `nScaleWidth` is the horizontal clip, not a pitch), `DrawSolidRectEx`/`DrawSolidRect` scanlines, `DrawPixel`/`DrawLine`/Bresenham plots, both `ClearFramebuffer` memsets (rows still logical counts; each cleared row is the real stride, full clear covers the padding), and the Smacker blit. Byte-identical at every %4 stop (pitch == width), by arithmetic. Gates: +4 `r116:` pins (stride computation, FillY call, 4 CELCMP slots, 26-site global wiring) + self-ID `Vanilla+0.0.7-R115` → **458/458**. llvm-mingw build gate + field re-test owed (owner; expected: windowed 1366x768 renders straight — UI, world, sprites aligned; every other stop unchanged; fullscreen untouched — this fix has no fullscreen code path).

- **vanilla-plus (R115): 1920x1080 — the slider's SIXTH stop (mode 6), plus wrapper 0.4.8 entry 19** — owner: "Great job. works. Published Glide3xVK 0.4.7. Now lets do 1080p" (the paused §15 plan; the R114 wrapper entry-18 round was the template). Resolution **mode 6** (`0x780 x 0x438`), the most universally listed CEA timing — D3D almost always enters it TRUE-exclusive (retail Flip, 1:1 cursor) and the R112 stretch ladder hosts (and downscales) it on the exotic unlistable driver: `D2GFX` case 6 + the D3D mirror, `CLIENT_SetScreenResolution` case 6 + probe stop-6 dims + refusal snap-back mode 6 → position 5, slider position 5 → mode 6 in both EscMenu mappings (ahead of the `>= 2` arm) + label `L"1920x1080"` + FOURTH runtime blank (count 6), mini-panel `== 5 → mode 6` + clamp `> 5`, D3D perspective-class gate `!= 6`, DDraw tables `[6]→[7]` (`0x780`/`0x438`) + horizontal bound, GDI DIB + scale width, Glide `grSstWinOpen` enum **19** + perspective path, wide class everywhere (Glide ×2 / D3D ×2 / DDraw / GDI / VanillaPlusCore ×2 — the 800-class UI re-centers as one rigid unit, offsets X 560 / Y 480, both formulas). The round's one retail-struct change repeats R112's: `D2EscMenuItemStrc.pDC6SubImages [5] → [6]` (EscMenu.h, 0x554 → 0x558 bytes) — with the R113 lesson applied IN THE SAME COMMIT: both raw int strides `0x155 → 0x156` (mouse-Y walk + loader slot writes), the loader count clamp `> 5 → > 6` (walk cap stays 4 real path fields), the free-path halt clamp `> 5 → > 6` (slot 5 in-bounds since the growth), the draw-site clamp `> 4 → > 5`. Wrapper dependency shipped in-round: [Glide3xVK 0.4.8](https://github.com/VTSTech/Glide3xVK) `g3x_resolution_size` entry **19 = {0x780, 0x438}** + guard `> 19` (patch: [`patch/glide3xvk-0.4.8-entry19.patch`](patch/glide3xvk-0.4.8-entry19.patch), verified to apply on the published 0.4.7; host check `scripts/test_res18.cpp` — 7/8/16/17/18/19 exact, 20/255 keep the soft-fail, ALL GREEN). Gates: +21 `res6:` rows, minipanel/init-attach re-counted (4/5), strides re-pinned `r115:` 0x156, count-6 legal re-counted (3), draw-clamp re-pointed (`nDrawIdx > 5`), EscMenu.h slot pin `[6]`, self-ID `Vanilla+0.0.7-R115` → **454/454**. llvm-mingw build gate + field matrix owed (owner: D3D 1080p true-exclusive native; Glide 1080p via wrapper 0.4.8; switch ladder 800↔720↔1344↔1366↔1080; cutscene round-trip; relaunch keeps the stop).

- **vanilla-plus (R114): Glide 1366x768 fix — wrapper-side (Glide3xVK 0.4.7), no game change** — owner field matrix on the R113 build: D3D held ("ok, working good in D3D now"), but in Glide "when the Resolution lands on 1366x768 it only displays the top left 1/4 of the screen and its very zoomed in". Root cause was WRAPPER-side, exactly the dependency R112 flagged: the game's fifth stop sends `grSstWinOpen` resolution enum **18**, but [Glide3xVK](https://github.com/VTSTech/Glide3xVK) 0.4.6's `g3x_resolution_size` table only knew 0-17 — the stop took the documented soft-fail (640x480 surface + `grSstWinOpen: unknown resolution 18 -> falling back 640x480` in glide3x.log) while the game laid out for 1366x768, and the wrapper presented that 640x480 surface — holding only the top-left 640x480 of the 1366x768 layout — stretched over the full host: the top-left quarter of the frame at ~2x zoom. D3D was unaffected (its mode table lives in d2vp.exe, fixed in R112) and the Glide stops 3/4 were fine (wrapper entries 17/16 shipped in 0.4.6). Fix ships in the wrapper repo as **0.4.7**: `g3x_resolution_size` entry **18 = {0x556, 0x300}** + the unknown-resolution guard `> 17` → `> 18` (patch: [`patch/glide3xvk-0.4.7-entry18.patch`](patch/glide3xvk-0.4.7-entry18.patch); host check `Glide3xVK/scripts/test_res18.cpp` — enums 7/8/16/17/18 map exact, 19/255 keep the soft-fail). d2vp.exe is UNCHANGED this round — the R113 build stays current (self-ID `Vanilla+0.0.7-R113`, gates 434/434 untouched). Field re-test owed (owner, glide3x.dll 0.4.7): Glide 1366x768 = native fullscreen, full frame; then the R112 field matrix.

- **vanilla-plus (R113): R112 regression hotfix — the esc-menu struct growth broke the menu (mouse dead, label corruption, shutdown halt)** — field round 4, owner: "We have bigger problems, your R112 changes broke the Esc menu. The mouse can no longer navigate or select options in the menu and there is some corruption. It no longer says 'Resolution' but '640x480' instead of the word Resolution. The resolution change itself works, i can navigate the Esc menu with the keyboard still." (R111 itself held — "R111 i think was good except for the letterbox"; the letterbox is what R112 fixed.) Root cause: R112's one retail-struct change — `D2EscMenuItemStrc.pDC6SubImages [4] → [5]` (EscMenu.h, 0x550 → 0x554-byte items) — shipped with three retail-shaped consumers of the OLD stride/count still live in `EscMenu.cpp`: **(1)** the mouse hit-test `ESCMENU_GetItemIndexFromMouseY` walks the row table with the raw int stride `pItemYPosition += 0x154` (0x550 bytes) — every row ≥ 1 read the tail of the PREVIOUS row's slot array as its `nYPosition`, so no row ever hit-tested (the keyboard walk indexes `gpEscMenuCurrentTable[i]` with the compiler's stride and kept working — exactly the field split); **(2)** `ESCMENU_LoadLocalizedImages` advances the same raw stride (`nSubImageOffset += 0x154`) to compute each item's sub-image slot address (`szLocalizedImagePath + nTableOffset * 4 - 0xc`) — every item's value-DC6 writes spilled 4·N bytes early: the "640x480" image parked in the background row's new slot 4 (the draw site prefers a non-null sub-image, so the spot over the baked "Resolution" caption showed "640x480") and later items wrote raw pointer bytes into `szSubImage4Path` tails (the reload corruption); **(3)** `ESCMENU_FreeLoadedImages` still carried the retail range check `nSubImageCount > 4 → HALT(0x565)` — legal since R112: the fifth stop makes the VP Resolution row count 5, so every menu teardown after visiting the video options would have taken the fatal tail. All three re-pointed in place: both strides → `0x155` ints (the R112 0x554-byte stride), the free path accepts count ≤ 5 (its walk is array-indexed, slot 5 in-bounds since the growth; freeing the synthesized blanks is the R109-proven `DC6_FreeAndDelete` shape). No stop/mapping/label/stretch behavior changes — this round only re-points the walks at the struct R112 already shipped. Gates: `res5: escmenu count 5 legal` re-counted 2 → 3 (loader + free), +2 net-new `r113:` stride pins (a future struct change cannot silently re-arm this), self-ID `Vanilla+0.0.7-R112` → **434/434**; syntax-check error digest identical pre/post on the two touched TUs (the pre-existing D2Hell.h host-shim header noise only). llvm-mingw build gate + menu re-test owed (owner; expected: mouse hits every row at every stop, the caption reads "Resolution", no value images outside the slider row, quit after visiting video options clean).

- **vanilla-plus (R112): hosted stops stretch to fill the host + 1366x768 becomes the slider's fifth stop** — field round 3 on the dormant-mode stop: R111 held ("ok, i see the black bars"), and the bars are the R111 letterbox itself — the field box hosts 1344x700 on its native 1366x768 panel, so the frame renders top-left 1344x700 and the extra host rows/cols stay black. Two asks, one round: **(1) "center and stretch it to 1366x768 when 1344x700 is not valid"** — hosted stops now STRETCH the logical frame over the whole host mode. The scene still renders into the top-left logical rect (R111 viewport untouched); what changes is the present: new `D2Direct3D::Renderer::VPRES3D_PresentHostedFrame` stretch-blits the logical rect onto the primary (`D3DSurfaceSecondary` → `g_pD3DSurfaceBackbuffer`, src 1344x700 → dst host, `DDBLT_WAIT`) and the four present sites — `EndScene2` (per-frame), `CreateWindow`, `ResizeWindow`, `PlayCutScene` (cutscenes stretch too; Bink rides the normal frame pump) — take the Blt when hosted and keep the retail Flip otherwise (byte-identical on true-exclusive stops; a Blt refusal on an exotic backend degrades to the R111 letterbox, never a lost frame). The cursor follows the pixels: InitializeDevice arms a new D2gfx stretch channel (`D2GFX_SetPresentationStretch`/`D2GFX_GetPresentationStretch`, pure data; ReleaseDevice clears) with the host size, and `MOUSE_CaptureMousePosition` maps raw host pixels back into the logical frame (`x * logical / host`, 64-bit intermediates) BEFORE the retail clamp — clicks land where the stretched pixels are, the bottom 68 px stay reachable, and the drawn cursor sprite sits under the physical cursor. Documented trade-off: hosted presents are unsynced blits (no vblank), true-exclusive keeps the Flip. **(2) "Might as well add 1366x768 as a resolution option"** — resolution **mode 5** (`0x556 x 0x300`), the panel-standard 16:9 CEA timing (unlike dormant mode 3, most panels list it natively, so D3D usually enters TRUE-exclusive — retail Flip, 1:1 cursor — and the stretch ladder only fires on drivers that omit it): `D2GFX` case 5 + the D3D mirror, `CLIENT_SetScreenResolution` case 5, probe stop-5 dims on the same exact-or-host candidate set, slider position 4 → mode 5 in both EscMenu mappings (ahead of the `>= 2` arm) + label `L"1366x768"` + third runtime blank (count 5), mini-panel `== 4 → mode 5` + clamp `> 4`, D3D perspective-class gate, DDraw `[6]` display tables + horizontal bound `0x556`, GDI DIB + scale width, Glide `grSstWinOpen` enum **18** + perspective path, wide class (`VPRES_IsWideLayoutMode` + all seven dims pairs gain the 1366x768 arm). The one retail-struct change: the esc-menu slot array grows `pDC6SubImages[4]` → `[5]` (EscMenu.h) — the VP Resolution row is the only count-5 row, blank 4 attaches at runtime, the loader accepts count 5 but still walks at most the four real path fields (the R61 empty-path degrade keeps the blanks across re-inits), and the R61 draw-site clamp widens to slot 4. Glide note: the paired [Glide3xVK](https://github.com/VTSTech/Glide3xVK) wrapper needs `g3x_resolution_size` entry 18 = `{0x556, 0x300}`; older wrappers fail soft (640x480 + "unknown resolution" log), same contract as entries 16/17. Slot ledger for row 15 (1080p): mode 6 / stop 6 / entry 19 / `[6]`. Gates: +21 `res5:` rows +6 `stretch:` rows, `init attach` re-counted (4), minipanel gate re-pinned (`> 4` + mode-5 arm), draw-covers/probe-gates/glide-path gates re-pinned to the widened shapes, self-ID `Vanilla+0.0.7-R112` → **432/432**; syntax-check: zero errors in the twelve touched TU bodies (pre-existing `__time32_t`/D2Hell.h host-shim header noise only). Record: PLAN §10, FEATURES §14 field round 3. llvm-mingw build gate + D3D re-test pending (owner; expected at 1344x700: edge-to-edge stretched frame, no bands; at 1366x768: true-exclusive native mode).

- **vanilla-plus (R110 hotfix → R111): D3D 1344x700 fullscreen presentation — probe-first, host-sized** — field round 2 on the dormant-mode stop: R110 (hotfix overlay, mark bumped only at R111) fixed the D3D crash ("that kinda worked. doesn't crash"), but fullscreen D3D + the 1344x700 stop then showed **a borderless window instead of fullscreen**. Second emulation behavior pinned: the Win10/11 DDraw fullscreen emulation can also ACCEPT a `SetDisplayMode` it cannot actually set (fake success — no error to react to) and fall back to a borderless window at the requested size on the unchanged desktop, so R110's retry ladder never fired on the field box (and had it fired, R110's `MoveWindow` to the logical 1344x700 inside the 1366x768 host mode would still have left the desktop peeking past the window edges). **R111 fixes both shapes**: (1) `RENDERER_D3D_InitializeDevice` now probes the driver's 32-bpp mode list (new file-local `VPRES3D_IsDisplayModeListed`, twin of the client helper) BEFORE any mode-set — an unlisted stop goes straight to the first listed host (1366x768 → 1360x768), so the doomed call is never made and every mode-set is real; (2) hosted stops size the game window to the HOST (true-exclusive paths stay retail byte-identical); the flip chain auto-sizes to the host mode (no DDSD_WIDTH/HEIGHT), the viewport/render coordinates stay 1344x700, bands stay black via the R110 locked-rows clear. Same commit: the client probe gained a depth-aware base (`VPRES_IsDisplayModeListedAtDepth`) and a **DirectDraw exact-only branch** — the 8-bit DDRAW renderer has no letterbox mechanism, so it enters stops 3/4 only when the driver lists the mode at 8 bpp (extinct on modern drivers → graceful refusal; closes the same-class crash DDRAW still carried). Expected D3D presentation: display switches to 1366x768, frame top-left 1344x700 with black bands right/bottom, no desktop visible; FOG log line `[vanilla-plus vpres] stop 1344x700 is not in the display mode list: hosting fullscreen at 1366x768`. Gates: retry-log pin replaced + counts re-pinned + 4 net-new rows + self-ID `Vanilla+0.0.7-R111` → **405/405**; syntax-check error digests identical pre/post on the three touched TUs (pre-existing D2Hell.h host-shim noise only). Record: PLAN §9.1. llvm-mingw build gate + D3D re-test pending (owner).

- **Repo/docs (zero source changes)**: the repository is renamed `D2LoD-libslang-vk` → **`D2VP-QoL`** (owner, 2026-11-03) — every markdown reference and URL re-pointed to the new slug (old GitHub URLs redirect regardless). The glide3x.dll wrapper is now its own repository, [`VTSTech/Glide3xVK`](https://github.com/VTSTech/Glide3xVK) (formerly the `Glide3xVK` branch of this repo; latest wrapper tag 0.4.6) — wrapper pointers in the docs go to the repo now, and the stale `Glide3xVK` branch on this remote is retired.

- **vanilla-plus (R106): `mana_burn_fix`** — opt-in fix for the retail 1.14d Mana Burn bug: Mana Burn unique monsters drained 256x the intended mana with melee hits (512x in Hell), emptying a mana pool in one blow. With `[features] mana_burn_fix=1` in `d2vp.ini`, a Mana Burn melee hit drains the designed 66-100% of the monster's level damage instead (33-50% for Nightmare/Hell minions); Nightmare/Hell still drain x1.4/x2 — the difficulty resist penalty the retail code applies to the drain fields is left alone, matching D2R's own post-2.4 behavior. Root cause (Basin wiki: "it is multiplied by 256 when applied by a melee attack. Fixed in D2R"): the Mana Burn umod stores its drain stats already in the mana 1/256 fixed point (Umod.cpp `* 0x100`), and the melee drain roll shifts them another 8 bits (`DAMAGE_RollElementalDamageWithBonus`'s `<<8`/`*0x100`, built for the plain-point fire/cold/lightning stats); the missile path reads the stats raw and was always correct. Default OFF = retail byte-for-byte. New server-side gate TU `D2Game/UNIT/VanillaPlusCombat.{h,cpp}` (2 s ini hot-read, shared-stash shape) + VP-marked drain-roll shim and three marked call sites in `D2Game/UNIT/SUnitDmg.cpp`; key documented in the auto-created `d2vp.ini` template; 6 new merge-gate rows. Field QA + llvm-mingw build gate pending (owner). (Landed as `Vanilla+0.0.7-R107` after the owner corrected the shim in place; that round carried no changelog entry of its own.)

- **vanilla-plus (R108): `mana_burn_fix` joins the Vanilla+ Options page** — the R106 toggle is no longer ini-only: `Esc → Options → Vanilla+ Options` grows an eighth row, **Mana Burn Fix**, on the same `D2VP_Get/SetFeatureToggle` bridge as the other seven (one `gaVanillaPlusRows` entry — the ini template block shipped with R106, so nothing else changes). The server gate keeps its 2 s hot-read (the stash-server shape — same latency as an external ini edit), so a panel flip applies on the next drain roll within 2 s; the panel draw re-reads the key fresh every frame and shows the flip immediately. The eighth row moved the Previous Menu row down one pitch (`VPPAGE_BACK_ROW_Y` 0x144 → 0x164) and grew the box (`VPPAGE_BOX_HEIGHT` 0x171 → 0x191, same 0x13 bottom margin; fits 640x480). Self-ID re-pinned to R108 (the R107 round never re-pinned it) + the row-count/back-row shape gates re-pinned + 1 new row-present gate → **387/387**.

- **vanilla-plus (R109): 1344x700 — retail's dormant mode 3 becomes the video-options slider's fourth stop** — the resolution Blizzard's 1.14d binaries always carried but never exposed: `D2GFX_GetWindowSizeByResolutionMode` case 3 = `0x540` x 700 and the renderer-side mode-3 branches (GDI DIB alloc + scale width, the DDraw table slot the 720p round filled, D3D's size case, the Glide wrapper's `g3x_resolution_size` entry 17 over in [Glide3xVK](https://github.com/VTSTech/Glide3xVK)) were in the binaries all along — only the client side was missing. **Gap A**: `CLIENT_SetScreenResolution` gains `case 3` (ScreenSize 1344x700, `nScreenMode = 1` — the same retail downstream flow the 720p case rides). **Gap B**: the slider's fourth stop — a second Route-B blank attaches into `pDC6SubImages[3]` at init (count 4; the slot was pre-reserved), both position→mode mappings add `3 → mode 3` tested ahead of the `>= 2` arm (positions 0–2 keep their meaning — a 720p registry never re-numbers), and `UI_SLIDER_DrawResolutionValue` generalizes to a per-stop label table (`L"1280x720"` / `L"1344x700"`, the issue-#23 right-align geometry proved label-agnostic; a failed 1344 attach degrades to the 720p stop via the existing load clamp). **Gap C** (the tree-verify catch, 1344x700-PLAN.md §8): `SetupMiniPanel` — which re-applies the saved position at every game-enter (`GAME_InitClientSide`) and cutscene-exit (GS packet 0x61) — kept its own `> 2 → 1` clamp that would have reset 1344x700 players to 800x600; the clamp widens to `> 3` and a `== 3 → mode 3` arm joins ahead of the `>= 2` arm. Not `[features]`-gated — opt-in by picking the stop, same as 720p; a stray position-3 on an older build clamps per-path (720p via the video-options clamp, 800x600 via the old mini-panel clamp). Drive-by: the stale GDI decompiler comment "1360x700" corrected (`0x540` = 1344). Gates: 7 new `res3:` rows + `init attach` re-counted + minipanel clamp gate re-pinned + self-ID `Vanilla+0.0.7-R109` → **394/394**; `tools/syntax-check.sh` pre/post error digests identical on all four touched TUs (pre-existing host-shim noise only). Plan: [1344x700-PLAN.md](1344x700-PLAN.md) (tree-verify record §8; QA matrix §5). **Field round 1 (owner QA): Glide GREEN** (owner screenshot — the stop live under glide3xvk 0.4.6); **D3D crash caught on the same pass and fixed in-round** — on Vista+ the exclusive DD7 `SetDisplayMode` only accepts timings from the driver's display-mode list, 1344x700 is a non-standard timing retail never shipped so no driver lists it (1280x720 is CEA-standard, which is why 720p never hit this), the rejected mode-set failed `RENDERER_D3D_InitializeDevice`, and D2Win's `D2GFX_SetVideoMode` treats a failed resize as fatal ("Failed to resize window... this is fatal!") — Glide immune (the wrapper sizes its own swapchain, never touches the display mode). Two-layer fix, one candidate set (exact mode, else 1366x768, else 1360x768): the D3D renderer retries a rejected mode with those letterbox hosts (viewport and all render coordinates stay 1344x700 — the larger flip chain shows as inert black bands; `ClearBackbuffer` now clears the LOCKED surface's row count, byte-identical on exact-fit), and `CLIENT_SetScreenResolution` probes the driver's 32-bpp mode list (`EnumDisplaySettingsA`) before committing stops 3/4 on DDraw/D3D, refusing cleanly (screen keeps the running mode, saved slider position snaps back) when neither exact nor host is listed — no surviving path can reach the fatal resize; boot covered by the ladder. Gates: +7 `res3:` D3D/probe rows → **401/401**; record: PLAN §9. llvm-mingw build gate + D3D re-test pending (owner).

## [R105] — 2026-11-04 (the sixth upstream sync — upstream `cc23648d..e2e9ec61`; self-ID `Vanilla+0.0.7-R105`)

Merge round, no owner-visible behavior change intended: six upstream commits
(`2595bd92`/`ccc7d063`/`e2e9ec61` msvc8 evidence bumps, `9addb0ec` six VC8
near-miss shapes, `3052237a` five more VC8 shapes, `fbdeca80` the structural
one), 124 files, +1,732/−2,059, Ghidra DB → **1894**, byte-match now 6,560 of
14,642 functions (44.8%) / 21.14% of bytes. `fbdeca80` folds the D2Multi
launcher file into ChatDlg (deletes `D2Multi/Launcher.{cpp,h}`,
`Launcher.data.h`, `Launcher.cpp.map`); no fork surface referenced the
launcher — verified by grep before staging.

Four conflicted files, resolutions per `docs/merge-upstream.md`:

- **`D2Client/GAME/RosterPets.cpp`** (the R50 pet-recolor sites, issue #8) —
  upstream `9addb0ec` re-spelled both writes in the VC8 near-miss shape
  (per-call named locals) while keeping the transcribed inverted polarity
  `!bHostile`. Convergence-conflict class (1794 rule): upstream's structure,
  fork's field-verified value — the calls keep upstream's new per-call-local
  shape but carry the raw flag (`BOOL bHostileSeq/bHostilePal = bHostile`),
  with dated supersession notes at both sites. The R50 gate row re-pinned to
  the superseding spelling (raw=4 across `bHostileSeq`/`bHostilePal`, and the
  inverted check now also catches upstream's `bNotHostile` spelling).
- **`D2Client/UI/Party.cpp`** (resolution site-6) — upstream inlined the
  `GetLocaleString` call (dropped the hoisted `wszMessage` local, codegen
  shape); the fork's `+ VPRES_GetWideUiOffsetY()` term re-planted onto
  upstream's single-call shape with the trailing `// VP: [resolution site-6]`
  marker. Site-6 gate count unchanged (54).
- **`D2Game/GAME/Clients.cpp`** (`GetGuildName`) — upstream recognized retail's
  plain `strcpy` shape; NOT taken: the rev35 bounded copy is the fix (a
  corrupt 32+-byte guild name would run off the 0x5B packet area), so the
  fork loop stays with a dated note for the next merge's auditor.
- **`README.md`** — fork Verification section kept; upstream's Ghidra version
  and evidence numbers absorbed (1892 → 1894).

Audit battery (merge-procedure step 4, all green): gates **380/380**;
rev47 placement audit pre/post-merge diff shows zero marker losses (the 12
GONE rows are the documented R102 re-homing residue; the only NEW row is the
intentional Party.cpp marker); rev48 wrong-branch audit 77 stores / 0
dead-store hits; d2boot include-visibility ALL VISIBLE; merge-site syntax
sweep (step 4e) — 29 TUs that differ from both parents compiled three-ways,
**0 unique-to-merge errors**. Self-ID bumped `R104` → **`R105`** (code round:
the three resolution sites are source changes).

## [R104] — 2026-11-04 (per-TU restructure fallout sweep — self-ID `Vanilla+0.0.7-R104`)

The `962f88d5` per-TU restructure left five fork anchors naming globals that
went TU-private — the owner's second post-R102 build break was SCmd.cpp's
roster-state mark (`Roster::gpRosterOther` / `pRosterPartyList`, whose
declarations moved to `Roster.data.h`). All sites fixed and verified by a new
full-fork-surface host sweep (`scripts/fork_surface_syntax_sweep.sh`: 90 TUs,
fork body vs upstream body of the same TU, baseline-diffed):

- `SCmd.cpp` — include `Roster.data.h` (the upstream consumer convention).
- `VanillaPlusCore.cpp` + `VanillaPlusOptions.cpp` — the esc-menu tables
  (`gaEscMenuOptionsTable` / `gaEscMenuHeaderOptions` /
  `gaEscMenuDeleteCharTable`, now static in EscMenu.cpp) and the font cache
  (`gaFontCache` / `gpfnFontGetCharWidth`, now static in D2WinFont.cpp) read
  through new VP accessors; the options-table sizeof assert became a runtime
  count check in the self-check; `ui.data.h` include for the UI-flag trio.
- `Direct3D.cpp` fix-27 — the pl2 UI-transpose base via
  `D2WinPalette::VP_GetTransposeBase()`.
- `ShadowDiag.cpp` — the sprite-cache buffer via `Glide::VP_GetSpriteBuffer()`
  (dump-time read, same semantics as the old global read).

Each accessor block is one VP-markered append at the TU's namespace close, so
the upstream files stay byte-minimal for the next sync. Gates 380/380; the
sweep's six remaining flags are host-shim noise only (`__time32_t`, the Win32
thread API — present in the upstream bodies too).

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
[GitHub releases](https://github.com/VTSTech/D2VP-QoL/releases).

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
- **The R54/R85 gossip hand-tables** ([#13](https://github.com/VTSTech/D2VP-QoL/issues/13)
  / [#22](https://github.com/VTSTech/D2VP-QoL/issues/22)): upstream
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
  the `Glide3xVK` branch — its own [`Glide3xVK`](https://github.com/VTSTech/Glide3xVK) repo since 2026-11-03 — which upstream does not carry).

### Changed

- Gates: 10 stale expectations updated to the superseding state with R87-merge notes
  (the epsilon quartet, the `-2000.0` `(long double)` cast, gossip sizes ×6,
  Bloodraven turn-in `0x5c` now expected ×2 — retail carries bMenu 0 and bMenu 2
  variants); the self-ID row repinned R87 → R88. Suite **326/326**.
- `D2GFX.cpp`: upstream's `uVar1` signed/unsigned model taken in
  `D2GFX_InitializeLineBuffer` (the fork-side comparison rewrite would have read an
  uninitialized variable).

## [R87] — 2026-11-02 ([`R87`](https://github.com/VTSTech/D2VP-QoL/releases/tag/R87) @ `486b936c`, self-ID `Vanilla+0.0.7-R87`)

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

## [R86] — 2026-11-02 ([`R86`](https://github.com/VTSTech/D2VP-QoL/releases/tag/R86) @ `3ca2da40`, self-ID `Vanilla+0.0.7-R86`)

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

## [R85] — 2026-11-02 ([`R85`](https://github.com/VTSTech/D2VP-QoL/releases/tag/R85) @ `71f4772f`, self-ID `Vanilla+0.0.7-R85`)

### Fixed

- A1Q2 "Sisters' Burial Grounds" turn-in: Blood Raven killed but the Kashya
  reward (rogue mercenary) could not be claimed
  ([#22](https://github.com/VTSTech/D2VP-QoL/issues/22)) — the same
  truncated-gossip-table regen defect as
  [#13](https://github.com/VTSTech/D2VP-QoL/issues/13): the A1Q2
  gossip table was emitted singular (accept line only) while the handler reads
  slot 3 whenever RewardAwaits is set, so the turn-in line never existed and
  the reward gate was never asked. Table widened to 5 slots (slot 3 = turn-in
  line 0x5c, pinned by the quest's own gate; flavor slots left empty rather
  than invented), Kashya chain marks added. Gates 311 → 320;
  **field-proven — the owner closed the issue 2026-11-02** (state completed,
  same day). Track record: HISTORY.md H35–H36. (The hand-built table was
  superseded at R88 by upstream's retail-exact table — see [R88].)

## [R84] — 2026-11-02 ([`R84`](https://github.com/VTSTech/D2VP-QoL/releases/tag/R84) @ `63e020a3`, self-ID `Vanilla+0.0.7-R84`)

The item-tooltip zero fix ([#16](https://github.com/VTSTech/D2VP-QoL/issues/16)),
built and field-verified by the owner the same day; SMOKE §J items 37–38 field-proven
at tag.

### Fixed

- Item tooltips printed "+0 cold damage" / "0 poison damage over 0 seconds"
  whenever a roll's range collapsed to a single value
  ([#16](https://github.com/VTSTech/D2VP-QoL/issues/16)) — the single-value
  elemental/poison branches of `SKILLDESC_BuildStatDescription` called
  `UNICODE_FormatWideString` with no varargs (a latent retail-1.14d bug the fork's build
  layout makes visible). Tooltip-only: item stats and generation untouched. Gates
  308 → 311; SMOKE §J items 37–38 field-proven on the owner's R84 verify pass.

## [0.0.7] — 2026-11-01/02

The first 0.0.7 build is the R66 content state (tag `R67`, self-ID `Vanilla+0.0.7-R66`,
release notes covering R59→R66); the R67→R77 block shipped as tag
[`R77`](https://github.com/VTSTech/D2VP-QoL/releases/tag/R77)
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
  ([#21](https://github.com/VTSTech/D2VP-QoL/issues/21), R81) — the two
  `waygateicons` draws were the only site-6 panel misses left in the wide-Y retrofit.
- Video-options 1280x720 label floated a label-width past its column and ~4 px above the
  neighbor rows ([#23](https://github.com/VTSTech/D2VP-QoL/issues/23), R83) —
  now right-aligned to the value column on the neighbors' ink line.
- Alt-toggle stale selection: standing on a latched label and walk-clicking far away
  picked the item up
  ([#15](https://github.com/VTSTech/D2VP-QoL/issues/15), R58) — root-caught v2
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
  ([#24](https://github.com/VTSTech/D2VP-QoL/issues/24), 2026-11-02 — the first
  full-belt field pass on a 0.0.7 build).
- `tools/syntax-check.sh` host syntax net added (R71) so a text gate can never again be
  the last line of defense; README restructured and the shipped client renamed
  `d2vp.exe` across the docs (R74/R96).

## [0.0.6] — 2026-10-30 ([`D2LoD-Vanilla-Plus-0.0.6`](https://github.com/VTSTech/D2VP-QoL/releases/tag/D2LoD-Vanilla-Plus-0.0.6) @ `0eb2655`, self-ID `Vanilla+0.0.6-R45`)

The 720p release: native 1280x720 widescreen mode; loading-screen freeze fixed; sound
constants repaired; the Vanilla+ Options page visible; inventory, skill flyouts and
dialogs aligned at 720p. Post-release, the owner kept shipping 0.0.6-line repair rounds
as tags `R47`–`R62`; all of it folded into the first 0.0.7 build.

### Fixed (post-release 0.0.6-line rounds)

- Glide `-3dfx` wrapper route crashed on the first frame (R46, field-verified
  "ok Glide i working now",
  [#11](https://github.com/VTSTech/D2VP-QoL/issues/11)).
- Auto Gold Pickup restored (R47,
  [#10](https://github.com/VTSTech/D2VP-QoL/issues/10)) — gold piles inside
  pickup range collected via the game's own server-side pickup path.
- 720p stash deposit/withdraw gold dialog misaligned (R48,
  [#12](https://github.com/VTSTech/D2VP-QoL/issues/12)) — the VP riders had
  resolved into the allocation-failure branch (a new wrong-branch merge-casualty class;
  `scripts/rev48_wrong_branch_audit.py` added).
- Unique monsters rendered base colors instead of their unique palettes (R49,
  field-verified, [#9](https://github.com/VTSTech/D2VP-QoL/issues/9)).
- Necro minion skeletons rendered gold — two inverted-polarity palette writes (roster
  pet-recolor R50 + spawn-time fallback R51,
  [#8](https://github.com/VTSTech/D2VP-QoL/issues/8); field verify still
  pending — the one open defect).
- 720p belt hover popup collapsed on the next mouse event (R52,
  [#14](https://github.com/VTSTech/D2VP-QoL/issues/14)).
- A1Q1 Den of Evil never completed: the reward gossip entry was missing from the tree
  data (R54, [#13](https://github.com/VTSTech/D2VP-QoL/issues/13)).
- Alt-toggle stale selection (R58,
  [#15](https://github.com/VTSTech/D2VP-QoL/issues/15)) — see 0.0.7.
- Save-and-re-enter crash (R60 probes + R61 fix,
  [#19](https://github.com/VTSTech/D2VP-QoL/issues/19)) — the VP Resolution row
  mutated the static esc-menu table; loader guards degrade to marks + draw-site NULL
  guard. Field-verified ("R61 Confirmed fixes #19").

### Synced

- Upstream to Ghidra 1783 (R62, merges `3d645e7` + `19f9791` + `d6d4b53` + two
  compile-fix rounds) — field-run green and the full SMOKE checklist passed on the same
  build (owner, 2026-11-01).

## [0.0.5] — 2026-10-29 ([`D2LoD-Vanilla-Plus-0.0.5`](https://github.com/VTSTech/D2VP-QoL/releases/tag/D2LoD-Vanilla-Plus-0.0.5))

Per-class player sounds restored (77 values byte-matched from the retail exe);
full-inventory pickup crash and walk-click mis-pickup fixed; **Persistent Globe Text**
(the 0.0.4 globe-hover readout reworked to retail's own always-on above-orb display).

## [0.0.4] — 2026-10-29 ([`D2LoD-Vanilla-Plus-0.0.4`](https://github.com/VTSTech/D2VP-QoL/releases/tag/D2LoD-Vanilla-Plus-0.0.4))

Auto Gold Pickup; globe hover values.

## [0.0.3] — 2026-10-28 ([`D2LoD-Vanilla-Plus-0.0.3`](https://github.com/VTSTech/D2VP-QoL/releases/tag/D2LoD-Vanilla-Plus-0.0.3))

Portable saves (`[features] portable_save` — saves read/write `<game folder>\save`,
registry untouched); alt-toggle click-through fix; diagnostics quieted (probe
retirement).

## [0.0.2] — 2026-10-28 ([`D2LoD-Vanilla-Plus-0.0.2`](https://github.com/VTSTech/D2VP-QoL/releases/tag/D2LoD-Vanilla-Plus-0.0.2))

First opt-in feature: the alt-key item-label toggle (`[features] alt_toggle`), with
self-provisioning `d2vp.ini`.

## [0.0.1] — 2026-10-27 ([`D2LoD-Vanilla-Plus-0.0.1`](https://github.com/VTSTech/D2VP-QoL/releases/tag/D2LoD-Vanilla-Plus-0.0.1))

First public release. Glide-renderer defect series closed (fixes 1–13), item drops
fixed, sound fixed.

## Wrapper tags

`0.3.1`, `0.4.1`–`0.4.6` (the [Glide3xVK](https://github.com/VTSTech/Glide3xVK) repo — formerly the
branch of the same name here) — `glide3x.dll` wrapper versions; the
current release 7z ships wrapper 0.4.6.
