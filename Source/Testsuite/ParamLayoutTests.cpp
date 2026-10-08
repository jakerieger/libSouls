//
// Created by Jake Rieger on 10/7/2026.
//

#include "TestResult.hpp"

#include <libSouls/Binders/BND3.hpp>
#include <libSouls/Formats/ParamLayout.hpp>

#include <algorithm>
#include <cmath>
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

    PARAMDEF MakeDef() {
        PARAMDEF Def;
        Def.FormatVersion = 104;
        Def.DataVersion   = 5;
        Def.ParamType     = "LAYOUT_TEST_ST";
        auto Add = [&](DefType Type, const char* Name, int Bits = -1, int Length = 1) {
            PARAMDEF::Field Field(&Def, Type, Name);
            Field.BitSize     = Bits;
            Field.ArrayLength = Length;
            Def.Fields.push_back(Field);
        };
        Add(DefType::s32, "id");                 // 0..3
        Add(DefType::u8, "a", 3);                // byte 4, bits 0-2
        Add(DefType::u8, "b", 5);                //         bits 3-7
        Add(DefType::s8, "sgn", 4);              // byte 5, bits 0-3 (signed)
        Add(DefType::u8, "c", 4);                //         bits 4-7
        Add(DefType::u16, "d", 12);              // bytes 6-7, bits 0-11
        Add(DefType::u16, "e", 4);               //            bits 12-15
        Add(DefType::dummy8, "pad", -1, 3);      // 8..10
        Add(DefType::dummy8, "spare", 2);        // byte 11, bits 0-1 (a dummy8 bitfield)
        Add(DefType::f32, "scale");              // 12..15
        Add(DefType::angle32, "turn");           // 16..19
        Add(DefType::f64, "precise");            // 20..27
        Add(DefType::fixstr, "label", -1, 8);    // 28..35
        Add(DefType::fixstrW, "wide", -1, 4);    // 36..43
        Add(DefType::b32, "enabled");            // 44..47
        Add(DefType::u32, "big");                // 48..51
        Add(DefType::s16, "small");              // 52..53
        Add(DefType::u8, "tiny");                // 54
        Def.Fields[0].Default = 7.0f;            // a default to check MakeRow with
        return Def;
    }

    constexpr size_t RowSize = 55;

    void TestLayout(bool Big) {
        const PARAMDEF Def = MakeDef();
        CHECK(Def.GetRowSize() == static_cast<int32_t>(RowSize));
        const ParamLayout Layout(Def, Big);
        CHECK(Layout.RowSize() == RowSize && Layout.FieldCount() == Def.Fields.size());
        CHECK(Layout.IndexOf("scale") == std::optional<size_t>(9) && !Layout.IndexOf("nope").has_value());
        CHECK(Layout.InternalName(2) == "b" && Layout.Type(11) == DefType::f64);

        // New rows start at the defaults.
        PARAM::Row Row = Layout.MakeRow(42, "Test row");
        CHECK(Row.ID == 42 && Row.Name == std::optional<std::string>("Test row") && Row.Bytes.size() == RowSize);
        CHECK(Layout.Get<int32_t>(Row, "id") == 7);
        CHECK(Layout.Get<float>(Row, "scale") == 0.f && Layout.Get<std::string>(Row, "label").empty());

        // Every type, set and read back.
        Layout.Set(Row, "id", int32_t{-123456});
        Layout.Set(Row, "a", uint8_t{5});
        Layout.Set(Row, "b", uint8_t{17});
        Layout.Set(Row, "sgn", int8_t{-3});
        Layout.Set(Row, "c", uint8_t{9});
        Layout.Set(Row, "d", uint16_t{0xABC});
        Layout.Set(Row, "e", uint16_t{0xD});
        Layout.Set(Row, "pad", std::vector<uint8_t>{1, 2, 3});
        Layout.Set(Row, "spare", uint8_t{2});
        Layout.Set(Row, "scale", 1.5f);
        Layout.Set(Row, "turn", -0.25f);
        Layout.Set(Row, "precise", 3.14159265358979);
        Layout.Set(Row, "label", std::string("hello"));
        Layout.Set(Row, "wide", std::string("\xE3\x82\xBD" "ab"));
        Layout.Set(Row, "enabled", int32_t{1});
        Layout.Set(Row, "big", uint32_t{4000000000u});
        Layout.Set(Row, "small", int16_t{-2});
        Layout.Set(Row, "tiny", uint8_t{200});

        CHECK(Layout.Get<int32_t>(Row, "id") == -123456);
        CHECK(Layout.Get<uint8_t>(Row, "a") == 5 && Layout.Get<uint8_t>(Row, "b") == 17);
        CHECK(Layout.Get<int8_t>(Row, "sgn") == -3 && Layout.Get<uint8_t>(Row, "c") == 9);
        CHECK(Layout.Get<uint16_t>(Row, "d") == 0xABC && Layout.Get<uint16_t>(Row, "e") == 0xD);
        CHECK((Layout.Get<std::vector<uint8_t>>(Row, "pad") == std::vector<uint8_t>{1, 2, 3}));
        CHECK(Layout.Get<uint8_t>(Row, "spare") == 2);
        CHECK(Layout.Get<float>(Row, "scale") == 1.5f && Layout.Get<float>(Row, "turn") == -0.25f);
        CHECK(Layout.Get<double>(Row, "precise") == 3.14159265358979);
        CHECK(Layout.Get<std::string>(Row, "label") == "hello");
        CHECK(Layout.Get<std::string>(Row, "wide") == "\xE3\x82\xBD" "ab");
        CHECK(Layout.Get<int32_t>(Row, "enabled") == 1 && Layout.Get<uint32_t>(Row, "big") == 4000000000u);
        CHECK(Layout.Get<int16_t>(Row, "small") == -2 && Layout.Get<uint8_t>(Row, "tiny") == 200);

        // The bytes land exactly where the format says. Bitfields fill from the low bit up.
        CHECK(Row.Bytes[4] == (5 | (17 << 3)));
        CHECK(Row.Bytes[5] == ((-3 & 0xF) | (9 << 4)));
        const uint16_t Packed = 0xABC | (0xD << 12);
        CHECK(Big ? (Row.Bytes[6] == (Packed >> 8) && Row.Bytes[7] == (Packed & 0xFF))
                  : (Row.Bytes[6] == (Packed & 0xFF) && Row.Bytes[7] == (Packed >> 8)));
        CHECK(Row.Bytes[8] == 1 && Row.Bytes[9] == 2 && Row.Bytes[10] == 3 && Row.Bytes[11] == 2);
        CHECK(Row.Bytes[28] == 'h' && Row.Bytes[33] == 0 && Row.Bytes[35] == 0);
        CHECK(Big ? (Row.Bytes[48] == 0xEE && Row.Bytes[51] == 0x00) : (Row.Bytes[48] == 0x00 && Row.Bytes[51] == 0xEE));

        // Setting one bitfield leaves its neighbours alone.
        Layout.Set(Row, "a", uint8_t{2});
        CHECK(Layout.Get<uint8_t>(Row, "a") == 2 && Layout.Get<uint8_t>(Row, "b") == 17);
        Layout.Set(Row, "sgn", int8_t{7});
        Layout.Set(Row, "sgn", int8_t{-8});
        CHECK(Layout.Get<int8_t>(Row, "sgn") == -8 && Layout.Get<uint8_t>(Row, "c") == 9);

        // Conversions: numbers adapt to the field, out-of-range bits fall off like they would in the file.
        Layout.SetNumber(Row, "tiny", 300);
        CHECK(Layout.Get<uint8_t>(Row, "tiny") == 44);
        Layout.SetNumber(Row, "scale", 2);
        Layout.SetNumber(Row, "precise", 0.5);
        CHECK(Layout.GetNumber(Row, "scale") == 2.0 && Layout.GetNumber(Row, "precise") == 0.5);
        Layout.Set(Row, "a", uint8_t{0xFF});  // only 3 bits fit
        CHECK(Layout.Get<uint8_t>(Row, "a") == 7 && Layout.Get<uint8_t>(Row, "b") == 17);
        Layout.Set(Row, "tiny", 3.9f);  // floats truncate into integer fields
        CHECK(Layout.Get<uint8_t>(Row, "tiny") == 3);
        CHECK(Layout.GetNumber(Row, "small") == -2.0);

        // Strings are cut to the field: 8 bytes holds 7 characters and the terminator.
        Layout.Set(Row, "label", std::string("0123456789"));
        CHECK(Layout.Get<std::string>(Row, "label") == "01234567");  // no room for a terminator: all 8 bytes
        Layout.Set(Row, "label", std::string("hi"));
        CHECK(Layout.Get<std::string>(Row, "label") == "hi" && Row.Bytes[30] == 0 && Row.Bytes[35] == 0);
        Layout.Set(Row, "wide", std::string("wxyz12"));
        CHECK(Layout.Get<std::string>(Row, "wide") == "wxyz");

        // Mistakes are errors, not silent.
        CHECK(Throws([&] { Layout.Get<float>(Row, "id"); }));
        CHECK(Throws([&] { Layout.Get(Row, "missing"); }));
        CHECK(Throws([&] { Layout.Set(Row, "id", std::string("text")); }));
        CHECK(Throws([&] { Layout.Set(Row, "label", int32_t{1}); }));
        CHECK(Throws([&] { Layout.Set(Row, "pad", std::vector<uint8_t>{1, 2}); }));
        CHECK(Throws([&] { Layout.GetNumber(Row, "label"); }));
        CHECK(Throws([&] { Layout.Get(Row, 999); }));
        PARAM::Row Short = Row;
        Short.Bytes.pop_back();
        CHECK(Throws([&] { Layout.Get(Short, 0); }));

        // Through a PARAM file and back.
        PARAM Param;
        Param.BigEndian           = Big;
        Param.Format2D            = PARAM::FormatFlags1::LongDataOffset | PARAM::FormatFlags1::OffsetParamType;
        Param.ParamType           = Def.ParamType;
        Param.ParamdefDataVersion = Def.DataVersion;
        Param.Rows.push_back(Row);
        Param.Rows.push_back(Layout.MakeRow(43));
        const PARAM Back = PARAM::Read(Param.Write());
        CHECK(Back.Matches(Def));
        const auto BoundLayout = ParamLayout::TryCreate(Back, Def);
        CHECK(BoundLayout.has_value());
        if (BoundLayout) {
            CHECK(BoundLayout->Get<std::string>(*Back.Find(42), "label") == "hi");
            CHECK(BoundLayout->Get<int32_t>(*Back.Find(43), "id") == 7);
        }
    }

    void TestMatching() {
        const PARAMDEF Def = MakeDef();
        PARAM Param;
        Param.ParamType           = Def.ParamType;
        Param.ParamdefDataVersion = Def.DataVersion;
        Param.Rows.push_back(ParamLayout(Def, false).MakeRow(1));
        const PARAM Read = PARAM::Read(Param.Write());
        CHECK(Read.Matches(Def));

        PARAMDEF OtherType = Def;
        OtherType.ParamType = "SOMETHING_ELSE";
        PARAMDEF OtherVersion = Def;
        OtherVersion.DataVersion = 6;
        PARAMDEF OtherSize = Def;
        OtherSize.Fields.pop_back();
        CHECK(!Read.Matches(OtherType) && !Read.Matches(OtherVersion) && !Read.Matches(OtherSize));
        CHECK(!ParamLayout::TryCreate(Read, OtherSize).has_value());

        const std::vector<PARAMDEF> Candidates{OtherType, OtherVersion, OtherSize, Def};
        CHECK(Read.FindMatchingDef(Candidates) == &Candidates[3]);
        const std::vector<PARAMDEF> None{OtherType, OtherSize};
        CHECK(Read.FindMatchingDef(None) == nullptr);

        // An empty param has no row size to contradict, so only type and version count.
        PARAM Empty;
        Empty.ParamType           = Def.ParamType;
        Empty.ParamdefDataVersion = Def.DataVersion;
        CHECK(Empty.Matches(Def) && Empty.Matches(OtherSize) && !Empty.Matches(OtherVersion));

        // Bad bitfields are caught when the layout is made.
        PARAMDEF BadBits = Def;
        BadBits.Fields[1].BitSize = 9;  // a u8 holds 8
        CHECK(Throws([&] { ParamLayout(BadBits, false); }));
        BadBits.Fields[1].BitSize = 0;
        CHECK(Throws([&] { ParamLayout(BadBits, false); }));
    }
}  // namespace

