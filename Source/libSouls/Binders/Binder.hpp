// Copyright (C) 2026 Jake Rieger
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, version 3. This program is distributed in the hope that it will
// be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU General
// Public License along with this program. If not, see https://www.gnu.org/licenses/.

#pragma once

#include <libSouls/Souls.hpp>
#include <libSouls/BinaryReader.hpp>
#include <libSouls/BinaryWriter.hpp>

#include <cstdint>
#include <string>
#include <type_traits>

/// @brief Shared pieces of the BND/BXF container formats: the format flags stored in headers and per-file flags.
namespace Souls::Binder {
    /// @brief Which optional fields a binder's headers contain.
    ///
    /// Stored bit-reversed in little-endian binders; see ReadFormat/WriteFormat.
    enum class Format : uint8_t {
        None        = 0,
        BigEndian   = 0b0000'0001,
        IDs         = 0b0000'0010,
        Names1      = 0b0000'0100,
        Names2      = 0b0000'1000,
        LongOffsets = 0b0001'0000,
        Compression = 0b0010'0000,
        Flag6       = 0b0100'0000,
        Flag7       = 0b1000'0000,
    };

    /// @brief Per-file flags in a binder's file headers. Only Compressed is understood; the others are kept as read.
    enum class FileFlags : uint8_t {
        None       = 0,
        Compressed = 0b0000'0001,
        Flag1      = 0b0000'0010,
        Flag2      = 0b0000'0100,
        Flag3      = 0b0000'1000,
        Flag4      = 0b0001'0000,
        Flag5      = 0b0010'0000,
        Flag6      = 0b0100'0000,
        Flag7      = 0b1000'0000,
    };

    template<typename E>
    concept BinderFlags = std::is_same_v<E, Format> || std::is_same_v<E, FileFlags>;

    /// @brief Combines flags.
    template<BinderFlags E>
    constexpr E operator|(E A, E B) {
        return static_cast<E>(static_cast<uint8_t>(A) | static_cast<uint8_t>(B));
    }

    /// @brief The flags set in both.
    template<BinderFlags E>
    constexpr E operator&(E A, E B) {
        return static_cast<E>(static_cast<uint8_t>(A) & static_cast<uint8_t>(B));
    }

    /// @brief Whether any of Flags is set in Value.
    template<BinderFlags E>
    constexpr bool HasAny(E Value, E Flags) {
        return (Value & Flags) != static_cast<E>(0);
    }

#pragma region Format
    /// @brief The current time as a binder version/timestamp string (what new binders default to).
    SOULS_API std::string CurrentVersion();

    /// @brief Reads the format byte.
    ///
    /// BitBigEndian says whether the flag bits are stored in big-endian bit order.
    SOULS_API Format ReadFormat(BinaryReader& Reader, bool BitBigEndian);
    /// @brief Writes the format byte (see ReadFormat).
    SOULS_API void WriteFormat(BinaryWriter& Writer, bool BitBigEndian, Format Value);

    /// @brief Whether the format forces big-endian values.
    constexpr bool ForceBigEndian(Format Value) {
        return HasAny(Value, Format::BigEndian);
    }
    /// @brief Whether file headers store IDs.
    constexpr bool HasIDs(Format Value) {
        return HasAny(Value, Format::IDs);
    }
    /// @brief Whether file headers store names.
    constexpr bool HasNames(Format Value) {
        return HasAny(Value, Format::Names1 | Format::Names2);
    }
    /// @brief Whether file data offsets are 64-bit.
    constexpr bool HasLongOffsets(Format Value) {
        return HasAny(Value, Format::LongOffsets);
    }
    /// @brief Whether file headers store uncompressed sizes (files may be compressed).
    constexpr bool HasCompression(Format Value) {
        return HasAny(Value, Format::Compression);
    }
    /// @brief Whether unknown format flag 6 is set.
    constexpr bool HasFlag6(Format Value) {
        return HasAny(Value, Format::Flag6);
    }
    /// @brief Whether unknown format flag 7 is set.
    constexpr bool HasFlag7(Format Value) {
        return HasAny(Value, Format::Flag7);
    }

    /// @brief Size in bytes of one file header in a BND4 with this format.
    constexpr int64_t GetBND4FileHeaderSize(Format Value) {
        return 0x10 + (HasLongOffsets(Value) ? 8 : 4) + (HasCompression(Value) ? 8 : 0) + (HasIDs(Value) ? 4 : 0) +
               (HasNames(Value) ? 4 : 0) + (Value == Format::Names1 ? 8 : 0);
    }
#pragma endregion

#pragma region FileFlags
    /// @brief Reads a file's flag byte.
    SOULS_API FileFlags ReadFileFlags(BinaryReader& Reader, bool BitBigEndian);
    /// @brief Writes a file's flag byte.
    SOULS_API void WriteFileFlags(BinaryWriter& Writer, bool BitBigEndian, FileFlags Value);

    /// @brief Whether the flags mark a file as compressed.
    constexpr bool IsCompressed(FileFlags Value) {
        return HasAny(Value, FileFlags::Compressed);
    }
#pragma endregion
}  // namespace Souls::Binder
