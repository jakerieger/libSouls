//
// Created by Jake Rieger on 10/7/2026.
//

#include "PARAMDEF.hpp"

#include <libSouls/TextEncoding.hpp>

#include <algorithm>
#include <cfloat>
#include <climits>
#include <map>
#include <regex>

namespace Souls {
    using DefType     = PARAMDEF::DefType;
    using EditFlags   = PARAMDEF::EditFlags;
    using EditorValue = PARAMDEF::EditorValue;

#pragma region ParamUtil
    namespace ParamUtil {
        const char* DefTypeName(DefType Type) {
            switch (Type) {
                case DefType::s8: return "s8";
                case DefType::u8: return "u8";
                case DefType::s16: return "s16";
                case DefType::u16: return "u16";
                case DefType::s32: return "s32";
                case DefType::u32: return "u32";
                case DefType::b32: return "b32";
                case DefType::f32: return "f32";
                case DefType::angle32: return "angle32";
                case DefType::f64: return "f64";
                case DefType::dummy8: return "dummy8";
                case DefType::fixstr: return "fixstr";
                case DefType::fixstrW: return "fixstrW";
            }
            throw BinaryException("Unknown PARAMDEF type");
        }

        DefType ParseDefType(const std::string& Name) {
            for (int I = static_cast<int>(DefType::s8); I <= static_cast<int>(DefType::fixstrW); ++I) {
                if (Name == DefTypeName(static_cast<DefType>(I))) {
                    return static_cast<DefType>(I);
                }
            }
            throw BinaryException("Unknown PARAMDEF field type \"" + Name + "\"");
        }

        int32_t GetValueSize(DefType Type) {
            switch (Type) {
                case DefType::s8:
                case DefType::u8:
                case DefType::dummy8:
                case DefType::fixstr: return 1;
                case DefType::s16:
                case DefType::u16:
                case DefType::fixstrW: return 2;
                case DefType::s32:
                case DefType::u32:
                case DefType::b32:
                case DefType::f32:
                case DefType::angle32: return 4;
                case DefType::f64: return 8;
            }
            throw BinaryException("Unknown PARAMDEF type");
        }

        int32_t GetBitLimit(DefType Type) {
            switch (Type) {
                case DefType::s8:
                case DefType::u8:
                case DefType::dummy8: return 8;
                case DefType::s16:
                case DefType::u16: return 16;
                case DefType::s32:
                case DefType::u32: return 32;
                default: break;
            }
            throw BinaryException(std::string("Type ") + DefTypeName(Type) + " cannot be a bitfield");
        }

        std::string GetDefaultFormat(DefType Type) {
            switch (Type) {
                case DefType::f32:
                case DefType::angle32:
                case DefType::f64: return "%f";
                case DefType::dummy8: return "";
                default: return "%d";
            }
        }

        EditFlags GetDefaultEditFlags(DefType Type) {
            return Type == DefType::dummy8 ? EditFlags::None : EditFlags::Wrap;
        }

        namespace {
            bool Variable(const PARAMDEF* Def) {
                return Def && Def->VariableEditorValueTypes();
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
        }  // namespace

        EditorValue GetDefaultDefault(const PARAMDEF* Def, DefType Type) {
            if (Variable(Def)) {
                if (IsIntegerLike(Type)) return int32_t{0};
                if (Type == DefType::f32 || Type == DefType::angle32) return 0.f;
                if (Type == DefType::f64) return 0.0;
                return std::monostate{};
            }
            return 0.f;
        }

