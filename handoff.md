# 720p branch — handoff for future work

*Rev 34 (2026-10-31, loading-freeze ROOT-CAUSE + FIX round — **current task**): the owner's expanded d2boot.log came back with a hangwatch dump — main thread stuck INSIDE S→C packet dispatch (0x5B → roster) re-entering handlers forever, not idle. Root cause: cross-thread packet-queue race — the SP loopback enqueue (`HandleIncomingBufferAndCreatePacket`, server/qserver thread) mutated `pD2PacketClient_to_0xAE`/`_from_0xAF` with no lock while the main-thread pump popped them under `lpCriticalSection_00882ce8`; one torn interleave cycles the chain and the pump never drains (no main-loop beat, frozen loading screen). Fixed in 4 files (enqueue serialized; variable-size `default:` returns 0 instead of success-with-size-0; zero-consumption dispatch guard; pump drain watchdog), 4 new "d2boot-fix:" gates, suite 151/151. Overlay zip delivered. — Rev 33 (2026-10-31, loading-freeze diagnostics round): merge+vanilla-plus port compiles, boots, title screen + char select fine, single-player freezes at the end of the loading screen before entering the world. Owner directive: **diagnose from current code + d2boot.log only — no git-history archaeology** ("dont look at history. look at current code"). Owner deliverable preference: **overlay zips of changed files, not .patch files**. This round expands d2boot.log along the whole SP enter-world handshake (8 files, 36 anchored edits, gates 147/147) so one more owner run pinpoints the stage; reading guide + triage table in §8. Rev 32 (2026-10-30, gold-dialog round): owner field-reported the stash "deposit gold" panel misaligned at 720p — the modal gold-transaction dialog (deposit/withdraw, stash/inventory/cube entries, `ui::UI_ShowGoldTransactionDialog`) anchored its background at raw 800-space (215,140) with all four sub-widgets (spinner 0xdf,0xdb; edit box 0x102,0xe4; OK 0xfa,0x11f; cancel 0x163,0x11f) in absolute 800-space too, so it sat one height-delta too high and one width-delta too left. Fixed at the construction site: dialog origin + sub-widget coords ride `VPRES_GetWideUiOffsetX()/Y()` (X: keeps vanilla's screen-centered placement, 400→640; Y: bottom-ride, 140→260). Draw and hit-test read the same stored coords, so both shift together — no inverse form needed. 0 at 800×600. Gates: ui.cpp Y 6→11 + new "gold dialog X shifts" x5 (suite 130/130). **Owner field-confirmed the same day: "Great job. Deposit Gold is aligned now."** Sibling dialogs still at raw 800-space (trade gold prompts, cube UI dialog, NPC menus, EscMenu) — exercise in §6 and ride any that misalign. Rev 31 (2026-10-30, renderer round): owner field-tested the providers at 720p — **Direct3D fullscreen (renderer 6) WORKS** ("-w is renderer 1, fullscreen is 6 so D3D does work"); the fps overlay's "Video: Software" line is the honest name of the windowed GDI provider (renderer 1). **Glide (renderer 4) fixed the same day**: `RENDERER_GLIDE_InitWindow` clamped every non-640 stop to an 800x600 surface AND left `gbGlidePerspectiveMode` 0 at a non-800x600 size — where `RENDERER_GLIDE_InitTextureMemory`'s non-perspective branch is gated on width==800 && height==600 and never runs, so the caches stay uninitialized (the D3D black-screen trap). Now: size table mode 4 = 1280x720 / mode 3 = dormant 1344x700, grSstWinOpen resolution enums 16/17, modes 2/3/4 take the perspective=1 path; 4 "resfix: glide*" gates (suite 129/129). **Requires the paired Glide3xVK wrapper patch** (`glide3xvk-720p-resolutions.patch` → `g3x_resolution_size` entries 16/17 + unknown-threshold 17) applied on branch `Glide3xVK`; older wrapper builds fail soft (640x480 + log line). Rev 30 (site-6 field-confirmed): belt potions and the inventory panel are correct (draws AND clicks; owner: "Great job. Belt Potions and Inventory Panel are correct"). Rev 29: the vertical half of the shift (§4) coded — `VPRES_GetWideUiOffsetY()` plus ~225 Y-ride sites across 9 files, gates "site-6:" (17 rows). Rev 28 history: handoff refocused on 720p, pre-720p content (revs 14–27, §1–§13) moved verbatim to HISTORY **H24**. Engineering record: [`docs/resolution-widelayout-audit.md`](docs/resolution-widelayout-audit.md); gates: `tools/check-vanilla-plus.sh` ("site-5:" + "site-6:" + "resfix:" rows).*

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
| *(sandbox, uncommitted)* | **site-6 fix round** — the Y half: `VPRES_GetWideUiOffsetY()` (`ScreenSizeY - 600`, clamped ≥0), ~225 Y-ride sites in VanillaPlusCore/Inv/Panel/Party/Waypoint/QuestLog/Text/ui/Engine/Help, coord-cache key keyed on the offsets (mode 3 vs 4), input paths get the inverse form; 17 "site-6:" gates |
| *(sandbox, uncommitted)* | **glide 720p round (site-7)** — the Glide provider learns the Vanilla Plus stops: size table (mode 4 = 1280x720, mode 3 = dormant 1344x700), `grSstWinOpen` enums 16/17 (contract with the Glide3xVK wrapper's `g3x_resolution_size`, patched on branch `Glide3xVK`), modes 2/3/4 take the perspective=1 init path; 4 "resfix: glide*" gates (suite 129/129) |
| *(sandbox, uncommitted)* | **gold-dialog round (site-6 addendum)** — the stash deposit/withdraw gold modal rides the wide offsets: 5 coord sites in `ui::UI_ShowGoldTransactionDialog` (bg 215,140; spinner 0xdf,0xdb; editbox 0x102,0xe4; OK 0xfa,0x11f; cancel 0x163,0x11f — all `+ VPRES_GetWideUiOffsetX()/Y()`); gates ui.cpp Y 6→11, new X gate x5 (suite 130/130) — **owner field-confirmed** |

## 3. Field status (2026-10-30)

Owner, first in-game round at 720p: **"it just started displaying at 720p. there is still a lot of UI and Menu alignment issues. I do have it built already."** Screenshot (windowed GDI): VIDEO OPTIONS centered and readable at 1280x720; world renders; bottom interface bar correctly bottom-anchored and horizontally centered; **belt potions floating one bar-height above the bar**; owner: **"same issue occurs with inventory panel"**. Diagnosis below — the X half of the site-5 shift works; the Y half is missing where Y is *not* bottom-anchored.

**Fix round coded the same day (site-6, sandbox — not yet built by owner).** Every fixed-800-space Y now rides the bottom-anchored art by `ScreenSizeY - 600`; input paths use the inverse. Belt draw + hit test, inventory page/grid/body rects, border frame, hireling/trade texts, party panel (draw AND click rects), waypoint (draw AND list/tab hit tests), quest log (draw AND buttons), help screen, stat tooltips, modal popups. Gates 125/125. §6 is the field checklist for this build.

**Owner field round 2, same day (site-6 build): headline defects CONFIRMED FIXED.** Owner: "Great job. Belt Potions and Inventory Panel are correct" — belt potions sit in the bar slots and the inventory panel aligns at 720p, draws and clicks both. This validates the §5 mechanism end-to-end (Y-ride primitive + table-rect shift + hit-test parity + coord-cache key). Still to exercise from §6: waypoint / quest log / party / help / trade-stash-cube-bank panels, NPC shop, EscMenu, in-game 800↔720 resolution switch (coord-cache refetch), and the 800x600 byte-parity regression pass.

**Owner field round 3, same day (providers): "-w is renderer 1, fullscreen is 6 so D3D does work. Glide doesnt work right now."** Direct3D at 720p = field-confirmed WORKING (the 0c52c70 dispatch fixes hold under a real fullscreen run). Glide root-caused and fixed the same day (§2 glide row): the provider opened an 800x600 surface for every non-640 stop and skipped the perspective=1 init path — the exact trap the 0c52c70 D3D fix documented. With the paired wrapper patch, `-3dfx` at the 1280x720 stop opens a real 1280x720 Vulkan swapchain. Note for triage: "Video: Software" in the fps overlay is CORRECT on `-w` — that is the windowed GDI provider's name (Rendering.cpp maps renderer 1 → "Software"), not a defect; fullscreen D3D reads "Video: Direct3D".

**Owner field round 4, same day (CI + panels): CI trigger fixed owner-side (`build.yml` `branches: [720p]`, run green); local build finished — "Glide works!" — then "the 'deposit gold' panel from stash is not aligned in 720p."** Root cause + fix = the §2 gold-dialog row (site-6 addendum): five absolute 800-space coords in `UI_ShowGoldTransactionDialog`, now riders. Same fix class as belt/inventory; construction coords feed both draw and input so no inverse form is needed. Sibling dialogs still at raw 800-space coords (vanilla-correct at 800×600): trade gold prompts (Trade.cpp 0x140,100), Horadric cube UI dialog (D2Client.cpp 0x96,0x82), NPC menus (dynamic positions), EscMenu prompts — exercise them in the §6 pass and ride any that misalign.

**Owner field round 5, same day: "Great job. Deposit Gold is aligned now."** — the gold-dialog round is field-confirmed (draws and clicks). Remaining §6 coverage: waypoint / quest / party / help deep-pass, sibling dialogs, in-game 800↔720 switch (coord-cache refetch), 800×600 byte-parity regression.

## 4. Live defect: the vertical half of the shift (root-caused in-tree)

Site 5's mechanism re-centers X only. Two Y anchoring styles coexist in the 800-class code, and they disagree at 720p:

- **Bottom-anchored** (`gnPanelOffsetY - const + ScreenSizeY`, bar/orb art at `ScreenSizeY`): self-adapts — everything shifts down by `ScreenSizeY - 600` (+120 at 720p). No action needed.
- **800-absolute table Y and top-anchored raw constants**: stay at their 800x600 values → land exactly one height-delta too high.

CONFIRMED — belt (the floating potions): every Belts.txt consumer in Panel.cpp shifts X and leaves Y raw. Item draw `DrawItemGfx(pItem, aRectItem[0] + offset, aRectItem[2])` (:2045), green/red slot highlights (:2024, :2041, helper :789), hotkey text (:2071), and the belt hit test `PANEL_GetPanelLayoutHeight` (:1471–1494 — Y pair never offset, so clicks target the phantom 800-space slots too). Bar art sits at 624..720; belt rects stay at ~545..604.

CONFIRMED — inventory: `VP_SHIFT_InvPageCoords / InvGridCoords / InvBodyCoords` (Inv.cpp :140–156) shift X bounds only; the panel art is bottom-anchored (4-tile `gnPanelOffsetY - 0xe0/-0x30 + ScreenSizeY` pattern, e.g. ui.cpp :5045–5054) and moves +120 → grid/body/page rects sit 120 px above their art. Same for hireling / trade / bank / cube / stash / npc-shop page+grid coords fetched in `InitializeiInventoryPages` (:159–286).

SAME CLASS, needs the same offset — top-anchored raw-constant sites (eyeball to confirm): 800BorderFrame pieces at raw Y `0x3f/0xfd/0x1e4/0x229` (Panel.cpp :1435–1443, :1460–1468); hireling name/stat texts (`0xd6 - gnPanelOffsetY` :5618, label rows :5729–5758, stat rows :5873–5875); trade window texts (`0x1b/0xf2 - gnPanelOffsetY` :9230–9276); stash gold text (:8881–8886 — **confirmed bottom-anchored on eyeball, `gnPanelOffsetY - 0x19f + ScreenSizeY`, no fix needed**); weapon-swap tooltip (`0x15 - gnPanelOffsetY` :8384); recipe scroll text (`0x82 - gnPanelOffsetY` :6727).

## 5. Fix round (site-6) — what landed

1. **Primitive**: `VPRES_GetWideUiOffsetY()` in VanillaPlusCore.{h,cpp} — `ScreenSizeY - 600` clamped ≥ 0: 0 at modes 0/1/2 (byte-identical retail), +100 at mode 3, +120 at mode 4. `gnPanelOffsetY` untouched (-60) — bottom-anchored expressions already adapt; double-shifting is the failure mode. Design per audit doc default: panels keep retail's bottom margin (hug the bar).
2. **Table rects**: `VP_SHIFT_InvPageCoords / GridCoords / BodyCoords` now shift `nTop`/`nBottom` too (covers inventory + hireling + trade + bank + cube + npc-shop + trophy + guild-vault pages). Every Belts.txt consumer in Panel.cpp shifts the Y pair: slot highlights (:789/:2027/:2044), item sprite (:2050), hotkey text (:2076), and the hit test (`nTop/nBottom` in `PANEL_GetPanelLayoutHeight` — clicks followed the draw).
3. **Top-anchored constants**: 800BorderFrame raw-Y pieces (10 draws); hireling name/stat rows; trade window texts; weapon-swap tooltip; cube recipe scroll; party panel art + member rows + all click rects; waypoint act tabs/list/rows + the help-bubble; quest log icons/titles/description + open/close buttons; help screen (Xpac border/bullets/text rows — classic help untouched, mirroring site-5's Xpac-only X); stat-window tooltip hit tests; Engine modal popups (OK/cancel art, text, and their hit-test bases). Scrolling text box panel-mode top. Left alone deliberately: repair icon + help scroll button + hireling/pet onscreen icons (ScreenSize-anchored corner widgets) and the classic-help path.
4. **Input half**: where the code converts screen→panel-relative mouse (`nMouseY += gnPanelOffsetY` idiom — waypoint :462/:684/:750, quest log :1794/:1848/:2072) the shift is subtracted back out: `+= gnPanelOffsetY - VPRES_GetWideUiOffsetY()`. Site-5 already did the X twin (`-= gnPanelOffsetX`) here.
5. **Trap fixed — coord cache key**: `nCoordPageKey` was `nScreenMode + 2·(wide-X)` which collides for mode 3 (272/100) and mode 4 (240/120) — now `nScreenMode + X + 0x400·Y` (raw offsets, injective for all 12 combos), so an in-game 3↔4 switch refetches coords.
6. **Gates**: 17 "site-6:" rows (primitive, per-file shift counts, waypoint/questlog input-inverse counts, 4 new fork-api includes); suite 125/125. `fork-surface.allowlist` must be regenerated by the owner after committing (`tools/check-fork-surface.sh --update`) — the sandbox has no `upstream` ref.

**Regression surface**: every offset is 0 at modes 0/1/2, so 800x600 must stay byte-identical and 640x480 untouched — the §6 checklist applies to 800x600 as well as 720p.

## 6. Field checklist for the fix round (720p, windowed `-w` + fullscreen D3D)

- Belt: potions inside bar slots; click/use per slot; place-with-cursor; hotkeys 1–4 highlight; expansion rows (hover) stack above the in-bar row; belt-full arrows.
- Inventory: drop items in each grid corner; every body/equipment slot; gold drop button + coin draw; hireling panel (name, stat rows, slots); weapon-swap boxes.
- Other panels: stash / trade / cube / bank pages — **stash deposit/withdraw gold modal = the site-6 gold-dialog round: window centered over the panel seam (X) and riding bottom (Y), edit box / OK / Cancel / spinner aligned, clicks hit each one** — plus waypoint (list rows AND act-tab clicks), quest log (icons, title, description, open/close buttons, per border-type), party panel (member rows, loot icons, party-on/off buttons AND their clicks), help screen (art, bullets, text rows, close button), NPC shop tabs/buttons, EscMenu/front-end menus (incl. the sibling dialogs noted in §3 if they misalign).
- Watch-items from the audit: repair warning arrows (deliberately screen-anchored), summit cloud tiling, cutscene mode round-trip, in-game 800↔720 resolution switch (coord-cache refetch).
- Glide (`-3dfx` + the PATCHED Glide3xVK build — apply `glide3xvk-720p-resolutions.patch` on branch `Glide3xVK` and rebuild glide3x.dll): 1280x720 stop boots into the game with the world visible (not black), fps overlay reads "Video: Glide"; cutscenes still play (they resize to 640x480 first — retail behavior); the 800x600 stop is byte-identical retail. With an OLD wrapper DLL the new stop fails soft: 640x480 surface + "unknown resolution 16" in glide3x.log.
- Regression: 800x600 byte-parity (same checklist — every offset must be 0 there) and 640x480 untouched.

## 7. Conventions (condensed)

Overlay workflow (sandbox edits the tree; owner applies as their own commits via llvm-mingw i686 Win11 build — `tools/build-local-win11.cmd`; the full build is the owner's gate). `VP-BEGIN/VP-END` markers on every fork edit in upstream files; fork-only TUs (VanillaPlusCore) keep upstream files free of new code. Every source change updates `tools/check-vanilla-plus.sh` gates + `fork-surface.allowlist` (after committing the site-6 round, run `tools/check-fork-surface.sh --update` — the sandbox has no `upstream` ref fetched). All source files are CRLF; the sandbox patchers (`/home/z/my-project/scripts/site6_patch*.py`) preserve that byte-exactly. Full pre-720p history: HISTORY H1–H24 (H24 = the offloaded handoff revs 14–27 + §1–§13).

## 8. Current task (rev 34): SP loading-screen freeze — ROOT-CAUSED + FIXED

**Task (owner, verbatim)**: 'merge+vanilla-plus port compiles, boots, goes to title screen, char select. freezes at end of loading screen. Expanding d2boot.log to diagnose'

**The rev-34 owner run delivered the answer.** The expanded d2boot.log shows the whole handshake completing — `s2c 0x04 STARTACT received - loading screen can drop now` — followed 12.9 s later by a **hangwatch stall dump** (`no main-loop beat for 12906 ms`). The dumped main-thread stack (d2exe.map symbolization; small offsets reliable):

```
#7  MessageGameLoop
#5/#6  S->C packet pump (Game.cpp HandleAllNonVariableSizePackets -> PacketHandle_to0xAF)
#4  NET_D2GS_CLIENT_PacketHandle_to0xAF +0x30B
#3  NET_D2GS_CLIENT_Incoming0x5B +0xC7        <- player-join/roster packet handler
#2  ROSTER_AllocOrUpdatePlayer +0x1B1
#0/#1 roster list code (GetRosterName map blob) <- eip spinning here
```

Reading: the main thread was NOT blocked/idle — it was stuck INSIDE S→C packet dispatch, processing packet 0x5B (player join → `ROSTER_AllocOrUpdatePlayer`) over and over. Every roster path was audited in-tree and terminates for a single fresh player (AllocOrUpdate/InitializeFields/RebuildPlayerParty/GroupIntoParties/SortAllParties/Flatten/ResetPartyUi all bounded with one roster node); the loop is NOT in the roster code — the dispatch machinery was re-delivering packets forever.

**Root cause (rev 34): cross-thread packet-queue race.** `NET_D2GS_CLIENT_ExtractPacketSize` (D2Net/SRC/D2Net.cpp) pops S→C packets off `pD2PacketClient_to_0xAE`/`pD2PacketClient_from_0xAF` **under `lpCriticalSection_00882ce8`** (main thread, every frame). In SP the server is in-process: `NET_D2GS_SERVER_SendPacketToClient` loops server packets straight back through `NET_D2GS_CLIENT_HandleIncomingBufferAndCreatePacket` (D2Net/SRC/Client.cpp) — **on the server/qserver thread, mutating the same two queues with NO lock**. The enqueue walks the chain to the tail and writes the head back while the pop can free nodes under it; one bad interleave leaves `pD2PacketClientNext` pointing at freed/recycled nodes — a self-sustaining cycle. The pump (`do { ExtractPacketSize; dispatch; } while (iSize != -1);`) then never sees -1, `MessageGameLoop` never returns, the main-loop beat stops (the hangwatch dump), and the loading screen — which drops only when the frame loop progresses past 0x04 — freezes. The race window is widest exactly at the post-0x04 packet burst (0x5B join + pets + quest state), which is why the hang lands at the END of the loading screen. Retail D2 ran the SP server on the main thread (unlocked queue was safe there); the port's threaded qserver turned it into a data race.

**Fixes (rev 34, 4 files)**:
1. `D2Net/SRC/Client.cpp` — `NET_D2GS_CLIENT_HandleIncomingBufferAndCreatePacket` now takes `lpCriticalSection_00882ce8` around the whole enqueue and releases it on every exit path. **The root-cause fix** (matches the lock discipline the pop side always had).
2. `D2Net/Game/Server.cpp` — `NET_D2GS_CLIENT_GetIncomingPacketSizeFromTableAndVariableSize`: the variable-size switch `default:` now returns **0 (unknown size)** instead of `1` with `*pExpectedSize = 0`. Success-with-size-0 made the dispatcher re-dispatch one packet forever (cursor never advances) and the TCP parser enqueue zero-size nodes forever — latent poison for any future variable-length opcode (all 14 current ones have explicit cases, so this is hardening, verified against `NET_D2GS_CLIENT_INCOMING_SIZE`).
3. `D2Client/GAME/SCmd.cpp` — `NET_D2GS_CLIENT_PacketHandle_to0xAF` zero-consumption guard: a dispatched packet that consumes 0 bytes logs `s2c dispatch guard: op 0x.. consumed 0 bytes; dropping N trailing bytes` and stops processing that buffer instead of spinning forever.
4. `D2Client/GAME/Game/Game.cpp` — pump drain watchdog: every 4096 packets drained within one pass logs `s2c pump: drained %u packets (op<0xAF|op>=0xAF) in one pass`, so a runaway drain is now visible in d2boot.log instead of being a silent infinite loop.

Gates: 4 new "d2boot-fix:" rows (suite **151/151**). Delivered as an **overlay zip of changed files** (owner preference; includes the still-uncommitted rev-33 instrumentation files so the zip is self-contained over a clean tree). After committing, regenerate `fork-surface.allowlist` owner-side (`tools/check-fork-surface.sh --update` — the sandbox has no `upstream` ref).

**Expected healthy tail for the next owner run** — after `s2c 0x04 STARTACT received - loading screen can drop now` the world should simply appear; the rev-34 lines only fire on pathology (`s2c dispatch guard: ...` = size-table gap; repeating `s2c pump: drained N packets ...` = queue never drains; plus a hangwatch dump naming the function if anything still wedges).

---

### Rev 33 (diagnostics round, retained)


**Task**: merge+vanilla-plus port compiles, boots, title screen and char select work; single-player freezes at the end of the loading screen, before the world appears. **Constraint (owner directive)**: diagnose from the *current* tree + the owner-provided `d2boot.log` only — no regression archaeology in commit history.

**Evidence so far** (`d2boot.log`, owner run): every stage up to `enter-world: game loop enter` succeeds (MPQs, GFX provider, window, sound, qserver start, connect wait `connected=1`, game data init). Then silence. No hangwatch dump before the owner killed the process — so either the main thread kept beating (a *live-loop* stall, not a wedged wait) or the kill came inside the 12 s threshold. `Client_CheckConnectionTimeouts` early-returns for SP (Game.cpp :2305), so nothing will ever time this load out — the hang is genuinely infinite, not a slow load.

**Mechanism established from current code (rev 33)**: in SP the "server" is cooperative — it is pumped from the *client's* frame function (`CLIENT_GameLoopFrame`, Game.cpp :2419–2431): `NET_D2GS_SERVER_HandleAnyIncomingPacket` → `SrvProcessAllGames(1)` (25 FPS gate) → `QSERVER_DispatchAndCleanup` (flushes S→C packets back into the same process). The loading screen drops only when the client handles S→C **0x04 STARTACT** (`NET_D2GS_CLIENT_Incoming0x04_STARTACT` sets `ServerLoadingComplete=1`, SCmd.cpp :422). The whole handshake:

1. EnterGame sends c2s **0x67** create-game (Game.cpp :2709, Msg.cpp :42).
2. `CCmd` case 'g' → `SetupGameChecks` gate → `GAME_CreateSingleplayerGame`: allocates game + client, queues S→C 0x01 NEWGAME + state-cmd **0** + state-cmd **2**, sets client `ConnectionState=1`.
3. Client 0x02 handler (`NET_D2GS_CLIENT_Incoming0x02_LoadSuccess`) replies c2s **0x6B** join-act.
4. `CCmd` case 'k' → `NET_IsClientAllowedToJoin` (requires `ConnectionState==1`) → `NET_D2GS_SERVER_SrvJoinAct`: `CLIENT_LoadCharacterAndSendGameData` (save load) → `CLIENT_LoadActAndSetState2` (`EnsureDrlgActLoaded` **DRLG worldgen** + `TellClientToLoadAct` queues 0x03 NEWACT + 0x53) → `CLIENT_PlacePlayerInRoom` → `ConnectionState=3`.
5. `UpdateClients` case 3 waits `DRLGROOM_AreAllNearRoomsActive(pClient->pRoom)`, then queues **0x04** → client sets `ServerLoadingComplete=1` → loading screen can drop.

**Instrumentation added (rev 33)** — all `[d2boot]`-tagged, all quiet once 0x04 arrives: client frame telemetry (`game-loop: ...` while loading), one line per distinct S→C opcode during load (`s2c recv 0x..`), explicit 0x02/0x04 handler marks, c2s 0x67/0x6B send marks, bounded server dequeue trace (`srv c2s ...`), SP create enter/exit, `srv state-cmd` choke-point log, `SrvJoinAct` stage marks (enter / char-load refusal / char ok / placed), `join: load-act` + `DRLG: generating act`/`act generated` worldgen marks, `client state N -> M` transition log, state-3 `waiting for rooms` telemetry (first + every 250 server frames), `join: all rooms active -> 0x04` mark, server frame marks (first tick + every 250 up to frame 1000). Files: D2Client `GAME/Game/Game.cpp`, `GAME/SCmd.cpp`, `GAME/Msg.cpp`; D2Game `GAME/Server.cpp`, `GAME/CCmd.cpp`, `GAME/Clients.cpp`, `GAME/SCmd.cpp`, `GAME/Level.cpp`. Plus 16 `d2boot:` gate rows (suite 147/147). Delivered as an **overlay zip** (owner applies over their tree, builds via `tools/build-local-win11.cmd`); after committing, regenerate `fork-surface.allowlist` owner-side (`tools/check-fork-surface.sh --update`).

**Healthy-load narrative to expect** (timestamps in ms; `[JOIN n]` SRVLog lines go to the server log, not here):

```
enter-world: game loop enter
game-loop: first frame after 'game loop enter' (still loading)
c2s send 0x67 create-game (gameType=3 class=.. diff=..)
srv c2s setup pkt (clientid .., op 0x67)
c2s 0x67 GAMELOGON_SP recv (clientid ..)
SP create-game enter (clientid .. class .. diff ..)
srv state-cmd 0x0 queued (clientid ..)
SP create-game done (0x01 + state-cmd 0 + state-cmd 2 queued)
s2c recv 0x01 during load / s2c recv 0x00 during load / s2c recv 0x02 during load
s2c 0x02 LoadSuccess -> replying c2s 0x6B join-act
c2s send 0x6B join-act
srv c2s setup pkt (clientid .., op 0x6b)
c2s 0x6B JOINGAME recv (clientid .., allowed=1)
SrvJoinAct enter (clientid ..)
join: LoadCharacter enter (clientid .., fromSave 1)
SrvJoinAct: character loaded ok
join: load-act enter (act 0)
DRLG: generating act 0 (first load)
DRLG: act generated
join: DRLG act ready
join: TellClientToLoadAct (act 0)
join: 0x03 NEWACT + 0x53 ACTTIMEOFDAY queued, state -> 2
client state 1 -> 2 (clientid ..)
SrvJoinAct: player placed in room, state -> 3 (waiting rooms)
client state 2 -> 3 (clientid ..)
server: first game tick (world loop running)
server: game frame 250/500/750/1000
join: all rooms active -> 0x04 STARTACT queued (loading done server-side)
srv state-cmd 0x4 queued (clientid ..)
s2c recv 0x04 during load
s2c 0x04 STARTACT received - loading screen can drop now
```

**Triage table — read the LAST d2boot line, it names the broken stage**:

| last line seen | broken stage / where to look next |
| --- | --- |
| `enter-world: game loop enter`, no `game-loop: first frame` | `MessageGameLoop` never ran the frame fn — cutscene state stuck (`D2GFX_EndCutSceneReturnSceneState` at CLIENT_GameLoopFrame top) |
| first frame ok, no `c2s send 0x67` | EnterGame's send branch didn't run (gametype routing before the loop) |
| 0x67 sent, no `srv c2s setup pkt` | client→server loopback dead: Fog QServer mode-0 queue / packet-size classifier |
| setup pkt seen, no `SP create-game enter` | `SetupGameChecks` rejected — the `c2s 0x67 REJECTED` line names it |
| create enter, no `create done` | creation hung/crashed inside (subsystem allocs, seed, quest/monster/object controls) + hangwatch dump |
| create done, no `s2c recv 0x02` | S→C dispatch dead: `QSERVER_DispatchAndCleanup`/`FlushClientPackets` loopback |
| 0x02 recv, no `c2s send 0x6B` | client 0x02 handler path (Msg.cpp `NET_D2GS_CLIENT_Send_0x6B`) |
| `0x6B JOINGAME recv (allowed=0)` | `ConnectionState != 1` at 0x6B time — ordering/double-send; also `QSERVER_IsClientAllowedToJoin` |
| `SrvJoinAct enter`, no `character loaded ok` | save load hung, or REFUSED line (0xB4 reason) — `CLIENT_LoadOrCreatePlayerUnit` |
| `load-act enter`, no `DRLG: act generated` | worldgen hang — hangwatch dump names the DRLG function |
| act generated, no `player placed in room` | `CLIENT_PlacePlayerInRoom` hang (corpse/golem restore paths) |
| `STILL waiting for rooms` repeating | `DRLGROOM_AreAllNearRoomsActive` never true — room activation (`InitNewRooms`, act dirty flags) |
| 0x04 queued, no `s2c recv 0x04` | server→client loopback dead (same class as row 6, opposite direction) |
| `s2c 0x04 STARTACT received` but screen frozen | client-side dismissal path — loading draw fn / load timer (`gLoadTimerDurationMs`), Game.cpp :2668–2683 |

If the log ends mid-narrative AND a `hangwatch: === stall detected` block appears, the dump's symbolized stack IS the answer (copy `d2exe.map` beside d2.exe for names). If the loop stays alive with no further marks, the table's "no <mark>" row is the diagnosis. Next round: one owner run with this build, paste the new `d2boot.log` tail.
