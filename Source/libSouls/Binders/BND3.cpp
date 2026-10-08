//
// Created by Jake Rieger on 10/7/2026.
//

#include "BND3.hpp"
#include "BinderFileHeader.hpp"

#include <limits>
#include <string_view>

using namespace std::string_view_literals;

namespace Souls {
    using namespace Binder;

    BND3::BND3() {
        Version = Binder::CurrentVersion();
        Format  = Format::IDs | Format::Names1 | Format::Names2 | Format::Compression;
    }

    bool BND3::IsImpl(BinaryReader& Reader) {
        if (Reader.Length() < 4) {
            return false;
        }
        return Reader.ReadAt<uint8_t>(0) == 'B' && Reader.ReadAt<uint8_t>(1) == 'N' &&
               Reader.ReadAt<uint8_t>(2) == 'D' && Reader.ReadAt<uint8_t>(3) == '3';
    }

    void BND3::ReadImpl(BinaryReader& Reader) {
        Reader.AssertMagic("BND3"sv);
        Version = Reader.ReadFixStr(8);

        // The bit order of the format byte depends on a flag that comes after it.
        BitBigEndian = Reader.ReadAt<uint8_t>(0xE) != 0;

        Format    = Binder::ReadFormat(Reader, BitBigEndian);
        BigEndian = Reader.ReadBool();
        Reader.Assert<uint8_t>(BitBigEndian ? 1 : 0);
        Reader.AssertPattern(1, 0);

        Reader.Order = (BigEndian || ForceBigEndian(Format)) ? Endian::Big : Endian::Little;

        const int32_t FileCount = Reader.ReadInt32();
        Reader.ReadInt32();  // end of file headers, not including padding before data
        Unk18 = Reader.Assert<int32_t>(0, std::numeric_limits<int32_t>::min());
        Reader.Assert<int32_t>(0);

        // Each header is at least 0xC bytes, so this also rejects absurd counts from corrupt files.
        if (FileCount < 0 || static_cast<int64_t>(FileCount) * 0xC > Reader.Remaining()) {
            throw BinaryException("Invalid BND3 file count " + std::to_string(FileCount));
        }

        std::vector<BinderFileHeader> FileHeaders;
        FileHeaders.reserve(static_cast<size_t>(FileCount));
        for (int32_t I = 0; I < FileCount; ++I) {
            FileHeaders.push_back(BinderFileHeader::ReadBinder3FileHeader(Reader, Format, BitBigEndian));
        }

        Files.clear();
        Files.reserve(FileHeaders.size());
        for (const BinderFileHeader& Header : FileHeaders) {
            Files.push_back(Header.ReadFileData(Reader));
        }
    }

    void BND3::WriteImpl(BinaryWriter& Writer) {
        std::vector<BinderFileHeader> FileHeaders;
        FileHeaders.reserve(Files.size());
        for (const BinderFile& File : Files) {
            FileHeaders.emplace_back(File);
        }

        Writer.Order = (BigEndian || ForceBigEndian(Format)) ? Endian::Big : Endian::Little;

        Writer.WriteMagic("BND3"sv);
        Writer.WriteFixStr(Version, 8);

        Binder::WriteFormat(Writer, BitBigEndian, Format);
        Writer.WriteBool(BigEndian);
        Writer.WriteBool(BitBigEndian);
        Writer.Pad(1);

        Writer.WriteInt32(static_cast<int32_t>(FileHeaders.size()));
        Writer.Reserve<int32_t>("FileHeadersEnd");
        Writer.WriteInt32(Unk18);
        Writer.WriteInt32(0);

        for (size_t I = 0; I < FileHeaders.size(); ++I) {
            FileHeaders[I].WriteBinder3FileHeader(Writer, Format, BitBigEndian, static_cast<int>(I));
        }

        for (size_t I = 0; I < FileHeaders.size(); ++I) {
            FileHeaders[I].WriteFileName(Writer, Format, false, static_cast<int>(I));
        }

        Writer.Fill<int32_t>("FileHeadersEnd", static_cast<int32_t>(Writer.Position()));

        for (size_t I = 0; I < FileHeaders.size(); ++I) {
            FileHeaders[I].WriteBinder3FileData(Writer, Writer, Format, static_cast<int>(I), Files[I].Bytes);
        }
    }
}  // namespace Souls
