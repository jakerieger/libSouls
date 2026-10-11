// Copyright (C) 2026 Jake Rieger
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, version 3. This program is distributed in the hope that it will
// be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU General
// Public License along with this program. If not, see https://www.gnu.org/licenses/.

#pragma once

#include <libSouls/SoulsFile.hpp>

#include <vector>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of an exported class; only touched by this DLL's own code

    /// @brief RMB: a collection of controller rumble effects used in all games.
    ///
    /// Extension: .rmb
    class SOULS_API RMB : public SoulsFile<RMB> {
    public:
        /// @brief Defines a sweep of rumble motor strength over a given period of time.
        struct State {
            /// @brief The time the state begins, in 30 fps frames.
            int16_t Start = 0;
            /// @brief The duration of the state, in 30 fps frames.
            int16_t Duration = 0;
            /// @brief The strength of the motor at the beginning and end of the state.
            uint8_t BeginStrength = 0;
            /// @brief The strength of the motor at the end of the state.
            uint8_t EndStrength = 0;
        };

        /// @brief A controller rumble effect.
        struct Rumble {
            /// @brief Sequences of states for the heavy and light rumble motors.
            std::vector<State> HeavyStates;
            /// @brief A sequence of states for the light rumble motor.
            std::vector<State> LightStates;
        };

        /// @brief Whether the file is big-endian.
        ///
        /// True for PS3 and X360, false otherwise.
        bool BigEndian = false;
        /// @brief Available effects; always 256 in vanilla files.
        std::vector<Rumble> Rumbles;

    protected:
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
