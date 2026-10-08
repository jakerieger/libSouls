//
// Created by Jake Rieger on 10/7/2026.
//

#include "BinderFileHeader.hpp"

#include <libSouls/TextEncoding.hpp>

namespace Souls {
    using namespace Binder;

    namespace {
        std::string ReadNameAt(BinaryReader& Reader, int64_t Offset, bool Unicode) {
            Reader.StepIn(Offset);
            std::string Name = Unicode ? Text::UTF16ToUTF8(Reader.ReadUTF16()) : Reader.ReadShiftJIS();
            Reader.StepOut();
            return Name;
        }

        std::string Key(const char* Field, int Index) {
            return Field + std::to_string(Index);
        }
    }  // namespace

    BinderFileHeader BinderFileHeader::ReadBinder3FileHeader(BinaryReader& Reader, Format Format, bool BitBigEndian) {
        BinderFileHeader Header;
        Header.Flags = ReadFileFlags(Reader, BitBigEndian);
        Reader.AssertPattern(3, 0);

        Header.CompressedSize = Reader.ReadInt32();
        Header.DataOffset = HasLongOffsets(Format) ? Reader.ReadInt64() : static_cast<int64_t>(Reader.ReadUInt32());

        if (HasIDs(Format)) {
            Header.ID = Reader.ReadInt32();
        }

        if (HasNames(Format)) {
            const int32_t NameOffset = Reader.ReadInt32();
            Header.Name              = ReadNameAt(Reader, NameOffset, false);
        }

        if (HasCompression(Format)) {
            Header.UncompressedSize = Reader.ReadInt32();
        }

        return Header;
    }

    BinderFileHeader BinderFileHeader::ReadBinder4FileHeader(BinaryReader& Reader,
                                                             Format Format,
                                                             bool BitBigEndian,
                                                             bool Unicode) {
        BinderFileHeader Header;
        Header.Flags = ReadFileFlags(Reader, BitBigEndian);
        Reader.AssertPattern(3, 0);
        Reader.Assert<int32_t>(-1);

        Header.CompressedSize = Reader.ReadInt64();
        if (HasCompression(Format)) {
            Header.UncompressedSize = Reader.ReadInt64();
        }

        Header.DataOffset = HasLongOffsets(Format) ? Reader.ReadInt64() : static_cast<int64_t>(Reader.ReadUInt32());

        if (HasIDs(Format)) {
            Header.ID = Reader.ReadInt32();
        }

        if (HasNames(Format)) {
            const uint32_t NameOffset = Reader.ReadUInt32();
            Header.Name               = ReadNameAt(Reader, NameOffset, Unicode);
        }

        // A strange case that (as far as anyone knows) only appears in PC save files. This isn't really an ID, but
        // it's non-zero in some cases.
        if (Format == Format::Names1) {
            Header.ID = Reader.ReadInt32();
            Reader.Assert<int32_t>(0);
        }

        return Header;
    }

    BinderFile BinderFileHeader::ReadFileData(BinaryReader& Reader) const {
        if (CompressedSize < 0 || CompressedSize > Reader.Length()) {
            throw BinaryException("Invalid file size " + std::to_string(CompressedSize) + " for binder file");
        }

        Reader.StepIn(DataOffset);
        std::vector<uint8_t> Bytes = Reader.ReadBytes(static_cast<size_t>(CompressedSize));
        Reader.StepOut();

        BinderFile File;
        File.Flags = Flags;
        File.ID    = ID;
        File.Name  = Name;
        File.CompressionType = DCX::Type::Zlib;
        if (IsCompressed(Flags)) {
            File.Bytes = DCX::Decompress(Bytes, File.CompressionType);
        } else {
            File.Bytes = std::move(Bytes);
        }
        return File;
    }

    void BinderFileHeader::WriteBinder3FileHeader(BinaryWriter& Writer,
                                                  Format Format,
                                                  bool BitBigEndian,
                                                  int Index) const {
        WriteFileFlags(Writer, BitBigEndian, Flags);
        Writer.Pad(3);

        Writer.Reserve<int32_t>(Key("FileCompressedSize", Index));

        if (HasLongOffsets(Format)) {
            Writer.Reserve<int64_t>(Key("FileDataOffset", Index));
        } else {
            Writer.Reserve<uint32_t>(Key("FileDataOffset", Index));
        }

        if (HasIDs(Format)) {
            Writer.WriteInt32(ID);
        }

        if (HasNames(Format)) {
            Writer.Reserve<int32_t>(Key("FileNameOffset", Index));
        }

        if (HasCompression(Format)) {
            Writer.Reserve<int32_t>(Key("FileUncompressedSize", Index));
        }
    }

