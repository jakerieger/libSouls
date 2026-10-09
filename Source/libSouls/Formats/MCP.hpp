//
// Created by Jake Rieger on 10/8/2026.
//

#pragma once

#include <libSouls/SoulsFile.hpp>

#include <vector>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of an exported class; only touched by this DLL's own code

    // MCP: a navigation format used in Demon's Souls and Dark Souls that defines a basic graph of connected volumes.
    // Extension: .mcp
    class SOULS_API MCP : public SoulsFile<MCP> {
    public:
        // A volume of space with connections to other rooms.
        struct SOULS_API Room {
            // The ID of the map the room is in, where mAA_BB_CC_DD is packed into bytes AABBCCDD.
            uint32_t MapID = 0;
            // Index of the room among rooms with the same map ID, for MCPs that span multiple maps.
            int32_t LocalIndex = 0;
            Vector3 BoundingBoxMin;
            Vector3 BoundingBoxMax;
            // Indices of rooms connected to this one.
            std::vector<int32_t> ConnectedRoomIndices;
        };

        // True for Demon's Souls, false for Dark Souls.
        bool BigEndian = false;
        int32_t Unk04  = 0;
        std::vector<Room> Rooms;

    protected:
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
