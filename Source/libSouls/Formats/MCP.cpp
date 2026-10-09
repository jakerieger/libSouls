//
// Created by Jake Rieger on 10/8/2026.
//

#include "MCP.hpp"

namespace Souls {
    void MCP::ReadImpl(BinaryReader& Reader) {
        Reader.Order = Endian::Big;
        BigEndian    = Reader.Assert<int32_t>(2, 0x2000000) == 2;
        Reader.Order = BigEndian ? Endian::Big : Endian::Little;

        Unk04                     = Reader.ReadInt32();
        const int32_t RoomCount   = Reader.ReadInt32();
        const int32_t RoomsOffset = Reader.ReadInt32();

        Reader.Seek(RoomsOffset);
        Rooms.clear();
        Rooms.reserve(static_cast<size_t>(RoomCount));
        for (int32_t I = 0; I < RoomCount; ++I) {
            Room R;
            R.MapID                     = Reader.ReadUInt32();
            R.LocalIndex                = Reader.ReadInt32();
            const int32_t IndexCount    = Reader.ReadInt32();
            const int32_t IndicesOffset = Reader.ReadInt32();
            R.BoundingBoxMin            = Reader.ReadVector3();
            R.BoundingBoxMax            = Reader.ReadVector3();
            Reader.StepIn(IndicesOffset);
            R.ConnectedRoomIndices = Reader.ReadArray<int32_t>(static_cast<size_t>(IndexCount));
            Reader.StepOut();
            Rooms.push_back(std::move(R));
        }
    }

    void MCP::WriteImpl(BinaryWriter& Writer) {
        Writer.Order = BigEndian ? Endian::Big : Endian::Little;
        Writer.WriteInt32(2);
        Writer.WriteInt32(Unk04);
        Writer.WriteInt32(static_cast<int32_t>(Rooms.size()));
        Writer.Reserve<int32_t>("RoomsOffset");

        std::vector<int64_t> IndicesOffsets(Rooms.size());
        for (size_t I = 0; I < Rooms.size(); ++I) {
            IndicesOffsets[I] = Writer.Position();
            Writer.WriteArray(Rooms[I].ConnectedRoomIndices);
        }

        Writer.Fill<int32_t>("RoomsOffset", static_cast<int32_t>(Writer.Position()));
        for (size_t I = 0; I < Rooms.size(); ++I) {
            const Room& R = Rooms[I];
            Writer.WriteUInt32(R.MapID);
            Writer.WriteInt32(R.LocalIndex);
            Writer.WriteInt32(static_cast<int32_t>(R.ConnectedRoomIndices.size()));
            Writer.WriteInt32(static_cast<int32_t>(IndicesOffsets[I]));
            Writer.WriteVector3(R.BoundingBoxMin);
            Writer.WriteVector3(R.BoundingBoxMax);
        }
    }
}  // namespace Souls
