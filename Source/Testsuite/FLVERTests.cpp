//
// Created by Jake Rieger on 10/8/2026.
//

#include "TestResult.hpp"

#include <libSouls/Binders/BND3.hpp>
#include <libSouls/Binders/BND4.hpp>
#include <libSouls/Formats/FLVER/FLVER2.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <map>
#include <string>

namespace {
    int Failures = 0;

#define CHECK(Cond)                                                     \
    do {                                                                \
        if (!(Cond)) {                                                  \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #Cond); \
            ++Failures;                                                 \
        }                                                               \
    } while (0)

    using namespace Souls;
    namespace fs = std::filesystem;
    using LT = FLVER::LayoutType;
    using LS = FLVER::LayoutSemantic;

    template<typename F>
    bool Throws(F&& Fn) {
        try {
            Fn();
        } catch (...) {
            return true;
        }
        return false;
    }

    bool Near(float A, float B, float Tolerance) {
        return std::fabs(A - B) <= Tolerance;
    }

    bool NearV3(const Vector3& A, const Vector3& B, float Tolerance) {
        return Near(A.X, B.X, Tolerance) && Near(A.Y, B.Y, Tolerance) && Near(A.Z, B.Z, Tolerance);
    }

    // The vertex layouts of the common games.
    FLVER::LayoutMembers LayoutDS3() {
        return {{LT::Float3, LS::Position},         {LT::Byte4B, LS::Normal},   {LT::Byte4B, LS::Tangent},
                {LT::Byte4B, LS::BoneIndices},      {LT::Short4toFloat4A, LS::BoneWeights}, {LT::UV, LS::UV},
                {LT::Byte4C, LS::VertexColor}};
    }

    FLVER::LayoutMembers LayoutDS1() {
        return {{LT::Float3, LS::Position},  {LT::Byte4B, LS::Normal},  {LT::Byte4B, LS::Tangent},
                {LT::Byte4B, LS::Bitangent}, {LT::Byte4C, LS::VertexColor}, {LT::Short2toFloat2, LS::UV},
                {LT::Byte4B, LS::BoneIndices}, {LT::Byte4A, LS::BoneWeights}};
    }

    // A two-buffer mesh: positions and normals in one buffer, UVs in the second.
    FLVER::LayoutMembers LayoutSplitA() {
        return {{LT::Float3, LS::Position}, {LT::Byte4B, LS::Normal}, {LT::Byte4B, LS::Tangent}, {LT::Byte4B, LS::BoneIndices}, {LT::Byte4A, LS::BoneWeights}};
    }
    FLVER::LayoutMembers LayoutSplitB() {
        return {{LT::UVPair, LS::UV}, {LT::Float4, LS::VertexColor}};
    }

    FLVER::Vertex MakeVertex(int Seed, size_t UVs, size_t Tangents, size_t Colors) {
        FLVER::Vertex V;
        V.Position = {Seed * 0.5f, Seed * -0.25f + 1, static_cast<float>(Seed % 7)};
        // Values chosen to survive being squeezed into bytes and shorts exactly.
        const float Quantum = 1.f / 127.f;
        V.Normal      = {(Seed % 20 - 10) * Quantum, (Seed % 13 - 6) * Quantum, 1.f};
        V.NormalW     = 0;
        V.BoneIndices = {Seed % 5, (Seed + 1) % 5, 0, 0};
        V.BoneWeights = {64.f / 127.f, 63.f / 127.f, 0.f, 0.f};
        for (size_t I = 0; I < UVs; ++I) V.UVs.push_back({(Seed + static_cast<int>(I)) / 8.f, (Seed * 3 % 16) / 16.f, 0.f});
        for (size_t I = 0; I < Tangents; ++I) V.Tangents.push_back({1.f, 0.f, -Quantum * (Seed % 5), 1.f});
        V.Bitangent = {0.f, 1.f, 0.f, 1.f};
        for (size_t I = 0; I < Colors; ++I) V.Colors.push_back(FLVER::VertexColor(uint8_t{255}, uint8_t(Seed * 5 % 256), uint8_t(0), uint8_t(128)));
        return V;
    }

