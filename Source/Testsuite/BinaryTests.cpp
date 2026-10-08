//
// Created by Jake Rieger on 10/7/2026.
//

#include <libSouls/BinaryReader.hpp>
#include <libSouls/BinaryWriter.hpp>
#include <libSouls/Formats/DCX.hpp>
#include <libSouls/TextEncoding.hpp>
#include <libSouls/Util.hpp>

#include <cstdio>
#include <filesystem>

namespace {
    int Failures = 0;

#define CHECK(Cond)                                                          \
    do {                                                                     \
        if (!(Cond)) {                                                       \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #Cond);      \
            ++Failures;                                                      \
        }                                                                    \
    } while (0)

    template<typename F>
    bool Throws(F&& Fn) {
        try {
            Fn();
        } catch (const Souls::BinaryException&) {
            return true;
        }
        return false;
    }

    void WriteAll(Souls::BinaryWriter& W) {
        W.WriteMagic("TEST");
        W.Reserve<uint32_t>("size");
        W.WriteBool(true);
        W.WriteSByte(-5);
        W.WriteInt16(-1234);
        W.WriteUInt16(0xBEEF);
        W.WriteInt32(-123456789);
        W.WriteUInt32(0xDEADBEEF);
        W.WriteInt64(-1234567890123LL);
        W.WriteUInt64(0x0123456789ABCDEFULL);
        W.WriteFloat(3.5f);
        W.WriteDouble(-2.25);
        W.WriteArray(std::vector<int32_t>{1, -2, 3});
        W.WriteString("hello");
        W.WriteUTF16(u"wideé");
        W.WriteFixedString("ab", 6);
        W.Align(16);
        W.VarintLong = true;
        W.WriteVarint(-7);
        W.VarintLong = false;
        W.WriteVarint(42);
        W.Fill<uint32_t>("size", static_cast<uint32_t>(W.Position()));
    }

    void ReadAll(Souls::BinaryReader& R) {
        R.AssertMagic("TEST");
        const uint32_t Size = R.ReadUInt32();
        CHECK(R.ReadBool());
        CHECK(R.ReadSByte() == -5);
        CHECK(R.ReadInt16() == -1234);
        CHECK(R.ReadUInt16() == 0xBEEF);
        CHECK(R.ReadInt32() == -123456789);
        CHECK(R.ReadUInt32() == 0xDEADBEEF);
        CHECK(R.ReadInt64() == -1234567890123LL);
        CHECK(R.ReadUInt64() == 0x0123456789ABCDEFULL);
        CHECK(R.ReadFloat() == 3.5f);
        CHECK(R.ReadDouble() == -2.25);
        CHECK((R.ReadArray<int32_t>(3) == std::vector<int32_t>{1, -2, 3}));
        CHECK(R.ReadCString() == "hello");
        CHECK(R.ReadUTF16() == u"wideé");
        CHECK(R.ReadString(6) == std::string("ab\0\0\0\0", 6));
        R.Align(16);
        R.VarintLong = true;
        CHECK(R.ReadVarint() == -7);
        R.VarintLong = false;
        CHECK(R.ReadVarint() == 42);
        CHECK(R.Position() == Size);
        CHECK(R.Eof());
    }
}  // namespace

