//
// Created by Jake Rieger on 10/8/2026.
//

#pragma once

#include <libSouls/SoulsFile.hpp>
#include <libSouls/ValuePtr.hpp>

#include <vector>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of an exported class; only touched by this DLL's own code

    // NVM: the navmesh format used in Demon's Souls and Dark Souls. Extension: .nvm
    class SOULS_API NVM : public SoulsFile<NVM> {
    public:
        // Available AI properties of a triangle.
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

        // A surface with flags indicating how AI should behave on it.
        struct Triangle {
            int32_t VertexIndex1 = 0, VertexIndex2 = 0, VertexIndex3 = 0;
            // Indices of the triangles adjacent to the 1-2, 2-3 and 1-3 edges, if any.
            int32_t EdgeIndex1 = 0, EdgeIndex2 = 0, EdgeIndex3 = 0;
            // Number of breakable objects on this triangle.
            int32_t ObstacleCount = 0;
            // Controls AI behavior on this triangle.
            TriangleFlags Flags = TriangleFlags::None;
        };

        // A rectangular prism in a tree structure encompassing the navmesh.
        struct Box {
            // Two corners defining the extent of the box.
            Vector3 Corner1, Corner2;
            // Indices of triangles within this box. Only used for leaf nodes.
            std::vector<int32_t> TriangleIndices;
            // The four boxes that subdivide this one (any may be absent).
            ValuePtr<Box> ChildBox1, ChildBox2, ChildBox3, ChildBox4;
        };

        // A list of triangles that can be disabled via event entity ID.
        struct Entity {
            int32_t EntityID = 0;
            std::vector<int32_t> TriangleIndices;
        };

        // True for the Demon's Souls format, false for Dark Souls.
        bool BigEndian = false;
        std::vector<Vector3> Vertices;
        std::vector<Triangle> Triangles;
        Box RootBox;
        std::vector<Entity> Entities;

    protected:
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

    constexpr NVM::TriangleFlags operator|(NVM::TriangleFlags A, NVM::TriangleFlags B) {
        return static_cast<NVM::TriangleFlags>(static_cast<int32_t>(A) | static_cast<int32_t>(B));
    }

#pragma warning(pop)

}  // namespace Souls
