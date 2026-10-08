//
// Created by Jake Rieger on 10/7/2026.
//

#include "TestResult.hpp"

#include <libSouls/Binders/BXF3.hpp>
#include <libSouls/Binders/BXF4.hpp>
#include <libSouls/Binders/BXFReader.hpp>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>

namespace {
    int Failures = 0;

#define CHECK(Cond)                                                     \
    do {                                                                \
        if (!(Cond)) {                                                  \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #Cond); \
            ++Failures;                                                 \
        }                                                               \
    } while (0)

    using namespace Souls;
    namespace fs = std::filesystem;

    template<typename F>
    bool Throws(F&& Fn) {
        try {
            Fn();
        } catch (...) {
            return true;
        }
        return false;
    }

    std::vector<uint8_t> MakeDDS(int Width, size_t Pixels) {
        DDS Header;
        Header.Width         = Width;
        Header.Height        = Width;
        Header.MipMapCount   = 1;
        Header.Pixels.Flags  = DDS::DDPF::FOURCC;
        Header.Pixels.FourCC = "DXT1";
        std::vector<uint8_t> Data(Pixels);
        for (size_t I = 0; I < Data.size(); ++I) Data[I] = static_cast<uint8_t>(I + Width);
        return Header.Write(Data);
    }

    // A TPF with one texture, wrapped in DCX the way the .tpfbdt files hold them.
    std::vector<uint8_t> MakeTpfFile(const std::string& TextureName, int Width) {
        TPF Tpf;
        Tpf.Textures.emplace_back(TextureName, 9, 0, MakeDDS(Width, 64));
        Tpf.Compression = DCX::Type::DCX_DFLT_10000_44_9;
        return Tpf.Write();
    }

    template<typename Bxf>
    void FillWithTpfs(Bxf& Container) {
        for (int I = 0; I < 5; ++I) {
            const std::string Name = "tex" + std::to_string(I);
            BinderFile File(Binder::FileFlags::Flag1, I * 10, "N:\\textures\\" + Name + ".tpf.dcx", MakeTpfFile(Name, 4 << I));
            Container.Files.push_back(std::move(File));
        }
        // A file the binder compresses itself, and an empty one.
        BinderFile Packed(Binder::FileFlags::Flag1 | Binder::FileFlags::Compressed, 50, "N:\\textures\\packed.bin", std::vector<uint8_t>(3000, 0x5A));
        Container.Files.push_back(std::move(Packed));
        Container.Files.push_back(BinderFile(Binder::FileFlags::Flag1, 60, "N:\\textures\\empty.bin", {}));
    }

