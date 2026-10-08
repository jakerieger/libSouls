//
// Created by Jake Rieger on 10/7/2026.
//

#include "TestResult.hpp"

#include <libSouls/Formats/DCX.hpp>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <map>
#include <string>

namespace fs = std::filesystem;

// Runs DCX against real game files if the Elden Ring install (same path Oodle26 uses) is present. Samples
// evenly across the tree so it stays quick. Returns the number of failures, or TestSkipped if there's no install.
int RunRealFileTests(size_t MaxFiles) {
    using namespace Souls;

    const fs::path Root = "C:/Program Files (x86)/Steam/steamapps/common/ELDEN RING/Game";
    if (!fs::exists(Root)) {
        std::printf("Real file tests skipped (no game install at %s)\n", Root.string().c_str());
        return 0;
    }

    std::vector<fs::path> Files;
    for (const auto& Entry : fs::recursive_directory_iterator(Root, fs::directory_options::skip_permission_denied)) {
        if (Entry.is_regular_file() && Entry.path().extension() == ".dcx") {
            Files.push_back(Entry.path());
        }
    }
    std::sort(Files.begin(), Files.end());

    const size_t Step = std::max<size_t>(1, Files.size() / MaxFiles);
    std::map<DCX::Type, int> Histogram;
    int Failures = 0, Tested = 0, HeaderMismatches = 0;

    for (size_t I = 0; I < Files.size(); I += Step) {
        const fs::path& Path = Files[I];
        ++Tested;
        try {
            BinaryReader Reader(Path);
            std::vector<uint8_t> Original = Reader.ReadBytes(static_cast<size_t>(Reader.Length()));

            DCX::Type Type = DCX::Type::Unknown;
            const auto Data = DCX::Decompress(Original, Type);
            ++Histogram[Type];
            if (Data.empty()) {
                throw BinaryException("decompressed to nothing");
            }

            // Recompress with the detected type, then decompress again.
            const auto Packed = DCX::Compress(Data, Type);
            if (DCX::Decompress(Packed) != Data) {
                throw BinaryException("recompressed data does not round-trip");
            }
            // The fixed headers (everything except the size fields) should match what the game wrote.
            if (Type != DCX::Type::DCX_EDGE && Original.size() >= 0x4C && Packed.size() >= 0x4C) {
                auto Same = [&](size_t From, size_t To) {
                    return std::equal(Original.begin() + From, Original.begin() + To, Packed.begin() + From);
                };
                // Skip the DCS uncompressed/compressed sizes at 0x1C..0x24.
                if (!Same(0, 0x1C) || !Same(0x24, 0x4C)) {
                    ++HeaderMismatches;
                    std::printf("HEADER MISMATCH %s\n", Path.string().c_str());
                }
            }
        } catch (const BinaryException& E) {
            ++Failures;
            std::printf("FAIL %s: %s\n", Path.string().c_str(), E.what());
        }
    }

    std::printf("Real DCX files: %d sampled of %zu, %d failed, %d header mismatches\n",
                Tested, Files.size(), Failures, HeaderMismatches);
    for (const auto& [Type, Count] : Histogram) {
        std::printf("  type %d: %d\n", static_cast<int>(Type), Count);
    }
    return Failures + HeaderMismatches;
}
