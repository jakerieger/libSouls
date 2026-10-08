//
// Created by Jake Rieger on 10/7/2026.
//

#include "PARAMDEF.hpp"

#include <pugixml.hpp>

#include <algorithm>
#include <charconv>
#include <regex>
#include <sstream>

namespace Souls {
    using DefType     = PARAMDEF::DefType;
    using EditFlags   = PARAMDEF::EditFlags;
    using EditorValue = PARAMDEF::EditorValue;

    namespace {
        [[noreturn]] void Fail(const std::string& Message) {
            throw BinaryException("PARAMDEF XML: " + Message);
        }

        std::string Trim(const std::string& Text) {
            const char* Whitespace = " \t\n\r\f\v";
            const size_t Start     = Text.find_first_not_of(Whitespace);
            if (Start == std::string::npos) return {};
            return Text.substr(Start, Text.find_last_not_of(Whitespace) - Start + 1);
        }

#pragma region Numbers
        // Integers and floats are written and read independent of locale. Floats use the shortest text that reads
        // back to the same value.
        template<typename T>
        std::string FormatNumber(T Value) {
            char Buffer[64];
            const auto Result = std::to_chars(Buffer, Buffer + sizeof Buffer, Value);
            return std::string(Buffer, Result.ptr);
        }

        template<typename T>
        T ParseNumber(const std::string& Text, const char* What) {
            std::string Trimmed = Trim(Text);
            if (!Trimmed.empty() && Trimmed.front() == '+') Trimmed.erase(Trimmed.begin());
            T Value{};
            const auto Result = std::from_chars(Trimmed.data(), Trimmed.data() + Trimmed.size(), Value);
            if (Result.ec != std::errc() || Result.ptr != Trimmed.data() + Trimmed.size()) {
                Fail(std::string("invalid ") + What + " \"" + Text + "\"");
            }
            return Value;
        }

        bool ParseBool(const std::string& Text, const char* What) {
            std::string Lower = Trim(Text);
            std::transform(Lower.begin(), Lower.end(), Lower.begin(), [](unsigned char C) { return std::tolower(C); });
            if (Lower == "true") return true;
            if (Lower == "false") return false;
            Fail(std::string("invalid ") + What + " \"" + Text + "\" (expected True or False)");
        }

        float AsFloat(const EditorValue& Value) {
            if (const auto* I = std::get_if<int32_t>(&Value)) return static_cast<float>(*I);
            if (const auto* F = std::get_if<float>(&Value)) return *F;
            if (const auto* D = std::get_if<double>(&Value)) return static_cast<float>(*D);
            return 0.f;
        }

        int32_t AsInt32(const EditorValue& Value) {
            if (const auto* I = std::get_if<int32_t>(&Value)) return *I;
            if (const auto* F = std::get_if<float>(&Value)) return static_cast<int32_t>(*F);
            if (const auto* D = std::get_if<double>(&Value)) return static_cast<int32_t>(*D);
            return 0;
        }

        double AsDouble(const EditorValue& Value) {
            if (const auto* I = std::get_if<int32_t>(&Value)) return *I;
            if (const auto* F = std::get_if<float>(&Value)) return *F;
            if (const auto* D = std::get_if<double>(&Value)) return *D;
            return 0.0;
        }

        bool IsIntegerLike(DefType Type) {
            switch (Type) {
                case DefType::s8:
                case DefType::u8:
                case DefType::s16:
                case DefType::u16:
                case DefType::s32:
                case DefType::u32:
                case DefType::b32: return true;
                default: return false;
            }
        }
#pragma endregion

#pragma region Flags
        std::string EditFlagsToString(EditFlags Flags) {
            const auto Raw = static_cast<int32_t>(Flags);
            if (Raw == 0) return "None";
            if ((Raw & ~(1 | 4)) != 0) return std::to_string(Raw);  // unknown bits: .NET prints the number
            std::string Result;
            if (Raw & 1) Result += "Wrap";
            if (Raw & 4) Result += std::string(Result.empty() ? "" : ", ") + "Lock";
            return Result;
        }

