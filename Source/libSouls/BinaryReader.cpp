//
// Created by Jake Rieger on 10/7/2026.
//

#include "BinaryReader.hpp"
#include "TextEncoding.hpp"

#include <fstream>
#include <streambuf>

namespace Souls {
    namespace {
        // Read-only, seekable stream buffer over existing memory.
        class MemoryBuffer final : public std::streambuf {
        public:
            MemoryBuffer(const uint8_t* Data, size_t Size) {
                char* Begin = const_cast<char*>(reinterpret_cast<const char*>(Data));
                setg(Begin, Begin, Begin + Size);
            }

        protected:
            pos_type seekoff(off_type Offset, std::ios_base::seekdir Dir, std::ios_base::openmode) override {
                char* Base = eback();
                off_type Target;
                switch (Dir) {
                    case std::ios_base::beg:
                        Target = Offset;
                        break;
                    case std::ios_base::cur:
                        Target = (gptr() - Base) + Offset;
                        break;
                    default:
                        Target = (egptr() - Base) + Offset;
                        break;
                }
                if (Target < 0 || Target > egptr() - Base) {
                    return pos_type(off_type(-1));
                }
                setg(Base, Base + Target, egptr());
                return pos_type(Target);
            }

            pos_type seekpos(pos_type Pos, std::ios_base::openmode Mode) override {
                return seekoff(off_type(Pos), std::ios_base::beg, Mode);
            }
        };
    }  // namespace

    BinaryReader::BinaryReader(const std::filesystem::path& Path, Endian Order) : Order(Order) {
        auto File = std::make_unique<std::ifstream>(Path, std::ios::binary);
        if (!File->is_open()) {
            throw BinaryException("Failed to open file for reading: " + Path.string());
        }
        Stream = File.get();
        Owned  = std::move(File);
    }

    BinaryReader::BinaryReader(std::vector<uint8_t>&& Data, Endian Order) : Order(Order), Storage(std::move(Data)) {
        InitMemory(Storage.data(), Storage.size());
    }

    BinaryReader::BinaryReader(std::span<const uint8_t> Data, Endian Order) : Order(Order) {
        InitMemory(Data.data(), Data.size());
    }

    BinaryReader::BinaryReader(std::istream& Stream, Endian Order) : Order(Order), Stream(&Stream) {}

    BinaryReader::~BinaryReader() = default;

    void BinaryReader::InitMemory(const uint8_t* Data, size_t Size) {
        Buffer = std::make_unique<MemoryBuffer>(Data, Size);
        Owned  = std::make_unique<std::istream>(Buffer.get());
        Stream = Owned.get();
    }

#pragma region Position
    int64_t BinaryReader::Position() {
        Stream->clear();
        const auto Pos = Stream->tellg();
        if (Pos == std::istream::pos_type(-1)) {
            throw BinaryException("Failed to query stream position");
        }
        return static_cast<int64_t>(Pos);
    }

    int64_t BinaryReader::Length() {
        const int64_t Current = Position();
        Stream->seekg(0, std::ios_base::end);
        const auto End = Stream->tellg();
        Stream->seekg(Current, std::ios_base::beg);
        if (End == std::istream::pos_type(-1) || Stream->fail()) {
            throw BinaryException("Failed to query stream length");
        }
        return static_cast<int64_t>(End);
    }

    int64_t BinaryReader::Remaining() {
        return Length() - Position();
    }

    bool BinaryReader::Eof() {
        return Remaining() <= 0;
    }

    void BinaryReader::Seek(int64_t Offset, std::ios_base::seekdir Origin) {
        Stream->clear();
        Stream->seekg(Offset, Origin);
        if (Stream->fail()) {
            Stream->clear();
            throw BinaryException("Seek out of range (offset " + std::to_string(Offset) + ")");
        }
    }

    void BinaryReader::Skip(int64_t Count) {
        Seek(Count, std::ios_base::cur);
    }

    void BinaryReader::Align(int64_t Alignment) {
        if (Alignment <= 0) {
            throw BinaryException("Alignment must be positive");
        }
        const int64_t Misalignment = Position() % Alignment;
        if (Misalignment != 0) {
            Skip(Alignment - Misalignment);
        }
    }

    void BinaryReader::AlignRelative(int64_t Start, int64_t Alignment) {
        if (Alignment <= 0) {
            throw BinaryException("Alignment must be positive");
        }
        const int64_t Misalignment = (Position() - Start) % Alignment;
        if (Misalignment != 0) {
            Skip(Alignment - Misalignment);
        }
    }

    void BinaryReader::StepIn(int64_t Position) {
        StepStack.push_back(this->Position());
        try {
            Seek(Position);
        } catch (...) {
            StepStack.pop_back();
            throw;
        }
    }

    void BinaryReader::StepOut() {
        if (StepStack.empty()) {
            throw BinaryException("StepOut without matching StepIn");
        }
        const int64_t Target = StepStack.back();
        StepStack.pop_back();
        Seek(Target);
    }
#pragma endregion

#pragma region Scalars
    bool BinaryReader::ReadBool() {
        const int64_t At   = Position();
        const uint8_t Byte = ReadByte();
        if (Byte > 1) {
            throw BinaryException("Invalid bool value " + std::to_string(Byte) + " at offset " + std::to_string(At));
        }
        return Byte == 1;
    }

