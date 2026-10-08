//
// Created by Jake Rieger on 10/7/2026.
//

#include "ParamLayout.hpp"

#include <libSouls/TextEncoding.hpp>

#include <algorithm>
#include <cstring>

namespace Souls {
    using DefType = PARAMDEF::DefType;

    namespace {
        template<typename T>
        T Load(const uint8_t* Source, bool Big) {
            T Value;
            std::memcpy(&Value, Source, sizeof(T));
            return ToHost(Value, Big ? Endian::Big : Endian::Little);
        }

        template<typename T>
        void Store(uint8_t* Destination, T Value, bool Big) {
            const T Converted = ToHost(Value, Big ? Endian::Big : Endian::Little);
            std::memcpy(Destination, &Converted, sizeof(T));
        }

        // An unsigned integer of 1, 2 or 4 bytes.
        uint64_t LoadUnsigned(const uint8_t* Source, size_t Length, bool Big) {
            switch (Length) {
                case 1: return Source[0];
                case 2: return Load<uint16_t>(Source, Big);
                case 4: return Load<uint32_t>(Source, Big);
            }
            throw BinaryException("Unexpected bitfield storage size");
        }

        void StoreUnsigned(uint8_t* Destination, size_t Length, uint64_t Value, bool Big) {
            switch (Length) {
                case 1: Destination[0] = static_cast<uint8_t>(Value); return;
                case 2: Store<uint16_t>(Destination, static_cast<uint16_t>(Value), Big); return;
                case 4: Store<uint32_t>(Destination, static_cast<uint32_t>(Value), Big); return;
            }
            throw BinaryException("Unexpected bitfield storage size");
        }

        // Numeric content of a variant alternative, as a double / integer. Empty if it isn't numeric.
        std::optional<double> NumberOf(const CellValue& Value) {
            return std::visit(
              [](const auto& V) -> std::optional<double> {
                  using T = std::decay_t<decltype(V)>;
                  if constexpr (std::is_arithmetic_v<T>) {
                      return static_cast<double>(V);
                  } else {
                      return std::nullopt;
                  }
              },
              Value);
        }

        std::optional<int64_t> IntegerOf(const CellValue& Value) {
            return std::visit(
              [](const auto& V) -> std::optional<int64_t> {
                  using T = std::decay_t<decltype(V)>;
                  if constexpr (std::is_integral_v<T>) {
                      return static_cast<int64_t>(V);
                  } else if constexpr (std::is_floating_point_v<T>) {
                      return static_cast<int64_t>(V);  // truncates
                  } else {
                      return std::nullopt;
                  }
              },
              Value);
        }

        double EditorNumber(const PARAMDEF::EditorValue& Value) {
            if (const auto* I = std::get_if<int32_t>(&Value)) return *I;
            if (const auto* F = std::get_if<float>(&Value)) return *F;
            if (const auto* D = std::get_if<double>(&Value)) return *D;
            return 0.0;
        }

        // Converts a number to the alternative the field's type uses.
        CellValue FromInteger(DefType Type, int64_t Value) {
            switch (Type) {
                case DefType::s8: return static_cast<int8_t>(Value);
                case DefType::u8:
                case DefType::dummy8: return static_cast<uint8_t>(Value);
                case DefType::s16: return static_cast<int16_t>(Value);
                case DefType::u16: return static_cast<uint16_t>(Value);
                case DefType::s32:
                case DefType::b32: return static_cast<int32_t>(Value);
                case DefType::u32: return static_cast<uint32_t>(Value);
                default: break;
            }
            throw BinaryException("Not an integer type");
        }
    }  // namespace