        EditFlags ParseEditFlags(const std::string& Text) {
            const std::string Trimmed = Trim(Text);
            if (Trimmed.empty()) Fail("empty EditFlags");
            if (std::isdigit(static_cast<unsigned char>(Trimmed.front())) || Trimmed.front() == '-') {
                return static_cast<EditFlags>(ParseNumber<int32_t>(Trimmed, "EditFlags"));
            }
            int32_t Result = 0;
            std::stringstream Stream(Trimmed);
            std::string Part;
            while (std::getline(Stream, Part, ',')) {
                Part = Trim(Part);
                std::transform(Part.begin(), Part.end(), Part.begin(), [](unsigned char C) { return std::tolower(C); });
                if (Part == "none") Result |= 0;
                else if (Part == "wrap") Result |= 1;
                else if (Part == "lock") Result |= 4;
                else Fail("unknown EditFlags value \"" + Part + "\"");
            }
            return static_cast<EditFlags>(Result);
        }
#pragma endregion

#pragma region Reading
        // The text of an element's child, if it has one.
        std::optional<std::string> ChildText(const pugi::xml_node& Node, const char* Name) {
            const pugi::xml_node Child = Node.child(Name);
            if (!Child) return std::nullopt;
            return std::string(Child.child_value());
        }

        std::string RequireChild(const pugi::xml_node& Node, const char* Name) {
            if (const auto Text = ChildText(Node, Name)) return *Text;
            Fail(std::string("missing <") + Name + "> element");
        }

        EditorValue ParseVariableValue(const PARAMDEF& Def, DefType Type, const std::string& Text) {
            if (!Def.VariableEditorValueTypes()) {
                return ParseNumber<float>(Text, "value");
            }
            if (IsIntegerLike(Type)) return ParseNumber<int32_t>(Text, "value");
            if (Type == DefType::f32 || Type == DefType::angle32) return ParseNumber<float>(Text, "value");
            if (Type == DefType::f64) return ParseNumber<double>(Text, "value");
            return std::monostate{};
        }

        EditorValue ReadVariableValueOrDefault(const PARAMDEF& Def,
                                               const pugi::xml_node& Node,
                                               DefType Type,
                                               const char* Name,
                                               const EditorValue& Default) {
            const auto Text = ChildText(Node, Name);
            if (Def.VariableEditorValueTypes() && !IsIntegerLike(Type) && Type != DefType::f32 &&
                Type != DefType::angle32 && Type != DefType::f64) {
                return std::monostate{};
            }
            if (!Text) return Default;
            return ParseVariableValue(Def, Type, *Text);
        }

