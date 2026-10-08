//
// Created by Jake Rieger on 10/7/2026.
//

#include "TestResult.hpp"

#include <libSouls/Binders/BND3.hpp>
#include <libSouls/Binders/BND4.hpp>
#include <libSouls/Binders/Regulation.hpp>
#include <libSouls/Formats/PARAM.hpp>

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
    using F1 = PARAM::FormatFlags1;
    using F2 = PARAM::FormatFlags2;

    template<typename F>
    bool Throws(F&& Fn) {
        try {
            Fn();
        } catch (...) {
            return true;
        }
        return false;
    }

    bool SameRows(const std::vector<PARAM::Row>& A, const std::vector<PARAM::Row>& B) {
        if (A.size() != B.size()) return false;
        for (size_t I = 0; I < A.size(); ++I) {
            if (A[I].ID != B[I].ID || A[I].Name != B[I].Name || A[I].Bytes != B[I].Bytes) return false;
        }
        return true;
    }

    PARAM MakeParam(F1 Format1, F2 Format2, bool Big) {
        PARAM Param;
        Param.BigEndian             = Big;
        Param.Format2D              = Format1;
        Param.Format2E              = Format2;
        Param.ParamdefFormatVersion = Big ? 0xFF : 0;
        Param.Unk06                 = 0;
        Param.ParamdefDataVersion   = 7;
        Param.ParamType             = "TEST_PARAM_ST";
        for (int I = 0; I < 5; ++I) {
            PARAM::Row Row;
            Row.ID = 100 + I * 10;
            if (I % 2 == 0) Row.Name = I == 4 ? std::string("\xE3\x82\xBD\xE3\x82\xA6\xE3\x83\xAB row") : "Row " + std::to_string(I);
            Row.Bytes.resize(12);
            for (size_t B = 0; B < Row.Bytes.size(); ++B) Row.Bytes[B] = static_cast<uint8_t>(I * 16 + B);
            Param.Rows.push_back(std::move(Row));
        }
        return Param;
    }

    void TestSynthetic() {
        const F1 Formats1[] = {F1::None,
                               F1::Flag01,
                               F1::Flag01 | F1::IntDataOffset,
                               F1::LongDataOffset,
                               F1::LongDataOffset | F1::OffsetParamType,
                               F1::Flag01 | F1::LongDataOffset | F1::OffsetParamType};
        for (const F1 Format1 : Formats1) {
            for (const F2 Format2 : {F2::None, F2::UnicodeRowNames}) {
                for (const bool Big : {false, true}) {
                    PARAM Source = MakeParam(Format1, Format2, Big);
                    // Shift-JIS can encode the katakana but the default (ASCII-only) names are fine either way.
                    const auto Bytes = Source.Write();
                    CHECK(PARAM::Is(Bytes));

                    PARAM Back = PARAM::Read(Bytes);
                    const bool Same = SameRows(Source.Rows, Back.Rows) && Back.ParamType == Source.ParamType &&
                                      Back.ParamdefDataVersion == 7 && Back.Format2D == Format1 &&
                                      Back.Format2E == Format2 && Back.BigEndian == Big && Back.DetectedSize == 12;
                    if (!Same) {
                        std::printf("  (format 0x%02X/0x%02X big %d: contents differ)\n", static_cast<unsigned>(Format1),
                                    static_cast<unsigned>(Format2), Big);
                    }
                    CHECK(Same);
                    CHECK(Back.Write() == Bytes);
                    CHECK(Back.ToString() == "TEST_PARAM_ST v7 [5]");
                }
            }
        }

        // Find / lookups.
        {
            PARAM Param = MakeParam(F1::LongDataOffset | F1::OffsetParamType, F2::UnicodeRowNames, false);
            CHECK(Param.Find(120) != nullptr && Param.Find(120)->Name == std::optional<std::string>("Row 2"));
            CHECK(Param.Find(121) == nullptr);
            CHECK(Param.Rows[1].ToString() == "110 ");
        }

        // A single row has no spacing to measure, so its size comes from where the strings start.
        {
            PARAM Param = MakeParam(F1::LongDataOffset | F1::OffsetParamType, F2::UnicodeRowNames, false);
            Param.Rows.resize(1);
            const PARAM Back = PARAM::Read(Param.Write());
            CHECK(Back.Rows.size() == 1 && Back.DetectedSize == 12 && Back.Rows[0].Bytes == Param.Rows[0].Bytes);
        }

        // An empty param.
        {
            PARAM Param = MakeParam(F1::LongDataOffset | F1::OffsetParamType, F2::None, false);
            Param.Rows.clear();
            const PARAM Back = PARAM::Read(Param.Write());
            CHECK(Back.Rows.empty() && Back.DetectedSize == -1 && Back.ParamType == "TEST_PARAM_ST");
        }

        // Row data can be edited and written back.
        {
            PARAM Param = MakeParam(F1::None, F2::None, false);
            Param.Find(110)->Bytes[0] = 0xEE;
            Param.Rows.push_back(PARAM::Row(999, "added", std::vector<uint8_t>(12, 0x42)));
            const PARAM Back = PARAM::Read(Param.Write());
            CHECK(Back.Find(110)->Bytes[0] == 0xEE && Back.Find(999)->Bytes == std::vector<uint8_t>(12, 0x42));
        }

        // Rows of different sizes can't be written.
        {
            PARAM Param = MakeParam(F1::None, F2::None, false);
            Param.Rows[2].Bytes.push_back(0);
            CHECK(Throws([&] { Param.Write(); }));
        }

        // Detection and bad input.
        {
            const std::vector<uint8_t> Junk(0x100, 0xAB);
            CHECK(!PARAM::Is(Junk));
            CHECK(!PARAM::Is(std::vector<uint8_t>(0x10)));
            CHECK(!PARAM::IsRead(Junk).has_value());
            CHECK(Throws([&] { PARAM::Read(Junk); }));

            const auto Bytes = MakeParam(F1::None, F2::None, false).Write();
            std::vector<uint8_t> Truncated(Bytes.begin(), Bytes.begin() + 0x40);
            CHECK(Throws([&] { PARAM::Read(Truncated); }));
            std::vector<uint8_t> BadCount = Bytes;
            BadCount[0x0A] = 0xFF;
            BadCount[0x0B] = 0xFF;
            CHECK(Throws([&] { PARAM::Read(BadCount); }));
            CHECK(!PARAM::Is(BadCount));
        }

        // Through DCX.
        {
            PARAM Param         = MakeParam(F1::LongDataOffset | F1::OffsetParamType, F2::UnicodeRowNames, false);
            Param.Compression   = DCX::Type::DCX_DFLT_10000_44_9;
            const auto Bytes    = Param.Write();
            const PARAM Back    = PARAM::Read(Bytes);
            CHECK(Back.Compression == DCX::Type::DCX_DFLT_10000_44_9 && SameRows(Param.Rows, Back.Rows));
        }
    }

    bool EndsWith(const std::string& Text, const std::string& Suffix) {
        return Text.size() >= Suffix.size() && Text.compare(Text.size() - Suffix.size(), Suffix.size(), Suffix) == 0;
    }

    std::string SafeLabel(const char* Label) {
        std::string Result = Label;
        std::replace(Result.begin(), Result.end(), ' ', '_');
        return Result;
    }

    struct Stats {
        int Params = 0, Rows = 0, ByteIdentical = 0, Failed = 0, Named = 0;
        std::map<std::string, int> Formats;  // "2D/2E/big" -> count
        std::vector<std::string> Different;  // names of params that don't rewrite identically
    };

    // Reads every .param in the binder's files and checks it survives a rewrite.
    template<typename B>
    void TestBinder(const char* Label, B& Bnd, Stats& Totals) {
        for (const BinderFile& File : Bnd.Files) {
            if (!File.Name || !EndsWith(*File.Name, ".param")) continue;
            try {
                if (!PARAM::Is(File.Bytes)) {
                    std::printf("FAIL %s: %s is not recognized as a PARAM (size 0x%zX):", Label, File.Name->c_str(),
                                File.Bytes.size());
                    for (size_t I = 0; I < 0x50 && I < File.Bytes.size(); ++I) std::printf(" %02X", File.Bytes[I]);
                    std::printf("\n");
                    ++Totals.Failed;
                    continue;
                }
                PARAM Param = PARAM::Read(File.Bytes);
                ++Totals.Params;
                Totals.Rows += static_cast<int>(Param.Rows.size());

                char Key[64];
                std::snprintf(Key, sizeof Key, "2D=0x%02X 2E=0x%02X %s", static_cast<unsigned>(Param.Format2D),
                              static_cast<unsigned>(Param.Format2E), Param.BigEndian ? "big" : "little");
                ++Totals.Formats[Key];

                bool Uniform = true;
                for (const auto& Row : Param.Rows) {
                    if (Row.Name) ++Totals.Named;
                    if (static_cast<int64_t>(Row.Bytes.size()) != Param.DetectedSize && !Row.Bytes.empty()) Uniform = false;
                }
                if (!Uniform || Param.ParamType.empty()) {
                    std::printf("FAIL %s: %s has irregular rows or no type\n", Label, File.Name->c_str());
                    ++Totals.Failed;
                }

                const auto Rewritten = Param.Write();
                if (const char* DumpDir = std::getenv("PARAM_DUMP_DIR")) {
                    const std::string Short = fs::path(*File.Name).filename().string();
                    BinaryWriter A(fs::path(DumpDir) / (SafeLabel(Label) + "_" + Short + ".orig"));
                    A.WriteBytes(File.Bytes);
                    A.Finish();
                    BinaryWriter B(fs::path(DumpDir) / (SafeLabel(Label) + "_" + Short + ".mine"));
                    B.WriteBytes(Rewritten);
                    B.Finish();
                }
                const PARAM Back     = PARAM::Read(Rewritten);
                if (!SameRows(Param.Rows, Back.Rows) || Back.ParamType != Param.ParamType) {
                    std::printf("FAIL %s: %s changed after a rewrite\n", Label, File.Name->c_str());
                    ++Totals.Failed;
                }
                if (Rewritten == File.Bytes) {
                    ++Totals.ByteIdentical;
                } else {
                    Totals.Different.push_back(*File.Name);
                    if (std::getenv("PARAM_DEBUG") && Totals.Different.size() <= 4) {
                        const auto& Original = File.Bytes;
                        size_t At = 0;
                        while (At < Rewritten.size() && At < Original.size() && Rewritten[At] == Original[At]) ++At;
                        std::printf("  diff %s: first at 0x%zX, sizes orig 0x%zX rewritten 0x%zX, rows %zu, rowsize %lld\n",
                                    File.Name->c_str(), At, Original.size(), Rewritten.size(), Param.Rows.size(),
                                    static_cast<long long>(Param.DetectedSize));
                        for (const auto* B : {&Original, &Rewritten}) {
                            std::printf("   ");
                            for (size_t I = At >= 16 ? At - 16 : 0; I < At + 48 && I < B->size(); ++I) std::printf(" %02X", (*B)[I]);
                            std::printf("\n");
                        }
                    }
                }
            } catch (const std::exception& E) {
                std::printf("FAIL %s: %s: %s\n", Label, File.Name->c_str(), E.what());
                ++Totals.Failed;
            }
        }
    }

    void Report(const char* Label, const Stats& Totals) {
        std::printf("%s: %d params, %d rows (%d named), %d byte-identical rewrites, %d failures\n", Label, Totals.Params,
                    Totals.Rows, Totals.Named, Totals.ByteIdentical, Totals.Failed);
        for (const auto& [Format, Count] : Totals.Formats) std::printf("  %s: %d files\n", Format.c_str(), Count);
        for (size_t I = 0; I < Totals.Different.size() && I < 5; ++I) {
            std::printf("  differs from the original: %s\n", Totals.Different[I].c_str());
        }
        if (Totals.Different.size() > 5) std::printf("  ... and %zu more\n", Totals.Different.size() - 5);
        Failures += Totals.Failed;
    }
}  // namespace

