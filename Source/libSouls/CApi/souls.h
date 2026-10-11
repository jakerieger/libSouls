/*
 * libSouls C API.
 *
 * A flat C interface to libSouls for other languages (Python, C#, Rust, Lua, ...). Plain C99; the same header is
 * usable from C++.
 *
 * Conventions
 * -----------
 *  - Objects are opaque handles. Every handle type has a matching *_free function; freeing NULL does nothing.
 *  - Functions that create an object return NULL on failure. Other functions return a SoulsStatus (SOULS_OK is 0).
 *    On failure, souls_last_error() describes what went wrong for the calling thread, and souls_last_error_code()
 *    gives the status. C++ exceptions never cross this boundary.
 *  - Text is UTF-8 and NUL-terminated. A returned `const char*` is borrowed: it stays valid until the object it came
 *    from is modified or freed (or, for souls_last_error(), until the next failing call on the same thread). Copy
 *    it if you need to keep it. Absent optional text is NULL, and optional text parameters accept NULL.
 *  - Bytes in are (pointer, size). Bytes out are either a SoulsBuffer, which you release with souls_buffer_free, or
 *    a borrowed pointer plus size, as documented per function.
 *  - Indices are zero-based size_t. An out-of-range index fails with SOULS_ERR_INVALID_ARGUMENT.
 *  - Handles are not internally synchronized; don't use one object from two threads at once. Error state is
 *    per-thread.
 */
#ifndef LIBSOULS_CAPI_SOULS_H
#define LIBSOULS_CAPI_SOULS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
    #define SOULS_EXTERN_C extern "C"
#else
    #define SOULS_EXTERN_C
#endif

#if defined(_WIN32)
    #ifdef LIBSOULS_EXPORTS
        #define SOULS_CALL __declspec(dllexport)
    #else
        #define SOULS_CALL __declspec(dllimport)
    #endif
#else
    #define SOULS_CALL
#endif

#define SOULS_C_API SOULS_EXTERN_C SOULS_CALL

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------------------------------------------------ */
/* Core                                                                                                         */
/* ------------------------------------------------------------------------------------------------------------ */

typedef enum SoulsStatus {
    SOULS_OK                   = 0,
    SOULS_ERR_INVALID_ARGUMENT = 1, /* NULL handle, bad index, bad enum value, ... */
    SOULS_ERR_IO               = 2, /* a file couldn't be opened, read or written */
    SOULS_ERR_PARSE            = 3, /* the data isn't valid for the format (or is an unsupported variant) */
    SOULS_ERR_NOT_FOUND        = 4, /* a lookup found nothing (no matching paramdef, no such entry, ...) */
    SOULS_ERR_INTERNAL         = 5, /* anything unexpected */
    SOULS_ERR_UNSUPPORTED      = 6  /* valid request that this object or game doesn't support */
} SoulsStatus;

/* Bytes owned by the caller. Release with souls_buffer_free. An empty result has data == NULL and size == 0. */
typedef struct SoulsBuffer {
    uint8_t* data;
    size_t size;
} SoulsBuffer;

/* The library version as "major.minor.patch". */
SOULS_C_API const char* souls_version(void);

/* The message for the last failure on this thread, or an empty string if there hasn't been one. */
SOULS_C_API const char* souls_last_error(void);
SOULS_C_API SoulsStatus souls_last_error_code(void);

SOULS_C_API void souls_buffer_free(SoulsBuffer buffer);

/* Reads a whole file. On failure the returned buffer is empty and the error is set. */
SOULS_C_API SoulsBuffer souls_read_file(const char* path);

/*
 * Most game files are Oodle-compressed, which needs oo2core_6_win64.dll from a FromSoftware game. By default it is
 * searched for next to the executable; set an explicit path before the first Oodle use to load it from elsewhere.
 * Returns 0 (and changes nothing) if the DLL has already been loaded.
 */
SOULS_C_API int souls_oodle_set_dll_path(const char* path);
SOULS_C_API int souls_oodle_is_available(void);

/* ------------------------------------------------------------------------------------------------------------ */
/* DCX compression                                                                                              */
/* ------------------------------------------------------------------------------------------------------------ */

typedef enum SoulsDcxType {
    SOULS_DCX_UNKNOWN              = 0,
    SOULS_DCX_NONE                 = 1, /* not compressed */
    SOULS_DCX_ZLIB                 = 2,
    SOULS_DCX_DCP_EDGE             = 3, /* decompress only */
    SOULS_DCX_DCP_DFLT             = 4,
    SOULS_DCX_DCX_EDGE             = 5,
    SOULS_DCX_DCX_DFLT_10000_24_9  = 6, /* Dark Souls, Dark Souls II */
    SOULS_DCX_DCX_DFLT_10000_44_9  = 7, /* Bloodborne, Dark Souls III */
    SOULS_DCX_DCX_DFLT_11000_44_8  = 8,
    SOULS_DCX_DCX_DFLT_11000_44_9  = 9, /* Sekiro */
    SOULS_DCX_DCX_DFLT_11000_44_9_15 = 10, /* Elden Ring regulation */
    SOULS_DCX_DCX_KRAK_6           = 11, /* Sekiro, Elden Ring (needs Oodle) */
    SOULS_DCX_DCX_KRAK_9           = 12, /* Armored Core VI (needs Oodle) */
    SOULS_DCX_DCX_ZSTD             = 13
} SoulsDcxType;

typedef enum SoulsGame {
    SOULS_GAME_DEMONS_SOULS  = 0,
    SOULS_GAME_DARK_SOULS_1  = 1,
    SOULS_GAME_DARK_SOULS_2  = 2,
    SOULS_GAME_BLOODBORNE    = 3,
    SOULS_GAME_DARK_SOULS_3  = 4,
    SOULS_GAME_SEKIRO        = 5,
    SOULS_GAME_ELDEN_RING    = 6,
    SOULS_GAME_ARMORED_CORE_6 = 7
} SoulsGame;

/* The compression a game's files usually use. */
SOULS_C_API SoulsDcxType souls_dcx_default_type(SoulsGame game);

/* 1 if the data starts with a DCX/DCP header. */
SOULS_C_API int souls_dcx_is(const uint8_t* data, size_t size);

/* Decompresses a DCX file. out_type (optional) receives the detected type. Empty buffer and an error on failure. */
SOULS_C_API SoulsBuffer souls_dcx_decompress(const uint8_t* data, size_t size, SoulsDcxType* out_type);

/* Compresses data. Empty buffer and an error on failure. */
SOULS_C_API SoulsBuffer souls_dcx_compress(const uint8_t* data, size_t size, SoulsDcxType type);

/* ------------------------------------------------------------------------------------------------------------ */
/* Binders (BND3 / BND4)                                                                                        */
/* ------------------------------------------------------------------------------------------------------------ */

/* A BND3 or BND4 archive. Files are decompressed when read and recompressed when written. */
typedef struct SoulsBinder SoulsBinder;

typedef enum SoulsBinderKind {
    SOULS_BINDER_BND3 = 3,
    SOULS_BINDER_BND4 = 4,
    /* A binder split over two files, a header and a data file (.bhd/.bdt, .tpfbhd/.tpfbdt, ...). Create and read
     * them with the souls_bxf_* functions; the file accessors below work on them like any other binder. */
    SOULS_BINDER_BXF3 = 5,
    SOULS_BINDER_BXF4 = 6
} SoulsBinderKind;

/* Per-file flags; combine with bitwise or. Bit 0 says the file is compressed inside the binder. */
enum {
    SOULS_FILE_COMPRESSED = 0x01,
    SOULS_FILE_FLAG1      = 0x02
};

SOULS_C_API SoulsBinder* souls_binder_new(SoulsBinderKind kind);
/* Reads either kind, detecting which. */
SOULS_C_API SoulsBinder* souls_binder_read_file(const char* path);
SOULS_C_API SoulsBinder* souls_binder_read_memory(const uint8_t* data, size_t size);
SOULS_C_API void souls_binder_free(SoulsBinder* binder);

SOULS_C_API SoulsBinderKind souls_binder_kind(const SoulsBinder* binder);
SOULS_C_API SoulsStatus souls_binder_write_file(SoulsBinder* binder, const char* path);
SOULS_C_API SoulsBuffer souls_binder_write_memory(SoulsBinder* binder);