    template<typename Bxf>
    void TestPair(const char* Label, Bxf& Container, BXFReader::Generation Expected) {
        const fs::path Dir = fs::temp_directory_path() / (std::string("libsouls_bxfreader_") + Label);
        fs::remove_all(Dir);
        try {
            Container.Write(Dir / "pair.tpfbhd", Dir / "pair.tpfbdt");
            const BXFBytes Bytes = Container.Write();

            for (int Variant = 0; Variant < 3; ++Variant) {
                // Header and data from files, header from memory with a data file, and both from memory.
                std::unique_ptr<BXFReader> Reader;
                if (Variant == 0) Reader = std::make_unique<BXFReader>(Dir / "pair.tpfbhd", Dir / "pair.tpfbdt");
                if (Variant == 1) Reader = std::make_unique<BXFReader>(Bytes.Header, Dir / "pair.tpfbdt");
                if (Variant == 2) Reader = std::make_unique<BXFReader>(Bytes.Header, Bytes.Data);

                CHECK(Reader->Kind == Expected && Reader->Format == Container.Format && Reader->Version == Container.Version);
                CHECK(Reader->BigEndian == Container.BigEndian && Reader->BitBigEndian == Container.BitBigEndian);
                CHECK(Reader->FileCount() == Container.Files.size());

                for (size_t I = 0; I < Container.Files.size(); ++I) {
                    const BinderFile& Expect = Container.Files[I];
                    const auto& Info = Reader->File(I);
                    CHECK(Info.Name == Expect.Name && Info.ID == Expect.ID && Info.Flags == Expect.Flags);
                    CHECK(Info.IsCompressed() == Binder::IsCompressed(Expect.Flags));
                    CHECK(Reader->ReadFile(I).Bytes == Expect.Bytes);  // compressed files come back decompressed
                }

                // Names.
                CHECK(Reader->IndexOf("N:\\textures\\tex3.tpf.dcx") == std::optional<size_t>(3));
                CHECK(!Reader->IndexOf("n:\\textures\\tex3.tpf.dcx").has_value());
                CHECK(Reader->IndexOf("n:\\textures\\tex3.tpf.dcx", true) == std::optional<size_t>(3));
                CHECK(Reader->IndexOfFileName("TEX2.TPF.DCX") == std::optional<size_t>(2));
                CHECK(!Reader->IndexOf("missing").has_value() && !Reader->IndexOfFileName("missing.tpf").has_value());

                // Textures.
                for (size_t I = 0; I < 5; ++I) {
                    const TPF Tpf = Reader->ReadTPF(I);
                    CHECK(Tpf.Textures.size() == 1 && Tpf.Textures[0].Name == "tex" + std::to_string(I));
                    CHECK(Tpf.Textures[0].ReadDDS().Width == (4 << I));
                    CHECK(Tpf.Compression == DCX::Type::DCX_DFLT_10000_44_9);
                }
                CHECK(Reader->ReadTPF("tex4.tpf.dcx").Textures[0].Name == "tex4");

                // Mistakes.
                CHECK(Throws([&] { Reader->File(99); }) && Throws([&] { Reader->ReadFile(99); }));
                CHECK(Throws([&] { Reader->ReadTPF("nope.tpf"); }));
                CHECK(Throws([&] { Reader->ReadTPF(5); }));  // a file that isn't a TPF
            }

            // Reading while the pair is also open elsewhere, and a moved reader keeps working.
            BXFReader A(Dir / "pair.tpfbhd", Dir / "pair.tpfbdt");
            BXFReader B = std::move(A);
            CHECK(B.ReadTPF(0).Textures.size() == 1);

            // The wrong data file, or a header that isn't one.
            CHECK(Throws([&] { BXFReader(Dir / "pair.tpfbdt", Dir / "pair.tpfbdt"); }));
            CHECK(Throws([&] { BXFReader(Dir / "pair.tpfbhd", Dir / "pair.tpfbhd"); }));
            CHECK(Throws([&] { BXFReader(Dir / "nope.tpfbhd", Dir / "pair.tpfbdt"); }));
            CHECK(Throws([&] { BXFReader(std::vector<uint8_t>{1, 2}, Dir / "pair.tpfbdt"); }));
        } catch (const std::exception& E) {
            std::printf("FAIL %s: %s\n", Label, E.what());
            ++Failures;
        }
        fs::remove_all(Dir);
    }

    bool EndsWith(const std::string& Text, const std::string& Suffix) {
        return Text.size() >= Suffix.size() && Text.compare(Text.size() - Suffix.size(), Suffix.size(), Suffix) == 0;
    }

