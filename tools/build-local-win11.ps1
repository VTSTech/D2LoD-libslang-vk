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

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File tools\win11-build.ps1
    powershell -ExecutionPolicy Bypass -File tools\win11-build.ps1 -Clean -DeployTo "D:\Games\Diablo II"
    powershell -ExecutionPolicy Bypass -File tools\win11-build.ps1 -SetupOnly
#>
param(
    # Repo root. Default: parent of the folder containing this script (tools/).
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
    # Copy build\d2.exe (+ d2exe.map) into this directory (a 1.14d install).
    [string]$DeployTo = ""
)

$ErrorActionPreference = "Stop"

function Write-Step { param($m) Write-Host ""; Write-Host "==> $m" -ForegroundColor Cyan }
function Write-Ok   { param($m) Write-Host "    $m" -ForegroundColor Green }

# ---------------------------------------------------------------------------
# Repo root
# ---------------------------------------------------------------------------
if (-not $RepoRoot) { $RepoRoot = Split-Path -Parent $PSScriptRoot }
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
# Build environment (MINGW32 first on PATH)
# ---------------------------------------------------------------------------
$Ming32Bin = Join-Path $Msys2Root "mingw32\bin"
$Gpp       = Join-Path $Ming32Bin "g++.exe"
if (-not (Test-Path $Gpp)) {
    throw "g++.exe not found in $Ming32Bin. Run once without -BuildOnly so pacman can install mingw-w64-i686-gcc."
}
$env:PATH = "$Ming32Bin;$Msys2Root\usr\bin;$env:PATH"

$dump = ((& $Gpp -dumpmachine) -join " ")
if ($dump -notmatch "^i686-w64-mingw32") {
    throw "Wrong compiler: $Gpp targets '$dump', need i686-w64-mingw32. Re-run without -BuildOnly."
}
Write-Step "Compiler: g++ ($dump)"

if ($Jobs -le 0) { $Jobs = $env:NUMBER_OF_PROCESSORS }
if ($Jobs -le 0) { $Jobs = 4 }
$BuildDir = Join-Path $RepoRoot "build"

if ($Clean -and (Test-Path $BuildDir)) {
    Write-Step "Removing build\ (-Clean)"
    Remove-Item $BuildDir -Recurse -Force
}

Write-Step "Configuring (CMake + Ninja, native i686 - CI's configure minus the cross toolchain file)"
& cmake -S $RepoRoot -B $BuildDir -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
if ($LASTEXITCODE -ne 0) { throw "cmake configure failed (exit $LASTEXITCODE)." }

Write-Step "Building d2.exe with -j $Jobs (unoptimised + DWARF - the long step)"
$sw = [System.Diagnostics.Stopwatch]::StartNew()
& cmake --build $BuildDir -j $Jobs
if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Host "Compile errors above. To resume: open 'MSYS2 MINGW32' from the Start menu, cd to the repo, run:  cmake --build build -j $Jobs" -ForegroundColor Yellow
    throw "build failed (exit $LASTEXITCODE)."
}
$sw.Stop()
Write-Ok ("build finished in {0}" -f $sw.Elapsed.ToString("hh\:mm\:ss"))

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

# ---------------------------------------------------------------------------
# Stripped artifact (CI's d2s-<sha>.exe equivalent)
# ---------------------------------------------------------------------------
if (-not $SkipStrip) {
    $id = ""
    try {
        $id = (& git -C $RepoRoot rev-parse --short HEAD)
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
Write-Host "  build\d2.exe       fat exe - unoptimised + DWARF, the charter default"
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
