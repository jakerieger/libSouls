//
// Created by Jake Rieger on 10/7/2026.
//

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

    // Reads files out of a BXF3 or BXF4 pair (header file + data file) one at a time, without loading the whole
    // archive. Some of these are huge: Elden Ring's 71_maptile.tpfbdt is 1.2 GB and holds 28,469 files, where
    // BXF4::Read would pull all of it into memory.
    //
    // This is also the way to get at the textures in a .tpfbhd/.tpfbdt pair, whose files are each a (DCX-wrapped) TPF:
    //
    //     BXFReader Reader("menu/71_maptile.tpfbhd", "menu/71_maptile.tpfbdt");
    //     for (size_t I = 0; I < Reader.FileCount(); ++I) {
    //         TPF Tpf = Reader.ReadTPF(I);
    //     }
    //
    // Whether the pair is BXF3 or BXF4 is worked out from the header file. Not thread-safe: reads share one stream.
    class SOULS_API BXFReader {
    public:
        enum class Generation { BXF3, BXF4 };

        // What the header file says about one file.
        struct FileInfo {
            Binder::FileFlags Flags = Binder::FileFlags::Flag1;
            int32_t ID              = -1;
            std::optional<std::string> Name;  // UTF-8
            // Sizes as stored: CompressedSize is what the data file holds, UncompressedSize is -1 if the format
            // doesn't record it.
            int64_t CompressedSize   = -1;
            int64_t UncompressedSize = -1;
            int64_t DataOffset       = -1;

            bool IsCompressed() const { return Binder::IsCompressed(Flags); }
        };

#pragma region Opening
        // Opens the pair from files. The data file stays open (and isn't loaded) until the reader is destroyed.
        BXFReader(const std::filesystem::path& HeaderPath, const std::filesystem::path& DataPath);
        // The header from memory, the data file on disk.
        BXFReader(std::span<const uint8_t> Header, const std::filesystem::path& DataPath);
        // Both from memory; the reader keeps the data.
        BXFReader(std::span<const uint8_t> Header, std::vector<uint8_t> Data);
#pragma endregion

        ~BXFReader();
        BXFReader(const BXFReader&)            = delete;
        BXFReader& operator=(const BXFReader&) = delete;
        BXFReader(BXFReader&&) noexcept;
        BXFReader& operator=(BXFReader&&) noexcept;

#pragma region Container settings
        Generation Kind = Generation::BXF3;
        std::string Version;
        Binder::Format Format = Binder::Format::None;
        bool BigEndian        = false;
        bool BitBigEndian     = false;
        // BXF4 only.
        bool Unk04       = false;
        bool Unk05       = false;
        bool Unicode     = false;
        uint8_t Extended = 0;
#pragma endregion

#pragma region Files
        size_t FileCount() const { return Files.size(); }
        const FileInfo& File(size_t Index) const;
        const std::vector<FileInfo>& AllFiles() const { return Files; }

        // The index of the file with exactly this stored name (a path like "71_MapTile\\x.tpf.dcx"), or nullopt.
        std::optional<size_t> IndexOf(std::string_view Name, bool IgnoreCase = false) const;
        // The index of the first file whose name ends in this file name, ignoring case and the folders before it.
        std::optional<size_t> IndexOfFileName(std::string_view FileName) const;

        // Reads one file, decompressing it if the binder flags it as compressed.
        BinderFile ReadFile(size_t Index);
        std::vector<uint8_t> ReadBytes(size_t Index);

        // Reads one file as a TPF (unwrapping DCX), for pairs that hold textures.
        TPF ReadTPF(size_t Index);
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
