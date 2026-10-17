// Copyright (C) 2026 Jake Rieger
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, version 3. This program is distributed in the hope that it will
// be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU General
// Public License along with this program. If not, see https://www.gnu.org/licenses/.

#pragma once

#include <libSouls/BinaryReader.hpp>
#include <libSouls/Formats/BHD5.hpp>
#include <libSouls/Souls.hpp>

#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of an exported class; only touched by this DLL's own code

    /// @brief The packed archives of an installed game (Data0.bhd and Data0.bdt, ...), read in place: files by their
    ///        path in the game, without unpacking.
    ///
    /// Archive headers are encrypted with RSA keys that aren't distributed with libSouls; BinderKeys
    /// (https://github.com/JKAnderson/BinderKeys) collects them, along with dictionaries of the paths the archives hold
    /// (the archives only store path hashes).
    ///
    /// @code
    /// ArchiveSet Archives = ArchiveSet::OpenWithKeys(GameFolder, BHD5::Game::EldenRing,
    /// "BinderKeys/EldenRing_PC/Key"); if (auto Bytes = Archives.Read("/chr/c0000.anibnd.dcx")) {
    ///     BND4 Binder = BND4::Read(*Bytes);  // decompresses the DCX
    /// }
    /// @endcode
    ///
    /// Reads are safe from several threads at once.
    class SOULS_API ArchiveSet {
    public:
        /// @brief An archive to open: its name (the .bhd/.bdt file name without extension, such as "Data0") and the
        ///        PEM text of its header key ("-----BEGIN RSA PUBLIC KEY-----"), or empty for an unencrypted header.
        struct Source {
            std::string Name;    ///< Such as "Data0".
            std::string KeyPem;  ///< The header's key, or empty.
        };

        ArchiveSet()                                 = default;
        ArchiveSet(ArchiveSet&&) noexcept            = default;
        ArchiveSet& operator=(ArchiveSet&&) noexcept = default;
        ArchiveSet(const ArchiveSet&)                = delete;
        ArchiveSet& operator=(const ArchiveSet&)     = delete;

        /// @brief Opens archives of a game folder. An archive whose .bhd isn't there is skipped; throws BinaryException
        ///        if one that is there can't be read (such as with the wrong key).
        static ArchiveSet
        Open(const std::filesystem::path& GameFolder, BHD5::Game Game, std::span<const Source> Archives);
        /// @brief Opens every archive of a game folder that has a key in KeyFolder (a `Name.pem` per archive, as
        ///        BinderKeys lays them out). Throws as Open does.
        static ArchiveSet
        OpenWithKeys(const std::filesystem::path& GameFolder, BHD5::Game Game, const std::filesystem::path& KeyFolder);

        /// @brief Whether a file is in the archives, by its path in the game ("/chr/c0000.anibnd.dcx"; case and
        ///        slashes don't matter).
        NODISCARD bool Contains(std::string_view Path) const;
        /// @brief A file's bytes as the game stores them (still DCX-compressed if it is), or nullopt if it isn't in the
        ///        archives.
        NODISCARD std::optional<std::vector<uint8_t>> Read(std::string_view Path) const;
        /// @brief The names of the archives that were opened.
        NODISCARD std::vector<std::string> ArchiveNames() const;
        /// @brief How many files the archives hold.
        NODISCARD size_t FileCount() const { return _ByHash.size(); }

    private:
        struct Archive {
            std::string Name;
            std::unique_ptr<BinaryReader> Data;  ///< the .bdt
            std::unique_ptr<std::mutex> Lock;    ///< one read at a time per .bdt
        };
        struct Located {
            size_t Archive = 0;
            BHD5::FileHeader File;
        };

        BHD5::Game _Format = BHD5::Game::EldenRing;
        std::vector<Archive> _Archives;
        std::unordered_map<uint64_t, Located> _ByHash;

        NODISCARD uint64_t Hash(std::string_view Path) const;
    };

#pragma warning(pop)

}  // namespace Souls
