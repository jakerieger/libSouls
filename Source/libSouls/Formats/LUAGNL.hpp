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

    /// @brief LUAGNL: a list of global variable names for Lua scripts.
    ///
    /// Extension: .luagnl
    class SOULS_API LUAGNL : public SoulsFile<LUAGNL> {
    public:
        /// @brief If true, write as big endian.
        bool BigEndian = false;
        /// @brief If true, write with 64-bit offsets and UTF-16 strings.
        bool LongFormat = false;
        /// @brief Global variable names.
        std::vector<std::string> Globals;

        /// @brief An empty LUAGNL formatted for PC Dark Souls.
        LUAGNL() = default;
        /// @brief An empty LUAGNL with the given layout settings.
        LUAGNL(bool BigEndian, bool LongFormat) : BigEndian(BigEndian), LongFormat(LongFormat) {}

    protected:
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
