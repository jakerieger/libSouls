//
// Created by Jake Rieger on 10/8/2026.
//

#include "NVM.hpp"

#include <deque>

namespace Souls {
    static NVM::Box ReadBox(BinaryReader& Reader) {
        NVM::Box B;
        B.Corner1                  = Reader.ReadVector3();
        const int32_t TriCount     = Reader.ReadInt32();
        B.Corner2                  = Reader.ReadVector3();
        const int32_t TriOffset    = Reader.ReadInt32();
        const int32_t BoxOffsets[4] = {Reader.ReadInt32(), Reader.ReadInt32(), Reader.ReadInt32(), Reader.ReadInt32()};
        Reader.AssertPattern(16, 0);

        if (TriCount > 0) {
            Reader.StepIn(TriOffset);
            B.TriangleIndices = Reader.ReadArray<int32_t>(static_cast<size_t>(TriCount));
            Reader.StepOut();
        }

        ValuePtr<NVM::Box>* Children[4] = {&B.ChildBox1, &B.ChildBox2, &B.ChildBox3, &B.ChildBox4};
        for (int I = 0; I < 4; ++I) {
            if (BoxOffsets[I] != 0) {
                Reader.Seek(BoxOffsets[I]);
                Children[I]->Emplace(ReadBox(Reader));
            }
        }
        return B;
    }

    // Children are written before their parents, bottom-up, and the box returns its own offset.
    static int32_t WriteBox(BinaryWriter& Writer, const NVM::Box& B, std::deque<int32_t>& TriangleIndexOffsets) {
        const ValuePtr<NVM::Box>* Children[4] = {&B.ChildBox1, &B.ChildBox2, &B.ChildBox3, &B.ChildBox4};
        int32_t Offsets[4]                    = {0, 0, 0, 0};
        for (int I = 0; I < 4; ++I) {
            if (*Children[I]) {
                Offsets[I] = WriteBox(Writer, **Children[I], TriangleIndexOffsets);
            }
        }

        const int32_t ThisOffset = static_cast<int32_t>(Writer.Position());
        Writer.WriteVector3(B.Corner1);
        Writer.WriteInt32(static_cast<int32_t>(B.TriangleIndices.size()));
        Writer.WriteVector3(B.Corner2);
        Writer.WriteInt32(TriangleIndexOffsets.front());
        TriangleIndexOffsets.pop_front();
        for (const int32_t Offset : Offsets) {
            Writer.WriteInt32(Offset);
        }
        Writer.Pad(16);
        return ThisOffset;
    }

    static void WriteBoxTriangleIndices(BinaryWriter& Writer, const NVM::Box& B, std::deque<int32_t>& Offsets) {
        const ValuePtr<NVM::Box>* Children[4] = {&B.ChildBox1, &B.ChildBox2, &B.ChildBox3, &B.ChildBox4};
        for (const ValuePtr<NVM::Box>* Child : Children) {
            if (*Child) {
                WriteBoxTriangleIndices(Writer, **Child, Offsets);
            }
        }
        if (B.TriangleIndices.empty()) {
            Offsets.push_back(0);
        } else {
            Offsets.push_back(static_cast<int32_t>(Writer.Position()));
            Writer.WriteArray(B.TriangleIndices);
        }
    }