        PARAMDEF::Field ReadField(const PARAMDEF& Def, const pugi::xml_node& Node) {
            PARAMDEF::Field Result;

            const pugi::xml_attribute DefAttribute = Node.attribute("Def");
            if (!DefAttribute) Fail("<Field> has no Def attribute");
            const std::string FieldDef = DefAttribute.value();

            static const std::regex OuterRx(R"(^\s*(\S+)\s+(.+?)(?:\s*=\s*(\S+))?\s*$)");
            static const std::regex BitRx(R"(^(.+?)\s*:\s*(\d+)$)");
            static const std::regex ArrayRx(R"(^(.+?)\s*\[\s*(\d+)\]$)");

            std::smatch Outer;
            if (!std::regex_match(FieldDef, Outer, OuterRx)) Fail("cannot parse field definition \"" + FieldDef + "\"");

            Result.DisplayType = ParamUtil::ParseDefType(Trim(Outer[1].str()));
            if (Outer[3].matched) {
                Result.Default = ParseVariableValue(Def, Result.DisplayType, Outer[3].str());
            } else {
                Result.Default = ParamUtil::GetDefaultDefault(&Def, Result.DisplayType);
            }

            std::string InternalName = Trim(Outer[2].str());
            Result.BitSize           = -1;
            Result.ArrayLength       = 1;
            std::smatch Match;
            if (ParamUtil::IsBitType(Result.DisplayType) && std::regex_match(InternalName, Match, BitRx)) {
                Result.BitSize = ParseNumber<int32_t>(Match[2].str(), "bit size");
                InternalName   = Match[1].str();
            } else if (ParamUtil::IsArrayType(Result.DisplayType)) {
                if (!std::regex_match(InternalName, Match, ArrayRx)) {
                    Fail("array field \"" + FieldDef + "\" needs a length, like name[4]");
                }
                Result.ArrayLength = ParseNumber<int32_t>(Match[2].str(), "array length");
                InternalName       = Match[1].str();
            }
            Result.InternalName = InternalName;

            Result.DisplayName = ChildText(Node, "DisplayName").value_or(Result.InternalName);
            Result.InternalType = ChildText(Node, "Enum").value_or(ParamUtil::DefTypeName(Result.DisplayType));
            Result.Description = ChildText(Node, "Description");
            Result.DisplayFormat = ChildText(Node, "DisplayFormat").value_or(ParamUtil::GetDefaultFormat(Result.DisplayType));
            if (const auto Flags = ChildText(Node, "EditFlags")) {
                Result.EditorFlags = ParseEditFlags(*Flags);
            } else {
                Result.EditorFlags = ParamUtil::GetDefaultEditFlags(Result.DisplayType);
            }
            Result.Minimum   = ReadVariableValueOrDefault(Def, Node, Result.DisplayType, "Minimum", ParamUtil::GetDefaultMinimum(&Def, Result.DisplayType));
            Result.Maximum   = ReadVariableValueOrDefault(Def, Node, Result.DisplayType, "Maximum", ParamUtil::GetDefaultMaximum(&Def, Result.DisplayType));
            Result.Increment = ReadVariableValueOrDefault(Def, Node, Result.DisplayType, "Increment", ParamUtil::GetDefaultIncrement(&Def, Result.DisplayType));
            if (const auto SortID = ChildText(Node, "SortID")) Result.SortID = ParseNumber<int32_t>(*SortID, "SortID");

            Result.UnkB8 = ChildText(Node, "UnkB8");
            Result.UnkC0 = ChildText(Node, "UnkC0");
            Result.UnkC8 = ChildText(Node, "UnkC8");
            return Result;
        }

        PARAMDEF ReadDocument(const pugi::xml_document& Document) {
            // In the interest of maximum compatibility the XmlVersion isn't checked: just try everything.
            const pugi::xml_node Root = Document.child("PARAMDEF");
            if (!Root) Fail("the root element is not <PARAMDEF>");

            PARAMDEF Def;
            Def.ParamType = RequireChild(Root, "ParamType");
            if (const auto DataVersion = ChildText(Root, "DataVersion")) {
                Def.DataVersion = ParseNumber<int16_t>(*DataVersion, "DataVersion");
            } else {
                Def.DataVersion = ParseNumber<int16_t>(RequireChild(Root, "Unk06"), "Unk06");
            }
            Def.BigEndian = ParseBool(RequireChild(Root, "BigEndian"), "BigEndian");
            Def.Unicode   = ParseBool(RequireChild(Root, "Unicode"), "Unicode");
            if (const auto FormatVersion = ChildText(Root, "FormatVersion")) {
                Def.FormatVersion = ParseNumber<int16_t>(*FormatVersion, "FormatVersion");
            } else {
                Def.FormatVersion = ParseNumber<int16_t>(RequireChild(Root, "Version"), "Version");
            }

            for (const pugi::xml_node& Node : Root.child("Fields").children("Field")) {
                Def.Fields.push_back(ReadField(Def, Node));
            }
            return Def;
        }
#pragma endregion

#pragma region Writing
        void AddTextElement(pugi::xml_node Parent, const char* Name, const std::string& Text) {
            Parent.append_child(Name).append_child(pugi::node_pcdata).set_value(Text.c_str());
        }

