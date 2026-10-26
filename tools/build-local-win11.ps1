<#
.SYNOPSIS
    Build Redivivus (d2.exe) locally on Windows 11 - no WSL, no cross toolchain.

.DESCRIPTION
    Runs the same build GitHub Actions runs on ubuntu-24.04 with the
    i686-w64-mingw32 cross compiler, except natively on Windows:

      toolchain : MSYS2 MINGW32 environment (mingw-w64 i686 g++, native)
      configure : cmake -S . -B build -G Ninja
      build     : cmake --build build -j
      gates     : build/d2.exe is a 32-bit PE (pei-i386), d2exe.map written
      artifact  : build/d2s-<id>.exe  (stripped copy, CI's d2s-<sha> equivalent)

    First run installs MSYS2 via winget and the i686 packages via pacman
    (one-time, a few hundred MB). The compile itself is ~500 translation
    units, unoptimised with DWARF - the first run is the slow one; later
    runs recompile only what changed. Keep ~3 GB free for build/.

    The exe is self-contained (-static libgcc/libstdc++) - no runtime DLLs.
    To play: copy build\d2.exe next to the original Game.exe of a 1.14d
    install and run "d2.exe -w" (-DeployTo automates the copy).

    v2 - fixes the "cmake configure failed (exit 1)" failure:

      * STALE CACHE. CMake caches every -D value and reuses it on every
        re-configure. If an earlier attempt set CMAKE_TOOLCHAIN_FILE (e.g.
        the README cross-configure), the native configure inherits it and
        dies with "Could not find toolchain file: cmake/mingw-i686" even
        though this script never passes one. v2 detects a stale toolchain
        entry / generator / bitness in build\CMakeCache.txt, clears it, and
        additionally passes -UCMAKE_TOOLCHAIN_FILE on the configure line.

      * NINJA LOOKUP. cmake.exe and ninja.exe are now resolved by FULL
        PATH, ninja is probed with --version (reinstalled via pacman if
        missing or quarantined, MSYS ninja as fallback), and
        -DCMAKE_MAKE_PROGRAM is passed explicitly, so a PATH quirk can
        never produce "unable to find a build program corresponding to
        Ninja" again.

      * Works whether this script sits in the repo root or in tools\.

    v3 - speed:

      * LEAN BY DEFAULT. The 350 MB "fat" exe is DWARF: the CMakeLists
        compiled every TU with -g. v3 passes -DREDIVIVUS_LEAN=ON (a new
        CMake option, default OFF so CI is untouched): same -O0 code
        generation, identical d2exe.map addresses, but objects and the
        exe shrink to a fraction and compiling + linking get much faster.
        Trade-off: the in-game crash handler symbolises from the running
        image's own DWARF, so a lean build reports module+0xOFFSET to
        resolve against d2exe.map - the same deal as CI's stripped
        d2s-<sha> artifact. Pass -Fat for the charter fat exe.

      * CCACHE ON. Uses the NATIVE WINDOWS ccache.exe (downloaded from the
        official ccache release into %LOCALAPPDATA%\ccache\bin) wrapped via
        CMAKE_*_COMPILER_LAUNCHER. The first build fills the cache; any
        later full recompile (-Clean, branch switch, a touched header like
        d2_platform.h, lean<->fat switch) is served from it. -NoCcache
        disables.

    v3.1 - ccache fix: the MSYS2 ccache package is a Cygwin program whose
      is_full_path() only sees '/'-separated paths as absolute (the _WIN32
      backslash branch is compiled out), so the backslashed compiler paths
      cmake writes into build.ninja fell through to its PATH search and
      every compile died with 'Could not find compiler ... in PATH'. v3.1
      installs the official native Windows ccache.exe instead. To build
      right now without ccache: -NoCcache.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File .\build-local-win11.ps1
    powershell -ExecutionPolicy Bypass -File .\build-local-win11.ps1 -Clean -DeployTo "D:\Games\Diablo II"
    powershell -ExecutionPolicy Bypass -File .\build-local-win11.ps1 -SetupOnly
#>
param(
    # Repo root. Default: the folder containing this script when it holds
    # CMakeLists.txt, otherwise its parent (script living in tools\).
    [string]$RepoRoot = "",
    # MSYS2 install directory. Installed via winget when missing.
    [string]$Msys2Root = "C:\msys64",
    # Install/verify the toolchain only, do not build.
    [switch]$SetupOnly,
    # Skip the toolchain check and go straight to building (faster repeat runs).
    [switch]$BuildOnly,
    # Delete build/ before configuring (parity with a clean CI run).
    [switch]$Clean,
    # Parallel compile jobs. 0 = all logical processors.
    [int]$Jobs = 0,
    # Do not produce the stripped d2s-<id>.exe copy.
    [switch]$SkipStrip,
    # Charter fat exe: compile with -g DWARF (REDIVIVUS_LEAN=OFF). Default is
    # the lean build - no -g, same code generation and map, much faster.
    [switch]$Fat,
    # Disable ccache even when it is installed.
    [switch]$NoCcache,
    # Copy build\d2.exe (+ d2exe.map) into this directory (a 1.14d install).
    [string]$DeployTo = ""
)

$ErrorActionPreference = "Stop"

function Write-Step { param($m) Write-Host ""; Write-Host "==> $m" -ForegroundColor Cyan }
function Write-Ok   { param($m) Write-Host "    $m" -ForegroundColor Green }

# Returns $true when the exe exists AND actually runs. Catches both a missing
# file and the silent breakage of a Defender-quarantined / truncated exe.
function Test-ExeRuns {
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) { return $false }
    try {
        $null = & $Path --version
        return ($LASTEXITCODE -eq 0)
    } catch { return $false }
}

