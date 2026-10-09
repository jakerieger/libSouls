//
// Created by Jake Rieger on 10/8/2026.
//

#include "PARAMTDF.hpp"

#include <charconv>
#include <sstream>

namespace Souls {
    using DefType = PARAMDEF::DefType;

    static std::string Trim(std::string Text, char Quote) {
        const auto First = Text.find_first_not_of(Quote);
        if (First == std::string::npos) {
            return {};
        }
        const auto Last = Text.find_last_not_of(Quote);
        return Text.substr(First, Last - First + 1);
    }

    template<typename T>
    static T ParseNumber(const std::string& Text) {
        long long Parsed = 0;
        const auto [Ptr, Error] = std::from_chars(Text.data(), Text.data() + Text.size(), Parsed);
        if (Error != std::errc{} || Ptr != Text.data() + Text.size() || Parsed < static_cast<long long>(std::numeric_limits<T>::min()) ||
            Parsed > static_cast<long long>(std::numeric_limits<T>::max())) {
            throw BinaryException("Invalid TDF value: " + Text);
        }
        return static_cast<T>(Parsed);
    }

    void PARAMTDF::SetType(DefType NewType) {
        switch (NewType) {
            case DefType::s8:
            case DefType::u8:
            case DefType::s16:
            case DefType::u16:
            case DefType::s32:
            case DefType::u32: Type = NewType; return;
            default: throw std::invalid_argument("TDF type may only be s8, u8, s16, u16, s32, or u32.");
        }
    }

    PARAMTDF::PARAMTDF(const std::string& Text) {
        std::vector<std::string> Lines;
        std::string Current;
        for (const char C : Text) {
            if (C == '\r' || C == '\n') {
                if (!Current.empty()) {
                    Lines.push_back(Current);
                    Current.clear();
                }
            } else {
                Current.push_back(C);
            }
        }
        if (!Current.empty()) {
            Lines.push_back(Current);
        }
        if (Lines.size() < 2) {
            throw BinaryException("A TDF needs at least a name and a type.");
        }

        Name = Trim(Lines[0], '"');
        SetType(ParamUtil::ParseDefType(Trim(Lines[1], '"')));

        for (size_t I = 2; I < Lines.size(); ++I) {
            const size_t Comma = Lines[I].find(',');
            if (Comma == std::string::npos) {
                throw BinaryException("Malformed TDF line: " + Lines[I]);
            }
            const std::string NameText  = Lines[I].substr(0, Comma);
            const std::string ValueText = Trim(Lines[I].substr(Comma + 1), '"');

            Entry E;
            switch (Type) {
                case DefType::s8: E.Val = ParseNumber<int8_t>(ValueText); break;
                case DefType::u8: E.Val = ParseNumber<uint8_t>(ValueText); break;
                case DefType::s16: E.Val = ParseNumber<int16_t>(ValueText); break;
                case DefType::u16: E.Val = ParseNumber<uint16_t>(ValueText); break;
                case DefType::s32: E.Val = ParseNumber<int32_t>(ValueText); break;
                default: E.Val = ParseNumber<uint32_t>(ValueText); break;
            }
            if (!NameText.empty()) {
                E.Name = Trim(NameText, '"');
            }
            Entries.push_back(std::move(E));
        }
    }

    std::optional<PARAMTDF::Value> PARAMTDF::FindValue(const std::string& EntryName) const {
        for (const Entry& E : Entries) {
            if (E.Name && *E.Name == EntryName) {
                return E.Val;
            }
        }
        return std::nullopt;
    }

    std::optional<std::string> PARAMTDF::FindName(const Value& Val) const {
        for (const Entry& E : Entries) {
            if (E.Val == Val) {
                return E.Name;
            }
        }
        return std::nullopt;
    }

    std::string PARAMTDF::Write() const {
        std::ostringstream Out;
        Out << '"' << Name << "\"\r\n";
        Out << '"' << ParamUtil::DefTypeName(Type) << "\"\r\n";
        for (const Entry& E : Entries) {
            if (E.Name) {
                Out << '"' << *E.Name << '"';
            }
            Out << ",\"";
            std::visit([&](auto V) { Out << static_cast<long long>(V); }, E.Val);
            Out << "\"\r\n";
        }
        return Out.str();
    }
}  // namespace Souls
