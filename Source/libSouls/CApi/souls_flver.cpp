//
// Created by Jake Rieger on 10/11/2026.
//

#include "CApiCommon.hpp"

#include <libSouls/Formats/FLVER/FLVER2.hpp>

using namespace Souls;
using namespace Souls::CApi;

struct SoulsFlver {
    FLVER2 Value;
};

namespace {
    SoulsFlver* Wrap(FLVER2 Value) {
        auto Result   = std::make_unique<SoulsFlver>();
        Result->Value = std::move(Value);
        return Result.release();
    }

    FLVER2& Model(const SoulsFlver* Handle) {
        return const_cast<SoulsFlver&>(Require(Handle, "flver is NULL")).Value;
    }

    FLVER::Bone& BoneAt(const SoulsFlver* Handle, size_t Index) {
        auto& Bones = Model(Handle).Bones;
        CheckIndex(Index, Bones.size(), "bone index out of range");
        return Bones[Index];
    }

    FLVER::Dummy& DummyAt(const SoulsFlver* Handle, size_t Index) {
        auto& Dummies = Model(Handle).Dummies;
        CheckIndex(Index, Dummies.size(), "dummy index out of range");
        return Dummies[Index];
    }

    FLVER2::Material& MaterialAt(const SoulsFlver* Handle, size_t Index) {
        auto& Materials = Model(Handle).Materials;
        CheckIndex(Index, Materials.size(), "material index out of range");
        return Materials[Index];
    }

    FLVER2::Texture& TextureAt(const SoulsFlver* Handle, size_t Material, size_t Index) {
        auto& Textures = MaterialAt(Handle, Material).Textures;
        CheckIndex(Index, Textures.size(), "texture index out of range");
        return Textures[Index];
    }

    FLVER2::Mesh& MeshAt(const SoulsFlver* Handle, size_t Index) {
        auto& Meshes = Model(Handle).Meshes;
        CheckIndex(Index, Meshes.size(), "mesh index out of range");
        return Meshes[Index];
    }

    FLVER2::FaceSet& FaceSetAt(const SoulsFlver* Handle, size_t Mesh, size_t Index) {
        auto& Sets = MeshAt(Handle, Mesh).FaceSets;
        CheckIndex(Index, Sets.size(), "face set index out of range");
        return Sets[Index];
    }

    void Out3(float* Out, const Vector3& V) {
        if (!Out) return;
        Out[0] = V.X;
        Out[1] = V.Y;
        Out[2] = V.Z;
    }

    Vector3 In3(const float* In) {
        return Vector3(In[0], In[1], In[2]);
    }

    const char* Text(const char* Value, const char* What) {
        if (!Value) ThrowInvalid(What);
        return Value;
    }

    size_t Components(SoulsVertexAttribute Attribute) {
        switch (Attribute) {
            case SOULS_VERTEX_POSITION:
            case SOULS_VERTEX_NORMAL:
            case SOULS_VERTEX_UV: return 3;
            case SOULS_VERTEX_BONE_WEIGHTS:
            case SOULS_VERTEX_BONE_INDICES:
            case SOULS_VERTEX_TANGENT:
            case SOULS_VERTEX_BITANGENT:
            case SOULS_VERTEX_COLOR: return 4;
        }
        ThrowInvalid("invalid vertex attribute");
    }

    size_t SetCount(const FLVER2::Mesh& Mesh, SoulsVertexAttribute Attribute) {
        if (Mesh.Vertices.empty()) return 0;
        switch (Attribute) {
            case SOULS_VERTEX_UV: return Mesh.Vertices.front().UVs.size();
            case SOULS_VERTEX_TANGENT: return Mesh.Vertices.front().Tangents.size();
            case SOULS_VERTEX_COLOR: return Mesh.Vertices.front().Colors.size();
            default: return 1;
        }
    }

    void RequireSet(const FLVER2::Mesh& Mesh, SoulsVertexAttribute Attribute, size_t Set) {
        if (Set >= SetCount(Mesh, Attribute)) ThrowInvalid("the mesh has no such attribute set");
    }
}  // namespace

