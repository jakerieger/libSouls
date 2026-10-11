/// @file
/// @author Jake Rieger
/// @date 10/7/2026

#pragma once

#include <libSouls/Formats/DCX.hpp>
#include "Binder.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Souls {
    /// @brief One file inside a binder, with its contents already decompressed.
    struct BinderFile {
        /// @brief Flags indicating compression, and possibly other things.
        Binder::FileFlags Flags = Binder::FileFlags::Flag1;
        /// @brief ID of the file, or -1 for none.
        int32_t ID              = -1;
        /// @brief UTF-8.
        ///
        /// Absent for binders whose format has no names.
        std::optional<std::string> Name;
        /// @brief Raw file data.
        std::vector<uint8_t> Bytes;
        /// @brief How the file is compressed when the binder is written with FileFlags::Compressed set.
        ///
        /// Set from the file's contents when read.
        DCX::Type CompressionType = DCX::Type::Zlib;

        BinderFile() = default;
        /// @brief A file with flags and contents, but no ID or name.
        BinderFile(Binder::FileFlags Flags, std::vector<uint8_t> Bytes)
            : Flags(Flags), Bytes(std::move(Bytes)) {}
        /// @brief A file with flags, an ID and contents.
        BinderFile(Binder::FileFlags Flags, int32_t ID, std::vector<uint8_t> Bytes)
            : Flags(Flags), ID(ID), Bytes(std::move(Bytes)) {}
        /// @brief A file with flags, a name (UTF-8) and contents.
        BinderFile(Binder::FileFlags Flags, std::string Name, std::vector<uint8_t> Bytes)
            : Flags(Flags), Name(std::move(Name)), Bytes(std::move(Bytes)) {}
        /// @brief A file with flags, an ID, a name (UTF-8) and contents.
        BinderFile(Binder::FileFlags Flags, int32_t ID, std::string Name, std::vector<uint8_t> Bytes)
            : Flags(Flags), ID(ID), Name(std::move(Name)), Bytes(std::move(Bytes)) {}

        /// @brief A one-line summary: flags, ID, name and length.
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
