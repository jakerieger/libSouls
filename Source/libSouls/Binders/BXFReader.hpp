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

#include <libSouls/Formats/TPF.hpp>

#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of an exported class; only touched by this DLL's own code

    /// @brief Reads files out of a BXF3 or BXF4 pair (header file + data file) one at a time, without loading the whole
    ///        archive.
    ///
    /// Some of these are huge: Elden Ring's 71_maptile.tpfbdt is 1.2 GB and holds 28,469 files, where BXF4::Read would
    /// pull all of it into memory.
    ///
    /// This is also the way to get at the textures in a .tpfbhd/.tpfbdt pair, whose files are each a (DCX-wrapped) TPF:
    ///
    /// @code
    /// BXFReader Reader("menu/71_maptile.tpfbhd", "menu/71_maptile.tpfbdt");
    /// for (size_t I = 0; I < Reader.FileCount(); ++I) {
    ///     TPF Tpf = Reader.ReadTPF(I);
    /// }
    /// @endcode
    ///
    /// Whether the pair is BXF3 or BXF4 is worked out from the header file. Not thread-safe: reads share one stream.
    class SOULS_API BXFReader {
    public:
        /// @brief Which BXF version a pair is.
        enum class Generation { BXF3, BXF4 };

        /// @brief What the header file says about one file.
        struct FileInfo {
            /// @brief The file's flags (bit 0 marks it as compressed).
            Binder::FileFlags Flags = Binder::FileFlags::Flag1;
            /// @brief The file's ID, or -1 if the binder doesn't store IDs.
            int32_t ID = -1;
            std::optional<std::string> Name;  ///< UTF-8
            /// @brief Sizes as stored: CompressedSize is what the data file holds, UncompressedSize is -1 if the format
            ///        doesn't record it.
            int64_t CompressedSize = -1;
            /// @brief The size after decompression, or -1 if the format doesn't record it.
            int64_t UncompressedSize = -1;
            /// @brief Where the file's data starts in the data file.
            int64_t DataOffset = -1;

            /// @brief Whether the file is stored compressed.
            bool IsCompressed() const { return Binder::IsCompressed(Flags); }
        };

#pragma region Opening
        /// @brief Opens the pair from files.
        ///
        /// The data file stays open (and isn't loaded) until the reader is destroyed.
        BXFReader(const std::filesystem::path& HeaderPath, const std::filesystem::path& DataPath);
        /// @brief The header from memory, the data file on disk.
        BXFReader(std::span<const uint8_t> Header, const std::filesystem::path& DataPath);
        /// @brief Both from memory; the reader keeps the data.
        BXFReader(std::span<const uint8_t> Header, std::vector<uint8_t> Data);
#pragma endregion

        ~BXFReader();
        BXFReader(const BXFReader&)            = delete;
        BXFReader& operator=(const BXFReader&) = delete;
        /// @brief Takes over another reader's open files.
        BXFReader(BXFReader&&) noexcept;
        /// @brief Takes over another reader's open files.
        BXFReader& operator=(BXFReader&&) noexcept;

#pragma region Container settings
        /// @brief Whether the pair is BXF3 or BXF4.
        Generation Kind = Generation::BXF3;
        /// @brief The binder's timestamp/version string.
        std::string Version;
        /// @brief Which fields the file headers have.
        Binder::Format Format = Binder::Format::None;
        /// @brief Whether multi-byte values are big-endian.
        bool BigEndian = false;
        /// @brief Whether the flag bits are in big-endian bit order.
        bool BitBigEndian = false;
        /// @brief BXF4 only.
        bool Unk04 = false;
        /// @brief Unknown.
        bool Unk05 = false;
        /// @brief Whether names are UTF-16 (BXF4 only; BXF3 names are Shift-JIS).
        bool Unicode = false;
        /// @brief 4 if the header has a path hash table (BXF4 only).
        uint8_t Extended = 0;
#pragma endregion

#pragma region Files
        /// @brief How many files the pair holds.
        size_t FileCount() const { return Files.size(); }
        /// @brief What the header says about one file; throws std::out_of_range for a bad index.
        const FileInfo& File(size_t Index) const;
        /// @brief What the header says about every file.
        const std::vector<FileInfo>& AllFiles() const { return Files; }

        /// @brief The index of the file with exactly this stored name (a path like "71_MapTile\\x.tpf.dcx"), or
        ///        nullopt.
        std::optional<size_t> IndexOf(std::string_view Name, bool IgnoreCase = false) const;
        /// @brief The index of the first file whose name ends in this file name, ignoring case and the folders before
        ///        it.
        std::optional<size_t> IndexOfFileName(std::string_view FileName) const;

        /// @brief Reads one file, decompressing it if the binder flags it as compressed.
        BinderFile ReadFile(size_t Index);
        /// @brief Reads one file's (decompressed) bytes.
        std::vector<uint8_t> ReadBytes(size_t Index);

        /// @brief Reads one file as a TPF (unwrapping DCX), for pairs that hold textures.
        TPF ReadTPF(size_t Index);
        /// @brief Reads the file with this file name (see IndexOfFileName) as a TPF; throws if there is none.
        TPF ReadTPF(std::string_view FileName);
#pragma endregion

    private:
        struct Impl;
        void Initialize(std::span<const uint8_t> Header);

        std::vector<FileInfo> Files;
        std::unique_ptr<Impl> State;
    };

#pragma warning(pop)

}  // namespace Souls
