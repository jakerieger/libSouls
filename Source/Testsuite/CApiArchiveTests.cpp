//
// Created by Jake Rieger on 10/11/2026.
//

#include "TestResult.hpp"

#include <libSouls/CApi/souls.h>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace {
    int Failures = 0;

#define CHECK(Cond)                                                     \
    do {                                                                \
        if (!(Cond)) {                                                  \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #Cond); \
            ++Failures;                                                 \
        }                                                               \
    } while (0)

    namespace fs = std::filesystem;

    void TestBxf(SoulsBinderKind Kind, const char* Label) {
        const fs::path Dir = fs::temp_directory_path() / (std::string("souls_capi_") + Label);
        fs::create_directories(Dir);
        const std::string Header = (Dir / "test.bhd").string();
        const std::string Data   = (Dir / "test.bdt").string();

        SoulsBinder* Bxf = souls_binder_new(Kind);
        CHECK(Bxf != nullptr && souls_binder_kind(Bxf) == Kind);
        if (!Bxf) return;
        const uint8_t First[]  = {1, 2, 3, 4, 5};
        const uint8_t Second[] = {9, 8, 7};
        CHECK(souls_binder_add_file(Bxf, 0, "N:\\data\\first.bin", SOULS_FILE_FLAG1, First, sizeof First) == 0);
        CHECK(souls_binder_add_file(Bxf, 1, "N:\\data\\second.bin", SOULS_FILE_FLAG1, Second, sizeof Second) == 1);

        // Split binders are written with their own functions.
        CHECK(souls_binder_write_file(Bxf, Header.c_str()) == SOULS_ERR_UNSUPPORTED);
        CHECK(souls_binder_set_compression(Bxf, SOULS_DCX_NONE) == SOULS_ERR_UNSUPPORTED);
        CHECK(souls_bxf_write_files(Bxf, Header.c_str(), Data.c_str()) == SOULS_OK);

        SoulsBuffer MemHeader{}, MemData{};
        CHECK(souls_bxf_write_memory(Bxf, &MemHeader, &MemData) == SOULS_OK && MemHeader.size > 0 && MemData.size > 0);
        SoulsBinder* Whole = souls_bxf_read_memory(Kind, MemHeader.data, MemHeader.size, MemData.data, MemData.size);
        CHECK(Whole != nullptr && souls_binder_file_count(Whole) == 2 && souls_binder_find_file_suffix(Whole, "second.bin") == 1);
        souls_binder_free(Whole);
        souls_buffer_free(MemHeader);
        souls_buffer_free(MemData);

        SoulsBinder* FromFiles = souls_bxf_read_files(Kind, Header.c_str(), Data.c_str());
        CHECK(FromFiles != nullptr && souls_binder_file_count(FromFiles) == 2);
        souls_binder_free(FromFiles);
        CHECK(souls_bxf_read_files(SOULS_BINDER_BND4, Header.c_str(), Data.c_str()) == nullptr);

        // One file at a time, without reading the rest.
        SoulsBxfReader* Reader = souls_bxf_reader_open(Header.c_str(), Data.c_str());
        CHECK(Reader != nullptr);
        if (Reader) {
            CHECK(souls_bxf_reader_kind(Reader) == Kind);
            CHECK(souls_bxf_reader_file_count(Reader) == 2);
            CHECK(souls_bxf_reader_file_id(Reader, 1) == 1);
            CHECK(souls_bxf_reader_file_name(Reader, 0) != nullptr);
            CHECK(souls_bxf_reader_find_file_name(Reader, "second.bin") == 1);
            CHECK(souls_bxf_reader_find(Reader, "no such file", 0) == -1);
            CHECK(souls_bxf_reader_find_file_name(Reader, "SECOND.BIN") == 1);
            SoulsBuffer Read = souls_bxf_reader_read(Reader, 1);
            CHECK(Read.size == sizeof Second && std::memcmp(Read.data, Second, sizeof Second) == 0);
            souls_buffer_free(Read);
            SoulsBuffer Bad = souls_bxf_reader_read(Reader, 99);
            CHECK(Bad.data == nullptr && souls_last_error_code() == SOULS_ERR_INVALID_ARGUMENT);
            souls_bxf_reader_free(Reader);
        }
        souls_binder_free(Bxf);
        std::error_code Ec;
        fs::remove_all(Dir, Ec);
    }
}  // namespace

