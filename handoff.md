# main branch — handoff for future work (Sound + Glide booting)

*Rev 40 (2026-10-31, handoff-refocus round — **handoff-only, zero source changes**): owner retired the `720p` branch (absorbed into `main` by merge `fec99ff`; PR branches all sent upstream). Owner status verbatim: **"Current status: D3D FS/Window booting. normal+720p res. Sound on Intro Video. No sound at/after Title Screen."** and **"the merge and port work is done so far as it compiles green and boots now. But the individual fixes vanilla-plus implemented have not been tested or re-litigated yet."** All 720p/crash-era handoff content (revs 28–39 + §1–§9) moved verbatim to **HISTORY.md H25**. This round verified in-tree facts for the two open tracks: **(A) SOUND — the merge port (`d6affdd`) dropped the defect-12 fix**: main's `D2Sound/Src/D2SoundSmp.cpp` again carries the raw IEEE-754 hex-literal casts (:769/:774/:785/:790/:793/:797) that were decoded and field-verified on the vanilla-plus line (H3 Defect 12, "Music works now! Both at title and in game.") and are still fixed on both `720p` and upstream `pr4/sound-except-intro` — as integer casts they are ≈4.54e18, not 0.0001, so every game-engine volume pins to DSBVOLUME_MIN (-10000) while Bink intro audio (own DirectSound) survives: exactly the field signature. Fix spec in §3.3 (constant decode only — do not re-audit the SMP layer, H3 exoneration). Fix 16 and the hwnd fix DID survive the merge. **(B) GLIDE — game-side site-7 verified intact** (size table, grSstWinOpen enums 16/17, perspective path; gates green); the DLL is built on branch `Glide3xVK` where the resolution table is absorbed in-source; prime suspect is a stale/unpatched deployed glide3x.dll; needs `glide3x.log` + `d2boot.log`. Gates re-run sandbox-side on merged main: **183/183**. No new overlay this round; the first code round on main bumps the self-ID (§8).*

## 1. Branch topology (owner-corrected, 2026-10-31)