# ---------------------------------------------------------------------------
# Repo root
# ---------------------------------------------------------------------------
if (-not $RepoRoot) {
    if (Test-Path (Join-Path $PSScriptRoot "CMakeLists.txt")) { $RepoRoot = $PSScriptRoot }
    else { $RepoRoot = Split-Path -Parent $PSScriptRoot }
}
$RepoRoot = (Resolve-Path $RepoRoot).Path
if (-not (Test-Path (Join-Path $RepoRoot "CMakeLists.txt"))) {
    throw "CMakeLists.txt not found under '$RepoRoot'. Pass -RepoRoot <path>."
}
if ($RepoRoot.Length -gt 90) {
    Write-Warning "Repo path is $($RepoRoot.Length) chars; Windows compilers can hit MAX_PATH on long trees."
    Write-Warning "A short checkout such as C:\dev\D2LoD-libslang-vk avoids that."
}
Write-Step "Repository: $RepoRoot"

# ---------------------------------------------------------------------------
# MSYS2 (provides the i686 mingw-w64 toolchain this project requires)
# ---------------------------------------------------------------------------
$Pacman = Join-Path $Msys2Root "usr\bin\pacman.exe"
$Bash   = Join-Path $Msys2Root "usr\bin\bash.exe"

if (-not (Test-Path $Pacman)) {
    Write-Step "MSYS2 not found at $Msys2Root - installing via winget"
    if (-not (Get-Command winget -ErrorAction SilentlyContinue)) {
        throw "winget not available. Install MSYS2 manually: https://github.com/msys2/msys2-installer/releases (default C:\msys64), then re-run."
    }
    winget install --id MSYS2.MSYS2 -e --accept-source-agreements --accept-package-agreements
    if ($LASTEXITCODE -ne 0) { throw "winget failed to install MSYS2 (exit $LASTEXITCODE)." }
    if (-not (Test-Path $Pacman)) {
        throw "MSYS2 installed, but $Pacman is missing - pass -Msys2Root if you picked another directory."
    }
    Write-Ok "MSYS2 installed"
} else {
    Write-Step "MSYS2 found: $Msys2Root"
}

# ---------------------------------------------------------------------------
# i686 toolchain packages (one-time)
# ---------------------------------------------------------------------------
$Packages = "mingw-w64-i686-gcc mingw-w64-i686-cmake mingw-w64-i686-ninja git"
$env:CHERE_INVOKING = "1"   # keep bash where it is
$env:MSYSTEM = "MINGW32"    # the 32-bit (i686) environment, not the 64-bit one