    FLVER2 MakeModel(int32_t Version) {
        FLVER2 Model;
        Model.Header.Version        = Version;
        Model.Header.Unicode        = Version >= 0x20013 || Version == 0x20010;
        Model.Header.BoundingBoxMin = {-1, -2, -3};
        Model.Header.BoundingBoxMax = {1, 2, 3};
        Model.Header.Unk4C          = 0;

        // Bones: a small chain.
        for (int I = 0; I < 5; ++I) {
            FLVER::Bone Bone;
            Bone.Name                 = I == 0 ? "Master" : "Bone_" + std::to_string(I);
            Bone.ParentIndex          = static_cast<int16_t>(I - 1);
            Bone.ChildIndex           = static_cast<int16_t>(I < 4 ? I + 1 : -1);
            Bone.Translation          = {0.f, 0.5f * I, 0.f};
            Bone.Rotation             = {0.1f * I, 0.f, 0.2f};
            Bone.BoundingBoxMax       = {1.f, 1.f, 1.f};
            Model.Bones.push_back(Bone);
        }

        for (int I = 0; I < 3; ++I) {
            FLVER::Dummy Dummy;
            Dummy.Position        = {static_cast<float>(I), 0.f, 1.f};
            Dummy.Forward         = {0.f, 0.f, 1.f};
            Dummy.Upward          = {0.f, 1.f, 0.f};
            Dummy.ReferenceID     = static_cast<int16_t>(100 + I);
            Dummy.ParentBoneIndex = static_cast<int16_t>(I);
            Dummy.Tint            = {255, 10, 20, static_cast<uint8_t>(30 + I)};
            Dummy.UseUpwardVector = I == 1;
            Model.Dummies.push_back(Dummy);
        }

        // Materials: one with several textures and a GX list, one plain.
        FLVER2::Material First;
        First.Name  = "Body";
        First.MTD   = "N:\\FDP\\data\\Material\\mtd\\M[DSB].mtd";
        First.Flags = 0x20000000;
        First.Textures.push_back({"g_Diffuse", "c1234_a.tga", {1.f, 1.f}, 1, true, 0.f, 0.f, 0.f});
        First.Textures.push_back({"g_Bumpmap", "c1234_n.tga", {2.f, 0.5f}, 0, false, 1.f, 2.f, 3.f});
        FLVER2::Material Second;
        Second.Name = "Hair";
        Second.MTD  = "hair.mtd";
        Second.Textures.push_back({"g_Diffuse", "hair.tga", {1.f, 1.f}, 0, false, 0.f, 0.f, 0.f});
        Model.Materials = {First, Second};

        FLVER2::GXList Gx;
        Gx.Items.push_back({Version <= 0x20010 ? "1" : "GXMD", 100, {1, 2, 3, 4, 5, 6, 7, 8}});
        if (Version >= 0x20010) {
            Gx.Items.push_back({Version <= 0x20010 ? "2" : "GXFF", 100, {9, 9, 9, 9}});
            Gx.TerminatorID = Version <= 0x20010 ? INT_MAX : -1;
            Gx.TerminatorLength = 4;
        }
        Model.GXLists.push_back(Gx);
        Model.Materials[0].GXIndex = 0;

        // Meshes.
        const bool Split = Version >= 0x20013;
        Model.BufferLayouts = Split ? std::vector<FLVER::LayoutMembers>{LayoutDS3(), LayoutSplitA(), LayoutSplitB()}
                                    : std::vector<FLVER::LayoutMembers>{LayoutDS1()};

        for (int M = 0; M < 3; ++M) {
            FLVER2::Mesh Mesh;
            Mesh.Dynamic          = M == 0 ? 1 : 0;
            Mesh.MaterialIndex    = M % 2;
            Mesh.DefaultBoneIndex = M == 0 ? -1 : 2;
            if (M == 0) Mesh.BoneIndices = {0, 1, 2, 3, 4};
            const size_t Count = 8 + static_cast<size_t>(M) * 4;

            if (Split && M == 2) {
                Mesh.VertexBuffers = {{1}, {2}};  // positions etc. in one buffer, UVs and colors in the other
                for (size_t I = 0; I < Count; ++I) Mesh.Vertices.push_back(MakeVertex(static_cast<int>(I) + M, 2, 1, 1));
            } else {
                Mesh.VertexBuffers = {{0}};
                const size_t UVs = Split ? 1 : 1;
                for (size_t I = 0; I < Count; ++I) Mesh.Vertices.push_back(MakeVertex(static_cast<int>(I) + M, UVs, 1, 1));
            }
            if (M == 1) Mesh.BoundingBox = FLVER2::BoundingBoxes{{-1, -1, -1}, {1, 1, 1}, {0, 0, 0}};

            FLVER2::FaceSet Triangles;
            for (int I = 0; I + 2 < static_cast<int>(Count); ++I) {
                Triangles.Indices.insert(Triangles.Indices.end(), {I, I + 1, I + 2});
            }
            FLVER2::FaceSet Strip;
            Strip.Flags         = FLVER2::FaceSet::FSFlags::LodLevel1;
            Strip.TriangleStrip = true;
            for (int I = 0; I < static_cast<int>(Count); ++I) Strip.Indices.push_back(I);
            Mesh.FaceSets = {Triangles, Strip};
            Model.Meshes.push_back(std::move(Mesh));
        }

        if (Version >= 0x2001A) {
            FLVER2::SekiroUnkStruct Unk;
            Unk.Members1.push_back({{1, 2, 3, 4}, 5});
            Unk.Members2.push_back({{5, 6, 7, 8}, 9});
            Model.SekiroUnk = Unk;
        }
        return Model;
    }

