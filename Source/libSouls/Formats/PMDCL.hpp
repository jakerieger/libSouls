//
// Created by Jake Rieger on 10/8/2026.
//

#pragma once

#include <libSouls/SoulsFile.hpp>

#include <vector>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of an exported class; only touched by this DLL's own code

    // PMDCL: defines static decals in Dark Souls III maps. Extension: .pmdcl
    class SOULS_API PMDCL : public SoulsFile<PMDCL> {
    public:
        // Effects such as blood spatter that are applied on nearby surfaces.
        struct Decal {
            // Unknown. Might not even be floats.
            Vector3 XAngles, YAngles, ZAngles;
            // Coordinates of the decal.
            Vector3 Position;
            // Unknown, 1 or 0 in existing files.
            float Unk3C = 1;
            // ID of a row in DecalParam.
            int32_t DecalParamID = 0;
            // Controls the size of the decal in ways that are not entirely clear.
            int16_t Size1 = 10, Size2 = 10;
        };

        std::vector<Decal> Decals;

    protected:
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
