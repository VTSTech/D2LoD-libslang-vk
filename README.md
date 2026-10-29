# glide3xvk — our own `glide3x.dll` for D2LoD (Glide 3 → Vulkan)

> **Standalone repo** — this tree builds ONLY the glide3x.dll wrapper,
> trimmed out of the `D2LoD-libslang-vk` superproject (no game source inside).
> Build it with `cmake -S . -B build-g3x -G Ninja
> -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-i686.cmake` and a `libslang-vk` checkout
> beside it (see BUILD-32BIT.md); CI does exactly that on every push.

The game's Glide loader (`D2Client/Renderer/Glide.cpp`) does
`LoadLibraryA("glide3x.dll")` and resolves **45 stdcall exports**. This
directory builds a drop-in replacement for that DLL: it exports exactly those
45 entry points, implements the Glide 3 state machine and raster semantics D2
actually uses, and presents the frame through a Vulkan swapchain. It runs on a
**vanilla retail 1.14d install** (`Game.exe -3dfx`) — a rebuilt `d2.exe` is
not required.

Status: the game renders correctly through it — menu, world, units, UI,
shadows, text, cutscenes (field-verified over rounds 4–12; see
`FIELD-TEST.md`). The CPU raster feeds a BGRA compose buffer; the GPU side is
bootstrap + present. **P3 shipped (0.4.0): the libslang-vk preset chain sits
at the present pass** — set `preset=` in `d2slangvk.conf` and every presented
frame runs through the slang preset at up-scaled resolution; the full-screen
0.4.3 field log proves it live on real hardware (crtglow_lanczos 7-pass chain,
516 filtered frames, 0 fallbacks). **0.4.5 ships perf round 1** for the CPU
side (SSE2 float math, O(1) texture table, swapchain headroom — verified
bit-identical by the agent-side parity harness); field numbers pending.

```
d2.exe ── LoadLibraryA("glide3x.dll") ──► glide3x.dll (this directory)
              45 Glide exports                │
              grDraw* / grTex* / grLfb*       ├─ P2 (done): Glide raster core
              grBufferSwap:                   │   paletted textures, combine
               compose buffer                 │   states, fan/strip/point
               + gamma                        │   raster, chroma, LFB, fills
               + version-tag overlay          ├─ P3 (done, 0.4.0): preset chain
               → queue submit                 │   slangvk_image (game res) →
                                              │   slangvk_chain_render_swapchain
                                              │   (d2slangvk.conf, hot-reload)
                                              └─ present → Vulkan swapchain
```

`libslang-vk` is **embedded** at the present pass — it is not the DLL itself
(it doesn't speak Glide; it turns an input image into a preset-filtered
swapchain image). The DLL links the static `slangvk` target and adopts our
own Vulkan context (`slangvk_device_adopt`); without `d2slangvk.conf` the
chain never engages and the DLL behaves exactly like 0.3.x.

## Layout