int RunPARAMTests() {
    Failures = 0;
    TestSynthetic();

    const fs::path Steam = "C:/Program Files (x86)/Steam/steamapps/common";

    // Elden Ring: regulation.bin holds the game's params.
    if (fs::exists(Steam / "ELDEN RING/Game/regulation.bin")) {
        try {
            BND4 Bnd = Regulation::DecryptER(Steam / "ELDEN RING/Game/regulation.bin");
            Stats Totals;
            TestBinder("Elden Ring", Bnd, Totals);
            Report("Elden Ring regulation.bin", Totals);
            CHECK(Totals.Params > 100);
            // Only the header's strings offset (unreliable, so not reproduced) and a few odd files differ.
            CHECK(Totals.ByteIdentical * 10 >= Totals.Params * 7);
        } catch (const std::exception& E) {
            std::printf("FAIL Elden Ring regulation: %s\n", E.what());
            ++Failures;
        }
    }

    // Dark Souls III: Data0.bdt is its (encrypted) regulation file.
    if (fs::exists(Steam / "DARK SOULS III/Game/Data0.bdt")) {
        try {
            BND4 Bnd = Regulation::DecryptDS3(Steam / "DARK SOULS III/Game/Data0.bdt");
            Stats Totals;
            TestBinder("Dark Souls III", Bnd, Totals);
            Report("Dark Souls III Data0.bdt", Totals);
            CHECK(Totals.Params > 50);
            CHECK(Totals.ByteIdentical * 10 >= Totals.Params * 9);
        } catch (const std::exception& E) {
            std::printf("FAIL Dark Souls III regulation: %s\n", E.what());
            ++Failures;
        }
    }

    // Dark Souls Remastered: BND3 param binders.
    const fs::path DarkSouls = Steam / "DARK SOULS REMASTERED/param";
    if (fs::exists(DarkSouls)) {
        Stats Totals;
        int Binders = 0;
        for (const auto& Entry : fs::recursive_directory_iterator(DarkSouls)) {
            if (!Entry.is_regular_file() || !EndsWith(Entry.path().filename().string(), ".parambnd.dcx")) continue;
            try {
                BND3 Bnd = BND3::Read(Entry.path());
                ++Binders;
                TestBinder("Dark Souls Remastered", Bnd, Totals);
            } catch (const std::exception& E) {
                std::printf("FAIL %s: %s\n", Entry.path().string().c_str(), E.what());
                ++Failures;
            }
        }
        std::printf("(%d param binders)\n", Binders);
        Report("Dark Souls Remastered", Totals);
        CHECK(Totals.Params > 50);
        // Dark Souls lays its string pool out differently in many files; contents are still checked above.
        CHECK(Totals.ByteIdentical * 10 >= Totals.Params * 4);
    }

    std::printf(Failures == 0 ? "PARAM tests passed\n" : "PARAM tests: %d failure(s)\n", Failures);
    return Failures;
}
