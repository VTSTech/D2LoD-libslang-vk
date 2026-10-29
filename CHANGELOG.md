# CHANGELOG

Notable changes to **glide3xvk** - the glide3x.dll Glide 3 -> Vulkan translation layer
for Diablo II 1.14d (vanilla retail, `-w -3dfx`).

Format follows Keep a Changelog. Where a field round already ran, entries cite what
glide3x.log showed on real hardware (RX 560, Vulkan 1.3, windowed). Commit refs are on
branch `libslang-vk` of VTSTech/D2LoD-libslang-vk.

## [0.4.6] - 2026-10-29

### Added

- **Scroll Lock overlay toggle** (g3x_frame + g3x_overlay + g3x_slang): press
  **Scroll Lock** in-game (game window focused) to hide or restore every
  overlay box at once — the version tag with its shader / hostver / FPS
  lines, the toast and the Shift+V conf panel — for clean screenshots and
  recordings without restarting the game or editing glide3xvk.ini. The poll
  rides the present seam (`g3x_frame_present`, `overlay_scrolllock_poll`) —
  the same always-on home as the conf-panel poll, so it works with or without
  a preset chain, foreground-gated and edge-latched like every polled hotkey
  (presses made in another window never toggle, and an unfocused press
  leaves the latch reset so alt-tabbing back never fires a stale flip). The
  flip itself is `g3x_overlay_toggle()`, a **session override** composed
  against the ini base in the overlay module: `grSstWinOpen` re-pushes
  `[glide3xvk] overlay=` on every context open and D2 opens contexts
  constantly (the 640x480 probes, the menu, the game), so a plain flip of the
  pushed flag would pop the boxes back on the next re-open — the override
  latches for the process lifetime while the ini still decides the state at
  startup and after a relaunch (file wins at load, key wins in flight, the
  same split as the F11/F12 hotkeys vs `d2slangvk.conf`). While hidden
  nothing draws (the render early-out is unchanged — the boxes vanishing and
  reappearing is the toggle's own feedback), the conf-panel poll parks with
  all its edge latches reset (a hidden panel must not keep editing
  `d2slangvk.conf` invisibly, and the arrow keys stay the game's), and a
  conf panel that was up comes back up when the overlay is toggled back on.
  The show press toasts `overlay on`; the hide press only logs (its toast
  would draw into a box that just died) — `overlay: on/off (Scroll Lock)`
  lands in glide3x.log either way. The OS-side Scroll Lock LED is untouched.

## [0.4.5] - 2026-10-25

### Added

- **Experimental Controller support** (g3x_controller): opt-in gamepad play via input
  injection — the wrapper polls pad 0 through XInput on its own background
  thread (~125 Hz, smooth regardless of the game's 25 Hz tick) and injects
  ordinary mouse/keyboard events (SendInput), so the game just sees a normal
  mouse and keyboard and every D2 screen works. No Game.exe patch, no driver,
  no new DLL imports (XInput loads dynamically: xinput1_4 -> xinput9_1_0 ->
  xinput1_3; none = logged once, feature stays off). Map v2 (field spec:
  left analog moves me, triggers click, d-pad belts, and hitting things NEAR
  me must not need pixel-perfect cursor work — in D2 movement IS the mouse):
  left stick = walk (steers the cursor and auto-holds left click while
  deflected — D2 walk-to-cursor; `controller_moveclick=0` for cursor-only),
  right stick = fine aim (same curve at `controller_aimsens` speed, default
  0.35, for placing the cursor on monsters/portals/item labels), LT = left
  click hold, RT = right click hold, A = hold Shift+LMB (D2 attack-in-place
  toward the cursor — swings/shoots at what is near it without moving, the
  forgiving way to fight with a stick), B = hold Alt (item labels),
  X = tap Space (close screens), Y = tap I (inventory), D-pad = belt 1-4,
  LB = C (character), RB = T (skill tree), L3 = tap R (run/walk),
  Start = Esc, Back = Tab (automap); R3 reserved. Binds are multi-step
  combos pressed in order and released in reverse, and the event model is
  desired-state diffing — sources OR together, so the walking LMB and the
  A combo's LMB can never double-press or steal each other's release.
  Taps are one clean keystroke (down this tick, up the next) so held menu
  keys cannot auto-repeat-flip a D2 screen closed. Injection is gated on
  the game window being foreground, and everything held is released on
  focus loss, disable, pad disconnect and shutdown (a still-held control
  stays silent until a fresh press — no surprise clicks after alt-tab).
  Conf keys (hot-reloaded like everything else, ~2s probe):
  `controller=0|1` (off by default), `controller_sens=` (left-stick speed
  multiplier, default 1.0, clamp 16), `controller_aimsens=` (right-stick
  fine-aim multiplier, default 0.35, clamp 0.05-4), `controller_deadzone=`
  (percent, default 15, clamp 1-80), `controller_moveclick=0` (opt out of
  stick-walk). Zero present-path cost — no grBufferSwap hook; lifecycle
  rides grSstWinOpen/grSstWinClose/detach. Pure core host-tested
  (scripts/ctrltest.cpp: 240/240 — conf fixups, bind-table shape, stick
  response, trigger edges, combo order, the stick-walk/A-combo interaction,
  tap walks, flood cap, release_all re-arm semantics).
- **Controller cursor-home** (field round 2 — the user's own design: "when
  no analog input, put the cursor on the player and hide it, then when the
  analog moves, it's from player position and when analog stops, it's back
  to player position"). The wrapper cannot see the player and does not need
  to: D2 walk-to-click always ends with the character standing AT the
  cursor, so the cursor position at every stick-walk release IS the player's
  screen position. That spot becomes the home anchor (self-correcting — a
  walk stopped short by an obstacle re-syncs on the next walk), and after a
  ~0.4 s quiet dwell (sticks centered, nothing injected, nothing held) the
  cursor is SetCursorPos'd back onto it; stick motion therefore always
  emanates from where the player is, and the cursor can no longer get lost
  at the screen edge. Real-mouse movement is detected and respected (it
  counts as activity and re-syncs the tracked cursor). Honest limits, in
  the README: the cursor is parked, not hidden (hiding D2's own cursor
  sprite would mean intercepting the game's WM_SETCURSOR — Game.exe
  territory, out of scope), and a click that walks the character somewhere
  (LT on a far portal) leaves home there until the next stick-walk.
  Requires stick-walk; `controller_home=0` restores a plain free cursor.
  Field round 3 (verdict on the shipped build): experimental — the anchor
  model helps but is not fully dialed in; docs now flag the module as such
  and the escape hatch is documented everywhere.

### Changed

- **Shift+V conf panel** (g3x_overlay + g3x_slang): press **Shift+V** in-game
  to toggle an overlay panel on the left edge (below the game's `/fps` line;
  the tag box keeps the top-right) listing every `d2slangvk.conf` option as
  currently resolved — preset (basename), scale, vsync, aspect and base_res
  at their live F11/F12 session values with a `file: ...` annotation when
  they differ from the conf, window, borderless, and the controller block
  (sens/aimsens/deadzone/moveclick/home — listed only while the controller
  master switch is on) with the same defaults their
  consumers apply (`g3x_ctrl_conf_fixup`, empty `window=` = native). The
  content is re-parsed from disk on every open — the file is the truth, and
  unlike the F11/F12 hotkeys the toggle polls on EVERY present
  (g3x_frame_present), so it works with the chain off, where the present-path
  hot-reload probe never runs. While up, the panel refreshes on conf
  hot-reloads and F11/F12 presses. Same focus rule as F11/F12 (presses made
  in another window never toggle), same fail-soft look (65%-black backdrop,
  8x8 font, drop shadow), honors the `[glide3xvk] overlay=` ini switch, and
  toasts + logs on toggle (`overlay: conf panel on/off (Shift+V)`).
- **Conf panel arrows: selection + value editing** (g3x_overlay +
  g3x_slang): while the panel is up, **Up/Down** move a green selection
  cursor through the option lines — the selected row draws as a dim-green
  band with bright-green glyphs (`#20FF20`-ish) against the white text, a
  menu cursor in the same hue D2's own UI uses for picked text. The
  `d2slangvk.conf` header line and the hint footer are not selectable; the
  cursor clamps at the ends (no wrap — on a 13-option list a wrap reads as a
  glitch); each move re-parses the conf (the file-is-truth rule holds for
  selection redraws too, and the panel picks up any conf edit that landed
  since the last refresh) and logs `overlay: conf panel sel <key>`.
  **Left/Right** change the selected option: one press = one step, written
  straight into `d2slangvk.conf` and pushed through the normal conf
  machinery, so it lands live exactly like a hand edit — aspect/base_res
  snap on the spot, scale reads per frame, vsync at the next swapchain
  (re)creation, window/borderless/controller on their own mtime probes.
  The write is surgical (g3x_slang_conf_rewrite_text, host-tested):
  comments, blank lines, spacing, key order and line endings survive
  byte-for-byte; only the edited key's assignment changes (every duplicate
  of it — the parser is last-one-wins); same-line comments are kept; a
  missing key is appended at the end of the `[slangvk]` section, a missing
  section or conf is synthesized. The steps mirror the consumers' own
  fixups, so what the arrows write is always a value the parser accepts:
  preset cycles the game dir's `*.slangp` files (sorted, `(none)` at the
  front; subdir/absolute presets stay file-edit-only), scale
  fit/stretch/integer, vsync auto/off/on, aspect auto/4:3/16:10/16:9 (the
  F11 order), base_res 1x-4x, window native/240p/480p/576p/720p/768p/
  1080p/desktop (the preset table; `WxH` stays file-edit-only),
  borderless/controller/moveclick/home toggle, controller_sens 0.25 steps
  (0.25-16), controller_aimsens 0.05 steps (0.05-4), controller_deadzone
  5% steps (1-80). Every change toasts (`aspect 16:9`) and logs
  (`overlay: conf aspect = 16:9`); a list end, an empty preset dir or a
  failed write touches nothing and logs why. Same focus rule and edge
  latches as every hotkey here; while the panel is hidden the arrow keys
  are untouched — the game keeps them. The footer hint reads
  `[Up/Down] select - [Left/Right] change - [Shift+V] hide` (on the preset
  row: `... [Left/Right] list ...`).
- **Conf panel: controller-off hiding + preset browser** (g3x_slang): with
  `controller=0` the controller sub-options are inert, so the panel skips
  their rows — the `controller off` row itself stays (Left/Right switches
  it back on, which re-lists the sub-options on the spot). And **Left/Right**
  on the preset row open the preset browser over the **current preset's
  folder subtree**: the scan root follows `preset=`'s dir component
  (`presets\crt.slangp` lists the `presets` tree; a bare/empty/absolute
  `preset=` lists the game dir tree) and the walk recurses to any depth —
  shader collections nest deeper than two folders — with the rows stored
  exactly as `preset=` accepts them (`presets\xbr.slangp`,
  `shaders\crt\royale\kurozumi.slangp` — a pick resolves back to the very
  file listed).
  Page shape: a `presets (N) - <dir>` header (the scan root), the
  conf's current preset starred (`* `), `(none)` at the front, up to 16 rows
  on screen with a scroll window that follows the cursor. **Up/Down** move
  the green cursor through it, **Right** loads the highlighted preset (the
  same surgical `preset=` write + conf hot-reload path as the value
  edits — a real change rebuilds the chain live, re-picking the same
  preset is a no-op rewrite that still confirms via toast), and **Left**
  goes back to the option page (Shift+V hides everything and resets the
  browser). The browser is deliberately Enter/Esc-free — Enter reaches the
  game's chat box and Esc its menu, and a GetAsyncKeyState poll cannot
  swallow either, so Left/Right do double duty (open from the preset row,
  then load/back while it is up). The tree is scanned fresh at every
  open; an empty tree toasts `no .slangp under <folder>` instead of opening
  an empty page. Logs `overlay: preset browser ...` lines to match.
- **Overlay: repo credit line** (g3x_overlay): the tag box ends with a
  permanent bottom line — `GitHub: VTSTech/D2LoD-libslang-vk` — the one
  overlay line that never clears (shader line) and never expires (toast),
  so every screenshot the overlay stamps also names the project's home.
  The conf panel draws before the tag box, so the stamp wins any overlap on
  tiny present targets.
- **Overlay: the running game exe + its patch version** (g3x_hostver +
  g3x_overlay + glide_exports): the tag box gains a line directly under the
  build stamp naming the host — `d2.exe 1.14d` — detected once per process at
  the first `grSstWinOpen` and fail-soft end to end. The primary identifier
  is a zlib-compatible CRC-32 of the exe file on disk (chunked read through
  a static buffer): a hit in the known-build table names the exact binary
  outright and outranks the resource tiers (`Game.exe Retail 1.14d` — the
  bytes are ground truth), an unknown one is the raw-hex fallback
  (`d2.exe [CRC 3A5D2B1F]`) for builds with no version resource (this
  repo's own CMake-built d2.exe) or just the exe name. Tier 2 reads the
  exe's VERSIONINFO resource and maps the Blizzard-stamped `dwFileVersion`
  pair to the community patch names via d2dx's field-tested table
  (`1.0.9.22`=1.09d, `1.0.10.9`=1.10, `1.0.12.49`=1.12, `1.0.13.60`=1.13c,
  `1.0.13.64`=1.13d, `1.14.3.68`=1.14b, `1.14.3.71`=1.14d); an unmapped
  pair shows the dotted value (`Game.exe v1.0.13.55`). Both tables grow
  from the field: the raw evidence is always logged (`hostver: ... (ver=
  ms= ls= crc= bytes=)`), so an unmapped build can be named later by
  quoting its log line. New module `g3x_hostver` (pure tables/CRC/format
  host-tested in scripts/test_hostver.cpp, 47 checks under ASan+UBSan;
  Win32 pass fakecheck type-checked); glide3x links the system version
  import lib. No game-side changes — the DLL stays strictly read-only
  toward Game.exe.
- **Overlay: live FPS in the tag line** (g3x_overlay + g3x_frame): the
  top-right box's first line reads `glide3xvk 0.4.5 (<hash>) FPS 025` — a
  per-second presented-frame count, so the in-game `/fps` readout is no
  longer needed for a quick frame-pace sanity check. `g3x_frame_present`
  ticks the meter once per presented frame (both present paths funnel
  through it: `grBufferSwap` for gameplay and `grLfbUnlock` for the Bink
  video loop — the video loop never calls grBufferSwap), and a 1-second
  window of ticks becomes the displayed value: `FPS 000` until the first
  full second completes, then the half-rounded rate over the window's
  actual elapsed time, clamped at 999. The clock reading (GetTickCount64)
  is taken at the call site and handed in as a `uint64_t` millisecond
  argument — the meter is pure, so the whole window math is host-tested
  with synthetic timestamps (scripts/test_fps_tag.cpp, 18 checks under
  ASan+UBSan: rolls, rounding, clamp, no per-frame recompose — the tag
  text is rebuilt only when the value changes, one snprintf per second of
  play — non-monotonic clock tolerance, 3000-window drift check, tiny
  present targets with the wider line). The string
  `grGetString(GR_RENDERER)` reports stays the plain static tag — the FPS
  field lives only in the box, so external renderer queries keep a stable,
  comparable name; the tag box grew 8 characters wide at most (the
  ` FPS nnn` suffix), still well under the credit line's width on any
  normal present target.

### Fixed

- **Flicker investigation closed by the config-only switch (field
  confirmation):** the "constantly resizing the frame" turbulence reported
  on the F10 builds (b6c5bce log: 20 applies = 20 clean one-shot swap
  recreates, no storms — the wrapper's per-frame path was never the
  culprit) rode on the live hotkey apply cycle itself (SetWindowPos +
  same-frame swapchain recreate + chain retarget + toast at the menu's
  cadence). Field-verified after the F10 removal: resizing via `window=` in
  the conf — applied at renderer open and on the quiet ~32-frame mtime
  hot-reload — changes the running size with **no flicker**. Config-only
  presets are therefore also the flicker fix; any future live-resize
  feature must solve the apply-cycle turbulence first.
- Conf-panel/browser toasts were invisible while no preset was loaded: the
  overlay's optional toast line required the active-shader line to be up
  (`three = two && s_toast_on`), so e.g. the browser's `no .slangp ...`
  toast never drew in exactly the situation that produced it — the field
  report of "never seeing" the empty-folder toast. The toast now draws
  whether or not a shader line is up; the tag box's credit line stays the
  bottom row either way.
- The 8x8 overlay font's 'm' rendered as a solid block: DejaVu Sans Mono
  Bold's triple-stem 'm' collapses under the 4x-box-filter at 8px, and no
  overlay string until now contained a lowercase 'm'. The Shift+V panel's
  controller keys (`controller_aimsens` / `controller_moveclick` /
  `controller_home`) exposed it. `scripts/gen_font8x8.py` now hand-draws 'm'
  alongside the g/p/q/j descender overrides (1px stems 3 columns apart, 1px
  top bar, x-height body rows 2-7, in the style of the generated 'n'); the
  header was regenerated — every other glyph byte-identical.

## [0.4.4] - 2026-10-25 (CPU raster perf round 1 — x87 off the hot path)

Field trigger: the 0.4.3 full-screen field log (round-17 build, crtglow_lanczos
7-pass chain) showed avg=51.6 ms ≈ 19 fps in-game @ 800x600 — the frame line
mix (152 fans + 170 strips + 142 RGB565 quads per frame, fill=0) pointed at
CPU raster + texture-table upkeep as the budget, not the chain or the game.

### Changed

- **SSE2 float math for the DLL** (CMakeLists): the i686 MinGW default is x87
  for all float math — stack-based 80-bit, slow `fistp` int conversions, no
  SIMD, no auto-vectorization — and the raster is float-heavy. glide3x now
  builds with `-msse2 -mfpmath=sse` (target-scoped; the game tree and the
  libslang-vk subbuild keep their own flags). Expected 1.5-3x on the
  fill_span/raster_tri inner loops. Pixel-risk class: the already-accepted
  "one texel at exact float boundaries" shift (SSE is IEEE single and MORE
  deterministic than x87's 80-bit intermediates, not less).
- **Texture table O(1)** (g3x_frame): `g3x_frame_tex_source` was a linear
  scan over up to 8192 slots per bind and `tex_slot_alloc` rescanned from 0
  per download. Now a key -> slot+1 open-addressing hash (16K entries,
  insert-only — slots are never freed) plus a monotonic allocation cursor
  that preserves the exact first-free slot order, plus a rebind fast-out for
  consecutive binds of the same texture. Bit-identical by construction
  (fxtest-parity harness: same 220-frame mixed-scene raster hash).
- **Swapchain headroom** (vk_backend): swapchain images = minImageCount+2
  (was +1) — the field log showed images=2, the bare minimum; one extra
  image keeps FIFO present pacing from stalling the raster thread.
- **Log hard cap, 1 MB** (g3x_log): glide3x.log rotated to `.1` only at
  open time before, so one long session grew the file unbounded (the
  state-line + summary traffic makes ~72 KB in a 30-second round). Now the
  size is tracked per written line and the log rotates to
  `glide3x.log.1` mid-session the moment it crosses 1 MB — a rotation
  marker banner separates the halves, worst case on disk is ~2 MB
  (log + .1), and the P2.4 append-across-sessions behavior is unchanged.
- **Field result (this round's log, 0.4.4 build 7a2e132):** in-game
  avg=39.0 ms ≈ 26 fps (was 51.6 ms ≈ 19 fps on 0.4.3), menu/light scenes
  avg=24.6 ms ≈ 40 fps; swapchain images=3 confirmed in the banner. Same
  scene is heavier this round (~800 draw calls/frame vs ~470), so the
  fps gain understates the raster speedup.
- **25 fps in-game is the engine, source-verified:** a second field log
  (build be8737e) shows in-game swap-to-swap avg dead-on 40.0-40.3 ms with
  the same per-frame draw volume as 24 ms Esc-menu windows;
  `CLIENT_GameLoopFrame` (D2Client/GAME/Game.cpp) gates sim+render on a
  40 ms budget and hardcodes fps_drawn=25, while the Esc menu early-returns
  and draws unthrottled (45 fps = the wrapper's true speed). `frame N:`
  lines now also carry **work=** — time spent inside the swap (gamma,
  compose upload, chain render, submit, present) — so the log separates
  engine pacing from wrapper cost from now on.
- **Field result (build 10da725 — first log carrying work=):** at 100%
  desktop scaling the window reports a true 1920x1080 (125% Windows scaling
  DPI-virtualizes it to 1536x864 = 1080p/1.25 — the earlier "windowed
  scale" lines, explained). Timeline: logo videos avg=87.0 ms (movie-paced
  LFB path), main menu avg=40.8 ms = the D2Win MessagePump 40 ms budget
  (L3, dead-on), first in-game window avg=45.6 ms (tick gate + first-load
  uploads), paused/Esc windows avg=23.9-26.5 ms unthrottled (510
  strips/frame, same scene as the be8737e log). work= puts the wrapper's
  present-path cost at 12.1-18.5 ms: ~31% of a gated gameplay frame, ~51%
  of a paused one; the 1080p chain costs about what the 1536x864 chain did
  (42 vs 45 fps paused for +55% pixels).
- **Window size presets + DPI awareness** (g3x_window): `window=` and
  `borderless=` join d2slangvk.conf — `native` (default: the game's own
  window, nothing touched), `240p`=320x240, `480p`=640x480, `576p`=720x576,
  `720p`=1280x720, `768p`=1366x768, `1080p`=1920x1080, `desktop` (monitor-
  sized, borderless) or an explicit `WxH`. **Config-only** — no hotkey (the
  game owns F10 for its skill slots; the wrapper doesn't steal game keys):
  presets apply at every renderer (re)open and a conf edit hot-reloads live
  (file is the source of truth, F11/F12 semantics; toast + log on apply,
  works with the chain off). The resize is DPI-aware:
  `SetThreadDpiAwarenessContext(PER_MONITOR_AWARE_V2)` loaded dynamically
  (pre-1607 Windows = the old best-effort virtualized behavior), because a
  DPI-unaware game otherwise sizes in virtualized coordinates — the field
  log's 1536x864 = 1080p/1.25 receipt. This is output sizing only: the game
  keeps rendering 800x600 (L4, SCOPE.md), so the unfiltered present path
  also learned to fit + center (below) instead of stretching.
- **Legacy present fits + centers** (g3x_frame): `gamma_scale_copy` stretched
  the raster to whatever the window was — 800x600 distorted to 16:9 on every
  16:9 window. Mismatched targets now get the largest centered source-ratio
  rect with black bars (same geometry as the chain's `scale=fit`), so
  chain-less users at 720p/1080p window sizes get correct aspect, and the
  LFB/Bink video path inherits it through the same seam. 1:1 fast path
  untouched; bars are a few hundred KB of zero fill per frame.
- **CI artifact names carry the commit hash + helper scripts**
  (.github/workflows/build.yml): the upload is now `glide3x-dll-<shortsha>` — the same 7-char
  `git rev-parse --short=7` form the DLL's log banner uses, so a downloaded
  artifact self-identifies against the in-game banner. PR runs name by head
  sha (github.sha there is the preview merge commit, which never exists in
  the branch history); the DLL inside stays `glide3x.dll`, the drop-in name
  the game loads. The artifact now also ships the `scripts/*.cmd` helpers
  (`clear_glide3x.log.cmd`, `install_shaders.cmd`, `install_git.cmd`) flat
  at the zip root beside the DLL — unzip-all into the game folder is a
  complete setup (gen_font8x8.py stays repo-side; it is a font-header dev
  tool, not a player-facing helper).

### Removed

- **The drivertag patch** (g3x_drivertag.cpp/.h, glide_exports install site):
  the 0.4.1-0.4.3 PE patch that repointed the game's renderer-name string so
  the in-game `/fps` box would read `Video: glide3xvk ...` instead of
  `Video: Glide` never actually showed on field builds (the stock line won
  either way), and field feedback is to leave the game's own line alone.
  Removed entirely: no Game.exe byte is touched anymore — the overlay box
  (now the primary stamp, on by default; `overlay=0` hides it) carries the
  version. SCOPE.md Tier A is now a hard "the DLL stays read-only toward
  Game.exe".

### Fixed

- **Window presets now retarget the present on the same frame** (g3x_window +
  vk_backend): a preset resize used to leave the picture at the OLD swapchain
  extent — the field receipt was a top-left crop ("smaller portion of the
  original scale screen"). Root cause of the miss: the recreate relied on the
  driver raising OUT_OF_DATE, which is flaky on windows we resize from a
  per-monitor-v2 context while the process is DPI-unaware. g3x_window now
  sets an explicit refresh request (g3x_vk_request_swapchain_refresh) and
  acquire_image consumes it BEFORE the acquire: fresh caps, swapchain
  recreate, g3x_slang_swapchain_resized retarget, all ahead of the same
  frame's present (g3x_window_frame_tick runs at the top of grBufferSwap).
  Applies to conf edits and revert-to-native alike.
- **A failed swapchain recreate is no longer a permanent present death**
  (vk_backend): one failed recreate (e.g. a minimized window's 0x0 surface
  extent, or a mode change in flight) used to leave the backend dead until
  the next grSstWinOpen — acquire_image returned early forever. acquire_image
  now retries the recreate every frame until it succeeds, and resize bails
  early (keeping the old swapchain) on a 0x0 currentExtent instead of tearing
  down into it.
- **Window preset apply compiled with the wrong `kind` type**
  (g3x_window.cpp): `apply_now()` declared `kind` as `unsigned` while
  `g3x_window_preset_size()` takes `int*` — a hard compile error, first
  seen as the 0.4.4 CI run failing at g3x_window.cpp.obj (invalid
  conversion from `unsigned int*` to `int*`, -fpermissive not an option
  here). One-line type fix; downstream uses are only `== 0` / `== 2`
  compares, so no behavior change. Full rebuild + link verified locally
  (45/45 Glide exports match D2Client/Renderer/Glide.cpp).

### Validated

- Host window-preset test (scripts/wintest.cpp, agent-side): the pure
  g3x_window.h core — table shape, case-insensitive name lookup, every
  canonical size, `WxH` forms, strict rejection (zero sides, >8192,
  non-digit, separators/space forms), failure zeroes its outputs (40/40
  checks; the F10 cycle walk retired with the hotkey).
- Host log-cap test (agent-side): mid-session rotation at exactly cap + one
  line, repeated rotation keeps log + `.1` ~= 2 MB for a 3 MB spam run,
  open-time rotation of an oversized file, and the P2.4
  append-across-sessions behavior unchanged (15/15 checks).
- Host parity harness (scripts/fxparity.cpp, agent-side): 220-frame mixed
  scene — ground strips, sprite fans (uniform/varying vc, 565 textures),
  chroma toggles, 5 blend classes, full-screen fills, points/lines, gamma
  swings, palette epoch bumps, rebind storms, 8600-key table-full stress —
  produces the identical present-buffer FNV hash and counters before/after.
  
## [0.4.3] - 2026-10-24 (F11/F12 hotkeys + the full d2slangvk.conf reference)

The user asked for Chocolate-Doom-VK's hotkey scheme here (the other
libslang-vk reference host cycles content aspect on F11 and shader base
resolution on F12) and for every d2slangvk.conf value to be documented.

### Added

- **F11 - content aspect, live**: cycles `4:3 -> 16:10 -> 16:9 -> auto`. A
  fixed ratio letterboxes the chain output into the largest centered rect of
  that ratio inside the swapchain (`g3x_slang_aspect_rect`, host-tested);
  `auto` hands the geometry back to `scale=`. Only intercepts while a chain
  filters frames and the game window has focus (GetAsyncKeyState is
  process-global; without the focus guard the keys would fire while alt-tabbed).
- **F12 - shader base resolution, live**: cycles `1x -> 2x -> 3x -> 4x -> 1x`.
  N > 1 uploads the game frame as usual, then one linear vkCmdBlitImage in a
  fenced one-shot submit (`g3x_vk_slang_blit_scaled` in vk_backend; layouts
  round-trip SHADER_READ_ONLY -> TRANSFER and back in the same command buffer)
  stretches it into a game-res-x-N slangvk_image, so every source-relative
  pass FBO re-derives at N-x - scanline pitch, glow and blur radii get finer
  with crtglow_lanczos and friends. Fail-soft: image_create or blit failure
  logs once and presents 1x.
- **Toast feedback**: each press draws a transient third overlay line
  (`aspect 4:3`, `shader base 2x`) for ~120 presented frames, then the box
  shrinks back. Every change is logged (`slang: F11/F12 ...`).
- **Conf keys for both**: `aspect=auto|4:3|16:10|16:9` and `base_res=1x..4x`
  (bare `1`..`4` accepted) in the `[slangvk]` section. Session hotkey picks
  are never written back; a conf hot-reload snaps them to the file's values.

### Documentation

- **d2slangvk.conf reference** (README.md): every key - `preset=`, `scale=`,
  `vsync=`, `aspect=`, `base_res=` - with defaults, accepted values, path
  resolution, reload semantics and error behavior, plus the hotkey table.
- Sample conf (`presets/d2slangvk.conf.sample`) now documents all five keys
  and drops the never-implemented `fallback=` line the 0.4.0 sample carried
  (unknown keys were always ignored; fail-soft to unfiltered is the only
  fallback mode).

### Verification

- fxtest 134/134 (42 new checks: aspect rect math incl. portrait and
  degenerate targets, conf aspect/base_res parse with bad-value and
  later-section-wins handling, toast grow/decay/shrink on a fresh page).
- mingw-i686 build links; exports unchanged 45/45 (def untouched);
  8,684,837 bytes. Field round 17 checklist in glide3xvk/FIELD-TEST.md.

## [0.4.2] - 2026-10-24 (overlay: descenders + active-shader line)

Field round 15 found the fallback box's 8x8 font crushing descenders ("can't
see the bottom of the g in glide"), and an active preset was invisible in
screenshots.

### Fixed

- **Descender glyphs** (`g p q j`): DejaVu's descent does not fit the 8-row
  tile under the generator's baseline, so the box filter crushed every tail
  into one broken row (the `g` read as chopped). The generator now carries
  hand-tuned overrides - bowls at rows 2-5, real tails on rows 6-7, the
  classic 8x8-font answer. 'y' was already acceptable and stays generated.

### Added

- **Second overlay line: the active preset's file name** (basename only, no
  path). `g3x_slang` pushes it when a chain goes ready (first open, conf
  change, hot reload) and clears it whenever the chain dies - `destroy_chain()`
  is the single choke point (reload start, `preset_load`/`chain_create`
  failure, shutdown). With no preset the line is absent and the box shrinks
  back to the tag alone (the clean P2 baseline look). 10px line step keeps
  line 1's descenders clear of line 2.
- The font generator ships in-tree at `scripts/gen_font8x8.py` - the header
  always promised it; the repo never carried it.

### Verification

- Host overlay harness: shader line painted on rows 17..24, `g`-descender ink
  on rows 14/15 (plus shadow), clear -> single-line shrink, long-path basename
  handling; mingw-i686 build links, 45/45 export surface unchanged.

## [0.4.1] - 2026-10-24 (181780c)

The version stamp moves onto the game's own "Video:" debug line; the P2.10 overlay box
retires to fallback status.

### Added

- **g3x_drivertag** (`g3x_drivertag.{h,cpp}`): claims the game's own debug line instead of
  competing with it. The `/fps` block ends with the game drawing `Video: <renderer>` at
  fixed game coords (500,2) from a hardcoded renderer name (decompiled `D2Client`
  `DRAW_UI_FPSStats`: `case RENDERER_Glide: szRendererName = "Glide";`), and that string
  never crosses the glide ABI - so the claim is a runtime patch inside glide3x.dll, done
  once at `grSstWinOpen`: PE-parse the mapped Game.exe image, locate the renderer-name
  string pool via the `"Software\0DirectDraw\0Glide"` / `"DirectDraw\0Glide"` anchors
  (exactly one hit or bail), find the single 4-byte VA reference to the `"Glide"` string
  (executable sections first, data sections as fallback), flip the page writable, store
  our static tag VA, restore protection, `FlushInstructionCache`. The game then draws
  `Video: glide3xvk <ver> (<build>)` in its own font at its own spot. Total invasion:
  one 4-byte store. Scanning mapped memory (not the file) means relocations are already
  applied, so found VA and replacement VA are both runtime VAs.
- **Fail-soft contract**: every step is guarded; any surprise (PE header unreadable, pool
  missing or ambiguous, no unique reference, `VirtualProtect` failure) skips the patch,
  logs the reason to glide3x.log and leaves the game untouched. `install()` is idempotent
  per session; the static tag buffer lives for the whole session (the game loads
  glide3x.dll and never frees it).
- **G3XVK_BUILD_SHORT** build id (7-char hash + `-dirty`): the game `sprintf()`s the tag
  into a `char[32]` stack buffer, and the long describe form (40 chars) would truncate
  mid-hash. The short form's worst case is 31 chars - an exact fit, proven both clean and
  dirty. The same composed line feeds the game's "Video:" line and the overlay tagline
  (`g3x_overlay_tagline()`), so the two stamps can never disagree.
- **FIELD-TEST.md Round 15**: field checklist for the drivertag. Log verdicts
  `drivertag=patched` and `drivertag=miss` are both safe outcomes by design.

### Changed

- **Overlay box demoted to fallback.** New semantics for `glide3xvk.ini [glide3xvk]
  overlay`: `overlay=0` forces the box off, `overlay=1` forces it on (debugging /
  preference), and with no key the value is auto - the box draws only when the drivertag
  patch did not land. Still re-read per context open, so a relaunch picks up edits.
- `grGetString(GR_RENDERER)` and the log banner keep the long describe id - their 64-byte
  budgets allow it; only the game-side tag uses the short form.
- Version bumped to 0.4.1; CMake builds the new sources. Export surface unchanged
  (45/45 exports diff-clean against 0.4.0; DLL 8,676,239 bytes).

### Field evidence (glide3x.log, 2026-10-24 14:19)

- Binary stamps `glide3xvk 0.4.1 (d76cf3c-dirty)` in the overlay tagline - the short-id
  composition renders correctly in the field, no truncation.
- `drivertag=miss` on this 1.14d Game.exe (`renderer-name pool not found (0 hits)`) - the
  fail-soft path engaged exactly as designed: box overlay stayed on, session unaffected.
  884 frames from menu into gameplay, 0 texmiss, the slang chain filtered every presented
  frame (0 fallbacks), gamma 1.55 applied in present.
- The next field round decides between the two remaining outcomes: pool anchors match this
  Game.exe layout (`patched`), or the reference lives in a table the scanner does not
  cover yet (`miss`). Both are logged; neither can hurt the session.

## [0.4.0] - 2026-10-24 (ca8d5bd)

P3: libslang-vk preset chain at the present pass.

### Added

- **Preset chain at the present pass**: `d2slangvk.conf` `[slangvk] preset=<path.slangp>`
  beside the game exe runs every presented frame through a libslang-vk preset.
  `scale=fit|integer|stretch`, `vsync=`, hot-reload on file change. No conf = the
  unfiltered P2 look. First field preset: `res-independent-scanlines.slangp`
  (1 pass, scale=fit, output 1536x864).
- **g3x_slang** (`g3x_slang.{h,cpp}`): slang runtime host - Vulkan device adoption,
  pass-chain build, per-frame input image upload, hot reload - integrated with
  vk_backend's compose/present path.

### Field evidence (first green run)

- Built and runs on the RX 560: menu into gameplay at 800x600 -> window 1536x864, FIFO
  vsync, roughly 28-31 fps with the scanline preset active (frame 600: avg 35.3 ms),
  zero preset fallbacks across the session.

## [0.3.1] - 2026-10-24 (4a329fa, tag)

P2.11: glide3x raster core complete.

### Added

- P2.10/P2.11 top-right version-tag overlay box: 8x8 bitmap font, 65% backdrop,
  post-gamma draw, `[glide3xvk] overlay=0/1` switch; version stamp from compile-time
  `git describe` shown in overlay, log banner and `grGetString`.
- CI (`build.yml`): mingw-w64-i686 build on every push, `glide3x-dll` artifact upload.
- P2.9 chroma index-0 fix (round-11 field evidence: black rects around in-game sprites -
  the DC6 transparent index now counts as a chroma hit), glyph/UI-bit texture class
  diagnostics, texTable high-water-mark tracking, per-activity logging and the frame
  120/240/300 summary lines.

## Earlier

- **P1..P2.x** (9292189..fb2d9d6): the core glide3x -> Vulkan backend - swapchain and
  present, texture manager, Glide state machine (texture combines, alpha blend, chroma,
  dither, fog), draw paths (vertex-cache, quad, fan, parr strips, point, line, fill),
  the grLfbLock/grLfbUnlock front-buffer present path, gamma applied in present,
  palette downloads, `grGetString`/GU wrappers, Voodoo-class caps profile
  (BOARDS=1 TMU=1 FB=1 MAX_TEX=256 UMA=0).
- **Redivivus** (4b0154c and earlier): Diablo II 1.14d recovered as source that builds -
  the reference tree (`D2Client`, `D2Glide`, ...) used to pin renderer behavior without
  touching the game.
