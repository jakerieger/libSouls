//
// Created by Jake Rieger on 10/7/2026.
//

#include "PARAM.hpp"

#include <libSouls/TextEncoding.hpp>

#include <algorithm>
#include <map>

namespace Souls {
    namespace {
        bool Has(PARAM::FormatFlags1 Value, PARAM::FormatFlags1 Flag) {
            return (Value & Flag) != PARAM::FormatFlags1::None;
        }

        bool Has(PARAM::FormatFlags2 Value, PARAM::FormatFlags2 Flag) {
            return (Value & Flag) != PARAM::FormatFlags2::None;
        }

        // The expanded headers store the data start as a 32-bit or 64-bit value after the fixed part.
        bool HasIntDataStart(PARAM::FormatFlags1 Format) {
            return Has(Format, PARAM::FormatFlags1::Flag01) && Has(Format, PARAM::FormatFlags1::IntDataOffset);
        }

        bool HasLongDataStart(PARAM::FormatFlags1 Format) {
            return Has(Format, PARAM::FormatFlags1::LongDataOffset);
        }

        std::string Key(const char* Field, size_t Index) {
            return Field + std::to_string(Index);
        }
    }  // namespace

    PARAM::Row* PARAM::Find(int32_t ID) {
        const auto It = std::find_if(Rows.begin(), Rows.end(), [ID](const Row& R) { return R.ID == ID; });
        return It == Rows.end() ? nullptr : &*It;
    }

    const PARAM::Row* PARAM::Find(int32_t ID) const {
        const auto It = std::find_if(Rows.begin(), Rows.end(), [ID](const Row& R) { return R.ID == ID; });
        return It == Rows.end() ? nullptr : &*It;
    }

    bool PARAM::Matches(const PARAMDEF& Def) const {
        return ParamType == Def.ParamType && ParamdefDataVersion == Def.DataVersion &&
               (DetectedSize == -1 || DetectedSize == Def.GetRowSize());
    }

    const PARAMDEF* PARAM::FindMatchingDef(std::span<const PARAMDEF> Defs) const {
        for (const PARAMDEF& Def : Defs) {
            if (Matches(Def)) {
                return &Def;
            }
        }
        return nullptr;
    }

    std::string PARAM::ToString() const {
        return ParamType + " v" + std::to_string(ParamdefDataVersion) + " [" + std::to_string(Rows.size()) + "]";
    }

    bool PARAM::IsImpl(BinaryReader& Reader) {
        const int64_t Length = Reader.Length();
        if (Length < 0x30) {
            return false;
        }

        const uint8_t Big = Reader.ReadAt<uint8_t>(0x2C);
        if (Big != 0 && Big != 0xFF) {
            return false;
        }
        Reader.Order = Big == 0xFF ? Endian::Big : Endian::Little;

        const auto Format1 = static_cast<FormatFlags1>(Reader.ReadAt<uint8_t>(0x2D));
        const bool Expanded = HasIntDataStart(Format1) || HasLongDataStart(Format1);
        const int64_t HeaderSize = Expanded ? 0x40 : 0x30;
        const int64_t RowHeaderSize = HasLongDataStart(Format1) ? 0x18 : 0xC;

        // The strings offset in the header is too unreliable to check; only the row table has to fit.
        const int64_t RowCount = Reader.ReadAt<uint16_t>(0x0A);
        return HeaderSize + RowCount * RowHeaderSize <= Length;
    }

