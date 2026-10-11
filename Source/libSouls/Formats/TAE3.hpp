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
            // The type and the type's fields. Used for Dark Souls III (version 0x1000C) files.
            TAE3EventData Data;

            // Files of version 0x1000D (Sekiro and Elden Ring) keep each event's fields as raw bytes instead, because
            // the layouts of their event types aren't known to the library. RawData holds everything after the event's
            // type, up to the start of the next thing in the file (so it includes any alignment padding), and Data is
            // not used. Changing an event's type or length changes the layout of the file after it.
            bool IsRaw = false;
            uint64_t RawType = 0;
            std::vector<uint8_t> RawData;
            // The file stores 0 instead of the offset of the fields (Sekiro does for some event types); kept so the
            // file writes back unchanged.
            bool RawNoFieldsOffset = false;

            EventType Type() const { return IsRaw ? static_cast<EventType>(RawType) : TAE3EventTypeOf(Data); }
            // The type number as stored in the file, even for types the library has no struct for.
            uint64_t TypeNumber() const { return static_cast<uint64_t>(Type()); }
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
        // 0x1000C for Dark Souls III, 0x1000D for Sekiro and Elden Ring.
        int32_t Version = 0x1000C;
        int32_t ID = 0;
        // Unknown flags.
        std::array<uint8_t, 8> Flags{};
        // Unknown .hkt file.
        std::string SkeletonName;
        // Unknown .sib file.
        std::string SibName;
        // False for files that store neither name (some Sekiro ones do), which also leaves out the space after the
        // header; SkeletonName and SibName are then ignored when writing.
        bool HasNames = true;
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
