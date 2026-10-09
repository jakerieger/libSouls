//
// Created by Jake Rieger on 10/8/2026.
//

#include "CLM2.hpp"

namespace Souls {
    bool CLM2::IsImpl(BinaryReader& Reader) {
        if (Reader.Length() < 4) {
            return false;
        }
        return Reader.GetASCII(0, 4) == "CLM2";
    }

    void CLM2::ReadImpl(BinaryReader& Reader) {
        Reader.Order = Endian::Little;
        Reader.AssertMagic("CLM2");
        Reader.Assert<int32_t>(0);
        Reader.Assert<int16_t>(1);
        Reader.Assert<int16_t>(1);
        Reader.Assert<int32_t>(0);
        Reader.Assert<int32_t>(0);
        const int32_t MeshCount = Reader.ReadInt32();
        Reader.Assert<int32_t>(0x28);
        Reader.Assert<int32_t>(0);
        Reader.Assert<int32_t>(0);
        Reader.Assert<int32_t>(0x28);

        Meshes.clear();
        Meshes.reserve(static_cast<size_t>(MeshCount));
        for (int32_t I = 0; I < MeshCount; ++I) {
            Mesh M;
            Reader.Assert<int32_t>(0);
            const int32_t EntryCount     = Reader.ReadInt32();
            const uint32_t EntriesOffset = Reader.ReadUInt32();
            Reader.Assert<int32_t>(0);
            Reader.StepIn(EntriesOffset);
            for (int32_t E = 0; E < EntryCount; ++E) {
                Entry Ent;
                Ent.Unk00 = Reader.ReadInt16();
                Ent.Unk02 = Reader.ReadInt16();
                M.Entries.push_back(Ent);
            }
            Reader.StepOut();
            Meshes.push_back(std::move(M));
        }
    }

    void CLM2::WriteImpl(BinaryWriter& Writer) {
        Writer.Order = Endian::Little;
        Writer.WriteMagic("CLM2");
        Writer.WriteInt32(0);
        Writer.WriteInt16(1);
        Writer.WriteInt16(1);
        Writer.WriteInt32(0);
        Writer.WriteInt32(0);
        Writer.WriteInt32(static_cast<int32_t>(Meshes.size()));
        Writer.WriteInt32(0x28);
        Writer.WriteInt32(0);
        Writer.WriteInt32(0);
        Writer.WriteInt32(0x28);

        for (size_t I = 0; I < Meshes.size(); ++I) {
            Writer.WriteInt32(0);
            Writer.WriteInt32(static_cast<int32_t>(Meshes[I].Entries.size()));
            Writer.Reserve<uint32_t>("EntriesOffset" + std::to_string(I));
            Writer.WriteInt32(0);
        }
        for (size_t I = 0; I < Meshes.size(); ++I) {
            const std::string Name = "EntriesOffset" + std::to_string(I);
            if (Meshes[I].Entries.empty()) {
                Writer.Fill<uint32_t>(Name, 0);
            } else {
                Writer.Fill<uint32_t>(Name, static_cast<uint32_t>(Writer.Position()));
                for (const Entry& E : Meshes[I].Entries) {
                    Writer.WriteInt16(E.Unk00);
                    Writer.WriteInt16(E.Unk02);
                }
                Writer.Align(8);
            }
        }
    }
}  // namespace Souls
