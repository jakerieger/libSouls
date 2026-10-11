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
#include <string>
#include <vector>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of an exported class; only touched by this DLL's own code

    /// @brief BTPB: a collection of spherical harmonics light probes for lighting characters and objects in a map.
    ///
    /// Extension: .btpb
    class SOULS_API BTPB : public SoulsFile<BTPB> {
    public:
        /// @brief Supported BTPB formats.
        enum class BTPBVersion {
            DarkSouls2LE,  ///< Dark Souls II on PC and Scholar of the First Sin on all platforms
            DarkSouls2BE,  ///< Dark Souls II on PS3 and Xbox 360
            Bloodborne,
            DarkSouls3,  ///< on all platforms
        };

        /// @brief A probe giving directional lighting information at a given point.
        struct Probe {
            /// @brief First-order spherical harmonics coefficients in R0G0B0R1G1B1... order.
            std::array<int16_t, 12> Coefficients {};
            /// @brief Multiplies sun lighting, where 0 is 0% sun and 1024 is 100%.
            int16_t LightMask = 0;
            /// @brief Always 0 outside the chalice BTPB.
            int16_t Unk1A = 0;
            /// @brief The position of the probe; not present in Dark Souls II.
            Vector3 Position;
        };

        /// @brief A volume containing light probes with some additional configuration.
        struct Group {
            /// @brief An optional name for the group.
            ///
            /// Presence appears to be indicated by the lowest bit of Flags08.
            std::string Name;
            /// @brief Appears to be flags, highly speculative.
            int32_t Flags08 = 0;
            /// @brief Unknown.
            int32_t Unk10 = 0;
            /// @brief Unknown.
            int32_t Unk14 = 0;
            /// @brief Unknown.
            int32_t Unk18 = 0;
            /// @brief Unknown.
            float Unk1C = 0;
            /// @brief Unknown.
            float Unk20 = 0;
            /// @brief Unknown.
            float Unk24 = 0;
            /// @brief Probably bounding box min and max.
            Vector3 Unk28;
            /// @brief Unknown; probably bounding box max.
            Vector3 Unk34;
            /// @brief Light probes in this group.
            std::vector<Probe> Probes;
            /// @brief Only present since Dark Souls III.
            float Unk48 = 0;
            /// @brief Unknown; only present since DS3.
            float Unk4C = 0;
            /// @brief Unknown; only present since DS3.
            float Unk50 = 0;
            /// @brief Unknown; only present since DS3.
            uint8_t Unk94 = 0;
            /// @brief Unknown; only present since DS3.
            uint8_t Unk95 = 0;
            /// @brief Unknown; only present since DS3.
            uint8_t Unk96 = 0;
        };

        /// @brief Indicates the format of the file and supported features.
        BTPBVersion Version = BTPBVersion::DarkSouls3;
        /// @brief Probably bounding box min and max.
        Vector3 Unk1C;
        /// @brief Unknown; probably bounding box max.
        Vector3 Unk28;
        /// @brief Groups of light probes in the map.
        std::vector<Group> Groups;

    protected:
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