SOULS_C_API SoulsFlver* souls_flver_new(void) {
    return GuardValue<SoulsFlver*>(nullptr, [&] { return Wrap(FLVER2()); });
}

SOULS_C_API SoulsFlver* souls_flver_read_file(const char* path) {
    return GuardValue<SoulsFlver*>(nullptr, [&] { return Wrap(FLVER2::Read(PathFrom(path))); });
}

SOULS_C_API SoulsFlver* souls_flver_read_memory(const uint8_t* data, size_t size) {
    return GuardValue<SoulsFlver*>(nullptr, [&] { return Wrap(FLVER2::Read(Bytes(data, size))); });
}

SOULS_C_API void souls_flver_free(SoulsFlver* flver) {
    delete flver;
}

SOULS_C_API SoulsStatus souls_flver_write_file(SoulsFlver* flver, const char* path) {
    return Guard([&] { Model(flver).Write(PathFrom(path)); });
}

SOULS_C_API SoulsBuffer souls_flver_write_memory(SoulsFlver* flver) {
    return GuardValue(SoulsBuffer{nullptr, 0}, [&] { return MakeBuffer(Model(flver).Write()); });
}

SOULS_C_API int32_t souls_flver_version(const SoulsFlver* flver) {
    return GuardValue<int32_t>(0, [&] { return Model(flver).Header.Version; });
}

SOULS_C_API SoulsStatus souls_flver_bounding_box(const SoulsFlver* flver, float out_min[3], float out_max[3]) {
    return Guard([&] {
        const auto& Header = Model(flver).Header;
        Out3(out_min, Header.BoundingBoxMin);
        Out3(out_max, Header.BoundingBoxMax);
    });
}

SOULS_C_API SoulsStatus souls_flver_set_bounding_box(SoulsFlver* flver, const float min[3], const float max[3]) {
    return Guard([&] {
        if (!min || !max) ThrowInvalid("bounding box is NULL");
        auto& Header          = Model(flver).Header;
        Header.BoundingBoxMin = In3(min);
        Header.BoundingBoxMax = In3(max);
    });
}

#pragma region Bones and dummies
SOULS_C_API size_t souls_flver_bone_count(const SoulsFlver* flver) {
    return GuardValue<size_t>(0, [&] { return Model(flver).Bones.size(); });
}

SOULS_C_API const char* souls_flver_bone_name(const SoulsFlver* flver, size_t bone) {
    return GuardValue<const char*>(nullptr, [&] { return BoneAt(flver, bone).Name.c_str(); });
}

SOULS_C_API SoulsStatus souls_flver_set_bone_name(SoulsFlver* flver, size_t bone, const char* name) {
    return Guard([&] { BoneAt(flver, bone).Name = Text(name, "name is NULL"); });
}

SOULS_C_API int32_t souls_flver_bone_parent(const SoulsFlver* flver, size_t bone) {
    return GuardValue<int32_t>(-1, [&] { return static_cast<int32_t>(BoneAt(flver, bone).ParentIndex); });
}

SOULS_C_API SoulsStatus souls_flver_set_bone_parent(SoulsFlver* flver, size_t bone, int32_t parent) {
    return Guard([&] {
        if (parent < -1 || parent >= static_cast<int32_t>(Model(flver).Bones.size())) ThrowInvalid("parent index out of range");
        BoneAt(flver, bone).ParentIndex = static_cast<int16_t>(parent);
    });
}

SOULS_C_API SoulsStatus souls_flver_bone_transform(const SoulsFlver* flver, size_t bone, float translation[3], float rotation[3], float scale[3]) {
    return Guard([&] {
        const FLVER::Bone& B = BoneAt(flver, bone);
        Out3(translation, B.Translation);
        Out3(rotation, B.Rotation);
        Out3(scale, B.Scale);
    });
}

SOULS_C_API SoulsStatus souls_flver_set_bone_transform(SoulsFlver* flver, size_t bone, const float translation[3], const float rotation[3],
                                                       const float scale[3]) {
    return Guard([&] {
        FLVER::Bone& B = BoneAt(flver, bone);
        if (translation) B.Translation = In3(translation);
        if (rotation) B.Rotation = In3(rotation);
        if (scale) B.Scale = In3(scale);
    });
}

