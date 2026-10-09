//
// Created by Jake Rieger on 10/8/2026.
//

#pragma once

#include "PARAMDEF.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of an exported class; only touched by this DLL's own code

    // PARAMTDF: a companion format to PARAM and PARAMDEF that provides friendly names for enumerated value types.
    // It is a plain text format (extension .tdf), not a binary SoulsFile.
    class SOULS_API PARAMTDF {
    public:
        // An entry's value, held as the type of the TDF.
        using Value = std::variant<int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t>;

        // A named enumerator.
        struct Entry {
            // Name given to this value, if any.
            std::optional<std::string> Name;
            // Value of this entry, of the same type as the parent TDF.
            Value Val = int32_t{0};
        };

        // The identifier of this type.
        std::string Name = "UNSPECIFIED";
        std::vector<Entry> Entries;

        // An empty TDF of type s32.
        PARAMTDF() = default;
        // Reads a TDF from From's plaintext format. Throws BinaryException on malformed text.
        explicit PARAMTDF(const std::string& Text);

        // The type of values in this TDF; must be an integral type (s8, u8, s16, u16, s32 or u32).
        PARAMDEF::DefType GetType() const { return Type; }
        void SetType(PARAMDEF::DefType NewType);

        // The value of the entry with this name, or nullopt.
        std::optional<Value> FindValue(const std::string& EntryName) const;
        // The name of the first entry with this value, or nullopt if there is none or it has no name.
        std::optional<std::string> FindName(const Value& Val) const;

        // Writes the TDF in From's plaintext format.
        std::string Write() const;

    private:
        PARAMDEF::DefType Type = PARAMDEF::DefType::s32;
    };

#pragma warning(pop)

}  // namespace Souls
