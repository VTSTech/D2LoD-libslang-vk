# Shared Stash — feature plan (track N)

*Drafted 2026-11-01 (Rev 81 docs round) on owner ask, after the R62 full-SMOKE pass.
Owner-set constraints recorded verbatim:*
1. *"the stash should have it's own save"* — the shared pool lives in its **own save file**, never inside the character `.d2s`.
2. *"vanilla+ character saves need to be compatible with retail"* — **hard invariant**: a Vanilla+ character save loads in retail D2 and a retail save loads in Vanilla+, byte-for-byte, in both directions, with this feature shipped or not.

Status: **ROUND 1 FIELD-PROVEN — code round R63 (Rev 82), build-fix R64 (Rev 83: mingw32 `uint32_t*` → `LPDWORD` in the `.sss` read out-param; no format or behavior change), tab-anchor fixes R65 (Rev 84: right edge `0x1f8` → `0x180`) and R66 (Rev 85: `0x180` → `0x130` — the word was drawn then overpainted by the right-panel art, whose rectangle starts at panel-relative `+0x140` (`ScreenSizeX - gnPanelOffsetX - 0x140`) and is blitted ~30 lines after the tab site; anchor now 16 px left of that rect), field status: **FIELD-PROVEN end to end** — the MECHANISM on the R65 build (owner toggled the page blind, deposited gold + an item, switched characters, both came back) and the VISIBLE TAB on the R66 build (owner: "Great job. I see it. It works." — SMOKE §H item 21 confirmed). First shipped in release **0.0.7** (self-ID `Vanilla+0.0.7-R66`; round journal in [HISTORY.md](HISTORY.md)). Still owed: SMOKE §H item 25 (the `.d2s` loads in unmodified retail — the I1 proof) and item 27 (the checksum degrade drill) on a 0.0.7 build; the 720p tab check confirmed on the R74 build (owner, 2026-11-02: "Went in game at 720p, Stash tab is visible and working.").** The owner's go message ("Give us 1 page of retail-shaped shared stash to start. Accessible from Stash Panel. No hotkey. Gold and Items can be deposited. If that works we'll do 10 pages. Then 100.") resolved all four §9 calls and merged the R63+R64 plan rounds into one increment: **1 page, retail grid; stash-panel tab, NO hotkey (the optional hotkey of §4 is vetoed); shared gold IN v1 (§1 non-goal superseded); file `_d2vp_shared.sss`.** The shipped architecture improves on §4 in one place worth recording: instead of a fork-owned runtime container behind the packet pipe, the shared page **materializes into the retail page-4 stash inventory** (personal set waits in a fork-owned `'JM'` snapshot), so every retail stash flow and the retail gold buttons work verbatim — zero new packets, zero server container code; the plan's §6 degrade matrix, `.bak` + atomic write and checksum are all implemented as designed. Multi-page (10/100) is a version-2 header extension as §5 planned. The hardening round (failure drills + the I1 retail round-trip proof, plan §7 R65) is the next field round. R67 (post-0.0.7): the gate is no longer ini-only — the Vanilla+ Options page gained a **Shared Stash** row (the sixth row, same D2VP_* bridge as the other five; D2VP_ConfigReloaded busts the client gate cache so the tab follows the toggle same-frame; the server half rides its own 2 s poll per the module boundary). R68 (field-polish, owner: "Can we make the 'Shared' text a bit bigger"): the tab label draws in **Font24** (was the panel's Font16) at the same right-anchored spot — `d2vp.ini [ui] stash_tab_font` hot-tunes it (1 = Font16, 2 = Font30, 3 = Font42, on the same 2 s gate tick), and the click band hugs the word as measured each draw instead of the hard R66 band (the Font16 case reproduces that band exactly); the right anchor `0x130` and the R66 art-seam story are untouched. R69 (field-polish, owner: "Can we move the Stash label to where my cursor is? Up and to the left. Above the gold amount deposited display" — the R68 bump had the word overlapping the retail "Gold Max: %ld" text drawn on the same row at `+0x4e`): the tab now **floats above the stash panel** — right edge `+0xdc` (abs 300 at 800x600, centered over the gold amount box) and the baseline 55 px above the gold amount box's own text row (`DRAW_UI_INVENTORY_Gold`'s stash row, `-0x1b8` expansion / `-0xf4` classic) — ending the shared row (and the overlap) entirely; the word rides the panel by the rigid-unit rule at every 800-class mode, while 640x480 keeps the R66 in-panel row (no headroom for the floating spot). Because the band now sits on the playfield above the panel, the mouse-DOWN in it is swallowed in `INPUT_StashLeftClick` (new `VPSHAREDUI_TabHitTest`, gate + band, no side effects) so the down can't walk the character out from under the stash — the mouse-UP toggle (INV_HandleStashLeftClick's VP site) is unchanged. R70 (field-fix, owner verdict on the R69 build: "The Stash label is in the right place, clicking around switches the tab. But it is invisible again."): position + click confirmed, the word itself washed out — the float rides the LIVE WORLD strip above the panel (the late frame queue was swept end to end: nothing paints there after the tab site — this is contrast, not R85's draw-order overpaint), and un-outlined white glyphs vanish on bright terrain (the R69 handoff's flagged tradeoff). Fix = the retail text-backdrop recipe: `VPSHAREDUI_DrawTab` draws a black `DRAW_VERYTRANSPARENT` rect under the word on float rows — the same pattern retail's own `UI_DrawPartyRoster` uses under its roster text — so the glyphs read on any terrain; the 640x480 legacy row keeps its field-proven bare look; anchor, hit band and down-swallow are unchanged. R71 (compile-fix): the first R70 overlay did not compile on the Win11 box — the pill call used the short `Engine::Gfx::` form, which only resolves from TUs inside `namespace D2Client` (`VanillaPlusOptions.cpp` is one, `VanillaPlusCore.cpp` is a global-scope TU, a rule the file's own `GetPlayerUnit_UI` note documents); R71 fully qualifies the call, the suite pins the qualified form + a must-NOT-match tripwire on the bare form, and `tools/syntax-check.sh` now compiles all four fork TUs on the sandbox host with the build's flags (298 gates + 4/4 TUs clean) — pill design/geometry unchanged from R70. **R67-R71 ROLLED BACK (owner commit `59ff645`, 2026-11-01)**: the Options-row hook survived, but the tab polish (Font24, the floating row, the pill) was reverted to the R66 in-panel tab — the field-proven state the owner then took through the 1792 merge ("Went in game at 720p, Stash tab is visible and working", 2026-11-02). R75 (code round, Rev 97, owner: "Can we give the current stash tab label an outline? Also can we add a 2nd page and buttons/arrows to navigate pages") builds on that R66 state: **(1) the outline** — the label draws with a 1-px black halo on all four sides (the D2Comp shadow recipe extended to a full outline, palette 6 under the color pass, word + arrows alike); **(2) the second page + pager** — two shared pages stored as two 'JM' blocks, the tab composite reads `Shared >` / `< Shared 1 >` / `< Shared 2`, the arrows step ONE page (personal → 1 → 2 → back) and the word toggles personal↔shared-1, the 0x4F/0x7D param word carries the ABSOLUTE desired page (0/1/2 — retail ClickButton rejects it all the same when the server isn't ours), page↔page switches swap the materialized set with NO goldbank re-borrow (one gold pool across both pages) and NO file write (the next restore-to-personal or save flush writes both pages); **(3) the file** — `.sss` **v2**: magic `VPS2`, per-page size/checksum pairs, both blocks after the 28-byte header; **v1 field files read back as page 1** (page 2 starts empty; the next save rewrites v2 — no converter, no data touch). The 10/100-page extension is now literally a header widening + pager math. Gates 285 → **297**, 4/4 fork TUs host-compile clean, self-ID **`Vanilla+0.0.7-R75`**; field ask = SMOKE §H items 33-34. R76 (field nudge, owner: "the page arrows are displaying on top of the gold amount slightly. can we move the stash label to the right a bit"): the composite is right-anchored and grew LEFTWARD from the R66 spot — the `<` tip landed on the tail of the retail gold amount text drawn on this same row at `+0x4e` (`Inv.cpp`'s stash-gold site); the anchor moved `+0x130` → `+0x13c` (12 px right, the `>` halo tip at `+0x13d`, 3 px inside the `+0x140` art-seam rule), draw and click bands both follow the shared layout; self-ID **`Vanilla+0.0.7-R76`**. R77 (field lift, owner: "Looks good horizontally, but still drawing on top of the last gold character a bit with the arrow. can we move the stash label up, closer to the top right corner ?"): horizontal was approved but the `<` halo still kissed the gold text's last glyph on the shared row — the ROW moved up 0x2d (45 px), `VPSHAREDUI_TabBaselineY` `-0x19f`/`-0xdb` → `-0x1cc`/`-0x108`, the empty band between the panel's top edge and the retail gold display row (`DRAW_UI_INVENTORY_Gold`'s stash row `-0x1b8`/`-0xf4`); the R76 right edge `+0x13c` untouched; the band's right side is empty retail space (the coin rect and the gold-area button rect both live at X `+0x49..+0x96`), the halo top stays inside the field-bounded panel edge, and the click band never probes the playfield; self-ID **`Vanilla+0.0.7-R77`**, gates 297 → **299**. R82 (capacity round, owner: "increase the shared stash pages to 10" — the "If that works we'll do 10 pages. Then 100." roadmap kept at ten): the multi-page extension §5 planned ships — **ten shared pages**, same `.sss` v2 magic + version, `pageCount` 10 (header 28 → 92 bytes), the reader accepts any pageCount the build can hold so pre-R82 two-page and pre-R75 one-page files read back with the missing pages empty (the next save rewrites ten), the pager labels read `Shared 1`…`Shared 10` from a literal table, arrows still step ONE page, one shared gold pool across all ten; I1 untouched (the flush parks all ten pages before any `.d2s` is built); gates 299 → **306**, self-ID **`Vanilla+0.0.7-R82`** (Rev 105); field ask = SMOKE §H item 35. The "Then 100" leg remains a pure widening of the same two constants + the pager bounds.