if (-not $BuildOnly) {
    Write-Step "Installing toolchain packages via pacman (one-time, few hundred MB)"
    $install = "pacman -Sy --noconfirm; pacman -S --needed --noconfirm $Packages"
    & $Bash -lc $install
    if ($LASTEXITCODE -ne 0) {
        Write-Warning "pacman install failed - updating the MSYS2 core first, then retrying once"
        & $Bash -lc "pacman -Syu --noconfirm"
        & $Bash -lc $install
        if ($LASTEXITCODE -ne 0) {
            throw "pacman could not install the packages. Close every MSYS2 window and re-run this script."
        }
    }
    Write-Ok "Toolchain packages present"
}
if ($SetupOnly) { Write-Step "Setup-only: done."; return }

# ---------------------------------------------------------------------------
# Build tools - resolved by FULL PATH, never by PATH luck
# ---------------------------------------------------------------------------
$Ming32Bin = Join-Path $Msys2Root "mingw32\bin"
$Gpp       = Join-Path $Ming32Bin "g++.exe"
$CmakeExe  = Join-Path $Ming32Bin "cmake.exe"
$NinjaExe  = Join-Path $Ming32Bin "ninja.exe"

$env:PATH = "$Ming32Bin;$Msys2Root\usr\bin;$env:PATH"

if (-not (Test-Path $Gpp)) {
    throw "g++.exe not found in $Ming32Bin. Run once without -BuildOnly so pacman can install mingw-w64-i686-gcc."
}
if (-not (Test-Path $CmakeExe)) {
    throw "cmake.exe not found in $Ming32Bin. Run once without -BuildOnly so pacman can install mingw-w64-i686-cmake."
}

$dump = ((& $Gpp -dumpmachine) -join " ")
if ($dump -notmatch "^i686-w64-mingw32") {
    throw "Wrong compiler: $Gpp targets '$dump', need i686-w64-mingw32. Re-run without -BuildOnly."
}
Write-Step "Compiler: g++ ($dump)"
Write-Ok "cmake: $CmakeExe"

# ninja: probe, reinstall, fall back to the MSYS build - in that order.
if (-not (Test-ExeRuns $NinjaExe)) {
    Write-Warning "ninja.exe missing or not runnable at $NinjaExe - reinstalling mingw-w64-i686-ninja"
    & $Bash -lc "pacman -S --needed --noconfirm mingw-w64-i686-ninja"
}
if (-not (Test-ExeRuns $NinjaExe)) {
    Write-Warning "mingw32 ninja still not usable - trying the MSYS ninja (usr\bin)"
    & $Bash -lc "pacman -S --needed --noconfirm ninja"
    $NinjaExe = Join-Path $Msys2Root "usr\bin\ninja.exe"
}
if (-not (Test-ExeRuns $NinjaExe)) {
    throw @"
No usable ninja found (checked $Ming32Bin\ninja.exe and $Msys2Root\usr\bin\ninja.exe).
If the files exist but will not run, open Windows Security > Protection
history: Defender sometimes quarantines freshly unpacked exes. Add an
exclusion for $Msys2Root and the repo folder, then re-run.
"@
}
Write-Ok "ninja: $NinjaExe"

