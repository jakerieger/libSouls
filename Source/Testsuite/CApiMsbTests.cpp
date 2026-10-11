// Copyright (C) 2026 Jake Rieger
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, version 3. This program is distributed in the hope that it will
// be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU General
// Public License along with this program. If not, see https://www.gnu.org/licenses/.

#include "TestResult.hpp"

#include <libSouls/CApi/souls.h>

#include <cstdio>
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

// Every game's map layout, built and read back through the C API.
int TestCApiMsbSynthetic() {
    Failures = 0;
    for (SoulsMsbKind Kind : {SOULS_MSB_DARK_SOULS_1,
                              SOULS_MSB_DARK_SOULS_2,
                              SOULS_MSB_DARK_SOULS_3,
                              SOULS_MSB_SEKIRO,
                              SOULS_MSB_ELDEN_RING}) {
        SoulsMsb* Msb = souls_msb_new(Kind);
        CHECK(Msb != nullptr);
        if (!Msb) continue;
        CHECK(souls_msb_kind(Msb) == Kind);
        CHECK(souls_msb_type_count(Msb, SOULS_MSB_MODELS) > 0 && souls_msb_type_count(Msb, SOULS_MSB_PARTS) > 0);
        CHECK(souls_msb_type_name(Msb, SOULS_MSB_PARTS, 9999) == nullptr);

        const int64_t Model =
          souls_msb_add_entry(Msb, SOULS_MSB_MODELS, souls_msb_type_name(Msb, SOULS_MSB_MODELS, 0), "m1000");
        CHECK(Model >= 0);
        const int64_t Part =
          souls_msb_add_entry(Msb, SOULS_MSB_PARTS, souls_msb_type_name(Msb, SOULS_MSB_PARTS, 0), "m1000_0000");
        CHECK(Part >= 0);
        const int64_t Region =
          souls_msb_add_entry(Msb, SOULS_MSB_REGIONS, souls_msb_type_name(Msb, SOULS_MSB_REGIONS, 0), "Region A");
        CHECK(Region >= 0);
        CHECK(souls_msb_add_entry(Msb, SOULS_MSB_PARTS, "NoSuchType", nullptr) == -1);
        if (Model < 0 || Part < 0 || Region < 0) {
            souls_msb_free(Msb);
            continue;
        }

        CHECK(souls_msb_set_string(Msb, SOULS_MSB_PARTS, static_cast<size_t>(Part), "ModelName", "m1000") == SOULS_OK);
        const float Position[3] = {1.f, 2.f, 3.f};
        CHECK(souls_msb_set_vec3(Msb, SOULS_MSB_PARTS, static_cast<size_t>(Part), "Position", Position) == SOULS_OK);
        CHECK(souls_msb_set_vec3(Msb, SOULS_MSB_REGIONS, static_cast<size_t>(Region), "Position", Position) ==
              SOULS_OK);
        const float Box[3] = {4.f, 5.f, 6.f};
        CHECK(souls_msb_set_region_shape(Msb, static_cast<size_t>(Region), SOULS_SHAPE_BOX, Box) == SOULS_OK);
        CHECK(souls_msb_set_int(Msb, SOULS_MSB_PARTS, static_cast<size_t>(Part), "NoSuchProperty", 1) ==
              SOULS_ERR_UNSUPPORTED);
        CHECK(souls_msb_set_int(Msb, SOULS_MSB_PARTS, static_cast<size_t>(Part), "Position", 1) ==
              SOULS_ERR_INVALID_ARGUMENT);
        CHECK(souls_msb_set_int(Msb, SOULS_MSB_PARTS, 99, "EntityID", 1) == SOULS_ERR_INVALID_ARGUMENT);
        if (Kind == SOULS_MSB_ELDEN_RING) {
            // Elden Ring parts keep their draw groups in a separate block; they are still offered by name.
            const int64_t Groups[8] = {1, 2, 3, 4, 5, 6, 7, 8};
            CHECK(souls_msb_set_int_array(Msb, SOULS_MSB_PARTS, static_cast<size_t>(Part), "DrawGroups", Groups, 8) ==
                  SOULS_OK);
            int64_t Out[8] = {};
            size_t Count   = 0;
            CHECK(
              souls_msb_get_int_array(Msb, SOULS_MSB_PARTS, static_cast<size_t>(Part), "DrawGroups", Out, 8, &Count) ==
                SOULS_OK &&
              Count == 8 && Out[7] == 8);
        }

        SoulsBuffer Bytes = souls_msb_write_memory(Msb);
        CHECK(Bytes.data != nullptr);
        SoulsMsb* Back = souls_msb_read_memory(Kind, Bytes.data, Bytes.size);
        CHECK(Back != nullptr);
        if (Back) {
            CHECK(souls_msb_entry_count(Back, SOULS_MSB_MODELS) == 1 &&
                  souls_msb_entry_count(Back, SOULS_MSB_PARTS) == 1);
            CHECK(souls_msb_find_entry(Back, SOULS_MSB_PARTS, "m1000_0000") == 0);
            const char* ModelName = nullptr;
            CHECK(souls_msb_get_string(Back, SOULS_MSB_PARTS, 0, "ModelName", &ModelName) == SOULS_OK && ModelName &&
                  std::strcmp(ModelName, "m1000") == 0);
            float Out[3] = {};
            CHECK(souls_msb_get_vec3(Back, SOULS_MSB_REGIONS, 0, "Position", Out) == SOULS_OK && Out[0] == 1.f &&
                  Out[2] == 3.f);
            float Dims[3] = {};
            CHECK(souls_msb_region_shape(Back, 0) == SOULS_SHAPE_BOX);
            CHECK(souls_msb_region_shape_dims(Back, 0, Dims) == SOULS_OK && Dims[0] == 4.f && Dims[2] == 6.f);

            // References are by name, so renaming a model and the part that uses it keeps the file consistent.
            CHECK(souls_msb_set_string(Back, SOULS_MSB_PARTS, 0, "ModelName", "m2000") == SOULS_OK);
            CHECK(souls_msb_set_entry_name(Back, SOULS_MSB_MODELS, 0, "m2000") == SOULS_OK);
            SoulsBuffer Again = souls_msb_write_memory(Back);
            CHECK(Again.data != nullptr);
            souls_buffer_free(Again);

            CHECK(souls_msb_remove_entry(Back, SOULS_MSB_REGIONS, 0) == SOULS_OK);
            CHECK(souls_msb_entry_count(Back, SOULS_MSB_REGIONS) == 0);
            souls_msb_free(Back);
        }
        souls_buffer_free(Bytes);
        souls_msb_free(Msb);
    }
    CHECK(souls_msb_read_file(SOULS_MSB_DARK_SOULS_1, "this/map/does/not/exist.msb") == nullptr);
    CHECK(souls_msb_read_memory(static_cast<SoulsMsbKind>(99), nullptr, 0) == nullptr);
    return Failures;
}

