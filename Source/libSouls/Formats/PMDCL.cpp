//
// Created by Jake Rieger on 10/8/2026.
//

#include "PMDCL.hpp"

namespace Souls {
    void PMDCL::ReadImpl(BinaryReader& Reader) {
        Reader.Order                = Endian::Little;
        const int64_t DecalCount    = Reader.ReadInt64();
        Reader.Assert<int64_t>(0x20);  // header size / offsets offset
        Reader.Assert<int64_t>(0);
        Reader.Assert<int64_t>(0);

        Decals.clear();
        Decals.reserve(static_cast<size_t>(DecalCount));
        for (int64_t I = 0; I < DecalCount; ++I) {
            const int64_t Offset = Reader.ReadInt64();
            Reader.StepIn(Offset);
            Decal D;
            D.XAngles = Reader.ReadVector3();
            Reader.Assert<int32_t>(0);
            D.YAngles = Reader.ReadVector3();
            Reader.Assert<int32_t>(0);
            D.ZAngles = Reader.ReadVector3();
            Reader.Assert<int32_t>(0);
            D.Position     = Reader.ReadVector3();
            D.Unk3C        = Reader.ReadFloat();
            D.DecalParamID = Reader.ReadInt32();
            D.Size1        = Reader.ReadInt16();
            D.Size2        = Reader.ReadInt16();
            Reader.AssertPattern(24, 0);
            Reader.StepOut();
            Decals.push_back(D);
        }
    }

    void PMDCL::WriteImpl(BinaryWriter& Writer) {
        Writer.Order = Endian::Little;
        Writer.WriteInt64(static_cast<int64_t>(Decals.size()));
        Writer.WriteInt64(0x20);
        Writer.WriteInt64(0);
        Writer.WriteInt64(0);
        for (size_t I = 0; I < Decals.size(); ++I) {
            Writer.Reserve<int64_t>("Decal" + std::to_string(I));
        }
        Writer.Align(0x20);
        for (size_t I = 0; I < Decals.size(); ++I) {
            const Decal& D = Decals[I];
            Writer.Fill<int64_t>("Decal" + std::to_string(I), Writer.Position());
            Writer.WriteVector3(D.XAngles);
            Writer.WriteInt32(0);
            Writer.WriteVector3(D.YAngles);
            Writer.WriteInt32(0);
            Writer.WriteVector3(D.ZAngles);
            Writer.WriteInt32(0);
            Writer.WriteVector3(D.Position);
            Writer.WriteFloat(D.Unk3C);
            Writer.WriteInt32(D.DecalParamID);
            Writer.WriteInt16(D.Size1);
            Writer.WriteInt16(D.Size2);
            Writer.Pad(24);
        }
    }
}  // namespace Souls