/* The compression applied when the binder itself is written (detected when read). */
SOULS_C_API SoulsDcxType souls_binder_compression(const SoulsBinder* binder);
SOULS_C_API SoulsStatus souls_binder_set_compression(SoulsBinder* binder, SoulsDcxType type);

SOULS_C_API size_t souls_binder_file_count(const SoulsBinder* binder);
SOULS_C_API int32_t souls_binder_file_id(const SoulsBinder* binder, size_t index);
/* Borrowed. NULL if the file has no name (or the index is invalid). */
SOULS_C_API const char* souls_binder_file_name(const SoulsBinder* binder, size_t index);
SOULS_C_API uint8_t souls_binder_file_flags(const SoulsBinder* binder, size_t index);
/* Borrowed pointer to the file's decompressed bytes; *out_size receives the length. */
SOULS_C_API const uint8_t* souls_binder_file_bytes(const SoulsBinder* binder, size_t index, size_t* out_size);
/* Index of the first file whose name matches exactly, or -1. */
SOULS_C_API int64_t souls_binder_find_file(const SoulsBinder* binder, const char* name);
/* Index of the first file whose name ends with the suffix (case-sensitive), or -1. Handy for build-machine paths. */
SOULS_C_API int64_t souls_binder_find_file_suffix(const SoulsBinder* binder, const char* suffix);

SOULS_C_API SoulsStatus souls_binder_set_file_bytes(SoulsBinder* binder, size_t index, const uint8_t* data, size_t size);
SOULS_C_API SoulsStatus souls_binder_set_file_id(SoulsBinder* binder, size_t index, int32_t id);
SOULS_C_API SoulsStatus souls_binder_set_file_name(SoulsBinder* binder, size_t index, const char* name_or_null);
SOULS_C_API SoulsStatus souls_binder_set_file_flags(SoulsBinder* binder, size_t index, uint8_t flags);
/* Appends a file (name may be NULL) and returns its index, or -1 on failure. */
SOULS_C_API int64_t souls_binder_add_file(SoulsBinder* binder, int32_t id, const char* name_or_null, uint8_t flags,
                                          const uint8_t* data, size_t size);
SOULS_C_API SoulsStatus souls_binder_remove_file(SoulsBinder* binder, size_t index);

/* regulation.bin: decrypt to a binder, and encrypt a binder back to a file. game is DARK_SOULS_3 or ELDEN_RING. */
SOULS_C_API SoulsBinder* souls_regulation_decrypt(SoulsGame game, const char* path);
SOULS_C_API SoulsStatus souls_regulation_encrypt(SoulsGame game, const char* path, SoulsBinder* binder);

/*
 * Save files (.sl2): a BND4 whose files (USER_DATA000, ...) each start with an MD5 hash of the rest, and are
 * AES-encrypted in the Dark Souls games. Reading checks the hashes (SOULS_ERR_PARSE if one doesn't match) and gives a
 * binder of the decoded data; writing encodes the files again. Work on copies of saves, not the originals.
 */
typedef enum SoulsSaveKind {
    SOULS_SAVE_DARK_SOULS_2         = 1,
    SOULS_SAVE_DARK_SOULS_2_SCHOLAR = 2,
    SOULS_SAVE_DARK_SOULS_3         = 3,
    SOULS_SAVE_ELDEN_RING           = 4
} SoulsSaveKind;

SOULS_C_API SoulsBinder* souls_save_read_file(SoulsSaveKind kind, const char* path);
SOULS_C_API SoulsBinder* souls_save_read_memory(SoulsSaveKind kind, const uint8_t* data, size_t size);
SOULS_C_API SoulsStatus souls_save_write_file(SoulsSaveKind kind, const char* path, const SoulsBinder* binder);
SOULS_C_API SoulsBuffer souls_save_write_memory(SoulsSaveKind kind, const SoulsBinder* binder);

/* ------------------------------------------------------------------------------------------------------------ */
/* FMG (text)                                                                                                   */
/* ------------------------------------------------------------------------------------------------------------ */

typedef struct SoulsFMG SoulsFMG;

typedef enum SoulsFmgVersion {
    SOULS_FMG_DEMONS_SOULS = 0,
    SOULS_FMG_DARK_SOULS_1 = 1, /* and Dark Souls II */
    SOULS_FMG_DARK_SOULS_3 = 2  /* Bloodborne, Dark Souls III, Sekiro, Elden Ring */
} SoulsFmgVersion;

SOULS_C_API SoulsFMG* souls_fmg_new(SoulsFmgVersion version);
SOULS_C_API SoulsFMG* souls_fmg_read_file(const char* path);
SOULS_C_API SoulsFMG* souls_fmg_read_memory(const uint8_t* data, size_t size);
SOULS_C_API void souls_fmg_free(SoulsFMG* fmg);
SOULS_C_API SoulsStatus souls_fmg_write_file(SoulsFMG* fmg, const char* path);
SOULS_C_API SoulsBuffer souls_fmg_write_memory(SoulsFMG* fmg);

SOULS_C_API SoulsFmgVersion souls_fmg_version(const SoulsFMG* fmg);
SOULS_C_API size_t souls_fmg_entry_count(const SoulsFMG* fmg);
SOULS_C_API int32_t souls_fmg_entry_id(const SoulsFMG* fmg, size_t index);
/* Borrowed. NULL if the entry has no text. */
SOULS_C_API const char* souls_fmg_entry_text(const SoulsFMG* fmg, size_t index);
/* Borrowed. NULL if there is no entry with this ID or it has no text. */
SOULS_C_API const char* souls_fmg_get_text(const SoulsFMG* fmg, int32_t id);
/* Sets the text of an entry, adding it if needed. NULL text clears it. */
SOULS_C_API SoulsStatus souls_fmg_set_text(SoulsFMG* fmg, int32_t id, const char* text_or_null);
SOULS_C_API SoulsStatus souls_fmg_remove_entry(SoulsFMG* fmg, size_t index);

/* ------------------------------------------------------------------------------------------------------------ */
/* PARAM, paramdefs and layouts                                                                                 */
/* ------------------------------------------------------------------------------------------------------------ */

/* The table of rows. Row data is raw bytes; use a SoulsParamLayout to read and write fields. */
typedef struct SoulsParam SoulsParam;
/* A set of paramdefs (field descriptions) loaded from XML for one game, such as Paramdex's Defs folders. */
typedef struct SoulsParamDefs SoulsParamDefs;
/* Where each field of a paramdef lives in a row; makes the bytes of a PARAM's rows readable. */
typedef struct SoulsParamLayout SoulsParamLayout;

SOULS_C_API SoulsParam* souls_param_read_file(const char* path);
SOULS_C_API SoulsParam* souls_param_read_memory(const uint8_t* data, size_t size);
SOULS_C_API void souls_param_free(SoulsParam* param);
SOULS_C_API SoulsStatus souls_param_write_file(SoulsParam* param, const char* path);
SOULS_C_API SoulsBuffer souls_param_write_memory(SoulsParam* param);

/* Borrowed. Identifies which paramdef describes this param. */
SOULS_C_API const char* souls_param_type(const SoulsParam* param);
SOULS_C_API size_t souls_param_row_count(const SoulsParam* param);
SOULS_C_API int32_t souls_param_row_id(const SoulsParam* param, size_t index);
/* Borrowed. NULL if the row has no name. */
SOULS_C_API const char* souls_param_row_name(const SoulsParam* param, size_t index);
SOULS_C_API SoulsStatus souls_param_set_row_name(SoulsParam* param, size_t index, const char* name_or_null);
SOULS_C_API SoulsStatus souls_param_set_row_id(SoulsParam* param, size_t index, int32_t id);
/* Borrowed pointer to the row's raw bytes. */
SOULS_C_API const uint8_t* souls_param_row_bytes(const SoulsParam* param, size_t index, size_t* out_size);
/* Index of the first row with this ID, or -1. */
SOULS_C_API int64_t souls_param_find_row(const SoulsParam* param, int32_t id);
SOULS_C_API SoulsStatus souls_param_remove_row(SoulsParam* param, size_t index);

SOULS_C_API SoulsParamDefs* souls_paramdefs_new(void);
SOULS_C_API void souls_paramdefs_free(SoulsParamDefs* defs);
/*
 * Loads every .xml paramdef in a folder. Files that fail to load don't stop the rest: check
 * souls_paramdefs_load_error_count afterwards. Fails only if the folder doesn't exist.
 */
