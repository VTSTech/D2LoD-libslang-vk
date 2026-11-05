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

    # 1.5) line-ending sanity BEFORE merging (the eighth-sync lesson): if the
    #      fork-touched files are CRLF and upstream's are LF, one normalization
    #      commit on the fork side FIRST (handoff §8: merged main is
    #      LF-normalized). The eighth sync's raw trial surface fell
    #      48 files/78 hunks -> 26 real sites/57 hunks — 22 of the raw
    #      conflicts were pure line-ending noise that auto-merged away.

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
    python3 scripts/include_path_audit.py
    #    INCLUDE-PATH audit (added R106, after the 1894->1908 sync compile
    #    break): every quoted #include in every tracked .cpp/.h must resolve
    #    to a tracked file the way the -I chain does (includer's dir, repo
    #    root, any dir holding a header = CMake's D2_INCLUDE_DIRS). No
    #    compiler, no shims - the whole tree in seconds, including the
    #    single-sided files the merge-site sweep never compiles. The
    #    1894->1908 sync shipped Draw.cpp still naming D2GfxHelperStrc.h's
    #    OLD D2Client/ path after the merge took upstream's move of that
    #    header to D2gfx/ - the owner's Win11 build died at [49/485]. Cheap
    #    enough to always run.
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
    python3 scripts/refile_aliasing_audit.py
    #    REFILE-ALIASING audit (added R124, after the 1908->1948 eighth-sync
    #    D3D fullscreen boot crash): upstream's file-symbols refile turned
    #    retail globals into function-local statics; a refiled SCALAR indexed
    #    as an array (or a refile static + the old global-relative spelling of
    #    the same storage living side by side in one TU) aliases the real
    #    storage only at RETAIL data addresses - in a rebuilt exe the static
    #    lives wherever the linker puts it and the indexed read runs into
    #    unrelated .bss. The eighth sync shipped UploadTextureToCache's four
    #    node-base sites split across both spellings (the resolver dropped the
    #    refile on one line of one hunk): the allocator wrote one region, the
    #    per-frame palette check + SetTexture indexed the other, and D3D
    #    fullscreen AVed (READ 0x10) on the first texture re-upload. Sweeps
    #    all 220 refile-marked TUs for the three defect shapes; every grep
    #    instrument stays green on the broken state (both spellings are
    #    textually plausible), so this audit is mandatory after any merge that
    #    carries a refile commit.
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

## Bundle identity (agent-built merge rounds)

When the agent sandbox ships a merge round as a git bundle (the handoff §8
overlay workflow: bundles for upstream merges, zips for regular rounds), every
commit the bundle carries must be authored AND committed as:

    Super-Z (GLM-5.3-Flash) <super-z@z.ai>

Never the sandbox default (`Z User <z@container>`) — it leaks the container
identity into the fork's permanent history and makes the owner's `git shortlog`
attribution lie. Set the repo-local identity BEFORE the first commit of the
round (amending after the fact rewrites hashes and forces a bundle rebuild):

    git config user.name  "Super-Z (GLM-5.3-Flash)"
    git config user.email "super-z@z.ai"

Verify before bundling (author and committer, both):

    git log origin/main..main --format='%h | A: %an <%ae> | C: %cn <%ce>'

