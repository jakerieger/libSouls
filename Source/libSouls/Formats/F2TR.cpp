//
// Created by Jake Rieger on 10/8/2026.
//

#include "F2TR.hpp"

namespace Souls {
    bool F2TR::IsImpl(BinaryReader& Reader) {
        if (Reader.Length() < 4) {
            return false;
        }
        return Reader.GetASCII(0, 4) == "F2TR";
    }

    void F2TR::ReadImpl(BinaryReader& Reader) {
        Reader.AssertMagic("F2TR");
        BigEndian    = Reader.Assert<uint8_t>(0, 0xFF) == 0xFF;
        Reader.Order = BigEndian ? Endian::Big : Endian::Little;
        Reader.Assert<uint8_t>(0);
        Reader.Assert<int16_t>(1);
        Reader.Assert<int16_t>(0);
        Reader.Assert<int16_t>(0x10);  // header size?
        const int16_t EntryCount = Reader.ReadInt16();
        Reader.Assert<int16_t>(0xC);  // entry size?

        Entries.clear();
        for (int16_t I = 0; I < EntryCount; ++I) {
            Entry E;
            const int32_t NameOffset    = Reader.ReadInt32();
            const int32_t IndicesOffset = Reader.ReadInt32();
            const int16_t IndexCount    = Reader.ReadInt16();
            Reader.Assert<int16_t>(0);
            E.Name = Reader.GetUTF16Text(NameOffset);
            Reader.StepIn(IndicesOffset);
            E.Indices = Reader.ReadArray<int16_t>(static_cast<size_t>(IndexCount));
            Reader.StepOut();
            Entries.push_back(std::move(E));
        }
    }

    void F2TR::WriteImpl(BinaryWriter& Writer) {
        Writer.Order = BigEndian ? Endian::Big : Endian::Little;
        Writer.WriteMagic("F2TR");
        Writer.WriteByte(BigEndian ? 0xFF : 0);
        Writer.WriteByte(0);
        Writer.WriteInt16(1);
        Writer.WriteInt16(0);
        Writer.WriteInt16(0x10);
        Writer.WriteInt16(static_cast<int16_t>(Entries.size()));
        Writer.WriteInt16(0xC);

        for (size_t I = 0; I < Entries.size(); ++I) {
            Writer.Reserve<int32_t>("NameOffset" + std::to_string(I));
            Writer.Reserve<int32_t>("IndicesOffset" + std::to_string(I));
            Writer.WriteInt16(static_cast<int16_t>(Entries[I].Indices.size()));
            Writer.WriteInt16(0);
        }
        for (size_t I = 0; I < Entries.size(); ++I) {
            Writer.Fill<int32_t>("IndicesOffset" + std::to_string(I), static_cast<int32_t>(Writer.Position()));
            Writer.WriteArray(Entries[I].Indices);
        }
        for (size_t I = 0; I < Entries.size(); ++I) {
            Writer.Fill<int32_t>("NameOffset" + std::to_string(I), static_cast<int32_t>(Writer.Position()));
            Writer.WriteUTF16Text(Entries[I].Name, true);
        }
    }
}  // namespace Souls
