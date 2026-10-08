//
// Created by Jake Rieger on 10/7/2026.
//

#pragma once

#include "PARAM.hpp"
#include "PARAMDEF.hpp"

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of an exported class; only touched by this DLL's own code

    // The value of one field of a row. Which alternative a field uses depends on its type:
    //   s8 -> int8_t, u8 -> uint8_t, s16 -> int16_t, u16 -> uint16_t, s32 and b32 -> int32_t, u32 -> uint32_t,
    //   f32 and angle32 -> float, f64 -> double, fixstr and fixstrW -> std::string (UTF-8),
    //   dummy8 -> std::vector<uint8_t> (or uint8_t for a bitfield).
    using CellValue =
      std::variant<int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, float, double, std::string, std::vector<uint8_t>>;

    // Where each field of a PARAMDEF lives within a row's bytes, and how to read and write it. This is what gives a
    // PARAM's raw rows meaning. Bind it to a def once and use it for any number of rows; it doesn't hold the def or
    // the param, so it stays valid on its own.
    //
    //     PARAMDEF Def = PARAMDEF::Read("EQUIP_PARAM_WEAPON_ST.paramdef");
    //     PARAM Param  = PARAM::Read("EquipParamWeapon.param");
    //     if (const auto Layout = ParamLayout::TryCreate(Param, Def)) {
    //         PARAM::Row* Row = Param.Find(1000000);
    //         float Weight = Layout->Get<float>(*Row, "weight");
    //         Layout->Set(*Row, "weight", 2.5f);
    //     }
    class SOULS_API ParamLayout {
    public:
        // Lays out the def's fields. BigEndian is the param's byte order. Throws BinaryException if the def's
        // bitfields are invalid.
        ParamLayout(const PARAMDEF& Def, bool BigEndian);

        // A layout for the param, or nullopt if the def doesn't describe it (same type, data version and row size;
        // see PARAM::Matches).
        static std::optional<ParamLayout> TryCreate(const PARAM& Param, const PARAMDEF& Def);

        // The size in bytes of every row.
        size_t RowSize() const { return Size; }
        size_t FieldCount() const { return Fields.size(); }

        // The index of the first field with this internal name, or nullopt.
        std::optional<size_t> IndexOf(std::string_view InternalName) const;
        const std::string& InternalName(size_t FieldIndex) const;
        PARAMDEF::DefType Type(size_t FieldIndex) const;

        // A new row with every field at its default value.
        PARAM::Row MakeRow(int32_t ID, std::optional<std::string> Name = std::nullopt) const;

#pragma region Access
        // Reads a field. Row.Bytes must be RowSize() long (throws otherwise).
        CellValue Get(const PARAM::Row& Row, size_t FieldIndex) const;
        CellValue Get(const PARAM::Row& Row, std::string_view InternalName) const;

        // Reads a field as a specific type (see CellValue); throws if the field has a different type.
        template<typename T>
        T Get(const PARAM::Row& Row, std::string_view InternalName) const {
            CellValue Value = Get(Row, InternalName);
            if (const T* Typed = std::get_if<T>(&Value)) return *Typed;
            ThrowTypeMismatch(InternalName);
        }

        // Writes a field. Numbers convert to the field's own type (truncating, as the file would), strings fit the
        // field's fixed width, and dummy8 arrays must be exactly the array length. Throws on a mismatch, such as
        // text for a number.
        void Set(PARAM::Row& Row, size_t FieldIndex, const CellValue& Value) const;
        void Set(PARAM::Row& Row, std::string_view InternalName, const CellValue& Value) const;

        // Reads any numeric field as a double / writes any numeric field from one.
        double GetNumber(const PARAM::Row& Row, std::string_view InternalName) const;
        void SetNumber(PARAM::Row& Row, std::string_view InternalName, double Value) const;
#pragma endregion

    private:
        struct FieldInfo {
            std::string Name;
            PARAMDEF::DefType Type = PARAMDEF::DefType::f32;
            int32_t ArrayLength    = 1;
            // Byte offset of the field (or, for a bitfield, of the integer it's packed into) in the row.
            size_t Offset = 0;
            // Bytes the field occupies on its own: the value (or array) size, or the bitfield's storage integer.
            size_t Length = 0;
            // For bitfields: the bit position within the storage integer (LSB first) and the width; BitSize is -1 for
            // ordinary fields.
            int32_t BitOffset = 0;
            int32_t BitSize   = -1;
            PARAMDEF::EditorValue Default;
        };

        const FieldInfo& Field(size_t FieldIndex) const;
        size_t Require(std::string_view InternalName) const;
        [[noreturn]] static void ThrowTypeMismatch(std::string_view InternalName);

        std::vector<FieldInfo> Fields;
        std::map<std::string, size_t, std::less<>> ByName;
        size_t Size   = 0;
        bool BigEndian = false;
    };

#pragma warning(pop)

}  // namespace Souls
