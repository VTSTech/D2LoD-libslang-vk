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
    python3 scripts/rev47_vp_placement_audit.py
    #    VP-marker PLACEMENT audit (added R47, after issue #10): presence
    #    checks pass a marked block that survived at the wrong brace depth;
    #    this diffs every VP marker's context against 44e3518 (the last
    #    field-verified pre-port tree). Mandatory when the merge resolved
    #    around braces; cheap enough to always run.

    # 5) build locally (Win11):
    powershell -File tools/build-local-win11.ps1

    # 6) push. NOTE: the gate does NOT run in CI (removed 0.0.6-R47: a gate
    #    failure must not halt the build) - step 4 above is the only run.
    # 7) in-game smoke: options page opens (Esc > Options), each toggle flips
    #    its d2vp.ini key, auto-gold picks a pile, alt-toggle latches labels.

The gate is a human audit instrument, not a build prerequisite: CI builds the
tree either way, and a red gate in CI only blocked shipping without stopping
the merge. Run it by hand whenever the merge surface moved (steps 0-4 above).

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

## Changing the fork surface on purpose

Every budget/count lives next to the thing it audits:

- per-feature counts: the `gate "..."` table in `tools/check-vanilla-plus.sh`
- injection-point invariants: the `static_assert`s and `VANILLAPLUS_SelfCheck`
  in `D2Client/UI/VanillaPlusCore.cpp`

Update them in the same commit as the refactor, never "later".

## Good upstream-citizen candidates

Fork edits that are generic fixes (not Vanilla+ features) should be PR'd
upstream - once accepted, they vanish from the merge surface. PRs are cut
from clean branches off main, never from this branch's working state:

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
- **0.0.6 merge port (`d6affdd`):** the port resolved conflicts to the fork's
  OLD side in `D2Glide/Renderer/Glide.cpp`, dropping the glide texture-memory
  init assignments (`0x400`/`0x8000` - first-frame divide-by-zero on `-3dfx`)
  and the fix-2/fix-3 upload guards. CI's gate run caught it ("2 FAILED,
  192 passed"), and R46 restored both. The gate did its job, but as a CI
  *blocker* it halted otherwise-good builds - owner call: gate off CI,
  run it manually (step 4). Rule: the gate's verdict is consumed by a human
  during the merge, not enforced by the build pipeline.
- **R47 auto-gold re-parenting (issue #10):** the same `d6affdd` port left
  every marker *present* but re-inserted the auto-gold trigger one brace
  early inside `DRAW_UI`'s HelpMenu block - `AUTO_GOLD_Tick()` only ran
  while the help overlay was open, auto gold was dead in normal play, and
  every presence-based check (marker audit, gate counts) was green. The
  last-known-good diff against `44e3518` caught it; the fix was one brace
  move plus a placement audit and an order-check gate row. Rule: presence
  is not placement - when a merge resolves around braces, diff each marked
  block's context against the last field-verified tree, not just its
  existence (step 4 now runs both instruments).