        // Elements holding a value are left out when it's the default, to keep the files small.
        void AddDefaultElement(pugi::xml_node Parent, const char* Name, const std::string& Value, const std::string& Default) {
            if (Value != Default) AddTextElement(Parent, Name, Value);
        }

        void AddOptionalElement(pugi::xml_node Parent, const char* Name, const std::optional<std::string>& Value) {
            if (Value) AddTextElement(Parent, Name, *Value);
        }

        std::string VariableValueToString(const PARAMDEF& Def, DefType Type, const EditorValue& Value) {
            if (Def.VariableEditorValueTypes()) {
                if (IsIntegerLike(Type)) return FormatNumber(AsInt32(Value));
                if (Type == DefType::f32 || Type == DefType::angle32) return FormatNumber(AsFloat(Value));
                if (Type == DefType::f64) return FormatNumber(AsDouble(Value));
                return "null";
            }
            return FormatNumber(AsFloat(Value));
        }

        void AddValueElement(pugi::xml_node Parent,
                             const PARAMDEF& Def,
                             DefType Type,
                             const char* Name,
                             const EditorValue& Value,
                             const EditorValue& Default) {
            if (Def.VariableEditorValueTypes()) {
                if (IsIntegerLike(Type)) {
                    if (AsInt32(Value) != AsInt32(Default)) AddTextElement(Parent, Name, FormatNumber(AsInt32(Value)));
                } else if (Type == DefType::f32 || Type == DefType::angle32) {
                    if (AsFloat(Value) != AsFloat(Default)) AddTextElement(Parent, Name, FormatNumber(AsFloat(Value)));
                } else if (Type == DefType::f64) {
                    if (AsDouble(Value) != AsDouble(Default)) AddTextElement(Parent, Name, FormatNumber(AsDouble(Value)));
                }
                return;
            }
            if (AsFloat(Value) != AsFloat(Default)) AddTextElement(Parent, Name, FormatNumber(AsFloat(Value)));
        }

        void AddField(pugi::xml_node Fields, const PARAMDEF& Def, const PARAMDEF::Field& Field) {
            pugi::xml_node Node = Fields.append_child("Field");

            std::string FieldDef = std::string(ParamUtil::DefTypeName(Field.DisplayType)) + " " + Field.InternalName;
            if (ParamUtil::IsBitType(Field.DisplayType) && Field.BitSize != -1) {
                FieldDef += ":" + std::to_string(Field.BitSize);
            } else if (ParamUtil::IsArrayType(Field.DisplayType)) {
                FieldDef += "[" + std::to_string(Field.ArrayLength) + "]";
            }
            if (Field.Default != ParamUtil::GetDefaultDefault(&Def, Field.DisplayType)) {
                FieldDef += " = " + VariableValueToString(Def, Field.DisplayType, Field.Default);
            }
            Node.append_attribute("Def") = FieldDef.c_str();

            AddDefaultElement(Node, "DisplayName", Field.DisplayName, Field.InternalName);
            AddDefaultElement(Node, "Enum", Field.InternalType, ParamUtil::DefTypeName(Field.DisplayType));
            AddOptionalElement(Node, "Description", Field.Description);
            AddDefaultElement(Node, "DisplayFormat", Field.DisplayFormat, ParamUtil::GetDefaultFormat(Field.DisplayType));
            AddDefaultElement(Node, "EditFlags", EditFlagsToString(Field.EditorFlags),
                              EditFlagsToString(ParamUtil::GetDefaultEditFlags(Field.DisplayType)));
            AddValueElement(Node, Def, Field.DisplayType, "Minimum", Field.Minimum, ParamUtil::GetDefaultMinimum(&Def, Field.DisplayType));
            AddValueElement(Node, Def, Field.DisplayType, "Maximum", Field.Maximum, ParamUtil::GetDefaultMaximum(&Def, Field.DisplayType));
            AddValueElement(Node, Def, Field.DisplayType, "Increment", Field.Increment, ParamUtil::GetDefaultIncrement(&Def, Field.DisplayType));
            if (Field.SortID != 0) AddTextElement(Node, "SortID", std::to_string(Field.SortID));

            AddOptionalElement(Node, "UnkB8", Field.UnkB8);
            AddOptionalElement(Node, "UnkC0", Field.UnkC0);
            AddOptionalElement(Node, "UnkC8", Field.UnkC8);
        }

