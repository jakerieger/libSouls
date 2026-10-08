//
// Created by Jake Rieger on 10/7/2026.
//

#include "BXF3.hpp"
#include "BinderFileHeader.hpp"

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

        void ReadBDFHeader(BinaryReader& Reader) {
            Reader.AssertMagic("BDF3"sv);
            Reader.ReadFixStr(8);  // version
            Reader.Assert<int32_t>(0);
        }

        std::vector<BinderFileHeader> ReadBHFHeader(BXF3& Bxf, BinaryReader& Reader) {
            Reader.AssertMagic("BHF3"sv);
            Bxf.Version = Reader.ReadFixStr(8);

            Bxf.BitBigEndian = Reader.ReadAt<uint8_t>(0xE) != 0;

            Bxf.Format    = Binder::ReadFormat(Reader, Bxf.BitBigEndian);
            Bxf.BigEndian = Reader.ReadBool();
            Reader.Assert<uint8_t>(Bxf.BitBigEndian ? 1 : 0);
            Reader.AssertPattern(1, 0);

            Reader.Order = (Bxf.BigEndian || ForceBigEndian(Bxf.Format)) ? Endian::Big : Endian::Little;

            const int32_t FileCount = Reader.ReadInt32();
            Reader.Assert<int32_t>(0);
            Reader.Assert<int32_t>(0);
            Reader.Assert<int32_t>(0);

            if (FileCount < 0 || static_cast<int64_t>(FileCount) * 0xC > Reader.Remaining()) {
                throw BinaryException("Invalid BXF3 file count " + std::to_string(FileCount));
            }

            std::vector<BinderFileHeader> FileHeaders;
            FileHeaders.reserve(static_cast<size_t>(FileCount));
            for (int32_t I = 0; I < FileCount; ++I) {
                FileHeaders.push_back(BinderFileHeader::ReadBinder3FileHeader(Reader, Bxf.Format, Bxf.BitBigEndian));
            }
            return FileHeaders;
        }

        void WriteBDFHeader(const BXF3& Bxf, BinaryWriter& Writer) {
            Writer.WriteMagic("BDF3"sv);
            Writer.WriteFixStr(Bxf.Version, 8);
            Writer.WriteInt32(0);
        }

        void WriteBHFHeader(const BXF3& Bxf, BinaryWriter& Writer, const std::vector<BinderFileHeader>& FileHeaders) {
            Writer.Order = (Bxf.BigEndian || ForceBigEndian(Bxf.Format)) ? Endian::Big : Endian::Little;

            Writer.WriteMagic("BHF3"sv);
            Writer.WriteFixStr(Bxf.Version, 8);

            Binder::WriteFormat(Writer, Bxf.BitBigEndian, Bxf.Format);
            Writer.WriteBool(Bxf.BigEndian);
            Writer.WriteBool(Bxf.BitBigEndian);
            Writer.Pad(1);

            Writer.WriteInt32(static_cast<int32_t>(FileHeaders.size()));
            Writer.WriteInt32(0);
            Writer.WriteInt32(0);
            Writer.WriteInt32(0);

            for (size_t I = 0; I < FileHeaders.size(); ++I) {
                FileHeaders[I].WriteBinder3FileHeader(Writer, Bxf.Format, Bxf.BitBigEndian, static_cast<int>(I));
            }
            for (size_t I = 0; I < FileHeaders.size(); ++I) {
                FileHeaders[I].WriteFileName(Writer, Bxf.Format, false, static_cast<int>(I));
            }
        }
    }  // namespace

    BXF3::BXF3() {
        Version = Binder::CurrentVersion();
        Format  = Format::IDs | Format::Names1 | Format::Names2 | Format::Compression;
    }

    bool BXF3::IsBHD(std::span<const uint8_t> Data) { return HasMagic(Data, "BHF3"sv); }
    bool BXF3::IsBHD(const std::filesystem::path& Path) { return HasMagic(Path, "BHF3"sv); }
    bool BXF3::IsBDT(std::span<const uint8_t> Data) { return HasMagic(Data, "BDF3"sv); }
    bool BXF3::IsBDT(const std::filesystem::path& Path) { return HasMagic(Path, "BDF3"sv); }

    BXF3 BXF3::Read(BinaryReader& HeaderReader, BinaryReader& DataReader) {
        BXF3 Result;
        ReadBDFHeader(DataReader);
        const std::vector<BinderFileHeader> FileHeaders = ReadBHFHeader(Result, HeaderReader);
        Result.Files.clear();
        Result.Files.reserve(FileHeaders.size());
        for (const BinderFileHeader& Header : FileHeaders) {
            Result.Files.push_back(Header.ReadFileData(DataReader));
        }
        return Result;
    }

    BXF3 BXF3::Read(std::span<const uint8_t> Header, std::span<const uint8_t> Data) {
        BinaryReader HeaderReader(Header);
        BinaryReader DataReader(Data);
        return Read(HeaderReader, DataReader);
    }

    BXF3 BXF3::Read(const std::filesystem::path& HeaderPath, const std::filesystem::path& DataPath) {
        BinaryReader HeaderReader(HeaderPath);
        BinaryReader DataReader(DataPath);
        return Read(HeaderReader, DataReader);
    }

    void BXF3::Write(BinaryWriter& HeaderWriter, BinaryWriter& DataWriter) {
        std::vector<BinderFileHeader> FileHeaders;
        FileHeaders.reserve(Files.size());
        for (const BinderFile& File : Files) {
            FileHeaders.emplace_back(File);
        }

        WriteBDFHeader(*this, DataWriter);
        WriteBHFHeader(*this, HeaderWriter, FileHeaders);
        for (size_t I = 0; I < FileHeaders.size(); ++I) {
            FileHeaders[I].WriteBinder3FileData(HeaderWriter, DataWriter, Format, static_cast<int>(I), Files[I].Bytes);
        }
    }

    BXFBytes BXF3::Write() {
        BinaryWriter HeaderWriter(Endian::Little);
        BinaryWriter DataWriter(Endian::Little);
        Write(HeaderWriter, DataWriter);
        return {HeaderWriter.ToBytes(), DataWriter.ToBytes()};
    }

    void BXF3::Write(const std::filesystem::path& HeaderPath, const std::filesystem::path& DataPath) {
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
