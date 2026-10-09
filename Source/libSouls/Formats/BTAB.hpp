//
// Created by Jake Rieger on 10/8/2026.
//

#pragma once

#include <libSouls/SoulsFile.hpp>

#include <string>
#include <vector>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of an exported class; only touched by this DLL's own code

    // BTAB: a lightmap atlasing config file introduced in Dark Souls II. Extension: .btab
    class SOULS_API BTAB : public SoulsFile<BTAB> {
    public:
        // Configures lightmap atlasing for a certain part and material.
        struct SOULS_API Entry {
            // The name of the target part defined in an MSB file.
            std::string PartName;
            // The name of the target material in the part's FLVER model.
            std::string MaterialName;
            // The ID of the atlas texture to use.
            int32_t AtlasID = 0;
            // Offsets the lightmap UVs.
            Vector2 UVOffset;
            // Scales the lightmap UVs.
            Vector2 UVScale{1.f, 1.f};

            std::string ToString() const { return PartName + " : " + MaterialName; }
        };

        // Whether the file is big-endian; true for PS3/X360, false otherwise.
        bool BigEndian = false;
        // Whether the file uses the 64-bit format; true for Dark Souls III, false for Dark Souls II.
        bool LongFormat = false;
        std::vector<Entry> Entries;

    protected:
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