    int64_t BinaryReader::ReadVarint() {
        return VarintLong ? ReadInt64() : static_cast<int64_t>(ReadInt32());
    }
#pragma endregion

#pragma region Strings
    std::string BinaryReader::ReadString(size_t Count) {
        CheckAvailable(Count, 1);
        std::string Result(Count, '\0');
        ReadRaw(Result.data(), Count);
        return Result;
    }

    std::string BinaryReader::ReadCString() {
        std::string Result;
        while (true) {
            const uint8_t Byte = ReadByte();
            if (Byte == 0) {
                return Result;
            }
            Result.push_back(static_cast<char>(Byte));
        }
    }

    std::u16string BinaryReader::ReadUTF16() {
        std::u16string Result;
        while (true) {
            const char16_t Unit = Read<char16_t>();
            if (Unit == 0) {
                return Result;
            }
            Result.push_back(Unit);
        }
    }

    std::u16string BinaryReader::ReadUTF16(size_t Count) {
        CheckAvailable(Count, sizeof(char16_t));
        std::u16string Result(Count, u'\0');
        ReadInto(std::span<char16_t>(Result));
        return Result;
    }

    std::string BinaryReader::ReadShiftJIS() {
        return Text::ShiftJISToUTF8(ReadCString());
    }

    std::string BinaryReader::ReadShiftJIS(size_t Count) {
        return Text::ShiftJISToUTF8(ReadString(Count));
    }

    std::string BinaryReader::ReadFixStr(size_t Size) {
        std::string Raw = ReadString(Size);
        Raw.resize(Raw.find('\0') == std::string::npos ? Raw.size() : Raw.find('\0'));
        return Text::ShiftJISToUTF8(Raw);
    }

    std::u16string BinaryReader::ReadFixStrW(size_t Size) {
        std::u16string Raw = ReadUTF16(Size / 2);
        if (Size % 2 != 0) {
            Skip(1);
        }
        const size_t End = Raw.find(u'\0');
        if (End != std::u16string::npos) {
            Raw.resize(End);
        }
        return Raw;
    }
#pragma endregion

#pragma region Vectors
    Vector2 BinaryReader::ReadVector2() {
        const float X = ReadFloat();
        const float Y = ReadFloat();
        return {X, Y};
    }

    Vector3 BinaryReader::ReadVector3() {
        const float X = ReadFloat();
        const float Y = ReadFloat();
        const float Z = ReadFloat();
        return {X, Y, Z};
    }

    Vector4 BinaryReader::ReadVector4() {
        const float X = ReadFloat();
        const float Y = ReadFloat();
        const float Z = ReadFloat();
        const float W = ReadFloat();
        return {X, Y, Z, W};
    }
#pragma endregion

#pragma region Assertions
    void BinaryReader::AssertMagic(std::string_view Magic) {
        const int64_t At         = Position();
        const std::string Actual = ReadString(Magic.size());
        if (Actual != Magic) {
            throw BinaryException("Magic mismatch at offset " + std::to_string(At) + ": expected \"" +
                                  std::string(Magic) + "\", got \"" + Actual + "\"");
        }
    }

    void BinaryReader::AssertPattern(size_t Count, uint8_t Value) {
        const int64_t At = Position();
        for (const uint8_t Byte : ReadBytes(Count)) {
            if (Byte != Value) {
                throw BinaryException("Pattern mismatch in " + std::to_string(Count) + " bytes at offset " +
                                      std::to_string(At) + ": expected " + std::to_string(Value) + ", got " +
                                      std::to_string(Byte));
            }
        }
    }
#pragma endregion

    void BinaryReader::ReadRaw(void* Destination, size_t Size) {
        if (Size == 0) {
            return;
        }
        const int64_t At = Position();
        Stream->read(static_cast<char*>(Destination), static_cast<std::streamsize>(Size));
        if (static_cast<size_t>(Stream->gcount()) != Size) {
            Stream->clear();
            throw BinaryException("Unexpected end of data reading " + std::to_string(Size) + " bytes at offset " +
                                  std::to_string(At));
        }
    }

    // Guards against allocating absurd sizes from corrupt counts before touching the stream.
    void BinaryReader::CheckAvailable(size_t Count, size_t ElementSize) {
        const int64_t Left = Remaining();
        if (Left < 0 || Count > static_cast<size_t>(Left) / ElementSize) {
            throw BinaryException("Requested " + std::to_string(Count) + " elements of " +
                                  std::to_string(ElementSize) + " bytes, but only " + std::to_string(Left) +
                                  " bytes remain");
        }
    }

    void BinaryReader::ThrowAssert(int64_t At, const std::string& Expected, const std::string& Actual) {
        throw BinaryException("Assertion failed at offset " + std::to_string(At) + ": expected " + Expected +
                              ", got " + Actual);
    }
}  // namespace Souls
