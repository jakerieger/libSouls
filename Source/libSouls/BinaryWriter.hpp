// Copyright (C) 2026 Jake Rieger
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, version 3. This program is distributed in the hope that it will
// be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU General
// Public License along with this program. If not, see https://www.gnu.org/licenses/.

#pragma once

#include "Souls.hpp"
#include "Endian.hpp"
#include "Vector.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of an exported class; only touched by this DLL's own code

    /// @brief Writes FromSoftware-style binary data to any std::ostream (file, memory buffer, ...), with a switchable
    ///        byte order.
    ///
    /// Supports "reservations": placeholder values (offsets, sizes, counts) that are filled in later once their real
    /// value is known, which most FromSoft formats require.
    class SOULS_API BinaryWriter {
    public:
        /// @brief Writes to a file (binary mode, truncating).
        explicit BinaryWriter(const std::filesystem::path& Path, Endian Order = Endian::Little);
        /// @brief Writes to an in-memory buffer; retrieve the result with ToBytes().
        explicit BinaryWriter(Endian Order = Endian::Little);
        /// @brief Wraps a caller-owned stream.
        ///
        /// The stream must be seekable and outlive the writer.
        explicit BinaryWriter(std::ostream& Stream, Endian Order = Endian::Little);

        ~BinaryWriter();
        BinaryWriter(const BinaryWriter&)            = delete;
        BinaryWriter& operator=(const BinaryWriter&) = delete;
        BinaryWriter(BinaryWriter&&)                 = delete;
        BinaryWriter& operator=(BinaryWriter&&)      = delete;

#pragma region State
        /// @brief Byte order used for all multi-byte writes.
        ///
        /// May be changed mid-write.
        Endian Order = Endian::Little;
        /// @brief Whether WriteVarint writes 64-bit (true) or 32-bit (false) values.
        bool VarintLong = false;
#pragma endregion

#pragma region Position
        /// @brief The current position, in bytes from the start.
        int64_t Position();
        /// @brief How many bytes have been written in total.
        int64_t Length();
        /// @brief Moves to a position, measured from the start, the current position or the end.
        void Seek(int64_t Position, std::ios_base::seekdir Origin = std::ios_base::beg);
        /// @brief Writes Count copies of Value.
        void Pad(size_t Count, uint8_t Value = 0);
        /// @brief Pads with Value up to the next multiple of Alignment (no-op if already aligned).
        void Align(int64_t Alignment, uint8_t Value = 0);
        /// @brief Same, but alignment is measured from Start rather than from the beginning of the stream.
        void AlignRelative(int64_t Start, int64_t Alignment, uint8_t Value = 0);
#pragma endregion

#pragma region Vectors
        void WriteVector2(const Vector2& Value);  ///< Writes two floats.
        void WriteVector3(const Vector3& Value);  ///< Writes three floats.
        void WriteVector4(const Vector4& Value);  ///< Writes four floats.
#pragma endregion

#pragma region Scalars
        /// @brief Writes one value in the current byte order.
        template<Scalar T>
        void Write(T Value) {
            const T Converted = ToHost(Value, Order);
            WriteRaw(&Converted, sizeof(T));
        }

        void WriteBool(bool Value) { Write<uint8_t>(Value ? 1 : 0); }  ///< Writes a bool as one byte, 0 or 1.
        void WriteByte(uint8_t Value) { Write(Value); }                ///< Writes an unsigned 8-bit integer.
        void WriteSByte(int8_t Value) { Write(Value); }                ///< Writes a signed 8-bit integer.
        void WriteInt16(int16_t Value) { Write(Value); }               ///< Writes a signed 16-bit integer.
        void WriteUInt16(uint16_t Value) { Write(Value); }             ///< Writes an unsigned 16-bit integer.
        void WriteInt32(int32_t Value) { Write(Value); }               ///< Writes a signed 32-bit integer.
        void WriteUInt32(uint32_t Value) { Write(Value); }             ///< Writes an unsigned 32-bit integer.
        void WriteInt64(int64_t Value) { Write(Value); }               ///< Writes a signed 64-bit integer.
        void WriteUInt64(uint64_t Value) { Write(Value); }             ///< Writes an unsigned 64-bit integer.
        void WriteFloat(float Value) { Write(Value); }                 ///< Writes a 32-bit float.
        void WriteDouble(double Value) { Write(Value); }               ///< Writes a 64-bit float.
        /// @brief 32- or 64-bit signed value depending on VarintLong.
        ///
        /// Throws if it doesn't fit in 32 bits.
        void WriteVarint(int64_t Value);
#pragma endregion

#pragma region Arrays
        /// @brief Writes each value in the current byte order.
        template<Scalar T>
        void WriteArray(std::span<const T> Values) {
            if (Order == HostEndian || sizeof(T) == 1) {
                WriteRaw(Values.data(), Values.size_bytes());
                return;
            }
            std::vector<T> Swapped(Values.begin(), Values.end());
            for (T& Value : Swapped) {
                Value = ByteSwap(Value);
            }
            WriteRaw(Swapped.data(), Swapped.size() * sizeof(T));
        }

        /// @brief Writes each value in the current byte order.
        template<Scalar T>
        void WriteArray(const std::vector<T>& Values) {
            WriteArray(std::span<const T>(Values));
        }

        /// @brief Writes raw bytes.
        void WriteBytes(std::span<const uint8_t> Bytes) { WriteArray(Bytes); }
#pragma endregion

#pragma region Strings
        /// @brief Raw bytes, optionally followed by a 0 terminator.
        ///
        /// No codepage conversion (pass pre-encoded Shift-JIS as is).
        void WriteString(std::string_view Value, bool NullTerminate = true);
        /// @brief UTF-16 code units in the current byte order, optionally followed by a 0 terminator.
        void WriteUTF16(std::u16string_view Value, bool NullTerminate = true);
        /// @brief Exactly Length bytes: Value truncated or zero-padded as needed.
        ///
        /// No terminator is guaranteed.
        void WriteFixedString(std::string_view Value, size_t Length);
        /// @brief UTF-8 text written as UTF-16 in the current byte order, optionally null-terminated.
        void WriteUTF16Text(std::string_view Utf8Value, bool NullTerminate = true);
        /// @brief UTF-8 text encoded as Shift-JIS, optionally null-terminated.
        void WriteShiftJIS(std::string_view Utf8Value, bool NullTerminate = true);
        /// @brief Fixed-size field of Size bytes: UTF-8 text as Shift-JIS, then a null terminator if it fits, then
        ///        PadByte for the rest.
        ///
        /// Truncated if too long.
        void WriteFixStr(std::string_view Utf8Value, size_t Size, uint8_t PadByte = 0);
        /// @brief Fixed-size field of Size bytes: UTF-16 text, then a null terminator if it fits, then PadByte for the
        ///        rest.
        ///
        /// Truncated (to whole units) if too long.
        void WriteFixStrW(std::u16string_view Value, size_t Size, uint8_t PadByte = 0);
        /// @brief Writes the bytes of Magic with no terminator (build a string_view with explicit size to include a
        ///        \0).
        void WriteMagic(std::string_view Magic) { WriteString(Magic, false); }
#pragma endregion

#pragma region Reservations
        /// @brief Writes a zeroed placeholder of T and remembers it under Name, to be filled in later with Fill().
        template<Scalar T>
        void Reserve(const std::string& Name) {
            ReserveRaw(Name, sizeof(T));
        }

        /// @brief Writes Value into the placeholder named Name (in the current Order), then restores the position.
        template<Scalar T>
        void Fill(const std::string& Name, T Value) {
            const T Converted = ToHost(Value, Order);
            FillRaw(Name, &Converted, sizeof(T));
        }

        /// @brief Varint-sized placeholder (32 or 64-bit depending on VarintLong at the time of reservation).
        void ReserveVarint(const std::string& Name);
        /// @brief Fills a placeholder made by ReserveVarint.
        void FillVarint(const std::string& Name, int64_t Value);
#pragma endregion

        /// @brief In-memory mode only: a copy of bytes already written (including filled reservations).
        std::vector<uint8_t> ReadBack(int64_t Position, size_t Count);

        /// @brief Flushes the stream and verifies every reservation was filled.
        ///
        /// Throws otherwise.
        void Finish();
        /// @brief In-memory mode only: Finish() and return everything written.
        std::vector<uint8_t> ToBytes();

    private:
        struct Reservation {
            int64_t Position;
            size_t Size;
        };

        void WriteRaw(const void* Source, size_t Size);
        void ReserveRaw(const std::string& Name, size_t Size);
        void FillRaw(const std::string& Name, const void* Source, size_t Size);

        std::unique_ptr<std::ostream> _Owned;  ///< stream we created, if any
        std::ostream* _Stream = nullptr;       ///< what all writes go through
        bool _IsMemory        = false;
        std::unordered_map<std::string, Reservation> _Reservations;
    };

#pragma warning(pop)

}  // namespace Souls
