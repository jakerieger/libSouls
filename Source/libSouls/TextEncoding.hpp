// Copyright (C) 2026 Jake Rieger
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, version 3. This program is distributed in the hope that it will
// be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU General
// Public License along with this program. If not, see https://www.gnu.org/licenses/.

#pragma once

#include "Souls.hpp"

#include <string>
#include <string_view>

/// @brief Conversions between the encodings FromSoftware formats use and UTF-8, which is what the rest of the library
///        uses for text.
///
/// Conversion is lenient: invalid input becomes replacement characters rather than throwing.
namespace Souls::Text {
    /// @brief Shift-JIS (code page 932) text to UTF-8.
    SOULS_API std::string ShiftJISToUTF8(std::string_view Value);
    /// @brief UTF-8 text to Shift-JIS (code page 932); characters it can't represent become the code page's default
    /// character (usually '?').
    SOULS_API std::string UTF8ToShiftJIS(std::string_view Value);
    /// @brief UTF-16 text to UTF-8.
    SOULS_API std::string UTF16ToUTF8(std::u16string_view Value);
    /// @brief UTF-8 text to UTF-16.
    SOULS_API std::u16string UTF8ToUTF16(std::string_view Value);
}  // namespace Souls::Text