SOULS_C_API SoulsStatus souls_paramdefs_add_folder(SoulsParamDefs* defs, const char* folder, int recursive);
SOULS_C_API size_t souls_paramdefs_count(const SoulsParamDefs* defs);
SOULS_C_API size_t souls_paramdefs_load_error_count(const SoulsParamDefs* defs);
/* Borrowed: "path: message" for an error from the last add_folder. */
SOULS_C_API const char* souls_paramdefs_load_error(const SoulsParamDefs* defs, size_t index);

/*
 * A layout for the param, or NULL (SOULS_ERR_NOT_FOUND) if no def in the set matches its type, data version and row
 * size. The layout is independent of the defs and the param it was made from.
 */
SOULS_C_API SoulsParamLayout* souls_param_layout_create(const SoulsParam* param, const SoulsParamDefs* defs);
SOULS_C_API void souls_param_layout_free(SoulsParamLayout* layout);

typedef enum SoulsFieldType {
    SOULS_FIELD_S8      = 0,
    SOULS_FIELD_U8      = 1,
    SOULS_FIELD_S16     = 2,
    SOULS_FIELD_U16     = 3,
    SOULS_FIELD_S32     = 4,
    SOULS_FIELD_U32     = 5,
    SOULS_FIELD_B32     = 6,
    SOULS_FIELD_F32     = 7,
    SOULS_FIELD_ANGLE32 = 8,
    SOULS_FIELD_F64     = 9,
    SOULS_FIELD_DUMMY8  = 10, /* padding bytes; read and written with the bytes functions */
    SOULS_FIELD_FIXSTR  = 11,
    SOULS_FIELD_FIXSTRW = 12
} SoulsFieldType;

SOULS_C_API size_t souls_param_layout_row_size(const SoulsParamLayout* layout);
SOULS_C_API size_t souls_param_layout_field_count(const SoulsParamLayout* layout);
/* Borrowed. The field's internal name, the one the access functions take. */
SOULS_C_API const char* souls_param_layout_field_name(const SoulsParamLayout* layout, size_t field);
SOULS_C_API SoulsFieldType souls_param_layout_field_type(const SoulsParamLayout* layout, size_t field);
/* Index of the field with this internal name, or -1. */
SOULS_C_API int64_t souls_param_layout_find_field(const SoulsParamLayout* layout, const char* name);

/* Numeric fields (everything but dummy8 and strings), as doubles. Writes convert to the field's own type. */
SOULS_C_API SoulsStatus souls_param_get_number(const SoulsParamLayout* layout, const SoulsParam* param, size_t row,
                                               const char* field, double* out_value);
SOULS_C_API SoulsStatus souls_param_set_number(const SoulsParamLayout* layout, SoulsParam* param, size_t row,
                                               const char* field, double value);
/* fixstr / fixstrW fields. The returned text is borrowed from the layout until its next string read. */
SOULS_C_API const char* souls_param_get_string(const SoulsParamLayout* layout, const SoulsParam* param, size_t row,
                                               const char* field);
SOULS_C_API SoulsStatus souls_param_set_string(const SoulsParamLayout* layout, SoulsParam* param, size_t row,
                                               const char* field, const char* text);
/* dummy8 arrays: copies up to capacity bytes into out and reports the field's full length in *out_size. */
SOULS_C_API SoulsStatus souls_param_get_bytes(const SoulsParamLayout* layout, const SoulsParam* param, size_t row,
                                              const char* field, uint8_t* out, size_t capacity, size_t* out_size);
SOULS_C_API SoulsStatus souls_param_set_bytes(const SoulsParamLayout* layout, SoulsParam* param, size_t row,
                                              const char* field, const uint8_t* data, size_t size);
/* Appends a row with every field at its default and returns its index, or -1 on failure. */
SOULS_C_API int64_t souls_param_add_row(const SoulsParamLayout* layout, SoulsParam* param, int32_t id,
                                        const char* name_or_null);

/* ------------------------------------------------------------------------------------------------------------ */
/* Param metadata: field descriptions, row names, enums                                                         */
/* ------------------------------------------------------------------------------------------------------------ */

/* What a paramdef says about one field, for building editors. Set struct_size to sizeof(SoulsFieldInfo) before the call. */
typedef struct SoulsFieldInfo {
    size_t struct_size;
    const char* display_name;   /* borrowed */
    const char* description;    /* borrowed; NULL if there is none */
    const char* internal_type;  /* borrowed; the type in the engine, which may name an enum (see SoulsParamEnums) */
    const char* display_format; /* borrowed; a printf-style format */
    int32_t array_length;       /* elements for array types, otherwise 1 */
    int32_t bit_size;           /* bits used by a bitfield, otherwise -1 */
    int32_t sort_id;            /* editors order fields by this */
    int32_t edit_flags;         /* bit 0: wraps around, bit 2: locked */
    int has_default;
    int has_minimum;
    int has_maximum;
    int has_increment;
    double default_value;
    double minimum;
    double maximum;
    double increment;
} SoulsFieldInfo;

SOULS_C_API SoulsStatus souls_param_layout_field_info(const SoulsParamLayout* layout, size_t field, SoulsFieldInfo* out_info);

/*
 * Names for param rows, which the game's own files leave out. They come from text files kept by the community, such as
 * Paramdex's Names folders: one file per param named after it (EquipParamWeapon.txt), one "<ID> <name>" line per row.
 */
typedef struct SoulsParamNames SoulsParamNames;

SOULS_C_API SoulsParamNames* souls_param_names_new(void);
SOULS_C_API void souls_param_names_free(SoulsParamNames* names);
/* Loads every .txt file in the folder; the file name without its extension is the param's name. */
SOULS_C_API SoulsStatus souls_param_names_add_folder(SoulsParamNames* names, const char* folder);
SOULS_C_API size_t souls_param_names_param_count(const SoulsParamNames* names);
/* Borrowed. The name of the row with this ID in the param, or NULL if there isn't one. */
SOULS_C_API const char* souls_param_names_find(const SoulsParamNames* names, const char* param_name, int32_t id);
/* Gives rows their names (rows that already have one keep it unless overwrite is set). Returns how many were named, or -1. */
SOULS_C_API int64_t souls_param_names_apply(const SoulsParamNames* names, SoulsParam* param, const char* param_name, int overwrite);
SOULS_C_API size_t souls_param_names_load_error_count(const SoulsParamNames* names);
SOULS_C_API const char* souls_param_names_load_error(const SoulsParamNames* names, size_t index);

/* Names for the values of enumerated field types (Paramdex's Tdfs folders). */
typedef struct SoulsParamEnums SoulsParamEnums;

SOULS_C_API SoulsParamEnums* souls_param_enums_new(void);
SOULS_C_API void souls_param_enums_free(SoulsParamEnums* enums);
/* Loads every .tdf file in the folder. */
SOULS_C_API SoulsStatus souls_param_enums_add_folder(SoulsParamEnums* enums, const char* folder);
SOULS_C_API size_t souls_param_enums_count(const SoulsParamEnums* enums);
/* Borrowed. */
SOULS_C_API const char* souls_param_enums_name(const SoulsParamEnums* enums, size_t index);
/* The enumerators of the named enum (case is ignored); 0 if there is no such enum. */
SOULS_C_API size_t souls_param_enums_entry_count(const SoulsParamEnums* enums, const char* enum_name);
SOULS_C_API int64_t souls_param_enums_entry_value(const SoulsParamEnums* enums, const char* enum_name, size_t entry);
/* Borrowed. NULL if the entry has no name. */
SOULS_C_API const char* souls_param_enums_entry_name(const SoulsParamEnums* enums, const char* enum_name, size_t entry);
/* The name of a value, or NULL if the enum doesn't name it. */
SOULS_C_API const char* souls_param_enums_find_name(const SoulsParamEnums* enums, const char* enum_name, int64_t value);
/* Looks a name up: returns SOULS_OK and fills *out_value, or SOULS_ERR_NOT_FOUND. */
SOULS_C_API SoulsStatus souls_param_enums_find_value(const SoulsParamEnums* enums, const char* enum_name, const char* entry_name, int64_t* out_value);
SOULS_C_API size_t souls_param_enums_load_error_count(const SoulsParamEnums* enums);
SOULS_C_API const char* souls_param_enums_load_error(const SoulsParamEnums* enums, size_t index);

