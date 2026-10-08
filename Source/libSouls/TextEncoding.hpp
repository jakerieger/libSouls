//
// Created by Jake Rieger on 10/7/2026.
//

#pragma once

#include "Souls.hpp"

#include <string>
#include <string_view>

// Conversions between the encodings FromSoftware formats use and UTF-8, which is what the rest of the library
// uses for text. Conversion is lenient: invalid input becomes replacement characters rather than throwing.
namespace Souls::Text {
    SOULS_API std::string ShiftJISToUTF8(std::string_view Value);
    SOULS_API std::string UTF8ToShiftJIS(std::string_view Value);
    SOULS_API std::string UTF16ToUTF8(std::u16string_view Value);
    SOULS_API std::u16string UTF8ToUTF16(std::string_view Value);
}  // namespace Souls::Text
