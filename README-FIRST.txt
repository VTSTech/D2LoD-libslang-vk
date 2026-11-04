d2linux M1 CI round — the i386 toolchain failure is self-diagnosing now
(branch `linux`, applies on top of your M1 commit, e.g. e557a4c)
========================================================================

WHAT THIS IS
  Your M1 CI run (e557a4c) died at the -m32 probe with a bare "cannot
  build -m32" while the apt step above it reported success -- and the
  check script had discarded attempt 1's configure log, destroying the
  only evidence of why. This round makes every link in that chain show
  its work. Zero changes to the verified M1 happy path (no TU list, no
  flag, no shim edits).

THE THREE FIXES
  1. platform/linux/build.cmake -- the probe is C++-only now (game TUs
     are C++; the SDL C TUs share the toolchain), and on failure it
     re-runs the same compile+link by hand and puts the compiler's own
     verdict (crt1.o vs 32-bit headers vs libgcc) into the FATAL_ERROR.
  2. tools/check-build-linux.sh -- attempt 1's failure tail is printed
     and kept for the record; the retry starts from a wiped build dir
     (a failed configure can leave poisoned cache state).
  3. .github/workflows/build.yml -- libc6-dev-i386 is named explicitly
     (it is the actual provider of the 32-bit crt objects/headers),
     plus a new fail-fast step that does a real gcc -m32 compile+link
     BEFORE cmake runs.

APPLY (from the repo root, on branch linux)
  1. unzip this overlay over the tree (paths are tree-relative).
  2. IMPORTANT: take .github/workflows/build.yml VERBATIM from this
     overlay -- the apt set is part of the gate. If your tree's copy
     was hand-adapted (ccache, artifact names, job name...), re-apply
     those adaptations AROUND the new steps, keeping:
       - libc6-dev-i386 in the apt install list
       - the "Verify the i386 toolchain" step after it
  3. Commit as your own (house convention):
       git add -A && git commit
     suggested subject:
       "linux: M1 CI round -- make the i386 toolchain failure
        self-diagnosing"

WHAT TO READ IN CI (next run)
  - The "Verify the i386 toolchain" step fails -> multilib is broken or
    absent on the runner; gcc's raw error names the missing piece
    (Scrt1.o/crt1.o = libc6-dev-i386; libstdc++/libgcc = g++-multilib).
  - That step passes but configure still fails -> the cmake FATAL_ERROR
    now carries the compiler's own verdict verbatim.
  - Either way the log names the cause; paste it back and this is one
    round, not five.

VERIFICATION DONE (sandbox, this round)
  - Failure path: fresh clone, no multilib, D2LINUX_ARCH=i386 ->
    attempt 1's tail printed, retry wipes the build dir, the FATAL
    carries the compiler verdict (Scrt1.o/crti.o not found, skipping
    incompatible libstdc++/libm/libgcc); script exits 1.
  - Happy path: the full 44-TU i386 build through the check script ->
    "compile errors: 0", ELF 32-bit statically linked. (The i386
    --smoke run stays CI's ring; this container refuses static-i386
    execution -- documented in PLAN.md §8.)
  - Native arch end-to-end: configure -> build -> --smoke OK, exit 0.

NEXT (step 2, toward M2)
  globals.cpp + the boot chain (D2Launch/D2Client/D2Multi/D2Common) --
  the step-1 weak globals yield to the strong ones automatically. Then
  the Storm/MPQ wiring onto the M0 presentation surface (title screen
  offline). Once this CI round is green, step 2 is unblocked.
