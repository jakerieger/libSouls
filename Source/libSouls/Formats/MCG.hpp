// Copyright (C) 2026 Jake Rieger
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, version 3. This program is distributed in the hope that it will
// be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU General
// Public License along with this program. If not, see https://www.gnu.org/licenses/.

#pragma once

#include <libSouls/SoulsFile.hpp>

#include <vector>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of an exported class; only touched by this DLL's own code

    /// @brief MCG: a navigation format used in Demon's Souls and Dark Souls that defines a coarse graph for moving
    ///        around the map.
    ///
    /// Extension: .mcg
    class SOULS_API MCG : public SoulsFile<MCG> {
    public:
        /// @brief A vertex in the map navigation graph.
        struct Node {
            /// @brief Coordinates of the node.
            Vector3 Position;
            /// @brief Indices of connected nodes; parallel to ConnectedEdgeIndices.
            std::vector<int32_t> ConnectedNodeIndices;
            /// @brief Edges leading to connected nodes; parallel to ConnectedNodeIndices.
            std::vector<int32_t> ConnectedEdgeIndices;
            /// @brief Unknown; possibly an index of another node, may be -1.
            int32_t Unk18 = -1;
            /// @brief Unknown.
            int32_t Unk1C = 0;
        };

        /// @brief A connection between two nodes.
        struct Edge {
            /// @brief Index of one of the endpoints of the edge.
            int32_t NodeIndexA = 0;
            /// @brief Unknown; not indices of anything in MCG or MCP.
            std::vector<int32_t> UnkIndicesA;
            /// @brief Index of the other endpoint of the edge.
            int32_t NodeIndexB = 0;
            /// @brief Unknown; not indices of anything in MCG or MCP.
            std::vector<int32_t> UnkIndicesB;
            /// @brief Index of the room in the corresponding MCP file containing this edge.
            int32_t MCPRoomIndex = 0;
            /// @brief The ID of the map the edge is in, where mAA_BB_CC_DD is packed into bytes AABBCCDD.
            uint32_t MapID = 0;
            /// @brief Unknown, presumably a weight.
            float Unk20 = 0;
        };

        /// @brief True for Demon's Souls, false for Dark Souls.
        bool BigEndian = false;
        /// @brief Unknown; possibly uninitialized memory.
        int32_t Unk04 = 0;
        /// @brief Vertices of the navigation graph.
        std::vector<Node> Nodes;
        /// @brief Edges of the navigation graph.
        std::vector<Edge> Edges;
        /// @brief Unknown; possibly uninitialized memory.
        int32_t Unk18 = 0;
        /// @brief Unknown; possibly uninitialized memory.
        int32_t Unk1C = 0;

    protected:
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