    bool SameModelStructure(const FLVER2& A, const FLVER2& B) {
        if (A.Dummies.size() != B.Dummies.size() || A.Materials.size() != B.Materials.size() || A.Bones.size() != B.Bones.size() ||
            A.Meshes.size() != B.Meshes.size() || A.BufferLayouts.size() != B.BufferLayouts.size() || A.GXLists.size() != B.GXLists.size()) {
            return false;
        }
        for (size_t I = 0; I < A.Bones.size(); ++I) {
            const auto& X = A.Bones[I];
            const auto& Y = B.Bones[I];
            if (X.Name != Y.Name || X.ParentIndex != Y.ParentIndex || X.ChildIndex != Y.ChildIndex || X.Translation != Y.Translation ||
                X.Rotation != Y.Rotation || X.Scale != Y.Scale) {
                return false;
            }
        }
        for (size_t I = 0; I < A.Dummies.size(); ++I) {
            const auto& X = A.Dummies[I];
            const auto& Y = B.Dummies[I];
            if (X.Position != Y.Position || X.ReferenceID != Y.ReferenceID || X.Tint != Y.Tint || X.UseUpwardVector != Y.UseUpwardVector) {
                return false;
            }
        }
        for (size_t I = 0; I < A.Materials.size(); ++I) {
            const auto& X = A.Materials[I];
            const auto& Y = B.Materials[I];
            if (X.Name != Y.Name || X.MTD != Y.MTD || X.Flags != Y.Flags || X.GXIndex != Y.GXIndex || X.Textures.size() != Y.Textures.size()) {
                return false;
            }
            for (size_t T = 0; T < X.Textures.size(); ++T) {
                if (X.Textures[T].Type != Y.Textures[T].Type || X.Textures[T].Path != Y.Textures[T].Path ||
                    X.Textures[T].Scale != Y.Textures[T].Scale || X.Textures[T].Unk18 != Y.Textures[T].Unk18) {
                    return false;
                }
            }
        }
        for (size_t I = 0; I < A.Meshes.size(); ++I) {
            const auto& X = A.Meshes[I];
            const auto& Y = B.Meshes[I];
            if (X.Dynamic != Y.Dynamic || X.MaterialIndex != Y.MaterialIndex || X.DefaultBoneIndex != Y.DefaultBoneIndex ||
                X.BoneIndices != Y.BoneIndices || X.Vertices.size() != Y.Vertices.size() || X.FaceSets.size() != Y.FaceSets.size() ||
                X.VertexBuffers.size() != Y.VertexBuffers.size() || X.BoundingBox.has_value() != Y.BoundingBox.has_value()) {
                return false;
            }
            for (size_t F = 0; F < X.FaceSets.size(); ++F) {
                if (X.FaceSets[F].Indices != Y.FaceSets[F].Indices || X.FaceSets[F].Flags != Y.FaceSets[F].Flags ||
                    X.FaceSets[F].TriangleStrip != Y.FaceSets[F].TriangleStrip) {
                    return false;
                }
            }
            for (size_t V = 0; V < X.Vertices.size(); ++V) {
                const auto& P = X.Vertices[V];
                const auto& Q = Y.Vertices[V];
                if (P.Position != Q.Position || P.BoneIndices != Q.BoneIndices || P.UVs.size() != Q.UVs.size() ||
                    P.Tangents.size() != Q.Tangents.size() || P.Colors.size() != Q.Colors.size() ||
                    !NearV3(P.Normal, Q.Normal, 1.f / 126.f)) {
                    return false;
                }
                for (size_t U = 0; U < P.UVs.size(); ++U) {
                    if (!NearV3(P.UVs[U], Q.UVs[U], 1.f / 512.f)) return false;
                }
            }
        }
        return true;
    }

