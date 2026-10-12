// Copyright (C) 2026 Jake Rieger
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, version 3. This program is distributed in the hope that it will
// be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU General
// Public License along with this program. If not, see https://www.gnu.org/licenses/.

#pragma once

#include "IBinder.hpp"

#include <filesystem>
#include <span>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of an exported class; only touched by this DLL's own code
#pragma warning(disable : 4275)  // IBinder is header-only, so its not being exported is harmless

    /// @brief A BXF4 binder: a BND4-style container split across two files, a header file (BHF4, e.g. .tpfbhd) and a
    ///        data file (BDF4, e.g. .tpfbdt).
    ///
    /// Used by Bloodborne onwards.
    class SOULS_API BXF4 : public IBinder {
    public:
        /// @brief Unknown header flags.
        ///
        /// Preserved so a read/write round trip keeps them.
        bool Unk04 = false;
        /// @brief Unknown.
        bool Unk05 = false;

        /// @brief Whether multi-byte values are big-endian (as opposed to the *bit* order of the flag bytes).
        bool BigEndian = false;
        /// @brief Controls ordering of flag bits.
        bool BitBigEndian = false;

        /// @brief Whether names are UTF-16 (true) or Shift-JIS (false).
        bool Unicode = true;

        /// @brief 4 means the binder has a path hash table; the other seen value is 0.
        uint8_t Extended = 4;

        /// @brief A new, empty binder with the common modern settings and a current timestamp as its version.
        BXF4();

#pragma region Is
        /// @brief Whether the data (after any DCX decompression) is a header (BHF4) or data (BDF4) file.
        static bool IsBHD(std::span<const uint8_t> Data);
        /// @brief Whether the file on disk is a BXF4 header file (BHF4).
        static bool IsBHD(const std::filesystem::path& Path);
        /// @brief Whether the data is a BXF4 data file (BDF4).
        static bool IsBDT(std::span<const uint8_t> Data);
        /// @brief Whether the file on disk is a BXF4 data file (BDF4).
        static bool IsBDT(const std::filesystem::path& Path);
#pragma endregion

#pragma region Read
        /// @brief Reads from a header/data pair.
        ///
        /// Mix sources by constructing the BinaryReaders yourself.
        static BXF4 Read(BinaryReader& HeaderReader, BinaryReader& DataReader);
        /// @brief Reads the pair from memory.
        static BXF4 Read(std::span<const uint8_t> Header, std::span<const uint8_t> Data);
        /// @brief Reads the pair from disk.
        static BXF4 Read(const std::filesystem::path& HeaderPath, const std::filesystem::path& DataPath);
#pragma endregion

#pragma region Write
        /// @brief Writes the header and data files to the two writers.
        void Write(BinaryWriter& HeaderWriter, BinaryWriter& DataWriter);
        /// @brief Writes the header and data files to memory.
        BXFBytes Write();
        /// @brief Creates missing folders.
        void Write(const std::filesystem::path& HeaderPath, const std::filesystem::path& DataPath);
#pragma endregion
    };

#pragma warning(pop)

}  // namespace Souls