int TestCApiArchivesSynthetic() {
    Failures = 0;
    TestBxf(SOULS_BINDER_BXF3, "bxf3");
    TestBxf(SOULS_BINDER_BXF4, "bxf4");

    // Hashes are case and slash insensitive.
    CHECK(souls_path_hash32("/Map/Test.msb") == souls_path_hash32("\\map\\test.msb"));
    CHECK(souls_path_hash64("map/test.msb") == souls_path_hash64("/MAP/TEST.MSB"));
    CHECK(souls_path_hash64("/a") != souls_path_hash64("/b"));

    CHECK(souls_bhd5_open(SOULS_BHD5_ELDEN_RING, "this/file/does/not/exist.bhd", nullptr, nullptr) == nullptr);
    CHECK(souls_last_error_code() == SOULS_ERR_IO);
    const uint8_t Junk[] = {1, 2, 3, 4};
    CHECK(souls_bhd5_read_memory(SOULS_BHD5_ELDEN_RING, Junk, sizeof Junk) == nullptr);
    souls_bhd5_free(nullptr);
    souls_bdt_free(nullptr);
    souls_bxf_reader_free(nullptr);
    return Failures;
}

// The real Elden Ring archives: decrypt the headers with the keys in the exe, find files by path, and extract them.
int TestCApiArchivesElden() {
    Failures        = 0;
    const fs::path Game = "C:/Program Files (x86)/Steam/steamapps/common/ELDEN RING/Game";
    if (!fs::exists(Game / "eldenring.exe") || !fs::exists(Game / "Data0.bhd")) {
        std::printf("Real archive C API checks skipped (no Elden Ring install)\n");
        return TestSkipped;
    }
    const std::string Exe    = (Game / "eldenring.exe").string();
    const std::string Oodle  = (Game / "oo2core_6_win64.dll").string();
    if (fs::exists(Game / "oo2core_6_win64.dll")) souls_oodle_set_dll_path(Oodle.c_str());

    // Files that should be in the archives of every unmodified install.
    const char* Paths[] = {"/map/mapstudio/m10_00_00_00.msb.dcx", "/map/mapstudio/m11_00_00_00.msb.dcx",
                           "/map/mapstudio/m60_44_36_00.msb.dcx", "/chr/c0000.chrbnd.dcx",
                           "/msg/engus/item.msgbnd.dcx",          "/regulation.bin",
                           "/event/common.emevd.dcx",             "/menu/hi/00_solo.tpf.dcx"};

    int Found = 0;
    for (const char* Name : {"Data0", "Data1", "Data2", "Data3", "DLC"}) {
        const std::string Header = (Game / (std::string(Name) + ".bhd")).string();
        const std::string Data   = (Game / (std::string(Name) + ".bdt")).string();
        SoulsBhd5* Bhd           = souls_bhd5_open(SOULS_BHD5_ELDEN_RING, Header.c_str(), Exe.c_str(), nullptr);
        CHECK(Bhd != nullptr);
        if (!Bhd) {
            std::printf("  %s: %s\n", Name, souls_last_error());
            continue;
        }
        const size_t Count = souls_bhd5_file_count(Bhd);
        CHECK(Count > 0);
        CHECK(souls_bhd5_find_hash(Bhd, souls_bhd5_file_hash(Bhd, 0)) >= 0);
        CHECK(souls_bhd5_find_hash(Bhd, 12345) == -1);

        SoulsBdt* Bdt = souls_bdt_open(Data.c_str());
        CHECK(Bdt != nullptr);
        for (const char* Path : Paths) {
            const int64_t Index = souls_bhd5_find_path(Bhd, Path);
            if (Index < 0) continue;
            ++Found;
            SoulsBuffer File = souls_bhd5_extract(Bhd, Bdt, static_cast<size_t>(Index));
            std::printf("  %s: %s -> %zu bytes%s\n", Name, Path, File.size, souls_bhd5_file_is_encrypted(Bhd, static_cast<size_t>(Index)) ? " (encrypted)" : "");
            CHECK(File.size > 0);
            // Anything with .dcx in its name should be a DCX that decompresses.
            if (std::strstr(Path, ".dcx") && File.size > 0) {
                CHECK(souls_dcx_is(File.data, File.size) == 1);
                if (souls_oodle_is_available()) {
                    SoulsBuffer Plain = souls_dcx_decompress(File.data, File.size, nullptr);
                    CHECK(Plain.size > 0);
                    souls_buffer_free(Plain);
                }
            }
            souls_buffer_free(File);
        }
        souls_bdt_free(Bdt);
        souls_bhd5_free(Bhd);
    }
    // If the 64-bit hash or the extraction were wrong, none of these well-known paths would turn up.
    CHECK(Found >= 3);
    return Failures;
}
