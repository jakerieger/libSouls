// Copyright (C) 2026 Jake Rieger
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, version 3. This program is distributed in the hope that it will
// be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU General
// Public License along with this program. If not, see https://www.gnu.org/licenses/.

#include "ArchiveSet.hpp"

#include <libSouls/Crypto.hpp>
#include <libSouls/Util.hpp>

#include <fstream>

namespace fs = std::filesystem;

namespace Souls {
    namespace {
        std::vector<uint8_t> ReadAll(const fs::path& Path) {
            std::ifstream File(Path, std::ios::binary);
            if (!File) throw BinaryException("Couldn't open " + Path.string());
            return {std::istreambuf_iterator<char>(File), std::istreambuf_iterator<char>()};
        }
    }  // namespace

    ArchiveSet ArchiveSet::Open(const fs::path& GameFolder, BHD5::Game Game, std::span<const Source> Sources) {
        ArchiveSet Result;
        Result._Format = Game;
        for (const Source& S : Sources) {
            const fs::path Bhd = GameFolder / (S.Name + ".bhd");
            const fs::path Bdt = GameFolder / (S.Name + ".bdt");
            if (!fs::exists(Bhd) || !fs::exists(Bdt)) continue;
            try {
                const std::vector<uint8_t> Raw    = ReadAll(Bhd);
                const std::vector<uint8_t> Header = S.KeyPem.empty() ? Raw : Crypto::DecryptRsaBlocks(S.KeyPem, Raw);
                const BHD5 Bhd5                   = BHD5::Read(Header, Game);
                for (const auto& Bucket : Bhd5.Buckets) {
                    for (const auto& File : Bucket)
                        Result._ByHash.emplace(File.FileNameHash, Located {Result._Archives.size(), File});
                }
            } catch (const std::exception& E) {
                throw BinaryException("Couldn't read the " + S.Name + " archive header: " + E.what());
            }
            Result._Archives.push_back({S.Name, std::make_unique<BinaryReader>(Bdt), std::make_unique<std::mutex>()});
        }
        return Result;
    }

    ArchiveSet ArchiveSet::OpenWithKeys(const fs::path& GameFolder, BHD5::Game Game, const fs::path& KeyFolder) {
        std::vector<Source> Sources;
        if (fs::is_directory(KeyFolder)) {
            for (const fs::directory_entry& Entry : fs::directory_iterator(KeyFolder)) {
                if (!Entry.is_regular_file() || Entry.path().extension() != ".pem") continue;
                const std::vector<uint8_t> Key = ReadAll(Entry.path());
                Sources.push_back({Entry.path().stem().string(), std::string(Key.begin(), Key.end())});
            }
        }
        return Open(GameFolder, Game, Sources);
    }

    uint64_t ArchiveSet::Hash(std::string_view Path) const {
        // Elden Ring names files by a 64-bit hash; the earlier games by the 32-bit one.
        return _Format == BHD5::Game::EldenRing ? Util::FromPathHash64(Path) : Util::FromPathHash(Path);
    }

    bool ArchiveSet::Contains(std::string_view Path) const {
        return _ByHash.count(Hash(Path)) != 0;
    }

    std::optional<std::vector<uint8_t>> ArchiveSet::Read(std::string_view Path) const {
        const auto Found = _ByHash.find(Hash(Path));
        if (Found == _ByHash.end()) return std::nullopt;
        const Archive& In = _Archives[Found->second.Archive];
        std::lock_guard Guard(*In.Lock);
        return Found->second.File.ReadFile(*In.Data);
    }

    std::vector<std::string> ArchiveSet::ArchiveNames() const {
        std::vector<std::string> Names;
        for (const Archive& A : _Archives)
            Names.push_back(A.Name);
        return Names;
    }
}  // namespace Souls