---

## 1. Goal and non-goals (v1)

**Goal.** With `[features] shared_stash=1`, all characters on this PC share one stash pool: any character can deposit items into it and any other character can withdraw them. The pool is stored in its own file next to the `.d2s` saves and survives forever, independent of any single character.

**Non-goals for v1** (each is a clean follow-up, deliberately deferred):
- No multi-page shared stash (single shared page ships first; more pages are a pure data-shape extension, §5).
- No shared gold — the stash gold box stays per-character (retail `goldbank`).
- No multiplayer (TCP/IP open, Battle.net) — v1 is single-player only, hard-inert elsewhere (§4).
- No PlugY `.sss` format compatibility — our own format (§5); a PlugY importer would be a separate, optional round.
- No changes to item bytes, item properties, or grid math beyond what retail already encodes.

## 2. The two hard invariants (pillars)

- **I1 — retail `.d2s` compat.** The character save engine in `D2Game/PLAYER/PlayerSave2.cpp` gains **zero** new blocks, zero changed blocks, zero fork-only item fields. Items deposited from the shared pool land in the `.d2s` exactly as retail-encoded stash items; items withdrawn were retail-encoded to begin with (they only ever lived in our wrapper file). Verification is behavioral, not just by inspection: the round-trip SMOKE item in §7 (save a char in Vanilla+ → load it in retail; and a retail-made save loads in Vanilla+). This invariant already holds repo-wide today (the R60 audit proved the `.d2s` engine byte-symmetric across all ten blocks) — I1 is about *keeping* it.
- **I2 — vanilla parity when off.** `shared_stash=0` (the default, and every existing `d2vp.ini`): no file IO, no extra memory, no packet changes, no UI pixels — byte-identical behavior to the current R62 build. The feature never writes the shared file unless explicitly enabled and explicitly used.

