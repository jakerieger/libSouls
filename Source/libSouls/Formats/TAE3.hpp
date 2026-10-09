//
// Created by Jake Rieger on 10/8/2026.
//

#pragma once

#include "TAE3Events.hpp"

#include <libSouls/SoulsFile.hpp>

#include <array>
#include <string>
#include <vector>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of an exported class; only touched by this DLL's own code

    // TAE3: controls when different events happen during animations; this specific version is used in Dark Souls
    // III. Extension: .tae. The event types and their fields are in TAE3Events.hpp.
    class SOULS_API TAE3 : public SoulsFile<TAE3> {
    public:
        using EventType = TAE3EventType;

        // An action or effect triggered at a certain time during an animation.
        struct Event {
            // When the event begins and ends, in seconds.
            float StartTime = 0;
            float EndTime   = 0;
            // The type and the type's fields.
            TAE3EventData Data;

            EventType Type() const { return TAE3EventTypeOf(Data); }
        };

        // A group of events in an animation with an associated EventType that does not necessarily match theirs.
        struct EventGroup {
            EventType Type = EventType::JumpTable;
            // Indices of events in this group in the parent animation's collection.
            std::vector<int32_t> Indices;
        };

        // Controls an individual animation.
        struct Animation {
            int64_t ID = 0;
            std::vector<Event> Events;
            // Unknown groups of events.
            std::vector<EventGroup> EventGroups;
            bool AnimFileReference = false;
            int32_t AnimFileUnk18  = 0;
            int32_t AnimFileUnk1C  = 0;
            std::string AnimFileName;
        };

        // ID number of this TAE.
        int32_t ID = 0;
        // Unknown flags.
        std::array<uint8_t, 8> Flags{};
        // Unknown .hkt file.
        std::string SkeletonName;
        // Unknown .sib file.
        std::string SibName;
        std::vector<Animation> Animations;
        // Unknown; chr tae: 0x15; obj tae: 0x4, 0x8, 0xE, 0xF, 0x10, 0x12, 0x13, 0x14, 0x15; mov tae: 0x4.
        int64_t Unk30 = 0;

    protected:
        bool IsImpl(BinaryReader& Reader) override;
        void ReadImpl(BinaryReader& Reader) override;
        // Sorts Animations by ID first (the file format requires it).
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
