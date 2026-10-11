# C API

libSouls has a flat C interface so it can be used from languages other than C++: Python (`ctypes`/`cffi`), C#
(P/Invoke), Rust, Lua, and so on. It lives in [`Source/libSouls/CApi/souls.h`](../Source/libSouls/CApi/souls.h), is plain
C99, and ships in the same `Souls.dll` as the C++ API. Include it as `<libSouls/CApi/souls.h>`.

What it covers:

| Area | What it gives you |
|---|---|
| Core | errors, byte buffers, Oodle setup, reading a file |
| DCX | compress, decompress, detect |
| Binders | BND3/BND4 and the split BXF3/BXF4 read, write, edit files; `regulation.bin` decrypt and encrypt (Dark Souls III, Elden Ring); a streaming reader for huge BXF pairs |
| Game archives | BHD5 headers (`Data0.bhd`, ...) with RSA decryption from the game exe, lookup by path or hash, file extraction |
| FMG | read, write, get/set text by ID |
| PARAM | rows; paramdef sets; layouts for reading and writing fields by name |
| TPF | read, write, get/replace/add textures |
| MSB | maps for Dark Souls 1/2/3, Sekiro and Elden Ring: list, add, remove and edit parts, models, events and regions |
| FLVER | models: bones, dummies, materials, textures, and bulk vertex and face arrays |
| EMEVD | event scripts: events, instructions, argument packing, parameters |
| TAE3 | animation events by field name (Dark Souls III layout) |

The remaining small formats are still C++-only for now and will follow the same conventions.

## Conventions

**Handles.** Every object is an opaque pointer (`SoulsFMG*`, `SoulsBinder*`, ...) you create with a `_new` or `_read_*`
function and release with the matching `_free`. Freeing `NULL` is fine.

**Errors.** Functions that create something return `NULL` on failure; the rest return a `SoulsStatus` (`SOULS_OK` is
`0`). After a failure, `souls_last_error()` gives a human-readable message and `souls_last_error_code()` gives the kind
(`SOULS_ERR_INVALID_ARGUMENT`, `_IO`, `_PARSE`, `_NOT_FOUND`, `_INTERNAL`). Both are per thread and are only updated by
failures. C++ exceptions never cross the boundary.

**Strings** are UTF-8 and NUL-terminated. A returned `const char*` is *borrowed*: it stays valid until you modify or free
the object it came from, so copy it if you need it longer. Optional text is `NULL` when absent, and setters take `NULL`
to clear it.

