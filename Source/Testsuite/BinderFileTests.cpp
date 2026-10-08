//
// Created by Jake Rieger on 10/7/2026.
//

#include "TestResult.hpp"

#include <libSouls/Binders/BND3.hpp>
#include <libSouls/Binders/BXF3.hpp>
#include <libSouls/Binders/BXF4.hpp>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <string>

namespace {
    namespace fs = std::filesystem;
    using namespace Souls;

    const fs::path DarkSoulsRoot = "C:/Program Files (x86)/Steam/steamapps/common/DARK SOULS REMASTERED";
    const fs::path EldenRingRoot = "C:/Program Files (x86)/Steam/steamapps/common/ELDEN RING/Game";

    int Failures = 0;

    void Fail(const fs::path& Path, const std::string& Message) {
        ++Failures;
        std::printf("FAIL %s: %s\n", Path.string().c_str(), Message.c_str());
    }

    std::vector<uint8_t> ReadAll(const fs::path& Path) {
        BinaryReader Reader(Path);
        return Reader.ReadBytes(static_cast<size_t>(Reader.Length()));
    }

    bool SameFiles(const std::vector<BinderFile>& A, const std::vector<BinderFile>& B) {
        if (A.size() != B.size()) return false;
        for (size_t I = 0; I < A.size(); ++I) {
            if (A[I].Flags != B[I].Flags || A[I].ID != B[I].ID || A[I].Name != B[I].Name || A[I].Bytes != B[I].Bytes) {
                return false;
            }
        }
        return true;
    }

    // Every file under Root whose name ends with one of the suffixes, sorted, then thinned to at most Max.
    std::vector<fs::path> Collect(const fs::path& Root, std::initializer_list<const char*> Suffixes, size_t Max) {
        std::vector<fs::path> Found;
        for (const auto& Entry : fs::recursive_directory_iterator(Root, fs::directory_options::skip_permission_denied)) {
            if (!Entry.is_regular_file()) continue;
            const std::string Name = Entry.path().filename().string();
            for (const char* Suffix : Suffixes) {
                const std::string S = Suffix;
                if (Name.size() >= S.size() && Name.compare(Name.size() - S.size(), S.size(), S) == 0) {
                    Found.push_back(Entry.path());
                    break;
                }
            }
        }
        std::sort(Found.begin(), Found.end());
        const size_t Step = std::max<size_t>(1, Found.size() / std::max<size_t>(1, Max));
        std::vector<fs::path> Sampled;
        for (size_t I = 0; I < Found.size(); I += Step) Sampled.push_back(Found[I]);
        return Sampled;
    }

    // Dark Souls Remastered ships BND3 binders (some wrapped in DCX).
    void TestBND3(size_t Max) {
        int Tested = 0, Mismatched = 0;
        for (const fs::path& Path : Collect(DarkSoulsRoot, {"bnd", "bnd.dcx"}, Max)) {
            try {
                if (!BND3::Is(Path)) continue;
                ++Tested;

                BND3 Bnd = BND3::Read(Path);
                if (Bnd.Files.empty()) {
                    Fail(Path, "binder has no files");
                    continue;
                }

                // The original uncompressed binder bytes, to compare headers against.
                std::vector<uint8_t> Original = ReadAll(Path);
                if (DCX::Is(Original)) Original = DCX::Decompress(Original);

                const bool Big = Bnd.BigEndian || Binder::ForceBigEndian(Bnd.Format);
                BinaryReader Header(Original, Big ? Endian::Big : Endian::Little);
                const int32_t HeadersEnd = Header.ReadAt<int32_t>(0x14);

                const DCX::Type Compression = Bnd.Compression;
                Bnd.Compression             = DCX::Type::None;
                const std::vector<uint8_t> Rewritten = Bnd.Write();
                if (HeadersEnd <= 0 || static_cast<size_t>(HeadersEnd) > Original.size() ||
                    static_cast<size_t>(HeadersEnd) > Rewritten.size() ||
                    !std::equal(Original.begin(), Original.begin() + HeadersEnd, Rewritten.begin())) {
                    ++Mismatched;
                    Fail(Path, "rewritten headers differ from the original");
                }

                BND3 Back = BND3::Read(Rewritten);
                if (!SameFiles(Bnd.Files, Back.Files) || Back.Format != Bnd.Format || Back.Unk18 != Bnd.Unk18) {
                    Fail(Path, "files differ after rewrite");
                }

                // Re-wrapping with the original compression must also read back.
                if (Compression != DCX::Type::None) {
                    Bnd.Compression = Compression;
                    if (!SameFiles(Bnd.Files, BND3::Read(Bnd.Write()).Files)) Fail(Path, "files differ after DCX rewrite");
                }
            } catch (const std::exception& E) {
                Fail(Path, E.what());
            }
        }
        std::printf("BND3: %d real binders checked, %d header mismatches\n", Tested, Mismatched);
        if (Tested == 0) std::printf("  (no BND3 binders found)\n");
    }

