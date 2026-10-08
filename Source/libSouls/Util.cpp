//
// Created by Jake Rieger on 10/7/2026.
//

#include "Util.hpp"

#include <zlib.h>

#include <algorithm>
#include <cctype>

namespace Souls::Util {
    uint8_t ReverseBits(uint8_t Value) {
        uint8_t Result = 0;
        for (int I = 0; I < 8; ++I) {
            Result = static_cast<uint8_t>((Result << 1) | ((Value >> I) & 1));
        }
        return Result;
    }

    std::filesystem::path Backup(const std::filesystem::path& File, bool Overwrite) {
        std::filesystem::path Bak = File;
        Bak += ".bak";
        if (Overwrite || !std::filesystem::exists(Bak)) {
            std::filesystem::copy_file(File, Bak, std::filesystem::copy_options::overwrite_existing);
        }
        return Bak;
    }

    std::string GetRealExtension(const std::filesystem::path& Path) {
        std::filesystem::path Name = Path.filename();
        if (Name.extension() == ".dcx") {
            Name = Name.stem();
        }
        return Name.extension().string();
    }

    std::string GetRealFileName(const std::filesystem::path& Path) {
        std::filesystem::path Name = Path.filename();
        if (Name.extension() == ".dcx") {
            Name = Name.stem();
        }
        return Name.stem().string();
    }

    uint32_t FromPathHash(std::string_view Text) {
        std::string Hashable(Text);
        for (char& C : Hashable) {
            C = C == '\\' ? '/' : static_cast<char>(std::tolower(static_cast<unsigned char>(C)));
        }
        if (Hashable.empty() || Hashable.front() != '/') {
            Hashable.insert(Hashable.begin(), '/');
        }

        uint32_t Hash = 0;
        for (const char C : Hashable) {
            Hash = Hash * 37u + static_cast<uint32_t>(static_cast<unsigned char>(C));
        }
        return Hash;
    }

    bool IsPrime(uint32_t Candidate) {
        if (Candidate < 2) {
            return false;
        }
        if (Candidate == 2) {
            return true;
        }
        if (Candidate % 2 == 0) {
            return false;
        }
        for (uint64_t I = 3; I * I <= Candidate; I += 2) {
            if (Candidate % I == 0) {
                return false;
            }
        }
        return true;
    }

    namespace {
        bool IsDigit(char C) {
            return C >= '0' && C <= '9';
        }

        bool IsWordChar(char C) {
            return IsDigit(C) || (C >= 'A' && C <= 'Z') || (C >= 'a' && C <= 'z') || C == '_';
        }
    }  // namespace

    BinderTime BinderTimestampToDate(std::string_view Timestamp) {
        // Format: YY<month letter>D+<hour letter>M+  (two digits, a word char, digits, a word char, digits)
        size_t Pos = 0;
        auto Fail  = [] { throw BinaryException("Unrecognized timestamp format."); };
        auto Digits = [&](size_t Min) {
            const size_t Start = Pos;
            while (Pos < Timestamp.size() && IsDigit(Timestamp[Pos])) {
                ++Pos;
            }
            if (Pos - Start < Min || Pos - Start > 9) {
                Fail();
            }
            return std::stoi(std::string(Timestamp.substr(Start, Pos - Start)));
        };
        auto Letter = [&] {
            if (Pos >= Timestamp.size() || !IsWordChar(Timestamp[Pos])) {
                Fail();
            }
            return Timestamp[Pos++];
        };

        // Upstream's regex is (\d\d)(\w)(\d+)(\w)(\d+); the letters encode month/hour as offsets from 'A'.
        if (Digits(2) > 99 || Pos != 2) {
            Fail();
        }
        const int Year   = std::stoi(std::string(Timestamp.substr(0, 2))) + 2000;
        const int Month  = Letter() - 'A';
        const int Day    = Digits(1);
        const int Hour   = Letter() - 'A';
        const int Minute = Digits(1);

        using namespace std::chrono;
        const year_month_day Date{
          std::chrono::year{Year}, std::chrono::month{static_cast<unsigned>(Month)}, std::chrono::day{static_cast<unsigned>(Day)}};
        if (!Date.ok() || Hour < 0 || Hour > 23 || Minute < 0 || Minute > 59) {
            throw BinaryException("Binder timestamp is not a valid date.");
        }
        return sys_days{Date} + hours{Hour} + minutes{Minute};
    }