/* ------------------------------------------------------------------------------------------------------------ */
/* TPF (textures)                                                                                               */
/* ------------------------------------------------------------------------------------------------------------ */

typedef struct SoulsTPF SoulsTPF;

SOULS_C_API SoulsTPF* souls_tpf_new(void);
SOULS_C_API SoulsTPF* souls_tpf_read_file(const char* path);
SOULS_C_API SoulsTPF* souls_tpf_read_memory(const uint8_t* data, size_t size);
SOULS_C_API void souls_tpf_free(SoulsTPF* tpf);
SOULS_C_API SoulsStatus souls_tpf_write_file(SoulsTPF* tpf, const char* path);
SOULS_C_API SoulsBuffer souls_tpf_write_memory(SoulsTPF* tpf);

SOULS_C_API size_t souls_tpf_texture_count(const SoulsTPF* tpf);
/* Borrowed. Name without path or extension. */
SOULS_C_API const char* souls_tpf_texture_name(const SoulsTPF* tpf, size_t index);
SOULS_C_API SoulsStatus souls_tpf_set_texture_name(SoulsTPF* tpf, size_t index, const char* name);
/* Borrowed pointer to the texture's data: a complete DDS file on PC. */
SOULS_C_API const uint8_t* souls_tpf_texture_bytes(const SoulsTPF* tpf, size_t index, size_t* out_size);
SOULS_C_API SoulsStatus souls_tpf_set_texture_bytes(SoulsTPF* tpf, size_t index, const uint8_t* data, size_t size);
/* Appends a PC texture from a DDS file and returns its index, or -1 (for example if the bytes aren't a DDS). */
SOULS_C_API int64_t souls_tpf_add_texture(SoulsTPF* tpf, const char* name, uint8_t format, uint8_t flags1,
                                          const uint8_t* dds, size_t size);
SOULS_C_API SoulsStatus souls_tpf_remove_texture(SoulsTPF* tpf, size_t index);

/* ------------------------------------------------------------------------------------------------------------ */
/* MSB (maps)                                                                                                   */
/* ------------------------------------------------------------------------------------------------------------ */

/*
 * A map layout: the models, parts, events and regions of one map. One handle type covers every game; the kind says
 * which layout it is, since the files don't identify themselves.
 *
 * Entries are addressed by category and index. The index order is the order they're written in (grouped by type), so
 * indices change when you add or remove entries; look entries up again afterwards. Entries refer to each other by
 * name (a part's model, an event's part), so renaming and reordering is safe.
 *
 * Every entry has a name, a type (such as "Enemy" or "MapPiece") and a set of properties. Which properties exist
 * depends on the category, the type and the game; use souls_msb_entry_property_count/name to discover them. The
 * common ones are:
 *   models:  SibPath (string), InstanceCount (int)
 *   parts:   ModelName, SibPath (strings), Position, Rotation, Scale (vec3), EntityID, MapStudioLayer (ints),
 *            DrawGroups, DispGroups, BackreadGroups, EntityGroups (int arrays)
 *   events:  EventID, EntityID (ints), PartName, RegionName (strings)
 *   regions: Position, Rotation (vec3), EntityID, MapStudioLayer (ints), ActivationPartName (string);
 *            the shape has its own functions
 * Properties specific to one entry type (an enemy's AI IDs, for instance) are not exposed by this interface yet.
 */
typedef struct SoulsMsb SoulsMsb;

typedef enum SoulsMsbKind {
    SOULS_MSB_DARK_SOULS_1 = 1, /* MSB1 */
    SOULS_MSB_DARK_SOULS_2 = 2, /* MSB2, including Scholar of the First Sin */
    SOULS_MSB_DARK_SOULS_3 = 3, /* MSB3 */
    SOULS_MSB_SEKIRO       = 4, /* MSBS */
    SOULS_MSB_ELDEN_RING   = 5  /* MSBE */
} SoulsMsbKind;

typedef enum SoulsMsbCategory {
    SOULS_MSB_MODELS  = 0,
    SOULS_MSB_PARTS   = 1,
    SOULS_MSB_EVENTS  = 2,
    SOULS_MSB_REGIONS = 3
} SoulsMsbCategory;

typedef enum SoulsPropertyKind {
    SOULS_PROPERTY_INT       = 0, /* int64 */
    SOULS_PROPERTY_STRING    = 1, /* text; may be absent (NULL) */
    SOULS_PROPERTY_VEC3      = 2, /* three floats */
    SOULS_PROPERTY_INT_ARRAY = 3, /* array of int64 (fixed length) */
    SOULS_PROPERTY_FLOAT     = 4, /* double (animation events) */
    SOULS_PROPERTY_BYTES     = 5  /* fixed-length byte array (animation events) */
} SoulsPropertyKind;

typedef enum SoulsShapeType {
    SOULS_SHAPE_POINT    = 0,
    SOULS_SHAPE_CIRCLE   = 1, /* radius */
    SOULS_SHAPE_SPHERE   = 2, /* radius */
    SOULS_SHAPE_CYLINDER = 3, /* radius, height */
    SOULS_SHAPE_RECT     = 4, /* width, depth */
    SOULS_SHAPE_BOX      = 5  /* width, depth, height */
} SoulsShapeType;

SOULS_C_API SoulsMsb* souls_msb_new(SoulsMsbKind kind);
SOULS_C_API SoulsMsb* souls_msb_read_file(SoulsMsbKind kind, const char* path);
SOULS_C_API SoulsMsb* souls_msb_read_memory(SoulsMsbKind kind, const uint8_t* data, size_t size);
SOULS_C_API void souls_msb_free(SoulsMsb* msb);
SOULS_C_API SoulsStatus souls_msb_write_file(SoulsMsb* msb, const char* path);
SOULS_C_API SoulsBuffer souls_msb_write_memory(SoulsMsb* msb);
SOULS_C_API SoulsMsbKind souls_msb_kind(const SoulsMsb* msb);

SOULS_C_API size_t souls_msb_entry_count(const SoulsMsb* msb, SoulsMsbCategory category);
/* Borrowed. */
SOULS_C_API const char* souls_msb_entry_name(const SoulsMsb* msb, SoulsMsbCategory category, size_t index);
SOULS_C_API SoulsStatus souls_msb_set_entry_name(SoulsMsb* msb, SoulsMsbCategory category, size_t index, const char* name);
/* Borrowed, such as "Enemy". Valid for the life of the process. */
SOULS_C_API const char* souls_msb_entry_type(const SoulsMsb* msb, SoulsMsbCategory category, size_t index);
/* Index of the first entry with this name, or -1. */
SOULS_C_API int64_t souls_msb_find_entry(const SoulsMsb* msb, SoulsMsbCategory category, const char* name);

/* The entry types that can be created in a category for this game. Names are valid for the life of the process. */
SOULS_C_API size_t souls_msb_type_count(const SoulsMsb* msb, SoulsMsbCategory category);
SOULS_C_API const char* souls_msb_type_name(const SoulsMsb* msb, SoulsMsbCategory category, size_t type_index);
/* Adds an entry of the named type with default values (and the given name, if not NULL). Returns its index, or -1. */
SOULS_C_API int64_t souls_msb_add_entry(SoulsMsb* msb, SoulsMsbCategory category, const char* type_name, const char* name_or_null);
SOULS_C_API SoulsStatus souls_msb_remove_entry(SoulsMsb* msb, SoulsMsbCategory category, size_t index);

/* Property discovery for one entry. */
SOULS_C_API size_t souls_msb_property_count(const SoulsMsb* msb, SoulsMsbCategory category, size_t index);
SOULS_C_API const char* souls_msb_property_name(const SoulsMsb* msb, SoulsMsbCategory category, size_t index, size_t property);
SOULS_C_API SoulsPropertyKind souls_msb_property_kind(const SoulsMsb* msb, SoulsMsbCategory category, size_t index, size_t property);

/*
 * Property access by name. These fail with SOULS_ERR_UNSUPPORTED if the entry doesn't have the property, and with
 * SOULS_ERR_INVALID_ARGUMENT if it has it with a different kind.
 */