int RunParamLayoutTests() {
    Failures = 0;
    TestLayout(false);
    TestLayout(true);
    TestMatching();

    // Dark Souls Remastered: apply its real paramdefs to its real params and read everything.
    const fs::path Root = "C:/Program Files (x86)/Steam/steamapps/common/DARK SOULS REMASTERED";
    if (!fs::exists(Root / "paramdef/paramdef.paramdefbnd.dcx") || !fs::exists(Root / "param/GameParam/GameParam.parambnd.dcx")) {
        std::printf("Real layout checks skipped (Dark Souls Remastered not installed)\n");
    } else {
        try {
            std::vector<PARAMDEF> Defs;
            const BND3 DefBnd = BND3::Read(Root / "paramdef/paramdef.paramdefbnd.dcx");
            for (const BinderFile& File : DefBnd.Files) {
                if (File.Name && EndsWith(*File.Name, ".paramdef")) Defs.push_back(PARAMDEF::Read(File.Bytes));
            }

            int Params = 0, Rows = 0, Cells = 0, Strings = 0, NonFinite = 0, Unchanged = 0, Changed = 0, Unmatched = 0;
            BND3 ParamBnd = BND3::Read(Root / "param/GameParam/GameParam.parambnd.dcx");
            std::optional<double> DaggerWeight;
            for (const BinderFile& File : ParamBnd.Files) {
                if (!File.Name || !EndsWith(*File.Name, ".param")) continue;
                PARAM Param = PARAM::Read(File.Bytes);
                const PARAMDEF* Def = Param.FindMatchingDef(Defs);
                if (!Def) {
                    ++Unmatched;
                    std::printf("  no matching def for %s v%d (row size %lld, %zu defs)", Param.ParamType.c_str(),
                                Param.ParamdefDataVersion, static_cast<long long>(Param.DetectedSize), Defs.size());
                    for (const PARAMDEF& D : Defs) {
                        if (D.ParamType == Param.ParamType) std::printf(" [def v%d size %d]", D.DataVersion, D.GetRowSize());
                    }
                    std::printf("\n");
                    continue;
                }
                ++Params;
                const ParamLayout Layout(*Def, Param.BigEndian);

                for (PARAM::Row& Row : Param.Rows) {
                    ++Rows;
                    for (size_t I = 0; I < Layout.FieldCount(); ++I) {
                        const CellValue Value = Layout.Get(Row, I);
                        ++Cells;
                        if (const float* F = std::get_if<float>(&Value)) {
                            if (!std::isfinite(*F)) ++NonFinite;
                        }
                        if (const double* D = std::get_if<double>(&Value)) {
                            if (!std::isfinite(*D)) ++NonFinite;
                        }
                        if (std::holds_alternative<std::string>(Value)) ++Strings;

                        // Writing back what was read leaves the row unchanged, except where a string field held
                        // bytes after its terminator.
                        const std::vector<uint8_t> Before = Row.Bytes;
                        Layout.Set(Row, I, Value);
                        if (Row.Bytes == Before) {
                            ++Unchanged;
                        } else {
                            ++Changed;
                            if (!std::holds_alternative<std::string>(Value)) {
                                std::printf("FAIL %s row %d field %s changed when written back\n", Param.ParamType.c_str(),
                                            Row.ID, Layout.InternalName(I).c_str());
                                ++Failures;
                            }
                            Row.Bytes = Before;
                        }
                    }
                }

                if (Param.ParamType == "EQUIP_PARAM_WEAPON_ST") {
                    // Row 100000 is the Dagger (ダガー), which weighs 0.5 in the game.
                    if (const PARAM::Row* Row = Param.Find(100000)) {
                        CHECK(Row->Name == std::optional<std::string>("\xE3\x83\x80\xE3\x82\xAC\xE3\x83\xBC"));
                        DaggerWeight = Layout.GetNumber(*Row, "weight");
                    }
                }
            }
            std::printf("Dark Souls Remastered: %d params with a def (%d without), %d rows, %d cells (%d strings), "
                        "%d non-finite floats, %d unchanged / %d changed on write-back\n",
                        Params, Unmatched, Rows, Cells, Strings, NonFinite, Unchanged, Changed);
            if (DaggerWeight) std::printf("  Dagger weight: %g\n", *DaggerWeight);
            // The shipped defs for SP_EFFECT_PARAM_ST and CHARACTER_INIT_PARAM are an older data version than the params.
            CHECK(Params >= 39 && Rows > 10000 && Unmatched == 2);
            CHECK(NonFinite == 0);
            CHECK(DaggerWeight.has_value() && *DaggerWeight == 0.5);
        } catch (const std::exception& E) {
            std::printf("FAIL real layouts: %s\n", E.what());
            ++Failures;
        }
    }

    std::printf(Failures == 0 ? "ParamLayout tests passed\n" : "ParamLayout tests: %d failure(s)\n", Failures);
    return Failures;
}