        EditorValue GetDefaultMinimum(const PARAMDEF* Def, DefType Type) {
            if (Variable(Def)) {
                switch (Type) {
                    case DefType::s8: return int32_t{-128};
                    case DefType::u8: return int32_t{0};
                    case DefType::s16: return int32_t{-32768};
                    case DefType::u16: return int32_t{0};
                    case DefType::s32: return INT32_MIN;
                    case DefType::u32: return int32_t{0};
                    case DefType::b32: return int32_t{0};
                    case DefType::f32:
                    case DefType::angle32: return -FLT_MAX;
                    case DefType::f64: return -DBL_MAX;
                    default: return std::monostate{};
                }
            }
            switch (Type) {
                case DefType::s8: return -128.f;
                case DefType::s16: return -32768.f;
                case DefType::s32: return -2147483520.f;  // smallest float greater than int.MinValue
                case DefType::f32:
                case DefType::angle32:
                case DefType::f64: return -FLT_MAX;
                case DefType::fixstr:
                case DefType::fixstrW: return -1.f;
                default: return 0.f;
            }
        }

        EditorValue GetDefaultMaximum(const PARAMDEF* Def, DefType Type) {
            if (Variable(Def)) {
                switch (Type) {
                    case DefType::s8: return int32_t{127};
                    case DefType::u8: return int32_t{255};
                    case DefType::s16: return int32_t{32767};
                    case DefType::u16: return int32_t{65535};
                    case DefType::s32: return INT32_MAX;
                    case DefType::u32: return INT32_MAX;  // yes, u32 uses a signed int too (usually)
                    case DefType::b32: return int32_t{1};
                    case DefType::f32:
                    case DefType::angle32: return FLT_MAX;
                    case DefType::f64: return DBL_MAX;
                    default: return std::monostate{};
                }
            }
            switch (Type) {
                case DefType::s8: return 127.f;
                case DefType::u8: return 255.f;
                case DefType::s16: return 32767.f;
                case DefType::u16: return 65535.f;
                case DefType::s32: return 2147483520.f;  // largest float less than int.MaxValue
                case DefType::u32: return 4294967040.f;  // largest float less than uint.MaxValue
                case DefType::b32: return 1.f;
                case DefType::f32:
                case DefType::angle32:
                case DefType::f64: return FLT_MAX;
                case DefType::fixstr:
                case DefType::fixstrW: return 1000000000.f;
                default: return 0.f;
            }
        }

        EditorValue GetDefaultIncrement(const PARAMDEF* Def, DefType Type) {
            if (Variable(Def)) {
                if (IsIntegerLike(Type)) return int32_t{1};
                if (Type == DefType::f32 || Type == DefType::angle32) return 0.01f;
                if (Type == DefType::f64) return 0.01;
                return std::monostate{};
            }
            switch (Type) {
                case DefType::f32:
                case DefType::angle32:
                case DefType::f64: return 0.01f;
                case DefType::dummy8: return 0.f;
                default: return 1.f;
            }
        }
    }  // namespace ParamUtil
#pragma endregion

    namespace {
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

        std::string Trim(const std::string& Text) {
            const char* Whitespace = " \t\n\r\f\v";
            const size_t Start     = Text.find_first_not_of(Whitespace);
            if (Start == std::string::npos) return {};
            return Text.substr(Start, Text.find_last_not_of(Whitespace) - Start + 1);
        }

        bool UsesOffsetStrings(int16_t FormatVersion) {
            return FormatVersion >= 202 || (FormatVersion >= 106 && FormatVersion < 200);
        }

        std::string Key(const char* Name, size_t Index) {
            return Name + std::to_string(Index);
        }

        // Reads a null-terminated string at an absolute position.
        std::string ReadUTF16At(BinaryReader& Reader, int64_t Offset) {
            Reader.StepIn(Offset);
            std::string Value = Text::UTF16ToUTF8(Reader.ReadUTF16());
            Reader.StepOut();
            return Value;
        }

        std::string ReadASCIIAt(BinaryReader& Reader, int64_t Offset) {
            Reader.StepIn(Offset);
            std::string Value = Reader.ReadCString();
            Reader.StepOut();
            return Value;
        }

        std::string ReadShiftJISAt(BinaryReader& Reader, int64_t Offset) {
            Reader.StepIn(Offset);
            std::string Value = Reader.ReadShiftJIS();
            Reader.StepOut();
            return Value;
        }

