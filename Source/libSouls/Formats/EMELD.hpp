//
// Created by Jake Rieger on 10/8/2026.
//

#pragma once

#include "EMEVD.hpp"

#include <libSouls/SoulsFile.hpp>

#include <string>
#include <vector>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of an exported class; only touched by this DLL's own code

    // EMELD: a companion file to EMEVD that assigns names to different events. Extension: .eld, .emeld
    class SOULS_API EMELD : public SoulsFile<EMELD> {
    public:
        // Assigns a name to a certain event ID.
        struct Event {
            int64_t ID = 0;
            std::string Name;
        };

        // Determines the format the EMELD will be written in.
        EMEVD::Game Format = EMEVD::Game::DarkSouls1;
        // Events corresponding to those in the EMEVD.
        std::vector<Event> Events;

        EMELD() = default;
        explicit EMELD(EMEVD::Game Format) : Format(Format) {}

    protected:
        bool IsImpl(BinaryReader& Reader) override;
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
