//
// Created by Jake Rieger on 10/7/2026.
//

#pragma once

#include <libSouls/SoulsFile.hpp>
#include <libSouls/Formats/PARAMDEF.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of an exported class; only touched by this DLL's own code

    // PARAM: the game's tabular configuration data (weapon stats, spell data, drop tables, and so on). A param is a
    // list of rows, each with an ID, an optional name and a fixed-size block of data.
    //
    // This class handles the container only. The meaning of the bytes in each row comes from a separate PARAMDEF,
    // which isn't applied here yet: rows are exposed as raw bytes, and writing them back is lossless.
    class SOULS_API PARAM : public SoulsFile<PARAM> {
    public:
        // First set of format flags; highly speculative.
        enum class FormatFlags1 : uint8_t {
            None           = 0,
            Flag01         = 0b0000'0001,  // unknown
            IntDataOffset  = 0b0000'0010,  // expanded header with a 32-bit data offset
            LongDataOffset = 0b0000'0100,  // expanded header with a 64-bit data offset (and 64-bit row offsets)
            Flag08         = 0b0000'1000,
            Flag10         = 0b0001'0000,
            Flag20         = 0b0010'0000,
            Flag40         = 0b0100'0000,
            OffsetParamType = 0b1000'0000,  // param type string is stored separately, not fixed-width in the header
        };

        // Second set of format flags; highly speculative.
        enum class FormatFlags2 : uint8_t {
            None            = 0,
            UnicodeRowNames = 0b0000'0001,  // row names are UTF-16 instead of Shift-JIS
            Flag02          = 0b0000'0010,
            Flag04          = 0b0000'0100,
            Flag08          = 0b0000'1000,
            Flag10          = 0b0001'0000,
            Flag20          = 0b0010'0000,
            Flag40          = 0b0100'0000,
            Flag80          = 0b1000'0000,
        };

        // One row. All rows of a param have Bytes of the same length.
        struct SOULS_API Row {
            int32_t ID = 0;
            // UTF-8. No functional significance; absent if the row has no name.
            std::optional<std::string> Name;
            // The row's data, uninterpreted.
            std::vector<uint8_t> Bytes;

            Row() = default;
            Row(int32_t ID, std::optional<std::string> Name, std::vector<uint8_t> Bytes)
                : ID(ID), Name(std::move(Name)), Bytes(std::move(Bytes)) {}

            std::string ToString() const { return std::to_string(ID) + " " + Name.value_or(""); }
        };

        // Whether the file is big-endian; true for PS3/360 files.
        bool BigEndian = false;
        FormatFlags1 Format2D = FormatFlags1::None;
        FormatFlags2 Format2E = FormatFlags2::None;
        // Originally matched the paramdef for version 101, but since is always 0 or 0xFF.
        uint8_t ParamdefFormatVersion = 0;
        int16_t Unk06                 = 0;
        // A revision of the row data structure. A paramdef only applies if this matches its data version.
        int16_t ParamdefDataVersion = 0;
        // Identifies which paramdef describes this param.
        std::string ParamType;
        // The size of each row's data, worked out from the spacing of row offsets when read; -1 if there were no
        // rows. Write() takes the row size from the rows' Bytes instead.
        int64_t DetectedSize = -1;
        std::vector<Row> Rows;

        // The first row with this ID, or nullptr.
        Row* Find(int32_t ID);
        const Row* Find(int32_t ID) const;

        // Whether the def describes this param: same param type and data version, and a row size that agrees with the
        // def's. (A param with no rows has no row size to compare.)
        bool Matches(const PARAMDEF& Def) const;
        // The first of the defs that matches, or nullptr.
        const PARAMDEF* FindMatchingDef(std::span<const PARAMDEF> Defs) const;

        // "<ParamType> v<DataVersion> [<row count>]"
        std::string ToString() const;

    protected:
        // Params have no magic, so this is a structural check of the header; it can't be certain.
        bool IsImpl(BinaryReader& Reader) override;
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

    constexpr PARAM::FormatFlags1 operator|(PARAM::FormatFlags1 A, PARAM::FormatFlags1 B) {
        return static_cast<PARAM::FormatFlags1>(static_cast<uint8_t>(A) | static_cast<uint8_t>(B));
    }
    constexpr PARAM::FormatFlags1 operator&(PARAM::FormatFlags1 A, PARAM::FormatFlags1 B) {
        return static_cast<PARAM::FormatFlags1>(static_cast<uint8_t>(A) & static_cast<uint8_t>(B));
    }
    constexpr PARAM::FormatFlags2 operator|(PARAM::FormatFlags2 A, PARAM::FormatFlags2 B) {
        return static_cast<PARAM::FormatFlags2>(static_cast<uint8_t>(A) | static_cast<uint8_t>(B));
    }
    constexpr PARAM::FormatFlags2 operator&(PARAM::FormatFlags2 A, PARAM::FormatFlags2 B) {
        return static_cast<PARAM::FormatFlags2>(static_cast<uint8_t>(A) & static_cast<uint8_t>(B));
    }

#pragma warning(pop)

}  // namespace Souls
