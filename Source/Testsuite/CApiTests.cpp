//
// Created by Jake Rieger on 10/9/2026.
//

#include "TestResult.hpp"

#include <libSouls/CApi/souls.h>
#include <libSouls/Formats/PARAM.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

extern "C" int RunCApiSmoke(void);

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

    // A param written by the C++ library, read back through the C API.
    void TestParamRows() {
        Souls::PARAM Param;
        Param.ParamType           = "TEST_PARAM_ST";
        Param.ParamdefDataVersion = 7;
        Param.Format2D            = Souls::PARAM::FormatFlags1::LongDataOffset | Souls::PARAM::FormatFlags1::OffsetParamType;
        Param.Format2E            = Souls::PARAM::FormatFlags2::UnicodeRowNames;
        for (int32_t I = 0; I < 3; ++I) {
            Param.Rows.push_back(Souls::PARAM::Row(100 + I * 10, "Row " + std::to_string(I), std::vector<uint8_t>(8, static_cast<uint8_t>(I + 1))));
        }
        const std::vector<uint8_t> Bytes = Param.Write();

        SoulsParam* P = souls_param_read_memory(Bytes.data(), Bytes.size());
        CHECK(P != nullptr);
        if (!P) return;
        CHECK(std::strcmp(souls_param_type(P), "TEST_PARAM_ST") == 0);
        CHECK(souls_param_row_count(P) == 3);
        CHECK(souls_param_find_row(P, 110) == 1);
        CHECK(souls_param_find_row(P, 111) == -1);
        CHECK(souls_param_row_id(P, 2) == 120);
        CHECK(std::strcmp(souls_param_row_name(P, 1), "Row 1") == 0);

        size_t Size = 0;
        const uint8_t* Row = souls_param_row_bytes(P, 1, &Size);
        CHECK(Row != nullptr && Size == 8 && Row[0] == 2);

        CHECK(souls_param_set_row_name(P, 1, nullptr) == SOULS_OK);
        CHECK(souls_param_row_name(P, 1) == nullptr);
        CHECK(souls_param_set_row_id(P, 0, 99) == SOULS_OK);
        CHECK(souls_param_remove_row(P, 2) == SOULS_OK);

        SoulsBuffer Out = souls_param_write_memory(P);
        SoulsParam* Back = souls_param_read_memory(Out.data, Out.size);
        CHECK(Back != nullptr && souls_param_row_count(Back) == 2 && souls_param_row_id(Back, 0) == 99);
        souls_param_free(Back);
        souls_buffer_free(Out);

        // No defs loaded: no layout, and the error says so.
        SoulsParamDefs* Defs = souls_paramdefs_new();
        CHECK(souls_param_layout_create(P, Defs) == nullptr);
        CHECK(souls_last_error_code() == SOULS_ERR_NOT_FOUND);
        CHECK(souls_paramdefs_add_folder(Defs, "this/folder/does/not/exist", 1) != SOULS_OK);
        souls_paramdefs_free(Defs);
        souls_param_free(P);
    }

    fs::path FindParamdex() {
        if (const char* Env = std::getenv("PARAMDEX_PATH")) return Env;
        if (const char* Home = std::getenv("USERPROFILE")) return fs::path(Home) / "Code/GitHub/Paramdex";
        return {};
    }

    // Elden Ring: decrypt regulation.bin, edit a weapon through a layout, write it back through the binder.
    int TestEldenRing() {
        const fs::path Game = "C:/Program Files (x86)/Steam/steamapps/common/ELDEN RING/Game";
        const fs::path Defs = FindParamdex() / "ER" / "Defs";
        if (!fs::exists(Game / "regulation.bin") || !fs::exists(Defs)) {
            std::printf("Elden Ring C API checks skipped (needs the game and Paramdex)\n");
            return TestSkipped;
        }

        const std::string RegulationPath = (Game / "regulation.bin").string();
        SoulsBinder* Bnd                 = souls_regulation_decrypt(SOULS_GAME_ELDEN_RING, RegulationPath.c_str());
        CHECK(Bnd != nullptr);
        if (!Bnd) return 0;

        const int64_t Index = souls_binder_find_file_suffix(Bnd, "EquipParamWeapon.param");
        CHECK(Index >= 0);
        if (Index < 0) {
            souls_binder_free(Bnd);
            return 0;
        }
        size_t Size = 0;
        const uint8_t* Data = souls_binder_file_bytes(Bnd, static_cast<size_t>(Index), &Size);
        SoulsParam* Param   = souls_param_read_memory(Data, Size);
        CHECK(Param != nullptr);

        SoulsParamDefs* Repo = souls_paramdefs_new();
        const std::string DefsPath = Defs.string();
        CHECK(souls_paramdefs_add_folder(Repo, DefsPath.c_str(), 1) == SOULS_OK);
        CHECK(souls_paramdefs_count(Repo) > 0);

        SoulsParamLayout* Layout = souls_param_layout_create(Param, Repo);
        CHECK(Layout != nullptr);
        if (Layout) {
            const int64_t Dagger = souls_param_find_row(Param, 1000000);
            CHECK(Dagger >= 0);
            double Weight = 0;
            CHECK(souls_param_get_number(Layout, Param, static_cast<size_t>(Dagger), "weight", &Weight) == SOULS_OK);
            CHECK(Weight > 0);
            CHECK(souls_param_set_number(Layout, Param, static_cast<size_t>(Dagger), "weight", 4.5) == SOULS_OK);
            double Again = 0;
            souls_param_get_number(Layout, Param, static_cast<size_t>(Dagger), "weight", &Again);
            CHECK(Again == 4.5);
            CHECK(souls_param_get_number(Layout, Param, static_cast<size_t>(Dagger), "no_such_field", &Again) != SOULS_OK);
            CHECK(souls_param_layout_find_field(Layout, "weight") >= 0);

            const int64_t Added = souls_param_add_row(Layout, Param, 999999999, "C API test");
            CHECK(Added >= 0 && souls_param_row_id(Param, static_cast<size_t>(Added)) == 999999999);

            // Put it back in the binder and read the whole thing through again (without re-encrypting the game's file).
            SoulsBuffer Packed = souls_param_write_memory(Param);
            CHECK(souls_binder_set_file_bytes(Bnd, static_cast<size_t>(Index), Packed.data, Packed.size) == SOULS_OK);
            souls_buffer_free(Packed);
            // Recompressing the whole regulation with zlib is very slow in a Debug build, so write it uncompressed.
            CHECK(souls_binder_set_compression(Bnd, SOULS_DCX_NONE) == SOULS_OK);
            SoulsBuffer BndBytes = souls_binder_write_memory(Bnd);
            SoulsBinder* Reread  = souls_binder_read_memory(BndBytes.data, BndBytes.size);
            CHECK(Reread != nullptr && souls_binder_file_count(Reread) == souls_binder_file_count(Bnd));
            souls_binder_free(Reread);
            souls_buffer_free(BndBytes);
        }
        souls_param_layout_free(Layout);
        souls_paramdefs_free(Repo);
        souls_param_free(Param);
        souls_binder_free(Bnd);
        return 0;
    }

    // Dark Souls Remastered: a real TPF through the C API.
    int TestTpf() {
        const fs::path Path = "C:/Program Files (x86)/Steam/steamapps/common/DARK SOULS REMASTERED/menu/menu_0.tpf.dcx";
        SoulsTPF* Empty     = souls_tpf_new();
        CHECK(Empty != nullptr && souls_tpf_texture_count(Empty) == 0);
        SoulsBuffer EmptyBytes = souls_tpf_write_memory(Empty);
        SoulsTPF* EmptyBack    = souls_tpf_read_memory(EmptyBytes.data, EmptyBytes.size);
        CHECK(EmptyBack != nullptr && souls_tpf_texture_count(EmptyBack) == 0);
        souls_tpf_free(EmptyBack);
        souls_buffer_free(EmptyBytes);
        souls_tpf_free(Empty);

        CHECK(souls_tpf_add_texture(nullptr, "x", 0, 0, nullptr, 0) == -1);

        if (!fs::exists(Path)) {
            std::printf("Real TPF C API checks skipped (no Dark Souls Remastered install)\n");
            return TestSkipped;
        }
        const std::string PathText = Path.string();
        SoulsTPF* Tpf              = souls_tpf_read_file(PathText.c_str());
        CHECK(Tpf != nullptr);
        if (!Tpf) return 0;
        const size_t Count = souls_tpf_texture_count(Tpf);
        CHECK(Count > 0);
        size_t Size = 0;
        const uint8_t* Dds = souls_tpf_texture_bytes(Tpf, 0, &Size);
        CHECK(Dds != nullptr && Size > 4 && std::memcmp(Dds, "DDS ", 4) == 0);
        CHECK(souls_tpf_texture_name(Tpf, 0) != nullptr);
        CHECK(souls_tpf_texture_bytes(Tpf, Count, &Size) == nullptr);

        const int64_t Added = souls_tpf_add_texture(Tpf, "copy", 0, 0, Dds, Size);
        CHECK(Added == static_cast<int64_t>(Count));
        CHECK(souls_tpf_add_texture(Tpf, "bad", 0, 0, reinterpret_cast<const uint8_t*>("not a dds"), 9) == -1);

        SoulsBuffer Out = souls_tpf_write_memory(Tpf);
        SoulsTPF* Back  = souls_tpf_read_memory(Out.data, Out.size);
        CHECK(Back != nullptr && souls_tpf_texture_count(Back) == Count + 1);
        souls_tpf_free(Back);
        souls_buffer_free(Out);
        souls_tpf_free(Tpf);
        return 0;
    }
}  // namespace

int RunCApiTests() {
    Failures = 0;
    Failures += RunCApiSmoke();
    TestParamRows();
    const int Er  = TestEldenRing();
    const int Tpf = TestTpf();
    std::printf("C API tests: %s\n", Failures ? "FAILED" : "ok");
    if (Failures) return 1;
    return Er == TestSkipped && Tpf == TestSkipped ? TestSkipped : 0;
}