- **`main`** — the only integration branch. The `720p` suite (merge `fec99ff`) and the vanilla-plus line are absorbed. Build self-ID `vanilla-plus 0.0.5 (rev37)` (`D2Client/Engine/Application.cpp:66`); gates **183/183** (`tools/check-vanilla-plus.sh`). Work happens here.
- **`720p`** — retired, read-only archive. Its handoff era (revs 28–39) lives in **HISTORY.md H25**.
- **PR branches** (`pr1..pr5`, `jaenster-main`) — sent upstream (jaenster/redivivus) already; **not integration targets; do not merge from them.** They remain useful as read-only references for fix specs (e.g. `pr4/sound-except-intro` = upstream PR #10, the sound trio).
- **`Glide3xVK`** — the glide3x.dll wrapper branch. **All Glide DLL work happens there.** `D2Glide/` + `D2Client/Renderer/Glide.cpp` in main are the game-side glide renderer client and stay in main.
- Full pre-720p history: HISTORY.md H1–H24 (H3 = the first sound-defect series record; H17 = the earlier supersession precedent; H21 = fix-chain ledger rows 1–27).

## 2. Field status (owner, 2026-10-31)

| Area | Status |
| --- | --- |
| Boot → Intro → Title → Char Select → In game | WORKING |
| D3D fullscreen + windowed, 800x600 and 1280x720 | WORKING |
| Intro video sound | WORKING (Bink path — explains why "sound" is split) |
| Sound at/after title screen | **NOT WORKING** → track A (§3) |
| Glide (`-3dfx`) boot | **NOT WORKING** → track B (§4) |

"Compiles green and boots" ≠ fixes verified: the individual vanilla-plus fixes are in-tree but untested on main's build — see the re-litigation ledger (§5) and checklist (§7).

## 3. Defect track A — Sound silent at/after title screen

### 3.0 The two audio paths (why intro works and everything after doesn't)

1. **Intro video (Bink)**: `BinkSetSoundSystem(BinkOpenDirectSound, 0)` (`D2Client/D2Client.cpp:4033`) — Bink opens its **own** DirectSound instance, independent of the game engine. Field-confirmed working.
2. **Game engine**: `SOUND_Initialize` at boot (`D2Client/Engine/Application.cpp:170–179`, real-hwnd fix already in) → `D2Sound/Sound.cpp` up to 3× `SOUND_InitDirectSound` attempts (EAX/3D downgrades) → on success: primary buffer play, `SOUNDSMP_CreateSoundThread`, `SOUND_InitMusicBuffer` (`SFileDdaInitialize` — the DDA music stream), `SOUNDSMP_SetGlobalVolume(0xff)`. Title music = DDA stream (`D2SoundSmp` → `SFileDdaBeginEx`); UI/game SFX = the sfx channels (`D2Client/GAME/Sound.cpp`). **Every volume→attenuation conversion funnels through `SOUNDSMP_ClampVolume` / `SOUNDSMP_ClampAttenuation` (`D2Sound/Src/D2SoundSmp.cpp`).**

### 3.1 Prime suspect (verified in-tree this round): the merge dropped the defect-12 fix

- main's `D2Sound/Src/D2SoundSmp.cpp` carries the raw hex-literal casts:
  - `ClampVolume` epsilons: :769, :774 — `(long double)(double)0x3f1a36e2e0000000`
  - `ClampVolume` scale: :785 — `(long double)(double)-0x3f60c00000000000` (decodes to −0.00341796875; retail scale is −2000.0)
  - `ClampAttenuation`: :790 epsilon, :793 bottom bound `(double)-0x3f3c780000000000` (decodes to −0.0004344), :797 divisor `(double)0x409f400000000000` (retail: 2000.0)
- In C++ `0x3f1a36e2e0000000` is the **integer** ≈4.54e18 (Ghidra printed the double `0.0001` as an IEEE-754 bit pattern and the port transcribed it literally), so `value <= min + eps` fires on **every** input → every conversion returns −10000.0 → **every engine buffer pinned at DSBVOLUME_MIN**. The intro video is immune (separate path). Field signature matches defect 12 exactly.
- **Provenance (verified via git)**: `720p` HAS the decoded constants (8 sites); upstream `pr4/sound-except-intro` HAS them (PR #10 commit 2); main's last change to the file is `d6affdd "merge port"` — **the fix did not survive the merge**. First confirmed casualty of the owner's "fixes not re-litigated" caveat; precedent is H17 (ad964f3 superseded 6 D3D/glide hand fixes).
- Why no gate caught it: the 183-row gate suite covers site-5/6/7 UI+glide and `d2boot-fix:` rows but has **zero sound rows**. Add sound gate rows when the fix lands (§5 rule).

### 3.2 What DID survive the merge (do not redo)

- **fix 16** — `D2Client/GAME/Sound.cpp:523–539` reads `SOUNDSMP_CalculateVolumeAndPan` out params as integers (`[vanilla-plus fix 16]` comment in-tree; H3 Defect 13, field-verified "The sounds works now. NPC's Monsters, Player. All working.").
- **the hwnd fix** — `Application.cpp:170–179` passes the real window handle (`[sound fix]` comment in-tree).
- **d2boot sound marks** — `Application.cpp:175–177`: `SOUND_Initialize enter (hwnd=...)` / `SOUND_Initialize done (dsoundInit=%d eax=%d)`.

### 3.3 The fix (verbatim spec from H3 Defect 12 / upstream PR #10 commit 2)

In `D2Sound/Src/D2SoundSmp.cpp`, decode five literals and restore the scale — nothing else:

| line | now | becomes |
| --- | --- | --- |
| :769 | `(long double)(double)0x3f1a36e2e0000000` | `(long double)0.0001` |
| :774 | `(long double)(double)0x3f1a36e2e0000000` | `(long double)0.0001` |
| :785 | `(float)(fVar1 * (long double)(double)-0x3f60c00000000000)` | `(float)(fVar1 * -2000.0)` |
| :790 | `(long double)(double)0x3f1a36e2e0000000` | `(long double)0.0001` |
| :793 | `(long double)(double)-0x3f3c780000000000` | `(long double)-10000.0` |
| :797 | `(long double)(double)0x409f400000000000` | `(long double)2000.0` |

Curve of record (retail FUN_005165f0, user-supplied): `value ≤ min+0.0001 → −10000`; `value ≥ max−0.0001 → 0`; else `−2000·log10((max+min)/value)` centibels; `SOUNDSMP_ClampAttenuation` (10^(att/2000)) is its exact inverse. Verified: 255→0, 110→−730, 0→−10000. DirectSound: −10000 = absolute mute, 0 = full. Keep the comments self-contained (upstream-clean wording, the pr4 commit comments are the model). Overlay zip over main, CRLF byte-exact, `VP-BEGIN/VP-END` markers (upstream file), gates for the decoded tokens (§8).

### 3.4 Discriminator first — read d2boot.log before touching code

- `SOUND_Initialize done (dsoundInit=1 ...)` → engine alive → the ClampVolume pinning is fully consistent; land §3.3.
- `dsoundInit=0` → engine dead, different track: the game log then contains `DirectSound error: ...` lines (`SOUND_InitDirectSound` logs every failing call — DirectSoundCreate / SetCooperativeLevel / CreateSoundBuffer / GetCaps / SetFormat / EAX probe) naming the exact failing call; fix that instead.
- If after §3.3 music works but SFX stay silent → re-walk the fix-16 chain with the sounddiag marks **re-cut, not reinvented** (mark formats of record survive in HISTORY.md H4/sounddiag: `SetVolume#N: vol= glob= lin= att= buf= hr= from=`, `updchan:`, `calc:`, `fadein:`/`fadevol:`).

### 3.5 Do-not list

- **Do NOT re-audit the SMP layer** — H3 exoneration: halt IDs, struct offsets, float literals, master multiply, EAX property IDs all match retail ("Do not re-audit the SMP layer").
- **Do NOT touch `support/d2_loud.cpp` for sound issues** — it is the LOUD diagnostic build's bounds/guard/canary handlers, not audio (name trap).
- **Do NOT chase the SetPan right-branch negation** as a silence cause — carried watch item (H3): stereo position only.
- **Do NOT merge PR branches to "get the fix"** — owner: they are upstreamed; §3.3 above is the complete record.

## 4. Defect track B — Glide not booting

### 4.1 Architecture split (owner-corrected)

- **Game side (main)**: `D2Glide/` + `D2Client/Renderer/Glide.cpp` — loads glide3x.dll via `GetProcAddress("_grSstWinOpen@28", ...)` etc. Stays in main.
- **DLL side (branch `Glide3xVK`)**: the glide3x.dll wrapper — **all DLL work happens there**, including the resolution table.

### 4.2 Verified in-tree this round (main, game side)

- Site-7 intact: size table mode 4 = 1280x720 / mode 3 = dormant 1344x700 (`D2Glide/Renderer/Glide.cpp:132–133, 597–610`); `grSstWinOpen` resolution enums 16/17 (:605/:609); modes 2/3/4 take the perspective=1 init path (gates `resfix: glide*` green within 183/183).
- Wrapper contract on `Glide3xVK`: `src/g3x_types.h` `g3x_resolution_size` carries `{1280,720}`=16 and `{1344,700}`=17 **in-source** (the old `glide3xvk-720p-resolutions.patch` is absorbed — no standalone .patch file exists anymore); `src/glide_exports.cpp:130–131` honors enums ≤17. The soft-fail signature ("unknown resolution 16" in glide3x.log + 640x480 surface) belongs to wrapper builds **older** than that absorption.
- Zero renderer files changed between the "Glide works!" field confirmation (0.0.4 era, rev31/round-4, H25) and now — the failure mode this run is unknown (black screen? soft-fail? crash?).

### 4.3 Triage plan (next owner round)

1. Owner supplies: `glide3x.log` + `d2boot.log` from a `-3dfx` run, and which glide3x.dll build is deployed (date/provenance).
2. `unknown resolution 16` in glide3x.log → **stale DLL**: rebuild glide3x.dll from `Glide3xVK` HEAD and redeploy; game-side needs nothing.
3. DLL current and still failing → `d2boot.log` names the provider-init stage; a 12 s hangwatch dump names the wedged function (`d2exe.map` beside d2.exe for symbolization).
4. Field pass afterwards per H25 §6 glide bullets: world visible (not black) at the 1280x720 stop, fps overlay reads "Video: Glide", cutscenes resize 640x480 first (retail behavior), 800x600 stop byte-identical.

## 5. Re-litigation ledger — the individual fixes on main

- **Rule of the round**: merge green + boots ≠ fixes verified. The gates statically cover site-5/6/7 + d2boot-fix surfaces, but the sound fixes have **no gate rows**, and `D2SoundSmp.cpp` is the first proven drop.
- **Method**: for each fix file in the ledger (H21 rows 1–27 + the 720p-era rounds archived in H25), diff main against the ref that carried the fix: `720p` for the site/gold/roster-era work, `origin/pr4/sound-except-intro` for the sound trio (other PR branches are upstream copies of their series). Watch for H17-style supersession ("upstream's re-typed tree supersedes N hand fixes").
- **Standing rule going forward**: every fix that lands on main adds a gate row (grep for the fixed token) so a future merge cannot silently drop it. The sound constant gates (§3.3) are the first to add.
- **Field re-pass**: §7 checklist on main's build, D3D first, then glide.

## 6. Standing watches (carry-overs, unchanged)

- `roster heal:` in d2boot.log → upstream roster corruptor alive (rev35/36 stack; context H25).
- `s2c dispatch guard:` / repeating `s2c pump: drained` → packet-path pathology (rev34 hardening; H25 §8).
- hangwatch dumps at 12 s stalls; keep `d2exe.map` beside d2.exe for symbolization.
- d2boot.log first mark must read the current self-ID string — pins the exe before any analysis.
- SetPan right-branch negation deviation (stereo position only; H3 watch item).

## 7. Field checklist (re-litigation pass on main)

- **Pending from the 720p era (H25 §6)**: waypoint / quest log / party / help deep-pass; sibling dialogs (trade gold, cube UI, NPC menus, EscMenu); in-game 800↔720 switch (coord-cache refetch); 800x600 byte-parity + 640x480 untouched.
- **Sound (after §3.3 lands)**: title music, UI clicks, NPC speech, monster/player SFX, footsteps, quest fanfares, pickup-fail voice, in-game music transitions.
- **Glide**: §4.3 step 4.

## 8. Conventions (condensed, unchanged)

- **Overlay workflow**: sandbox edits the tree → overlay **zip of changed files** (owner preference, not .patch) → owner applies as own commits → llvm-mingw i686 Win11 build (`tools/build-local-win11.cmd`; MSYS2 MINGW32 `cmake --build build -j 8`, ~501 targets). The full build is the owner's gate.
- **VP-BEGIN/VP-END markers** on every fork edit in upstream files; fork-only TUs (`VanillaPlusCore.{h,cpp}` at `D2Client/UI/`) keep upstream files free of new code.
- Every source change updates `tools/check-vanilla-plus.sh` gates + `fork-surface.allowlist` (owner-side `tools/check-fork-surface.sh --update` after committing — the sandbox has no upstream ref).
- All source files CRLF; sandbox patchers preserve that byte-exactly.
- **Version self-ID** (`Application.cpp:66`) bumps on the first code round after this one so d2boot.log pins the build (suggest `vanilla-plus 0.0.6 (rev40)` — final naming owner's call).
- Sandbox: **no mingw toolchain, no apt** — constants-level fixes are verified by diff review + curve math; the owner's local build is the compile gate.
- Diagnostic channels: `d2boot.log` (`[d2boot]` marks, permanent), hangwatch stall dumps, the game log (`DirectSound error:` lines), `glide3x.log` (wrapper side), `d2loud-*.txt` (LOUD builds only).
- Full history: HISTORY.md H1–H24 (pre-720p) and **H25** (the 720p era, incl. the freeze-era reading guide referenced by the `d2boot:` gate comments).
