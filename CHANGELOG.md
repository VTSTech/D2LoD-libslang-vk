# CHANGELOG

Notable changes to **glide3xvk** - the glide3x.dll Glide 3 -> Vulkan translation layer
for Diablo II 1.14d (vanilla retail, `-w -3dfx`).

Format follows Keep a Changelog. Where a field round already ran, entries cite what
glide3x.log showed on real hardware (RX 560, Vulkan 1.3, windowed). Commit refs are on
branch `libslang-vk` of VTSTech/D2LoD-libslang-vk.

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

### Validated

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
