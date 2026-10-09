//
// Created by Jake Rieger on 10/8/2026.
//

#include "GRASS.hpp"

namespace Souls {
    bool GRASS::IsImpl(BinaryReader& Reader) {
        if (Reader.Length() < 0x28) {
            return false;
        }
        Reader.Order = Endian::Little;
        return Reader.ReadAt<int32_t>(0) == 1 && Reader.ReadAt<int32_t>(4) == 0x28 && Reader.ReadAt<int32_t>(8) == 0x14 &&
               Reader.ReadAt<int32_t>(0x10) == 0x24 && Reader.ReadAt<int32_t>(0x18) == 0x18 &&
               Reader.ReadAt<int32_t>(0x20) == 0x18;
    }

    void GRASS::ReadImpl(BinaryReader& Reader) {
        Reader.Order = Endian::Little;
        Reader.Assert<int32_t>(1);     // version?
        Reader.Assert<int32_t>(0x28);  // header size
        Reader.Assert<int32_t>(0x14);  // volume size
        const int32_t VolumeCount = Reader.ReadInt32();
        Reader.Assert<int32_t>(0x24);  // vertex size
        const int32_t VertexCount = Reader.ReadInt32();
        Reader.Assert<int32_t>(0x18);  // face size
        const int32_t FaceCount = Reader.ReadInt32();
        Reader.Assert<int32_t>(0x18);  // bounding box size
        Reader.Assert<int32_t>(VolumeCount);

        BoundingVolumeHierarchy.clear();
        for (int32_t I = 0; I < VolumeCount; ++I) {
            Volume V;
            V.StartChildIndex = Reader.ReadInt32();
            V.EndChildIndex   = Reader.ReadInt32();
            V.StartFaceIndex  = Reader.ReadInt32();
            V.EndFaceIndex    = Reader.ReadInt32();
            V.Unk10           = Reader.ReadInt32();
            BoundingVolumeHierarchy.push_back(V);
        }
        Vertices.clear();
        for (int32_t I = 0; I < VertexCount; ++I) {
            Vertex V;
            V.Position = Reader.ReadVector3();
            Reader.ReadInto(std::span<float>(V.GrassDensities));
            Vertices.push_back(V);
        }
        Faces.clear();
        for (int32_t I = 0; I < FaceCount; ++I) {
            Face F;
            F.Unk00        = Reader.ReadVector3();
            F.VertexIndexA = Reader.ReadInt32();
            F.VertexIndexB = Reader.ReadInt32();
            F.VertexIndexC = Reader.ReadInt32();
            Faces.push_back(F);
        }
        for (Volume& V : BoundingVolumeHierarchy) {
            V.Box.Min = Reader.ReadVector3();
            V.Box.Max = Reader.ReadVector3();
        }
    }

    void GRASS::WriteImpl(BinaryWriter& Writer) {
        Writer.Order = Endian::Little;
        Writer.WriteInt32(1);
        Writer.WriteInt32(0x28);
        Writer.WriteInt32(0x14);
        Writer.WriteInt32(static_cast<int32_t>(BoundingVolumeHierarchy.size()));
        Writer.WriteInt32(0x24);
        Writer.WriteInt32(static_cast<int32_t>(Vertices.size()));
        Writer.WriteInt32(0x18);
        Writer.WriteInt32(static_cast<int32_t>(Faces.size()));
        Writer.WriteInt32(0x18);
        Writer.WriteInt32(static_cast<int32_t>(BoundingVolumeHierarchy.size()));

        for (const Volume& V : BoundingVolumeHierarchy) {
            Writer.WriteInt32(V.StartChildIndex);
            Writer.WriteInt32(V.EndChildIndex);
            Writer.WriteInt32(V.StartFaceIndex);
            Writer.WriteInt32(V.EndFaceIndex);
            Writer.WriteInt32(V.Unk10);
        }
        for (const Vertex& V : Vertices) {
            Writer.WriteVector3(V.Position);
            Writer.WriteArray(std::span<const float>(V.GrassDensities));
        }
        for (const Face& F : Faces) {
            Writer.WriteVector3(F.Unk00);
            Writer.WriteInt32(F.VertexIndexA);
            Writer.WriteInt32(F.VertexIndexB);
            Writer.WriteInt32(F.VertexIndexC);
        }
        for (const Volume& V : BoundingVolumeHierarchy) {
            Writer.WriteVector3(V.Box.Min);
            Writer.WriteVector3(V.Box.Max);
        }
    }
}  // namespace Souls