    void TestSynthetic() {
        for (const int32_t Version : {0x2000D, 0x20010, 0x20014, 0x2001A}) {
            try {
                FLVER2 Source = MakeModel(Version);
                const auto Bytes = Source.Write();
                CHECK(FLVER2::Is(Bytes));
                FLVER2 Back = FLVER2::Read(Bytes);
                CHECK(Back.Header.Version == Version && Back.Header.Unicode == Source.Header.Unicode);
                CHECK(Back.Header.BoundingBoxMin == Source.Header.BoundingBoxMin);
                const bool Same = SameModelStructure(Source, Back);
                if (!Same) std::printf("  (version 0x%X: models differ)\n", Version);
                CHECK(Same);
                CHECK(Back.Write() == Bytes);  // what was read writes back byte for byte
                CHECK(Back.SekiroUnk.has_value() == (Version >= 0x2001A));
                CHECK(Back.GXLists.size() == 1 && Back.GXLists[0].Items.size() == Source.GXLists[0].Items.size() &&
                      Back.GXLists[0].Items[0].Data == Source.GXLists[0].Items[0].Data);
            } catch (const std::exception& E) {
                std::printf("FAIL version 0x%X: %s\n", Version, E.what());
                ++Failures;
            }
        }

        // Big-endian files.
        {
            FLVER2 Source = MakeModel(0x2000D);
            Source.Header.BigEndian = true;
            const auto Bytes = Source.Write();
            CHECK(FLVER2::Is(Bytes) && Bytes[6] == 'B');
            FLVER2 Back = FLVER2::Read(Bytes);
            CHECK(Back.Header.BigEndian && SameModelStructure(Source, Back) && Back.Write() == Bytes);
        }

        // Faces, triangulation and counts.
        {
            FLVER2::FaceSet Strip;
            Strip.TriangleStrip = true;
            Strip.Indices       = {0, 1, 2, 3, 0xFFFF, 4, 5, 6, 6, 7};
            CHECK((Strip.Triangulate(true) == std::vector<int32_t>{0, 1, 2, 3, 2, 1, 4, 5, 6}));  // flip, restart, degenerates dropped
            CHECK(Strip.Triangulate(true, true).size() > Strip.Triangulate(true).size());
            int32_t True = 0, Total = 0;
            Strip.AddFaceCounts(true, True, Total);
            CHECK(True == 3 && Total == 5);
            FLVER2::FaceSet List;
            List.Indices = {0, 1, 2, 3, 4, 5};
            CHECK(List.Triangulate(true) == List.Indices && List.GetVertexIndexSize() == 16);
            List.Indices.push_back(70000);
            CHECK(List.GetVertexIndexSize() == 32);

            const FLVER2 Model = MakeModel(0x20014);
            const auto Faces   = Model.Meshes[0].GetFaces();  // the plain triangle list, not the LOD strip
            CHECK(Faces.size() == 6 && Faces[0][0] == &Model.Meshes[0].Vertices[0] && Faces[0][2] == &Model.Meshes[0].Vertices[2]);
            CHECK(!Model.Meshes[0].GetFaces(FLVER2::FaceSet::FSFlags::LodLevel1).empty());
        }

        // A 32-bit index model (65536+ vertices of indexing).
        {
            FLVER2 Model = MakeModel(0x20014);
            Model.Meshes[0].FaceSets[0].Indices.push_back(70000 % 8);  // still valid ones
            Model.Meshes[0].FaceSets[0].Indices.push_back(1);
            Model.Meshes[0].FaceSets[0].Indices.push_back(2);
            const FLVER2 Back = FLVER2::Read(Model.Write());
            CHECK(Back.Meshes[0].FaceSets[0].Indices == Model.Meshes[0].FaceSets[0].Indices);
        }

        // Transforms and other small things.
        {
            FLVER::Bone Bone;
            Bone.Translation = {1, 2, 3};
            Bone.Scale       = {2, 2, 2};
            const Vector3 Moved = Bone.ComputeLocalTransform().Transform({1, 0, 0});
            CHECK(NearV3(Moved, {3, 2, 3}, 1e-5f));  // scaled to 2, then translated
            FLVER::Bone Rotated;
            Rotated.Rotation = {0, 3.14159265f / 2, 0};  // a quarter turn about Y
            CHECK(NearV3(Rotated.ComputeLocalTransform().Transform({1, 0, 0}), {0, 0, -1}, 1e-5f));
            CHECK(FLVER::LayoutMember(LT::Float3, LS::Position).Size() == 12 && FLVER::LayoutMember(LT::EdgeCompressed, LS::Position).Size() == 1);
            CHECK(FLVER::LayoutSize(LayoutDS3()) == 12 + 4 + 4 + 4 + 8 + 4 + 4);
            CHECK(FLVER::LayoutMember(LT::Float3, LS::Position).ToString() == "Float3: Position");
            CHECK(FLVER::VertexColor(uint8_t{255}, uint8_t{0}, uint8_t{51}, uint8_t{255}).G == 51 / 255.f);
        }

        // Detection and bad input.
        {
            FLVER2 Model = MakeModel(0x20014);
            auto Bytes = Model.Write();
            CHECK(!FLVER2::Is(std::vector<uint8_t>{'F', 'L', 'V', 'E', 'R', 0, 'L', 0, 1, 0, 0, 0}));  // version too low
            CHECK(!FLVER2::Is(std::vector<uint8_t>(64, 0x42)) && !FLVER2::IsRead(std::vector<uint8_t>(64, 0x42)).has_value());
            CHECK(Throws([&] { FLVER2::Read(std::vector<uint8_t>(64, 0x42)); }));
            std::vector<uint8_t> Truncated(Bytes.begin(), Bytes.begin() + 0x100);
            CHECK(Throws([&] { FLVER2::Read(Truncated); }));
            std::vector<uint8_t> BadVersion = Bytes;
            BadVersion[8] = 0x99;
            CHECK(Throws([&] { FLVER2::Read(BadVersion); }));
            std::vector<uint8_t> BadCount = Bytes;
            BadCount[0x1C] = 0xFF;
            BadCount[0x1D] = 0xFF;
            BadCount[0x1E] = 0xFF;
            BadCount[0x1F] = 0x7F;
            CHECK(Throws([&] { FLVER2::Read(BadCount); }));

            // Writing mistakes.
            FLVER2 BadLayout = MakeModel(0x20014);
            BadLayout.Meshes[0].VertexBuffers[0].LayoutIndex = 99;
            CHECK(Throws([&] { BadLayout.Write(); }));
            FLVER2 MissingUV = MakeModel(0x20014);
            MissingUV.Meshes[1].Vertices[0].UVs.clear();
            CHECK(Throws([&] { MissingUV.Write(); }));
            FLVER2 BadGX = MakeModel(0x20014);
            BadGX.Materials[0].GXIndex = 5;
            CHECK(Throws([&] { BadGX.Write(); }));
        }

        // Through DCX.
        {
            FLVER2 Model      = MakeModel(0x20014);
            Model.Compression = DCX::Type::DCX_DFLT_10000_44_9;
            const auto Bytes  = Model.Write();
            CHECK(DCX::Is(Bytes) && FLVER2::Is(Bytes));
            const FLVER2 Back = FLVER2::Read(Bytes);
            CHECK(Back.Compression == DCX::Type::DCX_DFLT_10000_44_9 && SameModelStructure(Model, Back));
        }
    }

