//
// Created by Jake Rieger on 10/7/2026.
//

#include "TestResult.hpp"

#include <libSouls/Binders/BND3.hpp>
#include <libSouls/Formats/ParamDefRepository.hpp>
#include <libSouls/Formats/ParamLayout.hpp>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
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
    using DefType = PARAMDEF::DefType;

    template<typename F>
    bool Throws(F&& Fn) {
        try {
            Fn();
        } catch (...) {
            return true;
        }
        return false;
    }

    bool EndsWith(const std::string& Text, const std::string& Suffix) {
        return Text.size() >= Suffix.size() && Text.compare(Text.size() - Suffix.size(), Suffix.size(), Suffix) == 0;
    }

    // A def of the given type and data version with the given number of s32 fields (so, 4 * Fields bytes per row).
    PARAMDEF MakeDef(const std::string& Type, int16_t Version, int Fields = 3) {
        PARAMDEF Def;
        Def.FormatVersion = 203;
        Def.DataVersion   = Version;
        Def.ParamType     = Type;
        for (int I = 0; I < Fields; ++I) {
            Def.Fields.emplace_back(&Def, DefType::s32, "field" + std::to_string(I));
        }
        return Def;
    }

    PARAM MakeParam(const PARAMDEF& Def) {
        PARAM Param;
        Param.ParamType           = Def.ParamType;
        Param.ParamdefDataVersion = Def.DataVersion;
        Param.Rows.push_back(ParamLayout(Def, false).MakeRow(1));
        return PARAM::Read(Param.Write());  // so DetectedSize is filled in
    }

    void WriteText(const fs::path& Path, const std::string& Text) {
        fs::create_directories(Path.parent_path());
        std::ofstream(Path, std::ios::binary) << Text;
    }

    void TestFolders() {
        const fs::path Dir = fs::temp_directory_path() / "libsouls_defrepo_test";
        fs::remove_all(Dir);

        MakeDef("WEAPON_ST", 1).ToXml(Dir / "Defs" / "WEAPON_ST.xml");
        MakeDef("WEAPON_ST", 2, 4).ToXml(Dir / "Defs" / "Old" / "WEAPON_ST_v2.xml");  // another data version
        MakeDef("GOODS_ST", 1).ToXml(Dir / "Defs" / "GOODS_ST.xml");
        MakeDef("DEEP_ST", 1).ToXml(Dir / "Defs" / "a" / "b" / "c" / "DEEP_ST.XML");  // upper-case extension
        WriteText(Dir / "Defs" / "Meta" / "WEAPON_ST.xml", "<PARAMMETA><Field>x</Field></PARAMMETA>");   // not a def
        WriteText(Dir / "Defs" / "broken.xml", "<PARAMDEF><ParamType>BROKEN");                           // not XML
        WriteText(Dir / "Defs" / "incomplete.xml", "<PARAMDEF><ParamType>INCOMPLETE</ParamType></PARAMDEF>");
        WriteText(Dir / "Defs" / "readme.txt", "not an xml file");

        ParamDefRepository Repo;
        CHECK(Repo.Empty() && Repo.Size() == 0 && Repo.Find(PARAM{}) == nullptr && Repo.FindByType("WEAPON_ST") == nullptr);

        const auto Result = Repo.AddFolder(Dir / "Defs");
        CHECK(Result.Loaded == 4 && Result.Replaced == 0);
        CHECK(Result.Skipped == 1);       // the metadata file
        CHECK(Result.Errors.size() == 2);  // the broken one and the incomplete one
        for (const auto& Error : Result.Errors) {
            CHECK(!Error.Message.empty() && (EndsWith(Error.Path.string(), "broken.xml") || EndsWith(Error.Path.string(), "incomplete.xml")));
        }
        CHECK(Repo.Size() == 4 && !Repo.Empty() && Repo.All().size() == 4);

        // Lookup by param: type, data version and row size all have to agree.
        const PARAMDEF V1 = MakeDef("WEAPON_ST", 1);
        const PARAMDEF V2 = MakeDef("WEAPON_ST", 2, 4);
        const PARAM ParamV1 = MakeParam(V1);
        const PARAM ParamV2 = MakeParam(V2);
        const PARAMDEF* Found1 = Repo.Find(ParamV1);
        const PARAMDEF* Found2 = Repo.Find(ParamV2);
        CHECK(Found1 && Found1->DataVersion == 1 && Found1->Fields.size() == 3);
        CHECK(Found2 && Found2->DataVersion == 2 && Found2->Fields.size() == 4);
        CHECK(Repo.Find(MakeParam(MakeDef("GOODS_ST", 1))) != nullptr);
        CHECK(Repo.Find(MakeParam(MakeDef("DEEP_ST", 1))) != nullptr);
        CHECK(Repo.Find(MakeParam(MakeDef("UNKNOWN_ST", 1))) == nullptr);
        CHECK(Repo.Find(MakeParam(MakeDef("WEAPON_ST", 3))) == nullptr);  // no def for that data version
        PARAM WrongSize = MakeParam(V1);
        WrongSize.DetectedSize = 12345;
        CHECK(Repo.Find(WrongSize) == nullptr);

        // By type: newest first.
        CHECK(Repo.FindByType("WEAPON_ST") == Found2);
        const auto Candidates = Repo.FindCandidates("WEAPON_ST");
        CHECK(Candidates.size() == 2 && Candidates[0]->DataVersion == 2 && Candidates[1]->DataVersion == 1);
        CHECK(Repo.FindCandidates("NOPE").empty());

        // A usable layout comes out of the whole chain.
        {
            ParamLayout Layout(*Found2, false);
            PARAM::Row Row = Layout.MakeRow(5);
            Layout.Set(Row, "field3", int32_t{99});
            CHECK(Layout.Get<int32_t>(Row, "field3") == 99 && Layout.RowSize() == 16);
        }

        // Non-recursive loading only sees the top folder.
        {
            ParamDefRepository Shallow;
            const auto Top = Shallow.AddFolder(Dir / "Defs", false);
            // WEAPON_ST (v1) and GOODS_ST are at the top; the v2 def and DEEP_ST are in subfolders.
            CHECK(Top.Loaded == 2 && Shallow.Size() == 2 && Shallow.FindByType("DEEP_ST") == nullptr);
            CHECK(Shallow.FindByType("WEAPON_ST")->DataVersion == 1);
        }

        // Folders added later override earlier ones; a def with the same type and version replaces.
        {
            PARAMDEF Override = MakeDef("GOODS_ST", 1, 6);
            Override.ToXml(Dir / "Mine" / "GOODS_ST.xml");
            const auto Second = Repo.AddFolder(Dir / "Mine");
            CHECK(Second.Loaded == 1 && Second.Replaced == 1 && Repo.Size() == 4);
            const PARAMDEF* Goods = Repo.FindByType("GOODS_ST");
            CHECK(Goods && Goods->Fields.size() == 6);
            CHECK(Repo.Find(MakeParam(MakeDef("GOODS_ST", 1, 3))) == nullptr);  // the old layout no longer matches
        }

        // Defs added directly.
        {
            Repo.Add(MakeDef("EXTRA_ST", 1));
            CHECK(Repo.Size() == 5 && Repo.FindByType("EXTRA_ST") != nullptr);
            Repo.Add(MakeDef("EXTRA_ST", 1, 7));  // replaces
            CHECK(Repo.Size() == 5 && Repo.FindByType("EXTRA_ST")->Fields.size() == 7);
        }

        // AddFile.
        {
            ParamDefRepository One;
            const auto Loaded = One.AddFile(Dir / "Defs" / "GOODS_ST.xml");
            CHECK(Loaded.Loaded == 1 && One.Size() == 1);
            CHECK(One.AddFile(Dir / "Defs" / "Meta" / "WEAPON_ST.xml").Skipped == 1);
            CHECK(One.AddFile(Dir / "Defs" / "broken.xml").Errors.size() == 1);
            CHECK(Throws([&] { One.AddFile(Dir / "nope.xml"); }));
            CHECK(Throws([&] { One.AddFolder(Dir / "no_such_folder"); }));
        }

        // Reload picks up changes to the folders but keeps what was added by hand.
        {
            fs::remove(Dir / "Defs" / "GOODS_ST.xml");
            MakeDef("NEW_ST", 1).ToXml(Dir / "Defs" / "NEW_ST.xml");
            MakeDef("WEAPON_ST", 1, 9).ToXml(Dir / "Defs" / "WEAPON_ST.xml");  // changed
            const auto Reloaded = Repo.Reload();
            CHECK(Repo.FindByType("NEW_ST") != nullptr);
            CHECK(Repo.FindCandidates("WEAPON_ST").back()->Fields.size() == 9);
            CHECK(Repo.FindByType("EXTRA_ST") != nullptr);   // added by hand: kept
            CHECK(Repo.FindByType("GOODS_ST") != nullptr);   // from "Mine", which is still there
            CHECK(Reloaded.Errors.size() == 2);              // the same two broken files
            fs::remove_all(Dir / "Mine");
            Repo.Reload();
            CHECK(Repo.FindByType("GOODS_ST") == nullptr);   // its folder is gone, and so is its def
            fs::remove_all(Dir / "Defs");
            const auto Missing = Repo.Reload();
            CHECK(!Missing.Errors.empty() && Repo.FindByType("EXTRA_ST") != nullptr && Repo.FindByType("NEW_ST") == nullptr);
        }

        // Clear forgets the folders too.
        {
            Repo.Clear();
            CHECK(Repo.Empty() && Repo.Reload().Loaded == 0);
        }

        // Moving a repository keeps its defs.
        {
            ParamDefRepository A;
            A.Add(MakeDef("MOVED_ST", 1));
            ParamDefRepository B = std::move(A);
            CHECK(B.FindByType("MOVED_ST") != nullptr);
        }

        fs::remove_all(Dir);
    }
}  // namespace

