//
// Created by Jake Rieger on 10/7/2026.
//

#include "TestResult.hpp"

#include <libSouls/Binders/BND3.hpp>
#include <libSouls/Binders/BND4.hpp>
#include <libSouls/Binders/BXF3.hpp>
#include <libSouls/Binders/BXF4.hpp>
#include <libSouls/Binders/Regulation.hpp>
#include <libSouls/Crypto.hpp>

#include <cstdio>
#include <filesystem>

namespace {
    int Failures = 0;

#define CHECK(Cond)                                                                                                    \
    do {                                                                                                               \
        if (!(Cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #Cond);                                                \
            ++Failures;                                                                                                \
        }                                                                                                              \
    } while (0)

    using namespace Souls;
    using Binder::FileFlags;
    using Binder::Format;

    template<typename F>
    bool Throws(F&& Fn) {
        try {
            Fn();
        } catch (...) { return true; }
        return false;
    }

    std::vector<uint8_t> MakeBytes(size_t Size, uint32_t Seed) {
        std::vector<uint8_t> Data(Size);
        for (size_t I = 0; I < Size; ++I) {
            Seed    = Seed * 1664525u + 1013904223u;
            Data[I] = I % 3 == 0 ? static_cast<uint8_t>(Seed >> 24) : static_cast<uint8_t>(I % 11);
        }
        return Data;
    }

    bool SameFiles(const std::vector<BinderFile>& A, const std::vector<BinderFile>& B, bool CompareCompressionType) {
        if (A.size() != B.size()) return false;
        for (size_t I = 0; I < A.size(); ++I) {
            if (A[I].Flags != B[I].Flags || A[I].ID != B[I].ID || A[I].Name != B[I].Name || A[I].Bytes != B[I].Bytes) {
                return false;
            }
            if (CompareCompressionType && Binder::IsCompressed(A[I].Flags) &&
                A[I].CompressionType != B[I].CompressionType) {
                return false;
            }
        }
        return true;
    }

    // Round-trips a binder with the given settings: write, read, compare, and write again for stability.
    // Works for BND3 and BND4 (the SoulsFile binders).
    template<typename B>
    void RoundTrip(const char* Label, B& Source) {
        try {
            const auto Bytes = Source.Write();
            CHECK(B::Is(Bytes));
            B Back          = B::Read(Bytes);
            const bool Same = SameFiles(Source.Files, Back.Files, true);
            if (!Same) std::printf("  (%s: files differ)\n", Label);
            CHECK(Same);
            CHECK(Back.Format == Source.Format);
            CHECK(Back.Version == Source.Version);
            CHECK(Back.BigEndian == Source.BigEndian && Back.BitBigEndian == Source.BitBigEndian);
            if constexpr (requires { Source.Extended; }) {
                CHECK(Back.Unicode == Source.Unicode && Back.Extended == Source.Extended);
            }
            // Writing what we read must reproduce the exact bytes (all compression here is deterministic).
            const bool Stable = Back.Write() == Bytes;
            if (!Stable) std::printf("  (%s: rewrite is not byte-identical)\n", Label);
            CHECK(Stable);
        } catch (const std::exception& E) {
            std::printf("FAIL %s: %s\n", Label, E.what());
            ++Failures;
        }
    }

    // Same for the two-file BXF3/BXF4.
    template<typename B>
    void RoundTripBxf(const char* Label, B& Source) {
        try {
            const BXFBytes Bytes = Source.Write();
            CHECK(B::IsBHD(Bytes.Header) && B::IsBDT(Bytes.Data));
            CHECK(!B::IsBHD(Bytes.Data) && !B::IsBDT(Bytes.Header));
            B Back          = B::Read(Bytes.Header, Bytes.Data);
            const bool Same = SameFiles(Source.Files, Back.Files, true);
            if (!Same) std::printf("  (%s: files differ)\n", Label);
            CHECK(Same);
            CHECK(Back.Format == Source.Format);
            CHECK(Back.Version == Source.Version);
            CHECK(Back.BigEndian == Source.BigEndian && Back.BitBigEndian == Source.BitBigEndian);
            if constexpr (requires { Source.Extended; }) {
                CHECK(Back.Unicode == Source.Unicode && Back.Extended == Source.Extended);
            }
            const BXFBytes Again = Back.Write();
            const bool Stable    = Again.Header == Bytes.Header && Again.Data == Bytes.Data;
            if (!Stable) std::printf("  (%s: rewrite is not byte-identical)\n", Label);
            CHECK(Stable);

            // And through files on disk, creating the folders.
            namespace fs     = std::filesystem;
            const fs::path D = fs::temp_directory_path() / "libsouls_bxf_test";
            fs::remove_all(D);
            Source.Write(D / "a" / "x.bhd", D / "b" / "x.bdt");
            B FromDisk = B::Read(D / "a" / "x.bhd", D / "b" / "x.bdt");
            CHECK(B::IsBHD(D / "a" / "x.bhd") && B::IsBDT(D / "b" / "x.bdt"));
            CHECK(SameFiles(Source.Files, FromDisk.Files, true));
            fs::remove_all(D);
        } catch (const std::exception& E) {
            std::printf("FAIL %s: %s\n", Label, E.what());
            ++Failures;
        }
    }

    template<typename B>
    void AddFiles(B& Bnd, bool Names) {
        auto Add = [&](FileFlags Flags, int ID, const char* Name, std::vector<uint8_t> Bytes) {
            BinderFile File(Flags, ID, std::move(Bytes));
            if (Names) File.Name = Name;
            Bnd.Files.push_back(std::move(File));
        };
        // 0x02 is the usual flag set; compressed files add 0x01.
        Add(FileFlags::Flag1, 1, "N:\\data\\first.bin", MakeBytes(100, 1));
        Add(FileFlags::Flag1 | FileFlags::Compressed, 2, "N:\\data\\second.dat", MakeBytes(5000, 2));
        Add(FileFlags::Flag1, 3, "N:\\data\\empty.bin", {});
        Add(FileFlags::Flag1 | FileFlags::Compressed,
            400,
            "N:\\data\\\xE3\x82\xBD\xE3\x82\xA6\xE3\x83\xAB.txt",
            MakeBytes(70000, 3));
        Add(FileFlags::Flag1, -1, "N:\\data\\last.bin", MakeBytes(17, 4));
    }
}  // namespace

int RunBinderTests() {
    // The usual modern layout: IDs, names, compression, hash table, UTF-16 names.
    {
        BND4 Bnd;
        AddFiles(Bnd, true);
        RoundTrip("default", Bnd);

        // The hash table is present and findable: header says Extended == 4.
        const auto Bytes = Bnd.Write();
        CHECK(Bytes.size() > 0x40 && Bytes[0] == 'B' && Bytes[3] == '4');
    }

    // Shift-JIS names, no compression, no hash table.
    {
        BND4 Bnd;
        Bnd.Unicode  = false;
        Bnd.Extended = 0;
        Bnd.Format   = Format::IDs | Format::Names1 | Format::Names2;
        AddFiles(Bnd, true);
        for (auto& File : Bnd.Files)
            File.Flags = FileFlags::Flag1;
        RoundTrip("shift-jis", Bnd);
    }

    // Big-endian values and bit order, long offsets.
    {
        BND4 Bnd;
        Bnd.BigEndian    = true;
        Bnd.BitBigEndian = true;
        Bnd.Extended     = 0;
        Bnd.Format =
          Format::BigEndian | Format::IDs | Format::Names1 | Format::Names2 | Format::LongOffsets | Format::Compression;
        AddFiles(Bnd, true);
        RoundTrip("big-endian", Bnd);
    }

    // No IDs or names at all, short offsets.
    {
        BND4 Bnd;
        Bnd.Extended = 0;
        Bnd.Format   = Format::Compression;
        AddFiles(Bnd, false);
        for (auto& File : Bnd.Files)
            File.ID = -1;
        RoundTrip("anonymous", Bnd);
    }

    // The PC-save layout: names only, with an extra trailing ID per file header.
    {
        BND4 Bnd;
        Bnd.Extended = 0;
        Bnd.Format   = Format::Names1;
        AddFiles(Bnd, true);
        for (auto& File : Bnd.Files)
            File.Flags = FileFlags::Flag1;
        RoundTrip("names-only", Bnd);
    }

    // Wrapped in DCX as a whole, and per-file compression other than plain zlib.
    {
        BND4 Bnd;
        AddFiles(Bnd, true);
        Bnd.Files[1].CompressionType = DCX::Type::DCX_DFLT_10000_44_9;
        Bnd.Files[3].CompressionType = DCX::Type::DCX_ZSTD;
        Bnd.Compression              = DCX::Type::DCX_DFLT_11000_44_9;

        const auto Bytes = Bnd.Write();
        CHECK(DCX::Is(Bytes));
        BND4 Back = BND4::Read(Bytes);
        CHECK(Back.Compression == DCX::Type::DCX_DFLT_11000_44_9);
        CHECK(SameFiles(Bnd.Files, Back.Files, true));
        CHECK(Back.Files[1].CompressionType == DCX::Type::DCX_DFLT_10000_44_9);
        CHECK(Back.Files[3].CompressionType == DCX::Type::DCX_ZSTD);
    }

    // BND3: defaults, big-endian with long offsets, no IDs/names, DCX-wrapped.
    {
        BND3 Bnd;
        AddFiles(Bnd, true);
        RoundTrip("bnd3 default", Bnd);

        BND3 Big;
        Big.BigEndian    = true;
        Big.BitBigEndian = true;
        Big.Unk18        = static_cast<int32_t>(0x80000000);
        Big.Format = Format::BigEndian | Format::IDs | Format::Names1 | Format::Names2 | Format::LongOffsets | Format::Compression;
        AddFiles(Big, true);
        RoundTrip("bnd3 big-endian", Big);

        BND3 Anonymous;
        Anonymous.Format = Format::Compression;
        AddFiles(Anonymous, false);
        for (auto& File : Anonymous.Files) File.ID = -1;
        RoundTrip("bnd3 anonymous", Anonymous);

        BND3 Wrapped;
        AddFiles(Wrapped, true);
        Wrapped.Compression = DCX::Type::DCX_DFLT_10000_24_9;
        const auto Bytes    = Wrapped.Write();
        CHECK(DCX::Is(Bytes) && BND3::Is(Bytes) && !BND4::Is(Bytes));
        BND3 Back = BND3::Read(Bytes);
        CHECK(Back.Compression == DCX::Type::DCX_DFLT_10000_24_9 && SameFiles(Wrapped.Files, Back.Files, true));

        // Names with no way to encode them are rejected, not silently dropped.
        BND3 Nameless;
        AddFiles(Nameless, false);
        CHECK(Throws([&] { Nameless.Write(); }));
    }

    // BXF3 / BXF4 header+data pairs.
    {
        BXF3 Bxf3;
        AddFiles(Bxf3, true);
        RoundTripBxf("bxf3 default", Bxf3);

        BXF3 Bxf3Big;
        Bxf3Big.BigEndian    = true;
        Bxf3Big.BitBigEndian = true;
        Bxf3Big.Format       = Format::BigEndian | Format::IDs | Format::Names1 | Format::Names2 | Format::Compression;
        AddFiles(Bxf3Big, true);
        RoundTripBxf("bxf3 big-endian", Bxf3Big);

        BXF4 Bxf4;
        AddFiles(Bxf4, true);
        RoundTripBxf("bxf4 default", Bxf4);

        BXF4 Bxf4Big;
        Bxf4Big.BigEndian    = true;
        Bxf4Big.BitBigEndian = true;
        Bxf4Big.Extended     = 0;
        Bxf4Big.Unicode      = false;
        Bxf4Big.Unk04        = true;
        Bxf4Big.Format       = Format::BigEndian | Format::IDs | Format::Names1 | Format::Names2 | Format::LongOffsets;
        AddFiles(Bxf4Big, true);
        for (auto& File : Bxf4Big.Files) File.Flags = FileFlags::Flag1;
        RoundTripBxf("bxf4 big-endian", Bxf4Big);

        // The wrong half of a pair, or a different generation, is rejected.
        const BXFBytes Pair3 = Bxf3.Write();
        const BXFBytes Pair4 = Bxf4.Write();
        CHECK(!BXF4::IsBHD(Pair3.Header) && !BXF3::IsBHD(Pair4.Header));
        CHECK(Throws([&] { BXF3::Read(Pair3.Data, Pair3.Header); }));
        CHECK(Throws([&] { BXF4::Read(Pair3.Header, Pair3.Data); }));
    }

    // Detection and bad input.
    {
        BND4 Bnd;
        AddFiles(Bnd, true);
        auto Bytes = Bnd.Write();
        CHECK(!BND4::Is(std::vector<uint8_t> {'B', 'N', 'D', '3', 0, 0, 0, 0}));
        CHECK(!BND4::IsRead(std::vector<uint8_t> {1, 2, 3}).has_value());

        std::vector<uint8_t> Truncated(Bytes.begin(), Bytes.begin() + 0x80);
        CHECK(Throws([&] { BND4::Read(Truncated); }));
        std::vector<uint8_t> BadCount = Bytes;
        BadCount[0x0C]                = 0xFF;
        BadCount[0x0D]                = 0xFF;
        BadCount[0x0E]                = 0xFF;
        BadCount[0x0F]                = 0x7F;
        CHECK(Throws([&] { BND4::Read(BadCount); }));

        // A format that needs names can't be written without them.
        BND4 Nameless;
        AddFiles(Nameless, false);
        CHECK(Throws([&] { Nameless.Write(); }));
    }

    // BinderFile / flag helpers.
    {
        CHECK(Binder::GetBND4FileHeaderSize(Format::IDs | Format::Names1 | Format::Names2 | Format::Compression) ==
              0x24);
        CHECK(Binder::HasNames(Format::Names2) && !Binder::HasNames(Format::IDs));
        CHECK(Binder::IsCompressed(FileFlags::Compressed | FileFlags::Flag3));
        BinderFile File(FileFlags::Flag1, 7, "x.bin", {1, 2, 3});
        CHECK(File.ToString() == "Flags: 0x02 | ID: 7 | Name: x.bin | Length: 3");
    }

    std::printf(Failures == 0 ? "Binder tests passed\n" : "Binder tests: %d failure(s)\n", Failures);
    return Failures;
}

// Decrypts the real Elden Ring regulation.bin and checks that it survives a trip through the library.
int RunRegulationTests() {
    namespace fs = std::filesystem;

    const fs::path Path = "C:/Program Files (x86)/Steam/steamapps/common/ELDEN RING/Game/regulation.bin";
    if (!fs::exists(Path)) {
        std::printf("Regulation tests skipped (no regulation.bin at %s)\n", Path.string().c_str());
        return TestSkipped;
    }

    Failures = 0;
    try {
        BND4 Bnd = Regulation::DecryptER(Path);
        std::printf("regulation.bin: %zu files, version \"%s\", format 0x%02X, compression type %d\n",
                    Bnd.Files.size(),
                    Bnd.Version.c_str(),
                    static_cast<unsigned>(Bnd.Format),
                    static_cast<int>(Bnd.Compression));

        CHECK(!Bnd.Files.empty());
        bool AnyEquipParam = false;
        for (const BinderFile& File : Bnd.Files) {
            CHECK(File.Name.has_value() && !File.Name->empty());
            std::printf("BinderFile: %s\n", File.Name->c_str());

            CHECK(!File.Bytes.empty());
            if (File.Name && File.Name->find("EquipParamWeapon") != std::string::npos) AnyEquipParam = true;
        }
        CHECK(AnyEquipParam);

        // Rebuild the original uncompressed BND4 bytes to compare headers against.
        BinaryReader Raw(Path);
        const auto Encrypted = Raw.ReadBytes(static_cast<size_t>(Raw.Length()));
        const auto Decrypted = Crypto::DecryptAesCbc(Regulation::ERKey(), Encrypted);
        DCX::Type Type       = DCX::Type::Unknown;
        const auto Original  = DCX::Is(Decrypted) ? DCX::Decompress(Decrypted, Type) : Decrypted;
        CHECK(Type == Bnd.Compression);

        // Everything before the first file's data (headers, names, hash table) must be reproduced exactly.
        BinaryReader Header(Original);
        const int64_t HeadersEnd = Header.ReadAt<int64_t>(0x28);
        Bnd.Compression          = DCX::Type::None;
        const auto Rewritten     = Bnd.Write();
        CHECK(static_cast<int64_t>(Rewritten.size()) > HeadersEnd &&
              static_cast<int64_t>(Original.size()) > HeadersEnd);
        const bool HeadersMatch = std::equal(Original.begin(), Original.begin() + HeadersEnd, Rewritten.begin());
        if (!HeadersMatch) std::printf("  (regulation headers differ from the game's)\n");
        CHECK(HeadersMatch);

        // Full encrypt/decrypt round trip through a file.
        const fs::path Temp = fs::temp_directory_path() / "libsouls_regulation_test" / "regulation.bin";
        fs::remove_all(Temp.parent_path());
        // Level-21 zstd (what the game uses) is very slow, and DCX_ZSTD is covered elsewhere, so use deflate here.
        Bnd.Compression = DCX::Type::DCX_DFLT_11000_44_9_15;
        Regulation::EncryptER(Temp, Bnd);
        BND4 Back = Regulation::DecryptER(Temp);
        CHECK(Back.Compression == DCX::Type::DCX_DFLT_11000_44_9_15);
        CHECK(SameFiles(Bnd.Files, Back.Files, false));
        fs::remove_all(Temp.parent_path());
    } catch (const std::exception& E) {
        std::printf("FAIL regulation: %s\n", E.what());
        ++Failures;
    }

    std::printf(Failures == 0 ? "Regulation tests passed\n" : "Regulation tests: %d failure(s)\n", Failures);
    return Failures;
}
