/// @file
/// @author Jake Rieger
/// @date 10/8/2026

#pragma once

#include <libSouls/SoulsFile.hpp>

#include <string>
#include <vector>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of an exported class; only touched by this DLL's own code

    /// @brief BTAB: a lightmap atlasing config file introduced in Dark Souls II.
    ///
    /// Extension: .btab
    class SOULS_API BTAB : public SoulsFile<BTAB> {
    public:
        /// @brief Configures lightmap atlasing for a certain part and material.
        struct SOULS_API Entry {
            /// @brief The name of the target part defined in an MSB file.
            std::string PartName;
            /// @brief The name of the target material in the part's FLVER model.
            std::string MaterialName;
            /// @brief The ID of the atlas texture to use.
            int32_t AtlasID = 0;
            /// @brief Offsets the lightmap UVs.
            Vector2 UVOffset;
            /// @brief Scales the lightmap UVs.
            Vector2 UVScale{1.f, 1.f};

            /// @brief "PartName : MaterialName".
            std::string ToString() const { return PartName + " : " + MaterialName; }
        };

        /// @brief Whether the file is big-endian; true for PS3/X360, false otherwise.
        bool BigEndian = false;
        /// @brief Whether the file uses the 64-bit format; true for Dark Souls III, false for Dark Souls II.
        bool LongFormat = false;
        /// @brief Material configs in this file.
        std::vector<Entry> Entries;

    protected:
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
