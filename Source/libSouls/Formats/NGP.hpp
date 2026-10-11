/// @file
/// @author Jake Rieger
/// @date 10/8/2026

#pragma once

#include <libSouls/SoulsFile.hpp>
#include <libSouls/ValuePtr.hpp>

#include <optional>
#include <vector>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of an exported class; only touched by this DLL's own code

    /// @brief NGP: the navmesh format used in Dark Souls II.
    ///
    /// Extension: .ngp. Most fields are not understood yet.
    class SOULS_API NGP : public SoulsFile<NGP> {
    public:
        /// @brief Which version of Dark Souls II the file is from.
        enum class NGPVersion : uint16_t {
            Vanilla = 1,
            Scholar = 2,
        };

        /// @brief Unknown.
        struct StructA {
            /// @brief Unknown.
            Vector3 Unk00;
            /// @brief Unknown.
            float Unk0C   = 0;
            /// @brief Unknown.
            int32_t Unk10 = 0;
            /// @brief Unknown.
            int16_t Unk14 = 0;
            /// @brief Unknown.
            int16_t Unk16 = 0;
            /// @brief Unknown.
            int16_t Unk18 = 0;
            /// @brief Unknown.
            int16_t Unk1A = 0;
            /// @brief Unknown.
            int16_t Unk1C = 0;
            /// @brief Unknown.
            int16_t Unk1E = 0;
            /// @brief Unknown.
            int16_t Unk20 = 0;
            /// @brief Unknown.
            int16_t Unk22 = 0;
        };

        /// @brief Unknown.
        struct StructB {
            /// @brief Unknown.
            int32_t Unk00 = 0;
            /// @brief Unknown.
            int32_t Unk04 = 0;
            /// @brief Unknown.
            int32_t Unk08 = 0;
        };

        /// @brief A triangle of the navmesh.
        struct Face {
            /// @brief Index of the first vertex.
            int16_t V1 = 0;
            /// @brief Index of the second vertex.
            int16_t V2 = 0;
            /// @brief Index of the third vertex.
            int16_t V3 = 0;
            /// @brief Unknown.
            int16_t Unk06 = 0;
            /// @brief Unknown.
            int16_t Unk08 = 0;
            /// @brief Unknown.
            int16_t Unk0A = 0;
        };

        /// @brief Unknown.
        struct Struct4 {
            /// @brief Unknown.
            int16_t Unk00 = 0;
            /// @brief Unknown.
            int16_t Unk02 = 0;
            /// @brief Unknown.
            int16_t Unk04 = 0;
            /// @brief Unknown.
            int16_t Unk06 = 0;
            /// @brief Unknown.
            int16_t Unk08 = 0;
            /// @brief Unknown.
            int16_t Unk0A = 0;
            /// @brief Unknown.
            int16_t Unk0C = 0;
            /// @brief Unknown.
            int16_t Unk0E = 0;
        };

        /// @brief A node of the tree of face groups (a kd-tree, apparently).
        struct Struct5 {
            /// @brief Unknown.
            float Unk00 = 0;
            /// @brief The left child node, if any.
            ValuePtr<Struct5> Left;
            /// @brief The right child node, if any.
            ValuePtr<Struct5> Right;
            /// @brief Absent for nodes with no faces.
            std::optional<std::vector<int16_t>> FaceIndices;
        };

        /// @brief One navmesh.
        struct Mesh {
            /// @brief Unknown.
            int32_t Unk00 = 0;
            /// @brief Unknown.
            int32_t Unk08 = 0;
            /// @brief The minimum corner of the mesh's bounding box.
            Vector3 BoundingBoxMin;
            /// @brief The maximum corner of the mesh's bounding box.
            Vector3 BoundingBoxMax;
            /// @brief Unknown.
            int16_t Unk30 = 0;
            /// @brief Unknown.
            int16_t Unk32 = 0;
            /// @brief The mesh's vertices.
            std::vector<Vector3> Vertices;
            /// @brief Unknown.
            std::vector<int32_t> Struct2s;
            /// @brief The mesh's triangles.
            std::vector<Face> Faces;
            /// @brief Unknown.
            std::vector<Struct4> Struct4s;
            /// @brief The root of the tree of face groups.
            Struct5 Struct5Root;
        };

        /// @brief Whether the file is big-endian (console versions).
        bool BigEndian = false;
        /// @brief The version of Dark Souls II the file is from.
        NGPVersion Version = NGPVersion::Vanilla;
        /// @brief Unknown.
        int32_t Unk1C      = 0;
        /// @brief Unknown.
        std::vector<StructA> StructAs;
        /// @brief Unknown.
        std::vector<StructB> StructBs;
        /// @brief Unknown, maybe pairs of shorts.
        std::vector<int32_t> StructCs;
        /// @brief Unknown.
        std::vector<int16_t> StructDs;
        /// @brief The navmeshes.
        std::vector<Mesh> Meshes;

    protected:
        bool IsImpl(BinaryReader& Reader) override;
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