    void PARAM::ReadImpl(BinaryReader& Reader) {
        Reader.Seek(0x2C);
        BigEndian = Reader.Assert<uint8_t>(0, 0xFF) == 0xFF;
        Reader.Order          = BigEndian ? Endian::Big : Endian::Little;
        Format2D              = static_cast<FormatFlags1>(Reader.ReadByte());
        Format2E              = static_cast<FormatFlags2>(Reader.ReadByte());
        ParamdefFormatVersion = Reader.ReadByte();
        Reader.Seek(0);

        // The strings offset in the header is highly unreliable; only use it as a last resort.
        int64_t ActualStringsOffset = 0;
        const int64_t StringsOffset = Reader.ReadUInt32();
        if (HasIntDataStart(Format2D) || HasLongDataStart(Format2D)) {
            Reader.Assert<int16_t>(0);
        } else {
            Reader.ReadUInt16();  // data start
        }
        Unk06                       = Reader.ReadInt16();
        ParamdefDataVersion         = Reader.ReadInt16();
        const uint16_t RowCount     = Reader.ReadUInt16();

        ParamType.clear();
        if (Has(Format2D, FormatFlags1::OffsetParamType)) {
            Reader.Assert<int32_t>(0);
            const int64_t ParamTypeOffset = Reader.ReadInt64();
            Reader.AssertPattern(0x14, 0);

            if (ParamTypeOffset > 0 && ParamTypeOffset < Reader.Length()) {
                Reader.StepIn(ParamTypeOffset);
                ParamType = Reader.ReadCString();
                Reader.StepOut();
                ActualStringsOffset = ParamTypeOffset;
            }
        } else {
            ParamType = Reader.ReadFixStr(0x20);
        }
        Reader.Skip(4);  // format
        if (HasIntDataStart(Format2D)) {
            Reader.ReadInt32();  // data start
            Reader.Assert<int32_t>(0);
            Reader.Assert<int32_t>(0);
            Reader.Assert<int32_t>(0);
        } else if (HasLongDataStart(Format2D)) {
            Reader.ReadInt64();  // data start
            Reader.Assert<int64_t>(0);
        }

        const bool LongRows = HasLongDataStart(Format2D);
        const int64_t RowHeaderSize = LongRows ? 0x18 : 0xC;
        if (static_cast<int64_t>(RowCount) * RowHeaderSize > Reader.Remaining()) {
            throw BinaryException("PARAM row count " + std::to_string(RowCount) + " does not fit in the file");
        }

        struct RowHeader {
            int32_t ID;
            int64_t DataOffset;
        };
        std::vector<RowHeader> Headers;
        Headers.reserve(RowCount);
        Rows.clear();
        Rows.reserve(RowCount);
        for (uint16_t I = 0; I < RowCount; ++I) {
            RowHeader Header{};
            int64_t NameOffset;
            if (LongRows) {
                Header.ID = Reader.ReadInt32();
                Reader.ReadInt32();  // not always zero: some DS2 SotFS params have garbage here
                Header.DataOffset = Reader.ReadInt64();
                NameOffset        = Reader.ReadInt64();
            } else {
                Header.ID         = Reader.ReadInt32();
                Header.DataOffset = Reader.ReadUInt32();
                NameOffset        = Reader.ReadUInt32();
            }

            Row Result;
            Result.ID = Header.ID;
            if (NameOffset != 0) {
                if (ActualStringsOffset == 0 || NameOffset < ActualStringsOffset) {
                    ActualStringsOffset = NameOffset;
                }

                Reader.StepIn(NameOffset);
                if (Has(Format2E, FormatFlags2::UnicodeRowNames)) {
                    Result.Name = Text::UTF16ToUTF8(Reader.ReadUTF16());
                } else {
                    Result.Name = Reader.ReadShiftJIS();
                }
                Reader.StepOut();
            }
            Headers.push_back(Header);
            Rows.push_back(std::move(Result));
        }

        if (Rows.size() > 1) {
            DetectedSize = Headers[1].DataOffset - Headers[0].DataOffset;
        } else if (Rows.size() == 1) {
            DetectedSize = (ActualStringsOffset == 0 ? StringsOffset : ActualStringsOffset) - Headers[0].DataOffset;
        } else {
            DetectedSize = -1;
        }

        // Every row's data is DetectedSize bytes at its offset.
        if (!Rows.empty() && (DetectedSize < 0 || DetectedSize > Reader.Length())) {
            throw BinaryException("Could not determine the PARAM row size (got " + std::to_string(DetectedSize) + ")");
        }
        for (size_t I = 0; I < Rows.size(); ++I) {
            if (Headers[I].DataOffset == 0) {
                continue;  // a row with no data
            }
            if (Headers[I].DataOffset < 0 || Headers[I].DataOffset + DetectedSize > Reader.Length()) {
                throw BinaryException("PARAM row " + std::to_string(Rows[I].ID) + " data lies outside the file");
            }
            Reader.StepIn(Headers[I].DataOffset);
            Rows[I].Bytes = Reader.ReadBytes(static_cast<size_t>(DetectedSize));
            Reader.StepOut();
        }
    }

