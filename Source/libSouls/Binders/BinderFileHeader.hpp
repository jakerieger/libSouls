//
// Created by Jake Rieger on 10/7/2026.
//

#pragma once

#include "Binder.hpp"
#include "BinderFile.hpp"

#include <optional>
#include <string>

namespace Souls {
    // A file's entry in a binder's header table, plus the logic to read and write it. Internal to the binder formats.
    struct BinderFileHeader {
        Binder::FileFlags Flags = Binder::FileFlags::Flag1;
        int32_t ID              = -1;
        std::optional<std::string> Name;
        DCX::Type CompressionType = DCX::Type::Zlib;
        int64_t CompressedSize    = -1;
        int64_t UncompressedSize  = -1;
        int64_t DataOffset        = -1;

        BinderFileHeader() = default;
        explicit BinderFileHeader(const BinderFile& File)
            : Flags(File.Flags), ID(File.ID), Name(File.Name), CompressionType(File.CompressionType) {}

        static BinderFileHeader ReadBinder3FileHeader(BinaryReader& Reader, Binder::Format Format, bool BitBigEndian);
        static BinderFileHeader ReadBinder4FileHeader(BinaryReader& Reader,
                                                      Binder::Format Format,
                                                      bool BitBigEndian,
                                                      bool Unicode);

        // Reads (and decompresses, if flagged) this file's contents from the binder's data.
        BinderFile ReadFileData(BinaryReader& Reader) const;

        void WriteBinder3FileHeader(BinaryWriter& Writer, Binder::Format Format, bool BitBigEndian, int Index) const;
        void WriteBinder4FileHeader(BinaryWriter& Writer, Binder::Format Format, bool BitBigEndian, int Index) const;
        // Writes the contents (compressing if flagged) and fills in the sizes/offset reserved by the header.
        void WriteBinder3FileData(BinaryWriter& HeaderWriter,
                                  BinaryWriter& DataWriter,
                                  Binder::Format Format,
                                  int Index,
                                  const std::vector<uint8_t>& Bytes);
        void WriteBinder4FileData(BinaryWriter& HeaderWriter,
                                  BinaryWriter& DataWriter,
                                  Binder::Format Format,
                                  int Index,
                                  const std::vector<uint8_t>& Bytes);

        void WriteFileName(BinaryWriter& Writer, Binder::Format Format, bool Unicode, int Index) const;

    private:
        void WriteFileData(BinaryWriter& Writer, const std::vector<uint8_t>& Bytes);
    };
}  // namespace Souls
