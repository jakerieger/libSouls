//
// Created by Jake Rieger on 10/7/2026.
//

#include "BinaryWriter.hpp"
#include "TextEncoding.hpp"

#include <algorithm>
#include <fstream>
#include <limits>
#include <sstream>

namespace Souls {
    BinaryWriter::BinaryWriter(const std::filesystem::path& Path, Endian Order) : Order(Order) {
        auto File = std::make_unique<std::ofstream>(Path, std::ios::binary | std::ios::trunc);
        if (!File->is_open()) {
            throw BinaryException("Failed to open file for writing: " + Path.string());
        }
        Stream = File.get();
        Owned  = std::move(File);
    }

    BinaryWriter::BinaryWriter(Endian Order) : Order(Order), IsMemory(true) {
        Owned  = std::make_unique<std::ostringstream>(std::ios::binary);
        Stream = Owned.get();
    }

    BinaryWriter::BinaryWriter(std::ostream& Stream, Endian Order) : Order(Order), Stream(&Stream) {}

    BinaryWriter::~BinaryWriter() = default;

#pragma region Position
    int64_t BinaryWriter::Position() {
        Stream->clear();
        const auto Pos = Stream->tellp();
        if (Pos == std::ostream::pos_type(-1)) {
            throw BinaryException("Failed to query stream position");
        }
        return static_cast<int64_t>(Pos);
    }

    int64_t BinaryWriter::Length() {
        const int64_t Current = Position();
        Stream->seekp(0, std::ios_base::end);
        const auto End = Stream->tellp();
        Stream->seekp(Current, std::ios_base::beg);
        if (End == std::ostream::pos_type(-1) || Stream->fail()) {
            throw BinaryException("Failed to query stream length");
        }
        return static_cast<int64_t>(End);
    }

    void BinaryWriter::Seek(int64_t Offset, std::ios_base::seekdir Origin) {
        Stream->clear();
        Stream->seekp(Offset, Origin);
        if (Stream->fail()) {
            Stream->clear();
            throw BinaryException("Seek out of range (offset " + std::to_string(Offset) + ")");
        }
    }

    void BinaryWriter::Pad(size_t Count, uint8_t Value) {
        constexpr size_t ChunkSize = 256;
        const std::vector<uint8_t> Chunk(std::min(Count, ChunkSize), Value);
        while (Count > 0) {
            const size_t N = std::min(Count, Chunk.size());
            WriteRaw(Chunk.data(), N);
            Count -= N;
        }
    }

    void BinaryWriter::Align(int64_t Alignment, uint8_t Value) {
        if (Alignment <= 0) {
            throw BinaryException("Alignment must be positive");
        }
        const int64_t Misalignment = Position() % Alignment;
        if (Misalignment != 0) {
            Pad(static_cast<size_t>(Alignment - Misalignment), Value);
        }
    }

    void BinaryWriter::AlignRelative(int64_t Start, int64_t Alignment, uint8_t Value) {
        if (Alignment <= 0) {
            throw BinaryException("Alignment must be positive");
        }
        const int64_t Misalignment = (Position() - Start) % Alignment;
        if (Misalignment != 0) {
            Pad(static_cast<size_t>(Alignment - Misalignment), Value);
        }
    }

    void BinaryWriter::WriteVector2(const Vector2& Value) {
        WriteFloat(Value.X);
        WriteFloat(Value.Y);
    }

    void BinaryWriter::WriteVector3(const Vector3& Value) {
        WriteFloat(Value.X);
        WriteFloat(Value.Y);
        WriteFloat(Value.Z);
    }

    void BinaryWriter::WriteVector4(const Vector4& Value) {
        WriteFloat(Value.X);
        WriteFloat(Value.Y);
        WriteFloat(Value.Z);
        WriteFloat(Value.W);
    }
#pragma endregion

#pragma region Scalars
    void BinaryWriter::WriteVarint(int64_t Value) {
        if (VarintLong) {
            WriteInt64(Value);
            return;
        }
        if (Value < std::numeric_limits<int32_t>::min() || Value > std::numeric_limits<int32_t>::max()) {
            throw BinaryException("Varint value " + std::to_string(Value) + " does not fit in 32 bits");
        }
        WriteInt32(static_cast<int32_t>(Value));
    }
#pragma endregion

#pragma region Strings
    void BinaryWriter::WriteString(std::string_view Value, bool NullTerminate) {
        WriteRaw(Value.data(), Value.size());
        if (NullTerminate) {
            WriteByte(0);
        }
    }

