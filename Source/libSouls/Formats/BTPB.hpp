//
// Created by Jake Rieger on 10/8/2026.
//

#pragma once

#include <libSouls/SoulsFile.hpp>

#include <array>
#include <string>
#include <vector>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of an exported class; only touched by this DLL's own code

    // BTPB: a collection of spherical harmonics light probes for lighting characters and objects in a map.
    // Extension: .btpb
    class SOULS_API BTPB : public SoulsFile<BTPB> {
    public:
        // Supported BTPB formats.
        enum class BTPBVersion {
            DarkSouls2LE,  // Dark Souls II on PC and Scholar of the First Sin on all platforms
            DarkSouls2BE,  // Dark Souls II on PS3 and Xbox 360
            Bloodborne,
            DarkSouls3,    // on all platforms
        };

        // A probe giving directional lighting information at a given point.
        struct Probe {
            // First-order spherical harmonics coefficients in R0G0B0R1G1B1... order.
            std::array<int16_t, 12> Coefficients{};
            // Multiplies sun lighting, where 0 is 0% sun and 1024 is 100%.
            int16_t LightMask = 0;
            // Always 0 outside the chalice BTPB.
            int16_t Unk1A = 0;
            // The position of the probe; not present in Dark Souls II.
            Vector3 Position;
        };

        // A volume containing light probes with some additional configuration.
        struct Group {
            // An optional name for the group. Presence appears to be indicated by the lowest bit of Flags08.
            std::string Name;
            // Appears to be flags, highly speculative.
            int32_t Flags08 = 0;
            int32_t Unk10   = 0;
            int32_t Unk14   = 0;
            int32_t Unk18   = 0;
            float Unk1C     = 0;
            float Unk20     = 0;
            float Unk24     = 0;
            // Probably bounding box min and max.
            Vector3 Unk28;
            Vector3 Unk34;
            std::vector<Probe> Probes;
            // Only present since Dark Souls III.
            float Unk48  = 0;
            float Unk4C  = 0;
            float Unk50  = 0;
            uint8_t Unk94 = 0;
            uint8_t Unk95 = 0;
            uint8_t Unk96 = 0;
        };

        // Indicates the format of the file and supported features.
        BTPBVersion Version = BTPBVersion::DarkSouls3;
        // Probably bounding box min and max.
        Vector3 Unk1C;
        Vector3 Unk28;
        std::vector<Group> Groups;

    protected:
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
