# FIELD-TEST.md — P1 field test on a vanilla retail D2LoD 1.14d install

You do **not** need a rebuilt `d2.exe` for this. Retail 1.14d already contains the
seam we plug into: its Glide renderer does `LoadLibraryA("glide3x.dll")` and
resolves 45 stdcall exports — the Redivivus recovered source is exactly that code,
and nGlide / d2dx have used the same drop-in deployment model on stock installs
for years. The rebuilt d2.exe behaves identically if you have it, but it is not
required until P4 (optional VEH crash-report hook).

## What this DLL is right now (P1 scaffold)

- All 45 Glide exports, correct stdcall decorations, state-tracking stubs.
- `grGet(0xF)` (`GR_NUM_BOARDS`) answers 1 so the game **accepts** the renderer.
- `grSstWinOpen` → Vulkan: volk → instance → Win32 surface → device → swapchain.
- `grBufferSwap` → clear **black** + present. Nothing is rasterized yet (that's P2),
  no shader presets yet (that's P3).

## Requirements

- Any retail Diablo II: LoD **1.14d** install (GOG / Blizzard downloader). The game
  is 32-bit; it runs fine on 64-bit Windows 10/11.
- A Vulkan-capable GPU with a current driver. Drivers install the 32-bit loader
  by default — you can verify `C:\Windows\SysWOW64\vulkan-1.dll` exists.
- `glide3x.dll` from the field kit.

## Install

1. If the kit zip shows "blocked": right-click the zip → Properties → **Unblock**
   → OK, *then* extract (avoids any SmartScreen/AV friction with an unsigned DLL).
2. Copy `glide3x.dll` into the game root, next to `Game.exe`
   (e.g. `C:\Games\Diablo II\`).

That's the whole install. Nothing else in the game folder is touched.

## Run

```
Game.exe -3dfx
```

- **Fullscreen only** — 1.14d excludes the Glide renderer from windowed mode
  (do not pass `-w`).
- Alternative selection path: run `D2VidTst.exe` from the game folder and pick the
  **3dfx Glide** renderer — it writes the same video-mode setting.
- To revert: delete `glide3x.dll` (or just launch without `-3dfx`). The install is
  otherwise untouched.

## What success looks like (P1 exit criteria)

1. The game launches fullscreen to a **black screen — that is correct**. Nothing is
   rasterized yet; P1 only proves the DLL loads, Vulkan initializes, and present runs.
2. The menu is alive behind the black: music/menu sounds play, ESC and clicks
   register. The cursor may be invisible or glitchy — expected at this stage.
3. A `glide3x.log` appears next to `Game.exe`. A healthy run looks like:

   ```
   [g3xvk] glide3xvk scaffold <build-id> — loaded into C:\Games\Diablo II\Game.exe
   [g3xvk] vk: loader ok, api 1.x
   [g3xvk] vk: picked AMD Radeon RX 560 ...
   [g3xvk] vk: swapchain 640x480 fmt=... images=...
   [g3xvk] grSstWinOpen: hwnd=... res=7 (640x480) ... colbuf=2 aux=0
   [g3xvk] grGet:0xF(#1): TMU count -> 1
   [g3xvk] frame 300: swaps=300 draws=... texdl=... paldl=...
   ```

   `grGet:0xF -> 1` **before** `grSstWinOpen` is the game accepting the renderer;
   nonzero `texdl`/`paldl` counts mean P2 will have real work queued.
4. Idle in the menu ~5 minutes: no crash, log stays clean.

## If something's off

| Symptom | Meaning / fix |
|---|---|
| No `glide3x.log` at all | The DLL never loaded: wrong folder, AV quarantine, or the zip was blocked — Unblock, re-extract, confirm the DLL sits beside `Game.exe`. |
| Log says Vulkan unavailable / fail-soft | 32-bit loader missing: reinstall the full GPU driver package (includes 32-bit), recheck `SysWOW64\vulkan-1.dll`. The game keeps running; the screen just stays black. |
| Game errors about Glide / falls back to DDraw | Mode didn't take: try the `D2VidTst.exe` route, and confirm you're fullscreen (no `-w`). |
| Game window opens but immediately exits | Send back `glide3x.log` — the last lines name the failing export or Vulkan step. |

Battle.net is out of scope for this project — test offline / single-player only.

## What to report back

- The whole `glide3x.log` file.
- One line: black screen? menu sounds? cursor visible? any crash?
- GPU + driver version (e.g. "RX 560, Adrenalin 26.x").

---

# Round 2 — P1.1 (caps fix)

## Round 1 result (2026-10-24, RX 560, vanilla retail 1.14d)

The v1 DLL **loaded, initialized Vulkan end-to-end and shut down cleanly** — full
P1 plumbing validated on real hardware. But zero frames were presented: after
`grSstWinOpen`, D2Glide runs a caps block (`grGet` pnames 0xA/0xB/0x11/0x13/0x24/0xE)
and **hard-fails its init when `GR_NUM_TMU` (0x13) comes back 0**
(`if (!gnGlideTextureMemoryUsed) return 0;` in `D2Glide/Renderer/Glide.cpp`).
The v1 stub answered 0 → the game dropped us by its own rules. Decoded against
the real 3dfx glide3x source (diget.c, Voodoo2 build): the decompiler names were
misleading — 0xF is `GR_NUM_BOARDS` (the accept gate), 0x13 is `GR_NUM_TMU` (the
init gate), 0x24 is `GR_TEXTURE_ALIGN`, 0xE is `GR_MEMORY_UMA`.

## What changed in the DLL (v2 / P1.1)

1. **Voodoo-class caps table** — answers mirror 3dfx's own Voodoo2 glide3x:
   BOARDS=1, TMU=1, FB=1, MAX_TEXTURE_SIZE=256, TEX_ASPECT=3, UMA=0,
   TEXTURE_ALIGN=8, GAMMA=8-bit/32 entries. Single-TMU profile = the smallest,
   best-tested D2 code paths (no secondary-TMU combine layouts).
2. **Fallback detector** — at detach, the DLL logs if `D2Direct3D.dll` /
   `D2DDraw.dll` / `D2GDI.dll` are loaded (i.e. the game moved on to another
   renderer). `D2Glide.dll` is our caller, always present, not diagnostic.
3. Unknown `grGet` pnames now return 0 like real glide3x (v1 returned a
   success-looking value).

## Round 2 success criteria

1. Log shows the one-line **`caps: Voodoo-class profile — BOARDS=1 TMU=1 ...`**
   after the six caps queries.
2. After the caps block, expect a **burst of state calls** (`grVertexLayout`,
   `grTexCombine`, `grCoordinateSpace`, `grTexFilterMode`, …) — that is D2Glide
   finishing init, which it never reached in round 1.
3. The menu phase then shows up as periodic **`frame N: swaps=...`** summary
   lines. Menu visible + swaps counting = P1 fully exited, P2 gets its oracle
   data. The screen may still be black-ish (clear-only present) — sounds/UI
   clicks prove the game is alive.
4. If the menu appears but the log stays silent until you quit, the detach line
   will now **name the fallback renderer** that took over — that alone is a
   useful report.

Same install/run steps as above; just replace `glide3x.dll` with the v2 build
(build id in the banner is `77cb059-dirty` or later — check the first log line).

---

# Round 3 — P1.2 (Bink video / LFB present)

## Round 2 result (2026-10-24, RX 560, `Game -w -3dfx`)

The caps fix worked — the game got further than ever: state-call burst,
7 × `grTexDownloadMipMap` (P_8 palettized, exactly D2's art format), palette
uploads, one frame presented, and then the **Bink video path**: at 640x480 the
game called `grLfbLock(WRITE_ONLY, FRONTBUFFER, 8888)` in a ~25 fps rhythm.
Source-confirmed (D2Glide/glBlocks.cpp `GLBLOCKS_RenderBinkVideoFrame`): D2
plays cutscenes by `BinkCopyToBuffer`-ing frames **directly into the locked
front buffer** and never calls `grBufferSwap` — on a real Voodoo the front
write *is* the present. v2 dropped those writes on unlock → videos invisible.

Bonus finds from the round-2 log: the 800x600-swapchain/640x480-backend line
pair is windowed-mode reality (the Vulkan swapchain must match the 800x600
window client area while the game renders 640x480 video into it) — v3 handles
it explicitly by scaling in present. And the doubled `palette(#1)` log lines
were a logging bug (tag truncation), fixed.

## What changed in the DLL (v3 / P1.2)

1. **LFB front-buffer present** — `grLfbLock(WRITE_ONLY, FRONT)` hands the game
   a real shadow buffer; `grLfbUnlock` scales/converts it (8888 straight copy,
   565/555/1555 expand, row flip for LOWER_LEFT origin) and **presents**.
   This is the Bink cutscene path, live.
2. **Windowed scale** — present scales the game's frame (e.g. 640x480) to the
   window client area (e.g. 800x600); new log line names both sizes.
3. Logging fixes: palette counter, `grVertexLayout` params now decoded
   (`0x01=XY 0x30=PARGB 0x50=Q0 0x40=ST0` — the 20-byte D2 vertex, P2 contract).

## Round 3 success criteria

1. **You should SEE the intro/logo videos playing** (black letterbox around
   them is normal). Sound was always there; now picture joins it.
2. Log shows the new pair during video:
   `grLfbLock ... -> 640x480 shadow stride=2560 (unlock=present)` /
   `grLfbUnlock ... -> front-buffer frame presented (P1.2)`.
3. If you ran `-w`: a `vk: windowed scale — window 800x600, game renders
   640x480` line right after `backend ready`.
4. After the videos, the menu stays black — expected (menu needs P2
   rasterization: `grDrawVertexArrayContiguous` is still a counted no-op).
   Menu music + clicks = game alive behind it.
5. Same quit/restart safety as before; activity summary at detach should now
   show `grLfbLock`/`grLfbUnlock` counts.

Windowed note: v2 round-2 proved `Game -w -3dfx` loads the Glide renderer fine
on this install (the doc's earlier fullscreen-only warning was overly
conservative) — and `-w` is what exercises the scale path, so keep using it.

Build id: `77cb059-dirty`, banner line says **P1.2 = Bink LFB present +
windowed scale** — check the first two log lines to confirm you're on v3.