        EditorValue ReadVariableValue(BinaryReader& Reader, DefType Type) {
            switch (Type) {
                case DefType::s8:
                case DefType::u8:
                case DefType::s16:
                case DefType::u16:
                case DefType::s32:
                case DefType::u32:
                case DefType::b32: {
                    const int32_t Value = Reader.ReadInt32();
                    Reader.Assert<int32_t>(0);
                    return Value;
                }
                case DefType::f32:
                case DefType::angle32: {
                    const float Value = Reader.ReadFloat();
                    Reader.Assert<int32_t>(0);
                    return Value;
                }
                case DefType::f64: return Reader.ReadDouble();
                // With 8 bytes available these could be offsets; they're always zero so far.
                case DefType::dummy8:
                case DefType::fixstr:
                case DefType::fixstrW: Reader.Assert<int64_t>(0); return std::monostate{};
            }
            throw BinaryException("Missing variable read for type");
        }

        void WriteVariableValue(BinaryWriter& Writer, DefType Type, const EditorValue& Value) {
            switch (Type) {
                case DefType::s8:
                case DefType::u8:
                case DefType::s16:
                case DefType::u16:
                case DefType::s32:
                case DefType::u32:
                case DefType::b32:
                    Writer.WriteInt32(AsInt32(Value));
                    Writer.WriteInt32(0);
                    break;
                case DefType::f32:
                case DefType::angle32:
                    Writer.WriteFloat(AsFloat(Value));
                    Writer.WriteInt32(0);
                    break;
                case DefType::f64: Writer.WriteDouble(AsDouble(Value)); break;
                case DefType::dummy8:
                case DefType::fixstr:
                case DefType::fixstrW: Writer.WriteInt64(0); break;
            }
        }

        // The internal name as stored in the file, with ":bits" or "[length]" appended where they apply.
        std::string MakeInternalName(const PARAMDEF::Field& Field) {
            if (Field.BitSize != -1) return Field.InternalName + ":" + std::to_string(Field.BitSize);
            if (ParamUtil::IsArrayType(Field.DisplayType)) return Field.InternalName + "[" + std::to_string(Field.ArrayLength) + "]";
            return Field.InternalName;
        }

