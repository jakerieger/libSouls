//
// Created by Jake Rieger on 10/8/2026.
//

#pragma once

#include "BinaryReader.hpp"
#include "BinaryWriter.hpp"

#include <cstdint>

namespace Souls {

    // An 8-bit-per-channel color.
    struct Color {
        uint8_t A = 0, R = 0, G = 0, B = 0;

        bool operator==(const Color&) const = default;

        static constexpr Color FromArgb(uint8_t A, uint8_t R, uint8_t G, uint8_t B) { return {A, R, G, B}; }
        static constexpr Color White() { return {255, 255, 255, 255}; }
        static constexpr Color Black() { return {255, 0, 0, 0}; }
    };

    // Colors are stored in files as bytes in a variety of orders.
    inline Color ReadRGB(BinaryReader& Reader) {
        const uint8_t R = Reader.ReadByte(), G = Reader.ReadByte(), B = Reader.ReadByte();
        return {255, R, G, B};
    }
    inline Color ReadRGBA(BinaryReader& Reader) {
        const uint8_t R = Reader.ReadByte(), G = Reader.ReadByte(), B = Reader.ReadByte(), A = Reader.ReadByte();
        return {A, R, G, B};
    }
    inline Color ReadARGB(BinaryReader& Reader) {
        const uint8_t A = Reader.ReadByte(), R = Reader.ReadByte(), G = Reader.ReadByte(), B = Reader.ReadByte();
        return {A, R, G, B};
    }
    inline Color ReadBGRA(BinaryReader& Reader) {
        const uint8_t B = Reader.ReadByte(), G = Reader.ReadByte(), R = Reader.ReadByte(), A = Reader.ReadByte();
        return {A, R, G, B};
    }
    inline void WriteRGB(BinaryWriter& Writer, const Color& C) {
        Writer.WriteByte(C.R);
        Writer.WriteByte(C.G);
        Writer.WriteByte(C.B);
    }
    inline void WriteRGBA(BinaryWriter& Writer, const Color& C) {
        Writer.WriteByte(C.R);
        Writer.WriteByte(C.G);
        Writer.WriteByte(C.B);
        Writer.WriteByte(C.A);
    }
    inline void WriteARGB(BinaryWriter& Writer, const Color& C) {
        Writer.WriteByte(C.A);
        Writer.WriteByte(C.R);
        Writer.WriteByte(C.G);
        Writer.WriteByte(C.B);
    }
    inline void WriteBGRA(BinaryWriter& Writer, const Color& C) {
        Writer.WriteByte(C.B);
        Writer.WriteByte(C.G);
        Writer.WriteByte(C.R);
        Writer.WriteByte(C.A);
    }

}  // namespace Souls