    ParamLayout::ParamLayout(const PARAMDEF& Def, bool Big) : BigEndian(Big) {
        size_t Offset = 0;
        int32_t BitOffset = -1;
        int32_t BitLimit  = -1;
        size_t StorageOffset = 0;

        Fields.reserve(Def.Fields.size());
        for (const PARAMDEF::Field& Source : Def.Fields) {
            FieldInfo Info;
            Info.Name        = Source.InternalName;
            Info.Type        = Source.DisplayType;
            Info.ArrayLength = Source.ArrayLength;
            Info.BitSize     = -1;
            Info.Default     = Source.Default;
            const DefType Type = Source.DisplayType;

            bool Bitfield = false;
            switch (Type) {
                case DefType::b32:
                case DefType::f32:
                case DefType::angle32: Info.Length = 4; break;
                case DefType::f64: Info.Length = 8; break;
                case DefType::fixstr: Info.Length = static_cast<size_t>(Source.ArrayLength); break;
                case DefType::fixstrW: Info.Length = static_cast<size_t>(Source.ArrayLength) * 2; break;
                default:  // integer types and dummy8
                    if (Source.BitSize == -1) {
                        Info.Length = Type == DefType::dummy8 ? static_cast<size_t>(Source.ArrayLength)
                                                              : static_cast<size_t>(ParamUtil::GetValueSize(Type));
                    } else {
                        Bitfield = true;
                    }
                    break;
            }

            if (!Bitfield) {
                Info.Offset = Offset;
                Offset += Info.Length;
                BitOffset = -1;
            } else {
                if (Source.BitSize == 0) {
                    throw BinaryException("Bit size 0 is not supported (field " + Source.InternalName + ")");
                }
                const int32_t Limit = ParamUtil::GetBitLimit(Type);
                if (Source.BitSize > Limit) {
                    throw BinaryException("Bit size " + std::to_string(Source.BitSize) + " is too large to fit in type " +
                                          ParamUtil::DefTypeName(Type) + " (field " + Source.InternalName + ")");
                }
                // A new storage integer starts when there's none open, or the type changes, or the bits run out.
                if (BitOffset == -1 || Limit != BitLimit || BitOffset + Source.BitSize > BitLimit) {
                    BitOffset     = 0;
                    BitLimit      = Limit;
                    StorageOffset = Offset;
                    Offset += static_cast<size_t>(Limit / 8);
                }
                Info.Offset    = StorageOffset;
                Info.Length    = static_cast<size_t>(Limit / 8);
                Info.BitOffset = BitOffset;
                Info.BitSize   = Source.BitSize;
                BitOffset += Source.BitSize;
            }

            ByName.emplace(Info.Name, Fields.size());  // the first field with a name wins
            Fields.push_back(std::move(Info));
        }
        Size = Offset;

        if (static_cast<int64_t>(Size) != Def.GetRowSize()) {
            throw BinaryException("Internal error: layout size " + std::to_string(Size) + " disagrees with the def's row size " +
                                  std::to_string(Def.GetRowSize()));
        }
    }

    std::optional<ParamLayout> ParamLayout::TryCreate(const PARAM& Param, const PARAMDEF& Def) {
        if (!Param.Matches(Def)) {
            return std::nullopt;
        }
        return ParamLayout(Def, Param.BigEndian);
    }

    std::optional<size_t> ParamLayout::IndexOf(std::string_view InternalName) const {
        const auto It = ByName.find(InternalName);
        if (It == ByName.end()) return std::nullopt;
        return It->second;
    }

    const std::string& ParamLayout::InternalName(size_t FieldIndex) const {
        return Field(FieldIndex).Name;
    }

    PARAMDEF::DefType ParamLayout::Type(size_t FieldIndex) const {
        return Field(FieldIndex).Type;
    }

    const ParamLayout::FieldInfo& ParamLayout::Field(size_t FieldIndex) const {
        if (FieldIndex >= Fields.size()) {
            throw BinaryException("Field index " + std::to_string(FieldIndex) + " is out of range");
        }
        return Fields[FieldIndex];
    }

    size_t ParamLayout::Require(std::string_view InternalName) const {
        const auto Index = IndexOf(InternalName);
        if (!Index) {
            throw BinaryException("No field named \"" + std::string(InternalName) + "\"");
        }
        return *Index;
    }

    void ParamLayout::ThrowTypeMismatch(std::string_view InternalName) {
        throw BinaryException("Field \"" + std::string(InternalName) + "\" does not have the requested type");
    }