SOULS_C_API SoulsStatus souls_msb_get_int(const SoulsMsb* msb, SoulsMsbCategory category, size_t index, const char* property, int64_t* out_value);
SOULS_C_API SoulsStatus souls_msb_set_int(SoulsMsb* msb, SoulsMsbCategory category, size_t index, const char* property, int64_t value);
/* *out_text is borrowed, and NULL when the property is absent. */
SOULS_C_API SoulsStatus souls_msb_get_string(const SoulsMsb* msb, SoulsMsbCategory category, size_t index, const char* property, const char** out_text);
SOULS_C_API SoulsStatus souls_msb_set_string(SoulsMsb* msb, SoulsMsbCategory category, size_t index, const char* property, const char* text_or_null);
SOULS_C_API SoulsStatus souls_msb_get_vec3(const SoulsMsb* msb, SoulsMsbCategory category, size_t index, const char* property, float out_xyz[3]);
SOULS_C_API SoulsStatus souls_msb_set_vec3(SoulsMsb* msb, SoulsMsbCategory category, size_t index, const char* property, const float xyz[3]);
/* Copies up to capacity values and reports the array's full length in *out_count. */
SOULS_C_API SoulsStatus souls_msb_get_int_array(const SoulsMsb* msb, SoulsMsbCategory category, size_t index, const char* property,
                                                int64_t* out, size_t capacity, size_t* out_count);
/* The array length is fixed: count must match. */
SOULS_C_API SoulsStatus souls_msb_set_int_array(SoulsMsb* msb, SoulsMsbCategory category, size_t index, const char* property,
                                                const int64_t* values, size_t count);

/* Region shapes. dims receives up to 3 values (radius; radius, height; width, depth; width, depth, height). */
SOULS_C_API SoulsShapeType souls_msb_region_shape(const SoulsMsb* msb, size_t region_index);
SOULS_C_API SoulsStatus souls_msb_region_shape_dims(const SoulsMsb* msb, size_t region_index, float out_dims[3]);
SOULS_C_API SoulsStatus souls_msb_set_region_shape(SoulsMsb* msb, size_t region_index, SoulsShapeType shape, const float dims[3]);

/* ------------------------------------------------------------------------------------------------------------ */
/* FLVER (models)                                                                                               */
/* ------------------------------------------------------------------------------------------------------------ */

/*
 * A model: a skeleton of bones, dummy points, materials and meshes. Vertex data is read and written in bulk, one
 * attribute at a time, as flat arrays that map straight onto numpy / typed arrays / GPU buffers.
 *
 * What a mesh's vertices can hold is set by the vertex buffer layouts stored in the file. You can change the values
 * of the attributes a mesh already has, but this interface can't yet change a mesh's layout or build a mesh from
 * scratch; souls_flver_duplicate_mesh copies an existing one as a starting point.
 */
typedef struct SoulsFlver SoulsFlver;

typedef enum SoulsVertexAttribute {
    SOULS_VERTEX_POSITION     = 0, /* 3 floats */
    SOULS_VERTEX_NORMAL       = 1, /* 3 floats */
    SOULS_VERTEX_BONE_WEIGHTS = 2, /* 4 floats */
    SOULS_VERTEX_BONE_INDICES = 3, /* 4 int32 (indices into the mesh's bone indices list); use the _ints functions */
    SOULS_VERTEX_UV           = 4, /* 3 floats per UV set (z is 0 unless the format has three components) */
    SOULS_VERTEX_TANGENT      = 5, /* 4 floats per tangent set */
    SOULS_VERTEX_BITANGENT    = 6, /* 4 floats */
    SOULS_VERTEX_COLOR        = 7  /* 4 floats (r, g, b, a, each 0 to 1) per color set */
} SoulsVertexAttribute;

SOULS_C_API SoulsFlver* souls_flver_new(void);
SOULS_C_API SoulsFlver* souls_flver_read_file(const char* path);
SOULS_C_API SoulsFlver* souls_flver_read_memory(const uint8_t* data, size_t size);
SOULS_C_API void souls_flver_free(SoulsFlver* flver);
SOULS_C_API SoulsStatus souls_flver_write_file(SoulsFlver* flver, const char* path);
SOULS_C_API SoulsBuffer souls_flver_write_memory(SoulsFlver* flver);

/* The format version, such as 0x2000D (Dark Souls), 0x20014 (Dark Souls III) or 0x2001A (Sekiro). */
SOULS_C_API int32_t souls_flver_version(const SoulsFlver* flver);
SOULS_C_API SoulsStatus souls_flver_bounding_box(const SoulsFlver* flver, float out_min[3], float out_max[3]);
SOULS_C_API SoulsStatus souls_flver_set_bounding_box(SoulsFlver* flver, const float min[3], const float max[3]);

/* Bones. Parent / child / sibling indices are bone indices, or -1. */
SOULS_C_API size_t souls_flver_bone_count(const SoulsFlver* flver);
SOULS_C_API const char* souls_flver_bone_name(const SoulsFlver* flver, size_t bone);
SOULS_C_API SoulsStatus souls_flver_set_bone_name(SoulsFlver* flver, size_t bone, const char* name);
SOULS_C_API int32_t souls_flver_bone_parent(const SoulsFlver* flver, size_t bone);
SOULS_C_API SoulsStatus souls_flver_set_bone_parent(SoulsFlver* flver, size_t bone, int32_t parent);
/* Local translation, rotation (Euler angles in radians) and scale. Any output may be NULL. */
SOULS_C_API SoulsStatus souls_flver_bone_transform(const SoulsFlver* flver, size_t bone, float translation[3], float rotation[3], float scale[3]);
SOULS_C_API SoulsStatus souls_flver_set_bone_transform(SoulsFlver* flver, size_t bone, const float translation[3], const float rotation[3], const float scale[3]);

/* Dummy points. */
SOULS_C_API size_t souls_flver_dummy_count(const SoulsFlver* flver);
SOULS_C_API SoulsStatus souls_flver_dummy(const SoulsFlver* flver, size_t dummy, float position[3], float forward[3], float upward[3],
                                          int32_t* reference_id, int32_t* parent_bone, int32_t* attach_bone);
SOULS_C_API SoulsStatus souls_flver_set_dummy(SoulsFlver* flver, size_t dummy, const float position[3], const float forward[3],
                                              const float upward[3], int32_t reference_id, int32_t parent_bone, int32_t attach_bone);

/* Materials and their textures. */
SOULS_C_API size_t souls_flver_material_count(const SoulsFlver* flver);
SOULS_C_API const char* souls_flver_material_name(const SoulsFlver* flver, size_t material);
SOULS_C_API SoulsStatus souls_flver_set_material_name(SoulsFlver* flver, size_t material, const char* name);
/* The .mtd that picks the material's shader. */
SOULS_C_API const char* souls_flver_material_mtd(const SoulsFlver* flver, size_t material);
SOULS_C_API SoulsStatus souls_flver_set_material_mtd(SoulsFlver* flver, size_t material, const char* mtd);
SOULS_C_API size_t souls_flver_texture_count(const SoulsFlver* flver, size_t material);
/* What the texture is for ("g_Diffuse") and where it comes from. */
SOULS_C_API const char* souls_flver_texture_type(const SoulsFlver* flver, size_t material, size_t texture);
SOULS_C_API const char* souls_flver_texture_path(const SoulsFlver* flver, size_t material, size_t texture);
SOULS_C_API SoulsStatus souls_flver_set_texture(SoulsFlver* flver, size_t material, size_t texture, const char* type, const char* path);
SOULS_C_API int64_t souls_flver_add_texture(SoulsFlver* flver, size_t material, const char* type, const char* path);
SOULS_C_API SoulsStatus souls_flver_remove_texture(SoulsFlver* flver, size_t material, size_t texture);

/* Meshes. */
SOULS_C_API size_t souls_flver_mesh_count(const SoulsFlver* flver);
SOULS_C_API int32_t souls_flver_mesh_material(const SoulsFlver* flver, size_t mesh);
SOULS_C_API SoulsStatus souls_flver_set_mesh_material(SoulsFlver* flver, size_t mesh, int32_t material);
/* Whether the mesh is bound to bones (skinned). */
SOULS_C_API int souls_flver_mesh_is_dynamic(const SoulsFlver* flver, size_t mesh);
/* The bones that vertex bone indices refer to. Copies up to capacity values and reports the full count. */
SOULS_C_API SoulsStatus souls_flver_mesh_bone_indices(const SoulsFlver* flver, size_t mesh, int32_t* out, size_t capacity, size_t* out_count);
SOULS_C_API SoulsStatus souls_flver_set_mesh_bone_indices(SoulsFlver* flver, size_t mesh, const int32_t* indices, size_t count);
SOULS_C_API int64_t souls_flver_duplicate_mesh(SoulsFlver* flver, size_t mesh);
SOULS_C_API SoulsStatus souls_flver_remove_mesh(SoulsFlver* flver, size_t mesh);

