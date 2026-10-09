//
// Created by Jake Rieger on 10/7/2026.
//

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

    // Writes FromSoftware-style binary data to any std::ostream (file, memory buffer, ...), with a switchable
    // byte order. Supports "reservations": placeholder values (offsets, sizes, counts) that are filled in later once
    // their real value is known, which most FromSoft formats require.
    class SOULS_API BinaryWriter {
    public:
        // Writes to a file (binary mode, truncating).
        explicit BinaryWriter(const std::filesystem::path& Path, Endian Order = Endian::Little);
        // Writes to an in-memory buffer; retrieve the result with ToBytes().
        explicit BinaryWriter(Endian Order = Endian::Little);
        // Wraps a caller-owned stream. The stream must be seekable and outlive the writer.
        explicit BinaryWriter(std::ostream& Stream, Endian Order = Endian::Little);

        ~BinaryWriter();
        BinaryWriter(const BinaryWriter&)            = delete;
        BinaryWriter& operator=(const BinaryWriter&) = delete;
        BinaryWriter(BinaryWriter&&)                 = delete;
        BinaryWriter& operator=(BinaryWriter&&)      = delete;

#pragma region State
        // Byte order used for all multi-byte writes. May be changed mid-write.
        Endian Order = Endian::Little;
        // Whether WriteVarint writes 64-bit (true) or 32-bit (false) values.
        bool VarintLong = false;
#pragma endregion

#pragma region Position
        int64_t Position();
        int64_t Length();
        void Seek(int64_t Position, std::ios_base::seekdir Origin = std::ios_base::beg);
        // Writes Count copies of Value.
        void Pad(size_t Count, uint8_t Value = 0);
        // Pads with Value up to the next multiple of Alignment (no-op if already aligned).
        void Align(int64_t Alignment, uint8_t Value = 0);
        // Same, but alignment is measured from Start rather than from the beginning of the stream.
        void AlignRelative(int64_t Start, int64_t Alignment, uint8_t Value = 0);
#pragma endregion

#pragma region Vectors
        void WriteVector2(const Vector2& Value);
        void WriteVector3(const Vector3& Value);
        void WriteVector4(const Vector4& Value);
#pragma endregion

#pragma region Scalars
        template<Scalar T>
        void Write(T Value) {
            const T Converted = ToHost(Value, Order);
            WriteRaw(&Converted, sizeof(T));
        }

        void WriteBool(bool Value) { Write<uint8_t>(Value ? 1 : 0); }
        void WriteByte(uint8_t Value) { Write(Value); }
        void WriteSByte(int8_t Value) { Write(Value); }
        void WriteInt16(int16_t Value) { Write(Value); }
        void WriteUInt16(uint16_t Value) { Write(Value); }
        void WriteInt32(int32_t Value) { Write(Value); }
        void WriteUInt32(uint32_t Value) { Write(Value); }
        void WriteInt64(int64_t Value) { Write(Value); }
        void WriteUInt64(uint64_t Value) { Write(Value); }
        void WriteFloat(float Value) { Write(Value); }
        void WriteDouble(double Value) { Write(Value); }
        // 32- or 64-bit signed value depending on VarintLong. Throws if it doesn't fit in 32 bits.
        void WriteVarint(int64_t Value);
#pragma endregion

#pragma region Arrays
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

        template<Scalar T>
        void WriteArray(const std::vector<T>& Values) {
            WriteArray(std::span<const T>(Values));
        }

        void WriteBytes(std::span<const uint8_t> Bytes) { WriteArray(Bytes); }
#pragma endregion

#pragma region Strings
        // Raw bytes, optionally followed by a 0 terminator. No codepage conversion (pass pre-encoded Shift-JIS as is).
        void WriteString(std::string_view Value, bool NullTerminate = true);
        // UTF-16 code units in the current byte order, optionally followed by a 0 terminator.
        void WriteUTF16(std::u16string_view Value, bool NullTerminate = true);
        // Exactly Length bytes: Value truncated or zero-padded as needed. No terminator is guaranteed.
        void WriteFixedString(std::string_view Value, size_t Length);
        // UTF-8 text written as UTF-16 in the current byte order, optionally null-terminated.
        void WriteUTF16Text(std::string_view Utf8Value, bool NullTerminate = true);
        // UTF-8 text encoded as Shift-JIS, optionally null-terminated.
        void WriteShiftJIS(std::string_view Utf8Value, bool NullTerminate = true);
        // Fixed-size field of Size bytes: UTF-8 text as Shift-JIS, then a null terminator if it fits, then PadByte
        // for the rest. Truncated if too long.
        void WriteFixStr(std::string_view Utf8Value, size_t Size, uint8_t PadByte = 0);
        // Fixed-size field of Size bytes: UTF-16 text, then a null terminator if it fits, then PadByte for the rest.
        // Truncated (to whole units) if too long.
        void WriteFixStrW(std::u16string_view Value, size_t Size, uint8_t PadByte = 0);
        // Writes the bytes of Magic with no terminator (build a string_view with explicit size to include a \0).
        void WriteMagic(std::string_view Magic) { WriteString(Magic, false); }
#pragma endregion

#pragma region Reservations
        // Writes a zeroed placeholder of T and remembers it under Name, to be filled in later with Fill().
        template<Scalar T>
        void Reserve(const std::string& Name) {
            ReserveRaw(Name, sizeof(T));
        }

        // Writes Value into the placeholder named Name (in the current Order), then restores the position.
        template<Scalar T>
        void Fill(const std::string& Name, T Value) {
            const T Converted = ToHost(Value, Order);
            FillRaw(Name, &Converted, sizeof(T));
        }

        // Varint-sized placeholder (32 or 64-bit depending on VarintLong at the time of reservation).
        void ReserveVarint(const std::string& Name);
        void FillVarint(const std::string& Name, int64_t Value);
#pragma endregion

        // In-memory mode only: a copy of bytes already written (including filled reservations).
        std::vector<uint8_t> ReadBack(int64_t Position, size_t Count);

        // Flushes the stream and verifies every reservation was filled. Throws otherwise.
        void Finish();
        // In-memory mode only: Finish() and return everything written.
        std::vector<uint8_t> ToBytes();

    private:
        struct Reservation {
            int64_t Position;
            size_t Size;
        };

        void WriteRaw(const void* Source, size_t Size);
        void ReserveRaw(const std::string& Name, size_t Size);
        void FillRaw(const std::string& Name, const void* Source, size_t Size);

        std::unique_ptr<std::ostream> Owned;  // stream we created, if any
        std::ostream* Stream = nullptr;       // what all writes go through
        bool IsMemory        = false;
        std::unordered_map<std::string, Reservation> Reservations;
    };

#pragma warning(pop)

}  // namespace Souls
