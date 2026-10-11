// Copyright (C) 2026 Jake Rieger
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, version 3. This program is distributed in the hope that it will
// be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU General
// Public License along with this program. If not, see https://www.gnu.org/licenses/.

#pragma once

#include "EMEVD.hpp"

#include <libSouls/SoulsFile.hpp>

#include <string>
#include <vector>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of an exported class; only touched by this DLL's own code

    /// @brief EMELD: a companion file to EMEVD that assigns names to different events.
    ///
    /// Extension: .eld, .emeld
    class SOULS_API EMELD : public SoulsFile<EMELD> {
    public:
        /// @brief Assigns a name to a certain event ID.
        struct Event {
            /// @brief ID of the event.
            int64_t ID = 0;
            /// @brief Name of the event.
            std::string Name;
        };

        /// @brief Determines the format the EMELD will be written in.
        EMEVD::Game Format = EMEVD::Game::DarkSouls1;
        /// @brief Events corresponding to those in the EMEVD.
        std::vector<Event> Events;

        EMELD() = default;
        /// @brief An empty EMELD for the given game's layout.
        explicit EMELD(EMEVD::Game Format) : Format(Format) {}

    protected:
        bool IsImpl(BinaryReader& Reader) override;
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
