//
// Created by Jake Rieger on 10/8/2026.
//

#include "FMB.hpp"

namespace Souls {
    namespace {
        enum class Kind { None, String, Double, Double2 };

        Kind KindOf(int32_t Type) {
            switch (Type) {
                case 2: case 5: case 6: case 12: case 14: case 21: case 31: case 32: case 33: case 34: case 43: return Kind::None;
                case 7: case 11: return Kind::String;
                case 1: case 3: case 4: case 8: case 51: case 61: return Kind::Double;
                case 52: return Kind::Double2;
                default: throw BinaryException("Unknown FMB entry type: " + std::to_string(Type));
            }
        }
    }  // namespace

    bool FMB::IsImpl(BinaryReader& Reader) {
        if (Reader.Length() < 4) {
            return false;
        }
        return Reader.GetASCII(0, 4) == "FMB ";
    }

    void FMB::ReadImpl(BinaryReader& Reader) {
        Reader.Order = Endian::Little;
        Reader.AssertMagic("FMB ");
        Reader.Assert<int32_t>(1);
        Reader.Assert<int32_t>(1);
        Reader.Assert<int32_t>(0);
        Reader.Assert<int64_t>(0x20);
        Reader.Assert<int32_t>(0);
        Reader.Assert<int32_t>(0);
        Unk20 = Reader.ReadInt32();
        Reader.Assert<int32_t>(0);
        Reader.Assert<int64_t>(0x30);
        Reader.Assert<int32_t>(0);
        Reader.Assert<int32_t>(0);
        Reader.Assert<int64_t>(0x40);
        const int32_t EntryCount = Reader.ReadInt32();
        Reader.Assert<int32_t>(0);
        Reader.Assert<int64_t>(0x10);
        const std::vector<int64_t> Offsets = Reader.ReadArray<int64_t>(static_cast<size_t>(EntryCount));

        Entries.clear();
        Entries.reserve(Offsets.size());
        for (const int64_t Offset : Offsets) {
            Reader.Seek(0x40 + Offset);
            Entry E;
            E.Type = Reader.ReadInt32();
            Reader.Assert<int32_t>(0);
            switch (KindOf(E.Type)) {
                case Kind::None:
                    Reader.AssertPattern(24, 0);
                    break;
                case Kind::String:
                    E.Value = Reader.GetCString(0x40 + Reader.ReadInt64());
                    Reader.AssertPattern(16, 0);
                    break;
                case Kind::Double:
                    E.Value = Reader.ReadDouble();
                    Reader.AssertPattern(16, 0);
                    break;
                case Kind::Double2: {
                    std::array<double, 2> Values;
                    Values[0] = Reader.ReadDouble();
                    Values[1] = Reader.ReadDouble();
                    E.Value   = Values;
                    Reader.AssertPattern(8, 0);
                    break;
                }
            }
            Entries.push_back(std::move(E));
        }
    }

    void FMB::WriteImpl(BinaryWriter& Writer) {
        Writer.Order = Endian::Little;
        Writer.WriteMagic("FMB ");
        Writer.WriteInt32(1);
        Writer.WriteInt32(1);
        Writer.WriteInt32(0);
        Writer.WriteInt64(0x20);
        Writer.WriteInt32(0);
        Writer.WriteInt32(0);
        Writer.WriteInt32(Unk20);
        Writer.WriteInt32(0);
        Writer.WriteInt64(0x30);
        Writer.WriteInt32(0);
        Writer.WriteInt32(0);
        Writer.WriteInt64(0x40);
        Writer.WriteInt32(static_cast<int32_t>(Entries.size()));
        Writer.WriteInt32(0);
        Writer.WriteInt64(0x10);
        for (size_t I = 0; I < Entries.size(); ++I) {
            Writer.Reserve<int64_t>("EntryOffset[" + std::to_string(I) + "]");
        }
        Writer.Align(0x10);

        for (size_t I = 0; I < Entries.size(); ++I) {
            const Entry& E          = Entries[I];
            const std::string Index = "[" + std::to_string(I) + "]";
            Writer.Fill<int64_t>("EntryOffset" + Index, Writer.Position() - 0x40);
            Writer.WriteInt32(E.Type);
            Writer.WriteInt32(0);
            if (std::holds_alternative<std::string>(E.Value)) {
                Writer.Reserve<int64_t>("ValueOffset" + Index);
                Writer.Pad(16);
            } else if (std::holds_alternative<double>(E.Value)) {
                Writer.WriteDouble(std::get<double>(E.Value));
                Writer.Pad(16);
            } else if (std::holds_alternative<std::array<double, 2>>(E.Value)) {
                const auto& Values = std::get<std::array<double, 2>>(E.Value);
                Writer.WriteDouble(Values[0]);
                Writer.WriteDouble(Values[1]);
                Writer.Pad(8);
            } else {
                Writer.Pad(24);
            }
        }
        for (size_t I = 0; I < Entries.size(); ++I) {
            if (std::holds_alternative<std::string>(Entries[I].Value)) {
                Writer.Fill<int64_t>("ValueOffset[" + std::to_string(I) + "]", Writer.Position() - 0x40);
                Writer.WriteString(std::get<std::string>(Entries[I].Value), true);
            }
        }
        Writer.Align(0x10);
    }
}  // namespace Souls
