//
// Created by Jake Rieger on 10/7/2026.
//

#include "TestResult.hpp"

#include <libSouls/Formats/TPF.hpp>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <map>
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
    using Platform = TPF::TPFPlatform;

    template<typename F>
    bool Throws(F&& Fn) {
        try {
            Fn();
        } catch (...) {
            return true;
        }
        return false;
    }

    // A small but real-looking DDS file: a DXT1 header with the given dimensions and mipmaps, and fake pixel data.
    std::vector<uint8_t> MakeDDS(int Width, int Height, int Mips, DDS::DDSCAPS2 Caps2 = static_cast<DDS::DDSCAPS2>(0), size_t Pixels = 64) {
        DDS Header;
        Header.Width       = Width;
        Header.Height      = Height;
        Header.MipMapCount = Mips;
        Header.Pixels.Flags  = DDS::DDPF::FOURCC;
        Header.Pixels.FourCC = "DXT1";
        Header.Caps2         = Caps2;
        std::vector<uint8_t> Data(Pixels);
        for (size_t I = 0; I < Data.size(); ++I) Data[I] = static_cast<uint8_t>(I * 7 + Width);
        return Header.Write(Data);
    }

    bool SameTextures(const std::vector<TPF::Texture>& A, const std::vector<TPF::Texture>& B) {
        if (A.size() != B.size()) return false;
        for (size_t I = 0; I < A.size(); ++I) {
            const auto& X = A[I];
            const auto& Y = B[I];
            if (X.Name != Y.Name || X.Format != Y.Format || X.Type != Y.Type || X.Mipmaps != Y.Mipmaps || X.Flags1 != Y.Flags1 ||
                X.Bytes != Y.Bytes || X.Header.has_value() != Y.Header.has_value() || X.Floats.has_value() != Y.Floats.has_value()) {
                return false;
            }
            if (X.Header) {
                const auto& H = *X.Header;
                const auto& G = *Y.Header;
                if (H.Width != G.Width || H.Height != G.Height || H.TextureCount != G.TextureCount || H.Unk1 != G.Unk1 ||
                    H.Unk2 != G.Unk2 || H.DXGIFormat != G.DXGIFormat) {
                    return false;
                }
            }
            if (X.Floats && (X.Floats->Unk00 != Y.Floats->Unk00 || X.Floats->Values != Y.Floats->Values)) return false;
        }
        return true;
    }

    void TestDDS() {
        const auto Bytes = MakeDDS(256, 128, 9);
        const DDS Header = DDS::Read(Bytes);
        CHECK(Header.Width == 256 && Header.Height == 128 && Header.MipMapCount == 9);
        CHECK(Header.Pixels.FourCC == "DXT1" && Header.DataOffset() == 0x80 && !Header.IsCubemap() && !Header.IsVolume());
        CHECK(Bytes.size() == 0x80 + 64);
        CHECK(DDS::Read(MakeDDS(8, 8, 1, DDS::DDSCAPS2::CUBEMAP)).IsCubemap());
        CHECK(DDS::Read(MakeDDS(8, 8, 1, DDS::DDSCAPS2::VOLUME)).IsVolume());

        // The DX10 extension.
        DDS Extended;
        Extended.Pixels.FourCC = "DX10";
        Extended.DX10          = DDS::Header10{};
        Extended.DX10->Format  = DDS::DXGIFormat::BC7_UNORM;
        Extended.DX10->ArraySize = 6;
        const auto ExtendedBytes = Extended.Write(std::vector<uint8_t>(16, 0xAB));
        CHECK(ExtendedBytes.size() == 0x94 + 16);
        const DDS Back = DDS::Read(ExtendedBytes);
        CHECK(Back.DataOffset() == 0x94 && Back.DX10 && Back.DX10->Format == DDS::DXGIFormat::BC7_UNORM && Back.DX10->ArraySize == 6);
        CHECK(DDS::Read(Back.Write({})).DX10->Format == DDS::DXGIFormat::BC7_UNORM);

        CHECK(Throws([] { DDS::Read(std::vector<uint8_t>(0x80, 0)); }));
        CHECK(Throws([&] { DDS::Read(std::span<const uint8_t>(Bytes.data(), 0x40)); }));
    }

    void TestSynthetic() {
        // A PC TPF in each name encoding.
        for (const uint8_t Encoding : {uint8_t{0}, uint8_t{1}, uint8_t{2}}) {
            TPF Source;
            Source.Encoding = Encoding;
            Source.Flag2    = 3;
            Source.Textures.emplace_back("plain", 9, 0, MakeDDS(64, 64, 7));
            Source.Textures.emplace_back("cube", 5, 1, MakeDDS(32, 32, 1, DDS::DDSCAPS2::CUBEMAP, 200));
            Source.Textures.emplace_back("volume", 5, 0, MakeDDS(16, 16, 1, DDS::DDSCAPS2::VOLUME, 100));
            Source.Textures.emplace_back("\xE3\x82\xBD" "name", 1, 0, MakeDDS(4, 4, 3, static_cast<DDS::DDSCAPS2>(0), 33));  // odd data size
            Source.Textures[0].Floats = TPF::FloatStruct{42, {1.5f, -2.f, 0.25f}};

            CHECK(Source.Textures[0].Type == TPF::TexType::Texture && Source.Textures[1].Type == TPF::TexType::Cubemap &&
                  Source.Textures[2].Type == TPF::TexType::Volume);
            CHECK(Source.Textures[0].Mipmaps == 7 && Source.Textures[0].ToString() == "[9 Texture] plain");

            const auto Bytes = Source.Write();
            CHECK(TPF::Is(Bytes));
            TPF Back = TPF::Read(Bytes);
            const bool Same = SameTextures(Source.Textures, Back.Textures) && Back.Platform == Platform::PC &&
                              Back.Encoding == Encoding && Back.Flag2 == 3;
            if (!Same) std::printf("  (encoding %d: textures differ)\n", Encoding);
            CHECK(Same);
            CHECK(Back.Write() == Bytes);
        }

        // Data alignment: new files pad to 4; a file read back keeps whatever its offsets already satisfy.
        {
            TPF Tpf;
            CHECK(Tpf.DataAlignment == 4);
            Tpf.Textures.emplace_back("a", 0, 0, MakeDDS(8, 8, 1, static_cast<DDS::DDSCAPS2>(0), 1));  // odd size
            Tpf.Textures.emplace_back("b", 0, 0, MakeDDS(8, 8, 1, static_cast<DDS::DDSCAPS2>(0), 3));
            Tpf.Textures.emplace_back("c", 0, 0, MakeDDS(8, 8, 1, static_cast<DDS::DDSCAPS2>(0), 5));
            const auto Padded = Tpf.Write();
            CHECK(TPF::Read(Padded).DataAlignment >= 4);

            Tpf.DataAlignment = 1;  // no padding at all, like Elden Ring's files
            const auto Packed = Tpf.Write();
            CHECK(Packed.size() < Padded.size());
            TPF PackedBack = TPF::Read(Packed);
            CHECK(PackedBack.DataAlignment == 1 && SameTextures(Tpf.Textures, PackedBack.Textures));
            CHECK(PackedBack.Write() == Packed);  // an unchanged file writes back as it was

            Tpf.DataAlignment = 16;
            const auto Wide = Tpf.Write();
            CHECK(TPF::Read(Wide).DataAlignment == 16 && Wide.size() > Padded.size());
            Tpf.DataAlignment = 0;
            CHECK(Throws([&] { Tpf.Write(); }));
        }

        // Changing a texture's DDS updates its type and mipmap count on write.
        {
            TPF Tpf;
            Tpf.Textures.emplace_back("t", 0, 0, MakeDDS(8, 8, 2));
            Tpf.Textures[0].Bytes = MakeDDS(8, 8, 5, DDS::DDSCAPS2::CUBEMAP);
            const TPF Back = TPF::Read(Tpf.Write());
            CHECK(Back.Textures[0].Type == TPF::TexType::Cubemap && Back.Textures[0].Mipmaps == 5);
        }

        // Console platforms keep their headerless data and extra metadata, in the platform's byte order.
        {
            auto Make = [](Platform Target) {
                TPF Tpf;
                Tpf.Platform = Target;
                Tpf.Flag2    = 3;
                for (int I = 0; I < 3; ++I) {
                    TPF::Texture Texture;
                    Texture.Name    = "tex" + std::to_string(I);
                    Texture.Format  = static_cast<uint8_t>(10 + I);
                    Texture.Mipmaps = static_cast<uint8_t>(I + 1);
                    Texture.Bytes.assign(static_cast<size_t>(40 + I * 8), static_cast<uint8_t>(I + 1));
                    TPF::TexHeader Header;
                    Header.Width  = static_cast<int16_t>(128 << I);
                    Header.Height = 64;
                    if (Target == Platform::PS3) {
                        Header.Unk1 = 7;
                        Header.Unk2 = I == 1 ? 0xAAE4 : 0;
                    }
                    if (Target == Platform::PS4 || Target == Platform::Xbone) {
                        Header.TextureCount = I == 2 ? 6 : 1;
                        Header.Unk2         = 0xD;
                        Header.DXGIFormat   = 71 + I;
                        Texture.Type        = I == 2 ? TPF::TexType::Cubemap : TPF::TexType::Texture;
                    }
                    Texture.Header = Header;
                    Tpf.Textures.push_back(std::move(Texture));
                }
                return Tpf;
            };
            for (const Platform Target : {Platform::Xbox360, Platform::PS3, Platform::PS4, Platform::Xbone}) {
                TPF Source = Make(Target);
                const auto Bytes = Source.Write();
                TPF Back = TPF::Read(Bytes);
                const bool Same = Back.Platform == Target && SameTextures(Source.Textures, Back.Textures);
                if (!Same) std::printf("  (platform %d: textures differ)\n", static_cast<int>(Target));
                CHECK(Same);
                CHECK(Back.Write() == Bytes);
                const bool Big = Target == Platform::Xbox360 || Target == Platform::PS3;
                CHECK(Big ? (Bytes[8] == 0 && Bytes[11] == 3) : (Bytes[8] == 3 && Bytes[11] == 0));  // texture count 3
            }

            // PS3 with Flag2 == 0 leaves out the second unknown.
            TPF NoUnk2 = Make(Platform::PS3);
            NoUnk2.Flag2 = 0;
            for (auto& Texture : NoUnk2.Textures) Texture.Header->Unk2 = 0;
            const TPF NoUnk2Back = TPF::Read(NoUnk2.Write());
            CHECK(SameTextures(NoUnk2.Textures, NoUnk2Back.Textures));

            // A console texture without its header can't be written.
            TPF Headerless = Make(Platform::PS3);
            Headerless.Textures[1].Header.reset();
            CHECK(Throws([&] { Headerless.Write(); }));
        }

        // Detection, bad input, and what isn't supported.
        {
            TPF Tpf;
            Tpf.Textures.emplace_back("t", 0, 0, MakeDDS(8, 8, 1));
            auto Bytes = Tpf.Write();
            CHECK(!TPF::Is(std::vector<uint8_t>{'T', 'P', 'X', 0}) && !TPF::IsRead(std::vector<uint8_t>(64, 0xAB)).has_value());
            CHECK(Throws([&] { TPF::Read(std::vector<uint8_t>(64, 0xAB)); }));
            std::vector<uint8_t> Truncated(Bytes.begin(), Bytes.begin() + 0x20);
            CHECK(Throws([&] { TPF::Read(Truncated); }));
            std::vector<uint8_t> BadPlatform = Bytes;
            BadPlatform[0xC] = 3;
            CHECK(Throws([&] { TPF::Read(BadPlatform); }));
            std::vector<uint8_t> BadCount = Bytes;
            BadCount[0xB] = 0x7F;
            CHECK(Throws([&] { TPF::Read(BadCount); }));
            CHECK(Throws([] { TPF::Texture("x", 0, 0, std::vector<uint8_t>(10)); }));  // not a DDS

            TPF Edge;
            Edge.Textures.emplace_back("t", 0, 2, MakeDDS(8, 8, 1));
            CHECK(Throws([&] { Edge.Write(); }));
        }

        // Through DCX.
        {
            TPF Tpf;
            Tpf.Textures.emplace_back("t", 0, 0, MakeDDS(8, 8, 1));
            Tpf.Compression = DCX::Type::DCX_DFLT_10000_44_9;
            const TPF Back = TPF::Read(Tpf.Write());
            CHECK(Back.Compression == DCX::Type::DCX_DFLT_10000_44_9 && SameTextures(Tpf.Textures, Back.Textures));
        }
    }

    bool EndsWith(const std::string& Text, const std::string& Suffix) {
        return Text.size() >= Suffix.size() && Text.compare(Text.size() - Suffix.size(), Suffix.size(), Suffix) == 0;
    }

    std::vector<fs::path> Collect(const fs::path& Root, size_t Max) {
        std::vector<fs::path> Found;
        if (!fs::exists(Root)) return Found;
        for (const auto& Entry : fs::recursive_directory_iterator(Root, fs::directory_options::skip_permission_denied)) {
            if (!Entry.is_regular_file()) continue;
            const std::string Name = Entry.path().filename().string();
            if (EndsWith(Name, ".tpf.dcx") || EndsWith(Name, ".tpf")) Found.push_back(Entry.path());
        }
        std::sort(Found.begin(), Found.end());
        const size_t Step = std::max<size_t>(1, Found.size() / std::max<size_t>(1, Max));
        std::vector<fs::path> Sampled;
        for (size_t I = 0; I < Found.size(); I += Step) Sampled.push_back(Found[I]);
        return Sampled;
    }

    void TestGame(const char* Label, const fs::path& Root, size_t Max) {
        int Files = 0, Textures = 0, Identical = 0, Failed = 0, Cubemaps = 0, Volumes = 0, WithMips = 0;
        std::map<int, int> Formats;
        std::map<std::string, int> FourCCs;
        for (const fs::path& Path : Collect(Root, Max)) {
            try {
                BinaryReader Raw(Path);
                std::vector<uint8_t> Original = Raw.ReadBytes(static_cast<size_t>(Raw.Length()));
                if (DCX::Is(Original)) Original = DCX::Decompress(Original);
                if (!TPF::Is(Original)) {
                    std::printf("FAIL %s is not recognized as a TPF\n", Path.string().c_str());
                    ++Failed;
                    continue;
                }

                TPF Tpf = TPF::Read(Original);
                ++Files;
                if (const char* DumpDir = std::getenv("TPF_DUMP_DIR")) {
                    BinaryWriter Out(fs::path(DumpDir) / (std::string(Label).substr(0, 4) + "_" + Path.filename().string() + ".orig"));
                    Out.WriteBytes(Original);
                    Out.Finish();
                }
                Textures += static_cast<int>(Tpf.Textures.size());
                for (const TPF::Texture& Texture : Tpf.Textures) {
                    ++Formats[Texture.Format];
                    if (Texture.Type == TPF::TexType::Cubemap) ++Cubemaps;
                    if (Texture.Type == TPF::TexType::Volume) ++Volumes;
                    if (Texture.Mipmaps > 1) ++WithMips;
                    if (Tpf.Platform == Platform::PC) {
                        // Every PC texture is a real DDS whose data starts inside the file.
                        const DDS Header = Texture.ReadDDS();
                        ++FourCCs[Header.DX10 ? "DX10" : Header.Pixels.FourCC];
                        if (Header.Width <= 0 || Header.Height <= 0 || static_cast<size_t>(Header.DataOffset()) > Texture.Bytes.size()) {
                            std::printf("FAIL %s: texture %s has a bad DDS header\n", Path.string().c_str(), Texture.Name.c_str());
                            ++Failed;
                        }
                    }
                }

                Tpf.Compression = DCX::Type::None;
                const auto Rewritten = Tpf.Write();
                const TPF Back = TPF::Read(Rewritten);
                if (!SameTextures(Tpf.Textures, Back.Textures) || Back.Platform != Tpf.Platform) {
                    std::printf("FAIL %s: textures changed after a rewrite\n", Path.string().c_str());
                    ++Failed;
                }
                if (Rewritten == Original) {
                    ++Identical;
                } else if (std::getenv("TPF_DEBUG") && Failed < 100) {
                    static int Shown = 0;
                    if (Shown++ < 6) {
                        size_t At = 0;
                        while (At < Rewritten.size() && At < Original.size() && Rewritten[At] == Original[At]) ++At;
                        std::printf("  diff %s: first at 0x%zX, sizes orig 0x%zX rewritten 0x%zX, %zu textures, platform %d, flag2 %d\n",
                                    Path.filename().string().c_str(), At, Original.size(), Rewritten.size(), Tpf.Textures.size(),
                                    static_cast<int>(Tpf.Platform), Tpf.Flag2);
                        const std::vector<uint8_t>* Both[2] = {&Original, &Rewritten};
                        for (const auto* B : Both) {
                            std::printf("   ");
                            for (size_t I = At >= 8 ? At - 8 : 0; I < At + 24 && I < B->size(); ++I) std::printf(" %02X", (*B)[I]);
                            std::printf("\n");
                        }
                    }
                }
            } catch (const std::exception& E) {
                std::printf("FAIL %s: %s\n", Path.string().c_str(), E.what());
                ++Failed;
            }
        }
        std::printf("%s: %d TPFs, %d textures (%d cubemaps, %d volumes, %d with mipmaps), %d byte-identical rewrites, %d failures\n",
                    Label, Files, Textures, Cubemaps, Volumes, WithMips, Identical, Failed);
        std::printf("  DDS formats:");
        for (const auto& [FourCC, Count] : FourCCs) {
            std::string Printable;
            for (char C : FourCC) Printable += (C >= 32 && C < 127) ? C : '.';
            std::printf(" %s x%d", Printable.c_str(), Count);
        }
        std::printf("\n");
        Failures += Failed;
        if (Files == 0) std::printf("  (no unpacked files for %s; skipped)\n", Label);
    }
}  // namespace

int RunTPFTests(size_t MaxFiles) {
    Failures = 0;
    TestDDS();
    TestSynthetic();

    const fs::path Steam = "C:/Program Files (x86)/Steam/steamapps/common";
    if (fs::exists(Steam / "ELDEN RING/Game")) TestGame("Elden Ring", Steam / "ELDEN RING/Game", MaxFiles);
    if (fs::exists(Steam / "DARK SOULS REMASTERED")) TestGame("Dark Souls Remastered", Steam / "DARK SOULS REMASTERED", MaxFiles);

    std::printf(Failures == 0 ? "TPF tests passed\n" : "TPF tests: %d failure(s)\n", Failures);
    return Failures;
}
