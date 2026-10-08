//
// Created by Jake Rieger on 10/7/2026.
//

#include "FMG.hpp"

#include <libSouls/TextEncoding.hpp>

#include <algorithm>

namespace Souls {
    FMG::Entry* FMG::Find(int32_t ID) {
        const auto It = std::find_if(Entries.begin(), Entries.end(), [ID](const Entry& E) { return E.ID == ID; });
        return It == Entries.end() ? nullptr : &*It;
    }

    const FMG::Entry* FMG::Find(int32_t ID) const {
        const auto It = std::find_if(Entries.begin(), Entries.end(), [ID](const Entry& E) { return E.ID == ID; });
        return It == Entries.end() ? nullptr : &*It;
    }

    std::optional<std::string> FMG::GetText(int32_t ID) const {
        const Entry* Found = Find(ID);
        return Found ? Found->Text : std::nullopt;
    }

    void FMG::SetText(int32_t ID, std::optional<std::string> Text) {
        if (Entry* Found = Find(ID)) {
            Found->Text = std::move(Text);
        } else {
            Entries.emplace_back(ID, std::move(Text));
        }
    }

    bool FMG::IsImpl(BinaryReader& Reader) {
        constexpr int64_t HeaderSize = 0x14;
        const int64_t Length         = Reader.Length();
        if (Length < HeaderSize) {
            return false;
        }

        const uint8_t Big     = Reader.ReadAt<uint8_t>(1);
        const uint8_t Version = Reader.ReadAt<uint8_t>(2);
        if (Reader.ReadAt<uint8_t>(0) != 0 || Big > 1 || Version > 2 || Reader.ReadAt<uint8_t>(3) != 0) {
            return false;
        }

        // Fixed bytes after the file size.
        const uint8_t DeSMarker = Version == static_cast<uint8_t>(FMGVersion::DemonsSouls) ? 0xFF : 0x00;
        if (Reader.ReadAt<uint8_t>(8) != 1 || Reader.ReadAt<uint8_t>(9) != DeSMarker || Reader.ReadAt<uint8_t>(10) != 0 ||
            Reader.ReadAt<uint8_t>(11) != 0) {
            return false;
        }

        // The recorded file size can't exceed the data we have.
        Reader.Order           = Big ? Endian::Big : Endian::Little;
        const int64_t FileSize = Reader.ReadAt<uint32_t>(4);
        return FileSize >= HeaderSize && FileSize <= Length;
    }

    void FMG::ReadImpl(BinaryReader& Reader) {
        Reader.AssertPattern(1, 0);
        BigEndian = Reader.ReadBool();

        const uint8_t RawVersion = Reader.ReadByte();
        if (RawVersion > static_cast<uint8_t>(FMGVersion::DarkSouls3)) {
            throw BinaryException("Unknown FMG version " + std::to_string(RawVersion));
        }
        Version = static_cast<FMGVersion>(RawVersion);
        Reader.AssertPattern(1, 0);

        Reader.Order    = BigEndian ? Endian::Big : Endian::Little;
        const bool Wide = Version == FMGVersion::DarkSouls3;

        Reader.ReadInt32();  // file size
        Reader.Assert<uint8_t>(1);
        Reader.Assert<uint8_t>(Version == FMGVersion::DemonsSouls ? 0xFF : 0x00);
        Reader.AssertPattern(2, 0);
        const int32_t GroupCount  = Reader.ReadInt32();
        const int32_t StringCount = Reader.ReadInt32();

        if (Wide) {
            Reader.Assert<int32_t>(0xFF);
        }

        const int64_t StringOffsetsOffset = Wide ? Reader.ReadInt64() : static_cast<int64_t>(Reader.ReadInt32());

        if (Wide) {
            Reader.Assert<int64_t>(0);
        } else {
            Reader.Assert<int32_t>(0);
        }

        const int64_t GroupSize = Wide ? 16 : 12;
        const int64_t Stride    = Wide ? 8 : 4;
        if (GroupCount < 0 || StringCount < 0 || static_cast<int64_t>(GroupCount) * GroupSize > Reader.Remaining()) {
            throw BinaryException("Invalid FMG group count " + std::to_string(GroupCount));
        }

        Entries.clear();
        Entries.reserve(static_cast<size_t>(std::min(GroupCount, StringCount)));
        for (int32_t I = 0; I < GroupCount; ++I) {
            const int32_t OffsetIndex = Reader.ReadInt32();
            const int32_t FirstID     = Reader.ReadInt32();
            const int32_t LastID      = Reader.ReadInt32();

            if (Wide) {
                Reader.Assert<int32_t>(0);
            }

            const int64_t Count = static_cast<int64_t>(LastID) - FirstID + 1;
            if (Count < 0 || Count * Stride > Reader.Length()) {
                throw BinaryException("Invalid FMG group range " + std::to_string(FirstID) + ".." + std::to_string(LastID));
            }

            Reader.StepIn(StringOffsetsOffset + static_cast<int64_t>(OffsetIndex) * Stride);
            for (int64_t J = 0; J < Count; ++J) {
                const int64_t StringOffset = Wide ? Reader.ReadInt64() : static_cast<int64_t>(Reader.ReadInt32());

                Entry Result;
                Result.ID = static_cast<int32_t>(FirstID + J);
                if (StringOffset != 0) {
                    Reader.StepIn(StringOffset);
                    Result.Text = Text::UTF16ToUTF8(Reader.ReadUTF16());
                    Reader.StepOut();
                }
                Entries.push_back(std::move(Result));
            }
            Reader.StepOut();
        }
    }