# ---------------------------------------------------------------------------
# ccache: on by default - but it must be the NATIVE WINDOWS ccache.exe, not
# the MSYS2 package. The msys ccache is a Cygwin program: is_full_path()
# treats only '/'-separated paths as absolute (the _WIN32 backslash branch
# is compiled out of it), so the backslashed "C:\msys64\mingw32\bin\cc.exe"
# cmake writes into build.ninja falls through to its PATH search and every
# compile fails with 'Could not find compiler ... in PATH'. The official
# native build (MSVC, _WIN32 defined) handles both separators. It installs
# to %LOCALAPPDATA%\ccache\bin so it survives MSYS2 updates.
# ---------------------------------------------------------------------------
$CcacheExe = Join-Path $env:LOCALAPPDATA "ccache\bin\ccache.exe"
$UseCcache = $false
if (-not $NoCcache) {
    if (-not (Test-ExeRuns $CcacheExe)) {
        $CcacheVer = "4.14.1"
        Write-Step "Downloading ccache $CcacheVer (native Windows build)"
        $url = "https://github.com/ccache/ccache/releases/download/v$CcacheVer/ccache-$CcacheVer-windows-x86_64.zip"
        $tmpZip = Join-Path $env:TEMP "ccache-$CcacheVer.zip"
        try {
            [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
            Invoke-WebRequest -Uri $url -OutFile $tmpZip -UseBasicParsing
            $tmpDir = Join-Path $env:TEMP "ccache-$CcacheVer-x"
            if (Test-Path $tmpDir) { Remove-Item $tmpDir -Recurse -Force }
            Expand-Archive -Path $tmpZip -DestinationPath $tmpDir -Force
            # The release zip nests ccache.exe inside a versioned subfolder.
            $exe = Get-ChildItem $tmpDir -Recurse -Filter ccache.exe | Select-Object -First 1
            if ($exe) {
                New-Item -ItemType Directory -Force -Path (Split-Path -Parent $CcacheExe) | Out-Null
                Copy-Item $exe.FullName $CcacheExe -Force
            } else {
                Write-Warning "ccache.exe not found inside the downloaded zip."
            }
            Remove-Item $tmpDir -Recurse -Force -ErrorAction SilentlyContinue
            Remove-Item $tmpZip -Force -ErrorAction SilentlyContinue
        } catch {
            Write-Warning "ccache download failed: $($_.Exception.Message)"
        }
    }
    if (Test-ExeRuns $CcacheExe) {
        $UseCcache = $true
        $CcacheDir = Join-Path $env:LOCALAPPDATA "ccache"
        # Persisted into ccache's own config file, not just this shell's env,
        # so manually invoked builds reuse the same cache and settings.
        & $CcacheExe -o "cache_dir=$CcacheDir" -o "max_size=10G" -o "sloppiness=pch_defines,time_macros,include_file_mtime,include_file_ctime" | Out-Null
        Write-Ok "ccache: $CcacheExe (cache: $CcacheDir, 10G, mtime-sloppy)"
    } else {
        Write-Warning "ccache unavailable - building without it. If the download succeeded"
        Write-Warning "but ccache.exe will not start, see Windows Security > Protection"
        Write-Warning "history; add an exclusion for $env:LOCALAPPDATA\ccache and re-run."
    }
}

if ($Jobs -le 0) { $Jobs = $env:NUMBER_OF_PROCESSORS }
if ($Jobs -le 0) { $Jobs = 4 }
$BuildDir = Join-Path $RepoRoot "build"

if ($Clean -and (Test-Path $BuildDir)) {
    Write-Step "Removing build\ (-Clean)"
    Remove-Item $BuildDir -Recurse -Force
}

# ---------------------------------------------------------------------------
# Cache guard. CMake caches every -D value and reuses it on re-configure. A
# CMAKE_TOOLCHAIN_FILE, a different generator, or a 64-bit compiler left in
# build\CMakeCache.txt by an EARLIER configure attempt would poison this
# native one ("Could not find toolchain file: cmake/mingw-i686").
# ---------------------------------------------------------------------------
$CacheFile = Join-Path $BuildDir "CMakeCache.txt"
if (-not $Clean -and (Test-Path $CacheFile)) {
    $tcLine  = Select-String -LiteralPath $CacheFile -Pattern '^CMAKE_TOOLCHAIN_FILE:[^=]*=(.+)$' | Select-Object -First 1
    $genLine = Select-String -LiteralPath $CacheFile -Pattern '^CMAKE_GENERATOR:[^=]*=(.+)$'      | Select-Object -First 1
    $ptrLine = Select-String -LiteralPath $CacheFile -Pattern '^CMAKE_SIZEOF_VOID_P:[^=]*=(.+)$'  | Select-Object -First 1

    $staleToolchain = $false
    $staleOther     = ""

    if ($tcLine) {
        $tcVal = $tcLine.Matches[0].Groups[1].Value.Trim()
        if ($tcVal) {
            $staleToolchain = $true
            Write-Warning "build\CMakeCache.txt carries CMAKE_TOOLCHAIN_FILE='$tcVal' from an earlier configure - the native build uses none."
        }
    }
    if ($genLine) {
        $genVal = $genLine.Matches[0].Groups[1].Value.Trim()
        if ($genVal -and $genVal -ne "Ninja") { $staleOther = "generator '$genVal' (this script builds with Ninja)" }
    }
    if ($ptrLine) {
        $ptrVal = $ptrLine.Matches[0].Groups[1].Value.Trim()
        if ($ptrVal -and $ptrVal -ne "4") { $staleOther = "a 64-bit compiler (SIZEOF_VOID_P=$ptrVal, need 32-bit)" }
    }

    if ($staleToolchain -or $staleOther) {
        if ($staleOther) { Write-Warning "build\CMakeCache.txt was configured with $staleOther." }
        Write-Warning "Clearing the CMake cache so the native configure starts fresh (compiled objects are kept)."
        Remove-Item -LiteralPath $CacheFile -Force
        $cacheDir = Join-Path $BuildDir "CMakeFiles"
        if (Test-Path $cacheDir) { Remove-Item -LiteralPath $cacheDir -Recurse -Force }
    }
}

# Lean/fat is baked into the compile flags: switching modes invalidates every
# object. Ninja handles the recompile; ccache keeps later switches cheap.
if (Test-Path $CacheFile) {
    $leanLine = Select-String -LiteralPath $CacheFile -Pattern '^REDIVIVUS_LEAN:BOOL=(ON|OFF)$' | Select-Object -First 1
    if ($leanLine) {
        $leanCached = ($leanLine.Matches[0].Groups[1].Value -eq "ON")
        if ($leanCached -eq $Fat) {
            $old = if ($Fat) { "lean" } else { "fat" }
            $new = if ($Fat) { "fat" } else { "lean" }
            Write-Warning "Build mode switched from $old to $new - the next build recompiles everything once."
        }
    }
}

# ---------------------------------------------------------------------------
# Configure - CI's configure minus the cross toolchain file, plus:
#   -UCMAKE_TOOLCHAIN_FILE  kills any stale toolchain entry the cache kept
#   -DCMAKE_MAKE_PROGRAM    ninja by full path - no PATH lookup involved
# ---------------------------------------------------------------------------
Write-Step "Configuring (CMake + Ninja, native i686)"
$configureArgs = @(
    "-S", $RepoRoot,
    "-B", $BuildDir,
    "-G", "Ninja",
    "-DCMAKE_MAKE_PROGRAM=$NinjaExe",
    "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
)
if (-not $Fat) { $configureArgs += "-DREDIVIVUS_LEAN=ON" }
else           { $configureArgs += "-UREDIVIVUS_LEAN" }   # clear a cached ON from a lean run
if ($UseCcache) {
    $configureArgs += "-DCMAKE_C_COMPILER_LAUNCHER=$CcacheExe"
    $configureArgs += "-DCMAKE_CXX_COMPILER_LAUNCHER=$CcacheExe"
} else {
    $configureArgs += "-UCMAKE_C_COMPILER_LAUNCHER"
    $configureArgs += "-UCMAKE_CXX_COMPILER_LAUNCHER"
}
if (Test-Path $CacheFile) { $configureArgs += "-UCMAKE_TOOLCHAIN_FILE" }
& $CmakeExe @configureArgs
if ($LASTEXITCODE -ne 0) { throw "cmake configure failed (exit $LASTEXITCODE)." }

$modeText = if ($Fat) { "fat: unoptimised + DWARF - the long step" } else { "lean: no DWARF" }
Write-Step "Building d2.exe with -j $Jobs ($modeText)"
if ($UseCcache) { & $CcacheExe -z | Out-Null }
$sw = [System.Diagnostics.Stopwatch]::StartNew()
& $CmakeExe --build $BuildDir -j $Jobs
if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Host "Compile errors above. To resume: open 'MSYS2 MINGW32' from the Start menu, cd to the repo, run:  cmake --build build -j $Jobs" -ForegroundColor Yellow
    throw "build failed (exit $LASTEXITCODE)."
}
$sw.Stop()
Write-Ok ("build finished in {0}" -f $sw.Elapsed.ToString("hh\:mm\:ss"))
if ($UseCcache) { & $CcacheExe -s }

