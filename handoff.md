# HANDOFF — Glide3xVK Route-B, post-Stage-2a hotfix

Date: 2026-10-30. Repo: `github.com/VTSTech/D2LoD-libslang-vk` (Glide3xVK line), local tree
`/home/z/my-project/Glide3xVK`, branch `routeb`. Companion docs: `ROUTE-B.md` (as-built plan,
§7 stage table), `BUILD-32BIT.md` (DLL build recipe), `FIELD-TEST.md` (user-facing field manual).

---

## 1. Status snapshot

| Stage | Scope | State |
|---|---|---|
| 0 | compute-rasterizer spike + bit-exact CI gates | **CLOSED** (pushed) |
| 1 | binned raster, 16-frame corpus, fxparity capture v1 | **CLOSED** (pushed, `b715c88` = origin tip) |
| 2 | integration: `gen_raster_gls` single-source, DLL GPU path behind `raster=gpu`, `g3x_vk_present_image` seam | LANDED **locally** (`8c28807`) — CI corpus + replay green through the packer split; **field exit NOT yet met** |
| 2a | field-bug hotfix: per-swap frame-window reopen + conditional sink detach | LANDED **locally** (`4f3c50b`) — awaits user rebuild + field retest |
| 3 | chain + overlay: gamma pass + overlay boxes on GPU frames, storage-usage chain input, slang-path shots (~500 LOC) | not started |
| 4 | field round: default-off conf flag, dump-on-mismatch, perf measurement (~200 LOC) | not started |

**Branch state: `routeb` is ahead 2 of origin** (`8c28807`, `4f3c50b`). Sandbox git credentials
were rotated out — pushes happen from the user's side (apply patch or web-edit, then push).

## 2. Session story (what happened since Stage 1 closed)

1. Stage 2 approved → delivered as source overlay (`glide3xvk-stage2-overlay.zip`) + patch.
   User built the DLL (v0.4.6, their build id `053c605`, MinGW i686 per BUILD-32BIT.md).
2. First run: `routeb.comp.spv not found — CPU raster stays` → fail-soft worked as designed.
   Overlay was source-only (sandbox has no glslang). Fix: compile
   `glslangValidator -V spike/routeb.comp -o routeb.comp.spv` and put it next to `glide3x.dll`.
3. Second run (real hardware, RX 560, windowed 1920x1080, game 800x600): kernel engaged
   (`raster=gpu: kernel live (800x600, 25x19 tiles) — CPU raster idle (ghash=on)`) but
   **screen went black after the intro** — `detached (rendered 0 frames)`.

### Field bugs found & fixed (commit `4f3c50b`, "Stage 2a hotfix")

