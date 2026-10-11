// Copyright (C) 2026 Jake Rieger
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, version 3. This program is distributed in the hope that it will
// be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU General
// Public License along with this program. If not, see https://www.gnu.org/licenses/.

#include "Binder.hpp"

#include <libSouls/Util.hpp>

#include <chrono>

namespace Souls::Binder {
    std::string CurrentVersion() {
        std::string Version =
          Util::DateToBinderTimestamp(std::chrono::floor<std::chrono::minutes>(std::chrono::system_clock::now()));
        // Trailing NULs of the fixed-width timestamp aren't part of the string.
        Version.resize(Version.find('\0') == std::string::npos ? Version.size() : Version.find('\0'));
        return Version;
    }

    Format ReadFormat(BinaryReader& Reader, bool BitBigEndian) {
        const uint8_t Raw = Reader.ReadByte();
        // Little-endian binders store the flags bit-reversed, but the big-endian flag itself tells us when a binder
        // is actually in the other order.
        const bool Reverse = BitBigEndian || ((Raw & 1) != 0 && (Raw & 0b1000'0000) == 0);
        return static_cast<Format>(Reverse ? Raw : Util::ReverseBits(Raw));
    }

    void WriteFormat(BinaryWriter& Writer, bool BitBigEndian, Format Value) {
        const bool Reverse = BitBigEndian || ForceBigEndian(Value);
        const auto Raw     = static_cast<uint8_t>(Value);
        Writer.WriteByte(Reverse ? Raw : Util::ReverseBits(Raw));
    }

    FileFlags ReadFileFlags(BinaryReader& Reader, bool BitBigEndian) {
        const uint8_t Raw = Reader.ReadByte();
        return static_cast<FileFlags>(BitBigEndian ? Raw : Util::ReverseBits(Raw));
    }

    void WriteFileFlags(BinaryWriter& Writer, bool BitBigEndian, FileFlags Value) {
        const auto Raw = static_cast<uint8_t>(Value);
        Writer.WriteByte(BitBigEndian ? Raw : Util::ReverseBits(Raw));
    }
}  // namespace Souls::Binder
