// Copyright (C) 2026 Jake Rieger
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, version 3. This program is distributed in the hope that it will
// be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU General
// Public License along with this program. If not, see https://www.gnu.org/licenses/.

#pragma once

#include <libSouls/SoulsFile.hpp>

#include <array>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of an exported class; only touched by this DLL's own code

    /// @brief GPARAM: a graphics config file used since Dark Souls II.
    ///
    /// Extensions: .fltparam, .gparam
    class SOULS_API GPARAM : public SoulsFile<GPARAM> {
    public:
        /// @brief The game this GPARAM is from.
        enum class GPGame : uint32_t {
            DarkSouls2 = 2,
            DarkSouls3 = 3,  ///< and Bloodborne
            Sekiro     = 5,
        };

        /// @brief Value types allowed in a param.
        enum class ParamType : uint8_t {
            Byte   = 0x1,  ///< unknown; only ever appears as a single value
            Short  = 0x2,  ///< one short
            IntA   = 0x3,  ///< one int
            BoolA  = 0x5,  ///< one bool
            IntB   = 0x7,  ///< one int
            Float  = 0x9,  ///< one float
            BoolB  = 0xB,  ///< one bool
            Float2 = 0xC,  ///< two floats and 8 unused bytes
            Float3 = 0xD,  ///< three floats and 4 unused bytes
            Float4 = 0xE,  ///< four floats
            Byte4  = 0xF,  ///< four bytes, used for BGRA
        };

        /// @brief One value; the alternative in use depends on the param's Type (uint8_t for Byte, int16_t for Short,
        ///        int32_t for IntA/IntB, bool for BoolA/BoolB, float, Vector2/3/4, and a 4-byte array for Byte4).
        using ParamValue =
          std::variant<uint8_t, int16_t, int32_t, bool, float, Vector2, Vector3, Vector4, std::array<uint8_t, 4>>;

        /// @brief A collection of values controlling the same parameter in different circumstances.
        struct Param {
            /// @brief Identifies the param specifically.
            std::string Name1;
            /// @brief Identifies the param generically.
            ///
            /// Not present in Dark Souls II.
            std::string Name2;
            /// @brief Type of values in this param.
            ParamType Type = ParamType::Float;
            /// @brief Values in this param.
            std::vector<ParamValue> Values;
            /// @brief Unknown.
            std::vector<int32_t> ValueIDs;
            /// @brief Unknown; one for each value ID, only present in Sekiro.
            std::optional<std::vector<float>> UnkFloats;
        };

        /// @brief A group of graphics params.
        struct Group {
            /// @brief Identifies the group.
            std::string Name1;
            /// @brief Identifies the group, but shorter?
            ///
            /// Not present in Dark Souls II.
            std::string Name2;
            /// @brief Params in this group.
            std::vector<Param> Params;
            /// @brief Comments indicating the purpose of each entry in param values.
            ///
            /// Not present in Dark Souls II.
            std::vector<std::string> Comments;

            /// @brief The param with this name in the group, or nullptr.
            Param* FindParam(const std::string& Name);
        };

        /// @brief Unknown.
        struct Unk3 {
            /// @brief Index of a group.
            int32_t GroupIndex = 0;
            /// @brief Unknown; matches value IDs in the group.
            std::vector<int32_t> ValueIDs;
            /// @brief Unknown; only present in Sekiro.
            int32_t Unk0C = 0;
        };

        /// @brief Indicates the format of the GPARAM.
        GPGame Game = GPGame::Sekiro;
        /// @brief Unknown.
        bool Unk0D = false;
        /// @brief Unknown; in Dark Souls II, number of entries in UnkBlock2.
        int32_t Unk14 = 0;
        /// @brief Unknown; zero before Elden Ring.
        float Unk40 = 0;
        /// @brief Unknown; only present in Sekiro.
        float Unk50 = 0;
        /// @brief Groups of params in this file.
        std::vector<Group> Groups;
        /// @brief Unknown.
        std::vector<uint8_t> UnkBlock2;
        /// @brief Unknown.
        std::vector<Unk3> Unk3s;

        /// @brief The first group with this name, or nullptr.
        Group* FindGroup(const std::string& Name1);

    protected:
        bool IsImpl(BinaryReader& Reader) override;
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
