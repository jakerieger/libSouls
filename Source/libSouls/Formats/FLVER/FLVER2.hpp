//
// Created by Jake Rieger on 10/8/2026.
//

#pragma once

#include "FlverCommon.hpp"

#include <libSouls/SoulsFile.hpp>

#include <climits>
#include <cfloat>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of exported classes; only touched by this DLL's own code

    // FLVER2: the model format of Dark Souls (and its remaster), Dark Souls II and III, Bloodborne, Sekiro and Elden
    // Ring (format versions 0x20005 to 0x2001A, plus whatever the later games added that can be read the same way).
    // Extension: .flver. Demon's Souls' older FLVER0 isn't covered.
    //
    // A model is a skeleton (Bones), dummy points (Dummies), materials, and meshes. Each mesh draws with one
    // material and has its vertices and one or more face sets (triangle lists, usually one per level of detail).
    class SOULS_API FLVER2 : public SoulsFile<FLVER2> {
    public:
        struct SOULS_API FLVERHeader {
            bool BigEndian = false;
            // 0x20005 to 0x2001A; for example 0x2000D in Dark Souls, 0x20010 in Dark Souls II, 0x20014 in Dark Souls III
            // and Bloodborne, 0x2001A in Sekiro.
            int32_t Version = 0x20014;
            Vector3 BoundingBoxMin;
            Vector3 BoundingBoxMax;
            // Whether strings are UTF-16 (true) or Shift-JIS (false).
            bool Unicode = true;
            bool Unk4A   = false;
            int32_t Unk4C = 0;
            uint8_t Unk5C = 0;
            uint8_t Unk5D = 0;
            int32_t Unk68 = 0;
        };

        // A texture a material uses.
        struct SOULS_API Texture {
            // What the texture is for, such as "g_Diffuse".
            std::string Type;
            // The texture's path (often just a file name).
            std::string Path;
            Vector2 Scale{1.f, 1.f};
            uint8_t Unk10  = 0;
            bool Unk11     = false;
            float Unk14    = 0;
            float Unk18    = 0;
            float Unk1C    = 0;

            std::string ToString() const { return Type + " = " + Path; }
        };

        // One item in a GXList: a block of material settings the game's shaders read.
        struct SOULS_API GXItem {
            // A number as text for Dark Souls II and earlier, four characters ("GXMD", ...) after.
            std::string ID = "0";
            int32_t Unk04  = 100;
            std::vector<uint8_t> Data;
        };

        struct SOULS_API GXList {
            std::vector<GXItem> Items;
            int32_t TerminatorID     = INT_MAX;
            int32_t TerminatorLength = 0;
        };

        struct SOULS_API Material {
            std::string Name;
            // The material definition (.mtd) that says which shader to use.
            std::string MTD;
            int32_t Flags = 0;
            std::vector<Texture> Textures;
            // Index into GXLists, or -1.
            int32_t GXIndex = -1;
            int32_t Unk18   = 0;

            std::string ToString() const { return Name + " | " + MTD; }
        };

        struct SOULS_API FaceSet {
            enum class FSFlags : uint32_t {
                None           = 0,
                LodLevel1      = 0x0100'0000,
                LodLevel2      = 0x0200'0000,
                EdgeCompressed = 0x4000'0000,
                MotionBlur     = 0x8000'0000,
            };

            FSFlags Flags = FSFlags::None;
            // Whether the indices are a triangle strip rather than a list of triangles.
            bool TriangleStrip = false;
            bool CullBackfaces = true;
            int16_t Unk06      = 0;
            std::vector<int32_t> Indices;

            bool HasFlag(FSFlags Flag) const { return (static_cast<uint32_t>(Flags) & static_cast<uint32_t>(Flag)) != 0; }

            // The indices as plain triangles (three per face). Strips are expanded; AllowPrimitiveRestarts says
            // whether 0xFFFF in a strip means "start over" (true for meshes with fewer than 65535 vertices).
            std::vector<int32_t> Triangulate(bool AllowPrimitiveRestarts, bool IncludeDegenerateFaces = false) const;

            // 16 or 32: the smallest index size that can hold every index.
            int32_t GetVertexIndexSize() const;
            void AddFaceCounts(bool AllowPrimitiveRestarts, int32_t& TrueFaceCount, int32_t& TotalFaceCount) const;
        };

        // Which buffer layout a vertex buffer uses. (A mesh's vertices are stored in 1 to 3 buffers, each holding a
        // different set of data for every vertex; here they've been merged into the mesh's Vertices.)
        struct SOULS_API VertexBuffer {
            int32_t LayoutIndex = 0;
        };

        struct SOULS_API BoundingBoxes {
            Vector3 Min{-FLT_MAX, -FLT_MAX, -FLT_MAX};
            Vector3 Max{FLT_MAX, FLT_MAX, FLT_MAX};
            Vector3 Unk;  // Sekiro and later
        };

        struct SOULS_API Mesh {
            uint8_t Dynamic       = 0;  // 0 for static meshes, 1 for ones bound to bones
            int32_t MaterialIndex = 0;
            int32_t DefaultBoneIndex = -1;
            // Which bones the vertices' bone indices refer to.
            std::vector<int32_t> BoneIndices;
            std::vector<FaceSet> FaceSets;
            std::vector<VertexBuffer> VertexBuffers;
            std::vector<FLVER::Vertex> Vertices;
            std::optional<BoundingBoxes> BoundingBox;

            // The triangles of the face set with the given flags (or the first one) as vertices. Pointers are into
            // Vertices.
            std::vector<std::array<const FLVER::Vertex*, 3>> GetFaces(FaceSet::FSFlags Flags = FaceSet::FSFlags::None) const;
        };

        struct SOULS_API SekiroUnkStruct {
            struct Member {
                std::array<int16_t, 4> Unk00{};
                int32_t Index = 0;
            };
            std::vector<Member> Members1;
            std::vector<Member> Members2;
        };

        FLVERHeader Header;
        std::vector<FLVER::Dummy> Dummies;
        std::vector<Material> Materials;
        std::vector<GXList> GXLists;
        std::vector<FLVER::Bone> Bones;
        std::vector<Mesh> Meshes;
        std::vector<FLVER::LayoutMembers> BufferLayouts;
        // Present from version 0x2001A.
        std::optional<SekiroUnkStruct> SekiroUnk;

        // An empty model configured for Dark Souls III.
        FLVER2() = default;

    protected:
        bool IsImpl(BinaryReader& Reader) override;
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

    constexpr FLVER2::FaceSet::FSFlags operator|(FLVER2::FaceSet::FSFlags A, FLVER2::FaceSet::FSFlags B) {
        return static_cast<FLVER2::FaceSet::FSFlags>(static_cast<uint32_t>(A) | static_cast<uint32_t>(B));
    }

#pragma warning(pop)

}  // namespace Souls
