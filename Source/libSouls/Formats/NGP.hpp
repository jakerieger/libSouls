//
// Created by Jake Rieger on 10/8/2026.
//

#pragma once

#include <libSouls/SoulsFile.hpp>
#include <libSouls/ValuePtr.hpp>

#include <optional>
#include <vector>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of an exported class; only touched by this DLL's own code

    // NGP: the navmesh format used in Dark Souls II. Extension: .ngp. Most fields are not understood yet.
    class SOULS_API NGP : public SoulsFile<NGP> {
    public:
        enum class NGPVersion : uint16_t {
            Vanilla = 1,
            Scholar = 2,
        };

        struct StructA {
            Vector3 Unk00;
            float Unk0C   = 0;
            int32_t Unk10 = 0;
            int16_t Unk14 = 0, Unk16 = 0, Unk18 = 0, Unk1A = 0, Unk1C = 0, Unk1E = 0, Unk20 = 0, Unk22 = 0;
        };

        struct StructB {
            int32_t Unk00 = 0, Unk04 = 0, Unk08 = 0;
        };

        struct Face {
            int16_t V1 = 0, V2 = 0, V3 = 0, Unk06 = 0, Unk08 = 0, Unk0A = 0;
        };

        struct Struct4 {
            int16_t Unk00 = 0, Unk02 = 0, Unk04 = 0, Unk06 = 0, Unk08 = 0, Unk0A = 0, Unk0C = 0, Unk0E = 0;
        };

        // A node of the tree of face groups (a kd-tree, apparently).
        struct Struct5 {
            float Unk00 = 0;
            ValuePtr<Struct5> Left;
            ValuePtr<Struct5> Right;
            // Absent for nodes with no faces.
            std::optional<std::vector<int16_t>> FaceIndices;
        };

        struct Mesh {
            int32_t Unk00 = 0;
            int32_t Unk08 = 0;
            Vector3 BoundingBoxMin;
            Vector3 BoundingBoxMax;
            int16_t Unk30 = 0;
            int16_t Unk32 = 0;
            std::vector<Vector3> Vertices;
            std::vector<int32_t> Struct2s;
            std::vector<Face> Faces;
            std::vector<Struct4> Struct4s;
            Struct5 Struct5Root;
        };

        bool BigEndian = false;
        NGPVersion Version = NGPVersion::Vanilla;
        int32_t Unk1C      = 0;
        std::vector<StructA> StructAs;
        std::vector<StructB> StructBs;
        // Unknown, maybe pairs of shorts.
        std::vector<int32_t> StructCs;
        std::vector<int16_t> StructDs;
        std::vector<Mesh> Meshes;

    protected:
        bool IsImpl(BinaryReader& Reader) override;
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
