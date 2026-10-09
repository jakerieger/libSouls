//
// Created by Jake Rieger on 10/7/2026.
//

#pragma once

#include "Souls.hpp"
#include "Endian.hpp"
#include "Vector.hpp"

#include <cstdint>
#include <filesystem>
#include <istream>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of an exported class; only touched by this DLL's own code

    // Reads FromSoftware-style binary data from any std::istream (file, memory buffer, ...), with a switchable
    // byte order. All reads throw BinaryException on failure rather than returning partial data.
    class SOULS_API BinaryReader {
    public:
        // Opens a file (binary mode).
        explicit BinaryReader(const std::filesystem::path& Path, Endian Order = Endian::Little);
        // Takes ownership of the buffer.
        explicit BinaryReader(std::vector<uint8_t>&& Data, Endian Order = Endian::Little);
        // Views a caller-owned buffer without copying. The buffer must outlive the reader.
        explicit BinaryReader(std::span<const uint8_t> Data, Endian Order = Endian::Little);
        // Wraps a caller-owned stream. The stream must be seekable and outlive the reader.
        explicit BinaryReader(std::istream& Stream, Endian Order = Endian::Little);

        ~BinaryReader();
        BinaryReader(const BinaryReader&)            = delete;
        BinaryReader& operator=(const BinaryReader&) = delete;
        BinaryReader(BinaryReader&&)                 = delete;
        BinaryReader& operator=(BinaryReader&&)      = delete;

#pragma region State
        // Byte order used for all multi-byte reads. May be changed mid-parse (e.g. after reading a BOM).
        Endian Order = Endian::Little;
        // Whether ReadVarint reads 64-bit (true) or 32-bit (false) values.
        bool VarintLong = false;
#pragma endregion

#pragma region Position
        int64_t Position();
        int64_t Length();
        int64_t Remaining();
        bool Eof();
        void Seek(int64_t Position, std::ios_base::seekdir Origin = std::ios_base::beg);
        void Skip(int64_t Count);
        // Skips forward to the next multiple of Alignment (no-op if already aligned).
        void Align(int64_t Alignment);
        // Same, but alignment is measured from Start rather than from the beginning of the stream.
        void AlignRelative(int64_t Start, int64_t Alignment);
        // Jumps to Position, remembering where we were. StepOut returns to the last remembered position.
        void StepIn(int64_t Position);
        void StepOut();
#pragma endregion

#pragma region Scalars
        template<Scalar T>
        T Read() {
            T Value;
            ReadRaw(&Value, sizeof(T));
            return ToHost(Value, Order);
        }

        // Reads a value at an absolute position, leaving the current position unchanged.
        template<Scalar T>
        T ReadAt(int64_t Position) {
            StepIn(Position);
            T Value = Read<T>();
            StepOut();
            return Value;
        }

        // Reads the next value without consuming it.
        template<Scalar T>
        T Peek() {
            const int64_t Start = Position();
            T Value             = Read<T>();
            Seek(Start);
            return Value;
        }

        bool ReadBool();  // throws unless the byte is 0 or 1
        uint8_t ReadByte() { return Read<uint8_t>(); }
        int8_t ReadSByte() { return Read<int8_t>(); }
        int16_t ReadInt16() { return Read<int16_t>(); }
        uint16_t ReadUInt16() { return Read<uint16_t>(); }
        int32_t ReadInt32() { return Read<int32_t>(); }
        uint32_t ReadUInt32() { return Read<uint32_t>(); }
        int64_t ReadInt64() { return Read<int64_t>(); }
        uint64_t ReadUInt64() { return Read<uint64_t>(); }
        float ReadFloat() { return Read<float>(); }
        double ReadDouble() { return Read<double>(); }
        // 32- or 64-bit signed value depending on VarintLong.
        int64_t ReadVarint();
#pragma endregion

#pragma region Arrays
        template<Scalar T>
        std::vector<T> ReadArray(size_t Count) {
            CheckAvailable(Count, sizeof(T));
            std::vector<T> Values(Count);
            ReadRaw(Values.data(), Count * sizeof(T));
            if (Order != HostEndian) {
                for (T& Value : Values) {
                    Value = ByteSwap(Value);
                }
            }
            return Values;
        }

        // Fills the whole span.
        template<Scalar T>
        void ReadInto(std::span<T> Destination) {
            ReadRaw(Destination.data(), Destination.size_bytes());
            if (Order != HostEndian) {
                for (T& Value : Destination) {
                    Value = ByteSwap(Value);
                }
            }
        }

        std::vector<uint8_t> ReadBytes(size_t Count) { return ReadArray<uint8_t>(Count); }
#pragma endregion

#pragma region Strings
        // Exactly Count raw bytes as a string (no terminator handling, no codepage conversion).
        std::string ReadString(size_t Count);
        // Bytes up to and including a 0 terminator; the terminator is not returned. Raw bytes, so this serves ASCII
        // and Shift-JIS alike (convert after the fact if needed).
        std::string ReadCString();
        // UTF-16 code units in the current byte order, up to and including a 0 terminator.
        std::u16string ReadUTF16();
        // Exactly Count UTF-16 code units, no terminator handling.
        std::u16string ReadUTF16(size_t Count);
        // Shift-JIS text converted to UTF-8, null-terminated or exactly Count bytes.
        std::string ReadShiftJIS();
        std::string ReadShiftJIS(size_t Count);
        // Fixed-size field of Size bytes holding Shift-JIS text (converted to UTF-8) up to the first 0. Always
        // consumes Size bytes.
        std::string ReadFixStr(size_t Size);
        // Fixed-size field of Size bytes holding UTF-16 up to the first 0. Always consumes Size bytes.
        std::u16string ReadFixStrW(size_t Size);
#pragma endregion

#pragma region Vectors
        // Convenience readers for text at an absolute position (position is unchanged afterwards). UTF-16 text and
        // Shift-JIS text are returned as UTF-8.
        std::string GetUTF16Text(int64_t Offset);
        std::string GetShiftJIS(int64_t Offset);
        std::string GetASCII(int64_t Offset, size_t Count);
        // Null-terminated raw bytes (ASCII) at an absolute position.
        std::string GetCString(int64_t Offset);
        // Null-terminated UTF-16 at the current position, returned as UTF-8.
        std::string ReadUTF16Text();
        Vector2 ReadVector2();
        Vector3 ReadVector3();
        Vector4 ReadVector4();
#pragma endregion

#pragma region Assertions
        // Reads a value and throws unless it equals one of the given options (e.g. Assert<uint8_t>(0x01, 0x5E)).
        template<Scalar T, typename... Rest>
        T Assert(T First, Rest... Others) {
            const int64_t At = Position();
            const T Actual   = Read<T>();
            if (Actual != First && ((Actual != static_cast<T>(Others)) && ...)) {
                ThrowAssert(At,
                            Describe(First) + (std::string{} + ... + (", " + Describe(static_cast<T>(Others)))),
                            Describe(Actual));
            }
            return Actual;
        }

        // Same as Assert, but for a varint (sized by VarintLong).
        template<typename... Options>
        int64_t AssertVarint(Options... Allowed) {
            const int64_t At     = Position();
            const int64_t Actual = ReadVarint();
            if (((Actual != static_cast<int64_t>(Allowed)) && ...)) {
                ThrowAssert(At,
                            (std::string{} + ... + (Describe(static_cast<int64_t>(Allowed)) + " ")),
                            Describe(Actual));
            }
            return Actual;
        }

        // Reads Magic.size() bytes and throws on mismatch (e.g. "BND4", "DCX\0").
        void AssertMagic(std::string_view Magic);
        // Reads Count bytes and throws if any differ from Value (typically padding).
        void AssertPattern(size_t Count, uint8_t Value);
#pragma endregion

    private:
        void InitMemory(const uint8_t* Data, size_t Size);
        void ReadRaw(void* Destination, size_t Size);
        void CheckAvailable(size_t Count, size_t ElementSize);
        [[noreturn]] static void ThrowAssert(int64_t At, const std::string& Expected, const std::string& Actual);

        template<Scalar T>
        static std::string Describe(T Value) {
            if constexpr (std::is_enum_v<T>) {
                return std::to_string(static_cast<std::underlying_type_t<T>>(Value));
            } else {
                return std::to_string(Value);
            }
        }

        std::vector<uint8_t> Storage;           // backing for the owned-buffer ctor
        std::unique_ptr<std::streambuf> Buffer;  // memory-backed stream buffer, if any
        std::unique_ptr<std::istream> Owned;     // stream we created, if any
        std::istream* Stream = nullptr;          // what all reads go through
        std::vector<int64_t> StepStack;
    };

#pragma warning(pop)

}  // namespace Souls
