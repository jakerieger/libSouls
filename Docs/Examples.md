# Examples

Task-oriented recipes. If you haven't yet, read the [Quickstart](Quickstart.md) first for how to link the library and
how the API is shaped. Every snippet here was compiled against the library; paths are placeholders for your own game
folders. All of them assume:

```cpp
#include <filesystem>
#include <fstream>

using namespace Souls;
namespace fs = std::filesystem;
```

Mod your own copy of the game, not the Steam install: unpack the files you need (UXM/Nuxe for the newer games) and
keep backups.

- [Look inside an archive](#look-inside-an-archive)
- [Extract every file from an archive](#extract-every-file-from-an-archive)
- [Edit text (FMG)](#edit-text-fmg)
- [Edit game parameters (PARAM)](#edit-game-parameters-param)
- [Extract textures (TPF)](#extract-textures-tpf)
- [Read and edit a map (MSB)](#read-and-edit-a-map-msb)
- [Inspect a model (FLVER)](#inspect-a-model-flver)
- [Compress and decompress (DCX)](#compress-and-decompress-dcx)
- [Work out what a file is](#work-out-what-a-file-is)
- [Build a new archive](#build-a-new-archive)

## Look inside an archive

Archives ("binders") bundle many files. BND3 is used by Dark Souls and Dark Souls II; BND4 by Bloodborne and everything
after, and by `regulation.bin`. Both give you the same `Files` list, and each file is already decompressed.

```cpp
#include <libSouls/Binders/BND4.hpp>

BND4 Bnd = BND4::Read("C:/Games/ELDEN RING/Game/msg/engus/item.msgbnd.dcx");
for (const BinderFile& File : Bnd.Files) {
    std::printf("%6d  %8zu bytes  %s\n", File.ID, File.Bytes.size(), File.Name.value_or("<unnamed>").c_str());
}
```

Names are `std::optional` because some binders don't store them.

## Extract every file from an archive

```cpp
#include <libSouls/Binders/BND3.hpp>

BND3 Bnd = BND3::Read("C:/Games/DARK SOULS REMASTERED/chr/c2240.chrbnd.dcx");
const fs::path Out = "extracted";
for (const BinderFile& File : Bnd.Files) {
    if (!File.Name) continue;
    // Names are full build-machine paths such as N:\FRPG\data\...; keep just the file name.
    const fs::path Target = Out / fs::path(*File.Name).filename();
    fs::create_directories(Target.parent_path());
    std::ofstream(Target, std::ios::binary)
        .write(reinterpret_cast<const char*>(File.Bytes.data()), static_cast<std::streamsize>(File.Bytes.size()));
}
```

## Edit text (FMG)

Text lives in `.fmg` files inside `.msgbnd` archives. Read the archive, change the FMG, put its bytes back into the
`BinderFile`, and write the archive. Compression is kept as it was.

```cpp
#include <libSouls/Binders/BND4.hpp>
#include <libSouls/Formats/FMG.hpp>

bool EndsWith(const std::string& Text, const std::string& Suffix) {
    return Text.size() >= Suffix.size() && Text.compare(Text.size() - Suffix.size(), Suffix.size(), Suffix) == 0;
}

const fs::path Path = "C:/Games/ELDEN RING/Game/msg/engus/item.msgbnd.dcx";
BND4 Bnd = BND4::Read(Path);
for (BinderFile& File : Bnd.Files) {
    if (!File.Name || !EndsWith(*File.Name, "GoodsName.fmg")) continue;

    FMG Names = FMG::Read(File.Bytes);
    Names.SetText(1000, "Flask of Crimson Tears (Mine)");   // adds the entry if it doesn't exist
    File.Bytes = Names.Write();
}
Bnd.Write(Path);
```

Other useful `FMG` members: `GetText(ID)` (returns `std::optional`), `Find(ID)`, and `Entries`, a plain vector you can
iterate. Entries are written sorted by ID.

## Edit game parameters (PARAM)

A `PARAM` file is a table of rows, but the bytes of each row only mean something with a **PARAMDEF** that describes the
fields. libSouls doesn't ship any; get them from the community (for example the
[Paramdex](https://github.com/soulsmods/Paramdex) repository) and point a `ParamDefRepository` at the folder for your
game. Use one repository per game.

```cpp
#include <libSouls/Binders/Regulation.hpp>
#include <libSouls/Formats/ParamDefRepository.hpp>
#include <libSouls/Formats/ParamLayout.hpp>

ParamDefRepository Defs;
Defs.AddFolder("C:/Tools/Paramdex/ER/Defs");

const fs::path Path = "C:/Games/ELDEN RING/Game/regulation.bin";
BND4 Bnd = Regulation::DecryptER(Path);                 // regulation.bin is encrypted

for (BinderFile& File : Bnd.Files) {
    if (!File.Name || !EndsWith(*File.Name, "EquipParamWeapon.param")) continue;

    PARAM Param = PARAM::Read(File.Bytes);
    const PARAMDEF* Def = Defs.Find(Param);            // matches param type, data version and row size
    if (!Def) continue;                                // wrong or missing def for this game version
    const auto Layout = ParamLayout::TryCreate(Param, *Def);
    if (!Layout) continue;

    // Halve the weight of every weapon.
    for (PARAM::Row& Row : Param.Rows) {
        Layout->SetNumber(Row, "weight", Layout->GetNumber(Row, "weight") * 0.5);
    }

    // Typed access to a single row.
    if (PARAM::Row* Row = Param.Find(1000000)) {        // the Dagger
        Layout->Set(*Row, "weight", 1.0f);
        float Weight = Layout->Get<float>(*Row, "weight");
    }

    File.Bytes = Param.Write();
}
Regulation::EncryptER(Path, Bnd);
```

Fields are addressed by their *internal* names from the def. `Get<T>` throws if the field isn't of that type (see the
table in `ParamLayout.hpp`); `GetNumber`/`SetNumber` work on any numeric field. `Regulation` also has
`DecryptDS3`/`EncryptDS3`. Older games keep `.param` files in ordinary binders and need no decryption.

To add a row, `Layout->MakeRow(ID, "Name")` creates one with every field at its default, which you push onto
`Param.Rows`.

## Extract textures (TPF)

On PC, each texture in a TPF is a complete DDS file.

```cpp
#include <libSouls/Formats/TPF.hpp>

TPF Tpf = TPF::Read("C:/Games/DARK SOULS REMASTERED/menu/menu_0.tpf.dcx");
fs::create_directories("textures");
for (const TPF::Texture& Tex : Tpf.Textures) {
    std::ofstream("textures/" + Tex.Name + ".dds", std::ios::binary)
        .write(reinterpret_cast<const char*>(Tex.Bytes.data()), static_cast<std::streamsize>(Tex.Bytes.size()));
}
```

To replace one, set `Tex.Bytes` to a new DDS and write the TPF again; the type and mipmap count are refreshed from the
DDS header. To add one, use `Tpf.Textures.emplace_back(Name, Format, Flags1, DdsBytes)`.

## Read and edit a map (MSB)

Maps list every model, part (enemies, objects, collisions), region and event in a level. Each game has its own MSB
class: `MSB1` (Dark Souls), `MSB2` (Dark Souls II), `MSB3` (Dark Souls III) and `MSBS` (Sekiro). Entries refer to each
other by *name*, so renaming and reordering is safe: the indices the file needs are worked out on write.

```cpp
#include <libSouls/Formats/MSB/MSB1.hpp>

MSB1 Map = MSB1::Read("C:/Games/DARK SOULS REMASTERED/map/MapStudio/m10_00_00_00.msb");

for (MSB1::PartsParam::Enemy& Enemy : Map.Parts.Enemies) {
    std::printf("%-20s model %-8s at (%.1f, %.1f, %.1f)\n", Enemy.Name.c_str(), Enemy.ModelName.value_or("?").c_str(),
                Enemy.Position.X, Enemy.Position.Y, Enemy.Position.Z);
}

if (!Map.Parts.Enemies.empty()) {
    Map.Parts.Enemies[0].Position.Y += 5.f;             // lift the first enemy five meters
}
Map.Write("m10_00_00_00.msb");
```

Parts, models, events and regions are each split into one vector per type (`Parts.Enemies`, `Parts.Objects`,
`Models.MapPieces`, ...). `GetEntries()` on any of them returns every entry in the order it will be written, and `Add(...)`
puts a new entry in the right list. References such as `ModelName` are `std::optional<std::string>` holding the target's
name; make sure a model with that name exists in `Map.Models` before writing, or `Write` throws.

## Inspect a model (FLVER)

```cpp
#include <libSouls/Binders/BND3.hpp>
#include <libSouls/Formats/FLVER/FLVER2.hpp>

BND3 Bnd = BND3::Read("C:/Games/DARK SOULS REMASTERED/chr/c2240.chrbnd.dcx");
for (const BinderFile& File : Bnd.Files) {
    if (!File.Name || !EndsWith(*File.Name, ".flver")) continue;

    FLVER2 Model = FLVER2::Read(File.Bytes);
    for (const FLVER::Bone& Bone : Model.Bones) {
        std::printf("bone %s\n", Bone.Name.c_str());
    }
    for (const FLVER2::Mesh& Mesh : Model.Meshes) {
        std::printf("mesh: %zu vertices, material %s\n", Mesh.Vertices.size(),
                    Model.Materials[static_cast<size_t>(Mesh.MaterialIndex)].Name.c_str());
    }
}
```

Each `Mesh` has `Vertices` (positions, normals, UVs, bone weights...) and `FaceSets` for the triangle indices; each
`Material` lists its textures. Models in Dark Souls Remastered onwards are `FLVER2`.

## Compress and decompress (DCX)

You rarely need this, because the format classes handle it. For raw access:

```cpp
#include <libSouls/Formats/DCX.hpp>

std::vector<uint8_t> Raw = DCX::Decompress(fs::path("c2240.anibnd.dcx"));

DCX::Type Kind;                                         // also reports which compression it was
std::vector<uint8_t> Same = DCX::Decompress(fs::path("c2240.anibnd.dcx"), Kind);

std::vector<uint8_t> Packed = DCX::Compress(Raw, DCX::ToType(DCX::DefaultType::EldenRing));
```

To convert a file from one compression to another, read it, set `Compression`, and write it:

```cpp
BND4 Bnd = BND4::Read("in.bnd.dcx");
Bnd.Compression = DCX::ToType(DCX::DefaultType::EldenRing);
Bnd.Write("out.bnd.dcx");
```

## Work out what a file is

```cpp
void Identify(const fs::path& Path) {
    if (BND4::Is(Path))        std::puts("BND4 binder");
    else if (BND3::Is(Path))   std::puts("BND3 binder");
    else if (TPF::Is(Path))    std::puts("texture container");
    else if (FLVER2::Is(Path)) std::puts("model");
}
```

`Is` looks through DCX compression as well. Formats without a magic number (`FMG`, `PARAM`) can false-positive, so check
those last, or just try `Read` and catch `BinaryException`.

## Build a new archive

Formats are plain data, so creating a file means filling in the members:

```cpp
BND4 Bnd;                                               // sensible modern defaults
Bnd.Files.emplace_back(Binder::FileFlags::Flag1, 0, std::string("my/file.txt"), std::vector<uint8_t>{'h', 'i'});
Bnd.Compression = DCX::Type::None;
Bnd.Write("new.bnd");
```

The `BinderFile` constructor arguments are the flags, an ID, a name and the bytes; leave out the ID or the name when the
binder format doesn't use them.