    bool EndsWith(const std::string& Text, const std::string& Suffix) {
        return Text.size() >= Suffix.size() && Text.compare(Text.size() - Suffix.size(), Suffix.size(), Suffix) == 0;
    }

    std::vector<fs::path> CollectBinders(const fs::path& Root, std::initializer_list<const char*> Suffixes, size_t Max) {
        std::vector<fs::path> Found;
        if (!fs::exists(Root)) return Found;
        for (const auto& Entry : fs::recursive_directory_iterator(Root, fs::directory_options::skip_permission_denied)) {
            if (!Entry.is_regular_file()) continue;
            const std::string Name = Entry.path().filename().string();
            for (const char* Suffix : Suffixes) {
                if (EndsWith(Name, Suffix)) {
                    Found.push_back(Entry.path());
                    break;
                }
            }
        }
        std::sort(Found.begin(), Found.end());
        const size_t Step = std::max<size_t>(1, Found.size() / std::max<size_t>(1, Max));
        std::vector<fs::path> Sampled;
        for (size_t I = 0; I < Found.size(); I += Step) Sampled.push_back(Found[I]);
        return Sampled;
    }

    struct Stats {
        int Binders = 0, Models = 0, Meshes = 0, Vertices = 0, Faces = 0, Bones = 0, Identical = 0, Failed = 0;
        std::map<int32_t, int> Versions;
        std::map<std::string, int> Layouts;
        std::vector<std::string> Different;
    };