    template<typename B>
    void TestPairs(const char* Label, const fs::path& Root, std::initializer_list<const char*> HeaderSuffixes, size_t Max) {
        int Tested = 0, HeaderMismatches = 0;
        for (const fs::path& HeaderPath : Collect(Root, HeaderSuffixes, Max)) {
            fs::path DataPath = HeaderPath;
            DataPath.replace_extension(HeaderPath.extension().string().replace(HeaderPath.extension().string().size() - 3, 3, "bdt"));
            if (!fs::exists(DataPath)) continue;
            try {
                if (!B::IsBHD(HeaderPath) || !B::IsBDT(DataPath)) continue;
                ++Tested;

                B Bxf = B::Read(HeaderPath, DataPath);
                if (Bxf.Files.empty()) {
                    Fail(HeaderPath, "binder has no files");
                    continue;
                }

                const BXFBytes Rewritten = Bxf.Write();
                const std::vector<uint8_t> OriginalHeader = ReadAll(HeaderPath);
                if (Rewritten.Header != OriginalHeader) {
                    ++HeaderMismatches;
                    size_t At = 0;
                    while (At < OriginalHeader.size() && At < Rewritten.Header.size() &&
                           OriginalHeader[At] == Rewritten.Header[At]) {
                        ++At;
                    }
                    Fail(HeaderPath,
                         "rewritten header differs from the original at offset " + std::to_string(At) + " (sizes " +
                           std::to_string(OriginalHeader.size()) + " vs " + std::to_string(Rewritten.Header.size()) + ")");
                }

                B Back = B::Read(Rewritten.Header, Rewritten.Data);
                if (!SameFiles(Bxf.Files, Back.Files)) Fail(HeaderPath, "files differ after rewrite");
            } catch (const std::exception& E) {
                Fail(HeaderPath, E.what());
            }
        }
        std::printf("%s: %d real header/data pairs checked, %d header mismatches\n", Label, Tested, HeaderMismatches);
        if (Tested == 0) std::printf("  (no pairs found)\n");
    }
}  // namespace

// Reads real BND3, BXF3 and BXF4 files from installed games and checks they survive a rewrite. Samples at most
// MaxFiles of each kind. Skipped if neither game is installed.
int RunBinderFileTests(size_t MaxFiles) {
    const bool HaveDS = fs::exists(DarkSoulsRoot);
    const bool HaveER = fs::exists(EldenRingRoot);
    if (!HaveDS && !HaveER) {
        std::printf("Binder file tests skipped (no game installs found)\n");
        return TestSkipped;
    }

    Failures = 0;
    if (HaveDS) {
        TestBND3(MaxFiles);
        TestPairs<BXF3>("BXF3", DarkSoulsRoot, {".tpfbhd", ".hkxbhd"}, MaxFiles);
    } else {
        std::printf("BND3/BXF3 skipped (Dark Souls Remastered not installed)\n");
    }
    if (HaveER) {
        TestPairs<BXF4>("BXF4", EldenRingRoot, {".tpfbhd"}, MaxFiles);
    } else {
        std::printf("BXF4 skipped (Elden Ring not installed)\n");
    }

    std::printf(Failures == 0 ? "Binder file tests passed\n" : "Binder file tests: %d failure(s)\n", Failures);
    return Failures;
}
