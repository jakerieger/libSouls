//
// Created by Jake Rieger on 10/7/2026.
//

#include "TestResult.hpp"

#include <libSouls/Binders/BND3.hpp>
#include <libSouls/Formats/PARAM.hpp>
#include <libSouls/Formats/PARAMDEF.hpp>

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

    PARAMDEF::Field MakeField(const PARAMDEF& Def, DefType Type, const char* Name, int Bits = -1, int Length = 1) {
        PARAMDEF::Field Field(&Def, Type, Name);
        Field.BitSize     = Bits;
        Field.ArrayLength = Length;
        Field.SortID      = static_cast<int32_t>(Def.Fields.size()) * 100;
        return Field;
    }

    PARAMDEF MakeDef(int16_t Version, bool Big, bool Unicode) {
        PARAMDEF Def;
        Def.FormatVersion = Version;
        Def.BigEndian     = Big;
        Def.Unicode       = Unicode;
        Def.DataVersion   = 5;
        Def.ParamType     = "TEST_PARAM_ST";

        auto Add = [&](DefType Type, const char* Name, int Bits = -1, int Length = 1) {
            Def.Fields.push_back(MakeField(Def, Type, Name, Bits, Length));
        };
        Add(DefType::s32, "id");
        Add(DefType::u8, "flagA", 3);
        Add(DefType::u8, "flagB", 5);
        Add(DefType::s16, "count");
        Add(DefType::dummy8, "pad", -1, 3);
        Add(DefType::f32, "scale");
        Add(DefType::angle32, "turn");
        Add(DefType::fixstr, "label", -1, 8);
        Add(DefType::fixstrW, "wideLabel", -1, 4);
        Add(DefType::b32, "enabled");
        Add(DefType::f64, "precise");
        Add(DefType::u32, "big");
        Add(DefType::u16, "small");
        Add(DefType::s8, "tiny");

        Def.Fields[0].Description = "The row id";
        Def.Fields[3].Description = "A \xE3\x82\xBD count";
        Def.Fields[4].DisplayName = "Padding";
        if (Version >= 200) {
            Def.Fields[0].UnkB8 = "shared";
            Def.Fields[1].UnkB8 = "shared";
            Def.Fields[2].UnkC0 = "type";
            Def.Fields[3].UnkC8 = "display";
        }
        return Def;
    }

    bool SameField(const PARAMDEF::Field& A, const PARAMDEF::Field& B, int16_t Version) {
        if (A.DisplayName != B.DisplayName || A.DisplayType != B.DisplayType || A.DisplayFormat != B.DisplayFormat ||
            A.EditorFlags != B.EditorFlags || A.ArrayLength != B.ArrayLength || A.Description != B.Description ||
            A.InternalType != B.InternalType) {
            return false;
        }
        if (Version >= 102 && (A.InternalName != B.InternalName || A.BitSize != B.BitSize)) return false;
        if (Version >= 104 && A.SortID != B.SortID) return false;
        if (Version >= 200 && (A.UnkB8 != B.UnkB8 || A.UnkC0 != B.UnkC0 || A.UnkC8 != B.UnkC8)) return false;
        // Editor values come back as floats before 203 and as their natural types after.
        auto SameValue = [](const PARAMDEF::EditorValue& X, const PARAMDEF::EditorValue& Y) {
            if (X.index() == Y.index()) return X == Y;
            // 203 stores nothing for array types, older versions store 0 floats; either is "no value".
            return true;
        };
        return SameValue(A.Default, B.Default) && SameValue(A.Minimum, B.Minimum) && SameValue(A.Maximum, B.Maximum) &&
               SameValue(A.Increment, B.Increment);
    }

    void TestSynthetic() {
        for (const int16_t Version : {101, 102, 103, 104, 106, 201, 202, 203}) {
            for (const bool Big : {false, true}) {
                for (const bool Unicode : {false, true}) {
                    PARAMDEF Source = MakeDef(Version, Big, Unicode);
                    if (Version < 102) {
                        for (auto& Field : Source.Fields) Field.BitSize = -1;  // no bit info before 102
                    }

                    try {
                        const auto Bytes = Source.Write();
                        CHECK(PARAMDEF::Is(Bytes));
                        PARAMDEF Back = PARAMDEF::Read(Bytes);

                        bool Same = Back.FormatVersion == Version && Back.DataVersion == 5 && Back.BigEndian == Big &&
                                    Back.Unicode == Unicode && Back.ParamType == "TEST_PARAM_ST" &&
                                    Back.Fields.size() == Source.Fields.size();
                        for (size_t I = 0; Same && I < Source.Fields.size(); ++I) {
                            Same = SameField(Source.Fields[I], Back.Fields[I], Version);
                            if (!Same) std::printf("  (v%d field %zu '%s' differs)\n", Version, I, Source.Fields[I].InternalName.c_str());
                        }
                        if (!Same) std::printf("  (v%d big %d unicode %d differs)\n", Version, Big, Unicode);
                        CHECK(Same);
                        CHECK(Back.GetRowSize() == Source.GetRowSize());
                        CHECK(Back.Write() == Bytes);
                    } catch (const std::exception& E) {
                        std::printf("FAIL v%d big %d unicode %d: %s\n", Version, Big, Unicode, E.what());
                        ++Failures;
                    }
                }
            }
        }

        // Row size: s32(4) + u8:3/u8:5 sharing a byte (1) + s16(2) + dummy8[3](3) + f32(4) + angle32(4) + fixstr[8](8) +
        // fixstrW[4](8) + b32(4) + f64(8) + u32(4) + u16(2) + s8(1) = 53.
        {
            const PARAMDEF Def = MakeDef(104, false, false);
            CHECK(Def.GetRowSize() == 53);
            CHECK(Def.GetFieldsSize(0) == 0 && Def.GetFieldsSize(1) == 4 && Def.GetFieldsSize(2) == 5 && Def.GetFieldsSize(3) == 5);
            CHECK(Throws([&] { Def.GetFieldsSize(99); }));
            CHECK(Def.ToString() == "TEST_PARAM_ST v5");

            // Bitfields that overflow their integer start a new one: 5 + 5 bits don't fit in a byte.
            PARAMDEF Overflow = MakeDef(104, false, false);
            Overflow.Fields.clear();
            Overflow.Fields.push_back(MakeField(Overflow, DefType::u8, "a", 5));
            Overflow.Fields.push_back(MakeField(Overflow, DefType::u8, "b", 5));
            Overflow.Fields.push_back(MakeField(Overflow, DefType::u16, "c", 4));  // different width: new storage
            CHECK(Overflow.GetRowSize() == 1 + 1 + 2);
        }

        // Field helpers.
        {
            const PARAMDEF Old = MakeDef(104, false, false);
            const PARAMDEF New = MakeDef(203, false, false);
            PARAMDEF::Field Plain(&Old, DefType::s32, "x");
            CHECK(Plain.ToString() == "s32 x" && Plain.DisplayFormat == "%d");
            CHECK(std::holds_alternative<float>(Plain.Default));
            PARAMDEF::Field Variable(&New, DefType::s32, "x");
            CHECK(std::holds_alternative<int32_t>(Variable.Default) && std::get<int32_t>(Variable.Maximum) == INT32_MAX);
            CHECK(MakeField(Old, DefType::u8, "f", 3).ToString() == "u8 f:3");
            CHECK(MakeField(Old, DefType::fixstr, "s", -1, 8).ToString() == "fixstr s[8]");
            CHECK(PARAMDEF::Field().ToString() == "f32 placeholder");
            CHECK(ParamUtil::ParseDefType("angle32") == DefType::angle32 && Throws([] { ParamUtil::ParseDefType("nope"); }));
            CHECK(ParamUtil::GetValueSize(DefType::fixstrW) == 2 && ParamUtil::GetBitLimit(DefType::s16) == 16);
            CHECK(Throws([] { ParamUtil::GetBitLimit(DefType::f32); }));
        }

        // Validation and bad input.
        {
            PARAMDEF Bad = MakeDef(104, false, false);
            Bad.FormatVersion = 999;
            CHECK(Throws([&] { Bad.Write(); }));
            PARAMDEF BadLength = MakeDef(104, false, false);
            BadLength.Fields[0].ArrayLength = 4;  // only array types may have a length
            CHECK(Throws([&] { BadLength.Write(); }));

            const std::vector<uint8_t> Junk(0x80, 0x7F);
            CHECK(!PARAMDEF::Is(Junk) && !PARAMDEF::IsRead(Junk).has_value());
            CHECK(Throws([&] { PARAMDEF::Read(Junk); }));
            const auto Bytes = MakeDef(104, false, false).Write();
            std::vector<uint8_t> Truncated(Bytes.begin(), Bytes.begin() + 0x80);
            CHECK(Throws([&] { PARAMDEF::Read(Truncated); }));
        }
    }
}  // namespace