SOULS_C_API size_t souls_flver_dummy_count(const SoulsFlver* flver) {
    return GuardValue<size_t>(0, [&] { return Model(flver).Dummies.size(); });
}

SOULS_C_API SoulsStatus souls_flver_dummy(const SoulsFlver* flver, size_t dummy, float position[3], float forward[3], float upward[3],
                                          int32_t* reference_id, int32_t* parent_bone, int32_t* attach_bone) {
    return Guard([&] {
        const FLVER::Dummy& D = DummyAt(flver, dummy);
        Out3(position, D.Position);
        Out3(forward, D.Forward);
        Out3(upward, D.Upward);
        if (reference_id) *reference_id = D.ReferenceID;
        if (parent_bone) *parent_bone = D.ParentBoneIndex;
        if (attach_bone) *attach_bone = D.AttachBoneIndex;
    });
}

SOULS_C_API SoulsStatus souls_flver_set_dummy(SoulsFlver* flver, size_t dummy, const float position[3], const float forward[3],
                                              const float upward[3], int32_t reference_id, int32_t parent_bone, int32_t attach_bone) {
    return Guard([&] {
        FLVER::Dummy& D = DummyAt(flver, dummy);
        if (position) D.Position = In3(position);
        if (forward) D.Forward = In3(forward);
        if (upward) D.Upward = In3(upward);
        D.ReferenceID     = static_cast<int16_t>(reference_id);
        D.ParentBoneIndex = static_cast<int16_t>(parent_bone);
        D.AttachBoneIndex = static_cast<int16_t>(attach_bone);
    });
}
#pragma endregion

#pragma region Materials
SOULS_C_API size_t souls_flver_material_count(const SoulsFlver* flver) {
    return GuardValue<size_t>(0, [&] { return Model(flver).Materials.size(); });
}

SOULS_C_API const char* souls_flver_material_name(const SoulsFlver* flver, size_t material) {
    return GuardValue<const char*>(nullptr, [&] { return MaterialAt(flver, material).Name.c_str(); });
}

SOULS_C_API SoulsStatus souls_flver_set_material_name(SoulsFlver* flver, size_t material, const char* name) {
    return Guard([&] { MaterialAt(flver, material).Name = Text(name, "name is NULL"); });
}

SOULS_C_API const char* souls_flver_material_mtd(const SoulsFlver* flver, size_t material) {
    return GuardValue<const char*>(nullptr, [&] { return MaterialAt(flver, material).MTD.c_str(); });
}

SOULS_C_API SoulsStatus souls_flver_set_material_mtd(SoulsFlver* flver, size_t material, const char* mtd) {
    return Guard([&] { MaterialAt(flver, material).MTD = Text(mtd, "mtd is NULL"); });
}

SOULS_C_API size_t souls_flver_texture_count(const SoulsFlver* flver, size_t material) {
    return GuardValue<size_t>(0, [&] { return MaterialAt(flver, material).Textures.size(); });
}

SOULS_C_API const char* souls_flver_texture_type(const SoulsFlver* flver, size_t material, size_t texture) {
    return GuardValue<const char*>(nullptr, [&] { return TextureAt(flver, material, texture).Type.c_str(); });
}

SOULS_C_API const char* souls_flver_texture_path(const SoulsFlver* flver, size_t material, size_t texture) {
    return GuardValue<const char*>(nullptr, [&] { return TextureAt(flver, material, texture).Path.c_str(); });
}

SOULS_C_API SoulsStatus souls_flver_set_texture(SoulsFlver* flver, size_t material, size_t texture, const char* type, const char* path) {
    return Guard([&] {
        FLVER2::Texture& T = TextureAt(flver, material, texture);
        T.Type             = Text(type, "type is NULL");
        T.Path             = Text(path, "path is NULL");
    });
}

