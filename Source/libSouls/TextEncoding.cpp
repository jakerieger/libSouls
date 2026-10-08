//
// Created by Jake Rieger on 10/7/2026.
//

#include "TextEncoding.hpp"
#include "Endian.hpp"

#include <windows.h>

namespace Souls::Text {
    namespace {
        constexpr UINT ShiftJISCodePage = 932;

        static_assert(sizeof(wchar_t) == sizeof(char16_t));

        std::wstring ToWide(UINT CodePage, std::string_view Value) {
            if (Value.empty()) {
                return {};
            }
            const int Size = MultiByteToWideChar(CodePage, 0, Value.data(), static_cast<int>(Value.size()), nullptr, 0);
            if (Size <= 0) {
                throw BinaryException("Text conversion to UTF-16 failed");
            }
            std::wstring Result(static_cast<size_t>(Size), L'\0');
            MultiByteToWideChar(CodePage, 0, Value.data(), static_cast<int>(Value.size()), Result.data(), Size);
            return Result;
        }

        std::string FromWide(UINT CodePage, std::wstring_view Value) {
            if (Value.empty()) {
                return {};
            }
            const int Size = WideCharToMultiByte(
              CodePage, 0, Value.data(), static_cast<int>(Value.size()), nullptr, 0, nullptr, nullptr);
            if (Size <= 0) {
                throw BinaryException("Text conversion from UTF-16 failed");
            }
            std::string Result(static_cast<size_t>(Size), '\0');
            WideCharToMultiByte(
              CodePage, 0, Value.data(), static_cast<int>(Value.size()), Result.data(), Size, nullptr, nullptr);
            return Result;
        }
    }  // namespace

    std::string ShiftJISToUTF8(std::string_view Value) {
        return FromWide(CP_UTF8, ToWide(ShiftJISCodePage, Value));
    }

    std::string UTF8ToShiftJIS(std::string_view Value) {
        return FromWide(ShiftJISCodePage, ToWide(CP_UTF8, Value));
    }

    std::string UTF16ToUTF8(std::u16string_view Value) {
        return FromWide(CP_UTF8, std::wstring_view(reinterpret_cast<const wchar_t*>(Value.data()), Value.size()));
    }

    std::u16string UTF8ToUTF16(std::string_view Value) {
        const std::wstring Wide = ToWide(CP_UTF8, Value);
        return {reinterpret_cast<const char16_t*>(Wide.data()), Wide.size()};
    }
}  // namespace Souls::Text
