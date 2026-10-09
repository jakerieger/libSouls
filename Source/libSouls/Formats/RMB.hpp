//
// Created by Jake Rieger on 10/8/2026.
//

#pragma once

#include <libSouls/SoulsFile.hpp>

#include <vector>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of an exported class; only touched by this DLL's own code

    // RMB: a collection of controller rumble effects used in all games. Extension: .rmb
    class SOULS_API RMB : public SoulsFile<RMB> {
    public:
        // Defines a sweep of rumble motor strength over a given period of time.
        struct State {
            // The time the state begins, in 30 fps frames.
            int16_t Start = 0;
            // The duration of the state, in 30 fps frames.
            int16_t Duration = 0;
            // The strength of the motor at the beginning and end of the state.
            uint8_t BeginStrength = 0;
            uint8_t EndStrength   = 0;
        };

        // A controller rumble effect.
        struct Rumble {
            // Sequences of states for the heavy and light rumble motors.
            std::vector<State> HeavyStates;
            std::vector<State> LightStates;
        };

        // Whether the file is big-endian. True for PS3 and X360, false otherwise.
        bool BigEndian = false;
        // Available effects; always 256 in vanilla files.
        std::vector<Rumble> Rumbles;

    protected:
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
