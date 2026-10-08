//
// Created by Jake Rieger on 10/7/2026.
//

#include "BXF4.hpp"
#include "BinderFileHeader.hpp"
#include "BinderHashTable.hpp"

#include <libSouls/Formats/DCX.hpp>

#include <string_view>

using namespace std::string_view_literals;

namespace Souls {
    using namespace Binder;

    namespace {
        bool HasMagic(BinaryReader& Source, std::string_view Magic) {
            DCX::DecompressedReader Decompressed(Source);
            BinaryReader& Reader = Decompressed.Reader();
            if (Reader.Length() < 4) {
                return false;
            }
            return Reader.ReadAt<uint8_t>(0) == static_cast<uint8_t>(Magic[0]) &&
                   Reader.ReadAt<uint8_t>(1) == static_cast<uint8_t>(Magic[1]) &&
                   Reader.ReadAt<uint8_t>(2) == static_cast<uint8_t>(Magic[2]) &&
                   Reader.ReadAt<uint8_t>(3) == static_cast<uint8_t>(Magic[3]);
        }

        bool HasMagic(std::span<const uint8_t> Data, std::string_view Magic) {
            BinaryReader Source(Data);
            return HasMagic(Source, Magic);
        }

        bool HasMagic(const std::filesystem::path& Path, std::string_view Magic) {
            BinaryReader Source(Path);
            return HasMagic(Source, Magic);
        }

        // The data file's header only repeats what the header file says (and doesn't always match it), so none of
        // it is kept.
        void ReadBDFHeader(BinaryReader& Reader) {
            Reader.AssertMagic("BDF4"sv);
            Reader.ReadBool();  // Unk04
            Reader.ReadBool();  // Unk05
            Reader.AssertPattern(3, 0);
            Reader.Order = Reader.ReadBool() ? Endian::Big : Endian::Little;
            Reader.ReadBool();  // BitBigEndian
            Reader.AssertPattern(1, 0);
            Reader.Assert<int32_t>(0);
            Reader.Assert<int64_t>(0x30, 0x40);  // header size; 0x40 is probably a mistake in some files
            Reader.ReadFixStr(8);                // version
            Reader.Assert<int64_t>(0);
            Reader.Assert<int64_t>(0);
        }

        std::vector<BinderFileHeader> ReadBHFHeader(BXF4& Bxf, BinaryReader& Reader) {
            Reader.AssertMagic("BHF4"sv);

            Bxf.Unk04 = Reader.ReadBool();
            Bxf.Unk05 = Reader.ReadBool();
            Reader.AssertPattern(2, 0);

            Reader.AssertPattern(1, 0);
            Bxf.BigEndian    = Reader.ReadBool();
            Bxf.BitBigEndian = !Reader.ReadBool();
            Reader.AssertPattern(1, 0);

            Reader.Order = Bxf.BigEndian ? Endian::Big : Endian::Little;

            const int32_t FileCount = Reader.ReadInt32();
            Reader.Assert<int64_t>(0x40);  // header size
            Bxf.Version                  = Reader.ReadFixStr(8);
            const int64_t FileHeaderSize = Reader.ReadInt64();
            Reader.Assert<int64_t>(0);

            Bxf.Unicode  = Reader.ReadBool();
            Bxf.Format   = Binder::ReadFormat(Reader, Bxf.BitBigEndian);
            Bxf.Extended = Reader.Assert<uint8_t>(0, 4);
            Reader.AssertPattern(1, 0);

            if (FileHeaderSize != GetBND4FileHeaderSize(Bxf.Format)) {
                throw BinaryException("File header size for this binder format is expected to be " +
                                      std::to_string(GetBND4FileHeaderSize(Bxf.Format)) + ", but was " +
                                      std::to_string(FileHeaderSize));
            }

            Reader.Assert<int32_t>(0);

            if (Bxf.Extended == 4) {
                const int64_t HashGroupsOffset = Reader.ReadInt64();
                Reader.StepIn(HashGroupsOffset);
                BinderHashTable::Assert(Reader);
                Reader.StepOut();
            } else {
                Reader.Assert<int64_t>(0);
            }

            if (FileCount < 0 || static_cast<int64_t>(FileCount) * FileHeaderSize > Reader.Remaining()) {
                throw BinaryException("Invalid BXF4 file count " + std::to_string(FileCount));
            }

            std::vector<BinderFileHeader> FileHeaders;
            FileHeaders.reserve(static_cast<size_t>(FileCount));
            for (int32_t I = 0; I < FileCount; ++I) {
                FileHeaders.push_back(
                  BinderFileHeader::ReadBinder4FileHeader(Reader, Bxf.Format, Bxf.BitBigEndian, Bxf.Unicode));
            }
            return FileHeaders;
        }

        void WriteBDFHeader(const BXF4& Bxf, BinaryWriter& Writer) {
            Writer.Order = Bxf.BigEndian ? Endian::Big : Endian::Little;
            Writer.WriteMagic("BDF4"sv);
            Writer.WriteBool(Bxf.Unk04);
            Writer.WriteBool(Bxf.Unk05);
            Writer.Pad(3);
            Writer.WriteBool(Bxf.BigEndian);
            Writer.WriteBool(!Bxf.BitBigEndian);
            Writer.Pad(1);
            Writer.WriteInt32(0);
            Writer.WriteInt64(0x30);
            Writer.WriteFixStr(Bxf.Version, 8);
            Writer.WriteInt64(0);
            Writer.WriteInt64(0);
        }