int RunParamDefRepositoryTests() {
    Failures = 0;
    TestFolders();

    // Dark Souls Remastered end to end: its real defs written out as an XML folder, loaded back, and used to read
    // its real params.
    const fs::path Root = "C:/Program Files (x86)/Steam/steamapps/common/DARK SOULS REMASTERED";
    if (!fs::exists(Root / "paramdef/paramdef.paramdefbnd.dcx") || !fs::exists(Root / "param/GameParam/GameParam.parambnd.dcx")) {
        std::printf("Real repository checks skipped (Dark Souls Remastered not installed)\n");
    } else {
        const fs::path Dir = fs::temp_directory_path() / "libsouls_defrepo_real";
        try {
            fs::remove_all(Dir);
            const BND3 DefBnd = BND3::Read(Root / "paramdef/paramdef.paramdefbnd.dcx");
            size_t Written = 0;
            for (const BinderFile& File : DefBnd.Files) {
                if (!File.Name || !EndsWith(*File.Name, ".paramdef")) continue;
                PARAMDEF::Read(File.Bytes).ToXml(Dir / (fs::path(*File.Name).stem().string() + ".xml"));
                ++Written;
            }

            ParamDefRepository Repo;
            const auto Loaded = Repo.AddFolder(Dir);
            CHECK(Loaded.Loaded == Written && Loaded.Errors.empty() && Repo.Size() == Written);

            const BND3 ParamBnd = BND3::Read(Root / "param/GameParam/GameParam.parambnd.dcx");
            int Params = 0, Found = 0, Near = 0;
            std::optional<double> DaggerWeight;
            for (const BinderFile& File : ParamBnd.Files) {
                if (!File.Name || !EndsWith(*File.Name, ".param")) continue;
                const PARAM Param = PARAM::Read(File.Bytes);
                ++Params;
                if (const PARAMDEF* Def = Repo.Find(Param)) {
                    ++Found;
                    if (Param.ParamType == "EQUIP_PARAM_WEAPON_ST") {
                        const ParamLayout Layout(*Def, Param.BigEndian);
                        if (const PARAM::Row* Row = Param.Find(100000)) DaggerWeight = Layout.GetNumber(*Row, "weight");
                    }
                } else if (!Repo.FindCandidates(Param.ParamType).empty()) {
                    ++Near;  // a def exists, just for another data version
                }
            }
            std::printf("Dark Souls Remastered: %zu defs loaded from XML, %d of %d params found their def, %d have only an older def\n",
                        Repo.Size(), Found, Params, Near);
            CHECK(Params >= 39 && Found == Params - 2 && Near == 2);
            CHECK(DaggerWeight && *DaggerWeight == 0.5);
        } catch (const std::exception& E) {
            std::printf("FAIL real repository: %s\n", E.what());
            ++Failures;
        }
        fs::remove_all(Dir);
    }

    std::printf(Failures == 0 ? "ParamDefRepository tests passed\n" : "ParamDefRepository tests: %d failure(s)\n", Failures);
    return Failures;
}
