//
// Created by Jake Rieger on 10/7/2026.
//

#include "TestResult.hpp"

#include <libSouls/Binders/BND3.hpp>
#include <libSouls/Formats/PARAMDEF.hpp>

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

    bool Contains(const std::string& Text, const std::string& Part) {
        return Text.find(Part) != std::string::npos;
    }

    // Everything the XML form carries must survive.
    bool SameDef(const PARAMDEF& A, const PARAMDEF& B) {
        if (A.ParamType != B.ParamType || A.DataVersion != B.DataVersion || A.BigEndian != B.BigEndian ||
            A.Unicode != B.Unicode || A.FormatVersion != B.FormatVersion || A.Fields.size() != B.Fields.size()) {
            return false;
        }
        for (size_t I = 0; I < A.Fields.size(); ++I) {
            const auto& X = A.Fields[I];
            const auto& Y = B.Fields[I];
            if (X.DisplayName != Y.DisplayName || X.DisplayType != Y.DisplayType || X.DisplayFormat != Y.DisplayFormat ||
                X.Default != Y.Default || X.Minimum != Y.Minimum || X.Maximum != Y.Maximum || X.Increment != Y.Increment ||
                X.EditorFlags != Y.EditorFlags || X.ArrayLength != Y.ArrayLength || X.Description != Y.Description ||
                X.InternalType != Y.InternalType || X.InternalName != Y.InternalName || X.BitSize != Y.BitSize ||
                X.SortID != Y.SortID || X.UnkB8 != Y.UnkB8 || X.UnkC0 != Y.UnkC0 || X.UnkC8 != Y.UnkC8) {
                return false;
            }
        }
        return true;
    }

    PARAMDEF MakeDef(int16_t Version) {
        PARAMDEF Def;
        Def.FormatVersion = Version;
        Def.DataVersion   = 7;
        Def.BigEndian     = false;
        Def.Unicode       = true;
        Def.ParamType     = "XML_TEST_ST";

        auto Add = [&](DefType Type, const char* Name, int Bits = -1, int Length = 1) {
            PARAMDEF::Field Field(&Def, Type, Name);
            Field.BitSize     = Bits;
            Field.ArrayLength = Length;
            Field.SortID      = static_cast<int32_t>(Def.Fields.size()) * 10;
            Def.Fields.push_back(Field);
        };
        Add(DefType::s32, "id");
        Add(DefType::u8, "flagA", 3);
        Add(DefType::u8, "flagB", 5);
        Add(DefType::dummy8, "pad", -1, 3);
        Add(DefType::f32, "scale");
        Add(DefType::angle32, "turn");
        Add(DefType::fixstr, "label", -1, 8);
        Add(DefType::fixstrW, "wide", -1, 4);
        Add(DefType::b32, "enabled");
        Add(DefType::f64, "precise");
        Add(DefType::u32, "big");
        Add(DefType::s16, "small");

        // Values that differ from the defaults, so there's something to write.
        Def.Fields[0].Default     = Version >= 203 ? PARAMDEF::EditorValue(int32_t{5}) : PARAMDEF::EditorValue(5.f);
        Def.Fields[0].Minimum     = Version >= 203 ? PARAMDEF::EditorValue(int32_t{-10}) : PARAMDEF::EditorValue(-10.f);
        Def.Fields[0].Maximum     = Version >= 203 ? PARAMDEF::EditorValue(int32_t{999}) : PARAMDEF::EditorValue(999.f);
        Def.Fields[0].Increment   = Version >= 203 ? PARAMDEF::EditorValue(int32_t{5}) : PARAMDEF::EditorValue(5.f);
        Def.Fields[0].DisplayName = "The \"id\" <of> the row & more";
        Def.Fields[0].Description = "Line one\nLine two with <tags> & ampersands, \"quotes\" and 'apostrophes' \xE3\x82\xBD";
        Def.Fields[0].EditorFlags = PARAMDEF::EditFlags::Lock | PARAMDEF::EditFlags::Wrap;
        Def.Fields[0].InternalType = "ENUM_TYPE";
        Def.Fields[0].DisplayFormat = "%x";
        Def.Fields[4].Default = Version >= 203 ? PARAMDEF::EditorValue(0.1f) : PARAMDEF::EditorValue(0.1f);
        Def.Fields[4].Increment = Version >= 203 ? PARAMDEF::EditorValue(0.5f) : PARAMDEF::EditorValue(0.5f);
        Def.Fields[9].Minimum   = Version >= 203 ? PARAMDEF::EditorValue(-1.5e300) : PARAMDEF::EditorValue(-1.5f);
        Def.Fields[3].EditorFlags = PARAMDEF::EditFlags::Wrap;  // not dummy8's default
        Def.Fields[2].Description = "";  // present but empty
        if (Version >= 200) {
            Def.Fields[1].UnkB8 = "b8 text";
            Def.Fields[1].UnkC0 = "c0 text";
            Def.Fields[1].UnkC8 = "c8 text";
        }
        return Def;
    }

    void TestSynthetic() {
        for (const int16_t Version : {104, 106, 201, 202, 203}) {
            PARAMDEF Source = MakeDef(Version);
            try {
                const std::string Xml = Source.ToXmlString();
                const PARAMDEF Back   = PARAMDEF::FromXmlString(Xml);
                const bool Same       = SameDef(Source, Back);
                if (!Same) std::printf("  (v%d: XML round trip differs)\n%s\n", Version, Xml.c_str());
                CHECK(Same);
                CHECK(PARAMDEF::FromXmlString(Back.ToXmlString()).Write() == Source.Write());  // second pass is stable too
            } catch (const std::exception& E) {
                std::printf("FAIL v%d: %s\n", Version, E.what());
                ++Failures;
            }
        }

        const PARAMDEF Def = MakeDef(104);

        // Default values are left out of the XML.
        {
            const std::string Xml = Def.ToXmlString();
            CHECK(Contains(Xml, "<PARAMDEF XmlVersion=\"3\">"));
            CHECK(Contains(Xml, "<ParamType>XML_TEST_ST</ParamType>") && Contains(Xml, "<BigEndian>False</BigEndian>") &&
                  Contains(Xml, "<Unicode>True</Unicode>"));
            CHECK(Contains(Xml, "Def=\"s32 id = 5\"") && Contains(Xml, "Def=\"u8 flagA:3\"") &&
                  Contains(Xml, "Def=\"dummy8 pad[3]\"") && Contains(Xml, "Def=\"fixstrW wide[4]\""));
            CHECK(Contains(Xml, "<EditFlags>Wrap, Lock</EditFlags>") && Contains(Xml, "<DisplayFormat>%x</DisplayFormat>"));
            CHECK(!Contains(Xml, "<DisplayName>flagA"));  // equals the internal name, so omitted
            CHECK(!Contains(Xml, "<Enum>s8") && !Contains(Xml, "<SortID>0</SortID>"));
            CHECK(Contains(Xml, "&lt;tags&gt; &amp; ampersands"));  // escaped
            // Floats use the shortest text that reads back exactly.
            CHECK(Contains(Xml, "<Increment>0.5</Increment>") && Contains(Xml, "Def=\"f32 scale = 0.1\""));
        }

        // Offsets as comments.
        {
            const std::string Xml = Def.ToXmlString(true);
            CHECK(Contains(Xml, "<!-- +0x0 -->") && Contains(Xml, "<!-- +0x4 -->") && Contains(Xml, "<!-- +0x8 -->"));
            // One comment per field except flagB, which shares flagA's byte (so the offset doesn't move).
            size_t Comments = 0;
            for (size_t At = Xml.find("<!-- +0x"); At != std::string::npos; At = Xml.find("<!-- +0x", At + 1)) ++Comments;
            CHECK(Comments == Def.Fields.size() - 1);
            CHECK(SameDef(Def, PARAMDEF::FromXmlString(Xml)));
        }

        // Older layouts (0 to 2) name two elements differently and read back.
        {
            const std::string Xml = Def.ToXmlString(false, 0);
            CHECK(Contains(Xml, "<Unk06>7</Unk06>") && Contains(Xml, "<Version>104</Version>") && !Contains(Xml, "<DataVersion>"));
            CHECK(SameDef(Def, PARAMDEF::FromXmlString(Xml)));
            CHECK(SameDef(Def, PARAMDEF::FromXmlString(Def.ToXmlString(false, 2))));
            CHECK(Throws([&] { Def.ToXmlString(false, 4); }) && Throws([&] { Def.ToXmlString(false, -1); }));
        }

        // A hand-written file as the community shares them: most elements missing, so defaults apply.
        {
            const char* Xml = R"(<?xml version="1.0" encoding="utf-8"?>
<PARAMDEF XmlVersion="3">
  <ParamType>HAND_WRITTEN_ST</ParamType>
  <DataVersion>2</DataVersion>
  <BigEndian>False</BigEndian>
  <Unicode>True</Unicode>
  <FormatVersion>203</FormatVersion>
  <Fields>
    <Field Def="s32 itemId = 100">
      <DisplayName>Item ID</DisplayName>
      <Description>Which item.</Description>
      <Maximum>9999</Maximum>
      <SortID>200</SortID>
    </Field>
    <Field Def="u8 option:3" />
    <Field Def="dummy8 pad[5]" />
    <Field Def="f32 ratio = 1.5">
      <EditFlags>None</EditFlags>
      <Increment>0.25</Increment>
    </Field>
    <Field Def="f64 big">
      <Maximum>1e300</Maximum>
    </Field>
  </Fields>
</PARAMDEF>)";
            PARAMDEF Hand = PARAMDEF::FromXmlString(Xml);
            CHECK(Hand.ParamType == "HAND_WRITTEN_ST" && Hand.DataVersion == 2 && Hand.FormatVersion == 203 && Hand.Fields.size() == 5);
            CHECK(Hand.VariableEditorValueTypes());
            const auto& Id = Hand.Fields[0];
            CHECK(Id.InternalName == "itemId" && Id.DisplayName == "Item ID" && Id.Description == std::optional<std::string>("Which item."));
            CHECK(Id.Default == PARAMDEF::EditorValue(int32_t{100}) && Id.Maximum == PARAMDEF::EditorValue(int32_t{9999}));
            CHECK(Id.Minimum == PARAMDEF::EditorValue(INT32_MIN) && Id.SortID == 200 && Id.InternalType == "s32");
            CHECK(Id.DisplayFormat == "%d" && Id.EditorFlags == PARAMDEF::EditFlags::Wrap);
            CHECK(Hand.Fields[1].BitSize == 3 && Hand.Fields[1].InternalName == "option" && Hand.Fields[1].DisplayName == "option");
            CHECK(Hand.Fields[2].ArrayLength == 5 && Hand.Fields[2].EditorFlags == PARAMDEF::EditFlags::None);
            CHECK(std::holds_alternative<std::monostate>(Hand.Fields[2].Default));
            CHECK(Hand.Fields[3].Default == PARAMDEF::EditorValue(1.5f) && Hand.Fields[3].Increment == PARAMDEF::EditorValue(0.25f));
            CHECK(Hand.Fields[4].Maximum == PARAMDEF::EditorValue(1e300));
            CHECK(Hand.GetRowSize() == 4 + 1 + 5 + 4 + 8);
            // And it can be written as a binary PARAMDEF.
            const PARAMDEF Binary = PARAMDEF::Read(Hand.Write());
            CHECK(Binary.Fields.size() == 5 && Binary.GetRowSize() == Hand.GetRowSize());
        }

        // Files: folders are created, and what's written reads back.
        {
            const fs::path Dir  = fs::temp_directory_path() / "libsouls_paramdef_xml_test";
            const fs::path Path = Dir / "nested" / "def.xml";
            fs::remove_all(Dir);
            Def.ToXml(Path);
            CHECK(fs::exists(Path));
            CHECK(SameDef(Def, PARAMDEF::FromXml(Path)));
            fs::remove_all(Dir);
        }

        // Bad input is an error with a reason.
        {
            CHECK(Throws([] { PARAMDEF::FromXml("this/file/does/not/exist.xml"); }));
            CHECK(Throws([] { PARAMDEF::FromXmlString("not xml at all"); }));
            CHECK(Throws([] { PARAMDEF::FromXmlString("<SOMETHING_ELSE/>"); }));
            CHECK(Throws([] { PARAMDEF::FromXmlString("<PARAMDEF><ParamType>X</ParamType></PARAMDEF>"); }));  // missing parts
            const std::string Head =
              "<PARAMDEF><ParamType>X</ParamType><DataVersion>1</DataVersion><BigEndian>False</BigEndian><Unicode>False</Unicode>"
              "<FormatVersion>104</FormatVersion><Fields>";
            const std::string Tail = "</Fields></PARAMDEF>";
            CHECK(!Throws([&] { PARAMDEF::FromXmlString(Head + "<Field Def=\"s32 a\"/>" + Tail); }));
            CHECK(Throws([&] { PARAMDEF::FromXmlString(Head + "<Field/>" + Tail); }));                           // no Def
            CHECK(Throws([&] { PARAMDEF::FromXmlString(Head + "<Field Def=\"bogus a\"/>" + Tail); }));           // unknown type
            CHECK(Throws([&] { PARAMDEF::FromXmlString(Head + "<Field Def=\"fixstr a\"/>" + Tail); }));          // no length
            CHECK(Throws([&] { PARAMDEF::FromXmlString(Head + "<Field Def=\"s32 a = x\"/>" + Tail); }));         // bad default
            CHECK(Throws([&] { PARAMDEF::FromXmlString(Head + "<Field Def=\"s32 a\"><EditFlags>Bogus</EditFlags></Field>" + Tail); }));
        }
    }
}  // namespace