    void BinaryWriter::WriteUTF16(std::u16string_view Value, bool NullTerminate) {
        WriteArray(std::span<const char16_t>(Value.data(), Value.size()));
        if (NullTerminate) {
            Write<char16_t>(0);
        }
    }

    void BinaryWriter::WriteFixedString(std::string_view Value, size_t Length) {
        const size_t N = std::min(Value.size(), Length);
        WriteRaw(Value.data(), N);
        Pad(Length - N);
    }

    void BinaryWriter::WriteShiftJIS(std::string_view Utf8Value, bool NullTerminate) {
        WriteString(Text::UTF8ToShiftJIS(Utf8Value), NullTerminate);
    }

    void BinaryWriter::WriteFixStr(std::string_view Utf8Value, size_t Size, uint8_t PadByte) {
        const std::string Encoded = Text::UTF8ToShiftJIS(Utf8Value);
        const size_t Used         = std::min(Encoded.size(), Size);
        WriteRaw(Encoded.data(), Used);
        if (Used < Size) {
            WriteByte(0);  // terminator
            Pad(Size - Used - 1, PadByte);
        }
    }

    void BinaryWriter::WriteFixStrW(std::u16string_view Value, size_t Size, uint8_t PadByte) {
        const size_t Capacity = Size / 2;
        const size_t Units    = std::min(Value.size(), Capacity);
        WriteArray(std::span<const char16_t>(Value.data(), Units));
        size_t Written = Units * 2;
        if (Units < Capacity) {
            Write<char16_t>(0);  // terminator
            Written += 2;
        }
        Pad(Size - Written, PadByte);
    }
#pragma endregion

#pragma region Reservations
    void BinaryWriter::ReserveVarint(const std::string& Name) {
        ReserveRaw(Name, VarintLong ? sizeof(int64_t) : sizeof(int32_t));
    }

    void BinaryWriter::FillVarint(const std::string& Name, int64_t Value) {
        const auto It = Reservations.find(Name);
        if (It == Reservations.end()) {
            throw BinaryException("No pending reservation named \"" + Name + "\"");
        }
        // The placeholder's size, not the current VarintLong, decides the width.
        if (It->second.Size == sizeof(int64_t)) {
            Fill<int64_t>(Name, Value);
        } else {
            if (Value < std::numeric_limits<int32_t>::min() || Value > std::numeric_limits<int32_t>::max()) {
                throw BinaryException("Varint value " + std::to_string(Value) + " does not fit in 32 bits");
            }
            Fill<int32_t>(Name, static_cast<int32_t>(Value));
        }
    }

    void BinaryWriter::ReserveRaw(const std::string& Name, size_t Size) {
        if (Reservations.contains(Name)) {
            throw BinaryException("Reservation \"" + Name + "\" already exists");
        }
        Reservations[Name] = {Position(), Size};
        Pad(Size);
    }

    void BinaryWriter::FillRaw(const std::string& Name, const void* Source, size_t Size) {
        const auto It = Reservations.find(Name);
        if (It == Reservations.end()) {
            throw BinaryException("No pending reservation named \"" + Name + "\"");
        }
        if (It->second.Size != Size) {
            throw BinaryException("Reservation \"" + Name + "\" is " + std::to_string(It->second.Size) +
                                  " bytes, but a " + std::to_string(Size) + "-byte value was supplied");
        }
        const int64_t Return = Position();
        Seek(It->second.Position);
        WriteRaw(Source, Size);
        Seek(Return);
        Reservations.erase(It);
    }
#pragma endregion

    void BinaryWriter::Finish() {
        if (!Reservations.empty()) {
            throw BinaryException("Unfilled reservation: \"" + Reservations.begin()->first + "\" (" +
                                  std::to_string(Reservations.size()) + " pending)");
        }
        Stream->flush();
        if (Stream->fail()) {
            throw BinaryException("Failed to flush stream");
        }
    }

    std::vector<uint8_t> BinaryWriter::ToBytes() {
        if (!IsMemory) {
            throw BinaryException("ToBytes is only available on in-memory writers");
        }
        Finish();
        const std::string Data = static_cast<std::ostringstream*>(Owned.get())->str();
        return {Data.begin(), Data.end()};
    }

    void BinaryWriter::WriteRaw(const void* Source, size_t Size) {
        if (Size == 0) {
            return;
        }
        Stream->write(static_cast<const char*>(Source), static_cast<std::streamsize>(Size));
        if (Stream->fail()) {
            Stream->clear();
            throw BinaryException("Failed to write " + std::to_string(Size) + " bytes");
        }
    }
}  // namespace Souls