# ---------------------------------------------------------------------------
# Gates - the same checks CI runs before shipping an artifact
# ---------------------------------------------------------------------------
$Exe = Join-Path $BuildDir "d2.exe"
if (-not (Test-Path $Exe)) { throw "build\d2.exe was not produced." }

Write-Step "Gates"
$fmt = (& (Join-Path $Ming32Bin "objdump.exe") -f $Exe) | Out-String
if ($fmt -notmatch "pei-i386") { throw "d2.exe is NOT a 32-bit PE (pei-i386) - do not run it." }
Write-Ok ("d2.exe is a 32-bit PE ({0:N0} bytes)" -f (Get-Item $Exe).Length)

$Map = Join-Path $BuildDir "d2exe.map"
if (Test-Path $Map) { Write-Ok ("d2exe.map written ({0:N0} bytes) - crash addresses resolve against it" -f (Get-Item $Map).Length) }
else { Write-Warning "d2exe.map missing - crash triage will not symbolise." }
if (-not $Fat) {
    Write-Ok "lean build (no DWARF): in-game crash reports print module+0xOFFSET - resolve against d2exe.map"
}

# ---------------------------------------------------------------------------
# Stripped artifact (CI's d2s-<sha>.exe equivalent)
# ---------------------------------------------------------------------------
if (-not $SkipStrip) {
    $id = ""
    try {
        $id = (& git -C $RepoRoot describe --always --tags --dirty)
        if ($LASTEXITCODE -ne 0) { $id = "" }
    } catch { $id = "" }
    if (-not $id) { $id = "local" }
    $d2s = Join-Path $BuildDir ("d2s-{0}.exe" -f $id)
    Copy-Item $Exe $d2s -Force
    & (Join-Path $Ming32Bin "strip.exe") $d2s
    if ($LASTEXITCODE -ne 0) { Write-Warning "strip failed - only the fat exe is available." }
    else { Write-Ok ("stripped artifact: {0} ({1:N0} bytes)" -f $d2s, (Get-Item $d2s).Length) }
}

