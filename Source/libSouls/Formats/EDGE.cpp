//
// Created by Jake Rieger on 10/8/2026.
//

#include "EDGE.hpp"

namespace Souls {
    void EDGE::ReadImpl(BinaryReader& Reader) {
        Reader.Order = Endian::Little;
        Reader.Assert<int32_t>(4);
        const int32_t EdgeCount = Reader.ReadInt32();
        ID                      = Reader.ReadInt32();
        Reader.Assert<int32_t>(0);

        Edges.clear();
        Edges.reserve(static_cast<size_t>(EdgeCount));
        for (int32_t I = 0; I < EdgeCount; ++I) {
            Edge E;
            E.V1 = Reader.ReadVector3();
            Reader.Assert<float>(1);
            E.V2 = Reader.ReadVector3();
            Reader.Assert<float>(1);
            E.V3          = Reader.ReadVector3();
            E.Unk2C       = Reader.ReadFloat();
            E.Unk30       = Reader.ReadInt32();
            E.Type        = static_cast<EdgeType>(Reader.ReadByte());
            E.VariationID = Reader.ReadByte();
            E.Unk36       = Reader.ReadByte();
            Reader.Assert<uint8_t>(0);
            Reader.Assert<int32_t>(0);
            Reader.Assert<int32_t>(0);
            Edges.push_back(E);
        }
    }

    void EDGE::WriteImpl(BinaryWriter& Writer) {
        Writer.Order = Endian::Little;
        Writer.WriteInt32(4);
        Writer.WriteInt32(static_cast<int32_t>(Edges.size()));
        Writer.WriteInt32(ID);
        Writer.WriteInt32(0);
        for (const Edge& E : Edges) {
            Writer.WriteVector3(E.V1);
            Writer.WriteFloat(1);
            Writer.WriteVector3(E.V2);
            Writer.WriteFloat(1);
            Writer.WriteVector3(E.V3);
            Writer.WriteFloat(E.Unk2C);
            Writer.WriteInt32(E.Unk30);
            Writer.WriteByte(static_cast<uint8_t>(E.Type));
            Writer.WriteByte(E.VariationID);
            Writer.WriteByte(E.Unk36);
            Writer.WriteByte(0);
            Writer.WriteInt32(0);
            Writer.WriteInt32(0);
        }
    }
}  // namespace Souls
