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

    // ENFL: a mysterious format used in Bloodborne, Dark Souls III and later. Speculation: determines assets to load
    // based on location in a map. Extension: .entryfilelist
    class SOULS_API ENFL : public SoulsFile<ENFL> {
    public:
        // Some kind of weird iteration through the strings.
        struct Struct1 {
            // Increase of index to the next Struct1: 0 means the next one has the same index, 2 means this Index + 2.
            int16_t Step = 0;
            // Almost certainly an index into the strings.
            int16_t Index = 0;
        };

        // Some data corresponding to each string. Possibly a hash?
        struct Struct2 {
            // Almost definitely not a single field. Appears to be 6 bytes of hash and a short in range 0-2.
            int64_t Unk1 = 0;
        };

        std::vector<Struct1> Struct1s;
        std::vector<Struct2> Struct2s;
        // A list of file paths.
        std::vector<std::string> Strings;

    protected:
        bool IsImpl(BinaryReader& Reader) override;
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
