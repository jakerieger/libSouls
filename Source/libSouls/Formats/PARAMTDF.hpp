// Copyright (C) 2026 Jake Rieger
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, version 3. This program is distributed in the hope that it will
// be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU General
// Public License along with this program. If not, see https://www.gnu.org/licenses/.

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

    /// @brief PARAMTDF: a companion format to PARAM and PARAMDEF that provides friendly names for enumerated value
    ///        types.
    ///
    /// It is a plain text format (extension .tdf), not a binary SoulsFile.
    class SOULS_API PARAMTDF {
    public:
        /// @brief An entry's value, held as the type of the TDF.
        using Value = std::variant<int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t>;

        /// @brief A named enumerator.
        struct Entry {
            /// @brief Name given to this value, if any.
            std::optional<std::string> Name;
            /// @brief Value of this entry, of the same type as the parent TDF.
            Value Val = int32_t {0};
        };

        /// @brief The identifier of this type.
        std::string Name = "UNSPECIFIED";
        /// @brief Named values in this TDF.
        std::vector<Entry> Entries;

        /// @brief An empty TDF of type s32.
        PARAMTDF() = default;
        /// @brief Reads a TDF from From's plaintext format.
        ///
        /// Throws BinaryException on malformed text.
        explicit PARAMTDF(const std::string& Text);

        /// @brief The type of values in this TDF; must be an integral type (s8, u8, s16, u16, s32 or u32).
        PARAMDEF::DefType GetType() const { return Type; }
        /// @brief Changes the enum's value type; throws std::invalid_argument unless it's s8, u8, s16, u16, s32 or u32.
        void SetType(PARAMDEF::DefType NewType);

        /// @brief The value of the entry with this name, or nullopt.
        std::optional<Value> FindValue(const std::string& EntryName) const;
        /// @brief The name of the first entry with this value, or nullopt if there is none or it has no name.
        std::optional<std::string> FindName(const Value& Val) const;

        /// @brief Writes the TDF in From's plaintext format.
        std::string Write() const;

    private:
        PARAMDEF::DefType Type = PARAMDEF::DefType::s32;
    };

#pragma warning(pop)

}  // namespace Souls
