# Building and running Redivivus

Diablo II 1.14d shipped as two builds of the same source, each from its own compiler:

|Build|Compiler|Identified by|
|-|-|-|
|Windows `Game.exe`|Visual C++ 2005 SP1 (cl 14.00.50727.762), CRT linked statically|Rich header, then the linked CRT read function by function|
|Intel Mac `DiabloII_macho`|Apple LLVM 4.2 (clang-425.0.28, Xcode 4.6.2), i386, `-Os`, libc++|Load commands and the SDK, then reproduced: recompiled functions come out byte-identical|

The goal is that the tree builds with those, the original compilers, and with modern ones.
Where that stands:

|Compiler|What it does with the tree today|
|-|-|
|mingw-w64 g++ (C++17)|Builds and links the whole tree into a runnable `d2.exe`. This is the build below.|
|Visual C++ 2005 SP1|Compiles the tree translation unit by translation unit, and every function is compared byte for byte with the shipped `Game.exe`: [asm_matching/msvc8](../asm_matching/msvc8/README.md). It does not link an executable yet.|
|Apple LLVM 4.2|Identified and reproduced on single functions; the tree does not build for the Mac yet.|

## You need

- Your own installation of Diablo II: Lord of Destruction, version 1.14. This repository
  contains no game data and no Blizzard binaries; the executable reads the MPQ archives
  from your installation. If you have no installation at hand,
  [blizzard-legacy-dl](https://github.com/jaenster/blizzard-legacy-dl) downloads the
  installers from Blizzard's own servers:

  ```sh
  blizzard-legacy-dl fetch D2DV -o ./d2     # Diablo II
  blizzard-legacy-dl fetch D2XP -o ./d2     # Lord of Destruction
  ```

  You still need your own CD keys to install.
- To build: a mingw-w64 cross compiler for 32-bit Windows (`i686-w64-mingw32-g++`), CMake
  and Ninja.
  - macOS: `brew install mingw-w64 cmake ninja`
  - Debian/Ubuntu: `apt install g++-mingw-w64-i686 cmake ninja-build`
  - Windows: an MSYS2 MinGW32 shell

## Build with mingw-w64

```sh
cmake -S . -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-i686.cmake
cmake --build build
```

That produces `build/d2.exe`, a 32-bit Windows executable. `tools/check-build.sh` does
the same from a clean clone and checks the result; CI runs it on every push.

## Run

Copy `d2.exe` into your Diablo II folder, next to the original `Game.exe`, and
start it from there:

```
d2.exe -w
```

`-w` runs it in a window.

## Build with Visual C++ 2005 SP1

The original Windows compiler runs under wine, on Linux and on macOS, without a Windows machine
and without running Microsoft's installer:

```sh
asm_matching/msvc8/oracle setup     # once: Game.exe 1.14d from Blizzard's patch, and VC8 from Microsoft's packages
asm_matching/msvc8/oracle all       # compile every translation unit, pair the functions, compare
asm_matching/msvc8/oracle diff <function>
```

The full guide, what `setup` downloads and how the comparison works:
[asm_matching/msvc8/README.md](../asm_matching/msvc8/README.md).
