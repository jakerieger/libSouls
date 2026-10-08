//
// Created by Jake Rieger on 10/7/2026.
//

#pragma once

#include <libSouls/Formats/DCX.hpp>
#include "Binder.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Souls {
    // One file inside a binder, with its contents already decompressed.
    struct BinderFile {
        Binder::FileFlags Flags = Binder::FileFlags::Flag1;
        int32_t ID              = -1;
        // UTF-8. Absent for binders whose format has no names.
        std::optional<std::string> Name;
        std::vector<uint8_t> Bytes;
        // How the file is compressed when the binder is written with FileFlags::Compressed set. Set from the file's
        // contents when read.
        DCX::Type CompressionType = DCX::Type::Zlib;

        BinderFile() = default;
        BinderFile(Binder::FileFlags Flags, std::vector<uint8_t> Bytes)
            : Flags(Flags), Bytes(std::move(Bytes)) {}
        BinderFile(Binder::FileFlags Flags, int32_t ID, std::vector<uint8_t> Bytes)
            : Flags(Flags), ID(ID), Bytes(std::move(Bytes)) {}
        BinderFile(Binder::FileFlags Flags, std::string Name, std::vector<uint8_t> Bytes)
            : Flags(Flags), Name(std::move(Name)), Bytes(std::move(Bytes)) {}
        BinderFile(Binder::FileFlags Flags, int32_t ID, std::string Name, std::vector<uint8_t> Bytes)
            : Flags(Flags), ID(ID), Name(std::move(Name)), Bytes(std::move(Bytes)) {}

        std::string ToString() const {
            return "Flags: 0x" + ToHex(static_cast<uint8_t>(Flags)) + " | ID: " + std::to_string(ID) +
                   " | Name: " + Name.value_or("") + " | Length: " + std::to_string(Bytes.size());
        }

    private:
        static std::string ToHex(uint8_t Value) {
            constexpr char Digits[] = "0123456789ABCDEF";
            return {Digits[Value >> 4], Digits[Value & 0xF]};
        }
    };
}  // namespace Souls
