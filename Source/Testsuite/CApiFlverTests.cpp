// Copyright (C) 2026 Jake Rieger
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, version 3. This program is distributed in the hope that it will
// be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU General
// Public License along with this program. If not, see https://www.gnu.org/licenses/.

#include "TestResult.hpp"

#include <libSouls/CApi/souls.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

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

// A real Dark Souls Remastered character model, inspected and edited through the C API.
int TestCApiFlverReal() {
    Failures            = 0;
    const fs::path Path = "C:/Program Files (x86)/Steam/steamapps/common/DARK SOULS REMASTERED/chr/c2240.chrbnd.dcx";
    if (!fs::exists(Path)) {
        std::printf("Real FLVER C API checks skipped (no Dark Souls Remastered install)\n");
        return TestSkipped;
    }

    const std::string PathText = Path.string();
    SoulsBinder* Bnd           = souls_binder_read_file(PathText.c_str());
    CHECK(Bnd != nullptr);
    if (!Bnd) return Failures;
    // Several characters are in the binder as several .flver files; take the first with geometry.
    SoulsFlver* Model = nullptr;
    for (size_t I = 0; I < souls_binder_file_count(Bnd) && !Model; ++I) {
        const char* Name = souls_binder_file_name(Bnd, I);
        if (!Name || std::strlen(Name) < 6 || std::strcmp(Name + std::strlen(Name) - 6, ".flver") != 0) continue;
        size_t Size           = 0;
        const uint8_t* Data   = souls_binder_file_bytes(Bnd, I, &Size);
        SoulsFlver* Candidate = souls_flver_read_memory(Data, Size);
        if (Candidate && souls_flver_mesh_count(Candidate) > 0) Model = Candidate;
        else souls_flver_free(Candidate);
    }
    CHECK(Model != nullptr);
    if (!Model) {
        souls_binder_free(Bnd);
        return Failures;
    }

    CHECK(souls_flver_version(Model) == 0x2000C || souls_flver_version(Model) == 0x2000D);
    CHECK(souls_flver_bone_count(Model) > 0);
    CHECK(souls_flver_bone_name(Model, 0) != nullptr);
    CHECK(souls_flver_bone_parent(Model, 0) == -1);
    float Translation[3], Rotation[3], Scale[3];
    CHECK(souls_flver_bone_transform(Model, 0, Translation, Rotation, Scale) == SOULS_OK);
    CHECK(souls_flver_bone_name(Model, 100000) == nullptr && souls_last_error_code() == SOULS_ERR_INVALID_ARGUMENT);

    const size_t Materials = souls_flver_material_count(Model);
    CHECK(Materials > 0);
    CHECK(souls_flver_material_mtd(Model, 0) != nullptr);
    CHECK(souls_flver_texture_count(Model, 0) > 0 && souls_flver_texture_type(Model, 0, 0) != nullptr);

    const size_t Meshes = souls_flver_mesh_count(Model);
    CHECK(Meshes > 0);
    size_t Vertices = souls_flver_vertex_count(Model, 0);
    CHECK(Vertices > 0);
    CHECK(souls_flver_attribute_components(SOULS_VERTEX_POSITION) == 3);
    CHECK(souls_flver_attribute_sets(Model, 0, SOULS_VERTEX_UV) >= 1);
    CHECK(souls_flver_mesh_material(Model, 0) >= 0 &&
          static_cast<size_t>(souls_flver_mesh_material(Model, 0)) < Materials);

    std::vector<float> Positions(Vertices * 3);
    CHECK(souls_flver_read_vertex_floats(Model, 0, SOULS_VERTEX_POSITION, 0, Positions.data(), Positions.size()) ==
          SOULS_OK);
    std::vector<float> TooSmall(3);
    CHECK(souls_flver_read_vertex_floats(Model, 0, SOULS_VERTEX_POSITION, 0, TooSmall.data(), TooSmall.size()) ==
          SOULS_ERR_INVALID_ARGUMENT);
    std::vector<float> Normals(Vertices * 3), Uvs(Vertices * 3), Weights(Vertices * 4);
    CHECK(souls_flver_read_vertex_floats(Model, 0, SOULS_VERTEX_NORMAL, 0, Normals.data(), Normals.size()) == SOULS_OK);
    CHECK(souls_flver_read_vertex_floats(Model, 0, SOULS_VERTEX_UV, 0, Uvs.data(), Uvs.size()) == SOULS_OK);
    CHECK(souls_flver_read_vertex_floats(Model, 0, SOULS_VERTEX_BONE_WEIGHTS, 0, Weights.data(), Weights.size()) ==
          SOULS_OK);
    CHECK(souls_flver_read_vertex_floats(Model, 0, SOULS_VERTEX_BONE_INDICES, 0, Weights.data(), Weights.size()) ==
          SOULS_ERR_INVALID_ARGUMENT);
    std::vector<int32_t> BoneIndices(Vertices * 4);
    CHECK(souls_flver_read_vertex_ints(Model, 0, BoneIndices.data(), BoneIndices.size()) == SOULS_OK);

    // Normals are unit length for (nearly) every vertex.
    size_t Unit = 0;
    for (size_t I = 0; I < Vertices; ++I) {
        const float L = std::sqrt(Normals[I * 3] * Normals[I * 3] + Normals[I * 3 + 1] * Normals[I * 3 + 1] +
                                  Normals[I * 3 + 2] * Normals[I * 3 + 2]);
        if (std::fabs(L - 1.f) < 0.05f) ++Unit;
    }
    CHECK(Unit * 10 > Vertices * 9);

    // Faces reference real vertices.
    CHECK(souls_flver_face_set_count(Model, 0) > 0);
    const size_t Indices = souls_flver_face_set_index_count(Model, 0, 0);
    CHECK(Indices > 0 && Indices % 3 == 0);
    std::vector<int32_t> Faces(Indices);
    CHECK(souls_flver_read_face_set(Model, 0, 0, Faces.data(), Faces.size()) == SOULS_OK);
    bool InRange = true;
    for (int32_t Index : Faces)
        InRange = InRange && Index >= 0 && static_cast<size_t>(Index) < Vertices;
    CHECK(InRange);

    // Move every vertex of the first mesh and read it back after a write.
    for (size_t I = 0; I < Vertices; ++I)
        Positions[I * 3 + 1] += 1.f;
    CHECK(souls_flver_write_vertex_floats(Model, 0, SOULS_VERTEX_POSITION, 0, Positions.data(), Positions.size()) ==
          SOULS_OK);
    CHECK(souls_flver_write_vertex_floats(Model, 0, SOULS_VERTEX_POSITION, 0, Positions.data(), 3) ==
          SOULS_ERR_INVALID_ARGUMENT);
    CHECK(souls_flver_set_bone_name(Model, 0, "RenamedRoot") == SOULS_OK);

    SoulsBuffer Edited = souls_flver_write_memory(Model);
    CHECK(Edited.data != nullptr);
    SoulsFlver* Back = souls_flver_read_memory(Edited.data, Edited.size);
    CHECK(Back != nullptr);
    if (Back) {
        CHECK(souls_flver_vertex_count(Back, 0) == Vertices);
        std::vector<float> Again(Vertices * 3);
        CHECK(souls_flver_read_vertex_floats(Back, 0, SOULS_VERTEX_POSITION, 0, Again.data(), Again.size()) ==
              SOULS_OK);
        CHECK(std::fabs(Again[1] - Positions[1]) < 0.001f);
        CHECK(std::strcmp(souls_flver_bone_name(Back, 0), "RenamedRoot") == 0);

        const int64_t Copy = souls_flver_duplicate_mesh(Back, 0);
        CHECK(Copy == static_cast<int64_t>(Meshes));
        CHECK(souls_flver_remove_mesh(Back, static_cast<size_t>(Copy)) == SOULS_OK);
        souls_flver_free(Back);
    }
    souls_buffer_free(Edited);
    souls_flver_free(Model);
    souls_binder_free(Bnd);

    SoulsFlver* Empty = souls_flver_new();
    CHECK(Empty != nullptr && souls_flver_mesh_count(Empty) == 0 && souls_flver_bone_count(Empty) == 0);
    souls_flver_free(Empty);
    return Failures;
}