        PARAMDEF::Field ReadField(BinaryReader& Reader, const PARAMDEF& Def) {
            PARAMDEF::Field Result;
            const bool Offsets = UsesOffsetStrings(Def.FormatVersion);

            if (Offsets) {
                Result.DisplayName = ReadUTF16At(Reader, Reader.ReadVarint());
            } else if (Def.Unicode) {
                Result.DisplayName = Text::UTF16ToUTF8(Reader.ReadFixStrW(0x40));
            } else {
                Result.DisplayName = Reader.ReadFixStr(0x40);
            }

            Result.DisplayType   = ParamUtil::ParseDefType(Reader.ReadFixStr(8));
            Result.DisplayFormat = Reader.ReadFixStr(8);

            if (Def.FormatVersion >= 203) {
                Reader.AssertPattern(0x10, 0x00);
            } else {
                Result.Default   = Reader.ReadFloat();
                Result.Minimum   = Reader.ReadFloat();
                Result.Maximum   = Reader.ReadFloat();
                Result.Increment = Reader.ReadFloat();
            }

            Result.EditorFlags = static_cast<EditFlags>(Reader.ReadInt32());

            const int32_t ByteCount = Reader.ReadInt32();
            const int32_t ValueSize = ParamUtil::GetValueSize(Result.DisplayType);
            const bool IsArray      = ParamUtil::IsArrayType(Result.DisplayType);
            if ((!IsArray && ByteCount != ValueSize) || (IsArray && (ByteCount < 0 || ByteCount % ValueSize != 0))) {
                throw BinaryException("Unexpected byte count " + std::to_string(ByteCount) + " for type " +
                                      ParamUtil::DefTypeName(Result.DisplayType));
            }
            Result.ArrayLength = ByteCount / ValueSize;

            const int64_t DescriptionOffset = Reader.ReadVarint();
            if (DescriptionOffset != 0) {
                Result.Description = Def.Unicode ? ReadUTF16At(Reader, DescriptionOffset) : ReadShiftJISAt(Reader, DescriptionOffset);
            }

            Result.InternalType = Offsets ? Trim(ReadASCIIAt(Reader, Reader.ReadVarint())) : Trim(Reader.ReadFixStr(0x20));

            Result.BitSize = -1;
            if (Def.FormatVersion >= 102) {
                Result.InternalName = Offsets ? Trim(ReadASCIIAt(Reader, Reader.ReadVarint())) : Trim(Reader.ReadFixStr(0x20));

                static const std::regex BitSizeRx(R"(^\s*(.+?)\s*:\s*(\d+)\s*$)");
                static const std::regex ArrayLengthRx(R"(^\s*(.+?)\s*\[\s*(\d+)\s*\]\s*$)");

                std::smatch Match;
                if (std::regex_match(Result.InternalName, Match, BitSizeRx)) {
                    const std::string Name = Match[1].str();
                    Result.BitSize         = std::stoi(Match[2].str());
                    Result.InternalName    = Name;
                }

                if (IsArray) {
                    int32_t Length = 1;
                    const bool Found = std::regex_match(Result.InternalName, Match, ArrayLengthRx);
                    if (Found) Length = std::stoi(Match[2].str());
                    if (Length != Result.ArrayLength) {
                        throw BinaryException("Mismatched array length in " + Result.InternalName + " with byte count " +
                                              std::to_string(ByteCount));
                    }
                    if (Found) {
                        const std::string Name = Match[1].str();
                        Result.InternalName    = Name;
                    }
                }
            }

            if (Def.FormatVersion >= 104) {
                Result.SortID = Reader.ReadInt32();
            }

            if (Def.FormatVersion >= 200) {
                Reader.Assert<int32_t>(0);
                const int64_t UnkB8Offset = Reader.ReadInt64();
                const int64_t UnkC0Offset = Reader.ReadInt64();
                const int64_t UnkC8Offset = Reader.ReadInt64();

                if (UnkB8Offset != 0) Result.UnkB8 = ReadASCIIAt(Reader, UnkB8Offset);
                if (UnkC0Offset != 0) Result.UnkC0 = ReadASCIIAt(Reader, UnkC0Offset);
                if (UnkC8Offset != 0) Result.UnkC8 = ReadUTF16At(Reader, UnkC8Offset);
            } else if (Def.FormatVersion >= 106) {
                Reader.Assert<int32_t>(0);
                Reader.Assert<int32_t>(0);
                Reader.Assert<int32_t>(0);
            }

            if (Def.FormatVersion >= 203) {
                Result.Default   = ReadVariableValue(Reader, Result.DisplayType);
                Result.Minimum   = ReadVariableValue(Reader, Result.DisplayType);
                Result.Maximum   = ReadVariableValue(Reader, Result.DisplayType);
                Result.Increment = ReadVariableValue(Reader, Result.DisplayType);
            }

            return Result;
        }

