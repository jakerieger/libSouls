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
    SOULS_ERR_INTERNAL         = 5  /* anything unexpected */
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
    SOULS_BINDER_BND4 = 4
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

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* LIBSOULS_CAPI_SOULS_H */
