/*
 * Exercises the C API from a C translation unit, which proves souls.h is valid C (no C++ leaks into it) and that the
 * functions behave through plain C calls. Returns the number of failed checks.
 */
#include <libSouls/CApi/souls.h>

#include <stdio.h>
#include <string.h>

static int failures = 0;

#define CHECK(cond)                                                         \
    do {                                                                    \
        if (!(cond)) {                                                      \
            printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);         \
            ++failures;                                                     \
        }                                                                   \
    } while (0)

static void test_fmg(void) {
    SoulsFMG* fmg = souls_fmg_new(SOULS_FMG_DARK_SOULS_3);
    CHECK(fmg != NULL);
    CHECK(souls_fmg_set_text(fmg, 200, "Second") == SOULS_OK);
    CHECK(souls_fmg_set_text(fmg, 100, "First") == SOULS_OK);
    CHECK(souls_fmg_set_text(fmg, 300, NULL) == SOULS_OK);
    CHECK(souls_fmg_entry_count(fmg) == 3);
    CHECK(souls_fmg_get_text(fmg, 300) == NULL);
    CHECK(souls_fmg_get_text(fmg, 999) == NULL);

    SoulsBuffer bytes = souls_fmg_write_memory(fmg);
    CHECK(bytes.data != NULL && bytes.size > 0);

    SoulsFMG* back = souls_fmg_read_memory(bytes.data, bytes.size);
    CHECK(back != NULL);
    if (back) {
        CHECK(souls_fmg_version(back) == SOULS_FMG_DARK_SOULS_3);
        CHECK(souls_fmg_get_text(back, 100) != NULL && strcmp(souls_fmg_get_text(back, 100), "First") == 0);
        CHECK(souls_fmg_get_text(back, 200) != NULL && strcmp(souls_fmg_get_text(back, 200), "Second") == 0);
        /* written sorted by ID */
        CHECK(souls_fmg_entry_id(back, 0) == 100);
        CHECK(souls_fmg_set_text(back, 100, "Changed") == SOULS_OK);
        CHECK(strcmp(souls_fmg_get_text(back, 100), "Changed") == 0);
        CHECK(souls_fmg_remove_entry(back, 99) == SOULS_ERR_INVALID_ARGUMENT);
    }
    souls_buffer_free(bytes);
    souls_fmg_free(back);
    souls_fmg_free(fmg);
}

static void test_binder(void) {
    const uint8_t hello[] = {'h', 'e', 'l', 'l', 'o'};
    SoulsBinder* bnd = souls_binder_new(SOULS_BINDER_BND4);
    CHECK(bnd != NULL);
    CHECK(souls_binder_kind(bnd) == SOULS_BINDER_BND4);
    CHECK(souls_binder_set_compression(bnd, SOULS_DCX_NONE) == SOULS_OK);
    CHECK(souls_binder_add_file(bnd, 7, "N:\\test\\hello.txt", SOULS_FILE_FLAG1, hello, sizeof hello) == 0);
    CHECK(souls_binder_add_file(bnd, 8, "N:\\test\\second.bin", SOULS_FILE_FLAG1, hello, 2) == 1);

    SoulsBuffer bytes = souls_binder_write_memory(bnd);
    CHECK(bytes.data != NULL);
    if (!bytes.data) printf("  error: %s\n", souls_last_error());
    SoulsBinder* back = souls_binder_read_memory(bytes.data, bytes.size);
    CHECK(back != NULL);
    if (back) {
        size_t size = 0;
        CHECK(souls_binder_kind(back) == SOULS_BINDER_BND4);
        CHECK(souls_binder_file_count(back) == 2);
        CHECK(souls_binder_find_file_suffix(back, "hello.txt") == 0);
        CHECK(souls_binder_find_file(back, "hello.txt") == -1);
        CHECK(souls_binder_file_id(back, 0) == 7);
        const uint8_t* data = souls_binder_file_bytes(back, 0, &size);
        CHECK(data != NULL && size == 5 && memcmp(data, "hello", 5) == 0);
        CHECK(souls_binder_file_name(back, 1) != NULL);
        CHECK(souls_binder_set_file_bytes(back, 1, hello, sizeof hello) == SOULS_OK);
        CHECK(souls_binder_remove_file(back, 0) == SOULS_OK);
        CHECK(souls_binder_file_count(back) == 1);
        CHECK(souls_binder_file_bytes(back, 5, &size) == NULL);
        CHECK(souls_last_error_code() == SOULS_ERR_INVALID_ARGUMENT);
    }
    souls_buffer_free(bytes);
    souls_binder_free(back);
    souls_binder_free(bnd);
}

static void test_dcx(void) {
    uint8_t raw[1000];
    size_t i;
    for (i = 0; i < sizeof raw; ++i) raw[i] = (uint8_t)(i % 7);

    SoulsBuffer packed = souls_dcx_compress(raw, sizeof raw, souls_dcx_default_type(SOULS_GAME_DARK_SOULS_3));
    CHECK(packed.data != NULL);
    CHECK(souls_dcx_is(packed.data, packed.size) == 1);
    CHECK(souls_dcx_is(raw, sizeof raw) == 0);

    SoulsDcxType kind = SOULS_DCX_UNKNOWN;
    SoulsBuffer unpacked = souls_dcx_decompress(packed.data, packed.size, &kind);
    CHECK(unpacked.size == sizeof raw && memcmp(unpacked.data, raw, sizeof raw) == 0);
    CHECK(kind == SOULS_DCX_DCX_DFLT_10000_44_9);
    souls_buffer_free(unpacked);
    souls_buffer_free(packed);

    SoulsBuffer bad = souls_dcx_compress(raw, sizeof raw, (SoulsDcxType)99);
    CHECK(bad.data == NULL && souls_last_error_code() == SOULS_ERR_INVALID_ARGUMENT);
}

static void test_errors(void) {
    const uint8_t junk[] = {1, 2, 3, 4, 5, 6, 7, 8};
    SoulsFMG* fmg = souls_fmg_read_memory(junk, sizeof junk);
    CHECK(fmg == NULL);
    CHECK(souls_last_error_code() != SOULS_OK);
    CHECK(strlen(souls_last_error()) > 0);

    CHECK(souls_binder_read_memory(junk, sizeof junk) == NULL);
    CHECK(souls_binder_read_file("this/file/does/not/exist.bnd") == NULL);
    CHECK(souls_last_error_code() == SOULS_ERR_IO);

    CHECK(souls_fmg_set_text(NULL, 1, "x") == SOULS_ERR_INVALID_ARGUMENT);
    CHECK(souls_fmg_entry_count(NULL) == 0);
    CHECK(souls_fmg_read_file(NULL) == NULL);

    /* freeing NULL is always fine */
    souls_fmg_free(NULL);
    souls_binder_free(NULL);
    souls_param_free(NULL);
    souls_paramdefs_free(NULL);
    souls_param_layout_free(NULL);
    souls_tpf_free(NULL);
    souls_buffer_free(souls_read_file("this/file/does/not/exist"));

    CHECK(strlen(souls_version()) > 0);
}

int RunCApiSmoke(void) {
    failures = 0;
    test_fmg();
    test_binder();
    test_dcx();
    test_errors();
    return failures;
}