        void WriteField(BinaryWriter& Writer, const PARAMDEF& Def, const PARAMDEF::Field& Field, size_t Index) {
            const bool Offsets = UsesOffsetStrings(Def.FormatVersion);

            if (Offsets) {
                Writer.ReserveVarint(Key("DisplayNameOffset", Index));
            } else if (Def.Unicode) {
                Writer.WriteFixStrW(Text::UTF8ToUTF16(Field.DisplayName), 0x40, Def.FormatVersion >= 104 ? 0x00 : 0x20);
            } else {
                Writer.WriteFixStr(Field.DisplayName, 0x40, Def.FormatVersion >= 104 ? 0x00 : 0x20);
            }

            const uint8_t Padding = Def.FormatVersion >= 106 ? 0x00 : 0x20;
            Writer.WriteFixStr(ParamUtil::DefTypeName(Field.DisplayType), 8, Padding);
            Writer.WriteFixStr(Field.DisplayFormat, 8, Padding);

            if (Def.FormatVersion >= 203) {
                Writer.Pad(0x10);
            } else {
                Writer.WriteFloat(AsFloat(Field.Default));
                Writer.WriteFloat(AsFloat(Field.Minimum));
                Writer.WriteFloat(AsFloat(Field.Maximum));
                Writer.WriteFloat(AsFloat(Field.Increment));
            }

            Writer.WriteInt32(static_cast<int32_t>(Field.EditorFlags));
            Writer.WriteInt32(ParamUtil::GetValueSize(Field.DisplayType) *
                              (ParamUtil::IsArrayType(Field.DisplayType) ? Field.ArrayLength : 1));
            Writer.ReserveVarint(Key("DescriptionOffset", Index));

            if (Offsets) {
                Writer.ReserveVarint(Key("InternalTypeOffset", Index));
            } else {
                Writer.WriteFixStr(Field.InternalType, 0x20, Padding);
            }

            if (Offsets) {
                Writer.ReserveVarint(Key("InternalNameOffset", Index));
            } else if (Def.FormatVersion >= 102) {
                Writer.WriteFixStr(MakeInternalName(Field), 0x20, Padding);
            }

            if (Def.FormatVersion >= 104) {
                Writer.WriteInt32(Field.SortID);
            }

            if (Def.FormatVersion >= 200) {
                Writer.WriteInt32(0);
                Writer.Reserve<int64_t>(Key("UnkB8Offset", Index));
                Writer.Reserve<int64_t>(Key("UnkC0Offset", Index));
                Writer.Reserve<int64_t>(Key("UnkC8Offset", Index));
            } else if (Def.FormatVersion >= 106) {
                Writer.WriteInt32(0);
                Writer.WriteInt32(0);
                Writer.WriteInt32(0);
            }

            if (Def.FormatVersion >= 203) {
                WriteVariableValue(Writer, Field.DisplayType, Field.Default);
                WriteVariableValue(Writer, Field.DisplayType, Field.Minimum);
                WriteVariableValue(Writer, Field.DisplayType, Field.Maximum);
                WriteVariableValue(Writer, Field.DisplayType, Field.Increment);
            }
        }

        void WriteFieldStrings(BinaryWriter& Writer,
                               const PARAMDEF& Def,
                               const PARAMDEF::Field& Field,
                               size_t Index,
                               std::map<std::string, int64_t>& SharedStringOffsets) {
            if (UsesOffsetStrings(Def.FormatVersion)) {
                Writer.FillVarint(Key("DisplayNameOffset", Index), Writer.Position());
                Writer.WriteUTF16(Text::UTF8ToUTF16(Field.DisplayName), true);
            }

            int64_t DescriptionOffset = 0;
            if (Field.Description) {
                DescriptionOffset = Writer.Position();
                if (Def.Unicode) {
                    Writer.WriteUTF16(Text::UTF8ToUTF16(*Field.Description), true);
                } else {
                    Writer.WriteShiftJIS(*Field.Description, true);
                }
            }
            Writer.FillVarint(Key("DescriptionOffset", Index), DescriptionOffset);

            if (UsesOffsetStrings(Def.FormatVersion)) {
                Writer.FillVarint(Key("InternalTypeOffset", Index), Writer.Position());
                Writer.WriteString(Field.InternalType, true);

                Writer.FillVarint(Key("InternalNameOffset", Index), Writer.Position());
                Writer.WriteString(MakeInternalName(Field), true);
            }

            if (Def.FormatVersion >= 200) {
                // Strings are shared between fields. ASCII and UTF-16 copies are distinct entries.
                auto WriteSharedMaybe = [&](const std::optional<std::string>& Value, bool Unicode) -> int64_t {
                    if (!Value) return 0;
                    const std::string PoolKey = (Unicode ? "W:" : "A:") + *Value;
                    const auto Existing       = SharedStringOffsets.find(PoolKey);
                    if (Existing != SharedStringOffsets.end()) return Existing->second;

                    const int64_t Offset = Writer.Position();
                    SharedStringOffsets.emplace(PoolKey, Offset);
                    if (Unicode) {
                        Writer.WriteUTF16(Text::UTF8ToUTF16(*Value), true);
                    } else {
                        Writer.WriteString(*Value, true);
                    }
                    return Offset;
                };

                Writer.Fill<int64_t>(Key("UnkB8Offset", Index), WriteSharedMaybe(Field.UnkB8, false));
                Writer.Fill<int64_t>(Key("UnkC0Offset", Index), WriteSharedMaybe(Field.UnkC0, false));
                Writer.Fill<int64_t>(Key("UnkC8Offset", Index), WriteSharedMaybe(Field.UnkC8, true));
            }
        }
    }  // namespace

