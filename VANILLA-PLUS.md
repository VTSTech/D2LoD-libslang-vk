# Vanilla-Plus — VTSTech's custom Diablo II, built on Redivivus

This branch is the **game-source line**: improvements over vanilla Diablo II
1.14d made directly in the recovered source, on top of
[jaenster/redivivus](https://github.com/jaenster/redivivus) (`Game.exe`
recovered function-by-function as C++ that builds). It is kept deliberately
clean of the glide3x→Vulkan wrapper work — that lives on the `libslang-vk`
branch of [VTSTech/D2LoD-libslang-vk](https://github.com/VTSTech/D2LoD-libslang-vk)
and overlays on top of whatever `d2.exe` is running.

## The two layers

```
+----------------------------------------------------------+
|  d2.exe   (this branch: redivivus source + vanilla-plus  |
|            fixes/improvements, built with mingw i686)    |
+----------------------------------------------------------+
|  glide3x.dll                                             |
|  (libslang-vk branch: glide3xvk wrapper — Vulkan render, |
|   slang shader chain, window presets, controller, ...)   |
+----------------------------------------------------------+
|  your own Diablo II: LoD 1.14d install (MPQs + keys)     |
+----------------------------------------------------------+
```

The wrapper never touches Game.exe and this branch never touches the wrapper;
each can be fixed, replaced and released independently.

## Principles

1. **Vanilla behavior is the default.** Improvements are opt-in, config-gated
   or bug-fixes; nothing silently changes gameplay or balance.
2. **Fixes first, features second.** The recovered source still crashes in
   places (reconstruction artifacts). Every crash fixed is the top priority —
   upstream's rule holds: each root cause usually takes a whole class of
   crashes with it.
3. **Fixes belong upstream where possible.** A crash that comes from a wrong
   type or lost value in the reconstruction should be reported upstream
   (issue + `d2crash-*.txt`) so it survives regeneration. Only
   vanilla-plus-specific behavior (new toggles, QoL) lives only here.
4. **No game data, no Blizzard binaries.** MPQs and CD keys come from the
   user's own 1.14d install. Single-player focus; Battle.net is out of scope
   until upstream recovers it.

## Build

```sh
cmake -S . -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-i686.cmake
cmake --build build          # -> build/d2.exe
```

Copy `d2.exe` next to the original `Game.exe` in a 1.14d install and run
`d2.exe -w`. Works with the `libslang-vk` overlay: drop that overlay's
`glide3x.dll` in the same folder as usual.

## Crash triage protocol (current top priority)

The game is "currently crashing with glide". To attribute a crash correctly,
collect **both** reports from one repro:

| Artifact | Comes from | Tells us |
|---|---|---|
| `d2crash-<date>-<time>.txt` | this d2.exe (upstream's symbolized report, written next to the exe) | the crashed module, function and offset **in game source terms** |
| `glide3x.log` | the glide3xvk wrapper | what the wrapper was doing (frame, preset, chain, controller state) and whether the faulting thread was in wrapper code |

Decision rule:

- faulting frame in `glide3xvk` / `vk_backend` / slang chain -> wrapper issue,
  fix on `libslang-vk`;
- faulting frame in `D2*` / `Storm` / `Fog` source with a d2crash report ->
  game-source issue: minimize the repro, check upstream issues, then either
  report upstream (reconstruction bug) or fix here (vanilla-plus behavior);
- no `d2crash-*.txt` written and the process dies silently -> suspect the
  boundary: exports, calling conventions, or the glide driver swap itself.

Attach both files to a new issue on the fork; a crash without both artifacts
cannot be attributed.

## Roadmap

1. **Stable base** — a full single-player session (create character, several
   area transitions, save+exit, town + combat + inventory screens) crash-free
   on `d2.exe` + `glide3xvk` together, with the crash protocol above used to
   burn down whatever fires.
2. **Compatibility pass** — confirm the wrapper's whole feature set (window
   presets, shader chain, controller, base_res) against the rebuilt exe and
   note any game-side differences vs original Game.exe.
3. **Vanilla-plus improvements** — opt-in QoL on top of a stable base
   (candidates discussed per issue; nothing lands without a config gate).
