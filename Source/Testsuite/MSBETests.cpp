//
// Created by Jake Rieger on 10/11/2026.
//

#include "TestPaths.hpp"
#include "TestResult.hpp"

#include <libSouls/BinaryReader.hpp>
#include <libSouls/Crypto.hpp>
#include <libSouls/Formats/DCX.hpp>
#include <libSouls/Formats/BHD5.hpp>
#include <libSouls/Formats/MSB/MSBE.hpp>
#include <libSouls/Oodle26.hpp>
#include <libSouls/Util.hpp>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

using namespace Souls;
namespace fs = std::filesystem;

namespace {
    std::vector<uint8_t> ReadAll(const fs::path& Path) {
        std::ifstream File(Path, std::ios::binary);
        return {std::istreambuf_iterator<char>(File), std::istreambuf_iterator<char>()};
    }

    struct Located {
        size_t Archive;
        BHD5::FileHeader File;
    };

    // The map names Elden Ring uses: legacy dungeons and caves (mAA_BB_CC_DD), and the overworld tiles of the base
    // game and the DLC at their three levels of detail.
    std::vector<std::string> CandidateMaps() {
        std::vector<std::string> Names;
        char Buffer[32];
        for (int A = 10; A < 60; ++A) {
            for (int B = 0; B <= 20; ++B) {
                for (int C = 0; C <= 1; ++C) {
                    for (int D = 0; D <= 1; ++D) {
                        std::snprintf(Buffer, sizeof Buffer, "m%02d_%02d_%02d_%02d", A, B, C, D);
                        Names.emplace_back(Buffer);
                    }
                }
            }
        }
        for (int A : {60, 61}) {
            for (int X = 0; X < 100; ++X) {
                for (int Y = 0; Y < 100; ++Y) {
                    for (int L : {0, 1, 2, 10, 11, 12}) {
                        std::snprintf(Buffer, sizeof Buffer, "m%02d_%02d_%02d_%02d", A, X, Y, L);
                        Names.emplace_back(Buffer);
                    }
                }
            }
        }
        return Names;
    }
}  // namespace

// Reads Elden Ring maps straight out of the game's archives, and checks that writing them back gives the same bytes.
int RunMSBETests(size_t MaxFiles) {
    const fs::path Game = "C:/Program Files (x86)/Steam/steamapps/common/ELDEN RING/Game";
    if (!fs::exists(Game / "eldenring.exe") || !fs::exists(Game / "oo2core_6_win64.dll")) {
        std::printf("MSBE checks skipped (needs an Elden Ring install)\n");
        return TestSkipped;
    }
    Oodle26::SetDLLPath(Game / "oo2core_6_win64.dll");
    // Header keys from BinderKeys, with the ones found in the exe as a fallback.
    const std::vector<std::string> ExeKeys = Crypto::FindRsaPublicKeys(ReadAll(Game / "eldenring.exe"));

    std::vector<fs::path> Data;
    std::unordered_map<uint64_t, Located> ByHash;
    for (const auto& Entry : fs::directory_iterator(Game)) {
        if (Entry.path().extension() != ".bhd") continue;
        const std::vector<uint8_t> Encrypted = ReadAll(Entry.path());
        std::vector<std::string> Keys;
        if (const fs::path Pem = TestPaths::BinderKeys() / "EldenRing_PC/Key" / (Entry.path().stem().string() + ".pem"); fs::exists(Pem)) {
            const std::vector<uint8_t> Text = ReadAll(Pem);
            Keys.emplace_back(Text.begin(), Text.end());
        }
        Keys.insert(Keys.end(), ExeKeys.begin(), ExeKeys.end());
        for (const std::string& Key : Keys) {
            try {
                const auto Header = Crypto::DecryptRsaBlocks(Key, Encrypted);
                if (Header.size() < 4 || std::memcmp(Header.data(), "BHD5", 4) != 0) continue;
                const BHD5 Bhd = BHD5::Read(Header, BHD5::Game::EldenRing);
                for (const auto& Bucket : Bhd.Buckets) {
                    for (const auto& File : Bucket) ByHash.emplace(File.FileNameHash, Located{Data.size(), File});
                }
                Data.push_back(fs::path(Entry.path()).replace_extension(".bdt"));
                break;
            } catch (const std::exception&) {
            }
        }
    }
    if (ByHash.empty()) {
        std::printf("MSBE checks skipped (no archive header could be decrypted)\n");
        return TestSkipped;
    }

    std::vector<std::pair<std::string, Located>> Maps;
    for (const std::string& Name : CandidateMaps()) {
        const auto Found = ByHash.find(Util::FromPathHash64("/map/mapstudio/" + Name + ".msb.dcx"));
        // SOULS_MSBE_ONLY=<text> checks just the maps whose names contain the text.
        const char* Only = std::getenv("SOULS_MSBE_ONLY");
        if (Found != ByHash.end() && (!Only || Name.find(Only) != std::string::npos)) Maps.emplace_back(Name, Found->second);
    }
    std::printf("%zu maps found in the archives\n", Maps.size());
    if (Maps.size() > MaxFiles) {
        std::vector<std::pair<std::string, Located>> Picked;
        for (size_t I = 0; I < MaxFiles; ++I) Picked.push_back(Maps[I * Maps.size() / MaxFiles]);
        Maps = std::move(Picked);
    }

    int Failures = 0, Identical = 0;
    std::vector<std::unique_ptr<BinaryReader>> Readers;
    for (const fs::path& P : Data) Readers.push_back(std::make_unique<BinaryReader>(P));
    for (const auto& [Name, Where] : Maps) {
        try {
            const std::vector<uint8_t> Bytes = DCX::Decompress(Where.File.ReadFile(*Readers[Where.Archive]));
            MSBE Map                         = MSBE::Read(Bytes);
            Map.Compression                  = DCX::Type::None;
            const std::vector<uint8_t> Again = Map.Write();
            if (Again == Bytes) {
                ++Identical;
                continue;
            }
            size_t At = 0;
            while (At < Again.size() && At < Bytes.size() && Again[At] == Bytes[At]) ++At;
            std::printf("  %s differs: first at 0x%zX, sizes %zu vs %zu\n", Name.c_str(), At, Bytes.size(), Again.size());
            if (const char* Dump = std::getenv("SOULS_DUMP"); Dump && Failures == 0) {
                std::ofstream(fs::path(Dump) / "MSBE_orig.bin", std::ios::binary).write(reinterpret_cast<const char*>(Bytes.data()), static_cast<std::streamsize>(Bytes.size()));
                std::ofstream(fs::path(Dump) / "MSBE_new.bin", std::ios::binary).write(reinterpret_cast<const char*>(Again.data()), static_cast<std::streamsize>(Again.size()));
            }
            ++Failures;
        } catch (const std::exception& E) {
            std::printf("  %s FAILED: %s\n", Name.c_str(), E.what());
            ++Failures;
        }
    }
    std::printf("[MSBE] %zu maps: %d byte-identical, %d failed\n", Maps.size(), Identical, Failures);
    return Failures;
}