- **Frame-window lifecycle (the black screen).** The record sink's collection window opened on
  `api_frame`, which fires per `grSstWinOpen` (it IS the cap format's frame boundary). The live
  loop opens a context ONCE and presents N times; `end_frame()` closed the window at every
  present. After the first primitive-less present (game clears before it draws), `gp_prim`'s
  `fp_open` guard dropped the whole session's stream → 0 frames dispatched → slang presented a
  black out-image. Fix: `end_frame()` reopens the window at the real per-swap boundary,
  whatever the frame did (drawn / Bink-LFB / lost). The replay harness could never see this:
  it drives one window per `EV_FRAME`.
- **Shared sink slot.** `fxcapture` and the GPU path attach the single `s_routeb` slot; both
  stop paths detached with bare `nullptr`, silently disarming whichever module was still
  attached. Fix: `g3x_frame_routeb_detach()` — conditional by pointer.
- **Known limitation (documented, not "fixed"):** with both enabled, last attach wins
  (`fxcap_start` runs before `raster_gpu_start` in `grSstWinOpen`, so GPU wins) and the capture
  comes out empty. Rule: **record gate captures with `raster=cpu`.** A true mux sink is
  possible later if ever needed.

Local verification after the hotfix: `g3x_raster_gpu.cpp` / `g3x_fxcap.cpp` syntax-clean against
vendored volk + Vulkan-Headers; CPU corpus harness re-run matches `spike/raster_hashes_stage1.txt`
bit-exact (16/16).

## 3. Deliverables currently in `/home/z/my-project/download/`

| File | What |
|---|---|
| `stage2-integration.patch` / `0001-Route-B-Stage-2-...patch` / `stage2-flat.diff` | Stage 2 (`8c28807`) — user already applied & built |
| `stage2a-hotfix.patch` | `4f3c50b` as git format-patch (applies on top of a tree that has Stage 2) |
| `stage2a-hotfix-overlay.zip` | 3 files (`src/g3x_raster_gpu.cpp`, `src/g3x_frame.cpp`, `src/g3x_frame.h`) + `_OVERLAY.txt` manifest — **delete the manifest after extracting, never commit it** |
| `stage1-close.patch`, `Glide3xVK_stage1_overlay_binned.zip`, `stage2-overlay/` | earlier Stage 1/2 delivery artifacts (superseded) |

## 4. Next actions

**User:**
1. Apply `stage2a-hotfix.patch` (or extract the overlay zip over the repo) and rebuild the DLL
   per BUILD-32BIT.md. `routeb.comp.spv` must sit next to `glide3x.dll`.
2. Push `8c28807` + `4f3c50b` to origin (web-edit or `git am`) so CI runs against the hotfix.
3. Field retest, `glide3xvk.ini`: `raster=gpu`, `ghash=1`. Expect:
   ```
   raster=gpu: kernel live (800x600, 25x19 tiles) — CPU raster idle (ghash=on)
   frame 1: ghash=<16hex>          <- per-frame FNV-1a of what the KERNEL drew
   ...
   raster=gpu: detached (rendered 197 frames)     <- N > 0, menu renders
   ```
   Any `raster=gpu: ... — CPU raster stays` line = fail-soft engaged; send the log.
4. Once it draws: gate capture in a separate session with **`raster=cpu`**:
   `fxcapture=1` + `fxcap_frames=10` (10 = default). Play variety (town, dungeon, combat,
   menus, a save/load). Log names the file: `fxcapture: recording 10 frames -> ...\fxcapture.cap`.
   Note: `fxcap_frames` counts `EV_FRAME` events = **context opens** (`grSstWinOpen`), not
   swaps — a menu session is 1 big frame; area/mode switches add frames. Send me the cap.

**Me (on receiving the cap):**
1. Size check: >100 MB → gzip it and add a CI-side gunzip step to the replay gate in
   `.github/workflows/build.yml` (currently absent).
2. Stage as `spike/fxparity.cap` (replaces the 1-frame Stage 1 capture), pin nothing — the
   replay gate asserts CPU == GPU per frame boundary, no hash pin needed for replay.
3. CI replays through both backends on Lavapipe → all frames bit-exact = **Stage 2 exit
   closes**; update ROUTE-B.md §7 row 2 exit cell; then propose Stage 3.

## 5. Reference — commands, gates, gotchas

- **CPU gate (local, no Vulkan needed):**
  `g++ -O2 -std=c++17 -I spike/fakewin -I src spike/raster_regress.cpp src/g3x_frame.cpp -o raster_regress && ./raster_regress cpu`
  → 16 frame hashes, must match `spike/raster_hashes_stage1.txt` exactly.
- **Harness modes** (`spike/raster_regress.cpp`): `cpu` / `gpu <spv>` / `cmp` / `replay <cap> <spv>`.
- **CI routeb job** (`.github/workflows/build.yml`): `gen_raster_glsl.py --check` → NoContraction
  assert → corpus CPU hashes → pin diff → Lavapipe GPU hashes (`VK_DRIVER_FILES=/usr/share/vulkan/icd.d/lvp_icd.json`) →
  `raster_regress replay spike/fxparity.cap routeb.comp.spv`. Build job additionally stages
  `routeb.comp.spv` next to the DLL in the artifact.
- **Local box limits:** no Lavapipe, no glslang → GPU gates only run in CI. No git push from
  sandbox (creds rotated) → patches/overlays out, user pushes.
- **Invariants (do not break):** GLSL `precise` ↔ `NoContraction` ↔ SSE2 float == compute float;
  `+4096.0f` floor trick; integer blend `(x*y+255)>>8`; P2.9 chroma index-0; palette LUT +
  `pal_mult`; single source of truth = `src/g3x_raster_core.h` (generator extracts from it;
  `routeb.comp` is generated — never hand-edit); pushed commits are never amended, version
  numbers are user-set only.
- **ini keys:** `raster=gpu` (default `cpu`), `ghash=1`, `fxcapture=1`, `fxcap_frames=N`.
- **Fail-soft log strings to watch:** `gpu-nodev`, `gpu-spv` (not found / short read),
  `gpu-sm`, `gpu-pipe`, `gpu-pack`, `gpu-mem` — all end with "CPU raster stays" or "frame lost".
- **Stage 1 pins that must never move:** `spike/raster_hashes_stage1.txt` (16-frame corpus),
  `spike/fxparity.cap` (16381 events, frame 0 `b13a78b77af56861`).

## 6. Stage 3 preview (next work item after exit closes)

Per ROUTE-B.md §7 row 3: storage-usage chain input (the slang chain consumes the kernel's
output image), layout round-trips, **gamma pass + overlay boxes on GPU frames** (currently
CPU-side on GPU frames — the only visible behavioral deferral), slang-path shots (WYSIWYG
byte-identical to CPU path). ~500 LOC. Exit: chain-on replay clean + identical shots.
Stage 4 then: field round, dump-on-mismatch, perf measurement (frame `work=` before/after).
Deferred by design from Stage 2: device-local texel heap (§5 end state, Stage 4 perf round).

## 7. Housekeeping / open questions

- `5790d4a` (README hotkey table) — old unpushed commit, push-or-drop still user's call if it
  still exists anywhere; not on `routeb`'s current line.
- Stray-file rule stays: overlays carry `_OVERLAY.txt`, user deletes after extract, never commits.
- If a session-continuation summary is needed, this file is the source of truth — keep it
  updated after each milestone (it lives in `download/`, not the repo).
