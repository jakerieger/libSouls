// Copyright (C) 2026 Jake Rieger
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, version 3. This program is distributed in the hope that it will
// be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU General
// Public License along with this program. If not, see https://www.gnu.org/licenses/.

#pragma once

#include "Souls.hpp"
#include "BinaryReader.hpp"
#include "BinaryWriter.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// @brief Port of the dependency-free parts of SoulsFormats' SFUtil.
///
/// Anything that needs DCX, BND4 or a crypto library lives with those pieces instead.
namespace Souls::Util {
    /// @brief Minute-resolution timestamp used by binder headers.
    using BinderTime = std::chrono::sys_time<std::chrono::minutes>;

    /// @brief The byte with its bits in reverse order.
    SOULS_API uint8_t ReverseBits(uint8_t Value);

    /// @brief Copies File to "<File>.bak" unless that already exists (or Overwrite is set).
    ///
    /// Returns the backup path.
    SOULS_API std::filesystem::path Backup(const std::filesystem::path& File, bool Overwrite = false);

    /// @brief Extension / file name ignoring a trailing ".dcx": "a.flver.dcx" -> ".flver" / "a".
    SOULS_API std::string GetRealExtension(const std::filesystem::path& Path);
    /// @brief The file name without its extension, ignoring a trailing ".dcx" (see GetRealExtension).
    SOULS_API std::string GetRealFileName(const std::filesystem::path& Path);

    /// @brief FromSoftware's path hash used by BND/BXF hash tables.
    SOULS_API uint32_t FromPathHash(std::string_view Text);
    /// @brief The 64-bit form used to name files in the packed archives (BHD5) of Elden Ring: the path is lowercased,
    ///        uses forward slashes and starts with "/", then each character is folded in as hash = hash * Prime +
    ///        character.
    ///
    /// Dark Souls III and Sekiro store the 32-bit form (FromPathHash) in the same field.
    SOULS_API uint64_t FromPathHash64(std::string_view Text, uint64_t Prime = 0x85);
    /// @brief Whether the number is prime (binder hash tables size their buckets with primes).
    SOULS_API bool IsPrime(uint32_t Candidate);

    /// @brief Parses/produces binder timestamps like "07B26C10".
    ///
    /// Throws BinaryException on bad input.
    SOULS_API BinderTime BinderTimestampToDate(std::string_view Timestamp);
    /// @brief The binder timestamp for a time (the reverse of BinderTimestampToDate).
    SOULS_API std::string DateToBinderTimestamp(BinderTime Time);

    /// @brief The Adler-32 checksum of the data (as zlib streams end with).
    SOULS_API uint32_t Adler32(std::span<const uint8_t> Data);
    /// @brief Parses space-separated hex bytes: "DE AD BE EF".
    SOULS_API std::vector<uint8_t> ParseHexString(std::string_view Text);

    /// @brief Raw deflate streams (no zlib header or checksum).
    ///
    /// Level is the zlib level (-1 = default). Both throw BinaryException on failure.
    SOULS_API std::vector<uint8_t> InflateRaw(std::span<const uint8_t> Compressed);
    /// @brief Compresses Input as a raw deflate stream (see InflateRaw).
    SOULS_API std::vector<uint8_t> DeflateRaw(std::span<const uint8_t> Input, int Level = -1);

    /// @brief Reads a zlib stream occupying CompressedSize bytes (header + deflate data + checksum) and inflates it.
    SOULS_API std::vector<uint8_t> ReadZlib(BinaryReader& Reader, int CompressedSize);
    /// @brief Writes Input as a zlib stream and returns the number of bytes written.
    ///
    /// FormatByte is the second header byte (0x01, 0x5E, 0x9C or 0xDA, indicating compression level); Level is the zlib
    /// level (-1 = default).
    SOULS_API int WriteZlib(BinaryWriter& Writer, uint8_t FormatByte, std::span<const uint8_t> Input, int Level = -1);
}  // namespace Souls::Util
