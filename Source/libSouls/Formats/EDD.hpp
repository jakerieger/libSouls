//
// Created by Jake Rieger on 10/8/2026.
//

#pragma once

#include <libSouls/SoulsFile.hpp>

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of an exported class; only touched by this DLL's own code

    // EDD: a description format for ESDs, published only for Dark Souls II. It is not read by the game, and it can
    // only be read here, not written. Extension: .edd
    class SOULS_API EDD : public SoulsFile<EDD> {
    public:
        // A description of a built-in function in this type of ESD.
        struct FunctionSpec {
            // ID used in ESD to call the function.
            int32_t ID = 0;
            // Description of the function.
            std::string Name;
            uint8_t Unk06 = 0;
            uint8_t Unk07 = 0;
        };

        // A data structure associated with conditions. It has no data in Dark Souls II.
        struct ConditionDesc {};

        // A description of a built-in command in this type of ESD.
        struct CommandSpec {
            // ID used in ESD to call the command.
            int64_t ID = 0;
            // Description of the command.
            std::string Name;
            int16_t Unk0E = 0;
        };

        // A description of a command used in a state of the ESD.
        struct CommandDesc {
            // Description text. This often matches the command specification text, but is sometimes overridden.
            std::string Name;
        };

        // A description of commands in the pass command block of a condition. The game appears to ignore the pass
        // block if it only contains the "return" command, so this annotation is uncommon.
        struct PassCommandDesc {
            std::vector<CommandDesc> PassCommands;
        };

        // A description of a state defined in the ESD.
        struct StateDesc {
            int64_t ID = 0;
            std::string Name;
            std::vector<CommandDesc> EntryCommands, ExitCommands, WhileCommands;
            // Descriptions for commands in conditions' pass blocks when nontrivial.
            std::vector<PassCommandDesc> PassCommands;
            std::vector<ConditionDesc> Conditions;
        };

        // A description of a machine defined in the ESD.
        struct MachineDesc {
            int32_t ID = 0;
            std::string Name;
            int16_t Unk06 = 0;
            // Text description of params to the machine, when it is callable by other machines.
            std::array<std::optional<std::string>, 8> ParamNames;
            std::vector<StateDesc> States;
        };

        // Whether the EDD is in 64-bit or 32-bit format.
        bool LongFormat = false;
        std::vector<FunctionSpec> FunctionSpecs;
        std::vector<CommandSpec> CommandSpecs;
        std::vector<MachineDesc> Machines;
        int32_t Unk80 = 0;
        std::array<int32_t, 4> UnkB0{};

    protected:
        bool IsImpl(BinaryReader& Reader) override;
        void ReadImpl(BinaryReader& Reader) override;
    };

#pragma warning(pop)

}  // namespace Souls
