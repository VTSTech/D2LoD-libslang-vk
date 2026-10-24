# FIELD-TEST.md — P1 field test on a vanilla retail D2LoD 1.14d install

You do **not** need a rebuilt `d2.exe` for this. Retail 1.14d already contains the
seam we plug into: its Glide renderer does `LoadLibraryA("glide3x.dll")` and
resolves 45 stdcall exports — the Redivivus recovered source is exactly that code,
and nGlide / d2dx have used the same drop-in deployment model on stock installs
for years. The rebuilt d2.exe behaves identically if you have it, but it is not
required until P4 (optional VEH crash-report hook).

## What this DLL is right now (P1 scaffold)

- All 45 Glide exports, correct stdcall decorations, state-tracking stubs.
- `grGet(GR_SECONDARY_TMU, ...)` answers correctly so the game **accepts** the renderer.
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
