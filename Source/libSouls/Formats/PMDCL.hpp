/// @file
/// @author Jake Rieger
/// @date 10/8/2026

#pragma once

#include <libSouls/SoulsFile.hpp>

#include <vector>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of an exported class; only touched by this DLL's own code

    /// @brief PMDCL: defines static decals in Dark Souls III maps.
    ///
    /// Extension: .pmdcl
    class SOULS_API PMDCL : public SoulsFile<PMDCL> {
    public:
        /// @brief Effects such as blood spatter that are applied on nearby surfaces.
        struct Decal {
            /// @brief Unknown; might not be floats.
            Vector3 XAngles;
            /// @brief Unknown; might not be floats.
            Vector3 YAngles;
            /// @brief Unknown; might not be floats.
            Vector3 ZAngles;
            /// @brief Coordinates of the decal.
            Vector3 Position;
            /// @brief Unknown, 1 or 0 in existing files.
            float Unk3C = 1;
            /// @brief ID of a row in DecalParam.
            int32_t DecalParamID = 0;
            /// @brief Controls the size of the decal in ways that are not entirely clear.
            int16_t Size1 = 10;
            /// @brief Controls the size of the decal in ways that are not entirely clear.
            int16_t Size2 = 10;
        };

        /// @brief Decals in this map.
        std::vector<Decal> Decals;

    protected:
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
