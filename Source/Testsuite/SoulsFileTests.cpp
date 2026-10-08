//
// Created by Jake Rieger on 10/7/2026.
//

#include <libSouls/SoulsFile.hpp>

#include <cstdio>
#include <filesystem>

namespace {
    int Failures = 0;

#define CHECK(Cond)                                                     \
    do {                                                                \
        if (!(Cond)) {                                                  \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #Cond); \
            ++Failures;                                                 \
        }                                                               \
    } while (0)

    // A minimal format: "TST1", big-endian count, then that many int32 values.
    class TestFile : public Souls::SoulsFile<TestFile> {
    public:
        std::vector<int32_t> Values;
        const char* Forbidden = "ok";  // Validate fails if null

        bool Validate(std::exception_ptr& Error) override {
            return ValidateNull(Forbidden, "Forbidden is null", Error) &&
                   ValidateIndex(static_cast<int64_t>(Values.size()), 0, "Values must not be empty", Error);
        }

    protected:
        bool IsImpl(Souls::BinaryReader& Reader) override {
            return Reader.Length() >= 4 && Reader.ReadString(4) == "TST1";
        }

        void ReadImpl(Souls::BinaryReader& Reader) override {
            Reader.AssertMagic("TST1");
            Reader.Order = Souls::Endian::Big;
            Values       = Reader.ReadArray<int32_t>(Reader.ReadUInt32());
        }

        void WriteImpl(Souls::BinaryWriter& Writer) override {
            Writer.WriteMagic("TST1");
            Writer.Order = Souls::Endian::Big;
            Writer.WriteUInt32(static_cast<uint32_t>(Values.size()));
            Writer.WriteArray(Values);
        }
    };

    // Accepts anything starting with "BND4"; used to check DCX handling on real game files.
    class BndProbe : public Souls::SoulsFile<BndProbe> {
    public:
        std::vector<uint8_t> Bytes;

    protected:
        bool IsImpl(Souls::BinaryReader& Reader) override {
            return Reader.Length() >= 4 && Reader.ReadString(4) == "BND4";
        }
        void ReadImpl(Souls::BinaryReader& Reader) override {
            Bytes = Reader.ReadBytes(static_cast<size_t>(Reader.Length()));
        }
        void WriteImpl(Souls::BinaryWriter& Writer) override { Writer.WriteBytes(Bytes); }
    };

    // Doesn't override anything.
    class Unimplemented : public Souls::SoulsFile<Unimplemented> {};

    template<typename F>
    bool Throws(F&& Fn) {
        try {
            Fn();
        } catch (...) {
            return true;
        }
        return false;
    }
}  // namespace

int RunSoulsFileTests() {
    using namespace Souls;
    namespace fs = std::filesystem;

    TestFile Original;
    Original.Values = {1, -2, 300000};

    // Uncompressed round trip.
    {
        const auto Bytes = Original.Write();
        CHECK(Bytes.size() == 4 + 4 + 12);
        CHECK(Bytes[4] == 0 && Bytes[7] == 3);  // big-endian count
        CHECK(TestFile::Is(Bytes));
        TestFile Back = TestFile::Read(Bytes);
        CHECK(Back.Values == Original.Values);
        CHECK(Back.Compression == DCX::Type::None);
    }

    // Through each family of DCX compression: Compression is detected on read and reused on write.
    for (const DCX::Type Type : {DCX::Type::DCP_DFLT, DCX::Type::DCX_DFLT_10000_44_9, DCX::Type::DCX_EDGE,
                                 DCX::Type::DCX_KRAK_6, DCX::Type::DCX_ZSTD}) {
        const auto Bytes = Original.Write(Type);
        CHECK(TestFile::Is(Bytes));
        TestFile Back = TestFile::Read(Bytes);
        CHECK(Back.Values == Original.Values);
        CHECK(Back.Compression == Type);
        CHECK(TestFile::Read(Back.Write()).Compression == Type);
    }

    // IsRead / Is on things that aren't this format.
    {
        const std::vector<uint8_t> Junk{'N', 'O', 'P', 'E', 1, 2, 3};
        CHECK(!TestFile::Is(Junk));
        CHECK(!TestFile::Is(std::span<const uint8_t>()));
        CHECK(!TestFile::IsRead(Junk).has_value());
        CHECK(TestFile::IsRead(Original.Write()).has_value());
        CHECK(TestFile::IsRead(Original.Write(DCX::Type::DCX_DFLT_10000_44_9))->Values == Original.Values);
        CHECK(Throws([&] { TestFile::Read(Junk); }));
        // A format that doesn't implement the hooks says so.
        CHECK(Throws([&] { Unimplemented::Is(Junk); }));
        CHECK(Throws([&] { Unimplemented().Write(); }));
    }

    // Validation blocks writes.
    {
        TestFile Bad;  // empty Values
        CHECK(Throws([&] { Bad.Write(); }));
        TestFile Bad2 = Original;
        Bad2.Forbidden = nullptr;
        CHECK(Throws([&] { Bad2.Write(DCX::Type::DCX_ZSTD); }));
        std::exception_ptr Error;
        CHECK(!Bad.Validate(Error) && Error != nullptr);
        CHECK(Original.Validate(Error));
    }

    // Files: creates missing directories, reads back, keeps compression.
    {
        const fs::path Dir  = fs::temp_directory_path() / "libsouls_soulsfile_test";
        const fs::path Path = Dir / "nested" / "a.tst.dcx";
        fs::remove_all(Dir);
        Original.Write(Path, DCX::Type::DCX_KRAK_6);
        CHECK(fs::exists(Path));
        CHECK(TestFile::Is(Path));
        const TestFile Back = TestFile::Read(Path);
        CHECK(Back.Values == Original.Values && Back.Compression == DCX::Type::DCX_KRAK_6);
        CHECK(TestFile::IsRead(Path).has_value());

        const fs::path Empty = Dir / "empty.bin";
        { BinaryWriter W(Empty); W.Finish(); }
        CHECK(!TestFile::Is(Empty));
        fs::remove_all(Dir);
    }

    // A real DCX-wrapped BND4 from the game, if installed.
    {
        const fs::path Real =
          "C:/Program Files (x86)/Steam/steamapps/common/ELDEN RING/Game/asset/aeg/aeg001/aeg001_003.geombnd.dcx";
        if (fs::exists(Real)) {
            CHECK(BndProbe::Is(Real));
            CHECK(!TestFile::Is(Real));
            BndProbe Probe = BndProbe::Read(Real);
            CHECK(Probe.Compression == DCX::Type::DCX_KRAK_6);
            CHECK(Probe.Bytes.size() > 0x40 && Probe.Bytes[0] == 'B' && Probe.Bytes[3] == '4');
            CHECK(BndProbe::Read(Probe.Write()).Bytes == Probe.Bytes);
        } else {
            std::printf("Real BND4 check skipped (game file not found)\n");
        }
    }

    std::printf(Failures == 0 ? "SoulsFile tests passed\n" : "SoulsFile tests: %d failure(s)\n", Failures);
    return Failures;
}
