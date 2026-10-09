//
// Created by Jake Rieger on 10/8/2026.
//

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

    // EMEVD: a list of game area logic events, each with a script. Extension: .evd, .emevd
    class SOULS_API EMEVD : public SoulsFile<EMEVD> {
    public:
        // Possible configurations for EMEVD formatting.
        enum class Game {
            DarkSouls1,    // and Dark Souls II on PC
            DarkSouls1BE,  // Dark Souls and Dark Souls II on PS3 and Xbox 360
            Bloodborne,    // and Dark Souls II: Scholar of the First Sin on all platforms
            DarkSouls3,
            Sekiro,        // and Elden Ring
        };

        // Value type of an instruction argument.
        enum class ArgType {
            Byte   = 0,
            UInt16 = 1,
            UInt32 = 2,
            SByte  = 3,
            Int16  = 4,
            Int32  = 5,
            Single = 6,
        };

        // One argument value, held as the type it was packed as.
        using ArgValue = std::variant<uint8_t, uint16_t, uint32_t, int8_t, int16_t, int32_t, float>;

        // A single instruction to be executed by an event.
        struct SOULS_API Instruction {
            // The bank from which to select the instruction.
            int32_t Bank = 0;
            // The ID of this instruction within the bank.
            int32_t ID = 0;
            // Arguments provided to the instruction, as a raw block of bytes.
            std::vector<uint8_t> ArgData;
            // An optional value that causes the instruction to only run in certain ceremonies.
            std::optional<uint32_t> Layer;

            Instruction() = default;
            Instruction(int32_t Bank, int32_t ID) : Bank(Bank), ID(ID) {}
            Instruction(int32_t Bank, int32_t ID, std::vector<uint8_t> Args) : Bank(Bank), ID(ID), ArgData(std::move(Args)) {}
            Instruction(int32_t Bank, int32_t ID, uint32_t LayerMask, std::vector<uint8_t> Args)
                : Bank(Bank), ID(ID), ArgData(std::move(Args)), Layer(LayerMask) {}

            // Packs argument values into ArgData, aligning each to its size and padding the end to 4 bytes.
            void PackArgs(const std::vector<ArgValue>& Args, bool BigEndian = false);
            // Unpacks ArgData according to the given structure.
            std::vector<ArgValue> UnpackArgs(const std::vector<ArgType>& ArgStruct, bool BigEndian = false) const;
        };

        // An instruction to the game to substitute arg bytes in a particular instruction with ones defined here.
        struct Parameter {
            // The index into the event's instruction list for which to apply the substitution.
            int64_t InstructionIndex = 0;
            // Index of the starting byte in the instruction's arguments.
            int64_t TargetStartByte = 0;
            // Index of the starting byte in the event's parameters.
            int64_t SourceStartByte = 0;
            // Amount of bytes to copy to the target instruction's arguments.
            int32_t ByteCount = 0;
            // Always 0 before Sekiro, generally counts up from 1 for each parameter in an event.
            int32_t UnkID = 0;
        };

        // An event containing instructions to be executed.
        struct SOULS_API Event {
            // Defines the behavior of the event when resting.
            enum class RestBehaviorType : uint32_t {
                Default = 0,  // no effect upon resting
                Restart = 1,  // event restarts upon resting
                End     = 2,  // event is terminated upon resting
            };

            int64_t ID = 0;
            std::vector<Instruction> Instructions;
            // Parameters to be passed to this event.
            std::vector<Parameter> Parameters;
            RestBehaviorType RestBehavior = RestBehaviorType::Default;
            // Optional name for the event, stored separately in an EMELD file.
            std::optional<std::string> Name;

            Event() = default;
            Event(int64_t ID, RestBehaviorType RestBehavior = RestBehaviorType::Default) : ID(ID), RestBehavior(RestBehavior) {}
        };

        Game Format = Game::DarkSouls1;
        std::vector<Event> Events;
        // Offsets in the string data to linked file names used in Bloodborne and Dark Souls III.
        std::vector<int64_t> LinkedFileOffsets;
        // Raw string data referenced by linked files and some instructions.
        std::vector<uint8_t> StringData;

        // An empty EMEVD formatted for Dark Souls.
        EMEVD() = default;
        explicit EMEVD(Game Format) : Format(Format) {}

        // Imports event names from an EMELD file, overwriting existing names if specified.
        void ImportEMELD(const EMELD& Eld, bool Overwrite = false);
        // Exports event names to an EMELD file.
        EMELD ExportEMELD() const;

    protected:
        bool IsImpl(BinaryReader& Reader) override;
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
