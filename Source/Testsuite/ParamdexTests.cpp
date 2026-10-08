//
// Created by Jake Rieger on 10/7/2026.
//

#include "TestResult.hpp"

#include <libSouls/Binders/BND3.hpp>
#include <libSouls/Binders/BND4.hpp>
#include <libSouls/Binders/Regulation.hpp>
#include <libSouls/Formats/ParamDefRepository.hpp>
#include <libSouls/Formats/ParamLayout.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <string>

// Checks the library against the community's Paramdex repository, which isn't part of libSouls (and isn't shipped
// with it). Set PARAMDEX_PATH to its folder, or clone it to ~/Code/GitHub/Paramdex.

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

    bool EndsWith(const std::string& Text, const std::string& Suffix) {
        return Text.size() >= Suffix.size() && Text.compare(Text.size() - Suffix.size(), Suffix.size(), Suffix) == 0;
    }

    fs::path FindParamdex() {
        if (const char* Env = std::getenv("PARAMDEX_PATH")) return Env;
        if (const char* Home = std::getenv("USERPROFILE")) return fs::path(Home) / "Code/GitHub/Paramdex";
        return {};
    }

    bool SameFieldLists(const PARAMDEF& A, const PARAMDEF& B) {
        if (A.Fields.size() != B.Fields.size()) return false;
        for (size_t I = 0; I < A.Fields.size(); ++I) {
            const auto& X = A.Fields[I];
            const auto& Y = B.Fields[I];
            if (X.InternalName != Y.InternalName || X.DisplayType != Y.DisplayType || X.BitSize != Y.BitSize ||
                X.ArrayLength != Y.ArrayLength) {
                return false;
            }
        }
        return true;
    }

    // What a binary PARAMDEF can carry: field types, sizes and bit layout always; names only from format version 102
    // on, and then at most 31 bytes in the fixed-width versions, so longer names come back cut short.
    bool BinaryEquivalent(const PARAMDEF& Original, const PARAMDEF& Back) {
        if (Original.Fields.size() != Back.Fields.size() || Original.GetRowSize() != Back.GetRowSize()) return false;
        for (size_t I = 0; I < Original.Fields.size(); ++I) {
            const auto& X = Original.Fields[I];
            const auto& Y = Back.Fields[I];
            if (X.DisplayType != Y.DisplayType || X.ArrayLength != Y.ArrayLength) return false;
            if (Original.FormatVersion >= 102) {
                if (X.BitSize != Y.BitSize) return false;
                if (X.InternalName.compare(0, Y.InternalName.size(), Y.InternalName) != 0) return false;  // Y is X or a prefix of it
            }
        }
        return true;
    }

    // Loads every game's Defs folder: nothing may fail to load, and each def must survive XML and binary round trips.
    void TestAllGames(const fs::path& Paramdex) {
        std::printf("%-6s %6s %8s %7s %s\n", "game", "defs", "skipped", "errors", "notes");
        for (const auto& Game : fs::directory_iterator(Paramdex)) {
            const fs::path Defs = Game.path() / "Defs";
            if (!fs::is_directory(Defs)) continue;

            ParamDefRepository Repo;
            const auto Result = Repo.AddFolder(Defs);
            std::printf("%-6s %6zu %8zu %7zu", Game.path().filename().string().c_str(), Result.Loaded, Result.Skipped, Result.Errors.size());
            for (size_t I = 0; I < Result.Errors.size() && I < 3; ++I) {
                std::printf("\n    ERROR %s: %s", Result.Errors[I].Path.filename().string().c_str(), Result.Errors[I].Message.c_str());
            }
            if (Result.Errors.size() > 3) std::printf("\n    ... and %zu more errors", Result.Errors.size() - 3);

            // Each def: XML round trip keeps everything; binary write/read keeps the field list and row size.
            size_t Bad = 0, Fields = 0;
            std::map<int, size_t> Versions;
            for (const PARAMDEF* Def : Repo.All()) {
                Fields += Def->Fields.size();
                ++Versions[Def->FormatVersion];
                try {
                    PARAMDEF Copy = *Def;
                    const PARAMDEF ViaXml = PARAMDEF::FromXmlString(Copy.ToXmlString());
                    const bool XmlOk = SameFieldLists(*Def, ViaXml) && ViaXml.GetRowSize() == Def->GetRowSize();
                    // A few games' defs (Nightreign's) carry a format version with no binary form; nothing to check there.
                    const bool HasBinaryForm = Def->FormatVersion != 0;
                    PARAMDEF Writable = *Def;
                    const PARAMDEF ViaBinary = HasBinaryForm ? PARAMDEF::Read(Writable.Write()) : *Def;
                    const bool BinaryOk = BinaryEquivalent(*Def, ViaBinary);
                    if (!BinaryOk && std::getenv("PARAMDEX_DEBUG")) {
                        for (size_t F = 0; F < Def->Fields.size() && F < ViaBinary.Fields.size(); ++F) {
                            const auto& X = Def->Fields[F];
                            const auto& Y = ViaBinary.Fields[F];
                            if (X.InternalName != Y.InternalName || X.DisplayType != Y.DisplayType || X.BitSize != Y.BitSize || X.ArrayLength != Y.ArrayLength) {
                                std::printf("\n      %s field %zu: '%s' type %d bits %d len %d  ->  '%s' type %d bits %d len %d", Def->ParamType.c_str(), F,
                                            X.InternalName.c_str(), static_cast<int>(X.DisplayType), X.BitSize, X.ArrayLength,
                                            Y.InternalName.c_str(), static_cast<int>(Y.DisplayType), Y.BitSize, Y.ArrayLength);
                                break;
                            }
                        }
                        if (Def->Fields.size() != ViaBinary.Fields.size()) std::printf("\n      %s: %zu fields -> %zu", Def->ParamType.c_str(), Def->Fields.size(), ViaBinary.Fields.size());
                    }
                    if (!XmlOk || !BinaryOk) {
                        ++Bad;
                        if (Bad <= 3) std::printf("\n    ROUND TRIP %s (%s%s)", Def->ParamType.c_str(), XmlOk ? "" : "xml ", BinaryOk ? "" : "binary");
                    }
                } catch (const std::exception& E) {
                    ++Bad;
                    if (Bad <= 3) std::printf("\n    EXCEPTION %s: %s", Def->ParamType.c_str(), E.what());
                }
            }
            std::printf("\n       %zu fields; format versions:", Fields);
            for (const auto& [Version, Count] : Versions) std::printf(" %d x%zu", Version, Count);
            std::printf("%s\n", Bad ? "  <-- round-trip problems" : "");

            CHECK(Result.Errors.empty());
            CHECK(Bad == 0);
            CHECK(Result.Loaded > 0);
        }
    }

    struct Tally {
        int Params = 0, Matched = 0, NoDef = 0, WrongVersion = 0, WrongSize = 0, Rows = 0, NonFinite = 0, Unchanged = 0, Changed = 0, Cells = 0;
        std::vector<std::string> Unmatched;
    };

    // Applies the repository to every param in the binder and reads every cell of every row.
    template<typename Binder>
    void TestBinder(const Binder& Bnd, const ParamDefRepository& Repo, Tally& Totals) {
        for (const BinderFile& File : Bnd.Files) {
            if (!File.Name || !EndsWith(*File.Name, ".param")) continue;
            PARAM Param = PARAM::Read(File.Bytes);
            ++Totals.Params;

            const PARAMDEF* Def = Repo.Find(Param);
            if (!Def) {
                const auto Candidates = Repo.FindCandidates(Param.ParamType);
                std::string Why;
                if (Candidates.empty()) {
                    ++Totals.NoDef;
                    Why = "no def for type";
                } else {
                    bool VersionExists = std::any_of(Candidates.begin(), Candidates.end(),
                                                     [&](const PARAMDEF* D) { return D->DataVersion == Param.ParamdefDataVersion; });
                    if (VersionExists) {
                        ++Totals.WrongSize;
                        Why = "row size " + std::to_string(Param.DetectedSize) + " vs def " + std::to_string(Candidates[0]->GetRowSize());
                    } else {
                        ++Totals.WrongVersion;
                        Why = "param v" + std::to_string(Param.ParamdefDataVersion) + " vs def v" + std::to_string(Candidates[0]->DataVersion);
                    }
                }
                Totals.Unmatched.push_back(Param.ParamType + " (" + Why + ")");
                continue;
            }
            ++Totals.Matched;

            const ParamLayout Layout(*Def, Param.BigEndian);
            for (PARAM::Row& Row : Param.Rows) {
                ++Totals.Rows;
                for (size_t I = 0; I < Layout.FieldCount(); ++I) {
                    const CellValue Value = Layout.Get(Row, I);
                    ++Totals.Cells;
                    if (const float* F = std::get_if<float>(&Value)) {
                        if (!std::isfinite(*F)) ++Totals.NonFinite;
                    } else if (const double* D = std::get_if<double>(&Value)) {
                        if (!std::isfinite(*D)) ++Totals.NonFinite;
                    }
                    const std::vector<uint8_t> Before = Row.Bytes;
                    Layout.Set(Row, I, Value);
                    if (Row.Bytes == Before) {
                        ++Totals.Unchanged;
                    } else {
                        ++Totals.Changed;
                        Row.Bytes = Before;
                    }
                }
            }
        }
    }

    void Report(const char* Label, const Tally& Totals) {
        std::printf("%s: %d params; %d matched a def (%d rows, %d cells; %d unchanged / %d changed on write-back; %d non-finite floats)\n",
                    Label, Totals.Params, Totals.Matched, Totals.Rows, Totals.Cells, Totals.Unchanged, Totals.Changed, Totals.NonFinite);
        std::printf("  unmatched: %d with no def for the type, %d with only another data version, %d with another row size\n",
                    Totals.NoDef, Totals.WrongVersion, Totals.WrongSize);
        for (size_t I = 0; I < Totals.Unmatched.size() && I < 8; ++I) std::printf("    %s\n", Totals.Unmatched[I].c_str());
        if (Totals.Unmatched.size() > 8) std::printf("    ... and %zu more\n", Totals.Unmatched.size() - 8);
    }

    // Loads Paramdex's defs for the game and applies them to the game's own params.
    template<typename Binder>
    void TestGame(const char* Label, const fs::path& DefsFolder, const std::vector<Binder>& Binders) {
        ParamDefRepository Repo;
        const auto Loaded = Repo.AddFolder(DefsFolder);
        CHECK(Loaded.Errors.empty());
        Tally Totals;
        for (const Binder& Bnd : Binders) TestBinder(Bnd, Repo, Totals);
        Report(Label, Totals);
        CHECK(Totals.Params > 0 && Totals.NonFinite == 0 && Totals.Changed == 0);
        // Nearly every param should find its def; report the exceptions above rather than hide them behind a number.
        CHECK(Totals.Matched * 10 >= Totals.Params * 9);
    }
}  // namespace

