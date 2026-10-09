//
// Created by Jake Rieger on 10/8/2026.
//

#include "MCG.hpp"

namespace Souls {
    static std::vector<int32_t> GetInt32s(BinaryReader& Reader, int64_t Offset, int32_t Count) {
        Reader.StepIn(Offset);
        std::vector<int32_t> Values = Reader.ReadArray<int32_t>(static_cast<size_t>(Count));
        Reader.StepOut();
        return Values;
    }

    void MCG::ReadImpl(BinaryReader& Reader) {
        Reader.Order = Endian::Big;
        BigEndian    = Reader.Assert<int32_t>(1, 0x1000000) == 1;
        Reader.Order = BigEndian ? Endian::Big : Endian::Little;

        Unk04                     = Reader.ReadInt32();
        const int32_t NodeCount   = Reader.ReadInt32();
        const int32_t NodesOffset = Reader.ReadInt32();
        const int32_t EdgeCount   = Reader.ReadInt32();
        const int32_t EdgesOffset = Reader.ReadInt32();
        Unk18                     = Reader.ReadInt32();
        Unk1C                     = Reader.ReadInt32();

        Reader.Seek(NodesOffset);
        Nodes.clear();
        Nodes.reserve(static_cast<size_t>(NodeCount));
        for (int32_t I = 0; I < NodeCount; ++I) {
            Node N;
            const int32_t ConnectionCount = Reader.ReadInt32();
            N.Position                    = Reader.ReadVector3();
            const int32_t NodeIndicesOffset = Reader.ReadInt32();
            const int32_t EdgeIndicesOffset = Reader.ReadInt32();
            N.Unk18                         = Reader.ReadInt32();
            N.Unk1C                         = Reader.ReadInt32();
            N.ConnectedNodeIndices          = GetInt32s(Reader, NodeIndicesOffset, ConnectionCount);
            N.ConnectedEdgeIndices          = GetInt32s(Reader, EdgeIndicesOffset, ConnectionCount);
            Nodes.push_back(std::move(N));
        }

        Reader.Seek(EdgesOffset);
        Edges.clear();
        Edges.reserve(static_cast<size_t>(EdgeCount));
        for (int32_t I = 0; I < EdgeCount; ++I) {
            Edge E;
            E.NodeIndexA              = Reader.ReadInt32();
            const int32_t CountA      = Reader.ReadInt32();
            const int32_t OffsetA     = Reader.ReadInt32();
            E.NodeIndexB              = Reader.ReadInt32();
            const int32_t CountB      = Reader.ReadInt32();
            const int32_t OffsetB     = Reader.ReadInt32();
            E.MCPRoomIndex            = Reader.ReadInt32();
            E.MapID                   = Reader.ReadUInt32();
            E.Unk20                   = Reader.ReadFloat();
            E.UnkIndicesA             = GetInt32s(Reader, OffsetA, CountA);
            E.UnkIndicesB             = GetInt32s(Reader, OffsetB, CountB);
            Edges.push_back(std::move(E));
        }
    }

    void MCG::WriteImpl(BinaryWriter& Writer) {
        Writer.Order = BigEndian ? Endian::Big : Endian::Little;
        Writer.WriteInt32(1);
        Writer.WriteInt32(Unk04);
        Writer.WriteInt32(static_cast<int32_t>(Nodes.size()));
        Writer.Reserve<int32_t>("NodesOffset");
        Writer.WriteInt32(static_cast<int32_t>(Edges.size()));
        Writer.Reserve<int32_t>("EdgesOffset");
        Writer.WriteInt32(Unk18);
        Writer.WriteInt32(Unk1C);

        std::vector<int64_t> EdgeIndicesA(Edges.size()), EdgeIndicesB(Edges.size());
        for (size_t I = 0; I < Edges.size(); ++I) {
            EdgeIndicesA[I] = Writer.Position();
            Writer.WriteArray(Edges[I].UnkIndicesA);
            EdgeIndicesB[I] = Writer.Position();
            Writer.WriteArray(Edges[I].UnkIndicesB);
        }

        std::vector<int64_t> NodeNodeIndices(Nodes.size()), NodeEdgeIndices(Nodes.size());
        for (size_t I = 0; I < Nodes.size(); ++I) {
            const Node& N      = Nodes[I];
            NodeNodeIndices[I] = N.ConnectedNodeIndices.empty() ? 0 : Writer.Position();
            Writer.WriteArray(N.ConnectedNodeIndices);
            NodeEdgeIndices[I] = N.ConnectedEdgeIndices.empty() ? 0 : Writer.Position();
            Writer.WriteArray(N.ConnectedEdgeIndices);
        }

        Writer.Fill<int32_t>("EdgesOffset", static_cast<int32_t>(Writer.Position()));
        for (size_t I = 0; I < Edges.size(); ++I) {
            const Edge& E = Edges[I];
            Writer.WriteInt32(E.NodeIndexA);
            Writer.WriteInt32(static_cast<int32_t>(E.UnkIndicesA.size()));
            Writer.WriteInt32(static_cast<int32_t>(EdgeIndicesA[I]));
            Writer.WriteInt32(E.NodeIndexB);
            Writer.WriteInt32(static_cast<int32_t>(E.UnkIndicesB.size()));
            Writer.WriteInt32(static_cast<int32_t>(EdgeIndicesB[I]));
            Writer.WriteInt32(E.MCPRoomIndex);
            Writer.WriteUInt32(E.MapID);
            Writer.WriteFloat(E.Unk20);
        }

        Writer.Fill<int32_t>("NodesOffset", static_cast<int32_t>(Writer.Position()));
        for (size_t I = 0; I < Nodes.size(); ++I) {
            const Node& N = Nodes[I];
            Writer.WriteInt32(static_cast<int32_t>(N.ConnectedNodeIndices.size()));
            Writer.WriteVector3(N.Position);
            Writer.WriteInt32(static_cast<int32_t>(NodeNodeIndices[I]));
            Writer.WriteInt32(static_cast<int32_t>(NodeEdgeIndices[I]));
            Writer.WriteInt32(N.Unk18);
            Writer.WriteInt32(N.Unk1C);
        }
    }
}  // namespace Souls