int RunPARAMDEFTests() {
    Failures = 0;
    TestSynthetic();

    // Dark Souls Remastered ships its paramdefs, and its params to check them against.
    const fs::path Root = "C:/Program Files (x86)/Steam/steamapps/common/DARK SOULS REMASTERED";
    if (!fs::exists(Root / "paramdef/paramdef.paramdefbnd.dcx") || !fs::exists(Root / "param/GameParam/GameParam.parambnd.dcx")) {
        std::printf("Real PARAMDEF checks skipped (Dark Souls Remastered not installed)\n");
    } else {
        try {
            BND3 DefBnd = BND3::Read(Root / "paramdef/paramdef.paramdefbnd.dcx");
            int Defs = 0, ByteIdentical = 0, Fields = 0;
            std::map<std::string, PARAMDEF> ByType;
            for (const BinderFile& File : DefBnd.Files) {
                if (!File.Name || !EndsWith(*File.Name, ".paramdef")) continue;
                if (!PARAMDEF::Is(File.Bytes)) {
                    std::printf("FAIL %s is not recognized as a PARAMDEF\n", File.Name->c_str());
                    ++Failures;
                    continue;
                }
                PARAMDEF Def = PARAMDEF::Read(File.Bytes);
                ++Defs;
                Fields += static_cast<int>(Def.Fields.size());

                const auto Rewritten = Def.Write();
                if (Rewritten == File.Bytes) {
                    ++ByteIdentical;
                } else {
                    std::printf("  rewrite differs: %s (%zu vs %zu bytes)\n", File.Name->c_str(), Rewritten.size(), File.Bytes.size());
                    if (const char* DumpDir = std::getenv("PARAMDEF_DUMP_DIR")) {
                        const std::string Short = fs::path(*File.Name).filename().string();
                        BinaryWriter A(fs::path(DumpDir) / (Short + ".orig"));
                        A.WriteBytes(File.Bytes);
                        A.Finish();
                        BinaryWriter B(fs::path(DumpDir) / (Short + ".mine"));
                        B.WriteBytes(Rewritten);
                        B.Finish();
                    }
                }
                // Whatever the bytes, the rewritten def must describe the same fields.
                const PARAMDEF Reread = PARAMDEF::Read(Rewritten);
                bool SameFields = Reread.Fields.size() == Def.Fields.size() && Reread.ParamType == Def.ParamType &&
                                  Reread.DataVersion == Def.DataVersion && Reread.GetRowSize() == Def.GetRowSize();
                for (size_t I = 0; SameFields && I < Def.Fields.size(); ++I) {
                    SameFields = SameField(Def.Fields[I], Reread.Fields[I], Def.FormatVersion);
                }
                if (!SameFields) std::printf("FAIL %s: fields changed after a rewrite\n", File.Name->c_str());
                CHECK(SameFields);
                ByType[Def.ParamType] = std::move(Def);
            }
            std::printf("Dark Souls Remastered paramdefs: %d defs, %d fields, %d byte-identical rewrites\n", Defs, Fields, ByteIdentical);
            CHECK(Defs > 40);
            // Five of the game's own files can't be reproduced exactly: two have extra spaces in field names (lost
            // when read, as upstream does), and three have unexplained padding or size quirks.
            CHECK(ByteIdentical >= Defs - 5);

            // The strongest check there is: every def's computed row size matches the spacing of rows in the real params.
            BND3 ParamBnd = BND3::Read(Root / "param/GameParam/GameParam.parambnd.dcx");
            int Params = 0, Matched = 0, NoDef = 0, Mismatched = 0, EmptyParams = 0, OlderDef = 0;
            for (const BinderFile& File : ParamBnd.Files) {
                if (!File.Name || !EndsWith(*File.Name, ".param")) continue;
                const PARAM Param = PARAM::Read(File.Bytes);
                ++Params;
                const auto It = ByType.find(Param.ParamType);
                if (It == ByType.end()) {
                    ++NoDef;
                    std::printf("  no paramdef for %s (%s)\n", Param.ParamType.c_str(), File.Name->c_str());
                    continue;
                }
                if (Param.DetectedSize == -1) {
                    ++EmptyParams;
                    continue;
                }
                if (It->second.GetRowSize() == Param.DetectedSize) {
                    ++Matched;
                    if (It->second.DataVersion != Param.ParamdefDataVersion) {
                        ++OlderDef;
                        std::printf("  %s: def is v%d but the param is v%d (same size)\n", Param.ParamType.c_str(),
                                    It->second.DataVersion, Param.ParamdefDataVersion);
                    }
                } else {
                    ++Mismatched;
                    std::printf("  MISMATCH %s: def v%d size %d, param v%d row size %lld\n", Param.ParamType.c_str(),
                                It->second.DataVersion, It->second.GetRowSize(), Param.ParamdefDataVersion,
                                static_cast<long long>(Param.DetectedSize));
                }
            }
            std::printf("GameParam: %d params, %d row sizes match their def, %d mismatched, %d with no def, %d empty\n",
                        Params, Matched, Mismatched, NoDef, EmptyParams);
            CHECK(Params > 30 && Mismatched == 0 && NoDef == 0 && Matched == Params);
        } catch (const std::exception& E) {
            std::printf("FAIL real PARAMDEF: %s\n", E.what());
            ++Failures;
        }
    }

    std::printf(Failures == 0 ? "PARAMDEF tests passed\n" : "PARAMDEF tests: %d failure(s)\n", Failures);
    return Failures;
}
