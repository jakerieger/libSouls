//
// Created by Jake Rieger on 10/7/2026.
//

#include "TestResult.hpp"

#include <libSouls/Binders/BND3.hpp>
#include <libSouls/Binders/BND4.hpp>
#include <libSouls/Formats/FMG.hpp>

#include <map>

#include <algorithm>
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

    bool SameEntries(const std::vector<FMG::Entry>& A, const std::vector<FMG::Entry>& B) {
        if (A.size() != B.size()) return false;
        for (size_t I = 0; I < A.size(); ++I) {
            if (A[I].ID != B[I].ID || A[I].Text != B[I].Text) return false;
        }
        return true;
    }

    FMG MakeFmg(FMG::FMGVersion Version) {
        FMG Fmg(Version);
        // Out of order on purpose: Write sorts. 10..12 and 100..101 are consecutive runs; 50 stands alone.
        Fmg.SetText(101, "second of a run");
        Fmg.SetText(12, "third of a run");
        Fmg.SetText(10, "first of a run");
        Fmg.SetText(11, std::nullopt);  // an ID with no string
        Fmg.SetText(50, "");            // present but empty, which is not the same as no string
        Fmg.SetText(100, "\xE3\x82\xBD\xE3\x82\xA6\xE3\x83\xAB \xF0\x9F\x97\xA1");  // katakana and a non-BMP symbol
        Fmg.SetText(-5, "negative ID");
        Fmg.SetText(0x7FFFFFFF, "largest ID");
        return Fmg;
    }

    void TestSynthetic() {
        using V = FMG::FMGVersion;
        for (const V Version : {V::DemonsSouls, V::DarkSouls1, V::DarkSouls3}) {
            for (const bool Big : {false, true}) {
                FMG Source    = MakeFmg(Version);
                Source.BigEndian = Big;
                const auto Bytes = Source.Write();

                CHECK(FMG::Is(Bytes));
                FMG Back = FMG::Read(Bytes);
                CHECK(Back.Version == Version && Back.BigEndian == Big);

                // Write sorted the source; both sides now agree on order.
                CHECK(std::is_sorted(Source.Entries.begin(), Source.Entries.end(),
                                     [](const FMG::Entry& A, const FMG::Entry& B) { return A.ID < B.ID; }));
                const bool Same = SameEntries(Source.Entries, Back.Entries);
                if (!Same) std::printf("  (version %d big %d: entries differ)\n", static_cast<int>(Version), Big);
                CHECK(Same);
                CHECK(Back.Write() == Bytes);

                CHECK(Back.GetText(10) == "first of a run");
                CHECK(Back.Find(11) != nullptr && !Back.Find(11)->Text.has_value());
                CHECK(Back.GetText(50) == std::optional<std::string>(""));
                CHECK(!Back.GetText(12345).has_value() && Back.Find(12345) == nullptr);
            }
        }

        // Group layout: 10..12 is one group, so 3 groups cover the 8 sample IDs (-5, 10-12, 50, 100-101, max).
        {
            FMG Fmg = MakeFmg(FMG::FMGVersion::DarkSouls1);
            const auto Bytes = Fmg.Write();
            BinaryReader Reader(Bytes);
            CHECK(Reader.ReadAt<int32_t>(0x0C) == 5);  // group count
            CHECK(Reader.ReadAt<int32_t>(0x10) == 8);  // string count
        }

        // SetText replaces; Entry::ToString formats.
        {
            FMG Fmg;
            Fmg.SetText(1, "a");
            Fmg.SetText(1, "b");
            CHECK(Fmg.Entries.size() == 1 && Fmg.GetText(1) == "b");
            CHECK(Fmg.Entries[0].ToString() == "1: b");
            Fmg.SetText(2, std::nullopt);
            CHECK(Fmg.Entries[1].ToString() == "2: <null>");
        }

        // Detection and bad input.
        {
            const std::vector<uint8_t> Junk(64, 0xAB);
            CHECK(!FMG::Is(Junk));
            CHECK(!FMG::Is(std::vector<uint8_t>{0, 0, 1, 0}));
            CHECK(!FMG::IsRead(Junk).has_value());
            CHECK(Throws([&] { FMG::Read(Junk); }));

            auto Bytes = MakeFmg(FMG::FMGVersion::DarkSouls3).Write();
            std::vector<uint8_t> Truncated(Bytes.begin(), Bytes.begin() + 0x20);
            CHECK(Throws([&] { FMG::Read(Truncated); }));
            std::vector<uint8_t> BadVersion = Bytes;
            BadVersion[2] = 9;
            CHECK(Throws([&] { FMG::Read(BadVersion); }));
            CHECK(!FMG::Is(BadVersion));
            std::vector<uint8_t> BadCount = Bytes;
            BadCount[0x0F] = 0x7F;  // group count 0x7F...... (little-endian)
            CHECK(Throws([&] { FMG::Read(BadCount); }));
        }

        // Through DCX.
        {
            FMG Fmg         = MakeFmg(FMG::FMGVersion::DarkSouls3);
            Fmg.Compression = DCX::Type::DCX_DFLT_10000_44_9;
            const auto Bytes = Fmg.Write();
            CHECK(DCX::Is(Bytes) && FMG::Is(Bytes));
            const FMG Back = FMG::Read(Bytes);
            CHECK(Back.Compression == DCX::Type::DCX_DFLT_10000_44_9 && SameEntries(Fmg.Entries, Back.Entries));
        }
    }

    bool EndsWith(const std::string& Text, const std::string& Suffix) {
        return Text.size() >= Suffix.size() && Text.compare(Text.size() - Suffix.size(), Suffix.size(), Suffix) == 0;
    }

    std::vector<fs::path> Collect(const fs::path& Folder, size_t Max) {
        std::vector<fs::path> Found;
        if (!fs::exists(Folder)) return Found;
        for (const auto& Entry : fs::recursive_directory_iterator(Folder)) {
            if (Entry.is_regular_file() && EndsWith(Entry.path().filename().string(), ".msgbnd.dcx")) {
                Found.push_back(Entry.path());
            }
        }
        std::sort(Found.begin(), Found.end());
        const size_t Step = std::max<size_t>(1, Found.size() / std::max<size_t>(1, Max));
        std::vector<fs::path> Sampled;
        for (size_t I = 0; I < Found.size(); I += Step) Sampled.push_back(Found[I]);
        return Sampled;
    }

    // Reads every .fmg out of the msgbnd files in Folder and checks it survives a rewrite.
    template<typename Binder>
    void TestGame(const char* Label, const fs::path& Folder, size_t Max) {
        int Bnds = 0, Fmgs = 0, Strings = 0, LayoutMatches = 0, WithTrailingData = 0, OtherLayout = 0, Failed = 0;
        std::map<int, int> Versions;
        for (const fs::path& Path : Collect(Folder, Max)) {
            try {
                if (!Binder::Is(Path)) continue;
                ++Bnds;
                const Binder Bnd = Binder::Read(Path);
                for (const BinderFile& File : Bnd.Files) {
                    if (!File.Name || !EndsWith(*File.Name, ".fmg")) continue;
                    ++Fmgs;

                    if (!FMG::Is(File.Bytes)) {
                        std::printf("FAIL %s: %s is not recognized as an FMG\n", Path.string().c_str(), File.Name->c_str());
                        ++Failed;
                        continue;
                    }
                    FMG Fmg = FMG::Read(File.Bytes);
                    ++Versions[static_cast<int>(Fmg.Version)];
                    Strings += static_cast<int>(Fmg.Entries.size());

                    const auto Rewritten = Fmg.Write();
                    const FMG Back       = FMG::Read(Rewritten);
                    if (!SameEntries(Fmg.Entries, Back.Entries) || Back.Version != Fmg.Version ||
                        Back.BigEndian != Fmg.BigEndian) {
                        std::printf("FAIL %s: %s changed after a rewrite\n", Path.string().c_str(), File.Name->c_str());
                        ++Failed;
                    }
                    // Compare against the game's own file. Apart from the recorded file size (bytes 4..7), we usually
                    // write exactly the same bytes; the game's files can also carry extra bytes after the last
                    // string (a couple of zeros, or leftover strings nothing references). A few lay their strings
                    // out differently (padded to 4 bytes); those are still the same entries, checked above.
                    const auto& Original = File.Bytes;
                    const bool Matches   = Original.size() >= Rewritten.size() &&
                                         std::equal(Rewritten.begin(), Rewritten.begin() + 4, Original.begin()) &&
                                         std::equal(Rewritten.begin() + 8, Rewritten.end(), Original.begin() + 8);
                    if (Matches) {
                        ++LayoutMatches;
                        if (Original.size() - Rewritten.size() > 16) ++WithTrailingData;
                    } else {
                        ++OtherLayout;
                    }
                }
            } catch (const std::exception& E) {
                std::printf("FAIL %s: %s\n", Path.string().c_str(), E.what());
                ++Failed;
            }
        }
        std::printf("%s: %d msgbnd files, %d FMGs, %d strings, %d matching the game's layout, %d failures\n",
                    Label, Bnds, Fmgs, Strings, LayoutMatches, Failed);
        if (OtherLayout > 0) std::printf("  (%d use a different string layout)\n", OtherLayout);
        if (WithTrailingData > 0) std::printf("  (%d of them carry more than 16 unreferenced bytes at the end)\n", WithTrailingData);
        for (const auto& [Version, Count] : Versions) std::printf("  FMG version %d: %d files\n", Version, Count);
        Failures += Failed;
    }
}  // namespace

int RunFMGTests(size_t MaxFiles) {
    Failures = 0;
    TestSynthetic();

    const fs::path DarkSouls = "C:/Program Files (x86)/Steam/steamapps/common/DARK SOULS REMASTERED/msg";
    const fs::path EldenRing = "C:/Program Files (x86)/Steam/steamapps/common/ELDEN RING/Game/msg";
    if (!fs::exists(DarkSouls) && !fs::exists(EldenRing)) {
        std::printf("Real FMG checks skipped (no game installs found)\n");
    } else {
        if (fs::exists(DarkSouls)) TestGame<BND3>("Dark Souls Remastered", DarkSouls, MaxFiles);
        if (fs::exists(EldenRing)) TestGame<BND4>("Elden Ring", EldenRing, MaxFiles);
    }

    std::printf(Failures == 0 ? "FMG tests passed\n" : "FMG tests: %d failure(s)\n", Failures);
    return Failures;
}