    std::string DateToBinderTimestamp(BinderTime Time) {
        using namespace std::chrono;
        const sys_days Days = floor<days>(Time);
        const year_month_day Date{Days};
        const hh_mm_ss<minutes> Clock{Time - Days};

        const int Year = static_cast<int>(Date.year()) - 2000;
        if (Year < 0 || Year > 99) {
            throw BinaryException("BND timestamp year must be between 2000 and 2099 inclusive.");
        }

        std::string Result;
        Result += static_cast<char>('0' + Year / 10);
        Result += static_cast<char>('0' + Year % 10);
        Result += static_cast<char>(static_cast<unsigned>(Date.month()) + 'A');
        Result += std::to_string(static_cast<unsigned>(Date.day()));
        Result += static_cast<char>(Clock.hours().count() + 'A');
        Result += std::to_string(Clock.minutes().count());
        Result.resize(std::max<size_t>(Result.size(), 8), '\0');
        return Result;
    }

    uint32_t Adler32(std::span<const uint8_t> Data) {
        uint32_t A = 1;
        uint32_t B = 0;
        for (const uint8_t Byte : Data) {
            A = (A + Byte) % 65521;
            B = (B + A) % 65521;
        }
        return (B << 16) | A;
    }

    std::vector<uint8_t> ParseHexString(std::string_view Text) {
        std::vector<uint8_t> Bytes;
        size_t Pos = 0;
        while (true) {
            const size_t End = Text.find(' ', Pos);
            const std::string Token(Text.substr(Pos, End == std::string_view::npos ? End : End - Pos));
            size_t Used = 0;
            unsigned long Value = 0;
            try {
                Value = std::stoul(Token, &Used, 16);
            } catch (const std::exception&) {
                throw BinaryException("Invalid hex byte \"" + Token + "\"");
            }
            if (Used != Token.size() || Value > 0xFF) {
                throw BinaryException("Invalid hex byte \"" + Token + "\"");
            }
            Bytes.push_back(static_cast<uint8_t>(Value));
            if (End == std::string_view::npos) {
                return Bytes;
            }
            Pos = End + 1;
        }
    }

    std::vector<uint8_t> ReadZlib(BinaryReader& Reader, int CompressedSize) {
        if (CompressedSize < 2) {
            throw BinaryException("Zlib block is too small");
        }
        Reader.Assert<uint8_t>(0x78);
        Reader.Assert<uint8_t>(0x01, 0x5E, 0x9C, 0xDA);
        return InflateRaw(Reader.ReadBytes(static_cast<size_t>(CompressedSize - 2)));
    }

    std::vector<uint8_t> InflateRaw(std::span<const uint8_t> Compressed) {
        z_stream Stream{};
        if (inflateInit2(&Stream, -15) != Z_OK) {
            throw BinaryException("Failed to initialize inflate");
        }
        Stream.next_in  = const_cast<Bytef*>(Compressed.data());
        Stream.avail_in = static_cast<uInt>(Compressed.size());

        std::vector<uint8_t> Output(std::max<size_t>(Compressed.size() * 4, 4096));
        size_t Written = 0;
        int Result     = Z_OK;
        while (Result == Z_OK) {
            if (Written == Output.size()) {
                Output.resize(Output.size() * 2);
            }
            Stream.next_out  = Output.data() + Written;
            Stream.avail_out = static_cast<uInt>(Output.size() - Written);
            Result           = inflate(&Stream, Z_NO_FLUSH);
            Written          = Output.size() - Stream.avail_out;
        }
        inflateEnd(&Stream);

        if (Result != Z_STREAM_END) {
            throw BinaryException("Deflate decompression failed (code " + std::to_string(Result) + ")");
        }
        Output.resize(Written);
        return Output;
    }

    std::vector<uint8_t> DeflateRaw(std::span<const uint8_t> Input, int Level) {
        z_stream Stream{};
        if (deflateInit2(&Stream, Level, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
            throw BinaryException("Failed to initialize deflate");
        }
        Stream.next_in  = const_cast<Bytef*>(Input.data());
        Stream.avail_in = static_cast<uInt>(Input.size());

        std::vector<uint8_t> Output(deflateBound(&Stream, static_cast<uLong>(Input.size())));
        Stream.next_out  = Output.data();
        Stream.avail_out = static_cast<uInt>(Output.size());
        const int Result = deflate(&Stream, Z_FINISH);
        const size_t Size = Output.size() - Stream.avail_out;
        deflateEnd(&Stream);

        if (Result != Z_STREAM_END) {
            throw BinaryException("Deflate compression failed (code " + std::to_string(Result) + ")");
        }
        Output.resize(Size);
        return Output;
    }

    int WriteZlib(BinaryWriter& Writer, uint8_t FormatByte, std::span<const uint8_t> Input, int Level) {
        const int64_t Start = Writer.Position();
        Writer.WriteByte(0x78);
        Writer.WriteByte(FormatByte);

        Writer.WriteBytes(DeflateRaw(Input, Level));

        // The zlib checksum is stored big-endian regardless of the writer's byte order.
        const Endian Previous = Writer.Order;
        Writer.Order          = Endian::Big;
        Writer.WriteUInt32(Adler32(Input));
        Writer.Order = Previous;

        return static_cast<int>(Writer.Position() - Start);
    }
}  // namespace Souls::Util
