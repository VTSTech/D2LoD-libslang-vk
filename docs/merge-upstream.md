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
    python3 scripts/rev48_wrong_branch_audit.py
    #    VP-marker BRANCH audit (added R48, after issue #12): presence checks
    #    also pass a marked edit resolved into the WRONG if/else arm - the
    #    regen inverted the branch shape (decompiler-equivalent) and the port
    #    matched by text. Flags any VP-marked store nulled out on the next
    #    line: a dead store, never the intended placement.
    python3 scripts/d2boot_visibility_audit.py
    #    [d2boot] INCLUDE-VISIBILITY audit (added Rev 76, after the 1781 sync
    #    compile break): presence gates cannot see a lost #include. The 1781
    #    merge re-planted SCmd.cpp's srv state-cmd mark but dropped the fork's
    #    include of support/d2_boot.h (last include-block line) - fatal at
    #    compile: "D2BOOT_MarkFmt was not declared in this scope". This walks
    #    every D2BOOT_* user TU and proves it still reaches the declaration
    #    (direct include or transitively). Mandatory when the merge touched
    #    include blocks; cheap enough to always run.
    bash scripts/merge_site_syntax_sweep.sh "$(git log --merges -1 --format=%h)"
    #    MERGE-SITE SYNTAX SWEEP (added Rev 92, reworked Rev 93 - baseline
    #    diffing): every audit above is grep-based, and a merge that keeps
    #    BOTH sides' local declarations, double-splices one function from
    #    both parents' shapes, or drops a brace is textually plausible and
    #    semantically fatal. This compiles every TU that differs from BOTH
    #    merge parents (the true merge sites) THREE ways with the host shims
    #    - merge result + both parents' bodies against the same headers -
    #    and flags only errors unique to the merge result (plus the parse
    #    danger classes unconditionally). Pre-existing host noise (undeclared
    #    Win32 surface, shim overlaps, 64-bit precision casts) cancels out
    #    against the parents' own compile results; no SDK allowlist to
    #    maintain. Rev 92's name-allowlist version missed nothing it saw but
    #    needed a name per Win32 API; v2 borrows the parents as the baseline
    #    instead. Negative-tested on the 1792 merge: caught Inv.cpp pNpc,
    #    SpellSel.cpp pPlayerUnit_lcl x4, Direct3D.cpp double-splice x2 and
    #    SFile.cpp double-case (all pre-fix), clean post-fix. Takes minutes,
    #    not seconds - mandatory after every merge, skippable for
    #    overlay-only rounds.

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
- **Rev 77 DrawSolidRectEx undefined reference:** the `d6affdd` port's
  renderer resfix lived on BOTH sides of a signature pair - `DWORD dwColor`
  in `D2Gdi/Renderer/Windowed.h` AND `Windowed.cpp` for `DrawSolidRectEx`.
  The 1776-1781 merge absorbed upstream's regen header (`BYTE`) and dropped
  the header-side resfix while the .cpp side survived: declaration and
  definition silently became two overloads, every caller bound the
  declared-but-undefined one, and d2.exe died at LINK (not compile) with an
  undefined reference. Nothing was VP-marked (the resfix predates the marker
  convention), so the marker/placement/visibility audits were all blind -
  only the paired signature tripwires (`resfix: gdi DrawSolidRectEx sig *`
  gate rows) cover this class. Rule: when a fork edit pairs a header
  declaration with an implementation, gate BOTH sides; when a regen
  legitimately retypes one side, move both gates in the same commit.
