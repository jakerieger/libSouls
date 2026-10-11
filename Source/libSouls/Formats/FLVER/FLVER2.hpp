// Copyright (C) 2026 Jake Rieger
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, version 3. This program is distributed in the hope that it will
// be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU General
// Public License along with this program. If not, see https://www.gnu.org/licenses/.

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

    /// @brief FLVER2: the model format of Dark Souls (and its remaster), Dark Souls II and III, Bloodborne, Sekiro and
    ///        Elden Ring (format versions 0x20005 to 0x2001A, plus whatever the later games added that can be read the
    ///        same way).
    ///
    /// Extension: .flver. Demon's Souls' older FLVER0 isn't covered.
    ///
    /// A model is a skeleton (Bones), dummy points (Dummies), materials, and meshes. Each mesh draws with one
    /// material and has its vertices and one or more face sets (triangle lists, usually one per level of detail).
    class SOULS_API FLVER2 : public SoulsFile<FLVER2> {
    public:
        /// @brief General metadata about a FLVER.
        struct SOULS_API FLVERHeader {
            /// @brief If true FLVER will be written big-endian, if false little-endian.
            bool BigEndian = false;
            /// @brief 0x20005 to 0x2001A; for example 0x2000D in Dark Souls, 0x20010 in Dark Souls II, 0x20014 in Dark
            ///        Souls III and Bloodborne, 0x2001A in Sekiro.
            int32_t Version = 0x20014;
            /// @brief Minimum extent of the entire model.
            Vector3 BoundingBoxMin;
            /// @brief Maximum extent of the entire model.
            Vector3 BoundingBoxMax;
            /// @brief Whether strings are UTF-16 (true) or Shift-JIS (false).
            bool Unicode = true;
            /// @brief Unknown.
            bool Unk4A = false;
            /// @brief Unknown; I believe this is the primitive restart constant, but I'm not certain.
            int32_t Unk4C = 0;
            /// @brief Unknown.
            uint8_t Unk5C = 0;
            /// @brief Unknown.
            uint8_t Unk5D = 0;
            /// @brief Unknown.
            int32_t Unk68 = 0;
        };

        /// @brief A texture a material uses.
        struct SOULS_API Texture {
            /// @brief What the texture is for, such as "g_Diffuse".
            std::string Type;
            /// @brief The texture's path (often just a file name).
            std::string Path;
            /// @brief Unknown.
            Vector2 Scale {1.f, 1.f};
            /// @brief Unknown; observed values 0, 1, 2.
            uint8_t Unk10 = 0;
            /// @brief Unknown.
            bool Unk11 = false;
            /// @brief Unknown.
            float Unk14 = 0;
            /// @brief Unknown.
            float Unk18 = 0;
            /// @brief Unknown.
            float Unk1C = 0;

            /// @brief "Type = Path".
            std::string ToString() const { return Type + " = " + Path; }
        };

        /// @brief One item in a GXList: a block of material settings the game's shaders read.
        struct SOULS_API GXItem {
            /// @brief A number as text for Dark Souls II and earlier, four characters ("GXMD", ...) after.
            std::string ID = "0";
            /// @brief Unknown; typically 100.
            int32_t Unk04 = 100;
            /// @brief Raw parameter data, usually just a bunch of floats.
            std::vector<uint8_t> Data;
        };

        /// @brief A collection of items that set various material properties.
        struct SOULS_API GXList {
            /// @brief The items, in order.
            std::vector<GXItem> Items;
            /// @brief Value indicating the terminating item; typically int.MaxValue, sometimes -1.
            int32_t TerminatorID = INT_MAX;
            /// @brief The length in bytes of the terminator data block; most likely not important, but varies in
            /// original files.
            int32_t TerminatorLength = 0;
        };

        /// @brief A reference to an MTD file, specifying textures to use.
        struct SOULS_API Material {
            /// @brief Identifies the mesh that uses this material, may include keywords that determine hideable parts.
            std::string Name;
            /// @brief The material definition (.mtd) that says which shader to use.
            std::string MTD;
            /// @brief Unknown.
            int32_t Flags = 0;
            /// @brief Textures used by this material.
            std::vector<Texture> Textures;
            /// @brief Index into GXLists, or -1.
            int32_t GXIndex = -1;
            /// @brief Unknown; only used in Sekiro.
            int32_t Unk18 = 0;

            /// @brief "Name | MTD".
            std::string ToString() const { return Name + " | " + MTD; }
        };

        /// @brief Determines how vertices in a mesh are connected to form triangles.
        struct SOULS_API FaceSet {
            /// @brief Flags of a face set: which level of detail it is, and more.
            enum class FSFlags : uint32_t {
                None           = 0,
                LodLevel1      = 0x0100'0000,
                LodLevel2      = 0x0200'0000,
                EdgeCompressed = 0x4000'0000,
                MotionBlur     = 0x8000'0000,
            };

            /// @brief FaceSet Flags on this FaceSet.
            FSFlags Flags = FSFlags::None;
            /// @brief Whether the indices are a triangle strip rather than a list of triangles.
            bool TriangleStrip = false;
            /// @brief Whether triangles can be seen through from behind.
            bool CullBackfaces = true;
            /// @brief Unknown.
            int16_t Unk06 = 0;
            /// @brief Indices to vertices in a mesh.
            std::vector<int32_t> Indices;

            /// @brief Whether a flag is set.
            bool HasFlag(FSFlags Flag) const {
                return (static_cast<uint32_t>(Flags) & static_cast<uint32_t>(Flag)) != 0;
            }

            /// @brief The indices as plain triangles (three per face).
            ///
            /// Strips are expanded; AllowPrimitiveRestarts says whether 0xFFFF in a strip means "start over" (true for
            /// meshes with fewer than 65535 vertices).
            std::vector<int32_t> Triangulate(bool AllowPrimitiveRestarts, bool IncludeDegenerateFaces = false) const;

            /// @brief 16 or 32: the smallest index size that can hold every index.
            int32_t GetVertexIndexSize() const;
            /// @brief Adds this face set's triangles to the counts: those with area, and all of them.
            void AddFaceCounts(bool AllowPrimitiveRestarts, int32_t& TrueFaceCount, int32_t& TotalFaceCount) const;
        };

        /// @brief Which buffer layout a vertex buffer uses.
        ///
        /// (A mesh's vertices are stored in 1 to 3 buffers, each holding a different set of data for every vertex; here
        /// they've been merged into the mesh's Vertices.)
        struct SOULS_API VertexBuffer {
            /// @brief Index to a layout in the FLVER's layout collection.
            int32_t LayoutIndex = 0;
        };

        /// @brief An optional bounding box for meshes added in DS2.
        struct SOULS_API BoundingBoxes {
            /// @brief Minimum extent of the mesh.
            Vector3 Min {-FLT_MAX, -FLT_MAX, -FLT_MAX};
            /// @brief Maximum extent of the mesh.
            Vector3 Max {FLT_MAX, FLT_MAX, FLT_MAX};
            Vector3 Unk;  ///< Sekiro and later
        };

        /// @brief An individual chunk of a model.
        struct SOULS_API Mesh {
            uint8_t Dynamic = 0;  ///< 0 for static meshes, 1 for ones bound to bones
            /// @brief Index of the material used by all triangles in this mesh.
            int32_t MaterialIndex = 0;
            /// @brief Apparently does nothing. Usually points to a dummy bone named after the model, possibly just for
            /// labelling.
            int32_t DefaultBoneIndex = -1;
            /// @brief Which bones the vertices' bone indices refer to.
            std::vector<int32_t> BoneIndices;
            /// @brief Triangles in this mesh.
            std::vector<FaceSet> FaceSets;
            /// @brief Vertex buffers in this mesh.
            std::vector<VertexBuffer> VertexBuffers;
            /// @brief Vertices in this mesh.
            std::vector<FLVER::Vertex> Vertices;
            /// @brief Optional bounding box struct; may be null.
            std::optional<BoundingBoxes> BoundingBox;

            /// @brief The triangles of the face set with the given flags (or the first one) as vertices.
            ///
            /// Pointers are into Vertices.
            std::vector<std::array<const FLVER::Vertex*, 3>>
            GetFaces(FaceSet::FSFlags Flags = FaceSet::FSFlags::None) const;
        };

        /// @brief Unknown; only present in Sekiro.
        struct SOULS_API SekiroUnkStruct {
            /// @brief Unknown.
            struct Member {
                /// @brief Unknown; maybe bone indices? Length 4.
                std::array<int16_t, 4> Unk00 {};
                /// @brief Unknown; seems to just count up from 0.
                int32_t Index = 0;
            };
            /// @brief Unknown.
            std::vector<Member> Members1;
            /// @brief Unknown.
            std::vector<Member> Members2;
        };

        /// @brief General values for this model.
        FLVERHeader Header;
        /// @brief Dummy polygons in this model.
        std::vector<FLVER::Dummy> Dummies;
        /// @brief Materials in this model, usually one per mesh.
        std::vector<Material> Materials;
        /// @brief Lists of GX elements referenced by materials in DS2 and beyond.
        std::vector<GXList> GXLists;
        /// @brief Bones used by this model, may or may not be the full skeleton.
        std::vector<FLVER::Bone> Bones;
        /// @brief Individual chunks of the model.
        std::vector<Mesh> Meshes;
        /// @brief Layouts determining how to write vertex information.
        std::vector<FLVER::LayoutMembers> BufferLayouts;
        /// @brief Present from version 0x2001A.
        std::optional<SekiroUnkStruct> SekiroUnk;

        /// @brief An empty model configured for Dark Souls III.
        FLVER2() = default;

    protected:
        bool IsImpl(BinaryReader& Reader) override;
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

    /// @brief Combines face set flags.
    constexpr FLVER2::FaceSet::FSFlags operator|(FLVER2::FaceSet::FSFlags A, FLVER2::FaceSet::FSFlags B) {
        return static_cast<FLVER2::FaceSet::FSFlags>(static_cast<uint32_t>(A) | static_cast<uint32_t>(B));
    }

#pragma warning(pop)

}  // namespace Souls