| Path | What |
|---|---|
| `src/glide_exports.cpp` | All 45 exports with exact signatures; the game-contract entry points (`grGet(0xF)`, `grSstWinOpen`, ini reading, session banner) |
| `src/g3x_frame.{h,cpp}` | The P2 raster core: texture cache (P_8 / ARGB_8332 / RGB_565), palettes, scanline span rasterizer (fan-quad, pointer-array strips/fans, points/lines), combine/blend/chroma semantics, fill-rect fast path, LFB, gamma LUT, compose + present seam; 0.4.5: the texture table is O(1) (open-addressing hash + monotonic alloc cursor + same-texture rebind fast-out) |
| `src/g3x_state.{h,cpp}` | Glide state tracking (grChromakey/grColorCombine/... current values) |
| `src/g3x_overlay.{h,cpp}` | The version tag + active-shader line: composes `glide3xvk <version> (<short hash>)`, draws it as a top-right box (the fallback stamp since 0.4.1), since 0.4.2 a second line with the active preset's file name, since 0.4.3 a transient toast line for the F11/F12 hotkeys, since the 0.4.5 addendum the toggleable **Shift+V conf panel** on the left edge (storage + drawing; the content comes pre-formatted from g3x_slang) with its green arrow-key selection cursor, since the same addendum a permanent bottom line — the repo credit `GitHub: VTSTech/D2LoD-libslang-vk` — and (0.4.5 addendum) the host-exe version line pushed by g3x_hostver, drawn directly below the tag; the box's tag copy also carries the live ` FPS nnn` field (0.4.5 addendum: pure 1-second-window meter, ticked from g3x_frame_present); since 0.4.6 the module owns the Scroll Lock session toggle (`g3x_overlay_toggle` — a session override composed against the ini base, so the flip survives D2's context re-opens) |
| `src/g3x_drivertag.{h,cpp}` | 0.4.1–0.4.3: repointed the game's `"Video: Glide"` renderer-name string at our tag. **Removed in 0.4.5** (field call: the /fps tag must stay the game's own; the DLL is now strictly read-only toward Game.exe) — pending `git rm` of the two files |
| `src/g3x_slang.{h,cpp}` | P3: the libslang-vk preset chain — adopts our Vulkan context, parses `d2slangvk.conf` (`preset`/`scale`/`vsync`/`aspect`/`base_res`/`window`/`borderless`), owns the input image + preset + chain, hot-reloads on file change, polls the F11/F12 hotkeys on the present path plus the Shift+V conf-panel toggle on EVERY present (chain or no chain; it formats and pushes the panel's lines from a fresh conf parse + the live hotkey state) with the panel's Up/Down selection, Left/Right value editing and the Left/Right preset browser over the current preset's folder subtree — recursed to any depth, rows stored as the relative paths `preset=` accepts (0.4.5: the edit is a surgical single-key conf rewrite — `g3x_slang_conf_rewrite_text`, host-tested — pushed through the normal conf machinery, so it lands live; 0.4.6: the whole poll parks with its edge latches reset while the Scroll Lock overlay toggle has the overlay hidden, so no invisible conf edits and the arrow keys stay the game's), fails soft to the unfiltered path |
| `src/g3x_hostver.{h,cpp}` | 0.4.5 addendum: identifies the running game exe for the overlay's tag box — computes a zlib-compatible CRC-32 of the exe file on disk (a content fingerprint, stable across rebuilds where a link timestamp is not), checks it against the known-build table (a hit names the binary outright, e.g. `Game.exe Retail 1.14d`), then falls back through the host process's VERSIONINFO resource (GetFileVersionInfo/VerQueryValue, the Blizzard-stamped `dwFileVersion` pair mapped to the community patch names via the field-tested d2dx table: 1.09d/1.10/1.12/1.13c/1.13d/1.14b/1.14d) and the raw fingerprint (`d2.exe [CRC 3A5D2B1F]`); runs once per process from grSstWinOpen, fail-soft, evidence always logged. Pure logic (tables, CRC-32, line format) is windows.h-free and host-tested (scripts/test_hostver.cpp); the Win32 pass sits under `_WIN32` and is fakecheck type-checked |
| `src/g3x_window.{h,cpp}` | 0.4.5: present-window presets + DPI awareness — sizes the game's own HWND to a `window=` preset (canonical sizes, `desktop`, `WxH`), config-only (applies at every renderer (re)open + conf hot-reload; no hotkey — F10 is a game skill-slot bind), reverts cleanly to the game's own window; pure table/parse lives inline in the header (host-tested), the Win32 apply runs inside `SetThreadDpiAwarenessContext(PER_MONITOR_AWARE_V2)` |
| `src/g3x_controller.{h,cpp}` | 0.4.5 (experimental): controller support — a background XInput poller (pad 0, ~125 Hz, own thread; zero present-path cost) injects ordinary mouse/keyboard via SendInput (left stick = walk: cursor + auto-held LMB; right stick = fine aim at `controller_aimsens` speed; LT = LMB, RT = RMB, A = hold Shift+LMB attack-in-place combo, B = Alt hold, X = Space, Y = I, D-pad = belt 1-4, LB/RB = C/T, L3 = R run/walk, Start = Esc, Back = Tab), gated on game-window focus, everything held released on focus loss/disable/shutdown; cursor-home (0.4.5 field round 2): the walk-release cursor position becomes the player anchor (D2 walks end standing at the cursor — self-correcting) and a 0.4s quiet dwell parks the cursor back on it, so stick motion always emanates from the player; opt-in via `controller=` + `controller_sens=` + `controller_aimsens=` + `controller_deadzone=` + `controller_moveclick=` + `controller_home=`, hot-reloaded from the conf; XInput loaded dynamically (no new imports); pure core (bind table with multi-step combos, stick curve, desired-state diff step) inline in the header, host-tested (scripts/ctrltest.cpp) |
| `presets/` | Two starter presets (scanline, sharpen — MIT, ours) + `d2slangvk.conf.sample` |
| `src/g3x_font8x8.h` | Embedded 8x8 1-bit bitmap font for the overlay (generated from DejaVu Sans Mono Bold) |
| `src/g3x_log.{h,cpp}` | `glide3x.log` beside the game exe (`%TEMP%` fallback), log-once budgets, periodic frame summaries, exit summary |
| `src/g3x_types.h` | Minimal Glide type surface (mirror of the game's `d2_platform.h` block) + values observed in the in-tree `D2Glide` call sites |
| `src/vk_backend.{h,cpp}` | Vulkan bootstrap: volk → instance → win32 surface → device → swapchain → present; plus the 0.4.3 `g3x_vk_slang_blit_scaled` one-shot (fenced) upscale blit for the shader base resolution; 0.4.5: swapchain `minImageCount+2` for present-pacing headroom; fail-soft everywhere |
| `glide3x.def` | The 45 decorated export names (`_grBufferSwap@4`...), repo `binkw32.def` syntax |
| `vendor/volk/` | volk (runtime Vulkan loader, MIT — loads `vulkan-1.dll`, no SDK needed) |
| `vendor/Vulkan-Headers/` | Khronos Vulkan C headers (Apache-2.0) |
| `BUILD-32BIT.md` | **P1 spike notes**: building libslang-vk itself as i686 Windows |
| `FIELD-TEST.md` | The field log: per-round builds, diagnostics, and results on a vanilla retail 1.14d install |

## The version stamp (0.4.1) + the overlay's shader line (0.4.2) + hotkey toasts (0.4.3) + the conf panel with its arrows (0.4.5) + the Scroll Lock toggle + PrintScreen + preset cycling (0.4.6)

The primary stamp is the game's own debug line: the `/fps` block ends with
`Video: Glide` (a hardcoded string drawn at fixed game coords, decompiled
`D2Client/GAME/Rendering.cpp`), and `g3x_drivertag` repoints that string at a
static `glide3xvk <ver> (<short hash>)` buffer of ours with one 4-byte store —
the game then draws our tag in its own font at its own spot. Every patch step
is fail-soft and logged (`drivertag: ...` in glide3x.log); on any miss the
original P2.10 box below takes over untouched. The tag text uses a **short**
build id (`git rev-parse --short=7 HEAD` + `-dirty`) because the game
`sprintf()`s it into a `char[32]` stack buffer — worst case 31 chars, exact
fit; the long describe form would truncate mid-hash.

The fallback box is the original P2.10 overlay: the tag drawn every present
into the **top-right** corner (4px from the edge): 8x8 bitmap font, 65%-black
backdrop, 1px drop shadow, rendered post-gamma at swapchain resolution —
identical in the 640x480 probe phases, the 800x600 menu, gameplay, and the
LFB/Bink video path, and it never enters the game's own framebuffer. With no
ini key it draws only when the patch missed; the key is still honored
(re-read on every context open):

```ini
[glide3xvk]
overlay=1
```

**0.4.6: Scroll Lock toggles the whole overlay live.** The poll lives on the
present seam (`g3x_frame.cpp`, `overlay_scrolllock_poll`) — the same always-on
home as the Shift+V conf-panel poll, so the toggle works with or without a
preset chain, foreground-gated and edge-latched like every polled hotkey. The
flip itself is `g3x_overlay_toggle()` in the overlay module: a **session
override** composed against the ini base (`s_ini_enabled` / `s_session_toggle`
/ `s_enabled`), because `grSstWinOpen` re-pushes `overlay=` on every context
open and D2 opens contexts constantly (the 640x480 probes, the menu, the
game) — a plain flip of the pushed flag would pop the boxes back on the next
re-open. The override latches for the process lifetime; the ini still decides
the state at startup and after a relaunch. While hidden nothing draws (the
render early-out is unchanged), the conf-panel poll parks with all its edge
latches reset (a hidden panel must not keep editing the conf invisibly, and
the arrow keys stay the game's), and a conf panel that was up comes back up
when the overlay is toggled back on. The show press toasts `overlay on`; the
hide press only logs (its toast would draw into a box that just died) —
`overlay: on/off (Scroll Lock)` in glide3x.log either way.

**0.4.6: PrintScreen saves a screenshot** (new `g3x_shot` module, polled from
the same present seam). The capture point is BEFORE `g3x_overlay_render` on
either present path, so the PNG never carries the tag/panel/toast — Scroll
Lock composes with it but is no longer required for a clean shot. The frame
saved is the gamma-corrected game raster at native game resolution: on the
slang path that is the chain-input buffer (the chain itself renders straight
into the acquired GPU swapchain image, out of a CPU-side capture's reach — a
slang-FILTERED capture needs a Vulkan readback pass and stays future work),
on the legacy path an identical native-res copy is made on demand, so the
output shape never depends on which path is live. Naming is
`screenshots\D2_YYYYMMDD_HHMMSS.png` beside the game exe (`_1`..`_9` on
same-second collisions), the encoder is the house-rule dependency-free PNG
(8-bit truecolor RGB, filter-0 rows, STORED zlib blocks — ~1.4 MB for
800x600, host-verified end to end by chunk/CRC/inflate/pixel-round-trip),
and the whole encode+write runs synchronously on the game thread (a few ms,
at most one frame of hitch per press; the pixels are copied at the capture
point so the raster reuses its buffer next frame). `screenshots=0` in
d2slangvk.conf parks the hotkey (a press logs once); `screenshots` also has
a Shift+V panel row. The shot key is deliberately NOT parked while the
overlay is Scroll-Lock-toggled off — clean shots are the point — so while
the boxes are hidden the log line is the only feedback.

Since 0.4.2 the box carries a **second line when a preset filters frames**:
the active preset's file name (basename only). `g3x_slang` pushes it on chain
ready — first open, conf change, hot reload — and clears it whenever the
chain dies (`destroy_chain()` is the single choke point: reload start,
`preset_load`/`chain_create` failure, shutdown), so the line is truthful by
construction; with no preset the box shrinks back to the tag alone. The same
round re-tuned the 8x8 font's descenders (`g p q j`): bowls at rows 2-5, real
tails on rows 6-7 — DejaVu's descent does not fit the tile and the generated
tails were one broken row. Overrides live in `scripts/gen_font8x8.py`, which
ships in-tree now.

Since the 0.4.5 addendum the box also **names the exe it is running inside**:
`g3x_hostver` detects the host once per process at the first `grSstWinOpen`
(the same seam that reads `overlay=`) and pushes a line drawn directly below
the tag. The primary identifier is a zlib-compatible **CRC-32 of the exe
file on disk** — a content fingerprint, stable across rebuilds where a link
timestamp is not: a hit in the known-build table names the binary outright
(`Game.exe Retail 1.14d`), an unknown one shows the raw hex
(`d2.exe [CRC 3A5D2B1F]`) for resource-less builds, this repo's own
CMake-built d2.exe among them. Between the two sit the VERSIONINFO tiers —
`d2.exe 1.14d` when the exe's resource maps to a known patch, the dotted
value (`Game.exe v1.0.13.55`) when it does not. Both name sources are
field-extensible: the patch table is d2dx's field-tested set
(`1.0.9.22`=1.09d, `1.0.10.9`=1.10, `1.0.12.49`=1.12, `1.0.13.60`=1.13c,
`1.0.13.64`=1.13d, `1.14.3.68`=1.14b, `1.14.3.71`=1.14d), and the CRC table
grows from the evidence line every run logs (`ms`/`ls`/`crc`/`bytes`) —
quote the log line and the table gets the entry, so the exact binary gets
named from then on (the rows so far: the retail 1.14d Game.exe and the two
Vanilla+ builds — `Vanilla+ 0.0.1`/`0.0.2`). A known CRC outranks the
resource tiers: the bytes are ground truth. Every step is fail-soft; the
shader line and toast shift down one row while the hostver line shows.

Since the 0.4.5 addendum the tag line also carries a **live FPS field** —
`glide3xvk <ver> (<hash>) FPS 025` — so the in-game `/fps` readout is no
longer needed for a quick frame-pace sanity check. `g3x_frame_present` ticks
the meter once per presented frame (both present paths funnel through it:
`grBufferSwap` for gameplay and `grLfbUnlock` for the Bink video loop, which
never calls `grBufferSwap`), and a 1-second window of those ticks becomes the
displayed value — `FPS 000` until the first full second completes, then the
half-rounded per-second rate, clamped at 999. The clock reading comes from
`GetTickCount64` at the call site; the meter itself is pure (a `uint64_t` ms
argument, no clock calls) and host-tested with synthetic timestamps
(scripts/test_fps_tag.cpp: window rolls, rounding, clamp, no per-frame
recompose — the tag text is rebuilt only when the value changes, one snprintf
per second of play — clock-glitch tolerance, long-run stability). The string
`grGetString(GR_RENDERER)` reports stays the plain static tag: the FPS field
lives only in the box, so external renderer queries keep a stable name.

Since 0.4.3 the box also carries a **transient third line** when a hotkey
fires: F11 (content aspect, `4:3 -> 16:10 -> 16:9 -> auto`) and F12 (shader
base resolution, `1x -> 2x -> 3x -> 4x`) — the Chocolate-Doom-VK scheme. Both
only intercept while a chain filters frames and the game window is focused
(`GetAsyncKeyState` is process-global, so the focus check matters); each press
toasts (`aspect 16:9`, `shader base 2x`) for ~120 presented frames and logs a
`slang: F11/F12 ...` line. The picks are session-only — never written back to
`d2slangvk.conf`; a conf hot-reload snaps them back to the file's values. F12
feeds the chain from a game-res-x-N upscaled `slangvk_image` (one linear
`vkCmdBlitImage` in a fenced one-shot submit through `vk_backend`, layouts
round-tripped SHADER_READ_ONLY -> TRANSFER and back inside the same command
buffer), so every source-relative pass FBO re-derives at N-x; the output rect
math honors the aspect pick before `scale=`.

The 0.4.5 addendum rounds the box out with a **permanent bottom line** —
`GitHub: VTSTech/D2LoD-libslang-vk` — the one overlay line that never clears
(shader line) and never expires (toast), so every screenshot the box stamps
also names the project's home. Two smaller panel-era fixes ride along: the
hotkey toast now draws without the shader line too (a conf-panel toast with
no preset loaded used to be silently dropped), and the conf panel renders
before the tag box so the stamp wins any overlap on tiny present targets.

Window presets (0.4.5) carry the same toast scheme without a hotkey: `window=`
applies at renderer open and on conf change, toasting (`window 1080p
1920x1080`) from the swap path (g3x_window) — it works with the chain off.

Controller support (0.4.5) is log-only on purpose: `controller=1` prints one
`controller: on (...)` line with the active map and settings, the poll thread
lives entirely outside the present path (no grBufferSwap hook, no toast race),
and disabling or closing the renderer logs `controller: off (...)` after
releasing everything held. Focus loss releases too — after alt-tabbing back,
re-press a pad button if a held action did not resume.

The **Shift+V conf panel (0.4.5)** is the overlay's first toggleable box: a
left-edge sibling of the tag box (same 8x8 font, backdrop and shadow; it
starts one line below the game's `/fps` readout, which owns the very top-left
corner) listing every `d2slangvk.conf` option as resolved. The split of
responsibilities keeps each module honest: g3x_overlay only stores, toggles
and draws (it stays windows.h-free and host-testable); g3x_slang owns the
content — a fresh parse of the conf from disk on every open (file is the
truth, and this works with the chain OFF, where the present-path hot-reload
probe never runs), the module defaults filled in the way their consumers
apply them (`g3x_ctrl_conf_fixup` for the controller floats, empty
`window=` = native), and the two hotkey-cycled options shown at their live
value with a `file: ...` annotation when an F11/F12 pick differs from the
conf. While the panel is up it refreshes itself: conf hot-reloads (via
`reread_conf`) and F11/F12 presses re-push the lines, so what it shows is
never stale. The toggle polls on the present seam (g3x_frame_present) with
the same focus rule as F11/F12 — presses made while another window is
foreground never toggle — and honors the `overlay=` ini switch. The panel
round also fixed the 8x8 font's 'm': DejaVu Sans Mono Bold's triple-stem 'm'
collapsed to a solid block under the box filter, and the panel's controller
keys (`aimsens` / `moveclick` / `home`) were the first overlay text to expose
it — the generator (`scripts/gen_font8x8.py`) now hand-draws 'm' alongside
the descender overrides.

The **selection cursor (0.4.5)** rides on top: while the panel is up,
**Up/Down** move a green highlight through the option lines (the header and
the hint footer are skipped). The selected row draws a dim-green band across
the box with its glyphs in bright green (`#20FF20`-ish) — the only
non-white/non-black ink the overlay has ever used, so the cursor reads
instantly as "the thing the arrows move". g3x_slang owns the movement
(edge-latched like Shift+V, same focus rule, clamped at the ends with no
wrap, each move re-parsing the conf and logging `overlay: conf panel sel
<key>`); g3x_overlay only paints it (an out-of-range or stale index just
draws nothing, never out of the box). While the panel is hidden the arrow
keys are untouched — the game keeps them.

The **value editing (0.4.5)** is the other half: **Left/Right** on the
cursor's row change that option — one press = one step, written straight
into `d2slangvk.conf` through `g3x_slang_conf_rewrite_text` (a surgical
single-key rewrite: comments, blank lines, spacing, key order and line
endings survive byte-for-byte; duplicates of the key all become the new
value since the parser is last-one-wins; same-line comments are kept; a
missing key is appended at the end of the `[slangvk]` section, a missing
section or conf is synthesized) and then pushed through `reread_conf()` —
the exact path a hand edit takes, so every consumer applies it live
(aspect/base_res snap on the spot, scale reads per frame, vsync at the next
swapchain recreate, window/borderless/controller on their own mtime
probes). The per-key steps mirror the consumers' own fixups: preset cycles
the scanned `*.slangp` tree (FindFirstFileA, case-insensitive-sorted,
`(none)` at the front — kept as the editor's fallback; the browser owns the
row today), scale/vsync/aspect/base_res/window step their token
lists, the toggles flip, controller_sens 0.25 (0.25-16), controller_aimsens
0.05 (0.05-4), controller_deadzone 5% (1-80). Every change toasts and logs
(`overlay: conf aspect = 16:9`); a list end, an empty preset dir or a
failed write touches nothing and logs why.

Two 0.4.5 refinements round the panel out. First, the **controller rows
follow the master switch**: with `controller=0` the sub-options (sens/
aimsens/deadzone/moveclick/home) are inert, so the panel skips their rows —
the `controller off` row itself stays (Left/Right re-enables it, which
re-lists the sub-options on the spot), keeping the panel honest about what
the arrows can actually change. Second, **Left/Right on the preset row opens the
preset browser**: the `*.slangp` list of the **current preset's folder
subtree**, rendered in the panel itself — the scan root follows `preset=`'s
dir component and the walk recurses to any depth (shader collections nest
deeper than two folders; a leaf-only listing would show one entry per
folder): `presets\crt.slangp` lists everything under `presets`, a
bare/empty/absolute `preset=` lists the game dir tree — and the rows are
stored exactly as `preset=` accepts them (`presets\xbr.slangp`,
`shaders\crt\royale\kurozumi.slangp`), so a pick resolves back to the very
file listed. Page shape: a `presets (N) - <dir>` header (the scan
root), `(none)` at the front, the conf's
current preset starred (`* `), up to 16 rows on screen with a scroll window
that follows the cursor (the page builder, `g3x_slang_browser_page`, is a
portable host-tested seam). Up/Down move the green cursor, Right loads the
highlighted preset through the same write-conf + hot-reload path (a real
change rebuilds the chain live; re-picking the active preset is a no-op
rewrite that still confirms), Left goes back to the option page, Shift+V
hides everything and resets the browser. The tree is scanned fresh at every
open; an empty tree toasts `no .slangp under <folder>` instead of opening an
empty page. Same focus rule and edge latches as every panel key. The
browser is deliberately **Enter/Esc-free**: Enter reaches the game's chat
box and Esc reaches its menu, and a GetAsyncKeyState poll cannot swallow
either — so Left/Right do double duty (they open the browser from the
preset row, then become its load/back buttons while it is up).

## The preset chain (P3, 0.4.0)

Every present, the gamma-corrected frame is uploaded at its **native game
resolution** into a `slangvk_image`, and `slangvk_chain_render_swapchain`
draws the preset chain straight into the acquired swapchain image — so the
preset's own scaling algorithms see the true source size (640x480 in,
whatever the window is out). The version-tag overlay is drawn onto the chain
input, so it filters along with the frame (screenshots show what the player
sees). The scaling/vsync/overlay relationship:

- `scale=fit` — the chain fit-and-centers (letterbox). `scale=integer` — we
  compute the largest whole-number multiple of the game resolution that fits
  and hand the chain a centered rect. `scale=stretch` — full-rect.
- `vsync=` — swapchain present mode (FIFO default; `0` prefers IMMEDIATE).
- The overlay box anchors top-right exactly as in P2 (see above); with a
  preset on it warps/filters with everything else.

`d2slangvk.conf` beside `Game.exe` (copy it from
`presets/d2slangvk.conf.sample`):

```ini
[slangvk]
preset=presets/glide3xvk-scanline.slangp   ; or an absolute path into your
                                            ; RetroArch shaders (CRT-Royale)
scale=fit                                  ; fit | integer | stretch
vsync=                                     ; empty = FIFO, 0 = immediate
fallback=unfiltered                        ; preset failure -> raw frame
```

Behavior contract:

- **No conf / no `preset=` line = the chain never engages.** Identical pixels
  and logs to 0.3.x (the only log line is a one-shot "no preset" note).
- **Hot reload**: the conf and the preset file are stat'ed every 32 frames;
  any change reloads the preset and rebuilds the chain mid-session (a few
  frames of unfiltered present during the swap).
- **Fail-soft**: preset parse/compile/render failure logs the slang error and
  downgrades to the unfiltered path (the whole session after 3 consecutive
  render failures); the game never crashes on a bad preset.
- Starter presets (ours, MIT): `presets/glide3xvk-scanline.slangp` (the
  zero-risk smoke test — image identical apart from darkened alternate source
  lines) and `presets/glide3xvk-sharpen.slangp` (mild unsharp mask). Real CRT
  chains (CRT-Royale and friends) load from a RetroArch `shaders/` checkout
  by absolute or game-relative path.

## Performance (0.4.5)

Measured profile (full-screen 0.4.3 field log, RX 560, 800x600, crtglow_lanczos
7-pass chain active): **avg 51.6 ms/frame ≈ 19 fps** in-game. Per-frame draw
mix: 152 fans + 170 strips + 142 RGB565 quads (≈ 470 draw calls), **fill=0** —
the fullscreen dim-fill path is not a real-world cost. The budget is the CPU
raster + texture-table upkeep, with the chain's fullscreen GPU pass on top
(516 filtered frames, 0 fallbacks — the chain is not the bottleneck).

Perf round 1 (0.4.5), chosen off that profile:

- **SSE2 float math** — i686 MinGW defaults every float operation to x87
  (stack-based 80-bit, slow `fistp` int conversions, no SIMD, no
  auto-vectorization), and the raster is float-heavy. glide3x now builds with
  `-msse2 -mfpmath=sse` (target-scoped; the game tree keeps project policy).
- **O(1) texture table** — `g3x_frame_tex_source` linearly scanned up to 8192
  slots per bind and `tex_slot_alloc` rescanned from 0 per download; now an
  open-addressing hash plus a monotonic allocation cursor (preserves the exact
  first-free slot order) plus a same-texture rebind fast-out.
- **Swapchain `minImageCount+2`** — the field log showed images=2, the bare
  minimum; one extra image keeps FIFO present pacing from stalling the raster
  thread.

Zero pixel risk by construction: the agent-side `fxparity` harness replays a
220-frame deterministic mixed scene (all blend classes, chroma, palettes,
rebind storms, table-full stress) and hashes every present buffer — the
before/after hash and counters are identical, so the round cannot change what
you see, only how fast it gets there. Field numbers: capture `frame N:` lines
chain-on vs chain-off on the next round's build.

### The 25 fps ceiling (source-verified)

Second 0.4.5 field log closed the loop: in-game `avg=` reads a dead-on
40.0–40.3 ms (= 25.0 fps) while the Esc menu reads ~24 ms — with the same
per-frame draw volume. `avg=` is swap-to-swap wall time, and the engine
paces in-game frames itself: `CLIENT_GameLoopFrame`
(`D2Client/GAME/Game/Game.cpp`) gates sim + render on a 40 ms budget
(`uGlobalTickCountB = 0x28`, `fps_drawn = 25` hardcoded — the in-tree comment
says it outright: the SP gameserver runs as part of the drawing process,
"capped so hard at 25 frames"). The Esc menu early-returns and draws
unthrottled, so its 45 fps ≈ the wrapper's true per-frame cost today.

What this means: in-game fps is the engine's, not ours — the perf rounds
still mattered because 0.4.3's 51.6 ms frames overflowed the game's 40 ms
tick budget (the game's own frame-skip logic then drops ticks — the choppiness
source); 0.4.5's ~22 ms fits. The `work=` field in the `frame N:` line (this
build) reports the wrapper's present-path cost so the log separates engine
pacing from wrapper cost. 60+ fps smoothness in-game is an engine-level
project (d2dx-style motion interpolation on a rebuilt exe) — out of scope
here; the wrapper targets: stay far under the 40 ms budget on the heaviest
scenes, menus/cutscenes at full speed.

