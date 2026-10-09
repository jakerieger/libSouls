# C API

libSouls has a flat C interface so it can be used from languages other than C++: Python (`ctypes`/`cffi`), C#
(P/Invoke), Rust, Lua, and so on. It lives in [`Source/libSouls/CApi/souls.h`](../Source/libSouls/CApi/souls.h), is plain
C99, and ships in the same `Souls.dll` as the C++ API. Include it as `<libSouls/CApi/souls.h>`.

This first pass covers the pieces most tools need:

| Area | What it gives you |
|---|---|
| Core | errors, byte buffers, Oodle setup, reading a file |
| DCX | compress, decompress, detect |
| Binders | BND3/BND4 read, write, edit files; `regulation.bin` decrypt and encrypt (Dark Souls III, Elden Ring) |
| FMG | read, write, get/set text by ID |
| PARAM | rows; paramdef sets; layouts for reading and writing fields by name |
| TPF | read, write, get/replace/add textures |

The remaining formats (maps, models, animation events, ...) are still C++-only for now and will follow the same
conventions.

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
paramdex = os.path.expanduser(r"~\Code\GitHub\Paramdex\ER\Defs")

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

## Elden Ring regulation

```c
SoulsBinder* bnd = souls_regulation_decrypt(SOULS_GAME_ELDEN_RING, "regulation.bin");
/* ... edit params inside, as above ... */
souls_regulation_encrypt(SOULS_GAME_ELDEN_RING, "regulation.bin", bnd);
```

`SOULS_GAME_DARK_SOULS_3` works the same way. Older games keep their params in ordinary binders and need no decryption.

## Not yet covered

MSB (maps), FLVER (models), TAE (animation events), MQB, FXR, GPARAM and the smaller formats remain C++-only. They will
get C interfaces following the conventions above; for tools that need them today, use the C++ API
([Quickstart](Quickstart.md)).