    void NVM::ReadImpl(BinaryReader& Reader) {
        Reader.Order = Endian::Little;
        BigEndian    = Reader.Assert<int32_t>(1, 0x1000000) != 1;
        Reader.Order = BigEndian ? Endian::Big : Endian::Little;

        const int32_t VertexCount = Reader.ReadInt32();
        Reader.Assert<int32_t>(0x80);  // vertex offset
        const int32_t TriangleCount = Reader.ReadInt32();
        Reader.Assert<int32_t>(0x80 + VertexCount * 0xC);  // triangle offset
        const int32_t RootBoxOffset = Reader.ReadInt32();
        Reader.Assert<int32_t>(0);
        const int32_t EntityCount  = Reader.ReadInt32();
        const int32_t EntityOffset = Reader.ReadInt32();
        Reader.AssertPattern(23 * 4, 0);

        Vertices.clear();
        Vertices.reserve(static_cast<size_t>(VertexCount));
        for (int32_t I = 0; I < VertexCount; ++I) {
            Vertices.push_back(Reader.ReadVector3());
        }

        Triangles.clear();
        Triangles.reserve(static_cast<size_t>(TriangleCount));
        for (int32_t I = 0; I < TriangleCount; ++I) {
            Triangle T;
            T.VertexIndex1 = Reader.ReadInt32();
            T.VertexIndex2 = Reader.ReadInt32();
            T.VertexIndex3 = Reader.ReadInt32();
            T.EdgeIndex1   = Reader.ReadInt32();
            T.EdgeIndex2   = Reader.ReadInt32();
            T.EdgeIndex3   = Reader.ReadInt32();
            // Obstacle count and flags are packed together. Seems janky, but it works for Dark Souls.
            const int32_t ObstaclesAndFlags = Reader.ReadInt32();
            T.ObstacleCount                 = (ObstaclesAndFlags >> 2) & 0x3FFF;
            T.Flags                         = static_cast<TriangleFlags>(ObstaclesAndFlags >> 16);
            if ((ObstaclesAndFlags & 3) != 0) {
                throw BinaryException("Lower 2 bits of obstacle count are expected to be 0, but were not.");
            }
            Triangles.push_back(T);
        }

        Reader.Seek(RootBoxOffset);
        RootBox = ReadBox(Reader);

        Reader.Seek(EntityOffset);
        Entities.clear();
        Entities.reserve(static_cast<size_t>(EntityCount));
        for (int32_t I = 0; I < EntityCount; ++I) {
            Entity E;
            E.EntityID                  = Reader.ReadInt32();
            const int32_t IndexOffset   = Reader.ReadInt32();
            const int32_t IndexCount    = Reader.ReadInt32();
            Reader.Assert<int32_t>(0);
            Reader.StepIn(IndexOffset);
            E.TriangleIndices = Reader.ReadArray<int32_t>(static_cast<size_t>(IndexCount));
            Reader.StepOut();
            Entities.push_back(std::move(E));
        }
    }

    void NVM::WriteImpl(BinaryWriter& Writer) {
        Writer.Order = BigEndian ? Endian::Big : Endian::Little;
        Writer.WriteInt32(1);
        Writer.WriteInt32(static_cast<int32_t>(Vertices.size()));
        Writer.Reserve<int32_t>("VertexOffset");
        Writer.WriteInt32(static_cast<int32_t>(Triangles.size()));
        Writer.Reserve<int32_t>("TriangleOffset");
        Writer.Reserve<int32_t>("RootBoxOffset");
        Writer.WriteInt32(0);
        Writer.WriteInt32(static_cast<int32_t>(Entities.size()));
        Writer.Reserve<int32_t>("EntityOffset");
        Writer.Pad(23 * 4);

        Writer.Fill<int32_t>("VertexOffset", static_cast<int32_t>(Writer.Position()));
        for (const Vector3& Vertex : Vertices) {
            Writer.WriteVector3(Vertex);
        }

        Writer.Fill<int32_t>("TriangleOffset", static_cast<int32_t>(Writer.Position()));
        for (const Triangle& T : Triangles) {
            Writer.WriteInt32(T.VertexIndex1);
            Writer.WriteInt32(T.VertexIndex2);
            Writer.WriteInt32(T.VertexIndex3);
            Writer.WriteInt32(T.EdgeIndex1);
            Writer.WriteInt32(T.EdgeIndex2);
            Writer.WriteInt32(T.EdgeIndex3);
            Writer.WriteInt32((T.ObstacleCount << 2) | (static_cast<int32_t>(T.Flags) << 16));
        }

        std::deque<int32_t> BoxTriangleIndexOffsets;
        WriteBoxTriangleIndices(Writer, RootBox, BoxTriangleIndexOffsets);
        const int32_t RootBoxOffset = WriteBox(Writer, RootBox, BoxTriangleIndexOffsets);
        Writer.Fill<int32_t>("RootBoxOffset", RootBoxOffset);

        std::vector<int32_t> EntityTriangleIndexOffsets;
        for (const Entity& E : Entities) {
            EntityTriangleIndexOffsets.push_back(static_cast<int32_t>(Writer.Position()));
            Writer.WriteArray(E.TriangleIndices);
        }

        Writer.Fill<int32_t>("EntityOffset", static_cast<int32_t>(Writer.Position()));
        for (size_t I = 0; I < Entities.size(); ++I) {
            Writer.WriteInt32(Entities[I].EntityID);
            Writer.WriteInt32(EntityTriangleIndexOffsets[I]);
            Writer.WriteInt32(static_cast<int32_t>(Entities[I].TriangleIndices.size()));
            Writer.WriteInt32(0);
        }
    }
}  // namespace Souls