int RunPARAMDEFXmlTests() {
    Failures = 0;
    TestSynthetic();

    // Every real def survives the trip through XML and still writes the same binary file.
    const fs::path Root = "C:/Program Files (x86)/Steam/steamapps/common/DARK SOULS REMASTERED";
    if (!fs::exists(Root / "paramdef/paramdef.paramdefbnd.dcx")) {
        std::printf("Real PARAMDEF XML checks skipped (Dark Souls Remastered not installed)\n");
    } else {
        try {
            const BND3 DefBnd = BND3::Read(Root / "paramdef/paramdef.paramdefbnd.dcx");
            int Defs = 0, Fields = 0, Identical = 0;
            for (const BinderFile& File : DefBnd.Files) {
                if (!File.Name || !EndsWith(*File.Name, ".paramdef")) continue;
                PARAMDEF Original = PARAMDEF::Read(File.Bytes);
                ++Defs;
                Fields += static_cast<int>(Original.Fields.size());

                PARAMDEF FromXml = PARAMDEF::FromXmlString(Original.ToXmlString());
                if (!SameDef(Original, FromXml)) {
                    std::printf("FAIL %s: fields changed through XML\n", File.Name->c_str());
                    ++Failures;
                    continue;
                }
                if (FromXml.Write() == Original.Write()) {
                    ++Identical;
                } else {
                    std::printf("FAIL %s: writes a different binary after XML\n", File.Name->c_str());
                    ++Failures;
                }
            }
            std::printf("Dark Souls Remastered paramdefs through XML: %d defs, %d fields, %d writing identical binaries\n",
                        Defs, Fields, Identical);
            CHECK(Defs > 40 && Identical == Defs);
        } catch (const std::exception& E) {
            std::printf("FAIL real PARAMDEF XML: %s\n", E.what());
            ++Failures;
        }
    }

    std::printf(Failures == 0 ? "PARAMDEF XML tests passed\n" : "PARAMDEF XML tests: %d failure(s)\n", Failures);
    return Failures;
}