# ---------------------------------------------------------------------------
# Optional deploy: d2.exe goes NEXT TO the original Game.exe (never replaces it)
# ---------------------------------------------------------------------------
if ($DeployTo) {
    if (-not (Test-Path $DeployTo)) { throw "-DeployTo '$DeployTo' does not exist." }
    $DeployTo = (Resolve-Path $DeployTo).Path
    Write-Step "Deploying to $DeployTo"
    $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
    foreach ($f in @("d2.exe", "d2exe.map")) {
        $src = Join-Path $BuildDir $f
        if (-not (Test-Path $src)) { continue }
        $dst = Join-Path $DeployTo $f
        $bak = "$dst.bak-$stamp"
        $hadOld = Test-Path $dst
        if ($hadOld) { Copy-Item $dst $bak -Force }
        Copy-Item $src $dst -Force
        if ($hadOld) { Write-Ok "copied $f (previous saved as $(Split-Path -Leaf $bak))" }
        else { Write-Ok "copied $f" }
    }
}

# ---------------------------------------------------------------------------
Write-Host ""
Write-Step "Done"
if ($Fat) { Write-Host "  build\d2.exe       fat exe - unoptimised + DWARF, the charter default" }
else      { Write-Host "  build\d2.exe       lean exe - no DWARF, quickest build; crash triage via d2exe.map" }
if (-not $SkipStrip) { Write-Host "  build\d2s-<id>.exe stripped copy for play/distribution" }
Write-Host "  build\d2exe.map    linker map - resolve crash 'module+0xOFFSET' addresses"
Write-Host ""
Write-Host "Play: copy build\d2.exe next to Game.exe in a 1.14d install, run  d2.exe -w" -ForegroundColor White
Write-Host "      (-DeployTo does the copy; the folder also needs the original MPQs and" -ForegroundColor White
Write-Host "       binkw32/smackw32/ijl11 DLLs from that install, as always)." -ForegroundColor White
Write-Host ""
Write-Host "Tip: the first build is slower while Windows Defender scans every new .obj." -ForegroundColor DarkGray
Write-Host "     Excluding the repo folder (and $Msys2Root) from real-time scanning" -ForegroundColor DarkGray
Write-Host "     speeds up all later builds considerably." -ForegroundColor DarkGray