    void BinderFileHeader::WriteBinder4FileHeader(BinaryWriter& Writer,
                                                  Format Format,
                                                  bool BitBigEndian,
                                                  int Index) const {
        WriteFileFlags(Writer, BitBigEndian, Flags);
        Writer.Pad(3);
        Writer.WriteInt32(-1);

        Writer.Reserve<int64_t>(Key("FileCompressedSize", Index));
        if (HasCompression(Format)) {
            Writer.Reserve<int64_t>(Key("FileUncompressedSize", Index));
        }

        if (HasLongOffsets(Format)) {
            Writer.Reserve<int64_t>(Key("FileDataOffset", Index));
        } else {
            Writer.Reserve<uint32_t>(Key("FileDataOffset", Index));
        }

        if (HasIDs(Format)) {
            Writer.WriteInt32(ID);
        }

        if (HasNames(Format)) {
            Writer.Reserve<int32_t>(Key("FileNameOffset", Index));
        }

        if (Format == Format::Names1) {
            Writer.WriteInt32(ID);
            Writer.WriteInt32(0);
        }
    }

    void BinderFileHeader::WriteFileData(BinaryWriter& Writer, const std::vector<uint8_t>& Bytes) {
        if (!Bytes.empty()) {
            Writer.Align(0x10);
        }

        DataOffset       = Writer.Position();
        UncompressedSize = static_cast<int64_t>(Bytes.size());
        if (IsCompressed(Flags)) {
            const std::vector<uint8_t> Compressed = DCX::Compress(Bytes, CompressionType);
            CompressedSize                        = static_cast<int64_t>(Compressed.size());
            Writer.WriteBytes(Compressed);
        } else {
            CompressedSize = static_cast<int64_t>(Bytes.size());
            Writer.WriteBytes(Bytes);
        }
    }

    void BinderFileHeader::WriteBinder3FileData(BinaryWriter& HeaderWriter,
                                                BinaryWriter& DataWriter,
                                                Format Format,
                                                int Index,
                                                const std::vector<uint8_t>& Bytes) {
        WriteFileData(DataWriter, Bytes);

        HeaderWriter.Fill<int32_t>(Key("FileCompressedSize", Index), static_cast<int32_t>(CompressedSize));
        if (HasCompression(Format)) {
            HeaderWriter.Fill<int32_t>(Key("FileUncompressedSize", Index), static_cast<int32_t>(UncompressedSize));
        }

        if (HasLongOffsets(Format)) {
            HeaderWriter.Fill<int64_t>(Key("FileDataOffset", Index), DataOffset);
        } else {
            HeaderWriter.Fill<uint32_t>(Key("FileDataOffset", Index), static_cast<uint32_t>(DataOffset));
        }
    }

    void BinderFileHeader::WriteBinder4FileData(BinaryWriter& HeaderWriter,
                                                BinaryWriter& DataWriter,
                                                Format Format,
                                                int Index,
                                                const std::vector<uint8_t>& Bytes) {
        WriteFileData(DataWriter, Bytes);

        HeaderWriter.Fill<int64_t>(Key("FileCompressedSize", Index), CompressedSize);
        if (HasCompression(Format)) {
            HeaderWriter.Fill<int64_t>(Key("FileUncompressedSize", Index), UncompressedSize);
        }

        if (HasLongOffsets(Format)) {
            HeaderWriter.Fill<int64_t>(Key("FileDataOffset", Index), DataOffset);
        } else {
            HeaderWriter.Fill<uint32_t>(Key("FileDataOffset", Index), static_cast<uint32_t>(DataOffset));
        }
    }

    void BinderFileHeader::WriteFileName(BinaryWriter& Writer, Format Format, bool Unicode, int Index) const {
        if (!HasNames(Format)) {
            return;
        }
        if (!Name) {
            throw BinaryException("Binder file " + std::to_string(Index) + " has no name, but the format requires one");
        }

        Writer.Fill<int32_t>(Key("FileNameOffset", Index), static_cast<int32_t>(Writer.Position()));
        if (Unicode) {
            Writer.WriteUTF16(Text::UTF8ToUTF16(*Name), true);
        } else {
            Writer.WriteShiftJIS(*Name, true);
        }
    }
}  // namespace Souls