    PARAMDEF::Field::Field() : Field(nullptr, DefType::f32, "placeholder") {}

    PARAMDEF::Field::Field(const PARAMDEF* Def, DefType Type, std::string Name)
        : DisplayName(Name), DisplayType(Type), InternalName(std::move(Name)) {
        DisplayFormat = ParamUtil::GetDefaultFormat(Type);
        Default       = ParamUtil::GetDefaultDefault(Def, Type);
        Minimum       = ParamUtil::GetDefaultMinimum(Def, Type);
        Maximum       = ParamUtil::GetDefaultMaximum(Def, Type);
        Increment     = ParamUtil::GetDefaultIncrement(Def, Type);
        EditorFlags   = ParamUtil::GetDefaultEditFlags(Type);
        ArrayLength   = 1;
        InternalType  = ParamUtil::DefTypeName(Type);
        BitSize       = -1;
    }

    std::string PARAMDEF::Field::ToString() const {
        if (ParamUtil::IsBitType(DisplayType) && BitSize != -1) {
            return std::string(ParamUtil::DefTypeName(DisplayType)) + " " + InternalName + ":" + std::to_string(BitSize);
        }
        if (ParamUtil::IsArrayType(DisplayType)) {
            return std::string(ParamUtil::DefTypeName(DisplayType)) + " " + InternalName + "[" + std::to_string(ArrayLength) + "]";
        }
        return std::string(ParamUtil::DefTypeName(DisplayType)) + " " + InternalName;
    }

    int32_t PARAMDEF::GetRowSize() const {
        return GetFieldsSize(Fields.size());
    }

    int32_t PARAMDEF::GetFieldsSize(size_t FieldCount) const {
        if (FieldCount > Fields.size()) {
            throw BinaryException("Count must be from 0 to the total field count");
        }

        int32_t Size = 0;
        for (size_t I = 0; I < FieldCount; ++I) {
            const Field& Current = Fields[I];
            const DefType Type   = Current.DisplayType;
            if (ParamUtil::IsArrayType(Type)) {
                Size += ParamUtil::GetValueSize(Type) * Current.ArrayLength;
            } else {
                Size += ParamUtil::GetValueSize(Type);
            }

            // Consecutive bitfields that fit in the same integer share its storage.
            if (ParamUtil::IsBitType(Type) && Current.BitSize != -1) {
                int32_t BitOffset      = Current.BitSize;
                const int32_t BitLimit = ParamUtil::GetBitLimit(Type);

                for (; I + 1 < FieldCount; ++I) {
                    const Field& Next = Fields[I + 1];
                    if (!ParamUtil::IsBitType(Next.DisplayType) || Next.BitSize == -1 ||
                        ParamUtil::GetBitLimit(Next.DisplayType) != BitLimit || BitOffset + Next.BitSize > BitLimit) {
                        break;
                    }
                    BitOffset += Next.BitSize;
                }
            }
        }
        return Size;
    }

    std::string PARAMDEF::ToString() const {
        return ParamType + " v" + std::to_string(DataVersion);
    }

