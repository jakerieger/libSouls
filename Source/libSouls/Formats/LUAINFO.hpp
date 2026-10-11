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
#include <vector>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of an exported class; only touched by this DLL's own code

    /// @brief LUAINFO: information about AI goals for Lua scripts.
    ///
    /// Extension: .luainfo
    class SOULS_API LUAINFO : public SoulsFile<LUAINFO> {
    public:
        /// @brief Goal information for AI scripts.
        struct SOULS_API Goal {
            /// @brief ID of this goal.
            int32_t ID = 0;
            /// @brief Name of this goal.
            std::string Name;
            /// @brief Whether to trigger a battle interrupt.
            bool BattleInterrupt = false;
            /// @brief Whether to trigger a logic interrupt.
            bool LogicInterrupt = false;
            /// @brief Function name of the logic interrupt, if present.
            std::optional<std::string> LogicInterruptName;

            Goal() = default;
            /// @brief A goal with the given settings.
            Goal(int32_t ID,
                 std::string Name,
                 bool BattleInterrupt,
                 bool LogicInterrupt,
                 std::optional<std::string> LogicInterruptName = std::nullopt)
                : ID(ID), Name(std::move(Name)), BattleInterrupt(BattleInterrupt), LogicInterrupt(LogicInterrupt),
                  LogicInterruptName(std::move(LogicInterruptName)) {}
        };

        /// @brief If true, write as big endian.
        bool BigEndian = false;
        /// @brief If true, write with 64-bit offsets and UTF-16 strings.
        bool LongFormat = false;
        /// @brief AI goals for a luabnd.
        std::vector<Goal> Goals;

        LUAINFO() = default;
        /// @brief An empty LUAINFO with the given layout settings.
        LUAINFO(bool BigEndian, bool LongFormat) : BigEndian(BigEndian), LongFormat(LongFormat) {}

    protected:
        bool IsImpl(BinaryReader& Reader) override;
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