/* Vertices. */
SOULS_C_API size_t souls_flver_vertex_count(const SoulsFlver* flver, size_t mesh);
/* How many sets of a multi-set attribute (UV, tangent, color) the mesh's vertices have; 1 for the others. */
SOULS_C_API size_t souls_flver_attribute_sets(const SoulsFlver* flver, size_t mesh, SoulsVertexAttribute attribute);
/* Values per vertex for the attribute (3 for positions, 4 for weights, ...). */
SOULS_C_API size_t souls_flver_attribute_components(SoulsVertexAttribute attribute);
/*
 * Copies one attribute of every vertex into out as vertex_count * components values, vertex after vertex. capacity
 * is the size of out in values; the call fails if it is too small. set picks the UV / tangent / color set.
 */
SOULS_C_API SoulsStatus souls_flver_read_vertex_floats(const SoulsFlver* flver, size_t mesh, SoulsVertexAttribute attribute, size_t set,
                                                       float* out, size_t capacity);
SOULS_C_API SoulsStatus souls_flver_write_vertex_floats(SoulsFlver* flver, size_t mesh, SoulsVertexAttribute attribute, size_t set,
                                                        const float* values, size_t count);
/* Bone indices (SOULS_VERTEX_BONE_INDICES), 4 per vertex. */
SOULS_C_API SoulsStatus souls_flver_read_vertex_ints(const SoulsFlver* flver, size_t mesh, int32_t* out, size_t capacity);
SOULS_C_API SoulsStatus souls_flver_write_vertex_ints(SoulsFlver* flver, size_t mesh, const int32_t* values, size_t count);

/* Faces. A mesh has one face set per level of detail / purpose. */
SOULS_C_API size_t souls_flver_face_set_count(const SoulsFlver* flver, size_t mesh);
/* The face set's flags (lod levels, motion blur, ...). */
SOULS_C_API uint32_t souls_flver_face_set_flags(const SoulsFlver* flver, size_t mesh, size_t face_set);
/* Number of indices after turning strips into plain triangles (three per face). */
SOULS_C_API size_t souls_flver_face_set_index_count(const SoulsFlver* flver, size_t mesh, size_t face_set);
SOULS_C_API SoulsStatus souls_flver_read_face_set(const SoulsFlver* flver, size_t mesh, size_t face_set, int32_t* out, size_t capacity);
/* Replaces the face set's indices with a triangle list (and stops it being a strip). */
SOULS_C_API SoulsStatus souls_flver_write_face_set(SoulsFlver* flver, size_t mesh, size_t face_set, const int32_t* indices, size_t count);

/* ------------------------------------------------------------------------------------------------------------ */
/* Split binders (BXF3 / BXF4)                                                                                  */
/* ------------------------------------------------------------------------------------------------------------ */

/* kind is SOULS_BINDER_BXF3 or SOULS_BINDER_BXF4. The result is a SoulsBinder: use souls_binder_* to inspect and edit it. */
SOULS_C_API SoulsBinder* souls_bxf_read_files(SoulsBinderKind kind, const char* header_path, const char* data_path);
SOULS_C_API SoulsBinder* souls_bxf_read_memory(SoulsBinderKind kind, const uint8_t* header, size_t header_size,
                                               const uint8_t* data, size_t data_size);
SOULS_C_API SoulsStatus souls_bxf_write_files(SoulsBinder* binder, const char* header_path, const char* data_path);
SOULS_C_API SoulsStatus souls_bxf_write_memory(SoulsBinder* binder, SoulsBuffer* out_header, SoulsBuffer* out_data);

/*
 * Reads files out of a header/data pair one at a time without loading the whole data file, which matters for the
 * huge ones (Elden Ring's map tile texture archive is over a gigabyte). The kind (BXF3 or BXF4) is detected.
 */
typedef struct SoulsBxfReader SoulsBxfReader;

SOULS_C_API SoulsBxfReader* souls_bxf_reader_open(const char* header_path, const char* data_path);
SOULS_C_API void souls_bxf_reader_free(SoulsBxfReader* reader);
SOULS_C_API SoulsBinderKind souls_bxf_reader_kind(const SoulsBxfReader* reader);
SOULS_C_API size_t souls_bxf_reader_file_count(const SoulsBxfReader* reader);
SOULS_C_API int32_t souls_bxf_reader_file_id(const SoulsBxfReader* reader, size_t index);
/* Borrowed. NULL if the file has no name. */
SOULS_C_API const char* souls_bxf_reader_file_name(const SoulsBxfReader* reader, size_t index);
SOULS_C_API uint8_t souls_bxf_reader_file_flags(const SoulsBxfReader* reader, size_t index);
/* Size in the data file, and size after decompression (-1 if the format doesn't record it). */
SOULS_C_API int64_t souls_bxf_reader_file_stored_size(const SoulsBxfReader* reader, size_t index);
SOULS_C_API int64_t souls_bxf_reader_file_unpacked_size(const SoulsBxfReader* reader, size_t index);
/* Index of the file with exactly this stored name, or -1. */
SOULS_C_API int64_t souls_bxf_reader_find(const SoulsBxfReader* reader, const char* name, int ignore_case);
/* Index of the first file whose name ends in this file name (ignoring folders and case), or -1. */
SOULS_C_API int64_t souls_bxf_reader_find_file_name(const SoulsBxfReader* reader, const char* file_name);
/* Reads and decompresses one file. */
SOULS_C_API SoulsBuffer souls_bxf_reader_read(SoulsBxfReader* reader, size_t index);

/* ------------------------------------------------------------------------------------------------------------ */
/* Game archives (BHD5)                                                                                         */
/* ------------------------------------------------------------------------------------------------------------ */

/*
 * The big packed archives of a game install (Data0.bhd/.bdt, DLC.bhd/.bdt, ...) store files under hashes of their
 * paths rather than names. A SoulsBhd5 is the header (the index of every file); a SoulsBdt is the data file the
 * header points into. Files are found by hashing a path you already know.
 */
typedef struct SoulsBhd5 SoulsBhd5;
typedef struct SoulsBdt SoulsBdt;

typedef enum SoulsBhd5Game {
    SOULS_BHD5_DARK_SOULS_1 = 0,
    SOULS_BHD5_DARK_SOULS_2 = 1,
    SOULS_BHD5_DARK_SOULS_3 = 2, /* and Sekiro */
    SOULS_BHD5_ELDEN_RING   = 3
} SoulsBhd5Game;

/* The 32-bit and 64-bit (Elden Ring) hashes the archives use for paths. Case and slash direction don't matter. */
SOULS_C_API uint32_t souls_path_hash32(const char* path);
SOULS_C_API uint64_t souls_path_hash64(const char* path);

/*
 * Opens a header file. Most are RSA-encrypted; the key is a public key stored in the game's executable. Pass the
 * exe's path (every key in it is tried) and/or a PEM key; if the header isn't encrypted both may be NULL.
 */
SOULS_C_API SoulsBhd5* souls_bhd5_open(SoulsBhd5Game game, const char* header_path, const char* exe_path_or_null, const char* key_pem_or_null);
/* Reads a header that is already decrypted. */
SOULS_C_API SoulsBhd5* souls_bhd5_read_memory(SoulsBhd5Game game, const uint8_t* data, size_t size);
SOULS_C_API void souls_bhd5_free(SoulsBhd5* bhd5);

SOULS_C_API size_t souls_bhd5_file_count(const SoulsBhd5* bhd5);
SOULS_C_API uint64_t souls_bhd5_file_hash(const SoulsBhd5* bhd5, size_t index);
/* Size in the data file (including encryption padding), and the real size (-1 if the game doesn't record it). */
SOULS_C_API int64_t souls_bhd5_file_stored_size(const SoulsBhd5* bhd5, size_t index);
SOULS_C_API int64_t souls_bhd5_file_unpacked_size(const SoulsBhd5* bhd5, size_t index);
SOULS_C_API int64_t souls_bhd5_file_offset(const SoulsBhd5* bhd5, size_t index);
SOULS_C_API int souls_bhd5_file_is_encrypted(const SoulsBhd5* bhd5, size_t index);
/* Index of the file stored under this hash, or -1. */
SOULS_C_API int64_t souls_bhd5_find_hash(const SoulsBhd5* bhd5, uint64_t hash);
/* Index of the file at this path (hashed the way the game does), or -1. */
SOULS_C_API int64_t souls_bhd5_find_path(const SoulsBhd5* bhd5, const char* path);

