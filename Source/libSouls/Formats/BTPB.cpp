//
// Created by Jake Rieger on 10/8/2026.
//

#include "BTPB.hpp"

namespace Souls {
    void BTPB::ReadImpl(BinaryReader& Reader) {
        const bool BigEndian = Reader.ReadAt<uint8_t>(0x10) != 0;
        Reader.Order         = BigEndian ? Endian::Big : Endian::Little;

        const int32_t Unk00 = Reader.Assert<int32_t>(2, 3);
        const int32_t Unk04 = Reader.Assert<int32_t>(0, 1);
        const int32_t GroupCount = Reader.ReadInt32();
        const int32_t DataLength = Reader.ReadInt32();
        Reader.Assert<uint8_t>(BigEndian ? 1 : 0);
        Reader.AssertPattern(3, 0);
        const int32_t GroupSize = Reader.Assert<int32_t>(0x40, 0x48, 0x98);
        const int32_t ProbeSize = Reader.Assert<int32_t>(0x1C, 0x48);
        Unk1C = Reader.ReadVector3();
        Unk28 = Reader.ReadVector3();
        Reader.Assert<int64_t>(0);

        if (!BigEndian && Unk00 == 2 && Unk04 == 1 && GroupSize == 0x40 && ProbeSize == 0x1C) {
            Version = BTPBVersion::DarkSouls2LE;
        } else if (BigEndian && Unk00 == 2 && Unk04 == 1 && GroupSize == 0x40 && ProbeSize == 0x1C) {
            Version = BTPBVersion::DarkSouls2BE;
        } else if (!BigEndian && Unk00 == 2 && Unk04 == 1 && GroupSize == 0x48 && ProbeSize == 0x48) {
            Version = BTPBVersion::Bloodborne;
        } else if (!BigEndian && Unk00 == 3 && Unk04 == 0 && GroupSize == 0x98 && ProbeSize == 0x48) {
            Version = BTPBVersion::DarkSouls3;
        } else {
            throw BinaryException("Unknown BTPB format");
        }
        Reader.VarintLong = Version >= BTPBVersion::Bloodborne;

        const int64_t DataStart = Reader.Position();
        Reader.Skip(DataLength);
        Groups.clear();
        Groups.reserve(static_cast<size_t>(GroupCount));
        for (int32_t I = 0; I < GroupCount; ++I) {
            Group G;
            const int64_t NameOffset = Reader.ReadVarint();
            G.Flags08                = Reader.ReadInt32();
            const int32_t ProbeCount = Reader.ReadInt32();
            G.Unk10                  = Reader.ReadInt32();
            G.Unk14                  = Reader.ReadInt32();
            G.Unk18                  = Reader.ReadInt32();
            G.Unk1C                  = Reader.ReadFloat();
            G.Unk20                  = Reader.ReadFloat();
            G.Unk24                  = Reader.ReadFloat();
            G.Unk28                  = Reader.ReadVector3();
            G.Unk34                  = Reader.ReadVector3();
            const int64_t ProbesOffset = Reader.ReadVarint();
            if (Version >= BTPBVersion::DarkSouls3) {
                G.Unk48 = Reader.ReadFloat();
                G.Unk4C = Reader.ReadFloat();
                G.Unk50 = Reader.ReadFloat();
                Reader.AssertPattern(0x40, 0);
                G.Unk94 = Reader.ReadByte();
                G.Unk95 = Reader.ReadByte();
                G.Unk96 = Reader.ReadByte();
                Reader.Assert<uint8_t>(0);
            }
            if ((G.Flags08 & 1) != 0) {
                G.Name = Reader.GetUTF16Text(DataStart + NameOffset);
            }

            Reader.StepIn(DataStart + ProbesOffset);
            for (int32_t P = 0; P < ProbeCount; ++P) {
                Probe Pr;
                Reader.ReadInto(std::span<int16_t>(Pr.Coefficients));
                Pr.LightMask = Reader.ReadInt16();
                Pr.Unk1A     = Reader.ReadInt16();
                if (Version >= BTPBVersion::Bloodborne) {
                    Pr.Position = Reader.ReadVector3();
                    Reader.AssertPattern(0x20, 0);
                }
                G.Probes.push_back(Pr);
            }
            Reader.StepOut();
            Groups.push_back(std::move(G));
        }
    }