## Build (same toolchain as the game)

From the repository root, with a **libslang-vk checkout beside the repo**
(the P3 preset chain links its static `slangvk` target; see `BUILD-32BIT.md`
for the i686 verification of that build):

```sh
git clone https://github.com/VTSTech/libslang-vk ../libslang-vk   # once
```

Without the checkout the DLL still builds — with the chain stubbed out
(same API, always inactive) and a loud CMake STATUS line saying so.

```sh
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-i686.cmake
cmake --build build --target glide3x
# → build/glide3x.dll (the root project builds it from glide3xvk/ when present;
#   a full `cmake --build build` also lands it beside d2.exe automatically)
```

CI (`.github/workflows/build.yml`) does exactly this on every push and uploads
the DLL as the `glide3x-dll` artifact; it checks out `VTSTech/libslang-vk`
pinned (`ref: d208ac5`) so the chain builds reproducibly.

Verify the export surface against the game's own loader (must be an empty diff):

```sh
i686-w64-mingw32-objdump -p build/glide3x.dll \
  | sed -n '/Ordinal\/Name Pointer/,$p' \
  | grep -oE '_[A-Za-z]+@[0-9]+' | sort -u > /tmp/ours.txt
grep -oE '"_[A-Za-z]+@[0-9]+"' D2Client/Renderer/Glide.cpp | tr -d '"' | sort -u > /tmp/game.txt
diff /tmp/game.txt /tmp/ours.txt && echo "MATCH: all 45 GetProcAddress names covered"
```