    PARAM::Row ParamLayout::MakeRow(int32_t ID, std::optional<std::string> Name) const {
        PARAM::Row Result;
        Result.ID = ID;
        Result.Name = std::move(Name);
        Result.Bytes.assign(Size, 0);

        for (size_t I = 0; I < Fields.size(); ++I) {
            const FieldInfo& Info = Fields[I];
            switch (Info.Type) {
                case DefType::fixstr:
                case DefType::fixstrW:
                case DefType::dummy8:
                    if (Info.BitSize == -1) continue;  // zero bytes are the default
                    break;
                default: break;
            }
            const double Number = EditorNumber(Info.Default);
            if (Info.Type == DefType::f32 || Info.Type == DefType::angle32) {
                Set(Result, I, static_cast<float>(Number));
            } else if (Info.Type == DefType::f64) {
                Set(Result, I, Number);
            } else {
                Set(Result, I, FromInteger(Info.Type, static_cast<int64_t>(Number)));
            }
        }
        return Result;
    }

    CellValue ParamLayout::Get(const PARAM::Row& Row, size_t FieldIndex) const {
        const FieldInfo& Info = Field(FieldIndex);
        if (Row.Bytes.size() != Size) {
            throw BinaryException("Row " + std::to_string(Row.ID) + " has " + std::to_string(Row.Bytes.size()) +
                                  " bytes, but the layout needs " + std::to_string(Size));
        }
        const uint8_t* Source = Row.Bytes.data() + Info.Offset;

        if (Info.BitSize != -1) {
            const uint64_t Storage = LoadUnsigned(Source, Info.Length, BigEndian);
            const int32_t Left     = 64 - Info.BitSize - Info.BitOffset;
            const int32_t Right    = 64 - Info.BitSize;
            int64_t Value;
            if (ParamUtil::IsSignedBitType(Info.Type)) {
                Value = static_cast<int64_t>(Storage << Left) >> Right;  // arithmetic shift sign-extends
            } else {
                Value = static_cast<int64_t>(Storage << Left >> Right);
            }
            return FromInteger(Info.Type, Value);
        }

        switch (Info.Type) {
            case DefType::s8: return static_cast<int8_t>(Source[0]);
            case DefType::u8: return Source[0];
            case DefType::s16: return Load<int16_t>(Source, BigEndian);
            case DefType::u16: return Load<uint16_t>(Source, BigEndian);
            case DefType::s32:
            case DefType::b32: return Load<int32_t>(Source, BigEndian);
            case DefType::u32: return Load<uint32_t>(Source, BigEndian);
            case DefType::f32:
            case DefType::angle32: return Load<float>(Source, BigEndian);
            case DefType::f64: return Load<double>(Source, BigEndian);
            case DefType::dummy8: return std::vector<uint8_t>(Source, Source + Info.Length);
            case DefType::fixstr: {
                size_t Length = 0;
                while (Length < Info.Length && Source[Length] != 0) ++Length;
                return Text::ShiftJISToUTF8(std::string_view(reinterpret_cast<const char*>(Source), Length));
            }
            case DefType::fixstrW: {
                std::u16string Units;
                for (size_t I = 0; I < Info.Length / 2; ++I) {
                    const char16_t Unit = Load<char16_t>(Source + I * 2, BigEndian);
                    if (Unit == 0) break;
                    Units.push_back(Unit);
                }
                return Text::UTF16ToUTF8(Units);
            }
        }
        throw BinaryException("Unsupported field type");
    }

    CellValue ParamLayout::Get(const PARAM::Row& Row, std::string_view InternalName) const {
        return Get(Row, Require(InternalName));
    }