// A real Dark Souls Remastered map: unchanged it writes back identically, and its enemies can be moved.
int TestCApiMsbReal() {
    Failures = 0;
    const fs::path Path =
      "C:/Program Files (x86)/Steam/steamapps/common/DARK SOULS REMASTERED/map/MapStudio/m10_00_00_00.msb";
    if (!fs::exists(Path)) {
        std::printf("Real MSB C API checks skipped (no Dark Souls Remastered install)\n");
        return TestSkipped;
    }
    const std::string PathText = Path.string();
    SoulsBuffer Original       = souls_read_file(PathText.c_str());
    SoulsMsb* Msb              = souls_msb_read_file(SOULS_MSB_DARK_SOULS_1, PathText.c_str());
    CHECK(Msb != nullptr && Original.data != nullptr);
    if (!Msb) return Failures;

    SoulsBuffer Same = souls_msb_write_memory(Msb);
    CHECK(Same.size == Original.size && std::memcmp(Same.data, Original.data, Same.size) == 0);
    souls_buffer_free(Same);

    const size_t Parts = souls_msb_entry_count(Msb, SOULS_MSB_PARTS);
    CHECK(Parts > 0);
    int64_t Enemy = -1;
    for (size_t I = 0; I < Parts && Enemy < 0; ++I) {
        if (std::strcmp(souls_msb_entry_type(Msb, SOULS_MSB_PARTS, I), "Enemy") == 0) Enemy = static_cast<int64_t>(I);
    }
    CHECK(Enemy >= 0);
    if (Enemy >= 0) {
        const size_t E = static_cast<size_t>(Enemy);
        CHECK(souls_msb_property_count(Msb, SOULS_MSB_PARTS, E) >= 5);
        float Position[3] = {};
        CHECK(souls_msb_get_vec3(Msb, SOULS_MSB_PARTS, E, "Position", Position) == SOULS_OK);
        Position[1] += 5.f;
        CHECK(souls_msb_set_vec3(Msb, SOULS_MSB_PARTS, E, "Position", Position) == SOULS_OK);
        int64_t Groups[4] = {};
        size_t Count      = 0;
        CHECK(souls_msb_get_int_array(Msb, SOULS_MSB_PARTS, E, "DrawGroups", Groups, 4, &Count) == SOULS_OK &&
              Count == 4);
        const char* ModelName = nullptr;
        CHECK(souls_msb_get_string(Msb, SOULS_MSB_PARTS, E, "ModelName", &ModelName) == SOULS_OK &&
              ModelName != nullptr);
        CHECK(souls_msb_find_entry(Msb, SOULS_MSB_MODELS, ModelName) >= 0);

        SoulsBuffer Edited = souls_msb_write_memory(Msb);
        SoulsMsb* Back     = souls_msb_read_memory(SOULS_MSB_DARK_SOULS_1, Edited.data, Edited.size);
        CHECK(Back != nullptr);
        if (Back) {
            float Moved[3] = {};
            const int64_t Again =
              souls_msb_find_entry(Back, SOULS_MSB_PARTS, souls_msb_entry_name(Msb, SOULS_MSB_PARTS, E));
            CHECK(Again >= 0 &&
                  souls_msb_get_vec3(Back, SOULS_MSB_PARTS, static_cast<size_t>(Again), "Position", Moved) == SOULS_OK);
            CHECK(Moved[1] == Position[1]);
            souls_msb_free(Back);
        }
        souls_buffer_free(Edited);
    }
    souls_msb_free(Msb);
    souls_buffer_free(Original);
    return Failures;
}
