// Copyright (C) 2026 Jake Rieger
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, version 3. This program is distributed in the hope that it will
// be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU General
// Public License along with this program. If not, see https://www.gnu.org/licenses/.

#pragma once

#include "TestPaths.hpp"

#include <libSouls/Binders/ArchiveSet.hpp>
#include <libSouls/Formats/DCX.hpp>
#include <libSouls/Oodle26.hpp>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

// Elden Ring's packed archives, opened with the header keys from BinderKeys, for reading game files by path.
class EldenRingArchives {
public:
    static constexpr const char* GameDir = "C:/Program Files (x86)/Steam/steamapps/common/ELDEN RING/Game";

    // Opens every archive; IsOpen() is false without the game or the keys.
    EldenRingArchives() {
        namespace fs        = std::filesystem;
        const fs::path Game = GameDir;
        if (!fs::exists(Game / "oo2core_6_win64.dll")) return;
        Souls::Oodle26::SetDLLPath(Game / "oo2core_6_win64.dll");
        try {
            Archives = Souls::ArchiveSet::OpenWithKeys(
              Game, Souls::BHD5::Game::EldenRing, TestPaths::BinderKeys() / "EldenRing_PC" / "Key");
        } catch (const std::exception&) {}
    }

    bool IsOpen() const { return Archives.FileCount() != 0; }

    // A file by its path in the game ("/chr/c0000.anibnd.dcx"), decompressed; nullopt if it isn't there.
    std::optional<std::vector<uint8_t>> Read(const std::string& Path) const {
        std::optional<std::vector<uint8_t>> Bytes = Archives.Read(Path);
        if (Bytes && Souls::DCX::Is(*Bytes)) return Souls::DCX::Decompress(*Bytes);
        return Bytes;
    }

    bool Contains(const std::string& Path) const { return Archives.Contains(Path); }

private:
    Souls::ArchiveSet Archives;
};