SOULS_C_API int64_t souls_flver_add_texture(SoulsFlver* flver, size_t material, const char* type, const char* path) {
    return GuardValue<int64_t>(-1, [&]() -> int64_t {
        auto& Textures = MaterialAt(flver, material).Textures;
        FLVER2::Texture T;
        T.Type = Text(type, "type is NULL");
        T.Path = Text(path, "path is NULL");
        Textures.push_back(std::move(T));
        return static_cast<int64_t>(Textures.size() - 1);
    });
}

SOULS_C_API SoulsStatus souls_flver_remove_texture(SoulsFlver* flver, size_t material, size_t texture) {
    return Guard([&] {
        auto& Textures = MaterialAt(flver, material).Textures;
        CheckIndex(texture, Textures.size(), "texture index out of range");
        Textures.erase(Textures.begin() + static_cast<std::ptrdiff_t>(texture));
    });
}
#pragma endregion

#pragma region Meshes
SOULS_C_API size_t souls_flver_mesh_count(const SoulsFlver* flver) {
    return GuardValue<size_t>(0, [&] { return Model(flver).Meshes.size(); });
}

SOULS_C_API int32_t souls_flver_mesh_material(const SoulsFlver* flver, size_t mesh) {
    return GuardValue<int32_t>(-1, [&] { return MeshAt(flver, mesh).MaterialIndex; });
}

SOULS_C_API SoulsStatus souls_flver_set_mesh_material(SoulsFlver* flver, size_t mesh, int32_t material) {
    return Guard([&] {
        if (material < 0 || static_cast<size_t>(material) >= Model(flver).Materials.size()) ThrowInvalid("material index out of range");
        MeshAt(flver, mesh).MaterialIndex = material;
    });
}

SOULS_C_API int souls_flver_mesh_is_dynamic(const SoulsFlver* flver, size_t mesh) {
    return GuardValue(0, [&] { return MeshAt(flver, mesh).Dynamic != 0 ? 1 : 0; });
}

SOULS_C_API SoulsStatus souls_flver_mesh_bone_indices(const SoulsFlver* flver, size_t mesh, int32_t* out, size_t capacity, size_t* out_count) {
    return Guard([&] {
        const auto& Indices = MeshAt(flver, mesh).BoneIndices;
        if (out_count) *out_count = Indices.size();
        if (out) {
            for (size_t I = 0; I < Indices.size() && I < capacity; ++I) out[I] = Indices[I];
        }
    });
}

SOULS_C_API SoulsStatus souls_flver_set_mesh_bone_indices(SoulsFlver* flver, size_t mesh, const int32_t* indices, size_t count) {
    return Guard([&] {
        if (!indices && count) ThrowInvalid("indices is NULL");
        MeshAt(flver, mesh).BoneIndices.assign(indices, indices + count);
    });
}

SOULS_C_API int64_t souls_flver_duplicate_mesh(SoulsFlver* flver, size_t mesh) {
    return GuardValue<int64_t>(-1, [&]() -> int64_t {
        auto& Meshes = Model(flver).Meshes;
        CheckIndex(mesh, Meshes.size(), "mesh index out of range");
        FLVER2::Mesh Copy = Meshes[mesh];
        Meshes.push_back(std::move(Copy));
        return static_cast<int64_t>(Meshes.size() - 1);
    });
}

SOULS_C_API SoulsStatus souls_flver_remove_mesh(SoulsFlver* flver, size_t mesh) {
    return Guard([&] {
        auto& Meshes = Model(flver).Meshes;
        CheckIndex(mesh, Meshes.size(), "mesh index out of range");
        Meshes.erase(Meshes.begin() + static_cast<std::ptrdiff_t>(mesh));
    });
}
#pragma endregion

#pragma region Vertices
SOULS_C_API size_t souls_flver_vertex_count(const SoulsFlver* flver, size_t mesh) {
    return GuardValue<size_t>(0, [&] { return MeshAt(flver, mesh).Vertices.size(); });
}

SOULS_C_API size_t souls_flver_attribute_sets(const SoulsFlver* flver, size_t mesh, SoulsVertexAttribute attribute) {
    return GuardValue<size_t>(0, [&] {
        Components(attribute);
        return SetCount(MeshAt(flver, mesh), attribute);
    });
}

