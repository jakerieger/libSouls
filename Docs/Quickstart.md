# Quickstart

This guide gets you from a fresh clone to reading, changing and writing a FromSoftware file. For longer, task-oriented
recipes see [Examples.md](Examples.md).

## 1. Build it

Follow the build steps in the [README](../README.md#building). When you're done you have `Souls.dll` (and its import
library) in `build/<CONFIG>/bin`.

## 2. Use it in your project

libSouls builds as a CMake subproject. Add it and link the `Souls` target; the include path comes with it, so headers
are included as `<libSouls/...>`:

```cmake
add_subdirectory(extern/libSouls)

add_executable(MyTool main.cpp)
target_link_libraries(MyTool PRIVATE Souls)
target_compile_features(MyTool PRIVATE cxx_std_20)
```

libSouls is Windows-only and needs MSVC. See the [README](../README.md#building) for the toolchain.

Then put these next to your executable:

- `Souls.dll`, which CMake builds into the same `bin` folder as the library.
- `oo2core_6_win64.dll`, copied from whichever FromSoftware game you are modding. Most modern game files are
  Oodle-compressed, so without it they can't be read or written. You can instead point the library at it from code:

  ```cpp
  #include <libSouls/Oodle26.hpp>

  Souls::Oodle26::SetDLLPath("C:/Games/ELDEN RING/Game/oo2core_6_win64.dll");
  ```

  Call that once, before reading or writing any Oodle-compressed file.

## 3. The shape of the API

Everything lives in the `Souls` namespace. Every file format has a class named after it (`FMG`, `PARAM`, `TPF`,
`FLVER2`, `MSB1`, `BND4`, ...), and they all work the same way.

### Reading and writing

```cpp
#include <libSouls/Formats/FMG.hpp>

using namespace Souls;

FMG Text = FMG::Read("GoodsName.fmg");       // from a path...
FMG Same = FMG::Read(SomeBytes);             // ...or from bytes in memory (std::span<const uint8_t>)

Text.SetText(1000, "Estus Flask");

Text.Write("GoodsName.fmg");                 // to a path...
std::vector<uint8_t> Out = Text.Write();     // ...or to bytes
```

The static `Is` and `IsRead` functions help when you don't know what a file is:

```cpp
if (FMG::Is("something.bin")) { /* looks like an FMG */ }
if (std::optional<FMG> Maybe = FMG::IsRead("something.bin")) { /* it was, and here it is */ }
```

`Is` is exact for formats that start with a magic number (`BND4`, `TPF`, `MSB`...). A few formats (`FMG`, `PARAM`)
have no magic, so for them it is only a structural check.

### Compression is automatic

Game files are usually wrapped in DCX compression. `Read` unwraps it for you and remembers which kind it was in the
`Compression` member, so `Write` puts it back the way it came. To change it, set the member first:

```cpp
#include <libSouls/Formats/DCX.hpp>

Text.Compression = DCX::Type::DCX_KRAK_6;   // what Sekiro and Elden Ring use
Text.Compression = DCX::Type::None;         // write it bare
```

`DCX::ToType(DCX::DefaultType::EldenRing)` gives the usual type for each game.

### Text is UTF-8

Strings in the files are UTF-16 or Shift-JIS. libSouls converts them, so every `std::string` you see or give it is
UTF-8. Entries that may legitimately have no name or text are `std::optional<std::string>`.

### Errors are exceptions

Malformed, truncated or unsupported data throws `Souls::BinaryException` (a `std::runtime_error`). Parsers are strict:
they check the constants real files contain, so a failure usually means the file is a variant that isn't supported yet.

```cpp
try {
    FMG Text = FMG::Read(Path);
} catch (const BinaryException& E) {
    std::fprintf(stderr, "%s\n", E.what());
}
```

### Files you read are plain data

Formats are plain structs and `std::vector`s. There is nothing to close or free; edit the members and call `Write`.
Reading a file and writing it straight back reproduces it exactly for the formats that have been verified against real
game files.

## 4. A complete first program

This builds a small text file from scratch, prints it, and writes it out. To edit the real game text, see
[Edit text](Examples.md#edit-text-fmg).

```cpp
#include <libSouls/Formats/FMG.hpp>

#include <cstdio>

using namespace Souls;

int main() {
    FMG Names;                          // an empty FMG, DS1/DS2 layout
    Names.SetText(100, "Broken Straight Sword");
    Names.SetText(101, "Straight Sword Hilt");

    for (const FMG::Entry& Entry : Names.Entries) {
        std::printf("%d: %s\n", Entry.ID, Entry.Text.value_or("<null>").c_str());
    }

    Names.Write("WeaponName.fmg");

    try {
        FMG Back = FMG::Read("WeaponName.fmg");
        std::printf("read back %zu entries\n", Back.Entries.size());
    } catch (const BinaryException& E) {
        std::printf("failed: %s\n", E.what());
        return 1;
    }
    return 0;
}
```

## 5. Where things are

| Kind of file | Header |
|---|---|
| Archives: `.bnd`, `.bdt`/`.bhd`, `regulation.bin` | `Binders/BND3.hpp`, `BND4.hpp`, `BXF3.hpp`, `BXF4.hpp`, `Regulation.hpp`; `Formats/BHD5.hpp` |
| Text | `Formats/FMG.hpp` |
| Parameters | `Formats/PARAM.hpp`, `PARAMDEF.hpp`, `ParamDefRepository.hpp`, `ParamLayout.hpp` |
| Textures | `Formats/TPF.hpp`, `DDS.hpp` |
| Models | `Formats/FLVER/FLVER2.hpp` |
| Maps | `Formats/MSB/MSB1.hpp`, `MSB2.hpp`, `MSB3.hpp`, `MSBS.hpp` |
| Animation events, cutscenes, effects | `Formats/TAE3.hpp`, `MQB.hpp`, `FFXDLSE.hpp`, `FXR3.hpp` |
| Compression | `Formats/DCX.hpp` |

Which game uses which format version is listed in each header. Elden Ring's newer layouts of some formats (maps,
GPARAM, NVA, FXR, TAE) aren't supported yet.

Next: [Examples.md](Examples.md).
