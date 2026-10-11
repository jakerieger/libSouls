// Copyright (C) 2026 Jake Rieger
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, version 3. This program is distributed in the hope that it will
// be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU General
// Public License along with this program. If not, see https://www.gnu.org/licenses/.

#pragma once

#include "BinaryReader.hpp"
#include "BinaryWriter.hpp"

#include <cstdint>

namespace Souls {

    /// @brief An 8-bit-per-channel color.
    struct Color {
        uint8_t A = 0;  ///< Alpha (255 is opaque).
        uint8_t R = 0;  ///< Red.
        uint8_t G = 0;  ///< Green.
        uint8_t B = 0;  ///< Blue.

        /// @brief Channel-wise equality.
        bool operator==(const Color&) const = default;

        /// @brief A color from its alpha, red, green and blue channels.
        static constexpr Color FromArgb(uint8_t A, uint8_t R, uint8_t G, uint8_t B) { return {A, R, G, B}; }
        /// @brief Opaque white.
        static constexpr Color White() { return {255, 255, 255, 255}; }
        /// @brief Opaque black.
        static constexpr Color Black() { return {255, 0, 0, 0}; }
    };

    /// @brief Reads a color stored as red, green, blue bytes (alpha is 255).
    ///
    /// Files store colors as bytes in a variety of orders; there is a reader and writer for each.
    inline Color ReadRGB(BinaryReader& Reader) {
        const uint8_t R = Reader.ReadByte(), G = Reader.ReadByte(), B = Reader.ReadByte();
        return {255, R, G, B};
    }
    /// @brief Reads a color stored as red, green, blue, alpha bytes.
    inline Color ReadRGBA(BinaryReader& Reader) {
        const uint8_t R = Reader.ReadByte(), G = Reader.ReadByte(), B = Reader.ReadByte(), A = Reader.ReadByte();
        return {A, R, G, B};
    }
    /// @brief Reads a color stored as alpha, red, green, blue bytes.
    inline Color ReadARGB(BinaryReader& Reader) {
        const uint8_t A = Reader.ReadByte(), R = Reader.ReadByte(), G = Reader.ReadByte(), B = Reader.ReadByte();
        return {A, R, G, B};
    }
    /// @brief Reads a color stored as blue, green, red, alpha bytes.
    inline Color ReadBGRA(BinaryReader& Reader) {
        const uint8_t B = Reader.ReadByte(), G = Reader.ReadByte(), R = Reader.ReadByte(), A = Reader.ReadByte();
        return {A, R, G, B};
    }
    /// @brief Writes a color as red, green, blue bytes (alpha is dropped).
    inline void WriteRGB(BinaryWriter& Writer, const Color& C) {
        Writer.WriteByte(C.R);
        Writer.WriteByte(C.G);
        Writer.WriteByte(C.B);
    }
    /// @brief Writes a color as red, green, blue, alpha bytes.
    inline void WriteRGBA(BinaryWriter& Writer, const Color& C) {
        Writer.WriteByte(C.R);
        Writer.WriteByte(C.G);
        Writer.WriteByte(C.B);
        Writer.WriteByte(C.A);
    }
    /// @brief Writes a color as alpha, red, green, blue bytes.
    inline void WriteARGB(BinaryWriter& Writer, const Color& C) {
        Writer.WriteByte(C.A);
        Writer.WriteByte(C.R);
        Writer.WriteByte(C.G);
        Writer.WriteByte(C.B);
    }
    /// @brief Writes a color as blue, green, red, alpha bytes.
    inline void WriteBGRA(BinaryWriter& Writer, const Color& C) {
        Writer.WriteByte(C.B);
        Writer.WriteByte(C.G);
        Writer.WriteByte(C.R);
        Writer.WriteByte(C.A);
    }

}  // namespace Souls