    void ParamLayout::Set(PARAM::Row& Row, size_t FieldIndex, const CellValue& Value) const {
        const FieldInfo& Info = Field(FieldIndex);
        if (Row.Bytes.size() != Size) {
            throw BinaryException("Row " + std::to_string(Row.ID) + " has " + std::to_string(Row.Bytes.size()) +
                                  " bytes, but the layout needs " + std::to_string(Size));
        }
        uint8_t* Destination = Row.Bytes.data() + Info.Offset;
        auto Mismatch = [&]() -> void {
            throw BinaryException("Cannot store that value in field \"" + Info.Name + "\" (" +
                                  ParamUtil::DefTypeName(Info.Type) + ")");
        };

        if (Info.BitSize != -1) {
            const auto Integer = IntegerOf(Value);
            if (!Integer) Mismatch();
            const uint64_t Mask    = ((uint64_t{1} << Info.BitSize) - 1) << Info.BitOffset;
            const uint64_t Shifted = (static_cast<uint64_t>(*Integer) << Info.BitOffset) & Mask;  // out-of-range bits drop
            const uint64_t Storage = LoadUnsigned(Destination, Info.Length, BigEndian);
            StoreUnsigned(Destination, Info.Length, (Storage & ~Mask) | Shifted, BigEndian);
            return;
        }

        switch (Info.Type) {
            case DefType::s8:
            case DefType::u8:
            case DefType::s16:
            case DefType::u16:
            case DefType::s32:
            case DefType::b32:
            case DefType::u32: {
                const auto Integer = IntegerOf(Value);
                if (!Integer) Mismatch();
                const CellValue Typed = FromInteger(Info.Type, *Integer);
                std::visit(
                  [&](const auto& V) {
                      using T = std::decay_t<decltype(V)>;
                      if constexpr (std::is_integral_v<T>) {
                          if constexpr (sizeof(T) == 1) {
                              Destination[0] = static_cast<uint8_t>(V);
                          } else {
                              Store<T>(Destination, V, BigEndian);
                          }
                      }
                  },
                  Typed);
                return;
            }
            case DefType::f32:
            case DefType::angle32: {
                const auto Number = NumberOf(Value);
                if (!Number) Mismatch();
                Store<float>(Destination, static_cast<float>(*Number), BigEndian);
                return;
            }
            case DefType::f64: {
                const auto Number = NumberOf(Value);
                if (!Number) Mismatch();
                Store<double>(Destination, *Number, BigEndian);
                return;
            }
            case DefType::dummy8: {
                const auto* Bytes = std::get_if<std::vector<uint8_t>>(&Value);
                if (!Bytes || Bytes->size() != Info.Length) Mismatch();
                std::copy(Bytes->begin(), Bytes->end(), Destination);
                return;
            }
            case DefType::fixstr: {
                const auto* Text = std::get_if<std::string>(&Value);
                if (!Text) Mismatch();
                const std::string Encoded = Text::UTF8ToShiftJIS(*Text);
                std::memset(Destination, 0, Info.Length);
                std::memcpy(Destination, Encoded.data(), std::min(Encoded.size(), Info.Length));  // terminator if it fits
                return;
            }
            case DefType::fixstrW: {
                const auto* Text = std::get_if<std::string>(&Value);
                if (!Text) Mismatch();
                const std::u16string Units = Text::UTF8ToUTF16(*Text);
                std::memset(Destination, 0, Info.Length);
                const size_t Capacity = Info.Length / 2;
                for (size_t I = 0; I < Units.size() && I < Capacity; ++I) {
                    Store<char16_t>(Destination + I * 2, Units[I], BigEndian);
                }
                return;
            }
        }
        Mismatch();
    }

    void ParamLayout::Set(PARAM::Row& Row, std::string_view InternalName, const CellValue& Value) const {
        Set(Row, Require(InternalName), Value);
    }

    double ParamLayout::GetNumber(const PARAM::Row& Row, std::string_view InternalName) const {
        const CellValue Value = Get(Row, InternalName);
        const auto Number     = NumberOf(Value);
        if (!Number) {
            throw BinaryException("Field \"" + std::string(InternalName) + "\" is not numeric");
        }
        return *Number;
    }

    void ParamLayout::SetNumber(PARAM::Row& Row, std::string_view InternalName, double Value) const {
        const size_t Index = Require(InternalName);
        switch (Fields[Index].Type) {
            case DefType::f32:
            case DefType::angle32: Set(Row, Index, static_cast<float>(Value)); return;
            case DefType::f64: Set(Row, Index, Value); return;
            default: Set(Row, Index, FromInteger(Fields[Index].Type, static_cast<int64_t>(Value))); return;
        }
    }
}  // namespace Souls
