// Copyright (C) 2026 Jake Rieger
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, version 3. This program is distributed in the hope that it will
// be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU General
// Public License along with this program. If not, see https://www.gnu.org/licenses/.

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