> Toolchain note: GNU ld strips one leading `_` from every export name and
> searches def-file internals with an extra one. The scaffold defeats both:
> functions carry `__asm__("__name@N")` labels (double underscore → table gets
> single), `glide3x.def` lists the literal names, and `-Wl,-u` flags force the
> object archive member in for the def lookups. See the CMakeLists comment.

The root `CMakeLists.txt` picks this directory up automatically and keeps it
out of the `d2` glob — delete the directory and the game builds exactly as
before (zero game-source drift).

The DLL is self-contained (`-static-libgcc -static-libstdc++`, volk loaded at
runtime): KERNEL32 + msvcrt imports only. The build id embedded in the log
banner, `grGetString(GR_RENDERER)` and the overlay comes from
`git describe --always --dirty --tags` at compile time — build from a clean
checkout of the pushed branch and the tag names a commit you can find on
GitHub.

## Test procedure (field)

1. Copy `build/glide3x.dll` into the game root, next to the game exe.
2. Launch the game with `-3dfx` — that selects the Glide renderer
   (command line arg or `[VIDEO] 3DFX=1` in the ini). Works on a
   **vanilla retail 1.14d install** (`Game.exe -w -3dfx` windowed on the field
   rig). Full per-round walkthrough and history: `FIELD-TEST.md`.
