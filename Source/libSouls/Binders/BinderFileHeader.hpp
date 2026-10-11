// Copyright (C) 2026 Jake Rieger
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, version 3. This program is distributed in the hope that it will
// be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU General
// Public License along with this program. If not, see https://www.gnu.org/licenses/.

#pragma once

#include "Binder.hpp"
#include "BinderFile.hpp"

#include <optional>
#include <string>

namespace Souls {
    /// @brief A file's entry in a binder's header table, plus the logic to read and write it.
    ///
    /// Internal to the binder formats.
    struct BinderFileHeader {
        /// @brief Flags indicating compression, and possibly other things.
        Binder::FileFlags Flags = Binder::FileFlags::Flag1;
        /// @brief ID of the file, or -1 for none.
        int32_t ID = -1;
        /// @brief Name of the file, or null for none.
        std::optional<std::string> Name;
        /// @brief How the file is compressed, if it is.
        DCX::Type CompressionType = DCX::Type::Zlib;
        /// @brief Size of the file after compression (or just the size of the file, if not compressed). Do not modify
        /// unless you know what you're doing.
        int64_t CompressedSize = -1;
        /// @brief Size of the file without compression. Do not modify unless you know what you're doing.
        int64_t UncompressedSize = -1;
        /// @brief Location of file data in the BND or BXF. Do not modify unless you know what you're doing.
        int64_t DataOffset = -1;

        BinderFileHeader() = default;
        /// @brief The header of a file about to be written; sizes and offset are filled in while writing.
        explicit BinderFileHeader(const BinderFile& File)
            : Flags(File.Flags), ID(File.ID), Name(File.Name), CompressionType(File.CompressionType) {}

        /// @brief Reads a BND3/BXF3 file header.
        static BinderFileHeader ReadBinder3FileHeader(BinaryReader& Reader, Binder::Format Format, bool BitBigEndian);
        /// @brief Reads a BND4/BXF4 file header.
        static BinderFileHeader
        ReadBinder4FileHeader(BinaryReader& Reader, Binder::Format Format, bool BitBigEndian, bool Unicode);

        /// @brief Reads (and decompresses, if flagged) this file's contents from the binder's data.
        BinderFile ReadFileData(BinaryReader& Reader) const;

        /// @brief Writes a BND3/BXF3 file header, reserving the sizes and offset to fill in with the data.
        void WriteBinder3FileHeader(BinaryWriter& Writer, Binder::Format Format, bool BitBigEndian, int Index) const;
        /// @brief Writes a BND4/BXF4 file header, reserving the sizes and offset to fill in with the data.
        void WriteBinder4FileHeader(BinaryWriter& Writer, Binder::Format Format, bool BitBigEndian, int Index) const;
        /// @brief Writes the contents (compressing if flagged) and fills in the sizes/offset reserved by the header.
        void WriteBinder3FileData(BinaryWriter& HeaderWriter,
                                  BinaryWriter& DataWriter,
                                  Binder::Format Format,
                                  int Index,
                                  const std::vector<uint8_t>& Bytes);
        /// @brief Writes the contents (compressing if flagged) and fills in the sizes and offset reserved by the
        /// BND4/BXF4 header.
        void WriteBinder4FileData(BinaryWriter& HeaderWriter,
                                  BinaryWriter& DataWriter,
                                  Binder::Format Format,
                                  int Index,
                                  const std::vector<uint8_t>& Bytes);

        /// @brief Writes the file's name and fills in the name offset reserved by the header.
        void WriteFileName(BinaryWriter& Writer, Binder::Format Format, bool Unicode, int Index) const;

    private:
        void WriteFileData(BinaryWriter& Writer, const std::vector<uint8_t>& Bytes);
    };
}  // namespace Souls
