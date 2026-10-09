//
// Created by Jake Rieger on 10/8/2026.
//

#pragma once

#include <libSouls/SoulsFile.hpp>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of an exported class; only touched by this DLL's own code

    // ESD: a state machine used for gameplay, menus, and dialog throughout the series. Extension: .esd
    class SOULS_API ESD : public SoulsFile<ESD> {
    public:
        // A function to be called when certain conditions are met.
        struct CommandCall {
            // Unknown. Speculation: some kind of command bank a la emevd. Should be 1, 5, 6, or 7.
            int32_t CommandBank = 1;
            // ID of the command to be executed.
            int32_t CommandID = 0;
            // Bytecode expressions to evaluate and pass as arguments to the command.
            std::vector<std::vector<uint8_t>> Arguments;
        };

        // Represents a transition between states when certain conditions are met. Conditions form a graph: the same
        // Condition object can be shared by several states or subcondition lists (compared by identity).
        struct Condition {
            // The ID of the state to enter if the condition passes, or nullopt if subconditions are present.
            std::optional<int64_t> TargetState;
            // Commands to be executed if the condition passes.
            std::vector<CommandCall> PassCommands;
            // If present and this condition passes, evaluation will continue to these conditions.
            std::vector<std::shared_ptr<Condition>> Subconditions;
            // Bytecode which determines whether the condition passes.
            std::vector<uint8_t> Evaluator;
        };

        // A node in the state graph.
        struct State {
            // Possible transitions to other states.
            std::vector<std::shared_ptr<Condition>> Conditions;
            // Commands to be executed when the state is entered.
            std::vector<CommandCall> EntryCommands;
            // Commands to be executed when the state is exited.
            std::vector<CommandCall> ExitCommands;
            // Unknown. Speculation: commands to be executed constantly while in the state.
            std::vector<CommandCall> WhileCommands;
        };

        // If true, write in 64-bit format; if false, write in 32-bit format.
        bool LongFormat = false;
        // 1 for Dark Souls/Remastered, 2 for Dark Souls II/SotFS/Bloodborne, 3 for Dark Souls III and later.
        int32_t DarkSoulsCount = 1;
        // Name and/or brief description of the file, if present.
        std::optional<std::string> Name;
        // Unknown; not bytecode, not floats, not text. Perhaps a hash of something, but if so it is not checked.
        int32_t Unk70 = 0, Unk74 = 0, Unk78 = 0, Unk7C = 0;
        // State groups indexed by their ID, containing individual states indexed by their IDs.
        std::map<int64_t, std::map<int64_t, State>> StateGroups;

        // An empty ESD formatted for Dark Souls.
        ESD() = default;
        ESD(bool LongFormat, int32_t DarkSoulsCount) : LongFormat(LongFormat), DarkSoulsCount(DarkSoulsCount) {}

    protected:
        bool IsImpl(BinaryReader& Reader) override;
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