    // Whether a model's indices and references all point at things that exist.
    bool Consistent(const FLVER2& Model, std::string& Why) {
        for (size_t M = 0; M < Model.Meshes.size(); ++M) {
            const auto& Mesh = Model.Meshes[M];
            if (Mesh.MaterialIndex < 0 || static_cast<size_t>(Mesh.MaterialIndex) >= Model.Materials.size()) {
                Why = "mesh " + std::to_string(M) + " has a bad material index";
                return false;
            }
            for (const int32_t Bone : Mesh.BoneIndices) {
                if (Bone < 0 || static_cast<size_t>(Bone) >= Model.Bones.size()) {
                    Why = "mesh " + std::to_string(M) + " has a bad bone index";
                    return false;
                }
            }
            for (const auto& Set : Mesh.FaceSets) {
                for (const int32_t Index : Set.Indices) {
                    // 0xFFFF restarts a strip; other indices have to name a vertex.
                    if (!(Set.TriangleStrip && Index == 0xFFFF) && (Index < 0 || static_cast<size_t>(Index) >= Mesh.Vertices.size())) {
                        Why = "mesh " + std::to_string(M) + " has a face index outside its vertices";
                        return false;
                    }
                }
            }
            for (const auto& Vertex : Mesh.Vertices) {
                if (!std::isfinite(Vertex.Position.X) || !std::isfinite(Vertex.Position.Y) || !std::isfinite(Vertex.Position.Z)) {
                    Why = "mesh " + std::to_string(M) + " has a non-finite position";
                    return false;
                }
            }
        }
        for (const auto& Material : Model.Materials) {
            if (Material.GXIndex != -1 && (Material.GXIndex < 0 || static_cast<size_t>(Material.GXIndex) >= Model.GXLists.size())) {
                Why = "bad GX index";
                return false;
            }
        }
        return true;
    }

    void ReadModel(const fs::path& Where, const std::string& Name, const std::vector<uint8_t>& Bytes, Stats& Totals) {
        try {
            if (!FLVER2::Is(Bytes)) {
                std::printf("FAIL %s: %s is not recognized as a FLVER2\n", Where.string().c_str(), Name.c_str());
                ++Totals.Failed;
                return;
            }
            FLVER2 Model = FLVER2::Read(Bytes);
            ++Totals.Models;
            ++Totals.Versions[Model.Header.Version];
            Totals.Meshes += static_cast<int>(Model.Meshes.size());
            Totals.Bones += static_cast<int>(Model.Bones.size());
            for (const auto& Mesh : Model.Meshes) {
                Totals.Vertices += static_cast<int>(Mesh.Vertices.size());
                Totals.Faces += static_cast<int>(Mesh.GetFaces().size());
            }

            std::string Why;
            if (!Consistent(Model, Why)) {
                std::printf("FAIL %s: %s: %s\n", Where.filename().string().c_str(), Name.c_str(), Why.c_str());
                ++Totals.Failed;
            }

            Model.Compression    = DCX::Type::None;
            const auto Rewritten = Model.Write();
            if (Rewritten == Bytes) {
                ++Totals.Identical;
            } else {
                const FLVER2 Back = FLVER2::Read(Rewritten);
                if (!SameModelStructure(Model, Back)) {
                    std::printf("FAIL %s: %s changed after a rewrite\n", Where.filename().string().c_str(), Name.c_str());
                    ++Totals.Failed;
                }
                if (Totals.Different.size() < 6) {
                    size_t At = 0;
                    while (At < Rewritten.size() && At < Bytes.size() && Rewritten[At] == Bytes[At]) ++At;
                    Totals.Different.push_back(Where.filename().string() + " " + Name + " (v0x" + [&] {
                        char Hex[16];
                        std::snprintf(Hex, sizeof Hex, "%X", Model.Header.Version);
                        return std::string(Hex);
                    }() + ", first difference at 0x" + [&] {
                        char Hex[32];
                        std::snprintf(Hex, sizeof Hex, "%zX", At);
                        return std::string(Hex);
                    }() + ", sizes " + std::to_string(Bytes.size()) + " vs " + std::to_string(Rewritten.size()) + ")");
                }
            }
        } catch (const std::exception& E) {
            std::printf("FAIL %s: %s: %s\n", Where.filename().string().c_str(), Name.c_str(), E.what());
            ++Totals.Failed;
        }
    }

