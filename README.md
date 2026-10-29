<img width="1920" height="1080" alt="image" src="https://github.com/user-attachments/assets/8da961b2-fba3-4913-b2e4-bb2510c3a5e3" />
<img width="1920" height="1080" alt="image" src="https://github.com/user-attachments/assets/ef24cacf-3573-416b-a982-fc54520bab47" />

https://www.youtube.com/watch?v=fgnrBxsOLWU

# D2LoD-libslang-vk

[![Discord](https://img.shields.io/badge/Discord-join%20the%20chat-5865F2?logo=discord&logoColor=white)](https://discord.gg/XcJ8mSCNZ)

A drop-in **`glide3x.dll`** for **Diablo II: Lord of Destruction 1.14d** that renders the
game through **Vulkan** — with libretro slang shader presets (`.slangp`) routed through
**libslang-vk** as the goal of this branch.

When the game is started with `-3dfx`, its Glide loader
(`D2Client/Renderer/Glide.cpp`) does `LoadLibraryA("glide3x.dll")` and resolves
**45 stdcall exports**. This branch builds a replacement for exactly that DLL: it
exports those 45 entry points, implements the Glide 3 state machine and raster
semantics D2 actually uses, and presents every frame through a Vulkan swapchain.
It runs on a **vanilla retail 1.14d install** — copy the DLL next to `Game.exe`,
no rebuilt executable, no game-source changes. The payoff pattern — filtering the
presented frame through a user-chosen preset (CRT-Royale and friends) at up-scaled
resolution — is the same one proven in
[DevilutionX-libslang-vk](https://github.com/VTSTech/DevilutionX-libslang-vk)
(Diablo/Hellfire) and
[chocolate-doom-vk](https://github.com/VTSTech/chocolate-doom-vk) (Doom).

## Status — 0.4.6

- The full game renders correctly through the Vulkan swapchain: menu, world, units,
  UI, shadows, text, cutscenes (field-verified rounds 4–12 on real hardware; see
  [glide3xvk/FIELD-TEST.md](glide3xvk/FIELD-TEST.md)).
- **The libslang-vk preset chain is live (0.4.0)**: point `preset=` in
  `d2slangvk.conf` at any libretro `.slangp` and every presented frame runs
  through it at native game resolution — hot-reload on file change, fit /
  integer / stretch scaling, unfiltered fallback on any failure.
- **The version stamp is the overlay box (0.4.5)**: `glide3xvk <ver> (<hash>)`
  drawn top-right every present — 8x8 bitmap font, post-gamma, never in the
  game's framebuffer. The 0.4.1-0.4.3 drivertag patch (repointing the game's
  own `Video:` line) is removed: field builds showed the stock `Video: Glide`
  line either way, and the game's /fps output stays untouched now.
  `overlay=0` in glide3xvk.ini hides the box.
- **Scroll Lock toggles the overlay (0.4.6)**: press **Scroll Lock** in-game
  to hide/show every overlay box at once — the version tag, the shader /
  hostver / FPS lines, the toast and the Shift+V conf panel — for clean
  screenshots and recordings without restarting or editing the ini. The
  toggle is session-live (it survives D2's constant context re-opens — the
  640x480 probes, menu, game — which would otherwise re-push the ini value
  and pop the boxes back mid-session), while `overlay=` in glide3xvk.ini
  still decides the state at startup and after a relaunch. Works with or
  without a preset chain; while hidden the conf-panel hotkeys park too and
  the arrow keys stay the game's; every press is logged
  (`overlay: on/off (Scroll Lock)`).
- **PrintScreen saves a screenshot (0.4.6)**: press **PrintScreen** in-game and
  the current frame lands in `screenshots\` beside the game exe as a PNG
  (`D2_YYYYMMDD_HHMMSS.png`, same-second re-shots walk `_1`..`_9`), with a
  `saved ...` toast and a log line. The captured frame is the gamma-corrected
  game raster at native game resolution, taken before the overlay draws — no
  version tag, conf panel or toast in the file, ever (Scroll Lock composes,
  but is no longer required for a clean shot). Works with or without a preset
  chain; foreground-gated and edge-latched like every hotkey; the encode+write
  is a few ms (dependency-free PNG encoder, host-verified end to end). Disable
  with `screenshots=0`. A slang-FILTERED capture (post-shader pixels) needs a
  GPU readback pass and stays future work — the shot is the pre-shader game
  frame.
- **The overlay shows the active shader (0.4.2)**: while a preset filters
  frames, the fallback box's second line reads the preset's file name — pushed
  on chain ready (first open, conf change, hot reload), cleared whenever the
  chain dies; no preset = no line. Also fixed in 0.4.2: the 8x8 font's crushed
  descenders (`g p q j` — "can't see the bottom of the g in glide").
- **The overlay names the running game exe (0.4.5 addendum)**: detected once
  per process at the first window open, the box's line under the tag shows
  which `Game.exe`/`d2.exe` the wrapper is inside and its classic-D2 patch —
  `Game.exe Retail 1.14d` when the exe's CRC-32 is in the known-build table
  (a content fingerprint that outranks the resource tiers: the bytes are
  ground truth; the table also carries the Vanilla+ builds —
  `d2s-9cf9f69.exe Vanilla+ 0.0.2` / `d2s-4857c30.exe Vanilla+ 0.0.1`), `d2.exe 1.14d` when the exe's VERSIONINFO resource maps (the
  Blizzard-stamped `1.0.9.22` / `1.0.10.9` / `1.0.12.49` / `1.0.13.60` /
  `1.0.13.64` / `1.14.3.68` / `1.14.3.71` pairs, the same field-tested table
  d2dx uses), the dotted resource value (`Game.exe v1.0.13.55`) when it does
  not, or the exe's CRC-32 (`d2.exe [CRC 3A5D2B1F]`) for unknown builds with
  no version resource at all — this repo's own CMake-built d2.exe among them.
  Detection never touches the game: fail-soft, and the raw values land in
  `glide3x.log` either way.
- **The tag line carries a live FPS field (0.4.5 addendum)**: the top-right
  box's first line reads `glide3xvk <ver> (<hash>) FPS 025` — a per-second
  count of presented frames kept by the wrapper on the present seam (both
  present paths count: `grBufferSwap` for gameplay and `grLfbUnlock` for the
  Bink video loop), so it shows the game's true frame pace and the in-game
  `/fps` readout is no longer needed for a quick sanity check. `FPS 000`
  shows until the first full second completes; the value recomposes once per
  second (one snprintf per second of play, not per frame) and clamps at 999.
  The renderer name reported through `grGetString(GR_RENDERER)` stays the
  plain static tag — external queries keep a stable, comparable string.
- **Chocolate-Doom-VK-style hotkeys (0.4.3)**: in-game **F11** cycles the
  content aspect (`4:3 -> 16:10 -> 16:9 -> auto`) and **F12** cycles the shader
  base resolution (`1x -> 2x -> 3x -> 4x`) — the frame is linearly upscaled N-x
  before the chain sees it, so source-relative pass FBOs (scanlines, glow)
  render finer. Toast feedback in the overlay, logged, session-only; both also
  settable via the new `aspect=` / `base_res=` conf keys (see the
  [d2slangvk.conf reference](#d2slangvkconf-reference) below).
- **Shift+V conf panel (0.4.5)**: press **Shift+V** in-game to toggle an
  overlay panel on the left edge (below the game's `/fps` line) listing every
  `d2slangvk.conf` option as currently resolved — preset, scale, vsync,
  aspect/base_res (the live F11/F12 picks annotated with their `file:` values
  when they differ), window/borderless and the full controller block with the
  same defaults the modules apply. Re-read from disk each time it opens (the
  file is the truth — even with no chain running, where the present-path hot
  reload never fires), and refreshed live while it is up: conf hot-reloads and
  F11/F12 presses update it in place. Works with or without a preset chain;
  `overlay=0` in glide3xvk.ini hides it with the rest of the overlay.
  **Up/Down** move a green selection cursor through its option lines, and
  **Left/Right** change the selected option — one press = one step written
  straight into `d2slangvk.conf` (a surgical single-key rewrite that keeps
  comments, spacing and key order; every consumer hot-reloads the file, so
  the change lands live like a hand edit): preset opens the preset browser
  (below), scale/vsync/aspect/base_res/window step their token
  lists, the toggles flip, controller_sens/aimsens/deadzone step and clamp
  exactly like their conf fixups. With `controller=0` the controller
  sub-options are inert, so the panel skips their rows (`controller off`
  itself stays — Left/Right re-enables it, which re-lists them). **Left/Right**
  on the preset row opens the preset browser: the `*.slangp` list of the
  current preset's folder subtree in the panel (current preset starred,
  `(none)` at the front, scrolling), **Up/Down** to move, **Right** loads the
  highlighted preset live, **Left** goes back to the option list.
- **Preset cycling, Ctrl+Left/Right (0.4.6)**: list up to eight presets in
  `d2slangvk.conf` (`preset_1=` .. `preset_8=`) and step through them in-game —
  **Ctrl+Right** forward, **Ctrl+Left** back, wrapping, each press reloading
  the chain exactly like a hand edit of `preset=` (shader line + conf panel
  follow on their own), with a toast naming the preset. The pick is a session
  override that survives D2's context re-opens and yields to `preset=` the
  moment the file's value changes. No chain required — cycling from a
  preset-less conf builds the chain on the spot. Works while the conf panel is
  up or down (plain arrows still edit the panel's selected option; the Ctrl
  part of the chord routes to the cycler).
- **Window size presets (0.4.5)**: `window=native|240p|480p|576p|720p|768p|1080p|desktop|WxH`
  and `borderless=0|1` in `d2slangvk.conf` size the OS window the game presents
  into — canonical sizes (240p=320x240 ... 1080p=1920x1080; `desktop` =
  monitor-sized borderless), centered, `desktop`/`borderless` strip the frame.
  **Config-only** (no hotkey — F10 belongs to the game's own skill-slot binds):
  applied at every renderer (re)open, and a conf edit hot-reloads within half a
  second (toast + log). The resize is
  **DPI-aware** (runs inside `SetThreadDpiAwarenessContext(PER_MONITOR_AWARE_V2)`,
  loaded dynamically), so presets come out at their true physical size even
  though the game is DPI-unaware — no desktop-scaling fiddling required. The
  game still renders 800x600 into it: presets are output sizing (L4 — the
  game's own render resolution stays the game's), and the unfiltered present
  path now fits + centers with black bars instead of stretching, so 4:3
  content stays 4:3 on any 16:9 window with or without a chain.
- **Controller support (0.4.5, experimental)**: opt-in gamepad play via input
  injection —
  `controller=1` in `d2slangvk.conf` starts a background XInput poller that
  drives the game through ordinary injected mouse/keyboard events (SendInput),
  so every D2 screen works unchanged. Map tuned for Diablo II (movement IS
  the mouse): left stick = walk (steers the cursor and holds left click while
  deflected — D2 walk-to-cursor), right stick = fine aim (~35% speed for
  putting the cursor on monsters/portals/labels), LT = left click, RT = right
  click, A = hold Shift+LMB (attack in place toward the cursor — hit things
  near you without pixel-perfect aiming), B = hold Alt (item labels), X =
  Space, Y = I, D-pad = belt 1-4, LB = C, RB = T, L3 = R (run/walk), Start =
  Esc, Back = Tab. **Cursor-home** (field round 2): after a short quiet dwell
  the cursor returns to the player, and every stick move emanates from there
  — it works without seeing the player because a D2 walk always ends with the
  character standing AT the cursor, so each walk release re-syncs home. The
  cursor can no longer get lost at the screen edge. Tunables:
  `controller_sens=`, `controller_aimsens=`, `controller_deadzone=`,
  `controller_moveclick=` (set 0 for a cursor-only left stick),
  `controller_home=` (set 0 for a free cursor). Injects only while the game
  window is focused; off by default; XInput loaded dynamically (no new DLL
  imports). Status after three field rounds: **experimental** — close but not
  fully dialed in yet; `controller_home=0` restores a plain free cursor if the
  parked cursor fights hybrid mouse use. See the conf reference below.
- **Perf round 1 (0.4.5)**: the full-screen 0.4.3 field log showed
  avg=51.6 ms ≈ 19 fps @ 800x600 with the crtglow_lanczos 7-pass chain active
  (≈ 470 draw calls per frame; the chain itself filtered 516 frames with
  0 fallbacks — the chain is not the bottleneck). 0.4.5 goes after the CPU
  side: **SSE2 float math** (i686 MinGW defaults to x87 for all float math,
  and the raster is float-heavy), an **O(1) texture table** (binds were a
  linear scan over up to 8192 slots) and **swapchain minImageCount+2** (the
  log showed images=2, the bare minimum). Verified bit-identical by a
  220-frame parity harness, so pixels cannot change — this round is pure
  speed. Field numbers land in the next round's `frame N:` lines.
- Frame budget today (0.4.3 field log, chain active): 51.6 ms ≈ 19 fps
  @ 800x600 — the CPU raster + texture upkeep dominate; the chain adds a
  fullscreen GPU pass on top but is not the bottleneck. 0.4.5 attacks the CPU
  side (see above); moving the compose buffer to the GPU remains the 60 fps
  endgame (PLAN.md §5 perf track).
- Not wired yet, on purpose: GPU-side raster (the 60 fps endgame), bilinear
  texture filtering (a look delta only; the ground's `texFilterMode(1,1)`
  request is point-sampled).

## Install

No build required:

1. Get `glide3x.dll` — the
   [releases page](https://github.com/VTSTech/D2LoD-libslang-vk/releases)
   (0.3.1 there today) or the `glide3x-dll` artifact of the latest run in
   [Actions](https://github.com/VTSTech/D2LoD-libslang-vk/actions) (current
   code, preset chain included).
2. Copy it into your Diablo II folder, next to the original `Game.exe`
   (vanilla retail 1.14d). This repository contains no game data and no Blizzard
   binaries; the DLL only draws what the game hands it.
3. (Optional, the payoff) Copy `presets/d2slangvk.conf.sample` from the repo as
   `d2slangvk.conf` beside `Game.exe`, uncomment a `preset=` line, and every
   frame runs through that slang shader — hot-reload mid-game, `fit`/`integer`/
   `stretch` scaling, `aspect=`/`base_res=` geometry, F11/F12 live hotkeys,
   Shift+V conf panel, Scroll Lock overlay toggle, unfiltered fallback on any
   failure. No conf = the exact 0.3.x behavior.
   Every accepted value is documented in the
   [d2slangvk.conf reference](#d2slangvkconf-reference) below.
4. Launch: `Game.exe -w -3dfx` (`-w` windowed, `-3dfx` selects the Glide renderer).

A healthy run looks like: the menu renders, play works, and `glide3x.log` beside the
exe carries the session banner (`glide3xvk 0.4.5 <build-id>`), periodic `frame N:`
summaries, and a clean exit summary on shutdown. With a preset configured, the log
adds `slang: chain ready '<preset>' ...` and per-event slang lines; without one, it
says so once. If 32-bit Vulkan is unavailable
(`vulkan-1.dll` x86 missing), the log says so, the DLL still loads, and the game keeps
running — the DirectDraw path (`-w` without `-3dfx`) is untouched by all of this.

## The version stamp

The stamp is the original P2.10 overlay box — `glide3xvk <ver> (<hash>)` drawn
every present into the **top-right** corner (8x8 bitmap font, 65%-black backdrop,
1px drop shadow, post-gamma, never in the game's own framebuffer). It draws by
default; `overlay=0` in glide3xvk.ini hides it, `overlay=1` forces it (re-read on
every context open):

```ini
[glide3xvk]
overlay=0
```

**Scroll Lock toggles it live (0.4.6)**: with the game window focused, a
Scroll Lock press hides or restores every overlay box at once (the tag, the
shader / hostver / FPS lines, the toast and the Shift+V conf panel) — the
boxes vanishing and reappearing is the feedback, and the show press adds an
`overlay on` toast once the box is back. The toggle is a session override:
it survives D2's constant context re-opens (the 640x480 probes, menu and
game each open one, and a plain ini re-push would pop the boxes back
mid-session), while the ini value still decides the state at startup and
after a relaunch — file wins at load, key wins in flight, the same split as
the F11/F12 hotkeys vs `d2slangvk.conf`. While hidden nothing overlay-related
draws and the conf-panel hotkeys park (the arrow keys stay the game's);
every press lands in `glide3x.log` (`overlay: on/off (Scroll Lock)`).

The game's own `/fps` line keeps its stock `Video: Glide` text — an earlier
attempt to repoint that string (0.4.1-0.4.3, `g3x_drivertag`) never showed on
field builds and was removed in 0.4.5; the game binary is no longer patched at
all.

## d2slangvk.conf reference

`d2slangvk.conf` lives beside `Game.exe` (same folder as `glide3x.dll` and
`glide3xvk.ini` — the two files are independent: the ini only carries the
overlay switch above, everything shader-related lives in the conf). It is read
once at renderer init and re-checked every 32 frames while a chain filters:
edit the conf or the `.slangp` it points at in place and the chain reloads
within ~half a second, no game restart.

Syntax: a `[slangvk]` section header, `key=value` lines, `;` or `#` comment to
end of line, blank lines ignored, later assignments win. Unknown keys are
ignored (forward compatibility); a malformed value keeps the previous/default
one and never fails the session. No file at all — or no `preset=` line — means
the chain stays disabled and frames present unfiltered, exactly the 0.3.x look.

Every key (0.4.6):

| Key | Values (default first) | What it does |
|-----|------------------------|--------------|
| `preset=` | *(empty)* = off, or a path to a `.slangp` | The libretro slang preset every presented frame runs through. Relative paths resolve against the game exe folder; drive-letter (`F:\...`) and UNC paths work as-is. Empty or missing line = chain disabled. Changing it (in the file) triggers a full preset reload. |
| `preset_1=` .. `preset_8=` | *(empty)* = slot unused, or a `.slangp` path each | The Ctrl+Left/Right cycle list (0.4.6): up to eight presets to step through in-game, wrapping, with a toast. Same path rules as `preset=`; empty slots are skipped, order is 1..8; a current preset not in the list starts the walk at `preset_1` (Right) or the last entry (Left) — add `preset=` itself to the list to include it in the rotation. A pick lives for the session (survives D2's context re-opens) and yields to `preset=` when the file's value changes. No chain required — cycling from a preset-less conf builds the chain on the spot; a bad preset file fails soft (logged, unfiltered present, next press moves on). Hot-reloads like every key. |
| `scale=` | `fit` \| `integer` \| `stretch` | How the chain's output sits in the window: `fit` fits + centers with letterbox; `integer` picks the largest whole-number multiple of the game resolution, centered; `stretch` fills the window edge to edge. Ignored while an explicit `aspect=` (or an F11 pick) is active. |
| `vsync=` | *(empty)* = FIFO \| `0` \| `1` | Present mode of the swapchain the chain renders into: empty = default FIFO (vsync on); `0` prefers IMMEDIATE (uncapped, silently falls back to FIFO where unsupported); `1` = FIFO. Takes effect when the swapchain is (re)created — game start or resolution change. |
| `aspect=` | `auto` \| `4:3` \| `16:10` \| `16:9` | Content-aspect override (0.4.3). `auto` keeps the `scale=` geometry. A fixed ratio letterboxes the chain output into the largest centered rectangle of that ratio inside the window: `4:3` on a 16:9 window = pillarbox with correct proportions; `16:9` on a 16:9 window = full-bleed. F11 cycles this live. |
| `base_res=` | `1x` \| `2x` \| `3x` \| `4x` (bare `1`..`4` accepted) | Shader base resolution (0.4.3). The game frame is linearly upscaled N-x on the GPU before the chain sees it, so every source-relative pass FBO renders at N-x — scanline pitch, glow and blur radii get finer as N rises; the final pass is fit back into the output rect. Costs one extra fullscreen blit. F12 cycles this live. |
| `window=` | *(empty)* = `native` \| `240p` \| `480p` \| `576p` \| `720p` \| `768p` \| `1080p` \| `desktop` \| `WxH` | Present-window preset (0.4.5). Sizes the OS window: `240p`=320x240, `480p`=640x480, `576p`=720x576, `720p`=1280x720, `768p`=1366x768, `1080p`=1920x1080, `desktop` = the current monitor's size (borderless), `WxH` = any explicit client size (e.g. `1600x1200`). Windowed presets are centered on the current monitor; the game's own 800x600 window is untouched at `native`. DPI-aware (physical pixels, see the presets bullet above); reapplied at every renderer (re)open, and conf edits hot-reload live (file is the source of truth). Config-only — no hotkey. This is output sizing only — the game keeps rendering 800x600. |
| `borderless=` | `0` \| `1` | Strip the caption/borders on preset windows (0.4.5): `WS_POPUP` instead of the game's overlapped style, centered like the windowed presets. `desktop` is borderless regardless. `0`/empty keeps the game's own frame. |
| `screenshots=` | `1` \| `0` | The PrintScreen PNG key (0.4.6, on by default): pressing PrintScreen in-game saves the gamma-corrected game frame at native resolution into `screenshots\` beside the exe (`D2_YYYYMMDD_HHMMSS.png`, `_1`..`_9` on same-second collisions), captured before the overlay draws (no tag/panel/toast in the file) — see the screenshot bullet above. `0` parks the hotkey: a press logs once (`shot: PrintScreen ignored (screenshots=0 ...)`) and does nothing. The panel's `screenshots` row (Shift+V) toggles this live. |
| `controller=` | `0` \| `1` | Gamepad-to-mouse/keys input injection (0.4.5, off by default). `1` starts a background XInput poller (pad 0) that injects ordinary mouse/keyboard events — the game sees a normal mouse and keyboard, so every screen works. Map: left stick = walk (cursor + held LMB while deflected), right stick = fine aim, LT = LMB, RT = RMB, A = hold Shift+LMB (attack in place toward the cursor), B = hold Alt (item labels), X = tap Space, Y = tap I, D-pad = belt 1-4, LB = tap C, RB = tap T, L3 = tap R (run/walk), Start = Esc, Back = Tab (automap). Injects only while the game window is foreground; focus loss / disable / shutdown release everything held; taps are one clean keystroke (no auto-repeat flips). XInput loads dynamically (`xinput1_4` / `xinput9_1_0` / `xinput1_3`; none = logged once, feature stays off). Settings hot-reload with the conf (~2 s). |
| `controller_sens=` | float (default `1.0`, clamp `16`) | Cursor speed multiplier for the left stick (0.4.5). Full-tilt base speed is ~1200 px/s before the multiplier; the squared response curve keeps precision near center. Missing/`0` = default. Hot-reloads. |
| `controller_aimsens=` | float (default `0.35`, clamp `0.05`..`4`) | Right-stick fine-aim speed as a multiplier of the left-stick speed (0.4.5). Same curve and deadzone; slower by default so the cursor can be placed on a monster, portal or item label. Missing/`0` = default. Hot-reloads. |
| `controller_deadzone=` | percent `1`..`80` (default `15`) | Radial stick deadzone as a percent of full throw (0.4.5). Stick response stays zero inside the zone and follows the squared curve outside it. Missing/`0` = default 15. Hot-reloads. |
| `controller_moveclick=` | absent = on, `0` = off | Left-stick walk behavior (0.4.5): while the stick is deflected past the deadzone, left click is held so the character walks toward the cursor (D2 movement). Set `0` for a cursor-only left stick (clicks come only from LT/A). Hot-reloads. |
| `controller_home=` | absent = on, `0` = off | Cursor-home (0.4.5, experimental): after ~0.4 s of quiet (sticks centered, nothing injected) the cursor is moved back onto the player, and every stick move emanates from there. Needs no player detection — a D2 walk always ends with the character standing at the cursor, so each stick-walk release re-syncs home (self-correcting; a walk stopped short by an obstacle re-syncs on the next one). The cursor can no longer get lost at the screen edge. Two honest limits: the cursor is parked, not hidden (hiding D2's own cursor sprite would mean intercepting the game's WM_SETCURSOR — Game.exe territory, out of scope), and a click that walks the character somewhere (LT on a far portal) leaves home there until the next stick-walk. Requires `controller_moveclick` on; a real mouse always wins while it moves. Set `0` for a plain free cursor. Hot-reloads. |

A worked example (the field setup from the round-16 screenshots):

```ini
[slangvk]
preset=shaders/crt/crtglow_lanczos.slangp
scale=stretch
vsync=
aspect=auto
base_res=1x
```

### Hotkeys (in-game, 0.4.3 + 0.4.5 + 0.4.6)

While the game window has focus — the same scheme as Chocolate-Doom-VK
(Scroll Lock, Shift+V and the arrows work always; F11/F12 require a chain to
be filtering frames):

- **Scroll Lock** (0.4.6) — toggle the overlay: every overlay box (the tag,
  the shader / hostver / FPS lines, the toast and the conf panel) hides or
  restores at once, for clean screenshots and recordings. Session-live —
  survives D2's context re-opens; the `overlay=` ini key still decides the
  state at startup and after a relaunch. See [The version stamp](#the-version-stamp)
  for the details.
- **PrintScreen** (0.4.6) — save a screenshot: the current frame lands in
  `screenshots\` beside the game exe as a PNG, captured before the overlay
  draws (no tag/panel/toast in the file) and at native game resolution, with
  or without a preset chain. Toast + log; `screenshots=0` disables (the press
  logs once instead). Works while the overlay is toggled off — the log line
  is the feedback there.
- **F11** — cycle content aspect: `4:3 -> 16:10 -> 16:9 -> auto -> ...`
- **F12** — cycle shader base resolution: `1x -> 2x -> 3x -> 4x -> 1x`
- **Shift+V** (0.4.5) — toggle the conf panel: a box on the left edge listing
  every `d2slangvk.conf` option as currently resolved (defaults filled in;
  the F11/F12 session picks annotated `file: ...` when they differ). Re-read
  from disk every time it opens, refreshed live on conf hot-reload and
  F11/F12 presses while it is up. Works with or without a chain — the
  `window=` / `controller=` options matter without one. Press again to hide.
- **Up/Down** (0.4.5) — while the conf panel is up, move its green selection
  cursor through the option lines (clamped at the ends; every move re-parses
  the conf and is logged). While the panel is hidden the arrow keys are
  untouched — the game keeps them.
- **Left/Right** (0.4.5) — while the conf panel is up, change the selected
  option: one press = one step, written straight into `d2slangvk.conf` and
  applied live through the normal conf hot-reload path (see the Shift+V
  bullet above for the per-option steps). List ends, an empty preset dir
  and failed writes touch nothing and are logged. On the preset row they
  open the preset browser instead (below); inside the browser **Right**
  loads and **Left** backs out.
- **Ctrl+Left/Right** (0.4.6) — cycle the preset through the conf's
  `preset_1=`..`preset_8=` list, wrapping, with a toast and a live chain
  reload (see the preset-cycling bullet above). Works with the conf panel up
  or down; the Ctrl part of the chord routes to the cycler, so the panel's
  plain Left/Right value edits never double-fire on the same press.
- **Preset browser** (0.4.5) — while the conf panel is up and the cursor
  sits on the preset row, **Left/Right** open it: the `*.slangp` files of
  the **current preset's folder subtree** listed in the panel
  (`presets (N) - <dir>`, current preset starred, `(none)` first). The scan
  root follows `preset=`'s folder and recurses to any depth — shader
  collections nest deeper than two folders, and a leaf-only listing would
  show one entry per folder: `presets\crt.slangp` lists everything under
  `presets` (`presets\...` rows), `shaders\crt\royale\kurozumi.slangp`
  lists everything under `shaders\crt` (`shaders\crt\royale\...` rows
  included); a bare or empty `preset=` scans the game dir tree (an absolute
  `preset=` too — browser rows are relative paths by contract). Every row
  is written back exactly as `preset=` accepts it, so a pick resolves to
  the very file listed. **Up/Down** move the cursor, **Right** loads the
  highlighted preset through the same write-conf + hot-reload path as the
  value edits (the chain rebuilds live), **Left** goes back to the
  option page. An empty tree toasts `no .slangp under <folder>` instead of
  opening the page. The browser never asks for **Enter** or **Esc** — those
  reach the game's chat box and its menu, and a polled hotkey cannot
  swallow them.

Each press shows a toast line in the overlay box (`aspect 16:9`,
`shader base 2x`, `conf overlay on`) for ~2 seconds and logs a
`slang: F11/F12 ...` / `overlay: conf panel ...` line. The toast shows
whether or not a shader is loaded — an earlier build silently dropped it
without an active shader line, which is why a browser toast could go
unseen. The tag box itself now ends with a permanent bottom line,
`GitHub: VTSTech/D2LoD-libslang-vk`, so every screenshot the overlay stamps
also names the project's home. The F11/F12 picks
are **session-only**: they are never written back to `d2slangvk.conf`, and
a conf hot-reload snaps them back to whatever the file says. The panel's
Left/Right edits are the opposite — they write the file, which is what makes
them live for every consumer. The window presets have **no hotkey** by
design — F10 is a game skill-slot bind and the wrapper must not steal game
keys; the panel's window row (or the conf itself) is the way to change them.

## Build from source

```sh
apt install g++-mingw-w64-i686 binutils-mingw-w64-i686 cmake ninja-build   # Debian/Ubuntu

cmake -S . -B build-g3x -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-i686.cmake
cmake --build build-g3x --target glide3x
# → build-g3x/glide3x.dll (pei-i386; KERNEL32 + msvcrt imports only)
```

CI (`.github/workflows/build.yml`) builds exactly this on every push to `main` or
`libslang-vk` and uploads the DLL as the `glide3x-dll` workflow artifact. The build id
embedded in the log banner, `grGetString(GR_RENDERER)` and the overlay comes from
`git describe --always --dirty --tags` at compile time — build from a clean checkout
of the pushed branch and the tag names a commit you can find on GitHub.

Verify the export surface against the game's own loader (must be an empty diff):

```sh
i686-w64-mingw32-objdump -p build-g3x/glide3x.dll \
  | sed -n '/Ordinal\/Name Pointer/,$p' \
  | grep -oE '_[A-Za-z]+@[0-9]+' | sort -u > /tmp/ours.txt
grep -oE '"_[A-Za-z]+@[0-9]+"' D2Client/Renderer/Glide.cpp | tr -d '"' | sort -u > /tmp/game.txt
diff /tmp/game.txt /tmp/ours.txt && echo "MATCH: all 45 GetProcAddress names covered"
```

## Documentation

- [glide3xvk/README.md](glide3xvk/README.md) — the renderer's architecture, module
  layout, build, toolchain notes (GNU ld export decoration), overlay internals,
  field-test procedure and fail-soft behavior
- [glide3xvk/FIELD-TEST.md](glide3xvk/FIELD-TEST.md) — the per-round field log:
  builds, diagnostics and results on a vanilla retail 1.14d install
- [SCOPE.md](SCOPE.md) — the capability boundary: what the wrapper achieved and
  can still do alone vs. what needs Game.exe work (source-receipted)
- [glide3xvk/BUILD-32BIT.md](glide3xvk/BUILD-32BIT.md) — building libslang-vk itself
  as i686 Windows (the P3 gate)
- [PLAN.md](PLAN.md) — the P1–P5 integration plan with the phase ledger

## Roadmap

- **P3 (shipped, 0.4.0; chain field-proven live)** — the libslang-vk preset
  chain at the present pass (`d2slangvk.conf`, hot-reload, scaling modes,
  fail-soft); the full-screen 0.4.3 log shows a 7-pass community chain
  filtering every frame with 0 fallbacks
- **Perf track (in progress, 0.4.5)** — CPU raster round 1 shipped (SSE2 float
  math, O(1) texture table, swapchain headroom — bit-identical pixels); round-2
  candidates: present-path fence pipelining, span-raster templating; the GPU
  compose move is the 60 fps endgame (PLAN.md §5)
- **P4** — preset parameter UI, CI-packaged drop-in zip (DLL + presets +
  conf), crash-reporter hook
- Deferred look item: bilinear texture filtering

## Upstream

This branch is part of a fork whose `master` line tracks the upstream 1.14d source
recovery; the recovered game source stays untouched here — glide3xvk needs zero
game-source edits, and the DLL is designed to run against the vanilla retail exe.
While testing, upstream PRs we may discover (recovery fixes, build or diagnostics
material) will be noted and, where they hold up, sent upstream — but the work on this
branch is glide3xvk, and glide3x.dll loading Vulkan / libretro slang with libslang-vk
is the goal.

Diablo and Blizzard Entertainment are trademarks of Blizzard Entertainment, Inc. This
project is not affiliated with or endorsed by Blizzard.
