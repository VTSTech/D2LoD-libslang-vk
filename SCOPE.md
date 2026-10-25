# SCOPE.md — what glide3xvk can do alone, and what needs Game.exe

> Status: v1, 2026-10-25. Owner: VTSTech. Every claim below carries a receipt:
> either a field log (rounds 1–18, RX 560, vanilla retail 1.14d, see
> `glide3xvk/FIELD-TEST.md`) or a line in this fork's decompiled game source.
> The short version: **the wrapper is done rising — it now sits at the
> engine's own ceiling.** Everything past that ceiling is a Game.exe project,
> and unlike every prior art project (nGlide, d2dx, SGD2FreeRes), we hold the
> recovered source — so "Game.exe modification" here means editing `D2Client/`
> and rebuilding, not hex-hacking an unknown binary.

---

## 0. Why there is a hard line at all

The game's Glide loader (`D2Client/Renderer/Glide.cpp`) does
`LoadLibraryA("glide3x.dll")` and resolves **45 stdcall exports**. From the
game's point of view the DLL is a Voodoo card: it receives finished draw
primitives and buffer swaps, and answers a few capability queries. It does
NOT see (and cannot change):

- **when** frames happen (the engine's main loop paces them),
- **how big** they are (the game creates the window and picks the back-buffer
  resolution),
- **what the simulation does** (logic ticks are the game's business).

Everything the wrapper "is" lives inside that contract — which is exactly why
it runs on a vanilla retail install with zero game changes (the deployment
model nGlide and d2dx use).

---

## 1. Achieved with glide3x.dll alone (all field-evidenced)

| Capability | When | Receipt |
|---|---|---|
| Full Glide 3 → Vulkan translation: paletted/indexed textures, combine/alpha/chroma state machines, fan/strip/point raster, LFB, gamma, Bink cutscenes through the same path | P2.1–P2.11 (0.3.x) | round-12 field verdict: "Everything actually looks correct"; 45/45 export surface |
| libretro slang preset chain at the present pass: `.slangp` via libslang-vk, hot-reload, fit/integer/stretch, aspect + base-res keys, F11/F12 hotkeys, fail-soft fallback | P3 (0.4.0–0.4.3) | full-screen field log: crtglow_lanczos 7-pass chain, 516 filtered frames, 0 fallbacks |
| Version identity in any screenshot: top-right overlay + `grGetString(GR_RENDERER)` + the game's own `Video:` line (one 4-byte runtime pointer patch, fail-soft — no file touched) | 0.3.0–0.4.1 | `drivertag:` log lines every session |
| Performance round 1: SSE2 float math (x87 off the hot path), O(1) texture table, swapchain `minImageCount+2` | 0.4.4 | in-game swap-to-swap 51.6 ms → 39 ms (19 → 26 fps) on a ~30% HEAVIER scene; Esc menu 45 fps = the wrapper's true per-frame speed (~22 ms); bit-identical present-buffer hash before/after (fxparity harness) |
| Diagnostics: `frame N:` lines with `avg=` (engine pacing) + `work=` (wrapper present-path cost), draw-mix counters, 1 MB rotating log | P2.5–0.4.4 | every field round parsed from these lines |
| Present-window presets + DPI awareness: `window=`/`borderless=` conf keys sizing the OS window (canonical 240p..1080p, `desktop` borderless, `WxH`; config-only — no hotkey, F10 stays the game's skill-slot bind), `SetThreadDpiAwarenessContext` apply — output sizing only, the game still renders 800x600 | 0.4.4 | `g3x_window.{h,cpp}`; wintest 40/40; the 1536x864 = 1080p/1.25 DPI receipt in the 0.4.4 field log |
| Fail-soft everywhere: no Vulkan → unfiltered present; bad preset → downgrade; DLL still self-contained (KERNEL32+msvcrt imports only) | since P1 | R2/R4 risk register, PLAN.md §6 |
| CI: every push builds the DLL from a clean checkout and uploads it as an artifact | P2.11 | `.github/workflows/build.yml` |

**Where the wrapper now stands:** the game's in-game loop hands frames over at
25 Hz; the wrapper renders each of them in ~22 ms and presents. The engine
never waits on us, and its own frame-skip logic never triggers. That is the
ceiling of this seam.

---

## 2. Still achievable inside the wrapper (no Game.exe involvement)

Ordered by expected value:

1. **GPU-side raster** — the last big wrapper item. Submit the quads/strips
   as real Vulkan triangles (pipeline variants per combine state, palettes as
   lookup textures) instead of CPU spans. All semantics are discovered and
   field-proven (s/t spaces P2.7, chroma index-0 P2.9); this is re-expression,
   not research. Buys: Esc menu 45 → 60 (FIFO cap), heavy-scene headroom,
   lower present latency.
2. **Present-path round 2**: 2-frames-in-flight fence ring (today every swap
   fences serially), `fill_span` templating (straight-line inner loops),
   scanline-divide micro, `-O3` on the glide3x target.
3. **Bilinear texture filtering** — the ground requests `texFilterMode(1,1)`
   and we point-sample it. Look change only, wrapper-internal.
4. **P4 packaging**: CI-built drop-in zip (DLL + presets + conf), preset
   parameter UI via `slangvk_chain_set_parameter`, crash-ring export hook.
5. **~~Window niceties~~ — SHIPPED in 0.4.4** as `window=`/`borderless=` presets + F10 cycle + DPI-aware apply (`g3x_window.{h,cpp}`, see §1). Remaining nibble: window *placement* presets (multi-monitor pinning) if field rounds ask for it.
6. **F9 screenshot paletted compat** (P5 stretch) — LFB write path.

What this backlog does **not** buy: in-game fps above 25, resolutions beyond
what the game picks, or any engine behavior. See §3.

---

## 3. The game's locks — source receipts (the wrapper cannot change these)

| # | Lock | Receipt (in this fork's tree) |
|---|---|---|
| L1 | **In-game pace is 25 Hz.** `CLIENT_GameLoopFrame` gates sim + render on a 40 ms budget (`uGlobalTickCountB = 0x28`) and hardcodes `fps_drawn = 25`. The in-tree comment is explicit: *"In single player the gameserver runs as part of the drawing process, this is why in single player the game is capped so hard at 25 frames."* The gate is common code — **all** game types, including the ini-selectable `GAMETYPE_SINGLEPLAYER_UNCAPPED` (= 1, via `nGAMETYPE`; it changes tick processing, not the render gate) | `D2Client/GAME/Game/Game.cpp:2368–2472`; field: swap-to-swap avg dead-on 40.0–40.3 ms while Esc-menu windows read ~24 ms with identical per-frame draw volume |
| L2 | **The Esc menu is the one unthrottled window.** When `UI_ESCMENU`/`UI_CONFIG` is set in SP, the loop early-returns and draws every pump pass — measured 45 fps. That number IS the wrapper's current cost | `Game.cpp:2396–2405`; field log be8737e |
| L3 | **Out-of-game loops have their own 40 ms budget** (`MessagePump`, `nBudgetAccum += 0x28`), so main menus are engine-paced too | `D2Win/Src/D2WinMain.cpp:1152–1196` |
| L4 | **Resolution is the game's choice.** The game creates the window and back-buffer (`fpCreateWindow(HWND, int nResolutionMode)`, `D2Client/Renderer/Renderer.h:77`); our `grSstWinOpen` just receives what it picked (field: `res=8` = 800x600). The UI logic already branches on a third mode (`D2GFX_GetResolutionMode() == 2` in `D2Client/UI/QuestLog.cpp`) — the slot custom-resolution mods patch into, but the table itself is game data | `Renderer.h`, `D2Client/UI/QuestLog.cpp:837–855` |
| L5 | **Sim and render are 1:1 inside the SP draw process.** Each rendered in-game frame shows exactly one logic tick's state, with no interpolation — there is nothing between ticks for a wrapper to show | `Game.cpp` SP branch + `D2Client/GAME/Server.cpp` (QSERVER thread, 25 FPS) |
| L6 | **Engine frame-skip logic** (when a frame's cost overflows the tick budget the engine skips ticks) — the choppiness source when a wrapper is slow. Ours fits now (~22 ms < 40 ms) | `Game.cpp:2484–2529` (`framesSkipped`) |

The practical reading: **25 fps in-game is the game's authentic, intended
rate** — not a wrapper deficiency, and not fixable from a Voodoo card's
position.

---

## 4. What requires Game.exe modification

Two very different tiers, because of the fork's structure.

### Tier A — runtime soft patches from the DLL (removed)

glide3xvk 0.4.1-0.4.3 carried one 4-byte patch inside the mapped Game.exe image
(`g3x_drivertag` — repointing the /fps "Video:" line at our tag). It never showed
on field builds and was REMOVED in 0.4.4; the game binary is no longer patched at
all. The same *technique* could reach
engine variables — but almost everything worth reaching is either
version-fragile (offsets move per build) or a bad idea:

- **Tick-gate patch** (0x28 → smaller): speeds the *simulation*, not just the
  picture — the game runs fast-forward. Breaks the game. **Not viable.**
- **`fps_drawn` display patch**: cosmetic lie in the game's own /fps line.
  Pointless — our overlay already tells the truth.
- Verdict: **no Tier A work is planned; the DLL stays read-only toward
  Game.exe.**

### Tier B — projects on the rebuilt d2.exe (the fork's actual advantage)

Every prior art project had to hex-patch binaries they didn't own. This fork
*has the recovered source* — a Game.exe modification is a source edit and a
rebuild on the `master`-based toolchain (§2 of PLAN.md). That moves these from
"reverse-engineering project" to "engineering project":

| Project | What it takes | Prior art | Effort |
|---|---|---|---|
| **Motion interpolation (the real 60 fps answer)** | Render the 25 Hz sim state but interpolate sprite/entity positions between ticks to the display refresh. Requires hooking the draw path with sub-frame timing — as the Glide owner we already intercept every sprite quad, and on our own exe we can expose exact tick timestamps. d2dx proves the visual result is excellent | d2dx (MIT) — same Glide-patch architecture | Large (multi-milestone; version-lock problem disappears for us) |
| **Custom/higher resolutions** | The engine ships modes 0/1 (640x480/800x600) and its UI already branches on a mode 2; the mode table + camera/UI math are in-tree and ours to extend. The wrapper side needs nothing new — it presents whatever buffer size the game hands it | SGD2FreeRes (binary patches); here: source edit | Medium |
| **In-tree Vulkan renderer** (replace the Glide seam entirely) | Earlier rejected because hand-edits to the machine-regenerated tree risk upstream regen conflicts (PLAN.md §3). On our own rebuilt exe that objection weakens — but the Glide seam still works everywhere (vanilla installs), so this stays a research branch at best | — | Large, low priority |
| **Crash-reporter integration** (P4) | Master's VEH reporter (`d2crash-*.txt`) learns the DLL's last-N-Glide-calls ring; both sides are in-tree | — | Small |
| **Sim/render decoupling beyond interpolation** (client sim at display rate) | Touches the game-speed contract; risk class of the tick-gate patch. Only worth it as research | — | Large, likely never |

Branch discipline (PLAN.md §0 applies): Tier B work happens on feature
branches off the rebuilt-exe line, never inside `glide3xvk/`.

---

## 5. Decision matrix

| Want | glide3x.dll alone? | Game.exe needed? |
|---|---|---|
| Correct, complete D2 rendering on vanilla 1.14d | **Done** (0.3.x) | — |
| CRT/scanline/sharpen shaders, hotkeys, scaling | **Done** (0.4.0–0.4.3) | — |
| Max wrapper speed, no engine frame-skips | **Done** (0.4.4: 22 ms < 40 ms budget) | — |
| Esc menu / cutscenes at 60 | Backlog §2 (GPU raster + fence ring) | — |
| 60 fps-smooth *motion* in-game | — | **Yes — Tier B interpolation** (d2dx-style) |
| Resolutions beyond 800x600 | — | **Yes — Tier B mode table** |
| HD assets, BNet, engine features | — | Out of scope (PLAN.md §1 non-goals) |
| Zero-game-file-install guarantee (vanilla-safe) | **Preserved by everything in §2** | Tier B breaks it (rebuilt exe) — that's the trade |

---

## 6. Recommendation

1. **Commit 0.4.4** — the wrapper is at the engine's ceiling: skip-free,
   shader-chained, self-identifying, and now log-proven (`avg=` vs `work=`).
2. **GPU-side raster + fence ring** = the remaining wrapper milestone worth
   doing (Esc 60, headroom); package P4's drop-in zip with it.
3. **Open Tier B as a new phase (P6) on the rebuilt-exe line** — motion
   interpolation first (the fork's source advantage makes this the one place
   we can beat d2dx's result with less risk), custom resolutions second.
   Scoped separately so `glide3xvk/` stays vanilla-install-clean.
4. Keep the non-goals: no Tier A soft patches beyond drivertag, no BNet, no
   HD-asset replacement.