        void WriteBHFHeader(const BXF4& Bxf, BinaryWriter& Writer, const std::vector<BinderFileHeader>& FileHeaders) {
            Writer.Order = Bxf.BigEndian ? Endian::Big : Endian::Little;

            Writer.WriteMagic("BHF4"sv);

            Writer.WriteBool(Bxf.Unk04);
            Writer.WriteBool(Bxf.Unk05);
            Writer.Pad(2);

            Writer.Pad(1);
            Writer.WriteBool(Bxf.BigEndian);
            Writer.WriteBool(!Bxf.BitBigEndian);
            Writer.Pad(1);

            Writer.WriteInt32(static_cast<int32_t>(FileHeaders.size()));
            Writer.WriteInt64(0x40);
            Writer.WriteFixStr(Bxf.Version, 8);
            Writer.WriteInt64(GetBND4FileHeaderSize(Bxf.Format));
            Writer.WriteInt64(0);

            Writer.WriteBool(Bxf.Unicode);
            Binder::WriteFormat(Writer, Bxf.BitBigEndian, Bxf.Format);
            Writer.WriteByte(Bxf.Extended);
            Writer.Pad(1);

            Writer.WriteInt32(0);
            Writer.Reserve<int64_t>("HashTableOffset");

            for (size_t I = 0; I < FileHeaders.size(); ++I) {
                FileHeaders[I].WriteBinder4FileHeader(Writer, Bxf.Format, Bxf.BitBigEndian, static_cast<int>(I));
            }
            for (size_t I = 0; I < FileHeaders.size(); ++I) {
                FileHeaders[I].WriteFileName(Writer, Bxf.Format, Bxf.Unicode, static_cast<int>(I));
            }

            if (Bxf.Extended == 4) {
                Writer.Align(0x8);
                Writer.Fill<int64_t>("HashTableOffset", Writer.Position());
                BinderHashTable::Write(Writer, FileHeaders);
            } else {
                Writer.Fill<int64_t>("HashTableOffset", 0);
            }
        }
    }  // namespace

    BXF4::BXF4() {
        Version = Binder::CurrentVersion();
        Format  = Format::IDs | Format::Names1 | Format::Names2 | Format::Compression;
    }

    bool BXF4::IsBHD(std::span<const uint8_t> Data) { return HasMagic(Data, "BHF4"sv); }
    bool BXF4::IsBHD(const std::filesystem::path& Path) { return HasMagic(Path, "BHF4"sv); }
    bool BXF4::IsBDT(std::span<const uint8_t> Data) { return HasMagic(Data, "BDF4"sv); }
    bool BXF4::IsBDT(const std::filesystem::path& Path) { return HasMagic(Path, "BDF4"sv); }

    BXF4 BXF4::Read(BinaryReader& HeaderReader, BinaryReader& DataReader) {
        BXF4 Result;
        ReadBDFHeader(DataReader);
        const std::vector<BinderFileHeader> FileHeaders = ReadBHFHeader(Result, HeaderReader);
        Result.Files.clear();
        Result.Files.reserve(FileHeaders.size());
        for (const BinderFileHeader& Header : FileHeaders) {
            Result.Files.push_back(Header.ReadFileData(DataReader));
        }
        return Result;
    }

    BXF4 BXF4::Read(std::span<const uint8_t> Header, std::span<const uint8_t> Data) {
        BinaryReader HeaderReader(Header);
        BinaryReader DataReader(Data);
        return Read(HeaderReader, DataReader);
    }

    BXF4 BXF4::Read(const std::filesystem::path& HeaderPath, const std::filesystem::path& DataPath) {
        BinaryReader HeaderReader(HeaderPath);
        BinaryReader DataReader(DataPath);
        return Read(HeaderReader, DataReader);
    }

    void BXF4::Write(BinaryWriter& HeaderWriter, BinaryWriter& DataWriter) {
        std::vector<BinderFileHeader> FileHeaders;
        FileHeaders.reserve(Files.size());
        for (const BinderFile& File : Files) {
            FileHeaders.emplace_back(File);
        }

        WriteBDFHeader(*this, DataWriter);
        WriteBHFHeader(*this, HeaderWriter, FileHeaders);
        for (size_t I = 0; I < FileHeaders.size(); ++I) {
            FileHeaders[I].WriteBinder4FileData(HeaderWriter, DataWriter, Format, static_cast<int>(I), Files[I].Bytes);
        }
    }

    BXFBytes BXF4::Write() {
        BinaryWriter HeaderWriter(Endian::Little);
        BinaryWriter DataWriter(Endian::Little);
        Write(HeaderWriter, DataWriter);
        return {HeaderWriter.ToBytes(), DataWriter.ToBytes()};
    }

    void BXF4::Write(const std::filesystem::path& HeaderPath, const std::filesystem::path& DataPath) {
        for (const auto* Path : {&HeaderPath, &DataPath}) {
            if (Path->has_parent_path()) {
                std::filesystem::create_directories(Path->parent_path());
            }
        }
        BinaryWriter HeaderWriter(HeaderPath, Endian::Little);
        BinaryWriter DataWriter(DataPath, Endian::Little);
        Write(HeaderWriter, DataWriter);
        HeaderWriter.Finish();
        DataWriter.Finish();
    }
}  // namespace Souls

#include "BXFHeaders.hpp"

namespace Souls::BXFHeaders {
    void ReadBDF4(BinaryReader& Reader) {
        ReadBDFHeader(Reader);
    }

    std::vector<BinderFileHeader> ReadBHF4(BXF4& Bxf, BinaryReader& Reader) {
        return ReadBHFHeader(Bxf, Reader);
    }
}  // namespace Souls::BXFHeaders