Carried-over house pillars: **I3 degrade, never halt** (a corrupt shared file never kills the game — the R61 esc-menu lesson); **I4 fork code lives in fork TUs** (`VanillaPlus*`), upstream files only get thin `VP`-marked call sites; **I5 every code round = gates rows + self-ID bump + d2boot marks + SMOKE items**.

## 3. Where things live today (recon, this tree)

| Leg | Home | Facts |
| --- | --- | --- |
| Stash UI (client) | `D2Client/UI/Inv.cpp` | grid pick/place + stash-update packet: `INV_HandleStashLeftClick` (:4969); art load `invstash.dc6` / `invstashex.dc6` (:5156); stash opens through the NPC-interact flow (`gnStashNpcGuid`, `0x4f/0x17` notify, `INTERACT_AnyObject(2, guid)` :5230-5239); input handler :11217 |
| Authoritative container (server) | `D2Game` | the stash grid lives in the player unit's inventory server-side; moves arrive as packets from the client |
| Item save/load engine | `D2Game/PLAYER/PlayerSave2.cpp` | `SaveItems` (:624) serializes item lists into a save buffer; `PLRSAVE_LoadItemsFromBuffer` (:1350) reads them back; byte-symmetric (R60 audit of record) |
| Stash gold | `PlayerSave2.cpp:1807-1814` | `UNITSTAT_goldbank`, clamped on load by `D2Common::Units::GetStashGoldLimit` — stays per-character, untouched by this feature |
| Feature plumbing | `D2Client/UI/VanillaPlusCore.cpp` | `GetPrivateProfileIntA("features", …)` with ~2 s hot-reload (:548); self-provisioning `d2vp.ini` template (:205) |
| Save-dir resolution | `Fog/Source/SFile.cpp` | the portable_save path poll — the shared file anchors to the same resolved save directory, so `portable_save=1` makes the shared pool travel with the game folder automatically |

