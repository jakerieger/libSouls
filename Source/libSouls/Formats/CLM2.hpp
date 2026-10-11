/// @file
/// @author Jake Rieger
/// @date 10/8/2026

#pragma once

#include <libSouls/SoulsFile.hpp>

#include <vector>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of an exported class; only touched by this DLL's own code

    /// @brief CLM2: a companion file to a FLVER that has something to do with cloth, probably.
    ///
    /// Extension: .clm
    class SOULS_API CLM2 : public SoulsFile<CLM2> {
    public:
        /// @brief Unknown what this does.
        struct Entry {
            /// @brief Unknown.
            int16_t Unk00 = 0;
            /// @brief Unknown.
            int16_t Unk02 = 0;
        };

        /// @brief A list of entries that control something or other in a corresponding FLVER mesh.
        struct Mesh {
            /// @brief The entries for this mesh.
            std::vector<Entry> Entries;
        };

        /// @brief Each of these corresponds to a mesh in the FLVER.
        std::vector<Mesh> Meshes;

    protected:
        bool IsImpl(BinaryReader& Reader) override;
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