    void PARAM::WriteImpl(BinaryWriter& Writer) {
        // All rows share one size, taken from the data itself.
        const size_t RowSize = Rows.empty() ? 0 : Rows.front().Bytes.size();
        for (const Row& R : Rows) {
            if (R.Bytes.size() != RowSize) {
                throw BinaryException("PARAM rows must all have the same data size (row " + std::to_string(R.ID) +
                                      " has " + std::to_string(R.Bytes.size()) + ", expected " +
                                      std::to_string(RowSize) + ")");
            }
        }
        if (Rows.size() > 0xFFFF) {
            throw BinaryException("A PARAM can hold at most 65535 rows");
        }

        const bool LongRows = HasLongDataStart(Format2D);
        Writer.Order        = BigEndian ? Endian::Big : Endian::Little;

        Writer.Reserve<uint32_t>("StringsOffset");
        if (HasIntDataStart(Format2D) || HasLongDataStart(Format2D)) {
            Writer.WriteInt16(0);
        } else {
            Writer.Reserve<uint16_t>("DataStart");
        }
        Writer.WriteInt16(Unk06);
        Writer.WriteInt16(ParamdefDataVersion);
        Writer.WriteUInt16(static_cast<uint16_t>(Rows.size()));
        if (Has(Format2D, FormatFlags1::OffsetParamType)) {
            Writer.WriteInt32(0);
            Writer.Reserve<int64_t>("ParamTypeOffset");
            Writer.Pad(0x14);
        } else {
            // Older files pad the type name with spaces (observed in Dark Souls; the exact rule isn't known).
            Writer.WriteFixStr(ParamType, 0x20, 0x20);
        }
        Writer.WriteByte(BigEndian ? 0xFF : 0x00);
        Writer.WriteByte(static_cast<uint8_t>(Format2D));
        Writer.WriteByte(static_cast<uint8_t>(Format2E));
        Writer.WriteByte(ParamdefFormatVersion);
        if (HasIntDataStart(Format2D)) {
            Writer.Reserve<uint32_t>("DataStart");
            Writer.WriteInt32(0);
            Writer.WriteInt32(0);
            Writer.WriteInt32(0);
        } else if (HasLongDataStart(Format2D)) {
            Writer.Reserve<int64_t>("DataStart");
            Writer.WriteInt64(0);
        }

        for (size_t I = 0; I < Rows.size(); ++I) {
            Writer.WriteInt32(Rows[I].ID);
            if (LongRows) {
                Writer.WriteInt32(0);
                Writer.Reserve<int64_t>(Key("RowOffset", I));
                Writer.Reserve<int64_t>(Key("NameOffset", I));
            } else {
                Writer.Reserve<uint32_t>(Key("RowOffset", I));
                Writer.Reserve<uint32_t>(Key("NameOffset", I));
            }
        }

        // This is probably pretty stupid, but it's what the original files do.
        if (Format2D == FormatFlags1::Flag01) {
            Writer.Pad(0x20);
        }

        if (HasIntDataStart(Format2D)) {
            Writer.Fill<uint32_t>("DataStart", static_cast<uint32_t>(Writer.Position()));
        } else if (HasLongDataStart(Format2D)) {
            Writer.Fill<int64_t>("DataStart", Writer.Position());
        } else {
            Writer.Fill<uint16_t>("DataStart", static_cast<uint16_t>(Writer.Position()));
        }

        for (size_t I = 0; I < Rows.size(); ++I) {
            if (LongRows) {
                Writer.Fill<int64_t>(Key("RowOffset", I), Writer.Position());
            } else {
                Writer.Fill<uint32_t>(Key("RowOffset", I), static_cast<uint32_t>(Writer.Position()));
            }
            Writer.WriteBytes(Rows[I].Bytes);
        }

        const bool NewLayout = Has(Format2D, FormatFlags1::OffsetParamType);
        // In the newer layout the header's strings offset is where the type string starts; in older files it's the end
        // of the file (filled in last). It's unreliable either way and the reader doesn't depend on it.
        if (NewLayout) {
            Writer.Fill<uint32_t>("StringsOffset", static_cast<uint32_t>(Writer.Position()));
        }

        if (Has(Format2D, FormatFlags1::OffsetParamType)) {
            Writer.Fill<int64_t>("ParamTypeOffset", Writer.Position());
            Writer.WriteString(ParamType, true);
        }

        // In the newer layout identical names share one copy in the string pool, as in the game's files.
        std::map<std::string, int64_t> NameOffsets;
        for (size_t I = 0; I < Rows.size(); ++I) {
            int64_t NameOffset = 0;
            if (Rows[I].Name) {
                const auto Existing = NewLayout ? NameOffsets.find(*Rows[I].Name) : NameOffsets.end();
                if (Existing != NameOffsets.end()) {
                    NameOffset = Existing->second;
                } else {
                    NameOffset = Writer.Position();
                    NameOffsets.emplace(*Rows[I].Name, NameOffset);
                    if (Has(Format2E, FormatFlags2::UnicodeRowNames)) {
                        Writer.WriteUTF16(Text::UTF8ToUTF16(*Rows[I].Name), true);
                    } else {
                        Writer.WriteShiftJIS(*Rows[I].Name, true);
                    }
                }
            }

            if (LongRows) {
                Writer.Fill<int64_t>(Key("NameOffset", I), NameOffset);
            } else {
                Writer.Fill<uint32_t>(Key("NameOffset", I), static_cast<uint32_t>(NameOffset));
            }
        }
        // The game's files end with one more (empty) string terminator after the last name.
        const bool AnyNames = std::any_of(Rows.begin(), Rows.end(), [](const Row& R) { return R.Name.has_value(); });
        if (AnyNames && Has(Format2E, FormatFlags2::UnicodeRowNames)) {
            Writer.Pad(2);
        }

        if (!NewLayout) {
            Writer.Fill<uint32_t>("StringsOffset", static_cast<uint32_t>(Writer.Position()));
        }
    }
}  // namespace Souls
