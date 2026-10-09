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

    // F2TR: a format that indicates which vertices of a FLVER are relevant for FaceGen. Extension: .flver2tri
    class SOULS_API F2TR : public SoulsFile<F2TR> {
    public:
        // A collection of indices, probably corresponding to a mesh.
        struct Entry {
            // Name of the relevant FaceGen file, I think.
            std::string Name;
            // Presumably vertex indices in the FLVER.
            std::vector<int16_t> Indices;
        };

        // Whether the file is big-endian.
        bool BigEndian = false;
        std::vector<Entry> Entries;

    protected:
        bool IsImpl(BinaryReader& Reader) override;
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
