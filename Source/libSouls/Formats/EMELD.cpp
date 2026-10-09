//
// Created by Jake Rieger on 10/8/2026.
//

#include "EMELD.hpp"

namespace Souls {
    bool EMELD::IsImpl(BinaryReader& Reader) {
        if (Reader.Length() < 4) {
            return false;
        }
        return Reader.GetASCII(0, 4) == std::string("ELD\0", 4);
    }

    void EMELD::ReadImpl(BinaryReader& Reader) {
        Reader.AssertMagic(std::string_view("ELD\0", 4));
        const bool BigEndian = Reader.ReadBool();
        const bool Is64Bit   = Reader.Assert<int8_t>(0, -1) == -1;
        Reader.AssertPattern(2, 0);
        Reader.Order      = BigEndian ? Endian::Big : Endian::Little;
        Reader.VarintLong = Is64Bit;
        Reader.Assert<int16_t>(0x65);
        Reader.Assert<int16_t>(0xCC);
        Reader.ReadInt32();  // file size

        if (!BigEndian && !Is64Bit) {
            Format = EMEVD::Game::DarkSouls1;
        } else if (BigEndian && !Is64Bit) {
            Format = EMEVD::Game::DarkSouls1BE;
        } else if (!BigEndian && Is64Bit) {
            Format = EMEVD::Game::Bloodborne;
        } else {
            throw BinaryException("Unknown EMELD format");
        }

        const int64_t EventCount   = Reader.ReadVarint();
        const int64_t EventsOffset = Reader.ReadVarint();
        Reader.AssertVarint(0);  // unused count 2
        Reader.ReadVarint();     // unused offset 2
        Reader.AssertVarint(0);  // unused count 3
        Reader.ReadVarint();     // unused offset 3
        Reader.ReadVarint();     // strings length
        const int64_t StringsOffset = Reader.ReadVarint();
        if (!Is64Bit) {
            Reader.Assert<int32_t>(0);
            Reader.Assert<int32_t>(0);
        }

        Reader.Seek(EventsOffset);
        Events.clear();
        Events.reserve(static_cast<size_t>(EventCount));
        for (int64_t I = 0; I < EventCount; ++I) {
            Event E;
            E.ID                      = Reader.ReadVarint();
            const int64_t NameOffset  = Reader.ReadVarint();
            if (Format < EMEVD::Game::Bloodborne) {
                Reader.Assert<int32_t>(0);
            }
            E.Name = Reader.GetUTF16Text(StringsOffset + NameOffset);
            Events.push_back(std::move(E));
        }
    }

    void EMELD::WriteImpl(BinaryWriter& Writer) {
        const bool BigEndian = Format == EMEVD::Game::DarkSouls1BE;
        const bool Is64Bit   = Format >= EMEVD::Game::Bloodborne;

        Writer.WriteMagic(std::string_view("ELD\0", 4));
        Writer.WriteBool(BigEndian);
        Writer.WriteSByte(static_cast<int8_t>(Is64Bit ? -1 : 0));
        Writer.WriteByte(0);
        Writer.WriteByte(0);
        Writer.Order      = BigEndian ? Endian::Big : Endian::Little;
        Writer.VarintLong = Is64Bit;
        Writer.WriteInt16(0x65);
        Writer.WriteInt16(0xCC);
        Writer.Reserve<int32_t>("FileSize");
        Writer.WriteVarint(static_cast<int64_t>(Events.size()));
        Writer.ReserveVarint("EventsOffset");
        Writer.WriteVarint(0);
        Writer.ReserveVarint("Offset2");
        Writer.WriteVarint(0);
        Writer.ReserveVarint("Offset3");
        Writer.ReserveVarint("StringsLength");
        Writer.ReserveVarint("StringsOffset");
        if (!Is64Bit) {
            Writer.WriteInt32(0);
            Writer.WriteInt32(0);
        }

        Writer.FillVarint("EventsOffset", Writer.Position());
        for (size_t I = 0; I < Events.size(); ++I) {
            Writer.WriteVarint(Events[I].ID);
            Writer.ReserveVarint("Event" + std::to_string(I) + "NameOffset");
            if (Format < EMEVD::Game::Bloodborne) {
                Writer.WriteInt32(0);
            }
        }

        Writer.FillVarint("Offset2", Writer.Position());
        Writer.FillVarint("Offset3", Writer.Position());
        const int64_t StringsOffset = Writer.Position();
        Writer.FillVarint("StringsOffset", Writer.Position());
        for (size_t I = 0; I < Events.size(); ++I) {
            Writer.FillVarint("Event" + std::to_string(I) + "NameOffset", Writer.Position() - StringsOffset);
            Writer.WriteUTF16Text(Events[I].Name, true);
        }
        if ((Writer.Position() - StringsOffset) % 0x10 > 0) {
            Writer.Pad(static_cast<size_t>(0x10 - (Writer.Position() - StringsOffset) % 0x10));
        }
        Writer.FillVarint("StringsLength", Writer.Position() - StringsOffset);
        Writer.Fill<int32_t>("FileSize", static_cast<int32_t>(Writer.Position()));
    }
}  // namespace Souls