SOULS_C_API SoulsBdt* souls_bdt_open(const char* data_path);
SOULS_C_API void souls_bdt_free(SoulsBdt* bdt);
/* Reads one file's bytes from the data file, decrypting it if it's encrypted. Files are usually DCX-compressed. */
SOULS_C_API SoulsBuffer souls_bhd5_extract(const SoulsBhd5* bhd5, SoulsBdt* bdt, size_t index);

/* ------------------------------------------------------------------------------------------------------------ */
/* EMEVD (event scripts)                                                                                        */
/* ------------------------------------------------------------------------------------------------------------ */

/*
 * A map's event script: a list of events, each a list of instructions with raw argument bytes. What each
 * (bank, id) instruction means, and the layout of its arguments, comes from the community's EMEDF documents, which
 * libSouls doesn't bundle; given an argument layout you can unpack and pack the bytes as numbers.
 */
typedef struct SoulsEmevd SoulsEmevd;

typedef enum SoulsEmevdGame {
    SOULS_EMEVD_DARK_SOULS_1    = 0, /* and Dark Souls II on PC */
    SOULS_EMEVD_DARK_SOULS_1_BE = 1, /* console, big-endian */
    SOULS_EMEVD_BLOODBORNE      = 2, /* and Dark Souls II: Scholar of the First Sin */
    SOULS_EMEVD_DARK_SOULS_3    = 3,
    SOULS_EMEVD_SEKIRO          = 4  /* and Elden Ring */
} SoulsEmevdGame;

/* The type an instruction argument is packed as. */
typedef enum SoulsEmevdArgType {
    SOULS_EMEVD_ARG_BYTE   = 0,
    SOULS_EMEVD_ARG_UINT16 = 1,
    SOULS_EMEVD_ARG_UINT32 = 2,
    SOULS_EMEVD_ARG_SBYTE  = 3,
    SOULS_EMEVD_ARG_INT16  = 4,
    SOULS_EMEVD_ARG_INT32  = 5,
    SOULS_EMEVD_ARG_SINGLE = 6
} SoulsEmevdArgType;

SOULS_C_API SoulsEmevd* souls_emevd_new(SoulsEmevdGame game);
SOULS_C_API SoulsEmevd* souls_emevd_read_file(const char* path);
SOULS_C_API SoulsEmevd* souls_emevd_read_memory(const uint8_t* data, size_t size);
SOULS_C_API void souls_emevd_free(SoulsEmevd* emevd);
SOULS_C_API SoulsStatus souls_emevd_write_file(SoulsEmevd* emevd, const char* path);
SOULS_C_API SoulsBuffer souls_emevd_write_memory(SoulsEmevd* emevd);
SOULS_C_API SoulsEmevdGame souls_emevd_game(const SoulsEmevd* emevd);

SOULS_C_API size_t souls_emevd_event_count(const SoulsEmevd* emevd);
SOULS_C_API int64_t souls_emevd_event_id(const SoulsEmevd* emevd, size_t event);
SOULS_C_API SoulsStatus souls_emevd_set_event_id(SoulsEmevd* emevd, size_t event, int64_t id);
/* Index of the event with this ID, or -1. */
SOULS_C_API int64_t souls_emevd_find_event(const SoulsEmevd* emevd, int64_t id);
/* 0 = no effect when resting, 1 = restarts, 2 = ends. */
SOULS_C_API uint32_t souls_emevd_event_rest_behavior(const SoulsEmevd* emevd, size_t event);
SOULS_C_API SoulsStatus souls_emevd_set_event_rest_behavior(SoulsEmevd* emevd, size_t event, uint32_t behavior);
/* Borrowed. NULL if the event has no name (names live in .emeld files). */
SOULS_C_API const char* souls_emevd_event_name(const SoulsEmevd* emevd, size_t event);
SOULS_C_API SoulsStatus souls_emevd_set_event_name(SoulsEmevd* emevd, size_t event, const char* name_or_null);
SOULS_C_API int64_t souls_emevd_add_event(SoulsEmevd* emevd, int64_t id);
SOULS_C_API SoulsStatus souls_emevd_remove_event(SoulsEmevd* emevd, size_t event);

SOULS_C_API size_t souls_emevd_instruction_count(const SoulsEmevd* emevd, size_t event);
SOULS_C_API int32_t souls_emevd_instruction_bank(const SoulsEmevd* emevd, size_t event, size_t instruction);
SOULS_C_API int32_t souls_emevd_instruction_id(const SoulsEmevd* emevd, size_t event, size_t instruction);
/* The raw argument bytes (borrowed). */
SOULS_C_API const uint8_t* souls_emevd_instruction_args(const SoulsEmevd* emevd, size_t event, size_t instruction, size_t* out_size);
SOULS_C_API SoulsStatus souls_emevd_set_instruction_args(SoulsEmevd* emevd, size_t event, size_t instruction, const uint8_t* data, size_t size);
SOULS_C_API SoulsStatus souls_emevd_set_instruction(SoulsEmevd* emevd, size_t event, size_t instruction, int32_t bank, int32_t id);
/* The layer mask (Elden Ring and Sekiro), if the instruction has one. Returns 1 and fills *out_layer if so, else 0. */
SOULS_C_API int souls_emevd_instruction_layer(const SoulsEmevd* emevd, size_t event, size_t instruction, uint32_t* out_layer);
SOULS_C_API SoulsStatus souls_emevd_set_instruction_layer(SoulsEmevd* emevd, size_t event, size_t instruction, int has_layer, uint32_t layer);
/* Appends an instruction and returns its index, or -1. */
SOULS_C_API int64_t souls_emevd_add_instruction(SoulsEmevd* emevd, size_t event, int32_t bank, int32_t id, const uint8_t* args, size_t args_size);
SOULS_C_API SoulsStatus souls_emevd_remove_instruction(SoulsEmevd* emevd, size_t event, size_t instruction);
/*
 * Argument bytes as numbers, for an argument layout given as SoulsEmevdArgType values. unpack writes one double per
 * argument (exact for every type but 64-bit values, which don't occur); pack builds the bytes from the doubles,
 * aligning each argument and padding the end to 4 bytes.
 */
SOULS_C_API SoulsStatus souls_emevd_unpack_args(const SoulsEmevd* emevd, size_t event, size_t instruction, const uint8_t* arg_types,
                                                size_t arg_count, double* out_values);
SOULS_C_API SoulsStatus souls_emevd_pack_args(SoulsEmevd* emevd, size_t event, size_t instruction, const uint8_t* arg_types,
                                              size_t arg_count, const double* values);

/* Parameters: bytes of an event's own arguments copied into its instructions when it starts. */
SOULS_C_API size_t souls_emevd_parameter_count(const SoulsEmevd* emevd, size_t event);
SOULS_C_API SoulsStatus souls_emevd_parameter(const SoulsEmevd* emevd, size_t event, size_t parameter, int64_t* instruction_index,
                                              int64_t* target_start_byte, int64_t* source_start_byte, int32_t* byte_count, int32_t* unk_id);
SOULS_C_API int64_t souls_emevd_add_parameter(SoulsEmevd* emevd, size_t event, int64_t instruction_index, int64_t target_start_byte,
                                              int64_t source_start_byte, int32_t byte_count, int32_t unk_id);
SOULS_C_API SoulsStatus souls_emevd_remove_parameter(SoulsEmevd* emevd, size_t event, size_t parameter);

/* String data shared by instructions, and offsets into it naming linked files. */
SOULS_C_API const uint8_t* souls_emevd_string_data(const SoulsEmevd* emevd, size_t* out_size);
SOULS_C_API SoulsStatus souls_emevd_set_string_data(SoulsEmevd* emevd, const uint8_t* data, size_t size);
SOULS_C_API size_t souls_emevd_linked_file_count(const SoulsEmevd* emevd);
SOULS_C_API int64_t souls_emevd_linked_file_offset(const SoulsEmevd* emevd, size_t index);
SOULS_C_API SoulsStatus souls_emevd_set_linked_file_offsets(SoulsEmevd* emevd, const int64_t* offsets, size_t count);

