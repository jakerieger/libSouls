//
// Created by Jake Rieger on 10/8/2026.
//

#pragma once

#include <libSouls/SoulsFile.hpp>

#include <vector>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of an exported class; only touched by this DLL's own code

    // CLM2: a companion file to a FLVER that has something to do with cloth, probably. Extension: .clm
    class SOULS_API CLM2 : public SoulsFile<CLM2> {
    public:
        // Unknown what this does.
        struct Entry {
            int16_t Unk00 = 0;
            int16_t Unk02 = 0;
        };

        // A list of entries that control something or other in a corresponding FLVER mesh.
        struct Mesh {
            std::vector<Entry> Entries;
        };

        // Each of these corresponds to a mesh in the FLVER.
        std::vector<Mesh> Meshes;

    protected:
        bool IsImpl(BinaryReader& Reader) override;
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
