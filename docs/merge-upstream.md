# Merging upstream without losing Vanilla+ (playbook)

Upstream `jaenster/redivivus` moves; this fork recompiles it with the Vanilla+
opt-in layer on top. This document is the procedure for pulling upstream in
with zero feature loss and a bounded merge surface. It exists because every
rule in it was learned the hard way (see "War stories" at the bottom).

## The structure that makes this cheap

- **Fork-owned TUs are free.** New files under the tree are picked up by the
  CMake `GLOB_RECURSE` and never conflict. All vanilla-plus implementation
  lives in fork-owned files:
  - `D2Client/UI/VanillaPlusCore.{h,cpp}` - d2vp.ini gates, alt-toggle,
    auto-gold, D2VP_* bridge, self-check tripwires
  - `D2Client/UI/VanillaPlusOptions.{h,cpp}` - the in-game options page
  - `vanilla-plus/cmake/sources.cmake` - the source/header glob filters
- **Upstream files carry only marked anchors.** Every edit inside an upstream
  file is wrapped in `// VP-BEGIN [tag] ... // VP-END [tag]` (multi-line) or
  carries a trailing `// VP:` comment (single line). The live inventory:

      grep -rnE "VP-BEGIN|VP-END|// VP:" D2Client globals d2_enums.h

- **`D2Client.h` must stay pristine upstream.** Fork API declarations live in
  `VanillaPlusCore.h`; consumers include that, not D2Client.h.
- **Code inside upstream files speaks upstream's dialect.** E.g. Automap.cpp
  uses the per-file `D2_FREE_AT(p, ".\\UI\\automap.cpp", 0xNNN)` convention
  (plus a VP-marked `D2_FREE` shim); fork conveniences exist only in fork
  files. Violating this is how a clang-only compile regression happened once.

## Merge procedure

    # 0) clean tree, up to date
    git checkout main && git pull origin main
    #    Windows note: git on Windows cannot see the exec bit, so a script
    #    added from Windows lands as 100644 and Linux CI dies with
    #    "Permission denied". Add new tools scripts as:
    #    git add --chmod=+x tools/your-new-script.sh

    # 1) wire upstream once, then merge (NEVER rebase the fork onto upstream -
    #    published history + a shared base means merge is the only safe move)
    git remote add upstream https://github.com/jaenster/redivivus.git   # once
    git fetch upstream
    git merge upstream/main          # expect conflicts; they are all marked

    # 2) resolve conflicts:
    #    - VP-marked regions: ours (re-apply on top of upstream's new code)
    #    - CMakeLists.txt: root should end up = upstream's + the one
    #      include(vanilla-plus/cmake/sources.cmake) line + the regions tagged
    #      # [vanilla-plus]; re-read the tags, don't hand-merge the old block
    #    - anything else: upstream wins, then re-add the fork edit if it was
    #      a VP anchor

    # 3) normalize mtimes BEFORE building. If the merge was resolved while the
    #    system clock was wrong (fast), ninja's manifest check dies in a
    #    "still dirty after 100 tries" loop that no rebuild can fix:
    git ls-files -z | xargs -0 touch

    # 4) one-second feature audit, no toolchain needed:
    tools/check-vanilla-plus.sh           # per-feature grep gates
    tools/check-fork-surface.sh . upstream/main   # whole-diff budget

    # 5) build locally (Win11):
    powershell -File tools/build-local-win11.ps1

    # 6) push; CI runs the same two gates before its real GCC build.
    # 7) in-game smoke: options page opens (Esc > Options), each toggle flips
    #    its d2vp.ini key, auto-gold picks a pile, alt-toggle latches labels.

## When a gate fires

- `tools/check-vanilla-plus.sh FAIL <gate>` - a marked edit was dropped or
  upstream changed shape nearby. Open the named file, search `VP-` and the
  gate's pattern, re-apply the anchor.
- `D2BOOT_Fail("vanilla-plus self-check: ...")` at boot - a count/pointer
  tripwire in `VanillaPlusCore.cpp` fired. The message names the drift
  (`gnUIFlagCount`, `nItemCount`, the row wiring, the dismiss alias). Audit
  the named edit against upstream's change; never silence the check without
  understanding why it fired.
- `static_assert` failure at compile time - the enum value or array bound the
  fork pins moved. Upstream added/renamed a UI flag or resized a table:
  re-audit `d2_enums.h` flag 38, `globals.h` externs and the `0x27` count
  bumps as one set.
- `check-fork-surface.sh FAIL new/grown` - the fork diff vs upstream changed
  beyond the budget. If intentional (you refactored on purpose), run
  `tools/check-fork-surface.sh --update` **in the same commit** as the change
  and say so in the commit message. If not, something leaked.

## Changing the fork surface on purpose

Every budget/count lives next to the thing it audits:

- per-feature counts: the `gate "..."` table in `tools/check-vanilla-plus.sh`
- whole-diff budget: `tools/fork-surface.allowlist` (regenerate with
  `tools/check-fork-surface.sh --update`; the allowlist does not budget
  itself)
- injection-point invariants: the `static_assert`s and `VANILLAPLUS_SelfCheck`
  in `D2Client/UI/VanillaPlusCore.cpp`

Update them in the same commit as the refactor, never "later".

## Good upstream-citizen candidates

Fork edits that are generic fixes (not Vanilla+ features) should be PR'd
upstream - once accepted, they vanish from the merge surface:

- `D2Client/UI/Automap.cpp` stale-`.ma`-entry crash tolerance (VP-marked,
  4 sites)
- `d2_platform.h` mingw-w64 header-drift compat (`excpt.h` guard,
  `___init_*` ABI-neutral spell)
- `D2Client/Engine/Application.cpp` sound init hwnd fix (`[sound fix]`)

Keep them stylistically upstream (their conventions, no VP tags needed in the
PR) so acceptance is frictionless.

## War stories (why these rules exist)

- **0.0.4 CI failure:** fork code inside Automap.cpp called bare `D2_FREE`,
  a macro defined only in a different TU. Local clang and CI GCC both should
  fail; CI caught it. Rule: upstream-dialect code in upstream files.
- **Ninja "manifest still dirty after 100 tries":** merge resolved while the
  clock was ~3h45m fast; after time re-synced, every generated `build.ninja`
  was older than the future-dated sources, forever. Fix is step 3's `touch`.
- **Patch does not apply:** a patch cut against a different base than the
  working tree. Whole files or in-tree edits only; patches must be generated
  from the exact commit the recipient has.