int RunBinaryTests() {
    using namespace Souls;

    for (Endian Order : {Endian::Little, Endian::Big}) {
        // In-memory round trip.
        BinaryWriter W(Order);
        WriteAll(W);
        std::vector<uint8_t> Bytes = W.ToBytes();

        BinaryReader ViewReader(std::span<const uint8_t>(Bytes), Order);
        ReadAll(ViewReader);

        // File round trip.
        const auto Path = std::filesystem::temp_directory_path() / "libsouls_binary_test.bin";
        {
            BinaryWriter FileWriter(Path, Order);
            WriteAll(FileWriter);
            FileWriter.Finish();
        }
        {
            BinaryReader FileReader(Path, Order);
            ReadAll(FileReader);
        }
        std::filesystem::remove(Path);

        // Owned-buffer ctor.
        BinaryReader OwnedReader(std::vector<uint8_t>(Bytes), Order);
        ReadAll(OwnedReader);
    }

    // Raw byte order.
    {
        BinaryWriter Le(Endian::Little), Be(Endian::Big);
        Le.WriteUInt32(0x01020304);
        Be.WriteUInt32(0x01020304);
        CHECK((Le.ToBytes() == std::vector<uint8_t>{4, 3, 2, 1}));
        CHECK((Be.ToBytes() == std::vector<uint8_t>{1, 2, 3, 4}));
    }

    // Mid-stream endian flip, peek, ReadAt, StepIn/StepOut.
    {
        const std::vector<uint8_t> Data{0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02};
        BinaryReader R{std::span<const uint8_t>(Data)};
        CHECK(R.Peek<uint32_t>() == 1);
        CHECK(R.Position() == 0);
        CHECK(R.ReadAt<uint32_t>(4) == 0x02000000);
        R.Order = Endian::Big;
        CHECK(R.ReadAt<uint32_t>(4) == 2);
        R.StepIn(4);
        CHECK(R.ReadUInt32() == 2);
        R.StepOut();
        CHECK(R.Position() == 0);
    }

    // Error paths.
    {
        const std::vector<uint8_t> Data{1, 2, 3};
        BinaryReader R{std::span<const uint8_t>(Data)};
        CHECK(Throws([&] { R.ReadUInt32(); }));
        CHECK(Throws([&] { R.ReadArray<uint32_t>(1000000); }));
        R.Seek(0);
        CHECK(Throws([&] { R.Assert<uint8_t>(9); }));
        CHECK(Throws([&] { R.AssertMagic("XYZ"); }));
        CHECK(Throws([&] { R.StepOut(); }));

        BinaryWriter W;
        W.Reserve<uint32_t>("a");
        CHECK(Throws([&] { W.Reserve<uint32_t>("a"); }));
        CHECK(Throws([&] { W.Fill<uint16_t>("a", 1); }));
        CHECK(Throws([&] { W.Fill<uint32_t>("missing", 1); }));
        CHECK(Throws([&] { W.Finish(); }));
        W.Fill<uint32_t>("a", 1);
        CHECK(!Throws([&] { W.Finish(); }));
    }

    // Multi-option assert, text encodings, vectors, relative alignment.
    {
        const std::string Utf8      = "\xE3\x82\xBD\xE3\x82\xA6\xE3\x83\xAB";  // ソウル
        const std::string ShiftJIS  = "\x83\x5C\x83\x45\x83\x8B";
        CHECK(Text::UTF8ToShiftJIS(Utf8) == ShiftJIS);
        CHECK(Text::ShiftJISToUTF8(ShiftJIS) == Utf8);
        CHECK(Text::UTF16ToUTF8(Text::UTF8ToUTF16(Utf8)) == Utf8);

        BinaryWriter W(Endian::Big);
        W.WriteFixStr(Utf8, 10);
        W.WriteFixStrW(u"abc", 10);
        W.WriteShiftJIS(Utf8);
        W.WriteVector2({1.f, 2.f});
        W.WriteVector3({3.f, 4.f, 5.f});
        W.WriteVector4({6.f, 7.f, 8.f, 9.f});
        W.WriteByte(0x5E);
        W.Pad(1);
        W.AlignRelative(3, 4, 0xFF);
        const auto Bytes = W.ToBytes();

        BinaryReader R(std::span<const uint8_t>(Bytes), Endian::Big);
        CHECK(R.ReadFixStr(10) == Utf8);
        CHECK(R.ReadFixStrW(10) == u"abc");
        CHECK(R.ReadShiftJIS() == Utf8);
        CHECK((R.ReadVector2() == Vector2{1.f, 2.f}));
        CHECK((R.ReadVector3() == Vector3{3.f, 4.f, 5.f}));
        CHECK((R.ReadVector4() == Vector4{6.f, 7.f, 8.f, 9.f}));
        CHECK(R.Assert<uint8_t>(0x01, 0x5E, 0x9C) == 0x5E);
        CHECK(Throws([&] { R.Seek(-2, std::ios::cur); R.Assert<uint8_t>(0x01, 0x02); }));
    }

    // Util helpers.
    {
        CHECK(Util::ReverseBits(0x01) == 0x80);
        CHECK(Util::ReverseBits(0x0F) == 0xF0);
        CHECK(Util::FromPathHash("A") == 47u * 37u + 97u);
        CHECK(Util::FromPathHash("\\A") == Util::FromPathHash("/a"));
        CHECK(Util::IsPrime(7919) && !Util::IsPrime(7917) && !Util::IsPrime(1) && Util::IsPrime(2));
        const std::string Wiki = "Wikipedia";
        CHECK(Util::Adler32({reinterpret_cast<const uint8_t*>(Wiki.data()), Wiki.size()}) == 0x11E60398);
        CHECK((Util::ParseHexString("DE AD be EF") == std::vector<uint8_t>{0xDE, 0xAD, 0xBE, 0xEF}));
        CHECK(Throws([] { Util::ParseHexString("DE ZZ"); }));
        CHECK(Util::GetRealExtension("dir/a.flver.dcx") == ".flver");
        CHECK(Util::GetRealExtension("a.flver") == ".flver");
        CHECK(Util::GetRealFileName("dir/a.flver.dcx") == "a");

        using namespace std::chrono;
        const Util::BinderTime Time = sys_days{year{2007} / 2 / 26} + hours{12} + minutes{10};
        const std::string Stamp     = Util::DateToBinderTimestamp(Time);
        CHECK(Stamp.size() == 8 && Stamp.substr(0, 6) == "07C26M");
        CHECK(Util::BinderTimestampToDate(Stamp) == Time);
        CHECK(Throws([] { Util::BinderTimestampToDate("garbage"); }));

        // zlib: round trip through the big-endian writer DCX uses, and check the framing.
        std::vector<uint8_t> Input(5000);
        for (size_t I = 0; I < Input.size(); ++I) {
            Input[I] = static_cast<uint8_t>((I * 7) % 13);
        }
        BinaryWriter W(Endian::Big);
        const int Written = Util::WriteZlib(W, 0xDA, Input);
        const auto Zipped = W.ToBytes();
        CHECK(Written == static_cast<int>(Zipped.size()));
        CHECK(Zipped[0] == 0x78 && Zipped[1] == 0xDA);
        CHECK(static_cast<size_t>(Written) < Input.size());
        BinaryReader R(std::span<const uint8_t>(Zipped), Endian::Big);
        CHECK(Util::ReadZlib(R, Written) == Input);
        // Trailer is the big-endian Adler-32 of the input.
        CHECK(Zipped[Zipped.size() - 4] == (Util::Adler32(Input) >> 24));

        std::vector<uint8_t> Corrupt = Zipped;
        Corrupt[10] ^= 0xFF;
        BinaryReader BadReader(std::span<const uint8_t>(Corrupt), Endian::Big);
        // Inflate doesn't verify the checksum, so corruption either errors or yields different data.
        bool Detected = false;
        try {
            Detected = Util::ReadZlib(BadReader, Written) != Input;
        } catch (const BinaryException&) {
            Detected = true;
        }
        CHECK(Detected);
    }

    // DCX round trips for every writable type, on sizes that exercise chunk boundaries.
    {
        auto MakeData = [](size_t Size) {
            std::vector<uint8_t> Data(Size);
            uint32_t State = 12345;
            for (size_t I = 0; I < Size; ++I) {
                // First half compressible, second half pseudo-random (incompressible chunks for EDGE).
                State   = State * 1664525u + 1013904223u;
                Data[I] = I < Size / 2 ? static_cast<uint8_t>((I / 7) % 5) : static_cast<uint8_t>(State >> 24);
            }
            return Data;
        };

        using DCX::Type;
        const Type Types[] = {Type::Zlib,
                              Type::DCP_DFLT,
                              Type::DCX_EDGE,
                              Type::DCX_DFLT_10000_24_9,
                              Type::DCX_DFLT_10000_44_9,
                              Type::DCX_DFLT_11000_44_8,
                              Type::DCX_DFLT_11000_44_9,
                              Type::DCX_DFLT_11000_44_9_15,
                              Type::DCX_KRAK_6,
                              Type::DCX_KRAK_9,
                              Type::DCX_ZSTD};

        for (const Type T : Types) {
            for (const size_t Size : {size_t{1}, size_t{1000}, size_t{0x10000}, size_t{0x20000}, size_t{0x20000 + 123}}) {
                const auto Data = MakeData(Size);
                try {
                    const auto Packed = DCX::Compress(Data, T);
                    Type Detected     = Type::Unknown;
                    const auto Back   = DCX::Decompress(Packed, Detected);
                    CHECK(Back == Data);
                    CHECK(Detected == T);
                    CHECK(DCX::Is(Packed) == (T != Type::Zlib));
                } catch (const BinaryException& E) {
                    std::printf("FAIL DCX type %d size %zu: %s\n", static_cast<int>(T), Size, E.what());
                    ++Failures;
                }
            }
        }

        // Header sanity: a DFLT container starts with big-endian "DCX\0" and its 0x28 format tag.
        const auto Data   = MakeData(100);
        const auto Packed = DCX::Compress(Data, Type::DCX_DFLT_10000_44_9);
        CHECK(std::string(Packed.begin(), Packed.begin() + 4) == std::string("DCX\0", 4));
        CHECK(std::string(Packed.begin() + 0x28, Packed.begin() + 0x2C) == "DFLT");
        CHECK(!DCX::Is(Data));
        CHECK(Throws([&] { DCX::Decompress(Data); }));
        CHECK(Throws([&] { DCX::Compress(Data, Type::Unknown); }));
        CHECK(Throws([&] { DCX::Compress(Data, Type::None); }));

        // Truncated container must throw rather than return garbage.
        std::vector<uint8_t> Truncated(Packed.begin(), Packed.end() - 20);
        CHECK(Throws([&] { DCX::Decompress(Truncated); }));
    }

    std::printf(Failures == 0 ? "Binary tests passed\n" : "Binary tests: %d failure(s)\n", Failures);
    return Failures;
}