## 4. Design (recommended shape)

**Storage — the pool's own file.** `<save dir>/_d2vp_shared.sss`, own format (§5): our header + retail-encoded item records + trailing checksum. Written atomically (temp file + rename) with a one-generation `.bak` kept behind it. Never read or written when the feature is off.

**Lifecycle.** Lazy load on the first stash open of a session (zero cost for players who never open it; no new load-path risk at game join); written at exactly the `.d2s` save points (Save & Exit, game teardown) — one consistency point per session, same as retail saves. SP has exactly one process owning the file, so no locking is needed beyond the atomic rename.

**The pipe.** When the shared page is active, the existing stash-grid click path (`INV_HandleStashLeftClick`) routes pick/place against the shared container instead of the personal grid, using the same packet shape with the container identity riding the VP call site — the server side keeps the shared container in a fork-owned runtime structure (a `VanillaPlusStash` TU), never inside a decomp static table (the R61 lesson).

**UI.** One **Vanilla+ page toggle** in the stash panel: a small VP-drawn button/tab (runtime-synthesized DC6 — the same technique as the R61 Resolution row's "1280x720" stop, proven to survive re-init) labeled **"Shared"**; clicking it swaps the visible grid between Personal and Shared; the gold box stays visible and personal on both pages. Fallback hotkey (`d2vp.ini` `shared_stash_hotkey`, default off) for folks who hate the button.

**Grid.** Same 6x8 = 48 slots as the retail stash page. Zero new layout math — the existing grid hit-mapping, drag logic and draw paths are reused against the shared container's grid.

**Scope guard.** The feature engages only when: `shared_stash=1` AND single-player. In TCP/IP open games it is inert (button hidden, key ignored); Battle.net never sees it (the client-side flag is off unless SP).

## 5. File format sketch — `_d2vp_shared.sss`

```
offset  size  field
0       4     magic  'V','P','S','S'
4       2     version (u16, currently 1 — bump on any shape change)
6       2     flags (u16, bit0 = page present; reserved)
8       2     itemCount (u16, cap 48 in v1)
10      2     reserved (0)
12      4     checksum (u32, the .d2s additive checksum over everything below)
16      ...   item records — each item serialized EXACTLY as the retail .d2s
              item block encodes it (the SaveItems per-item record: retail
              status/location bits with stash-equivalent placement, x/y grid
              coords, item data) — identical bytes, zero fork-only fields
after   4     record count echo (u32) — belt-and-suspenders tail guard
```

Rationale: because the per-item bytes are the retail encoder's output, an item can move file→`.d2s`→file forever without a conversion step, and I1 holds by construction. The wrapper (magic/version/checksum/counts) is 16 bytes of fork-owned bookkeeping that never touches the `.d2s`. Future pages: `pageCount` in flags + per-page counts — v1 ships single-page and the header already leaves room.

## 6. Failure and recovery matrix (R61 pattern — degrade to mark, never halt)

| Condition | Behavior | d2boot mark |
| --- | --- | --- |
| File missing, feature on | start empty; create on first save | `shared stash: no file - starting empty` |
| Bad magic / version | ignore file contents, start empty, file left on disk untouched for inspection | `shared stash: bad header (magic/version) - starting empty` |
| Checksum fail | retry with `.bak`; if `.bak` also fails, start empty | `shared stash: checksum fail - <file|bak> - starting empty` |
| itemCount out of range | clamp to grid cap, load what parses | `shared stash: item count %d out of range - clamped` |
| Write fail (disk/AV/lock) | temp abandoned, old file + `.bak` untouched, session keeps items in memory | `shared stash: write failed (%d) - keeping memory copy` |
| Item won't parse | skip item, keep loading the rest, count the skips | `shared stash: item %d unparsable - skipped` |

Every mark is gate-pinned; none halts. A degraded load always leaves the on-disk file untouched so nothing is destroyed by a misread.

## 7. Roadmap — three code rounds, each field-verifiable

**R63 — the store (engine + file, no UI yet).**
New fork TU pair (`VanillaPlusStash.{h,cpp}` — placement call during recon; exact anchor points pinned then): container struct, file load/save per §5-§6 (atomic write, `.bak`, checksum, all six degrade marks), `[features] shared_stash` ini key + template line + ~2 s hot-read, lazy load armed at stash-open hook (mark fires when it loads: `shared stash: loaded %d items`). With the UI not built yet, the field-verify ask is log-only: boot with the key on, see the load mark, save & exit, see the save mark, file appears next to the `.d2s`. **Gates**: ~8 rows (ini key, load/save marks, degrade marks, atomic write marker). **Self-ID → R63.**

**R64 — the pipe + the page toggle (the visible feature).**
Server-side shared container wired to the stash click path; VP "Shared" tab drawn in the stash panel; pick/place routed by active page; SMOKE **§H** written (deposit from char A → save → exit → char B → withdraw the same item). **Gates**: tab draw marker, routing marks, §H chain marks. **Field-verify ask (one run)**: the A-deposits/B-withdraws round trip; personal page byte-unchanged behavior; key off → no tab.

**R65 — hardening + the retail-compat proof.**
The §6 failure drills scripted into SMOKE (scribble a byte → checksum → `.bak` recovers; delete the file → empty start; kill the write → memory copy survives), plus the **I1 round-trip**: a Vanilla+ character save loads in **retail** D2 (owner has retail; one boot, one load) and a retail-made save loads in Vanilla+. Full SMOKE pass. On the owner's pass, track N closes the same way M did.

## 8. Risks — mapped to the class lessons already on the books

| Risk | Lesson it rhymes with | Mitigation designed in |
| --- | --- | --- |
| Fork hook dropped by a future upstream merge | Rev 76 (include drop) + Rev 77 (overload split) | every hook = thin VP-marked call site + gate row + the d2boot visibility audit in the merge ritual; paired signature tripwires if any hooked signature changes |
| Corrupt shared file bricks trust in the pool | R61 (halt on data guards) | §6: degrade to mark, `.bak`, file never overwritten on a misread |
| `.d2s` drift (accidental save change) | R60 save audit | I1 is checked behaviorally every code round (§7 R65 round-trip), not just by review |
| UI state mutating static tables | R61 root cause | the shared container and tab state are fork-owned runtime structures; zero decomp static tables touched |
| Multiplayer inconsistency | — | v1 is SP-only, hard-inert elsewhere; MP is a separate design round if ever asked |

## 9. The four owner calls

1. **Capacity/page** — recommend: one shared page, same 6x8 grid as retail (zero layout risk). Alternative: 8x10 oversized grid from day one (more room, new draw math). **[OWNER CALL]**
2. **UI affordance** — recommend: VP "Shared" tab button in the stash panel (synthesized DC6) + optional hotkey. Alternative: hotkey-only, no drawn button. **[OWNER CALL]**
3. **Gold** — recommend: personal only in v1 (retail goldbank untouched). Alternative: a shared gold pool in the same file (adds a second consistency domain). **[OWNER CALL]**
4. **File name** — recommend: `<save dir>/_d2vp_shared.sss`. Alternative names fine; only constraint: never collide with a retail or PlugY file the owner may already have. **[OWNER CALL]**

Decided already (owner, 2026-11-01): own save file (§1/§5); `.d2s` stays retail-compatible (§2 I1); staged R63→R65 (§7); SP-only v1 (§4) — veto anything, else R63 starts on your go.
