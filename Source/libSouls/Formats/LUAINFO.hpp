//
// Created by Jake Rieger on 10/8/2026.
//

#pragma once

#include <libSouls/SoulsFile.hpp>

#include <optional>
#include <string>
#include <vector>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of an exported class; only touched by this DLL's own code

    // LUAINFO: information about AI goals for Lua scripts. Extension: .luainfo
    class SOULS_API LUAINFO : public SoulsFile<LUAINFO> {
    public:
        // Goal information for AI scripts.
        struct SOULS_API Goal {
            int32_t ID = 0;
            std::string Name;
            // Whether to trigger a battle interrupt.
            bool BattleInterrupt = false;
            // Whether to trigger a logic interrupt.
            bool LogicInterrupt = false;
            // Function name of the logic interrupt, if present.
            std::optional<std::string> LogicInterruptName;

            Goal() = default;
            Goal(int32_t ID, std::string Name, bool BattleInterrupt, bool LogicInterrupt,
                 std::optional<std::string> LogicInterruptName = std::nullopt)
                : ID(ID), Name(std::move(Name)), BattleInterrupt(BattleInterrupt), LogicInterrupt(LogicInterrupt),
                  LogicInterruptName(std::move(LogicInterruptName)) {}
        };

        bool BigEndian = false;
        // If true, write with 64-bit offsets and UTF-16 strings.
        bool LongFormat = false;
        std::vector<Goal> Goals;

        LUAINFO() = default;
        LUAINFO(bool BigEndian, bool LongFormat) : BigEndian(BigEndian), LongFormat(LongFormat) {}

    protected:
        bool IsImpl(BinaryReader& Reader) override;
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
