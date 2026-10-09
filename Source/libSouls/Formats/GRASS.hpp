//
// Created by Jake Rieger on 10/8/2026.
//

#pragma once

#include <libSouls/SoulsFile.hpp>

#include <array>
#include <vector>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of an exported class; only touched by this DLL's own code

    // GRASS: defines a dynamic grass mesh attached to a model; only used in Sekiro. Extension: .grass
    class SOULS_API GRASS : public SoulsFile<GRASS> {
    public:
        // Defines the space contained by a Volume.
        struct BoundingBox {
            Vector3 Min, Max;
        };

        // A volume of space in the bounding volume hierarchy.
        struct Volume {
            // Index of first child volume and of the last, exclusive.
            int32_t StartChildIndex = 0, EndChildIndex = 0;
            // Index of first contained face and of the last, exclusive.
            int32_t StartFaceIndex = 0, EndFaceIndex = 0;
            int32_t Unk10          = 0;
            // Space contained within the volume.
            BoundingBox Box;
        };

        // A point in the grass mesh with weights for each grass type.
        struct Vertex {
            // Position of the vertex, relative to the parent model.
            Vector3 Position;
            // Densities of the six possible grass types; usual range is 0 to 1 but higher is supported.
            std::array<float, 6> GrassDensities{};
        };

        // A triangular patch of grass.
        struct Face {
            // Unknown; affects direction/rotation somehow, components range from -1 to 1.
            Vector3 Unk00;
            int32_t VertexIndexA = 0, VertexIndexB = 0, VertexIndexC = 0;
        };

        // A recursive subdivision of space for efficient culling or collision testing.
        std::vector<Volume> BoundingVolumeHierarchy;
        // Points making up the grass mesh.
        std::vector<Vertex> Vertices;
        // Triangular patches of grass.
        std::vector<Face> Faces;

    protected:
        bool IsImpl(BinaryReader& Reader) override;
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