SOULS_C_API size_t souls_flver_attribute_components(SoulsVertexAttribute attribute) {
    return GuardValue<size_t>(0, [&] { return Components(attribute); });
}

SOULS_C_API SoulsStatus souls_flver_read_vertex_floats(const SoulsFlver* flver, size_t mesh, SoulsVertexAttribute attribute, size_t set,
                                                       float* out, size_t capacity) {
    return Guard([&] {
        const FLVER2::Mesh& M = MeshAt(flver, mesh);
        const size_t N        = Components(attribute);
        if (attribute == SOULS_VERTEX_BONE_INDICES) ThrowInvalid("bone indices are integers; use souls_flver_read_vertex_ints");
        if (!out && !M.Vertices.empty()) ThrowInvalid("out is NULL");
        if (capacity < M.Vertices.size() * N) ThrowInvalid("out is too small");
        if (M.Vertices.empty()) return;
        RequireSet(M, attribute, set);
        size_t At = 0;
        for (const FLVER::Vertex& V : M.Vertices) {
            switch (attribute) {
                case SOULS_VERTEX_POSITION: out[At++] = V.Position.X; out[At++] = V.Position.Y; out[At++] = V.Position.Z; break;
                case SOULS_VERTEX_NORMAL: out[At++] = V.Normal.X; out[At++] = V.Normal.Y; out[At++] = V.Normal.Z; break;
                case SOULS_VERTEX_BONE_WEIGHTS:
                    for (float W : V.BoneWeights) out[At++] = W;
                    break;
                case SOULS_VERTEX_UV: {
                    const Vector3& UV = V.UVs.at(set);
                    out[At++] = UV.X; out[At++] = UV.Y; out[At++] = UV.Z;
                    break;
                }
                case SOULS_VERTEX_TANGENT: {
                    const Vector4& T = V.Tangents.at(set);
                    out[At++] = T.X; out[At++] = T.Y; out[At++] = T.Z; out[At++] = T.W;
                    break;
                }
                case SOULS_VERTEX_BITANGENT: out[At++] = V.Bitangent.X; out[At++] = V.Bitangent.Y; out[At++] = V.Bitangent.Z; out[At++] = V.Bitangent.W; break;
                case SOULS_VERTEX_COLOR: {
                    const FLVER::VertexColor& C = V.Colors.at(set);
                    out[At++] = C.R; out[At++] = C.G; out[At++] = C.B; out[At++] = C.A;
                    break;
                }
                default: break;
            }
        }
    });
}

SOULS_C_API SoulsStatus souls_flver_write_vertex_floats(SoulsFlver* flver, size_t mesh, SoulsVertexAttribute attribute, size_t set,
                                                        const float* values, size_t count) {
    return Guard([&] {
        FLVER2::Mesh& M = MeshAt(flver, mesh);
        const size_t N  = Components(attribute);
        if (attribute == SOULS_VERTEX_BONE_INDICES) ThrowInvalid("bone indices are integers; use souls_flver_write_vertex_ints");
        if (count != M.Vertices.size() * N) ThrowInvalid("count must be vertex_count * components");
        if (!values && count) ThrowInvalid("values is NULL");
        if (M.Vertices.empty()) return;
        RequireSet(M, attribute, set);
        size_t At = 0;
        for (FLVER::Vertex& V : M.Vertices) {
            switch (attribute) {
                case SOULS_VERTEX_POSITION: V.Position = Vector3(values[At], values[At + 1], values[At + 2]); At += 3; break;
                case SOULS_VERTEX_NORMAL: V.Normal = Vector3(values[At], values[At + 1], values[At + 2]); At += 3; break;
                case SOULS_VERTEX_BONE_WEIGHTS:
                    for (float& W : V.BoneWeights) W = values[At++];
                    break;
                case SOULS_VERTEX_UV: V.UVs.at(set) = Vector3(values[At], values[At + 1], values[At + 2]); At += 3; break;
                case SOULS_VERTEX_TANGENT: V.Tangents.at(set) = Vector4(values[At], values[At + 1], values[At + 2], values[At + 3]); At += 4; break;
                case SOULS_VERTEX_BITANGENT: V.Bitangent = Vector4(values[At], values[At + 1], values[At + 2], values[At + 3]); At += 4; break;
                case SOULS_VERTEX_COLOR: V.Colors.at(set) = FLVER::VertexColor(values[At + 3], values[At], values[At + 1], values[At + 2]); At += 4; break;
                default: break;
            }
        }
    });
}