        void BuildDocument(pugi::xml_document& Document, const PARAMDEF& Def, bool IncludeOffsets, int XmlVersion) {
            if (XmlVersion < 0 || XmlVersion > PARAMDEF::CurrentXmlVersion) {
                throw BinaryException("PARAMDEF XML version " + std::to_string(XmlVersion) + " is not recognized");
            }

            pugi::xml_node Declaration = Document.append_child(pugi::node_declaration);
            Declaration.append_attribute("version")  = "1.0";
            Declaration.append_attribute("encoding") = "utf-8";

            pugi::xml_node Root = Document.append_child("PARAMDEF");
            Root.append_attribute("XmlVersion") = XmlVersion;
            AddTextElement(Root, "ParamType", Def.ParamType);
            AddTextElement(Root, XmlVersion == 0 ? "Unk06" : "DataVersion", std::to_string(Def.DataVersion));
            AddTextElement(Root, "BigEndian", Def.BigEndian ? "True" : "False");
            AddTextElement(Root, "Unicode", Def.Unicode ? "True" : "False");
            AddTextElement(Root, XmlVersion == 0 ? "Version" : "FormatVersion", std::to_string(Def.FormatVersion));

            int32_t Offset = 0;
            pugi::xml_node Fields = Root.append_child("Fields");
            for (size_t I = 0; I < Def.Fields.size(); ++I) {
                if (IncludeOffsets) {
                    const int32_t CurrentSize = Def.GetFieldsSize(I + 1);
                    if (CurrentSize != Offset) {
                        char Hex[32];
                        std::snprintf(Hex, sizeof Hex, " +0x%X ", static_cast<unsigned>(Offset));
                        Fields.append_child(pugi::node_comment).set_value(Hex);
                        Offset = CurrentSize;
                    }
                }
                AddField(Fields, Def, Def.Fields[I]);
            }
        }
#pragma endregion
    }  // namespace

    PARAMDEF PARAMDEF::FromXml(const std::filesystem::path& Path) {
        pugi::xml_document Document;
        const pugi::xml_parse_result Result = Document.load_file(Path.c_str());
        if (!Result) {
            throw BinaryException("Failed to read PARAMDEF XML \"" + Path.string() + "\": " + Result.description());
        }
        return ReadDocument(Document);
    }

    PARAMDEF PARAMDEF::FromXmlString(std::string_view Xml) {
        pugi::xml_document Document;
        const pugi::xml_parse_result Result = Document.load_buffer(Xml.data(), Xml.size());
        if (!Result) {
            throw BinaryException(std::string("Failed to parse PARAMDEF XML: ") + Result.description());
        }
        return ReadDocument(Document);
    }

    void PARAMDEF::ToXml(const std::filesystem::path& Path, bool IncludeOffsets, int XmlVersion) const {
        pugi::xml_document Document;
        BuildDocument(Document, *this, IncludeOffsets, XmlVersion);
        if (Path.has_parent_path()) {
            std::filesystem::create_directories(Path.parent_path());
        }
        if (!Document.save_file(Path.c_str(), "  ", pugi::format_default, pugi::encoding_utf8)) {
            throw BinaryException("Failed to write PARAMDEF XML \"" + Path.string() + "\"");
        }
    }

    std::string PARAMDEF::ToXmlString(bool IncludeOffsets, int XmlVersion) const {
        pugi::xml_document Document;
        BuildDocument(Document, *this, IncludeOffsets, XmlVersion);
        std::ostringstream Stream;
        Document.save(Stream, "  ", pugi::format_default, pugi::encoding_utf8);
        return Stream.str();
    }
}  // namespace Souls