If commits already went out under the wrong identity, re-author locally
(`git commit --amend --reset-author` on the tip; for a merge+docs pair,
`git reset --soft` back and recommit), re-verify, and rebuild the bundle —
hashes change, so any doc that cites the merge hash must be updated in the
same rewrite.

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
- **The 1801 convergence repeat (f518fda0):** the same class fired again, in
  the opposite direction - upstream independently restructured
  `RENDERER_D3D_CreateSurfaceWithTexture`'s QueryInterface branch (inverted
  guard, typed `ppTexture` out-param, `ErrorLog`) into the shape the fork's
  fix already produced semantically (success -> 1, failure -> log + release +
  0). Here the fork's branch was a stylistic inversion of the same behavior,
  not an offset carrier, so the resolution was upstream verbatim with nothing
  re-planted - and the merged signature already required upstream's `ppTexture`
  (the fork's `dwParam` no longer existed as a parameter). Validates the rule's
  second half: when the fork delta carries no feature content, upstream's
  structure wins whole; the offset-carrying-variable half (Panel.cpp 1794,
  Glide.cpp's clamped `nContrastWeight` this round) is for deltas that MEAN
  something.

- **The 1806 shim-name handoff (f427f962):** `globals.h` gained
  `#include "Fog/Engine/Application.h"`, whose non-Windows path declares
  `struct IMAGE_DATA_DIRECTORY` / `struct IMAGE_OPTIONAL_HEADER32` — names the
  host shim had typedef'd since its first cut. The collision is the
  typedef-name/struct-keyword kind, so coexistence is impossible; the
  reconciliation renames the shim's layout-identical copies to
  `IMAGE_DATA_DIRECTORY_SHIM` / `IMAGE_OPTIONAL_HEADER_SHIM` (d2_crash.cpp's PE
  walk reads members, never those type names; the real winnt.h still wins under
  `_WIN32`). Rule: when upstream's tree starts DECLARING an SDK form the shim
  carried, the shim yields the canonical name and keeps its copy under an alias
  — never the reverse. Also the lightest round yet: 8 unmerged paths of 46
  two-sided files, and QuestLog.cpp's site-5 hunk was a zero-edit replant — the
  fork line was already spelled in upstream's new variable shape.

- **The 1839/1851 supersession syncs (`b2937091`, `1d401785`):** upstream
  recovered the retail forms of two fixes the fork had hand-built — the quest
  gossip tables (R54/R85, issues #13/#22; upstream's `bc47c6bc` restored every
  table verbatim, sizes AND the flavor string ids the fork had left empty rather
  than invent) and the SFX float model (vanilla-plus fix 16, Defect 13; upstream's
  `D2FloatIntUnion` is the same root cause in the retail-faithful shape). Both
  were taken whole: the hand-builds are gone, each old fix site keeps a dated
  supersession note, the `VP-BEGIN [gossip-table-fix history]` markers keep the
  root-cause lineage, and the gate rows were re-pinned to the NEW state (never
  deleted — the gates now assert the superseding form). Checked at the same merge
  and NOT superseded: issue #11's `[glide fix 11]` hook (still fork-side; the
  glide3x.dll wrapper ships from its own [`Glide3xVK`](https://github.com/VTSTech/Glide3xVK)
  repository — formerly the branch of the same name here — which upstream does not
  carry — "superseded" requires upstream to actually carry the surface). Rules:
  when upstream ships the retail-exact form of a fork fix, take it whole and
  record the supersession in the old site's comment (two spellings of one
  behavior is exactly the 1794 convergence-conflict class); re-pin, never delete,
  the affected gate rows; and before accepting "upstream superseded X", verify
  upstream actually touched X's file.
- **The 1851/1855 ground-tile supersession (`77e87e02`, fresh-clone round):** the
  repo was re-cloned from origin at the owner's R89 tip (the local-only R88 docs
  commit rode in a bundle and was re-applied from a backup patch), then upstream
  `bc47c6bc..57380e21` merged. Upstream's regen rebuilt the D3D ground-tile index
  buffers in retail form (stack `aStripIndices` prefill into a properly-extent
  `gpD3DGroundTileVertexBuffer`; low-quality fan `gdwRendererMode = {0,5,0x23,0x1e}`
  — note the retail fan's 4th vertex is 30, not the fork's hand-derived 28),
  superseding vanilla-plus fixes 24/25. Two instruments mattered: the auto-merge
  had silently spliced the fork's fix-25 loop into `InitGroundTileCache` BESIDE
  upstream's (both paths in one function, one referencing a static the resolution
  had already retired) — a dangling-reference class the conflict markers never
  showed; the per-function diff against upstream's file caught it, and the whole
  function was realigned to upstream's byte-verified body. And the R51 gate fired
  raw=2 — not a regression but the owner's own R89 skeleton-class palshift site;
  the expectation was re-pinned with the reason, not relaxed blindly. Rules: after
  resolving supersession hunks, DIFF EVERY touched function against the winning
  side whole (the splice class hides between hunks); grep the retired symbols'
  names file-wide before staging; and when a gate count changes, diff the new site
  against the owner's own recent commits before treating it as merge damage.

