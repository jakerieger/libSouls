// Copyright (C) 2026 Jake Rieger
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, version 3. This program is distributed in the hope that it will
// be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU General
// Public License along with this program. If not, see https://www.gnu.org/licenses/.

#pragma once

#include "TestPaths.hpp"

#include <libSouls/BinaryReader.hpp>
#include <libSouls/Crypto.hpp>
#include <libSouls/Formats/BHD5.hpp>
#include <libSouls/Formats/DCX.hpp>
#include <libSouls/Oodle26.hpp>
#include <libSouls/Util.hpp>

#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

// Elden Ring's packed archives, opened with the header keys from BinderKeys, for reading game files by path.
class EldenRingArchives {
public:
    static constexpr const char* GameDir = "C:/Program Files (x86)/Steam/steamapps/common/ELDEN RING/Game";

    // Opens every archive; IsOpen() is false without the game or the keys.
    EldenRingArchives() {
        namespace fs        = std::filesystem;
        const fs::path Game = GameDir;
        const fs::path Keys = TestPaths::BinderKeys() / "EldenRing_PC" / "Key";
        if (!fs::exists(Game / "oo2core_6_win64.dll")) return;
        Souls::Oodle26::SetDLLPath(Game / "oo2core_6_win64.dll");
        for (const char* Name : {"Data0", "Data1", "Data2", "Data3", "DLC"}) {
            const fs::path Pem = Keys / (std::string(Name) + ".pem");
            const fs::path Bhd = Game / (std::string(Name) + ".bhd");
            if (!fs::exists(Pem) || !fs::exists(Bhd)) continue;
            try {
                const std::vector<uint8_t> KeyText = ReadAll(Pem);
                const auto Header =
                  Souls::Crypto::DecryptRsaBlocks(std::string(KeyText.begin(), KeyText.end()), ReadAll(Bhd));
                const Souls::BHD5 Bhd5 = Souls::BHD5::Read(Header, Souls::BHD5::Game::EldenRing);
                for (const auto& Bucket : Bhd5.Buckets) {
                    for (const auto& File : Bucket)
                        ByHash.emplace(File.FileNameHash, Located {Readers.size(), File});
                }
                Readers.push_back(std::make_unique<Souls::BinaryReader>(Game / (std::string(Name) + ".bdt")));
            } catch (const std::exception&) {}
        }
    }

    bool IsOpen() const { return !ByHash.empty(); }

    // A file by its path in the game ("/chr/c0000.anibnd.dcx"), decompressed; nullopt if it isn't there.
    std::optional<std::vector<uint8_t>> Read(const std::string& Path) {
        const auto Found = ByHash.find(Souls::Util::FromPathHash64(Path));
        if (Found == ByHash.end()) return std::nullopt;
        std::vector<uint8_t> Bytes = Found->second.File.ReadFile(*Readers[Found->second.Archive]);
        return Souls::DCX::Is(Bytes) ? Souls::DCX::Decompress(Bytes) : Bytes;
    }

    bool Contains(const std::string& Path) const { return ByHash.count(Souls::Util::FromPathHash64(Path)) != 0; }

    static std::vector<uint8_t> ReadAll(const std::filesystem::path& Path) {
        std::ifstream File(Path, std::ios::binary);
        return {std::istreambuf_iterator<char>(File), std::istreambuf_iterator<char>()};
    }

private:
    struct Located {
        size_t Archive;
        Souls::BHD5::FileHeader File;
    };
    std::unordered_map<uint64_t, Located> ByHash;
    std::vector<std::unique_ptr<Souls::BinaryReader>> Readers;
};