    void FMG::WriteImpl(BinaryWriter& Writer) {
        Writer.Order    = BigEndian ? Endian::Big : Endian::Little;
        const bool Wide = Version == FMGVersion::DarkSouls3;

        // Groups are runs of consecutive IDs, so entries must be in ID order.
        std::stable_sort(Entries.begin(), Entries.end(), [](const Entry& A, const Entry& B) { return A.ID < B.ID; });

        Writer.WriteByte(0);
        Writer.WriteBool(BigEndian);
        Writer.WriteByte(static_cast<uint8_t>(Version));
        Writer.WriteByte(0);

        Writer.Reserve<int32_t>("FileSize");
        Writer.WriteByte(1);
        Writer.WriteByte(Version == FMGVersion::DemonsSouls ? 0xFF : 0x00);
        Writer.Pad(2);
        Writer.Reserve<int32_t>("GroupCount");
        Writer.WriteInt32(static_cast<int32_t>(Entries.size()));

        if (Wide) {
            Writer.WriteInt32(0xFF);
            Writer.Reserve<int64_t>("StringOffsets");
            Writer.WriteInt64(0);
        } else {
            Writer.Reserve<int32_t>("StringOffsets");
            Writer.WriteInt32(0);
        }

        int32_t GroupCount = 0;
        for (size_t I = 0; I < Entries.size(); ++I) {
            Writer.WriteInt32(static_cast<int32_t>(I));
            Writer.WriteInt32(Entries[I].ID);
            while (I + 1 < Entries.size() && static_cast<int64_t>(Entries[I + 1].ID) == static_cast<int64_t>(Entries[I].ID) + 1) {
                ++I;
            }
            Writer.WriteInt32(Entries[I].ID);

            if (Wide) {
                Writer.WriteInt32(0);
            }
            ++GroupCount;
        }
        Writer.Fill<int32_t>("GroupCount", GroupCount);

        if (Wide) {
            Writer.Fill<int64_t>("StringOffsets", Writer.Position());
        } else {
            Writer.Fill<int32_t>("StringOffsets", static_cast<int32_t>(Writer.Position()));
        }

        for (size_t I = 0; I < Entries.size(); ++I) {
            if (Wide) {
                Writer.Reserve<int64_t>("StringOffset" + std::to_string(I));
            } else {
                Writer.Reserve<int32_t>("StringOffset" + std::to_string(I));
            }
        }

        for (size_t I = 0; I < Entries.size(); ++I) {
            const std::optional<std::string>& Value = Entries[I].Text;
            const int64_t Offset                    = Value ? Writer.Position() : 0;

            if (Wide) {
                Writer.Fill<int64_t>("StringOffset" + std::to_string(I), Offset);
            } else {
                Writer.Fill<int32_t>("StringOffset" + std::to_string(I), static_cast<int32_t>(Offset));
            }

            if (Value) {
                Writer.WriteUTF16(Text::UTF8ToUTF16(*Value), true);
            }
        }

        Writer.Fill<int32_t>("FileSize", static_cast<int32_t>(Writer.Position()));
    }
}  // namespace Souls