- **Rev 92 double-declared locals (the 1792 merge):** the merge kept BOTH
  sides' `D2UnitStrc* pNpc;` in `D2Client/UI/Inv.cpp` - upstream's flattened
  shape declares it at the top of `InitializeiInventoryPages` (line 182),
  the fork side at the use site (line 202) - and dropped the declaration
  line under `SPELLSEL_RenderAllSkillGrids` that the fork-side body's four
  `pPlayerUnit_lcl` uses still needed (upstream's shape inlines
  `GetPlayerUnit_UI()` into every call). Both files are textually plausible:
  every line existed in a parent, no marker lost, no gate count changed -
  and the merge round's 4-TU fork battery was green because neither file is
  fork-owned. The owner's Win11 build died on the first one. Fix (R73):
  Inv.cpp keeps upstream's top declaration and drops the use-site duplicate;
  SpellSel.cpp re-expresses the four VP-marked calls on upstream's inlined
  receiver (zero future merge surface); the R45 gate rows re-pin the new
  call shape with unchanged counts. Rule: a resolution that merges
  declaration blocks from both parents must be COMPILED, not diffed - the
  redeclaration class is invisible to every grep instrument and only the
  merge-site syntax sweep (step 4e) sees it.
- **Rev 93 double-spliced regions (the same 1792 merge, second and third
  classes):** the owner's next build died in `D2Direct3D/Renderer/
  Direct3D.cpp` with `a function-definition is not allowed here` x20. TWO
  more artifacts, both in the same resolution style as Rev 92's:
  (1) `RENDERER_D3D_RenderWallTileWithPerspective` contained the whole
  subtile do-loop TWICE - the fork-resolved shape AND upstream's flat shape
  concatenated inside one `if (nTileType <= 9) {` wrapper whose closing
  brace the splice consumed, so the FUNCTION never closed and every later
  signature parsed inside it; (2) `Direct3DCallBack` had the same
  double-splice (upstream's `iVar4` rename dropped with it, `expected '}'
  before 'else'`); (3) `Fog/Source/SFile.cpp` `LOGFILE_StoreGameStartupInfo`
  had `case 9:`/`case 10:` TWICE inside one switch - fork-side (`szOsName`)
  and upstream-side (`__frame0.szOsName`, a local the merge tree does not
  declare) concatenated. All three regions carry ZERO VP marks - pure
  decompiled code - so the fix per house policy was upstream's shape
  VERBATIM (zero future merge surface; RenderWallTileWithPerspective is now
  byte-identical to upstream's). Why the Rev 92 sweep passed them: its
  30-line per-TU error cap cut the parse errors off behind precision-cast
  noise, and its SDK allowlist chased names the decompiled TUs would never
  stop needing. Fixes: `tools/syntax-check.sh` grew `VP_SYNTAX_FULL=1`
  (cap off), the sweep switched to baseline diffing (compile each TU's
  parent bodies too; only errors unique to the merge result are real), and
  the parse danger classes surface unconditionally. Rules: a region whose
  conflict resolution touches BOTH parents' shapes must be diffed against
  BOTH parents, not one; and an audit's output cap is part of its blind
  spot - truncation is how a clean sweep lies.
- **The 1794 convergence conflict (ea0b84d4):** upstream independently implemented
  a feature the fork already carried - the 1794 regen added the health-orb
  `DrawVerticalCropImage` calls while the fork's resfix already drew the same
  fill with the wide-X offset baked into `nValue`. Two different spellings of
  the same behavior collided in one conflict hunk; resolving EITHER side alone
  silently loses the other's contribution (the fork's offset, or upstream's new
  in-arm call site), and every instrument stays green on the wrong pick:
  markers present, gate counts unchanged, sweep clean. Resolution: upstream's
  structure, fork's variable - the calls keep upstream's in-arm placement and
  take `nValue` (the offset-carrying local) as the argument, mirroring what the
  auto-merge produced for the mana orb 40 lines below. Rule: when both parents
  changed the same lines with different intent, ask what each delta MEANT
  before picking a side - this class is invisible to presence, placement,
  branch and syntax audits by construction. The same round validated the
  verbatim-upstream policy: upstream shapes taken whole (QuestLog.cpp's
  variable-role swap, Direct3D.cpp's `gadwBitMasks[]` regen) with only the VP
  terms re-planted cut the conflict surface 39 -> 10 and needed zero
  compile-fix rounds.