/* ------------------------------------------------------------------------------------------------------------ */
/* TAE3 (animation events)                                                                                      */
/* ------------------------------------------------------------------------------------------------------------ */

/*
 * The events attached to a character's animations (sounds, effects, damage windows, ...). Each event has a type and
 * a set of fields that depend on the type; fields are read and written by name, like MSB properties. Many field
 * names are still just their offset ("Unk04"); the ones that are known have real names. The typed fields cover the Dark Souls III
 * layout of the format; Sekiro and Elden Ring files (version 0x1000D) are read and written with raw events.
 */
typedef struct SoulsTae3 SoulsTae3;

SOULS_C_API SoulsTae3* souls_tae3_new(void);
SOULS_C_API SoulsTae3* souls_tae3_read_file(const char* path);
SOULS_C_API SoulsTae3* souls_tae3_read_memory(const uint8_t* data, size_t size);
SOULS_C_API void souls_tae3_free(SoulsTae3* tae);
SOULS_C_API SoulsStatus souls_tae3_write_file(SoulsTae3* tae, const char* path);
SOULS_C_API SoulsBuffer souls_tae3_write_memory(SoulsTae3* tae);

/* 0x1000C (Dark Souls III) or 0x1000D (Sekiro, Elden Ring). Events of 0x1000D files are raw: see below. */
SOULS_C_API int32_t souls_tae3_version(const SoulsTae3* tae);
SOULS_C_API SoulsStatus souls_tae3_set_version(SoulsTae3* tae, int32_t version);
SOULS_C_API int32_t souls_tae3_id(const SoulsTae3* tae);
SOULS_C_API SoulsStatus souls_tae3_set_id(SoulsTae3* tae, int32_t id);
/* Borrowed. */
SOULS_C_API const char* souls_tae3_skeleton_name(const SoulsTae3* tae);
SOULS_C_API const char* souls_tae3_sib_name(const SoulsTae3* tae);
SOULS_C_API SoulsStatus souls_tae3_set_names(SoulsTae3* tae, const char* skeleton_name, const char* sib_name);

/* The event types that have a layout, with the number the file stores for each. */
SOULS_C_API size_t souls_tae3_type_count(void);
SOULS_C_API int64_t souls_tae3_type_number(size_t type_index);
SOULS_C_API const char* souls_tae3_type_name(size_t type_index);

SOULS_C_API size_t souls_tae3_animation_count(const SoulsTae3* tae);
SOULS_C_API int64_t souls_tae3_animation_id(const SoulsTae3* tae, size_t animation);
SOULS_C_API SoulsStatus souls_tae3_set_animation_id(SoulsTae3* tae, size_t animation, int64_t id);
SOULS_C_API int64_t souls_tae3_find_animation(const SoulsTae3* tae, int64_t id);
SOULS_C_API int64_t souls_tae3_add_animation(SoulsTae3* tae, int64_t id);
SOULS_C_API SoulsStatus souls_tae3_remove_animation(SoulsTae3* tae, size_t animation);
/* An animation can just point at another animation file instead of having events. */
SOULS_C_API int souls_tae3_animation_is_reference(const SoulsTae3* tae, size_t animation);
SOULS_C_API const char* souls_tae3_animation_file_name(const SoulsTae3* tae, size_t animation);
SOULS_C_API SoulsStatus souls_tae3_set_animation_reference(SoulsTae3* tae, size_t animation, int is_reference, const char* file_name);

SOULS_C_API size_t souls_tae3_event_count(const SoulsTae3* tae, size_t animation);
SOULS_C_API double souls_tae3_event_start(const SoulsTae3* tae, size_t animation, size_t event);
SOULS_C_API double souls_tae3_event_end(const SoulsTae3* tae, size_t animation, size_t event);
SOULS_C_API SoulsStatus souls_tae3_set_event_times(SoulsTae3* tae, size_t animation, size_t event, double start, double end);
/* The event type number (see souls_tae3_type_number) and its name (borrowed). */
SOULS_C_API int64_t souls_tae3_event_type(const SoulsTae3* tae, size_t animation, size_t event);
SOULS_C_API const char* souls_tae3_event_type_name(const SoulsTae3* tae, size_t animation, size_t event);
/* Adds an event of the given type with default fields and returns its index, or -1. */
SOULS_C_API int64_t souls_tae3_add_event(SoulsTae3* tae, size_t animation, int64_t type_number, double start, double end);
/* Events are referred to by event groups by index, so removing one shifts the later indices. */
SOULS_C_API SoulsStatus souls_tae3_remove_event(SoulsTae3* tae, size_t animation, size_t event);

/*
 * Events in 0x1000D files (Sekiro and Elden Ring) are raw: the library doesn't know the layouts of their event types,
 * so it keeps each event's fields as bytes (everything after its type, including alignment padding) and writes them back
 * unchanged. The field functions below fail with SOULS_ERR_UNSUPPORTED for them. Their type names are NULL, because the
 * same number means different things in different games.
 */
SOULS_C_API int souls_tae3_event_is_raw(const SoulsTae3* tae, size_t animation, size_t event);
SOULS_C_API const uint8_t* souls_tae3_event_raw_data(const SoulsTae3* tae, size_t animation, size_t event, size_t* out_size);
SOULS_C_API SoulsStatus souls_tae3_set_event_raw_data(SoulsTae3* tae, size_t animation, size_t event, const uint8_t* data, size_t size);
/* Appends a raw event (for 0x1000D files) with any type number and returns its index, or -1. */
SOULS_C_API int64_t souls_tae3_add_raw_event(SoulsTae3* tae, size_t animation, int64_t type_number, double start, double end,
                                             const uint8_t* data, size_t size);

SOULS_C_API size_t souls_tae3_field_count(const SoulsTae3* tae, size_t animation, size_t event);
SOULS_C_API const char* souls_tae3_field_name(const SoulsTae3* tae, size_t animation, size_t event, size_t field);
SOULS_C_API SoulsPropertyKind souls_tae3_field_kind(const SoulsTae3* tae, size_t animation, size_t event, size_t field);
/* Numeric and boolean fields, as doubles. Fields of other kinds fail with SOULS_ERR_INVALID_ARGUMENT. */
SOULS_C_API SoulsStatus souls_tae3_get_number(const SoulsTae3* tae, size_t animation, size_t event, const char* field, double* out_value);
SOULS_C_API SoulsStatus souls_tae3_set_number(SoulsTae3* tae, size_t animation, size_t event, const char* field, double value);
/* Byte-array and integer-array fields (fixed length). */
SOULS_C_API SoulsStatus souls_tae3_get_bytes(const SoulsTae3* tae, size_t animation, size_t event, const char* field, uint8_t* out,
                                             size_t capacity, size_t* out_count);
SOULS_C_API SoulsStatus souls_tae3_set_bytes(SoulsTae3* tae, size_t animation, size_t event, const char* field, const uint8_t* data, size_t count);
SOULS_C_API SoulsStatus souls_tae3_get_int_array(const SoulsTae3* tae, size_t animation, size_t event, const char* field, int64_t* out,
                                                 size_t capacity, size_t* out_count);
SOULS_C_API SoulsStatus souls_tae3_set_int_array(SoulsTae3* tae, size_t animation, size_t event, const char* field, const int64_t* values, size_t count);

/* Event groups tie events of one type together; they hold event indices. */
SOULS_C_API size_t souls_tae3_group_count(const SoulsTae3* tae, size_t animation);
SOULS_C_API int64_t souls_tae3_group_type(const SoulsTae3* tae, size_t animation, size_t group);
SOULS_C_API SoulsStatus souls_tae3_group_indices(const SoulsTae3* tae, size_t animation, size_t group, int32_t* out, size_t capacity, size_t* out_count);
SOULS_C_API int64_t souls_tae3_add_group(SoulsTae3* tae, size_t animation, int64_t type_number, const int32_t* indices, size_t count);
SOULS_C_API SoulsStatus souls_tae3_remove_group(SoulsTae3* tae, size_t animation, size_t group);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* LIBSOULS_CAPI_SOULS_H */
