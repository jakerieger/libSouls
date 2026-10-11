/// @file
/// @author Jake Rieger
/// @date 10/8/2026

#pragma once

#include <libSouls/SoulsFile.hpp>
#include <libSouls/ValuePtr.hpp>

#include <vector>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of an exported class; only touched by this DLL's own code

    /// @brief NVM: the navmesh format used in Demon's Souls and Dark Souls.
    ///
    /// Extension: .nvm
    class SOULS_API NVM : public SoulsFile<NVM> {
    public:
        /// @brief Available AI properties of a triangle.
        enum class TriangleFlags : int32_t {
            None          = 0x0000,
            InsideWall    = 0x0001,
            BlockGate     = 0x0002,
            ClosedDoor    = 0x0004,
            Door          = 0x0008,
            Hole          = 0x0010,
            Ladder        = 0x0020,
            LargeSpace    = 0x0040,
            Edge          = 0x0080,
            Event         = 0x0100,
            LandingPoint  = 0x0200,
            FloorToWall   = 0x0400,
            Degenerate    = 0x0800,
            Wall          = 0x1000,
            Block         = 0x2000,
            Gate          = 0x4000,
            Disable       = 0x8000,
        };

        /// @brief A surface with flags indicating how AI should behave on it.
        struct Triangle {
            /// @brief Index of the triangle's first vertex.
            int32_t VertexIndex1 = 0;
            /// @brief Index of the triangle's second vertex.
            int32_t VertexIndex2 = 0;
            /// @brief Index of the triangle's third vertex.
            int32_t VertexIndex3 = 0;
            /// @brief Indices of the triangles adjacent to the 1-2, 2-3 and 1-3 edges, if any.
            int32_t EdgeIndex1 = 0;
            /// @brief Index of the triangle adjacent to the vertex 2-3 edge, if any.
            int32_t EdgeIndex2 = 0;
            /// @brief Index of the triangle adjacent to the vertex 1-3 edge, if any.
            int32_t EdgeIndex3 = 0;
            /// @brief Number of breakable objects on this triangle.
            int32_t ObstacleCount = 0;
            /// @brief Controls AI behavior on this triangle.
            TriangleFlags Flags = TriangleFlags::None;
        };

        /// @brief A rectangular prism in a tree structure encompassing the navmesh.
        struct Box {
            /// @brief One corner of the box.
            Vector3 Corner1;
            /// @brief The opposite corner of the box.
            Vector3 Corner2;
            /// @brief Indices of triangles within this box.
            ///
            /// Only used for leaf nodes.
            std::vector<int32_t> TriangleIndices;
            /// @brief The first subdivision.
            ValuePtr<Box> ChildBox1;
            /// @brief The second subdivision.
            ValuePtr<Box> ChildBox2;
            /// @brief The third subdivision.
            ValuePtr<Box> ChildBox3;
            /// @brief The fourth subdivision.
            ValuePtr<Box> ChildBox4;
        };

        /// @brief A list of triangles that can be disabled via event entity ID.
        struct Entity {
            /// @brief The event entity ID used to access these triangles.
            int32_t EntityID = 0;
            /// @brief The triangles to be disabled from an event script.
            std::vector<int32_t> TriangleIndices;
        };

        /// @brief True for the Demon's Souls format, false for Dark Souls.
        bool BigEndian = false;
        /// @brief Positions of triangle vertices.
        std::vector<Vector3> Vertices;
        /// @brief Triangles in this navmesh.
        std::vector<Triangle> Triangles;
        /// @brief The topmost box in the hierarchy of boxes encompassing the navmesh.
        Box RootBox;
        /// @brief Enables disabling of specific triangles by event entity ID.
        std::vector<Entity> Entities;

    protected:
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

    /// @brief Combines triangle flags.
    constexpr NVM::TriangleFlags operator|(NVM::TriangleFlags A, NVM::TriangleFlags B) {
        return static_cast<NVM::TriangleFlags>(static_cast<int32_t>(A) | static_cast<int32_t>(B));
    }

#pragma warning(pop)

}  // namespace Souls
