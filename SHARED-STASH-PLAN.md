# Shared Stash — feature plan (track N)

*Drafted 2026-11-01 (Rev 81 docs round) on owner ask, after the R62 full-SMOKE pass.
Owner-set constraints recorded verbatim:*
1. *"the stash should have it's own save"* — the shared pool lives in its **own save file**, never inside the character `.d2s`.
2. *"vanilla+ character saves need to be compatible with retail"* — **hard invariant**: a Vanilla+ character save loads in retail D2 and a retail save loads in Vanilla+, byte-for-byte, in both directions, with this feature shipped or not.

Status: **SHIPPED — field-proven end to end; first in release 0.0.7 (cut at R66), ten pages since R82.** The mechanism was field-proven blind on the R65 build (page swap + cross-char item/gold round trip), the visible tab on the R66 build (owner: "Great job. I see it. It works."), the tab seat hardened through R75–R77 (outline + pager, the R76/R77 nudges), capacity widened to **ten** shared pages at R82 (`.sss` v2 `pageCount` 10, header 92 bytes; pre-R82 files load with the missing pages empty) — SMOKE §H items 25/27/33–35 field-proven on the R83/R104 full belts (issues #24/#28). The design below is the pre-implementation draft: its hotkey idea was superseded by the in-panel tab, and the rule-5 contract note was owner-signed off by shipping (`.d2s` stays retail-pure). The round-by-round chronicle this paragraph carried (R63 → R82) moved to [HISTORY.md](HISTORY.md) H44 (trim #20).

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
