//
// Created by Jake Rieger on 10/7/2026.
//

#pragma once

#include <libSouls/SoulsFile.hpp>

#include <cstdint>
#include <filesystem>
#include <string_view>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of exported classes; only touched by this DLL's own code

    // PARAMDEF: the companion to a PARAM that says what each field in a row is: its name, type, size, default value
    // and so on. Rows are fixed-size blocks of bytes; this is the only thing that gives them meaning. File
    // extensions: .def, .paramdef.
    class SOULS_API PARAMDEF : public SoulsFile<PARAMDEF> {
    public:
        // The primitive types a field can have.
        enum class DefType {
            s8,       // signed 1-byte integer
            u8,       // unsigned 1-byte integer
            s16,      // signed 2-byte integer
            u16,      // unsigned 2-byte integer
            s32,      // signed 4-byte integer
            u32,      // unsigned 4-byte integer
            b32,      // 4-byte integer representing a boolean
            f32,      // single-precision float
            angle32,  // single-precision float representing an angle
            f64,      // double-precision float
            dummy8,   // byte or array of bytes used for padding or placeholding
            fixstr,   // fixed-width Shift-JIS string
            fixstrW,  // fixed-width UTF-16 string
        };

        // Flags that control editor behavior for a field.
        enum class EditFlags : int32_t {
            None = 0,  // editable and doesn't wrap
            Wrap = 1,  // wraps around when scrolled past the minimum or maximum
            Lock = 4,  // may not be edited
        };

        // A default/minimum/maximum/increment value. Empty for array types; always a float before format version 203,
        // after which it's an int, float or double to suit the field's type.
        using EditorValue = std::variant<std::monostate, int32_t, float, double>;

        // One field of a row.
        struct SOULS_API Field {
            // Name to show in the editor (UTF-8).
            std::string DisplayName;
            DefType DisplayType = DefType::f32;
            // printf-style format to apply to the value in the editor.
            std::string DisplayFormat;
            EditorValue Default;
            EditorValue Minimum;
            EditorValue Maximum;
            EditorValue Increment;
            EditFlags EditorFlags = EditFlags::None;
            // Number of elements for array types (dummy8, fixstr, fixstrW); otherwise 1.
            int32_t ArrayLength = 1;
            std::optional<std::string> Description;
            // The value's type in the engine; may be an enum type.
            std::string InternalType;
            // The value's name in the engine; not present before format version 102.
            std::string InternalName;
            // Number of bits used by a bitfield (unsigned types only); -1 when not used.
            int32_t BitSize = -1;
            // Fields are ordered by this in the editor; not present before format version 104.
            int32_t SortID = 0;
            // Unknown strings, only in format version 202 and up (so far only seen in 202).
            std::optional<std::string> UnkB8;
            std::optional<std::string> UnkC0;
            std::optional<std::string> UnkC8;

            // A placeholder f32 field.
            Field();
            // A field of the given type and name with the usual default values for the def's format.
            Field(const PARAMDEF* Def, DefType DisplayType, std::string InternalName);

            // "<type> <name>", with ":bits" or "[length]" where they apply.
            std::string ToString() const;
        };

        // Indicates a revision of the row data structure.
        int16_t DataVersion = 0;
        // Identifies the params this def describes.
        std::string ParamType;
        // True for PS3 and X360 games.
        bool BigEndian = false;
        // Whether certain strings are UTF-16 (true) or Shift-JIS (false).
        bool Unicode = false;
        // The file's format. 101: Enchanted Arms, Chromehounds, Armored Core 4/For Answer/V/Verdict Day, Shadow Assault:
        // Tenchu. 102: Demon's Souls. 103: Ninja Blade, Another Century's Episode: R. 104: Dark Souls, Steel
        // Battalion. 106: Elden Ring (deprecated ObjectParam). 201: Bloodborne. 202: Dark Souls 3. 203: Elden Ring,
        // Armored Core VI.
        int16_t FormatVersion = 104;
        // Fields in each row, in order of appearance.
        std::vector<Field> Fields;

        // A PARAMDEF formatted for Dark Souls.
        PARAMDEF() = default;

        // Whether defaults, minimums, maximums and increments may be ints/doubles as well as floats.
        bool VariableEditorValueTypes() const { return FormatVersion >= 203; }

        // The size in bytes of each row's data.
        int32_t GetRowSize() const;
        // The size of the first FieldCount fields.
        int32_t GetFieldsSize(size_t FieldCount) const;

        // "<ParamType> v<DataVersion>"
        std::string ToString() const;

        bool Validate(std::exception_ptr& Error) override;

#pragma region XML
        // The XML layout this library writes by default. Older layouts (0 to 2) differ only in a couple of element names.
        static constexpr int CurrentXmlVersion = 3;

        // Reads a PARAMDEF from the XML form the modding community shares defs in (for example Paramdex). Throws
        // BinaryException if the file is missing, isn't well-formed XML, or isn't a valid PARAMDEF.
        static PARAMDEF FromXml(const std::filesystem::path& Path);
        static PARAMDEF FromXmlString(std::string_view Xml);

        // Writes the PARAMDEF as XML in the given layout version (0 to CurrentXmlVersion), creating missing folders.
        // IncludeOffsets adds a "+0xNN" comment before each field at that byte offset in the row.
        void ToXml(const std::filesystem::path& Path, bool IncludeOffsets = false, int XmlVersion = CurrentXmlVersion) const;
        std::string ToXmlString(bool IncludeOffsets = false, int XmlVersion = CurrentXmlVersion) const;
#pragma endregion

    protected:
        // PARAMDEFs have no magic, so this is a structural check of the header; it can't be certain.
        bool IsImpl(BinaryReader& Reader) override;
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

    constexpr PARAMDEF::EditFlags operator|(PARAMDEF::EditFlags A, PARAMDEF::EditFlags B) {
        return static_cast<PARAMDEF::EditFlags>(static_cast<int32_t>(A) | static_cast<int32_t>(B));
    }
    constexpr PARAMDEF::EditFlags operator&(PARAMDEF::EditFlags A, PARAMDEF::EditFlags B) {
        return static_cast<PARAMDEF::EditFlags>(static_cast<int32_t>(A) & static_cast<int32_t>(B));
    }

#pragma warning(pop)

    // Facts about field types that param code needs.
    namespace ParamUtil {
        using DefType = PARAMDEF::DefType;

        // The name of the type as written in the files ("s8", "fixstrW", ...).
        SOULS_API const char* DefTypeName(DefType Type);
        // Parses a type name; throws BinaryException if it isn't one.
        SOULS_API DefType ParseDefType(const std::string& Name);

        // Types whose value is an array of ArrayLength elements.
        constexpr bool IsArrayType(DefType Type) {
            return Type == DefType::dummy8 || Type == DefType::fixstr || Type == DefType::fixstrW;
        }

        // Types that can be bitfields (packed into a shared integer).
        constexpr bool IsBitType(DefType Type) {
            switch (Type) {
                case DefType::s8:
                case DefType::u8:
                case DefType::s16:
                case DefType::u16:
                case DefType::s32:
                case DefType::u32:
                case DefType::dummy8:
                    return true;
                default:
                    return false;
            }
        }

        constexpr bool IsSignedBitType(DefType Type) {
            return Type == DefType::s8 || Type == DefType::s16 || Type == DefType::s32;
        }

        // The size in bytes of one element of the type.
        SOULS_API int32_t GetValueSize(DefType Type);
        // The number of bits in the integer a bitfield of this type is packed into. Throws for non-bit types.
        SOULS_API int32_t GetBitLimit(DefType Type);

        SOULS_API std::string GetDefaultFormat(DefType Type);
        SOULS_API PARAMDEF::EditFlags GetDefaultEditFlags(DefType Type);
        SOULS_API PARAMDEF::EditorValue GetDefaultDefault(const PARAMDEF* Def, DefType Type);
        SOULS_API PARAMDEF::EditorValue GetDefaultMinimum(const PARAMDEF* Def, DefType Type);
        SOULS_API PARAMDEF::EditorValue GetDefaultMaximum(const PARAMDEF* Def, DefType Type);
        SOULS_API PARAMDEF::EditorValue GetDefaultIncrement(const PARAMDEF* Def, DefType Type);
    }  // namespace ParamUtil

}  // namespace Souls