SOULS_C_API SoulsStatus souls_flver_read_vertex_ints(const SoulsFlver* flver, size_t mesh, int32_t* out, size_t capacity) {
    return Guard([&] {
        const FLVER2::Mesh& M = MeshAt(flver, mesh);
        if (!out && !M.Vertices.empty()) ThrowInvalid("out is NULL");
        if (capacity < M.Vertices.size() * 4) ThrowInvalid("out is too small");
        size_t At = 0;
        for (const FLVER::Vertex& V : M.Vertices) {
            for (int32_t Index : V.BoneIndices) out[At++] = Index;
        }
    });
}

SOULS_C_API SoulsStatus souls_flver_write_vertex_ints(SoulsFlver* flver, size_t mesh, const int32_t* values, size_t count) {
    return Guard([&] {
        FLVER2::Mesh& M = MeshAt(flver, mesh);
        if (count != M.Vertices.size() * 4) ThrowInvalid("count must be vertex_count * 4");
        if (!values && count) ThrowInvalid("values is NULL");
        size_t At = 0;
        for (FLVER::Vertex& V : M.Vertices) {
            for (int32_t& Index : V.BoneIndices) Index = values[At++];
        }
    });
}
#pragma endregion

#pragma region Faces
SOULS_C_API size_t souls_flver_face_set_count(const SoulsFlver* flver, size_t mesh) {
    return GuardValue<size_t>(0, [&] { return MeshAt(flver, mesh).FaceSets.size(); });
}

SOULS_C_API uint32_t souls_flver_face_set_flags(const SoulsFlver* flver, size_t mesh, size_t face_set) {
    return GuardValue<uint32_t>(0, [&] { return static_cast<uint32_t>(FaceSetAt(flver, mesh, face_set).Flags); });
}

SOULS_C_API size_t souls_flver_face_set_index_count(const SoulsFlver* flver, size_t mesh, size_t face_set) {
    return GuardValue<size_t>(0, [&] {
        return FaceSetAt(flver, mesh, face_set).Triangulate(MeshAt(flver, mesh).Vertices.size() < 65535).size();
    });
}

SOULS_C_API SoulsStatus souls_flver_read_face_set(const SoulsFlver* flver, size_t mesh, size_t face_set, int32_t* out, size_t capacity) {
    return Guard([&] {
        const auto Triangles = FaceSetAt(flver, mesh, face_set).Triangulate(MeshAt(flver, mesh).Vertices.size() < 65535);
        if (!out && !Triangles.empty()) ThrowInvalid("out is NULL");
        if (capacity < Triangles.size()) ThrowInvalid("out is too small");
        for (size_t I = 0; I < Triangles.size(); ++I) out[I] = Triangles[I];
    });
}

SOULS_C_API SoulsStatus souls_flver_write_face_set(SoulsFlver* flver, size_t mesh, size_t face_set, const int32_t* indices, size_t count) {
    return Guard([&] {
        if (!indices && count) ThrowInvalid("indices is NULL");
        if (count % 3 != 0) ThrowInvalid("count must be a multiple of 3");
        const int32_t VertexCount = static_cast<int32_t>(MeshAt(flver, mesh).Vertices.size());
        for (size_t I = 0; I < count; ++I) {
            if (indices[I] < 0 || indices[I] >= VertexCount) ThrowInvalid("an index is out of range for the mesh's vertices");
        }
        FLVER2::FaceSet& Set = FaceSetAt(flver, mesh, face_set);
        Set.Indices.assign(indices, indices + count);
        Set.TriangleStrip = false;
    });
}
#pragma endregion