    void BTPB::WriteImpl(BinaryWriter& Writer) {
        bool BigEndian;
        int32_t Unk00, Unk04, GroupSize, ProbeSize;
        switch (Version) {
            case BTPBVersion::DarkSouls2LE:
            case BTPBVersion::DarkSouls2BE:
                BigEndian = Version == BTPBVersion::DarkSouls2BE;
                Unk00 = 2; Unk04 = 1; GroupSize = 0x40; ProbeSize = 0x1C;
                break;
            case BTPBVersion::Bloodborne:
                BigEndian = false;
                Unk00 = 2; Unk04 = 1; GroupSize = 0x48; ProbeSize = 0x48;
                break;
            case BTPBVersion::DarkSouls3:
                BigEndian = false;
                Unk00 = 3; Unk04 = 0; GroupSize = 0x98; ProbeSize = 0x48;
                break;
            default: throw BinaryException("Write is not supported for this BTPB version.");
        }

        Writer.Order      = BigEndian ? Endian::Big : Endian::Little;
        Writer.VarintLong = Version >= BTPBVersion::Bloodborne;
        Writer.WriteInt32(Unk00);
        Writer.WriteInt32(Unk04);
        Writer.WriteInt32(static_cast<int32_t>(Groups.size()));
        Writer.Reserve<int32_t>("DataLength");
        Writer.WriteBool(BigEndian);
        Writer.Pad(3);
        Writer.WriteInt32(GroupSize);
        Writer.WriteInt32(ProbeSize);
        Writer.WriteVector3(Unk1C);
        Writer.WriteVector3(Unk28);
        Writer.WriteInt64(0);

        std::vector<int64_t> NameOffsets(Groups.size()), ProbesOffsets(Groups.size());
        const int64_t DataStart = Writer.Position();
        for (size_t I = 0; I < Groups.size(); ++I) {
            const Group& G = Groups[I];
            if ((G.Flags08 & 1) != 0) {
                NameOffsets[I] = Writer.Position() - DataStart;
                Writer.WriteUTF16Text(G.Name, true);
                Writer.AlignRelative(DataStart, 8);
            } else {
                NameOffsets[I] = 0;
            }
            ProbesOffsets[I] = Writer.Position() - DataStart;
            for (const Probe& P : G.Probes) {
                Writer.WriteArray(std::span<const int16_t>(P.Coefficients));
                Writer.WriteInt16(P.LightMask);
                Writer.WriteInt16(P.Unk1A);
                if (Version >= BTPBVersion::Bloodborne) {
                    Writer.WriteVector3(P.Position);
                    Writer.Pad(0x20);
                }
            }
        }
        Writer.Fill<int32_t>("DataLength", static_cast<int32_t>(Writer.Position() - DataStart));

        for (size_t I = 0; I < Groups.size(); ++I) {
            const Group& G = Groups[I];
            Writer.WriteVarint(NameOffsets[I]);
            Writer.WriteInt32(G.Flags08);
            Writer.WriteInt32(static_cast<int32_t>(G.Probes.size()));
            Writer.WriteInt32(G.Unk10);
            Writer.WriteInt32(G.Unk14);
            Writer.WriteInt32(G.Unk18);
            Writer.WriteFloat(G.Unk1C);
            Writer.WriteFloat(G.Unk20);
            Writer.WriteFloat(G.Unk24);
            Writer.WriteVector3(G.Unk28);
            Writer.WriteVector3(G.Unk34);
            Writer.WriteVarint(ProbesOffsets[I]);
            if (Version >= BTPBVersion::DarkSouls3) {
                Writer.WriteFloat(G.Unk48);
                Writer.WriteFloat(G.Unk4C);
                Writer.WriteFloat(G.Unk50);
                Writer.Pad(0x40);
                Writer.WriteByte(G.Unk94);
                Writer.WriteByte(G.Unk95);
                Writer.WriteByte(G.Unk96);
                Writer.WriteByte(0);
            }
        }
    }
}  // namespace Souls
