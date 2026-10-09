//
// Created by Jake Rieger on 10/8/2026.
//

#include "BTAB.hpp"

namespace Souls {
    void BTAB::ReadImpl(BinaryReader& Reader) {
        Reader.Order = Reader.ReadAt<uint8_t>(0x10) != 0 ? Endian::Big : Endian::Little;
        Reader.Assert<int32_t>(1);
        Reader.Assert<int32_t>(0);
        const int32_t EntryCount    = Reader.ReadInt32();
        const int32_t StringsLength = Reader.ReadInt32();
        BigEndian                   = Reader.ReadBool();
        Reader.AssertPattern(3, 0);
        LongFormat        = Reader.Assert<int32_t>(0x1C, 0x28) == 0x28;  // entry size
        Reader.VarintLong = LongFormat;
        Reader.AssertPattern(0x24, 0);

        const int64_t StringsStart = Reader.Position();
        Reader.Skip(StringsLength);
        Entries.clear();
        Entries.reserve(static_cast<size_t>(EntryCount));
        for (int32_t I = 0; I < EntryCount; ++I) {
            Entry E;
            const int64_t PartNameOffset     = Reader.ReadVarint();
            const int64_t MaterialNameOffset = Reader.ReadVarint();
            E.AtlasID                        = Reader.ReadInt32();
            E.UVOffset                       = Reader.ReadVector2();
            E.UVScale                        = Reader.ReadVector2();
            if (Reader.VarintLong) {
                Reader.Assert<int32_t>(0);
            }
            E.PartName     = Reader.GetUTF16Text(StringsStart + PartNameOffset);
            E.MaterialName = Reader.GetUTF16Text(StringsStart + MaterialNameOffset);
            Entries.push_back(std::move(E));
        }
    }

    void BTAB::WriteImpl(BinaryWriter& Writer) {
        Writer.Order      = BigEndian ? Endian::Big : Endian::Little;
        Writer.VarintLong = LongFormat;
        Writer.WriteInt32(1);
        Writer.WriteInt32(0);
        Writer.WriteInt32(static_cast<int32_t>(Entries.size()));
        Writer.Reserve<int32_t>("StringsLength");
        Writer.WriteBool(BigEndian);
        Writer.Pad(3);
        Writer.WriteInt32(LongFormat ? 0x28 : 0x1C);
        Writer.Pad(0x24);

        const int64_t StringsStart = Writer.Position();
        std::vector<int64_t> StringOffsets;
        StringOffsets.reserve(Entries.size() * 2);
        for (const Entry& E : Entries) {
            StringOffsets.push_back(Writer.Position() - StringsStart);
            Writer.WriteUTF16Text(E.PartName, true);
            Writer.AlignRelative(StringsStart, 8);  // This padding is not consistent, but it is the best we can do
            StringOffsets.push_back(Writer.Position() - StringsStart);
            Writer.WriteUTF16Text(E.MaterialName, true);
            Writer.AlignRelative(StringsStart, 8);
        }
        Writer.Fill<int32_t>("StringsLength", static_cast<int32_t>(Writer.Position() - StringsStart));

        for (size_t I = 0; I < Entries.size(); ++I) {
            const Entry& E = Entries[I];
            Writer.WriteVarint(StringOffsets[I * 2]);
            Writer.WriteVarint(StringOffsets[I * 2 + 1]);
            Writer.WriteInt32(E.AtlasID);
            Writer.WriteVector2(E.UVOffset);
            Writer.WriteVector2(E.UVScale);
            if (Writer.VarintLong) {
                Writer.WriteInt32(0);
            }
        }
    }
}  // namespace Souls