    // Opens a real pair with the reader, reads every texture file (or a sample of a huge one), and checks the
    // reader and the load-everything BXF classes agree.
    template<typename Bxf>
    void TestRealPair(const fs::path& Header, const fs::path& Data, size_t MaxFiles, bool CompareWithFull) {
        using Clock = std::chrono::steady_clock;
        const auto Start = Clock::now();
        BXFReader Reader(Header, Data);
        const double OpenSeconds = std::chrono::duration<double>(Clock::now() - Start).count();

        const size_t Count = Reader.FileCount();
        const size_t Step  = std::max<size_t>(1, Count / std::max<size_t>(1, MaxFiles));
        size_t Read = 0, Textures = 0, Failed = 0;
        for (size_t I = 0; I < Count; I += Step) {
            try {
                const auto& Info = Reader.File(I);
                if (!Info.Name || !(EndsWith(*Info.Name, ".tpf") || EndsWith(*Info.Name, ".tpf.dcx"))) continue;
                const TPF Tpf = Reader.ReadTPF(I);
                ++Read;
                Textures += Tpf.Textures.size();
                if (Tpf.Textures.empty()) ++Failed;
            } catch (const std::exception& E) {
                if (Failed++ < 3) std::printf("FAIL %s file %zu: %s\n", Header.filename().string().c_str(), I, E.what());
            }
        }
        std::printf("%s: %s, %zu files; opened in %.2fs; read %zu TPFs (%zu textures), %zu failures\n",
                    Header.filename().string().c_str(), Reader.Kind == BXFReader::Generation::BXF3 ? "BXF3" : "BXF4", Count,
                    OpenSeconds, Read, Textures, Failed);
        Failures += static_cast<int>(Failed);
        CHECK(Count > 0 && Read > 0);

        if (CompareWithFull) {
            // The small pairs: every file the reader returns matches what the whole-archive reader gives.
            const Bxf Full = Bxf::Read(Header, Data);
            CHECK(Full.Files.size() == Count);
            bool Same = Full.Files.size() == Count;
            for (size_t I = 0; Same && I < Count; ++I) {
                Same = Full.Files[I].Name == Reader.File(I).Name && Full.Files[I].Bytes == Reader.ReadBytes(I);
            }
            if (!Same) std::printf("FAIL %s: reader and BXF disagree\n", Header.filename().string().c_str());
            CHECK(Same);
        }
    }
}  // namespace

int RunBXFReaderTests() {
    Failures = 0;

    {
        BXF3 Container;
        FillWithTpfs(Container);
        TestPair("bxf3", Container, BXFReader::Generation::BXF3);

        BXF3 Big;
        Big.BigEndian = Big.BitBigEndian = true;
        Big.Format = Binder::Format::BigEndian | Binder::Format::IDs | Binder::Format::Names1 | Binder::Format::Names2 | Binder::Format::Compression;
        FillWithTpfs(Big);
        TestPair("bxf3big", Big, BXFReader::Generation::BXF3);
    }
    {
        BXF4 Container;
        FillWithTpfs(Container);
        TestPair("bxf4", Container, BXFReader::Generation::BXF4);
    }

    // Real texture archives.
    const fs::path Steam = "C:/Program Files (x86)/Steam/steamapps/common";
    const fs::path DarkSouls = Steam / "DARK SOULS REMASTERED/map";
    if (fs::exists(DarkSouls)) {
        size_t Pairs = 0;
        for (const auto& Entry : fs::recursive_directory_iterator(DarkSouls)) {
            if (!Entry.is_regular_file() || Entry.path().extension() != ".tpfbhd") continue;
            fs::path Data = Entry.path();
            Data.replace_extension(".tpfbdt");
            if (!fs::exists(Data)) continue;
            if (++Pairs > 8) break;  // a handful is plenty; they're all alike
            TestRealPair<BXF3>(Entry.path(), Data, 1000, true);
        }
    }
    const fs::path EldenRing = Steam / "ELDEN RING/Game/menu";
    if (fs::exists(EldenRing)) {
        for (const char* Pair : {"hi/00_solo", "low/00_solo"}) {
            const fs::path Header = EldenRing / (std::string(Pair) + ".tpfbhd");
            const fs::path Data   = EldenRing / (std::string(Pair) + ".tpfbdt");
            if (fs::exists(Header) && fs::exists(Data)) TestRealPair<BXF4>(Header, Data, 1000, true);
        }
        // The big one: 28k files in a 1.2 GB data file. Only a sample is read, and it isn't loaded whole.
        const fs::path Header = EldenRing / "71_maptile.tpfbhd";
        const fs::path Data   = EldenRing / "71_maptile.tpfbdt";
        if (fs::exists(Header) && fs::exists(Data)) TestRealPair<BXF4>(Header, Data, 300, false);
    }

    std::printf(Failures == 0 ? "BXFReader tests passed\n" : "BXFReader tests: %d failure(s)\n", Failures);
    return Failures;
}
