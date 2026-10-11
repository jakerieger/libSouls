// Copyright (C) 2026 Jake Rieger
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, version 3. This program is distributed in the hope that it will
// be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU General
// Public License along with this program. If not, see https://www.gnu.org/licenses/.

#include "TestPaths.hpp"
#include "TestResult.hpp"

#include <libSouls/CApi/souls.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

namespace {
    int Failures = 0;

#define CHECK(Cond)                                                                                                    \
    do {                                                                                                               \
        if (!(Cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #Cond);                                                \
            ++Failures;                                                                                                \
        }                                                                                                              \
    } while (0)

    namespace fs = std::filesystem;

}  // namespace

// Row names and enum names from Paramdex, and field descriptions from its paramdefs.
int TestCApiParamMetadata() {
    Failures                = 0;
    const fs::path Paramdex = TestPaths::Paramdex() / "ER";
    const fs::path Game     = "C:/Program Files (x86)/Steam/steamapps/common/ELDEN RING/Game";
    if (!fs::exists(Paramdex / "Names") || !fs::exists(Paramdex / "Tdfs") || !fs::exists(Paramdex / "Defs")) {
        std::printf("Param metadata C API checks skipped (needs Paramdex)\n");
        return TestSkipped;
    }

    const std::string NamesPath = (Paramdex / "Names").string();
    SoulsParamNames* Names      = souls_param_names_new();
    CHECK(Names != nullptr);
    CHECK(souls_param_names_add_folder(Names, NamesPath.c_str()) == SOULS_OK);
    CHECK(souls_param_names_param_count(Names) > 50);
    CHECK(souls_param_names_load_error_count(Names) == 0);
    const char* Unarmed = souls_param_names_find(Names, "EquipParamWeapon", 110000);
    CHECK(Unarmed != nullptr && std::strcmp(Unarmed, "Unarmed") == 0);
    CHECK(souls_param_names_find(Names, "equipparamweapon", 110000) != nullptr);  // case is ignored
    CHECK(souls_param_names_find(Names, "EquipParamWeapon", 7) == nullptr);
    CHECK(souls_param_names_find(Names, "NoSuchParam", 1) == nullptr);
    CHECK(souls_param_names_add_folder(Names, "this/folder/does/not/exist") != SOULS_OK);

    const std::string TdfPath = (Paramdex / "Tdfs").string();
    SoulsParamEnums* Enums    = souls_param_enums_new();
    CHECK(souls_param_enums_add_folder(Enums, TdfPath.c_str()) == SOULS_OK);
    CHECK(souls_param_enums_count(Enums) > 0 && souls_param_enums_name(Enums, 0) != nullptr);
    CHECK(souls_param_enums_entry_count(Enums, "GAITEM_CATEGORY") >= 5);
    int64_t Value = -1;
    CHECK(souls_param_enums_find_value(Enums, "gaitem_category", "Good", &Value) == SOULS_OK && Value == 3);
    const char* Good = souls_param_enums_find_name(Enums, "GAITEM_CATEGORY", 3);
    CHECK(Good != nullptr && std::strcmp(Good, "Good") == 0);
    CHECK(souls_param_enums_find_name(Enums, "GAITEM_CATEGORY", 999) == nullptr);
    CHECK(souls_param_enums_find_value(Enums, "GAITEM_CATEGORY", "Nope", &Value) == SOULS_ERR_NOT_FOUND);
    CHECK(souls_param_enums_entry_value(Enums, "NO_SUCH_ENUM", 0) == 0 &&
          souls_last_error_code() == SOULS_ERR_NOT_FOUND);
    CHECK(std::strcmp(souls_param_enums_entry_name(Enums, "GAITEM_CATEGORY", 0), "Weapon") == 0);

    if (fs::exists(Game / "regulation.bin")) {
        const std::string Regulation = (Game / "regulation.bin").string();
        SoulsBinder* Bnd             = souls_regulation_decrypt(SOULS_GAME_ELDEN_RING, Regulation.c_str());
        CHECK(Bnd != nullptr);
        const int64_t Index = Bnd ? souls_binder_find_file_suffix(Bnd, "EquipParamWeapon.param") : -1;
        CHECK(Index >= 0);
        if (Index >= 0) {
            size_t Size         = 0;
            const uint8_t* Data = souls_binder_file_bytes(Bnd, static_cast<size_t>(Index), &Size);
            SoulsParam* Param   = souls_param_read_memory(Data, Size);
            CHECK(Param != nullptr);

            const int64_t Named = souls_param_names_apply(Names, Param, "EquipParamWeapon", 0);
            CHECK(Named > 100);
            const int64_t Row = souls_param_find_row(Param, 110000);
            CHECK(Row >= 0 && souls_param_row_name(Param, static_cast<size_t>(Row)) != nullptr &&
                  std::strcmp(souls_param_row_name(Param, static_cast<size_t>(Row)), "Unarmed") == 0);
            CHECK(souls_param_names_apply(Names, Param, "NoSuchParam", 0) == 0);

            const std::string DefsPath = (Paramdex / "Defs").string();
            SoulsParamDefs* Defs       = souls_paramdefs_new();
            CHECK(souls_paramdefs_add_folder(Defs, DefsPath.c_str(), 1) == SOULS_OK);
            SoulsParamLayout* Layout = souls_param_layout_create(Param, Defs);
            CHECK(Layout != nullptr);
            if (Layout) {
                const int64_t Field = souls_param_layout_find_field(Layout, "weight");
                CHECK(Field >= 0);
                SoulsFieldInfo Info;
                std::memset(&Info, 0, sizeof Info);
                Info.struct_size = sizeof Info;
                CHECK(souls_param_layout_field_info(Layout, static_cast<size_t>(Field), &Info) == SOULS_OK);
                CHECK(Info.display_name != nullptr && std::strlen(Info.display_name) > 0);
                CHECK(Info.internal_type != nullptr && Info.display_format != nullptr);
                CHECK(Info.has_minimum && Info.has_maximum && Info.minimum <= Info.maximum);
                SoulsFieldInfo TooSmall {};
                TooSmall.struct_size = 4;
                CHECK(souls_param_layout_field_info(Layout, static_cast<size_t>(Field), &TooSmall) ==
                      SOULS_ERR_INVALID_ARGUMENT);
                CHECK(souls_param_layout_field_info(Layout, 1000000, &Info) == SOULS_ERR_INVALID_ARGUMENT);
            }
            souls_param_layout_free(Layout);
            souls_paramdefs_free(Defs);
            souls_param_free(Param);
        }
        souls_binder_free(Bnd);
    }
    souls_param_enums_free(Enums);
    souls_param_names_free(Names);
    return Failures;
}