    namespace {
        bool KnownFormatVersion(int16_t Version) {
            return Version == 101 || Version == 102 || Version == 103 || Version == 104 || Version == 106 ||
                   Version == 201 || Version == 202 || Version == 203;
        }

        int16_t ExpectedFieldSize(int16_t Version) {
            switch (Version) {
                case 101: return 0x8C;
                case 102: return 0xAC;
                case 103: return 0x6C;
                case 104: return 0xB0;
                case 106: return 0x48;
                case 201: return 0xD0;
                case 202: return 0x68;
                case 203: return 0x88;
                default: return 0;
            }
        }
    }  // namespace

    bool PARAMDEF::Validate(std::exception_ptr& Error) {
        if (!KnownFormatVersion(FormatVersion)) {
            Error = std::make_exception_ptr(BinaryException("Unknown version: " + std::to_string(FormatVersion)));
            return false;
        }
        for (size_t I = 0; I < Fields.size(); ++I) {
            const Field& Current = Fields[I];
            // (Zero-length arrays occur in real defs, so 0 is allowed for array types.)
            if (Current.ArrayLength < 0 || (!ParamUtil::IsArrayType(Current.DisplayType) && Current.ArrayLength != 1)) {
                Error = std::make_exception_ptr(
                  BinaryException("Fields[" + std::to_string(I) + "]: invalid array length " + std::to_string(Current.ArrayLength)));
                return false;
            }
        }
        Error = nullptr;
        return true;
    }

    bool PARAMDEF::IsImpl(BinaryReader& Reader) {
        if (Reader.Length() < 0x30) {
            return false;
        }

        const uint8_t Big = Reader.ReadAt<uint8_t>(0x2C);
        if (Big != 0 && Big != 0xFF) {
            return false;
        }
        Reader.Order = Big == 0xFF ? Endian::Big : Endian::Little;

        const int16_t Version = Reader.ReadAt<int16_t>(0x2E);
        if (!KnownFormatVersion(Version)) {
            return false;
        }
        const int16_t HeaderSize = Reader.ReadAt<int16_t>(0x04);
        return Version < 200 ? HeaderSize == 0x30 : HeaderSize == 0xFF;
    }

    void PARAMDEF::ReadImpl(BinaryReader& Reader) {
        BigEndian      = Reader.ReadAt<uint8_t>(0x2C) == 0xFF;
        Reader.Order   = BigEndian ? Endian::Big : Endian::Little;
        FormatVersion  = Reader.ReadAt<int16_t>(0x2E);
        Reader.VarintLong = FormatVersion >= 200;

        Reader.ReadInt32();  // file size
        const int16_t HeaderSize = Reader.Assert<int16_t>(0x30, 0xFF);
        DataVersion              = Reader.ReadInt16();
        const int16_t FieldCount = Reader.ReadInt16();
        const int16_t FieldSize  = Reader.Assert<int16_t>(0x48, 0x68, 0x6C, 0x88, 0x8C, 0xAC, 0xB0, 0xD0);

        if (FormatVersion >= 202) {
            Reader.Assert<int32_t>(0);
            ParamType = ReadShiftJISAt(Reader, Reader.ReadInt64());
            Reader.Assert<int64_t>(0);
            Reader.Assert<int64_t>(0);
            Reader.Assert<int32_t>(0);
        } else if (FormatVersion >= 106 && FormatVersion < 200) {
            ParamType = ReadShiftJISAt(Reader, Reader.ReadInt32());
            Reader.Assert<int64_t>(0);
            Reader.Assert<int64_t>(0);
            Reader.Assert<int64_t>(0);
            Reader.Assert<int32_t>(0);
        } else {
            ParamType = Reader.ReadFixStr(0x20);
        }

        Reader.Assert<int8_t>(0, -1);  // big-endian
        Unicode = Reader.ReadBool();
        Reader.Assert<int16_t>(101, 102, 103, 104, 106, 201, 202, 203);  // format version
        if (FormatVersion >= 200) {
            Reader.Assert<int64_t>(0x38);
        }

        if (!((FormatVersion < 200 && HeaderSize == 0x30) || (FormatVersion >= 200 && HeaderSize == 0xFF))) {
            throw BinaryException("Unexpected header size " + std::to_string(HeaderSize) + " for version " +
                                  std::to_string(FormatVersion));
        }

        // (For version 103 the value in the file is wrong, but it's the one the files use.)
        if (FieldSize != ExpectedFieldSize(FormatVersion)) {
            throw BinaryException("Unexpected field size " + std::to_string(FieldSize) + " for version " +
                                  std::to_string(FormatVersion));
        }

        if (FieldCount < 0 || static_cast<int64_t>(FieldCount) * FieldSize > Reader.Remaining() + FieldSize) {
            throw BinaryException("Invalid PARAMDEF field count " + std::to_string(FieldCount));
        }

        Fields.clear();
        Fields.reserve(static_cast<size_t>(FieldCount));
        for (int16_t I = 0; I < FieldCount; ++I) {
            Fields.push_back(ReadField(Reader, *this));
        }
    }

