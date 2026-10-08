# libSouls

**libSouls** is a C++ library for working with and modifying FromSoftware's Souls games (Dark Souls, ELDEN RING, Sekiro,
etc.). It is partially a port of the renowned [SoulsFormats](https://github.com/JKAnderson/SoulsFormats) C# library with
additions for other functionality beyond
modifying game file formats. It aims to be easily integrable, extensible, and to open the door for language bindings
such that mods can theoretically be written in any language one desires without redoing the ground work already laid by
the wonderful Souls modding community.

## Why C++?

SoulsFormats is the foundation of the Souls modding scene, and its quality is the reason so many tools exist. But
because
it is a .NET library, the tools built on it (Smithbox, DSMapStudio, DarkScript3, etc.) are tied to the .NET ecosystem,
and anyone working in another language has to rewrite years of format knowledge from scratch. libSouls is a native
implementation of the same formats, meant to **complement** the existing tools rather than replace them.

A native library with a stable C interface (planned) offers things a managed library can't easily match:

- **Any language can use it.** Python, Rust, Lua, C#, and others can call it directly, without hosting a .NET runtime in
  the process.
- **It works inside the game.** Many mods are native DLLs injected into the game process. These can use libSouls to read
  and write the formats in-process, which isn't practical with a managed library.
- **It's easy to ship.** There is no runtime to install, and it is a small dependency for native tools, including ones
  that use C++ libraries like ImGui, DirectX, or Vulkan directly instead of through bindings.

*Why not compile SoulsFormats with .NET NativeAOT and export a C interface instead?* That is a reasonable route, but it
keeps a managed codebase (with its own runtime, garbage collector, and debugging story) behind a native facade. A native
codebase is simpler to reason about, profile, and debug for the in-process and cross-language cases above. Performance
is a side benefit rather than the goal, and any claims about it will be backed by benchmarks as they become available.

#### The format knowledge in SoulsFormats took years of community work to build. libSouls aims to make that work available in more places.

## Current State

**libSouls** is in **VERY** early development (read: unusable). The current trajectory is porting the SoulsFormat
library in its
entirety to C++ and move on to supporting other functionality from there.

### SoulsFormats Port Progress

> *Verified via end-to-end testing (see [Testsuite](Source/Testsuite)).*

- [x] Oodle26 DLL bindings (see [Oodle26.hpp](Source/libSouls/Oodle26.hpp))
- [x] **DCX** Format (see [DCX.hpp](Source/libSouls/Formats/DCX.hpp))
- [ ] **DRB** Format
- [ ] **EMEVD** Format
- [ ] **FFXDLSE** Format
- [ ] **FLVER** Format
- [ ] **MQB** Format
- [ ] **MSB** Format
- [x] **PARAM** Format (see [PARAM.hpp](Source/libSouls/Formats/PARAM.hpp))
- [ ] **TAE3** Format
- [x] **TPF** Format (see [TPF.hpp](Source/libSouls/Formats/TPF.hpp))
- [x] **BHD5** Format (see [BHD5.hpp](Source/libSouls/Formats/BHD5.hpp))
- [x] **BND3** Binder (see [BND3.hpp](Source/libSouls/Binders/BND3.hpp))
- [x] **BND4** Binder (see [BND4.hpp](Source/libSouls/Binders/BND4.hpp))
- [x] **BXF3** Binder (see [BXF3.hpp](Source/libSouls/Binders/BXF3.hpp))
- [x] **BXF4** Binder (see [BXF4.hpp](Source/libSouls/Binders/BXF4.hpp))

## Building

Before building libSouls, ensure your local development environment meets the following requirements:

- MSVC with Windows 11 SDK (*Linux not supported*)
- CMake (*>= v3.26*)
- Internet connection (*required to fetch zlib dependency on first configure*)
- Ninja Build (*not required but HIGHLY recommended*)

> [!IMPORTANT]
> Execute the following commands from within a **VS Developer PowerShell**. Failure to do so will result in
CMake failing to locate the correct compiler and the configuration/build will fail. Support for GCC or Clang is not
tested nor supported.

### 1. Clone the repository

```shell
git clone https://github.com/jakerieger/libSouls
```

### 2. Configure CMake

```shell
cd libSouls

cmake -B build/Debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
# OR
cmake -B build/Release -G Ninja -DCMAKE_BUILD_TYPE=Release
```

### 3. Build libSouls

```shell
cmake --build build/<CONFIG>
```

### 4. Run tests

Some tests use real game files in order to validate file format implementations. If you want to run the full testsuite,
you'll need a valid Steam install of both Dark Souls Remastered and ELDEN RING with ELDEN RING's content unpacked via
UXM or Nuxe. A select number of random game files are pulled and tested against.

```shell
ctest --test-dir build/<CONFIG> --output-on-failure
```

The final `Souls.dll` library can be found in `build/<CONFIG>/bin`. libSouls currently does not support static linking.

> [!NOTE]
> `oo2core_6_win64.dll` needs to exist in the same path as `Souls.dll` and whatever executable is utilizing this
library. It can be copied from whichever From game you are modding (DS1/2/3, ELDEN RING, Sekiro, etc.).
>
> It contains the proprietary [Oodle](https://www.radgametools.com/oodle.htm) codec used by the game engine and without
> it, none of the game file formats can be read, modified, or written to.
>
> **Optionally**, your project can tell libSouls where to find the DLL instead, by calling `Oodle26::SetDLLPath` once
> before any Oodle-compressed file is read or written (the first use loads the DLL):
> ```cpp
> #include <libSouls/Oodle26.hpp>
>
> Souls::Oodle26::SetDLLPath("Path/To/oo2core_6_win64.dll");
> ```
> It returns `false` (and changes nothing) if the DLL has already been loaded. If an earlier attempt failed because the
> path was wrong, you can call it again with the corrected path. `Oodle26::IsAvailable()` reports whether the DLL could
> be loaded.

## Contributing

Contributions are more than welcome. I'm one guy with limited free time and any help is greatly appreciated.

> [!IMPORTANT]
> If you'd like to contribute, **please submit an issue prior to submitting a pull request so we can discuss an
implementation plan.**

## License

libSouls is licensed under the [ISC license](LICENSE).