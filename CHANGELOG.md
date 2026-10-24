# CHANGELOG

Notable changes to **glide3xvk** - the glide3x.dll Glide 3 -> Vulkan translation layer
for Diablo II 1.14d (vanilla retail, `-w -3dfx`).

Format follows Keep a Changelog. Where a field round already ran, entries cite what
glide3x.log showed on real hardware (RX 560, Vulkan 1.3, windowed). Commit refs are on
branch `libslang-vk` of VTSTech/D2LoD-libslang-vk.

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
