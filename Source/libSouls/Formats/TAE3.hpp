// Copyright (C) 2026 Jake Rieger
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, version 3. This program is distributed in the hope that it will
// be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU General
// Public License along with this program. If not, see https://www.gnu.org/licenses/.

#pragma once

#include "TAE3Events.hpp"

#include <libSouls/SoulsFile.hpp>

#include <array>
#include <string>
#include <vector>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of an exported class; only touched by this DLL's own code

    /// @brief TAE3: controls when different events happen during animations; this specific version is used in Dark
    ///        Souls III.
    ///
    /// Extension: .tae. The event types and their fields are in TAE3Events.hpp.
    class SOULS_API TAE3 : public SoulsFile<TAE3> {
    public:
        /// @brief Shorthand for TAE3EventType.
        using EventType = TAE3EventType;

        /// @brief An action or effect triggered at a certain time during an animation.
        struct Event {
            /// @brief When the event begins and ends, in seconds.
            float StartTime = 0;
            /// @brief When the event ends.
            float EndTime = 0;
            /// @brief The type and the type's fields.
            ///
            /// Used for Dark Souls III (version 0x1000C) files.
            TAE3EventData Data;

            /// @brief Files of version 0x1000D (Sekiro and Elden Ring) keep each event's fields as raw bytes instead,
            ///        because the layouts of their event types aren't known to the library.
            ///
            /// RawData holds everything after the event's type, up to the start of the next thing in the file (so it
            /// includes any alignment padding), and Data is not used. Changing an event's type or length changes the
            /// layout of the file after it.
            bool IsRaw = false;
            /// @brief The event's type number, for raw events.
            uint64_t RawType = 0;
            /// @brief The event's fields as bytes, for raw events (see IsRaw).
            std::vector<uint8_t> RawData;
            /// @brief The file stores 0 instead of the offset of the fields (Sekiro does for some event types); kept so
            ///        the file writes back unchanged.
            bool RawNoFieldsOffset = false;

            /// @brief The event's type (RawType for raw events).
            EventType Type() const { return IsRaw ? static_cast<EventType>(RawType) : TAE3EventTypeOf(Data); }
            /// @brief The type number as stored in the file, even for types the library has no struct for.
            uint64_t TypeNumber() const { return static_cast<uint64_t>(Type()); }
        };

        /// @brief A group of events in an animation with an associated EventType that does not necessarily match
        ///        theirs.
        struct EventGroup {
            /// @brief Unknown.
            EventType Type = EventType::JumpTable;
            /// @brief Indices of events in this group in the parent animation's collection.
            std::vector<int32_t> Indices;
        };

        /// @brief Controls an individual animation.
        struct Animation {
            /// @brief ID number of this animation.
            int64_t ID = 0;
            /// @brief Timed events in this animation.
            std::vector<Event> Events;
            /// @brief Unknown groups of events.
            std::vector<EventGroup> EventGroups;
            /// @brief Unknown.
            bool AnimFileReference = false;
            /// @brief Unknown.
            int32_t AnimFileUnk18 = 0;
            /// @brief Unknown.
            int32_t AnimFileUnk1C = 0;
            /// @brief Unknown.
            std::string AnimFileName;
        };

        /// @brief ID number of this TAE.
        ///
        /// 0x1000C for Dark Souls III, 0x1000D for Sekiro and Elden Ring.
        int32_t Version = 0x1000C;
        /// @brief ID number of this TAE.
        int32_t ID = 0;
        /// @brief Unknown flags.
        std::array<uint8_t, 8> Flags {};
        /// @brief Unknown .hkt file.
        std::string SkeletonName;
        /// @brief Unknown .sib file.
        std::string SibName;
        /// @brief False for files that store neither name (some Sekiro ones do), which also leaves out the space after
        ///        the header; SkeletonName and SibName are then ignored when writing.
        bool HasNames = true;
        /// @brief Animations controlled by this TAE.
        std::vector<Animation> Animations;
        /// @brief Unknown; chr tae: 0x15; obj tae: 0x4, 0x8, 0xE, 0xF, 0x10, 0x12, 0x13, 0x14, 0x15; mov tae: 0x4.
        int64_t Unk30 = 0;

    protected:
        bool IsImpl(BinaryReader& Reader) override;
        void ReadImpl(BinaryReader& Reader) override;
        /// @brief Sorts Animations by ID first (the file format requires it).
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
