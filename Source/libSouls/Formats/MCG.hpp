//
// Created by Jake Rieger on 10/8/2026.
//

#pragma once

#include <libSouls/SoulsFile.hpp>

#include <vector>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of an exported class; only touched by this DLL's own code

    // MCG: a navigation format used in Demon's Souls and Dark Souls that defines a coarse graph for moving around
    // the map. Extension: .mcg
    class SOULS_API MCG : public SoulsFile<MCG> {
    public:
        // A vertex in the map navigation graph.
        struct Node {
            Vector3 Position;
            // Indices of connected nodes; parallel to ConnectedEdgeIndices.
            std::vector<int32_t> ConnectedNodeIndices;
            // Edges leading to connected nodes; parallel to ConnectedNodeIndices.
            std::vector<int32_t> ConnectedEdgeIndices;
            // Unknown; possibly an index of another node, may be -1.
            int32_t Unk18 = -1;
            int32_t Unk1C = 0;
        };

        // A connection between two nodes.
        struct Edge {
            // Index of one of the endpoints of the edge.
            int32_t NodeIndexA = 0;
            // Unknown; not indices of anything in MCG or MCP.
            std::vector<int32_t> UnkIndicesA;
            // Index of the other endpoint of the edge.
            int32_t NodeIndexB = 0;
            std::vector<int32_t> UnkIndicesB;
            // Index of the room in the corresponding MCP file containing this edge.
            int32_t MCPRoomIndex = 0;
            // The ID of the map the edge is in, where mAA_BB_CC_DD is packed into bytes AABBCCDD.
            uint32_t MapID = 0;
            // Unknown, presumably a weight.
            float Unk20 = 0;
        };

        // True for Demon's Souls, false for Dark Souls.
        bool BigEndian = false;
        int32_t Unk04  = 0;
        std::vector<Node> Nodes;
        std::vector<Edge> Edges;
        int32_t Unk18 = 0;
        int32_t Unk1C = 0;

    protected:
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
