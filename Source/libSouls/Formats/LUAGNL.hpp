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

    // LUAGNL: a list of global variable names for Lua scripts. Extension: .luagnl
    class SOULS_API LUAGNL : public SoulsFile<LUAGNL> {
    public:
        // If true, write as big endian.
        bool BigEndian = false;
        // If true, write with 64-bit offsets and UTF-16 strings.
        bool LongFormat = false;
        // Global variable names.
        std::vector<std::string> Globals;

        // An empty LUAGNL formatted for PC Dark Souls.
        LUAGNL() = default;
        LUAGNL(bool BigEndian, bool LongFormat) : BigEndian(BigEndian), LongFormat(LongFormat) {}

    protected:
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