    void PARAMDEF::WriteImpl(BinaryWriter& Writer) {
        Writer.Order      = BigEndian ? Endian::Big : Endian::Little;
        Writer.VarintLong = FormatVersion >= 200;

        Writer.Reserve<int32_t>("FileSize");
        Writer.WriteInt16(FormatVersion >= 200 ? 0xFF : 0x30);
        Writer.WriteInt16(DataVersion);
        Writer.WriteInt16(static_cast<int16_t>(Fields.size()));
        Writer.WriteInt16(ExpectedFieldSize(FormatVersion));

        if (FormatVersion >= 202) {
            Writer.WriteInt32(0);
            Writer.ReserveVarint("ParamTypeOffset");
            Writer.WriteInt64(0);
            Writer.WriteInt64(0);
            Writer.WriteInt32(0);
        } else if (FormatVersion >= 106 && FormatVersion < 200) {
            Writer.ReserveVarint("ParamTypeOffset");
            Writer.WriteInt64(0);
            Writer.WriteInt64(0);
            Writer.WriteInt64(0);
            Writer.WriteInt32(0);
        } else {
            Writer.WriteFixStr(ParamType, 0x20, FormatVersion >= 200 ? 0x00 : 0x20);
        }

        Writer.WriteSByte(BigEndian ? -1 : 0);
        Writer.WriteBool(Unicode);
        Writer.WriteInt16(FormatVersion);
        if (FormatVersion >= 200) {
            Writer.WriteInt64(0x38);
        }

        for (size_t I = 0; I < Fields.size(); ++I) {
            WriteField(Writer, *this, Fields[I], I);
        }

        if (UsesOffsetStrings(FormatVersion)) {
            Writer.FillVarint("ParamTypeOffset", Writer.Position());
            Writer.WriteShiftJIS(ParamType, true);
        }

        const int64_t FieldStringsStart = Writer.Position();
        std::map<std::string, int64_t> SharedStringOffsets;
        for (size_t I = 0; I < Fields.size(); ++I) {
            WriteFieldStrings(Writer, *this, Fields[I], I, SharedStringOffsets);
        }

        // This entire heuristic seems extremely dubious, but it's what the files do.
        if (FormatVersion == 104 || FormatVersion == 201) {
            const int64_t FieldStringsLength = Writer.Position() - FieldStringsStart;
            if (FieldStringsLength % 0x10 != 0) {
                Writer.Pad(static_cast<size_t>(0x10 - FieldStringsLength % 0x10));
            }
        } else {
            if (FormatVersion >= 202 && Writer.Position() % 0x10 == 0) {
                Writer.Pad(0x10);
            }
            Writer.Align(0x10);
        }
        Writer.Fill<int32_t>("FileSize", static_cast<int32_t>(Writer.Position()));
    }
}  // namespace Souls