int RunParamdexTests() {
    Failures = 0;

    const fs::path Paramdex = FindParamdex();
    if (Paramdex.empty() || !fs::exists(Paramdex / "ER" / "Defs")) {
        std::printf("Paramdex checks skipped (set PARAMDEX_PATH, or clone Paramdex to ~/Code/GitHub/Paramdex)\n");
        return TestSkipped;
    }

    try {
        TestAllGames(Paramdex);

        const fs::path Steam = "C:/Program Files (x86)/Steam/steamapps/common";

        // Dark Souls Remastered's own shipped defs agree with Paramdex's copies (apart from translated display text).
        if (fs::exists(Steam / "DARK SOULS REMASTERED/paramdef/paramdef.paramdefbnd.dcx")) {
            ParamDefRepository Repo;
            Repo.AddFolder(Paramdex / "DS1R/Defs");
            const BND3 DefBnd = BND3::Read(Steam / "DARK SOULS REMASTERED/paramdef/paramdef.paramdefbnd.dcx");
            int Compared = 0, Same = 0, Missing = 0;
            for (const BinderFile& File : DefBnd.Files) {
                if (!File.Name || !EndsWith(*File.Name, ".paramdef")) continue;
                const PARAMDEF Shipped = PARAMDEF::Read(File.Bytes);
                const auto Candidates  = Repo.FindCandidates(Shipped.ParamType);
                const auto It = std::find_if(Candidates.begin(), Candidates.end(), [&](const PARAMDEF* D) { return D->DataVersion == Shipped.DataVersion; });
                if (It == Candidates.end()) {
                    ++Missing;
                    std::printf("  Paramdex has no DS1R def for %s v%d\n", Shipped.ParamType.c_str(), Shipped.DataVersion);
                    continue;
                }
                ++Compared;
                if (SameFieldLists(Shipped, **It)) {
                    ++Same;
                } else {
                    std::printf("  DS1R %s: shipped and Paramdex fields differ\n", Shipped.ParamType.c_str());
                }
            }
            std::printf("DS1R shipped defs vs Paramdex: %d compared, %d identical field lists, %d with no Paramdex def\n", Compared, Same, Missing);
            CHECK(Compared > 40 && Same == Compared);
        }

        if (fs::exists(Steam / "ELDEN RING/Game/regulation.bin")) {
            TestGame<BND4>("Elden Ring (regulation.bin)", Paramdex / "ER/Defs", {Regulation::DecryptER(Steam / "ELDEN RING/Game/regulation.bin")});
        }
        if (fs::exists(Steam / "DARK SOULS III/Game/Data0.bdt")) {
            TestGame<BND4>("Dark Souls III (Data0.bdt)", Paramdex / "DS3/Defs", {Regulation::DecryptDS3(Steam / "DARK SOULS III/Game/Data0.bdt")});
        }
        if (fs::exists(Steam / "DARK SOULS REMASTERED/param")) {
            std::vector<BND3> Binders;
            for (const auto& Entry : fs::recursive_directory_iterator(Steam / "DARK SOULS REMASTERED/param")) {
                if (Entry.is_regular_file() && EndsWith(Entry.path().filename().string(), ".parambnd.dcx")) {
                    Binders.push_back(BND3::Read(Entry.path()));
                }
            }
            TestGame<BND3>("Dark Souls Remastered (param binders)", Paramdex / "DS1R/Defs", Binders);
        }
    } catch (const std::exception& E) {
        std::printf("FAIL Paramdex: %s\n", E.what());
        ++Failures;
    }

    std::printf(Failures == 0 ? "Paramdex tests passed\n" : "Paramdex tests: %d failure(s)\n", Failures);
    return Failures;
}