**Bytes** going in are a pointer and a size. Bytes coming out are either a `SoulsBuffer` that you own and release with
`souls_buffer_free`, or (for data that lives inside an object, such as a binder's files) a borrowed pointer plus a size.

**Indices** are zero-based `size_t`; a bad index fails with `SOULS_ERR_INVALID_ARGUMENT`. Functions that search return a
signed index, with `-1` for "not found".

**Threads.** Don't use one object from two threads at once. Different objects are independent.

## Setup

Link against `Souls`. From CMake, with libSouls as a subproject:

```cmake
add_subdirectory(extern/libSouls)
target_link_libraries(MyTool PRIVATE Souls)      # also adds the include path
```

Put `Souls.dll` next to your program, and `oo2core_6_win64.dll` (from a FromSoftware game) next to it as well, or call
`souls_oodle_set_dll_path()` before the first Oodle-compressed file is touched.

## Example: change an item name (C)

Read a binder, find an FMG inside it, change a string, and write everything back:

```c
#include <libSouls/CApi/souls.h>

#include <stdio.h>

int main(void) {
    const char* path = "C:/Games/ELDEN RING/Game/msg/engus/item.msgbnd.dcx";

    SoulsBinder* bnd = souls_binder_read_file(path);
    if (!bnd) {
        fprintf(stderr, "read failed: %s\n", souls_last_error());
        return 1;
    }

    int64_t index = souls_binder_find_file_suffix(bnd, "GoodsName.fmg");
    if (index < 0) {
        fprintf(stderr, "no GoodsName.fmg in the binder\n");
        souls_binder_free(bnd);
        return 1;
    }

    size_t size = 0;
    const uint8_t* bytes = souls_binder_file_bytes(bnd, (size_t)index, &size);
    SoulsFMG* fmg = souls_fmg_read_memory(bytes, size);
    if (!fmg) {
        fprintf(stderr, "not an FMG: %s\n", souls_last_error());
        souls_binder_free(bnd);
        return 1;
    }

    printf("before: %s\n", souls_fmg_get_text(fmg, 1000));
    souls_fmg_set_text(fmg, 1000, "Flask of Crimson Tears (Mine)");

    SoulsBuffer edited = souls_fmg_write_memory(fmg);
    souls_binder_set_file_bytes(bnd, (size_t)index, edited.data, edited.size);
    souls_buffer_free(edited);

    if (souls_binder_write_file(bnd, path) != SOULS_OK) {
        fprintf(stderr, "write failed: %s\n", souls_last_error());
    }

    souls_fmg_free(fmg);
    souls_binder_free(bnd);
    return 0;
}
```

`souls_fmg_get_text` returns `NULL` for an ID that doesn't exist, so check it before printing in real code.

## Example: read a game parameter (Python)

This decrypts Elden Ring's `regulation.bin`, loads the community paramdefs for it, and reads a weapon's weight by field
name. It uses only the standard library. Set the first command-line argument to the built `Souls.dll`.

```python
import ctypes as C
import os
import sys

lib = C.CDLL(sys.argv[1])


class Buffer(C.Structure):
    _fields_ = [("data", C.POINTER(C.c_uint8)), ("size", C.c_size_t)]


def sig(name, restype, *argtypes):
    fn = getattr(lib, name)
    fn.restype = restype
    fn.argtypes = argtypes
    return fn


last_error = sig("souls_last_error", C.c_char_p)

binder_find_suffix = sig("souls_binder_find_file_suffix", C.c_int64, C.c_void_p, C.c_char_p)
binder_bytes = sig("souls_binder_file_bytes", C.POINTER(C.c_uint8), C.c_void_p, C.c_size_t, C.POINTER(C.c_size_t))
binder_free = sig("souls_binder_free", None, C.c_void_p)
regulation_decrypt = sig("souls_regulation_decrypt", C.c_void_p, C.c_int, C.c_char_p)

param_read = sig("souls_param_read_memory", C.c_void_p, C.POINTER(C.c_uint8), C.c_size_t)
param_find_row = sig("souls_param_find_row", C.c_int64, C.c_void_p, C.c_int32)
param_free = sig("souls_param_free", None, C.c_void_p)

defs_new = sig("souls_paramdefs_new", C.c_void_p)
defs_add_folder = sig("souls_paramdefs_add_folder", C.c_int, C.c_void_p, C.c_char_p, C.c_int)
defs_free = sig("souls_paramdefs_free", None, C.c_void_p)

layout_create = sig("souls_param_layout_create", C.c_void_p, C.c_void_p, C.c_void_p)
layout_free = sig("souls_param_layout_free", None, C.c_void_p)
get_number = sig("souls_param_get_number", C.c_int, C.c_void_p, C.c_void_p, C.c_size_t, C.c_char_p,
                 C.POINTER(C.c_double))

SOULS_GAME_ELDEN_RING = 6

game = r"C:\Games\ELDEN RING\Game"
paramdex = r"libSouls\Vendor\Paramdex\ER\Defs"  # or any Paramdex checkout

binder = regulation_decrypt(SOULS_GAME_ELDEN_RING, (game + r"\regulation.bin").encode())
if not binder:
    sys.exit("decrypt failed: " + last_error().decode())

index = binder_find_suffix(binder, b"EquipParamWeapon.param")
size = C.c_size_t()
data = binder_bytes(binder, index, C.byref(size))
param = param_read(data, size.value)          # copies what it needs; the binder can stay alive meanwhile

defs = defs_new()
defs_add_folder(defs, paramdex.encode(), 1)
layout = layout_create(param, defs)
if not layout:
    sys.exit("no layout: " + last_error().decode())

row = param_find_row(param, 1000000)           # the Dagger
weight = C.c_double()
if get_number(layout, param, row, b"weight", C.byref(weight)) != 0:
    sys.exit(last_error().decode())
print("Dagger weight:", weight.value)

layout_free(layout)
defs_free(defs)
param_free(param)
binder_free(binder)
```

Two details worth copying into your own bindings: always set `restype` and `argtypes` (the default `int` return type
truncates 64-bit pointers), and treat the `Buffer` struct as `{pointer, size}` returned by value.

## Changing parameters

Parameter rows are raw bytes until a **paramdef** describes them. libSouls doesn't bundle any; point
`souls_paramdefs_add_folder` at a folder such as Paramdex's `ER/Defs`. `souls_param_layout_create` then picks the def that
matches the param's type, data version and row size, or returns `NULL` with `SOULS_ERR_NOT_FOUND`.

With a layout you read and write fields by their internal names:

```c
double weight;
souls_param_get_number(layout, param, row, "weight", &weight);
souls_param_set_number(layout, param, row, "weight", weight * 0.5);

int64_t added = souls_param_add_row(layout, param, 999999999, "My new row");   /* every field at its default */
```

All numeric fields go through `get_number`/`set_number` as `double`, and writes convert to the field's own type just as
the file would. Use `souls_param_get_string`/`set_string` for fixed-width text fields and
`souls_param_get_bytes`/`set_bytes` for `dummy8` padding arrays; `souls_param_layout_field_type` tells you which a
field is. Remember to write the param back (`souls_param_write_memory`) and store the bytes in its binder.

## Row names, enums and field descriptions

The game's files don't name param rows or explain fields. Community projects such as
[Paramdex](https://github.com/soulsmods/Paramdex) do, and libSouls can load their files (it doesn't bundle them):

```c
SoulsParamNames* names = souls_param_names_new();
souls_param_names_add_folder(names, "Paramdex/ER/Names");          /* one <Param>.txt per param */
souls_param_names_apply(names, weapons, "EquipParamWeapon", 0);    /* fills in rows that have no name */
const char* unarmed = souls_param_names_find(names, "EquipParamWeapon", 110000);

SoulsParamEnums* enums = souls_param_enums_new();
souls_param_enums_add_folder(enums, "Paramdex/ER/Tdfs");
int64_t goods;
souls_param_enums_find_value(enums, "GAITEM_CATEGORY", "Good", &goods);

SoulsFieldInfo info = {sizeof info};
souls_param_layout_field_info(layout, field, &info);   /* display name, description, limits, enum type... */
```

A file that fails to load doesn't stop the rest: `souls_param_names_load_error_count`/`_load_error` (and the `enums`
equivalents) list what was skipped and why. Param names are matched ignoring case. `SoulsFieldInfo` starts with its own
size so it can grow; set `struct_size` before the call.

## Elden Ring regulation

```c
SoulsBinder* bnd = souls_regulation_decrypt(SOULS_GAME_ELDEN_RING, "regulation.bin");
/* ... edit params inside, as above ... */
souls_regulation_encrypt(SOULS_GAME_ELDEN_RING, "regulation.bin", bnd);
```

`SOULS_GAME_DARK_SOULS_3` works the same way. Older games keep their params in ordinary binders and need no decryption.

## Save files

A save (`ER0000.sl2`, `DS30000.sl2`, ...) is a binder of `USER_DATA` files, each guarded by an MD5 hash and, in the
Dark Souls games, AES-encrypted. Reading checks the hashes and gives the decoded files; writing redoes both:

```c
SoulsBinder* save = souls_save_read_file(SOULS_SAVE_ELDEN_RING, "copy/ER0000.sl2");
/* souls_binder_file_bytes / souls_binder_set_file_bytes on USER_DATA000..011 */
souls_save_write_file(SOULS_SAVE_ELDEN_RING, "copy/ER0000.sl2", save);
```

Work on a copy: the game refuses a save whose hashes don't match, and a broken save can cost a character. libSouls
reads and writes the container only; what's inside each slot is up to you.

## Maps (MSB)

One `SoulsMsb` handle covers every game; you say which when you read, since the files don't identify themselves
(`SOULS_MSB_DARK_SOULS_1`, `_2`, `_3`, `_SEKIRO`, `_ELDEN_RING`). A map has four categories of entries: models, parts, events and
regions. Entries are addressed by category and index, they have a name and a type, and everything else is a
*property* read and written by name.

```c
SoulsMsb* map = souls_msb_read_file(SOULS_MSB_DARK_SOULS_1, "map/MapStudio/m10_00_00_00.msb");

for (size_t i = 0; i < souls_msb_entry_count(map, SOULS_MSB_PARTS); ++i) {
    if (strcmp(souls_msb_entry_type(map, SOULS_MSB_PARTS, i), "Enemy") != 0) continue;

    float position[3];
    souls_msb_get_vec3(map, SOULS_MSB_PARTS, i, "Position", position);
    position[1] += 5.0f;                                          /* lift every enemy five meters */
    souls_msb_set_vec3(map, SOULS_MSB_PARTS, i, "Position", position);

    int64_t entity_id;
    if (souls_msb_get_int(map, SOULS_MSB_PARTS, i, "EntityID", &entity_id) == SOULS_OK) {
        printf("%s (entity %lld)\n", souls_msb_entry_name(map, SOULS_MSB_PARTS, i), (long long)entity_id);
    }
}
souls_msb_write_file(map, "m10_00_00_00.msb");
souls_msb_free(map);
```

The properties each entry has depend on the category, the type and the game; `souls_msb_property_count`, `_name` and
`_kind` list them, and asking for one an entry doesn't have fails with `SOULS_ERR_UNSUPPORTED`. The common ones are
`ModelName`, `Position`, `Rotation`, `Scale`, `EntityID`, `DrawGroups` and `DispGroups` on parts, and `Position`,
`Rotation` and the region shape (`souls_msb_region_shape`, `_dims`, `souls_msb_set_region_shape`) on regions. See the
header for the full list. Properties specific to one type (an enemy's AI IDs, for example) are not exposed yet.

`souls_msb_type_name` lists what can be created in each category, and `souls_msb_add_entry` creates one with default
values. Entries refer to each other by name (a part's `ModelName`), so renaming and reordering are safe, but a
reference to a name that doesn't exist makes the write fail. Indices follow the written order and change when you add
or remove entries.

## Models (FLVER)

Vertex data is read and written one attribute at a time as flat arrays, so it maps directly onto numpy arrays or GPU
buffers:

```python
mesh = 0
count = flver_vertex_count(model, mesh)
positions = (C.c_float * (count * 3))()
flver_read_vertex_floats(model, mesh, SOULS_VERTEX_POSITION, 0, positions, count * 3)

for i in range(count):
    positions[i * 3 + 1] += 1.0                       # move the mesh up
flver_write_vertex_floats(model, mesh, SOULS_VERTEX_POSITION, 0, positions, count * 3)
```

`souls_flver_read_face_set` gives triangle indices (strips are expanded), and bones, dummy points, materials and their
textures all have getters and setters. A mesh's *layout* (which attributes it stores) can't be changed through the C
interface yet, so you can edit what a mesh has but not build one from nothing; `souls_flver_duplicate_mesh` copies an
existing mesh as a starting point.

## Game archives (BHD5 and BXF)

Elden Ring's `Data0.bhd` ... `DLC.bhd` store files under hashes of their paths. Open a header with the game's exe
(which carries the RSA keys that decrypt it), look a path up, and extract it from the matching `.bdt`:

```python
header = bhd5_open(SOULS_BHD5_ELDEN_RING, game + r"\Data2.bhd", game + r"\eldenring.exe", None)
index = bhd5_find_path(header, b"/map/mapstudio/m10_00_00_00.msb.dcx")
if index >= 0:
    bdt = bdt_open(game + r"\Data2.bdt")
    packed = bhd5_extract(header, bdt, index)          # decrypted; still DCX-compressed
    plain = dcx_decompress(packed.data, packed.size, None)
```

(`plain.data` now holds an MSB, which the map functions above can read.) The archives don't store names, only hashes,
so you can only find paths you already know; community-maintained path dictionaries exist for building a complete
unpacker. `souls_path_hash64` and `souls_path_hash32` expose the hashes directly.

Split binders (`.bhd`/`.bdt` pairs holding named files, such as texture archives) are `SoulsBinder`s too:
`souls_bxf_read_files` loads one, and for the very large ones `souls_bxf_reader_open` reads single files without loading
the data file.

## Event scripts (EMEVD)

```c
SoulsEmevd* script = souls_emevd_read_file("event/m10_00_00_00.emevd.dcx");
for (size_t e = 0; e < souls_emevd_event_count(script); ++e) {
    printf("event %lld: %zu instructions\n", (long long)souls_emevd_event_id(script, e),
           souls_emevd_instruction_count(script, e));
}
```

Instructions are `(bank, id)` plus raw argument bytes. What each one means is documented in the community's EMEDF
files, which libSouls doesn't bundle; once you know an instruction's argument layout, `souls_emevd_unpack_args` and
`souls_emevd_pack_args` convert the bytes to and from numbers.

## Animation events (TAE3)

Each event has a type (`souls_tae3_event_type_name`) and fields read and written by name:

```c
SoulsTae3* tae = souls_tae3_read_file("c2240.tae");
double sound_id;
if (souls_tae3_get_number(tae, animation, event, "SoundID", &sound_id) == SOULS_OK) {
    souls_tae3_set_number(tae, animation, event, "SoundID", sound_id + 1);
}
```

`souls_tae3_field_count`, `_name` and `_kind` list an event's fields.

Sekiro and Elden Ring files are version `0x1000D` (`souls_tae3_version`). Their event types mean different things per
game and aren't described yet, so libSouls keeps each event's data as bytes and writes it back unchanged:
`souls_tae3_event_is_raw` says when that's the case, `souls_tae3_event_raw_data`/`_set_event_raw_data` read and replace
the bytes, and `souls_tae3_add_raw_event` adds one. Animations, timings and event types can be edited as usual.

## Not yet covered

MQB, FXR, GPARAM, BTL, ESD and the other smaller formats remain C++-only. They will get C interfaces following the
conventions above; for tools that need them
today, use the C++ API ([Quickstart](Quickstart.md)).
