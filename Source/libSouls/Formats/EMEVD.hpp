// Copyright (C) 2026 Jake Rieger
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, version 3. This program is distributed in the hope that it will
// be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU General
// Public License along with this program. If not, see https://www.gnu.org/licenses/.

#pragma once

#include <libSouls/SoulsFile.hpp>

#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of an exported class; only touched by this DLL's own code

    class EMELD;

    /// @brief EMEVD: a list of game area logic events, each with a script.
    ///
    /// Extension: .evd, .emevd
    class SOULS_API EMEVD : public SoulsFile<EMEVD> {
    public:
        /// @brief Possible configurations for EMEVD formatting.
        enum class Game {
            DarkSouls1,    ///< and Dark Souls II on PC
            DarkSouls1BE,  ///< Dark Souls and Dark Souls II on PS3 and Xbox 360
            Bloodborne,    ///< and Dark Souls II: Scholar of the First Sin on all platforms
            DarkSouls3,
            Sekiro,  ///< and Elden Ring
        };

        /// @brief Value type of an instruction argument.
        enum class ArgType {
            Byte   = 0,
            UInt16 = 1,
            UInt32 = 2,
            SByte  = 3,
            Int16  = 4,
            Int32  = 5,
            Single = 6,
        };

        /// @brief One argument value, held as the type it was packed as.
        using ArgValue = std::variant<uint8_t, uint16_t, uint32_t, int8_t, int16_t, int32_t, float>;

        /// @brief A single instruction to be executed by an event.
        struct SOULS_API Instruction {
            /// @brief The bank from which to select the instruction.
            int32_t Bank = 0;
            /// @brief The ID of this instruction within the bank.
            int32_t ID = 0;
            /// @brief Arguments provided to the instruction, as a raw block of bytes.
            std::vector<uint8_t> ArgData;
            /// @brief An optional value that causes the instruction to only run in certain ceremonies.
            std::optional<uint32_t> Layer;

            Instruction() = default;
            /// @brief An instruction with no arguments.
            Instruction(int32_t Bank, int32_t ID) : Bank(Bank), ID(ID) {}
            /// @brief An instruction with packed argument data.
            Instruction(int32_t Bank, int32_t ID, std::vector<uint8_t> Args)
                : Bank(Bank), ID(ID), ArgData(std::move(Args)) {}
            /// @brief An instruction limited to some layers, with packed argument data.
            Instruction(int32_t Bank, int32_t ID, uint32_t LayerMask, std::vector<uint8_t> Args)
                : Bank(Bank), ID(ID), ArgData(std::move(Args)), Layer(LayerMask) {}

            /// @brief Packs argument values into ArgData, aligning each to its size and padding the end to 4 bytes.
            void PackArgs(const std::vector<ArgValue>& Args, bool BigEndian = false);
            /// @brief Unpacks ArgData according to the given structure.
            std::vector<ArgValue> UnpackArgs(const std::vector<ArgType>& ArgStruct, bool BigEndian = false) const;
        };

        /// @brief An instruction to the game to substitute arg bytes in a particular instruction with ones defined
        ///        here.
        struct Parameter {
            /// @brief The index into the event's instruction list for which to apply the substitution.
            int64_t InstructionIndex = 0;
            /// @brief Index of the starting byte in the instruction's arguments.
            int64_t TargetStartByte = 0;
            /// @brief Index of the starting byte in the event's parameters.
            int64_t SourceStartByte = 0;
            /// @brief Amount of bytes to copy to the target instruction's arguments.
            int32_t ByteCount = 0;
            /// @brief Always 0 before Sekiro, generally counts up from 1 for each parameter in an event.
            int32_t UnkID = 0;
        };

        /// @brief An event containing instructions to be executed.
        struct SOULS_API Event {
            /// @brief Defines the behavior of the event when resting.
            enum class RestBehaviorType : uint32_t {
                Default = 0,  ///< no effect upon resting
                Restart = 1,  ///< event restarts upon resting
                End     = 2,  ///< event is terminated upon resting
            };

            /// @brief The ID of the event.
            int64_t ID = 0;
            /// @brief Instructions to execute for this event.
            std::vector<Instruction> Instructions;
            /// @brief Parameters to be passed to this event.
            std::vector<Parameter> Parameters;
            /// @brief Behavior of this event when resting.
            RestBehaviorType RestBehavior = RestBehaviorType::Default;
            /// @brief Optional name for the event, stored separately in an EMELD file.
            std::optional<std::string> Name;

            Event() = default;
            /// @brief An empty event with the given ID and rest behavior.
            Event(int64_t ID, RestBehaviorType RestBehavior = RestBehaviorType::Default)
                : ID(ID), RestBehavior(RestBehavior) {}
        };

        /// @brief Determines the format the EMEVD will be written in.
        Game Format = Game::DarkSouls1;
        /// @brief List of events in this EMEVD.
        std::vector<Event> Events;
        /// @brief Offsets in the string data to linked file names used in Bloodborne and Dark Souls III.
        std::vector<int64_t> LinkedFileOffsets;
        /// @brief Raw string data referenced by linked files and some instructions.
        std::vector<uint8_t> StringData;

        /// @brief An empty EMEVD formatted for Dark Souls.
        EMEVD() = default;
        /// @brief An empty EMEVD for the given game's layout.
        explicit EMEVD(Game Format) : Format(Format) {}

        /// @brief Imports event names from an EMELD file, overwriting existing names if specified.
        void ImportEMELD(const EMELD& Eld, bool Overwrite = false);
        /// @brief Exports event names to an EMELD file.
        EMELD ExportEMELD() const;

    protected:
        bool IsImpl(BinaryReader& Reader) override;
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