3. A healthy run looks like: the menu renders, play works, and
   `glide3x.log` beside the exe carries the session banner
   (`glide3xvk 0.4.5 <build-id>`), periodic `frame N:` summaries and a clean
   exit summary on shutdown.
4. The version tag in the top-right corner identifies the DLL in any
   screenshot; `/fps` in the game stays readable top-left.

## Fail-soft behavior

If 32-bit Vulkan is unavailable (`vulkan-1.dll` x86 missing — see R2), the
log says so, the DLL still loads, and the game keeps running (black window).
The DirectDraw path (`-w`) is untouched by all of this.

## What is deliberately NOT here yet

- GPU-side raster (the 60 fps endgame): the 0.4.3 full-screen log measured
  51.6 ms ≈ 19 fps @ 800x600 with the chain active; 0.4.5 took the first CPU
  side cut (see Performance above), and the compose-to-GPU move remains the
  main remaining perf item — present-path fence pipelining and span-raster
  templating are queued as CPU-side round 2 (PLAN.md §5 perf track).
- Bilinear texture filtering (look delta only): the ground requests
  `texFilterMode(1,1)` and is point-sampled today.
- Preset parameter UI / per-preset geometry defaults (P4): parameters are
  reachable through `slangvk_chain_set_parameter` but nothing drives them yet.
- Slang-FILTERED screenshots (0.4.6's PrintScreen captures the pre-shader
  game frame): the chain renders straight into the acquired swapchain image,
  so a filtered capture needs a Vulkan readback pipeline (copy the swapchain
  image to a host-visible buffer between chain render and present, or render
  the chain into an offscreen target first) — a field round of its own.
