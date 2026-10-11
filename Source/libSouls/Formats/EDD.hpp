// Copyright (C) 2026 Jake Rieger
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, version 3. This program is distributed in the hope that it will
// be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU General
// Public License along with this program. If not, see https://www.gnu.org/licenses/.

#pragma once

#include <libSouls/SoulsFile.hpp>

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of an exported class; only touched by this DLL's own code

    /// @brief EDD: a description format for ESDs, published only for Dark Souls II.
    ///
    /// It is not read by the game, and it can only be read here, not written. Extension: .edd
    class SOULS_API EDD : public SoulsFile<EDD> {
    public:
        /// @brief A description of a built-in function in this type of ESD.
        struct FunctionSpec {
            /// @brief ID used in ESD to call the function.
            int32_t ID = 0;
            /// @brief Description of the function.
            std::string Name;
            /// @brief Unknown.
            uint8_t Unk06 = 0;
            /// @brief Unknown.
            uint8_t Unk07 = 0;
        };

        /// @brief A data structure associated with conditions.
        ///
        /// It has no data in Dark Souls II.
        struct ConditionDesc {};

        /// @brief A description of a built-in command in this type of ESD.
        struct CommandSpec {
            /// @brief ID used in ESD to call the command.
            int64_t ID = 0;
            /// @brief Description of the command.
            std::string Name;
            /// @brief Unknown.
            int16_t Unk0E = 0;
        };

        /// @brief A description of a command used in a state of the ESD.
        struct CommandDesc {
            /// @brief Description text.
            ///
            /// This often matches the command specification text, but is sometimes overridden.
            std::string Name;
        };

        /// @brief A description of commands in the pass command block of a condition.
        ///
        /// The game appears to ignore the pass block if it only contains the "return" command, so this annotation is
        /// uncommon.
        struct PassCommandDesc {
            /// @brief Descriptions for the commands in the pass block.
            std::vector<CommandDesc> PassCommands;
        };

        /// @brief A description of a state defined in the ESD.
        struct StateDesc {
            /// @brief ID of the state.
            int64_t ID = 0;
            /// @brief Description text.
            std::string Name;
            /// @brief Descriptions for commands in the entry block.
            std::vector<CommandDesc> EntryCommands;
            /// @brief Descriptions for commands in the exit block.
            std::vector<CommandDesc> ExitCommands;
            /// @brief Descriptions for commands in the while block.
            std::vector<CommandDesc> WhileCommands;
            /// @brief Descriptions for commands in conditions' pass blocks when nontrivial.
            std::vector<PassCommandDesc> PassCommands;
            /// @brief Descriptions for conditions. Doesn't contain anything interesting.
            std::vector<ConditionDesc> Conditions;
        };

        /// @brief A description of a machine defined in the ESD.
        struct MachineDesc {
            /// @brief ID of the machine.
            int32_t ID = 0;
            /// @brief Text description.
            std::string Name;
            /// @brief Unknown.
            int16_t Unk06 = 0;
            /// @brief Text description of params to the machine, when it is callable by other machines.
            std::array<std::optional<std::string>, 8> ParamNames;
            /// @brief Descriptions of the machine's states.
            std::vector<StateDesc> States;
        };

        /// @brief Whether the EDD is in 64-bit or 32-bit format.
        bool LongFormat = false;
        /// @brief Descriptions of built-in functions which can be used in the ESD file.
        std::vector<FunctionSpec> FunctionSpecs;
        /// @brief Descriptions of built-in commands which can be used in the ESD file.
        std::vector<CommandSpec> CommandSpecs;
        /// @brief Descriptions of machines and states defined in the ESD file.
        std::vector<MachineDesc> Machines;
        /// @brief Unknown.
        int32_t Unk80 = 0;
        /// @brief Unknown.
        std::array<int32_t, 4> UnkB0 {};

    protected:
        bool IsImpl(BinaryReader& Reader) override;
        void ReadImpl(BinaryReader& Reader) override;
    };

#pragma warning(pop)

}  // namespace Souls