- **The 1894->1908 dangling include (427551b7, seventh sync):** the merge took
  upstream's whole-file regen of `D2GfxHelperStrc.h` (`D2Client/` -> `D2gfx/`,
  old copies deleted — the commit message even lists the regen) while
  `D2Client/GAME/Draw.cpp` resolved byte-identical to the fork parent (the
  [resolution site-5] VP block means the fork side wins the file whole) and kept
  `#include "D2Client/D2GfxHelperStrc.h"`. Every instrument the round ran was
  green and DESERVED to be: presence/placement/branch audits check the LINE, and
  the line was fine — the FILE it named was gone. The merge-site sweep is
  structurally blind here too: its selection is "differs from BOTH parents", and
  a parent-1-identical body never compiles, even though the merge changed the
  header set around it (negative-tested: the sweep runs 0-error on the broken
  tree). The owner's Win11 build died at [49/485] on the first TU to reach the
  include. Fix: the include is not VP-marked, so upstream wins — dropped
  (`Engine/Gfx.h`, already included, carries the full struct; upstream's own
  Draw.cpp includes nothing for it). Instruments: `include_path_audit.py`
  (step 4) resolves every quoted include in all 1183 tracked sources without a
  compiler, covering the single-sided files the sweep's selection skips; the
  sweep's header now carries the blind-spot note. Rules: when a merge moves or
  deletes a header, grep the OLD path across the tree before resolving the
  round ("the include survives the file"); and a file resolved to one parent
  wholesale still compiles against the MERGE tree's headers — only a whole-tree
  include walk sees that.
- **The 1908->1948 refile-aliasing split (`18a368ce`/`fd89b115`, eighth
  sync):** upstream's `7d30a7d8` refiled module-local symbols into their using
  modules, turning `RENDERER_DIRECT3D_UploadTextureToCache`'s texture-cache
  node-buffer bases at retail `0x00970ec4` into a function-local static
  addressed as `(&INT_00970ec4)[iMipLevel * 7]` — a spelling that aliases the
  real storage (`&gD3DGenericTextureCache - 4 + iMipLevel * 28`) ONLY at
  retail data addresses; in a rebuilt exe the static lives wherever the linker
  puts it and the indexed read runs past the 4-byte static into unrelated
  .bss. The merge took upstream's spelling for three of the function's four
  base sites and kept the fourth (the palette-identity check) on the fork's
  relative form — one hunk, two spellings, and the resolver dropped the refile
  on the if-condition line. Every instrument was green and DESERVED to be:
  presence gates count markers, the count gates did not count the statics, the
  syntax sweep compiles both spellings fine, and each spelling alone is a
  valid resolution of its own hunk — the defect lived BETWEEN hunks, in which
  spelling each of the four sites took. The rebuilt exe indexed the texture
  cache through two different regions (allocator writes vs the per-frame
  palette check + SetTexture), and D3D fullscreen died at the first
  title-screen texture re-upload (VEH 0xC0000005, AV READ at 0x10, own-code;
  `-w` and Glide untouched). Fix: all four sites back on the fork's
  build-independent relative form (field-proven R74..R122), upstream's dead
  statics dropped, dated VP note. Instruments: `scripts/refile_aliasing_audit.py`
  (step 4) sweeps all 220 refile-marked TUs for the three defect shapes —
  repo-wide CLEAN after the fix, i.e. the only site the eighth sync created.
  Rules: when a merge carries a refile commit, diff every touched FUNCTION
  against BOTH parents' spellings of the same storage (the four-sites-in-one-
  function class hides between hunks); a spelling that is address-equivalent
  at retail is NOT equivalent in a rebuilt exe — retail-relative forms stay
  build-independent, prefer them; and when a field crash survives every
  grep-based audit, suspect the two-spellings-in-one-function class and run
  the aliasing audit before blaming the old war-story classes.
