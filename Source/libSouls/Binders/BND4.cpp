//
// Created by Jake Rieger on 10/7/2026.
//

#include "BND4.hpp"
#include "BinderFileHeader.hpp"
#include "BinderHashTable.hpp"

#include <libSouls/Util.hpp>

#include <string_view>

using namespace std::string_view_literals;

namespace Souls {
    using namespace Binder;

    BND4::BND4() {
        Version = Binder::CurrentVersion();
        Format = Format::IDs | Format::Names1 | Format::Names2 | Format::Compression;
    }

    bool BND4::IsImpl(BinaryReader& Reader) {
        if (Reader.Length() < 4) {
            return false;
        }
        return Reader.ReadAt<uint8_t>(0) == 'B' && Reader.ReadAt<uint8_t>(1) == 'N' &&
               Reader.ReadAt<uint8_t>(2) == 'D' && Reader.ReadAt<uint8_t>(3) == '4';
    }

    void BND4::ReadImpl(BinaryReader& Reader) {
        Reader.AssertMagic("BND4"sv);

        Unk04 = Reader.ReadBool();
        Unk05 = Reader.ReadBool();
        Reader.AssertPattern(2, 0);

        Reader.AssertPattern(1, 0);
        BigEndian    = Reader.ReadBool();
        BitBigEndian = !Reader.ReadBool();
        Reader.AssertPattern(1, 0);

        Reader.Order = BigEndian ? Endian::Big : Endian::Little;

        const int32_t FileCount = Reader.ReadInt32();
        Reader.Assert<int64_t>(0x40);  // header size
        Version = Reader.ReadFixStr(8);
        const int64_t FileHeaderSize = Reader.ReadInt64();
        Reader.ReadInt64();  // end of headers (includes the hash table)

        Unicode  = Reader.ReadBool();
        Format   = Binder::ReadFormat(Reader, BitBigEndian);
        Extended = Reader.Assert<uint8_t>(0, 1, 4, 0x80);
        Reader.AssertPattern(1, 0);

        Reader.Assert<int32_t>(0);

        if (Extended == 4) {
            const int64_t HashTableOffset = Reader.ReadInt64();
            Reader.StepIn(HashTableOffset);
            BinderHashTable::Assert(Reader);
            Reader.StepOut();
        } else {
            Reader.Assert<int64_t>(0);
        }

        if (FileHeaderSize != GetBND4FileHeaderSize(Format)) {
            throw BinaryException("File header size for this binder format is expected to be " +
                                  std::to_string(GetBND4FileHeaderSize(Format)) + ", but was " +
                                  std::to_string(FileHeaderSize));
        }
        // Each header is at least 0x10 bytes, so this also rejects absurd counts from corrupt files.
        if (FileCount < 0 || static_cast<int64_t>(FileCount) * FileHeaderSize > Reader.Remaining()) {
            throw BinaryException("Invalid BND4 file count " + std::to_string(FileCount));
        }

        std::vector<BinderFileHeader> FileHeaders;
        FileHeaders.reserve(static_cast<size_t>(FileCount));
        for (int32_t I = 0; I < FileCount; ++I) {
            FileHeaders.push_back(BinderFileHeader::ReadBinder4FileHeader(Reader, Format, BitBigEndian, Unicode));
        }

        Files.clear();
        Files.reserve(FileHeaders.size());
        for (const BinderFileHeader& Header : FileHeaders) {
            Files.push_back(Header.ReadFileData(Reader));
        }
    }

    void BND4::WriteImpl(BinaryWriter& Writer) {
        std::vector<BinderFileHeader> FileHeaders;
        FileHeaders.reserve(Files.size());
        for (const BinderFile& File : Files) {
            FileHeaders.emplace_back(File);
        }

        Writer.Order = BigEndian ? Endian::Big : Endian::Little;

        Writer.WriteMagic("BND4"sv);

        Writer.WriteBool(Unk04);
        Writer.WriteBool(Unk05);
        Writer.Pad(2);

        Writer.Pad(1);
        Writer.WriteBool(BigEndian);
        Writer.WriteBool(!BitBigEndian);
        Writer.Pad(1);

        Writer.WriteInt32(static_cast<int32_t>(FileHeaders.size()));
        Writer.WriteInt64(0x40);
        Writer.WriteFixStr(Version, 8);
        Writer.WriteInt64(GetBND4FileHeaderSize(Format));
        Writer.Reserve<int64_t>("HeadersEnd");

        Writer.WriteBool(Unicode);
        Binder::WriteFormat(Writer, BitBigEndian, Format);
        Writer.WriteByte(Extended);
        Writer.Pad(1);

        Writer.WriteInt32(0);
        Writer.Reserve<int64_t>("HashTableOffset");

        for (size_t I = 0; I < FileHeaders.size(); ++I) {
            FileHeaders[I].WriteBinder4FileHeader(Writer, Format, BitBigEndian, static_cast<int>(I));
        }

        for (size_t I = 0; I < FileHeaders.size(); ++I) {
            FileHeaders[I].WriteFileName(Writer, Format, Unicode, static_cast<int>(I));
        }

        if (Extended == 4) {
            Writer.Align(0x8);
            Writer.Fill<int64_t>("HashTableOffset", Writer.Position());
            BinderHashTable::Write(Writer, FileHeaders);
        } else {
            Writer.Fill<int64_t>("HashTableOffset", 0);
        }

        Writer.Fill<int64_t>("HeadersEnd", Writer.Position());

        for (size_t I = 0; I < FileHeaders.size(); ++I) {
            FileHeaders[I].WriteBinder4FileData(Writer, Writer, Format, static_cast<int>(I), Files[I].Bytes);
        }
    }
}  // namespace Souls