    template<typename Binder>
    void ScanBinders(const fs::path& Root, std::initializer_list<const char*> Suffixes, size_t Max, Stats& Totals) {
        for (const fs::path& Path : CollectBinders(Root, Suffixes, Max)) {
            try {
                if (!Binder::Is(Path)) continue;
                const Binder Bnd = Binder::Read(Path);
                ++Totals.Binders;
                for (const BinderFile& File : Bnd.Files) {
                    if (File.Name && EndsWith(*File.Name, ".flver")) ReadModel(Path, *File.Name, File.Bytes, Totals);
                }
            } catch (const std::exception& E) {
                std::printf("FAIL %s: %s\n", Path.string().c_str(), E.what());
                ++Totals.Failed;
            }
        }
    }

    void Report(const char* Label, const Stats& Totals) {
        std::printf("%s: %d binders, %d models (%d meshes, %d vertices, %d faces, %d bones), %d byte-identical rewrites, %d failures\n",
                    Label, Totals.Binders, Totals.Models, Totals.Meshes, Totals.Vertices, Totals.Faces, Totals.Bones,
                    Totals.Identical, Totals.Failed);
        std::printf("  versions:");
        for (const auto& [Version, Count] : Totals.Versions) std::printf(" 0x%X x%d", Version, Count);
        std::printf("\n");
        for (const std::string& Line : Totals.Different) std::printf("  differs: %s\n", Line.c_str());
        Failures += Totals.Failed;
    }
}  // namespace

int RunFLVERTests(size_t MaxFiles) {
    Failures = 0;
    TestSynthetic();

    const fs::path Steam = "C:/Program Files (x86)/Steam/steamapps/common";
    if (fs::exists(Steam / "DARK SOULS REMASTERED")) {
        Stats Totals;
        const fs::path Root = Steam / "DARK SOULS REMASTERED";
        ScanBinders<BND3>(Root / "chr", {".chrbnd.dcx", ".chrbnd"}, MaxFiles, Totals);
        ScanBinders<BND3>(Root / "obj", {".objbnd.dcx", ".objbnd"}, MaxFiles, Totals);
        ScanBinders<BND3>(Root / "parts", {".partsbnd.dcx", ".partsbnd"}, MaxFiles, Totals);
        Report("Dark Souls Remastered", Totals);
        CHECK(Totals.Models > 0);
    }
    if (fs::exists(Steam / "ELDEN RING/Game")) {
        Stats Totals;
        const fs::path Root = Steam / "ELDEN RING/Game";
        ScanBinders<BND4>(Root / "chr", {".chrbnd.dcx"}, MaxFiles, Totals);
        ScanBinders<BND4>(Root / "parts", {".partsbnd.dcx"}, MaxFiles, Totals);
        ScanBinders<BND4>(Root / "asset", {".geombnd.dcx"}, MaxFiles, Totals);
        Report("Elden Ring", Totals);
        CHECK(Totals.Models > 0);
    }

    std::printf(Failures == 0 ? "FLVER tests passed\n" : "FLVER tests: %d failure(s)\n", Failures);
    return Failures;
}
