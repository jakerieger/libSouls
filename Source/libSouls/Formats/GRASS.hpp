/// @file
/// @author Jake Rieger
/// @date 10/8/2026

#pragma once

#include <libSouls/SoulsFile.hpp>

#include <array>
#include <vector>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of an exported class; only touched by this DLL's own code

    /// @brief GRASS: defines a dynamic grass mesh attached to a model; only used in Sekiro.
    ///
    /// Extension: .grass
    class SOULS_API GRASS : public SoulsFile<GRASS> {
    public:
        /// @brief Defines the space contained by a Volume.
        struct BoundingBox {
            /// @brief Minimum extent of the box.
            Vector3 Min;
            /// @brief Maximum extent of the box.
            Vector3 Max;
        };

        /// @brief A volume of space in the bounding volume hierarchy.
        struct Volume {
            /// @brief Index of first child volume and of the last, exclusive.
            int32_t StartChildIndex = 0;
            /// @brief Index of last child volume, exclusive.
            int32_t EndChildIndex = 0;
            /// @brief Index of first contained face and of the last, exclusive.
            int32_t StartFaceIndex = 0;
            /// @brief Index of last contained face, exclusive.
            int32_t EndFaceIndex = 0;
            /// @brief Unknown.
            int32_t Unk10          = 0;
            /// @brief Space contained within the volume.
            BoundingBox Box;
        };

        /// @brief A point in the grass mesh with weights for each grass type.
        struct Vertex {
            /// @brief Position of the vertex, relative to the parent model.
            Vector3 Position;
            /// @brief Densities of the six possible grass types; usual range is 0 to 1 but higher is supported.
            std::array<float, 6> GrassDensities{};
        };

        /// @brief A triangular patch of grass.
        struct Face {
            /// @brief Unknown; affects direction/rotation somehow, components range from -1 to 1.
            Vector3 Unk00;
            /// @brief Index of the first vertex in the triangle.
            int32_t VertexIndexA = 0;
            /// @brief Index of the second vertex in the triangle.
            int32_t VertexIndexB = 0;
            /// @brief Index of the third vertex in the triangle.
            int32_t VertexIndexC = 0;
        };

        /// @brief A recursive subdivision of space for efficient culling or collision testing.
        std::vector<Volume> BoundingVolumeHierarchy;
        /// @brief Points making up the grass mesh.
        std::vector<Vertex> Vertices;
        /// @brief Triangular patches of grass.
        std::vector<Face> Faces;

    protected:
        bool IsImpl(BinaryReader& Reader) override;
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
