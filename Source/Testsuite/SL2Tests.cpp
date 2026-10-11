//
// Created by Jake Rieger on 10/11/2026.
//

#include "TestResult.hpp"

#include <libSouls/Binders/SL2.hpp>
#include <libSouls/CApi/souls.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace Souls;
namespace fs = std::filesystem;

namespace {
    int Failures = 0;

#define CHECK(Cond)                                                     \
    do {                                                                \
        if (!(Cond)) {                                                  \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #Cond); \
            ++Failures;                                                 \
        }                                                               \
    } while (0)

    std::vector<uint8_t> Pattern(size_t Size, uint8_t Seed) {
        std::vector<uint8_t> Bytes(Size);
        for (size_t I = 0; I < Size; ++I) Bytes[I] = static_cast<uint8_t>(Seed + I * 7);
        return Bytes;
    }

    // Every game's save layout, on made-up data: written, read back, and rejected once damaged.
    void TestSynthetic() {
        for (SL2::Game Game : {SL2::Game::DarkSouls2, SL2::Game::DarkSouls2Scholar, SL2::Game::DarkSouls3, SL2::Game::EldenRing}) {
            BND4 Save;
            Save.Files.emplace_back(Binder::FileFlags::Flag1, "USER_DATA000", Pattern(0x100, 1));
            Save.Files.emplace_back(Binder::FileFlags::Flag1, "USER_DATA001", Pattern(0x40, 9));
            Save.Files[0].ID = 0;
            Save.Files[1].ID = 1;

            const std::vector<uint8_t> Written = SL2::Write(Save, Game);
            const BND4 Back                    = SL2::Read(Written, Game);
            CHECK(Back.Files.size() == 2 && Back.Files[0].Bytes == Save.Files[0].Bytes && Back.Files[1].Bytes == Save.Files[1].Bytes);
            CHECK(Back.Files[1].Name == "USER_DATA001");

            // Each stored file is the MD5 of the rest, then (for the Dark Souls games) an IV and the ciphertext.
            const std::vector<uint8_t> Stored = SL2::EncodeFile(Save.Files[0].Bytes, Game);
            const bool Encrypted              = Game != SL2::Game::EldenRing;
            CHECK(Stored.size() == 16 + (Encrypted ? 16 : 0) + 0x100);
            CHECK(SL2::Key(Game).size() == (Encrypted ? 16u : 0u));
            if (!Encrypted) {
                CHECK(std::equal(Save.Files[0].Bytes.begin(), Save.Files[0].Bytes.end(), Stored.begin() + 16));
            }

            // A changed byte breaks the hash, and the wrong game's key is caught by it too.
            std::vector<uint8_t> Damaged = Stored;
            Damaged.back() ^= 0xFF;
            bool Threw = false;
            try {
                SL2::DecodeFile(Damaged, Game);
            } catch (const BinaryException&) {
                Threw = true;
            }
            CHECK(Threw);
            if (Encrypted) {
                const SL2::Game Other = Game == SL2::Game::DarkSouls3 ? SL2::Game::DarkSouls2 : SL2::Game::DarkSouls3;
                CHECK(SL2::DecodeFile(Stored, Other) != Save.Files[0].Bytes);

                // Encrypted data must be whole AES blocks, as the game's is.
                Threw = false;
                try {
                    SL2::EncodeFile(Pattern(0x41, 0), Game);
                } catch (const BinaryException&) {
                    Threw = true;
                }
                CHECK(Threw);
            }
        }
    }

    // The first Elden Ring save found (or SOULS_SAVE_PATH). Only ever read: nothing is written to it.
    fs::path FindEldenRingSave() {
        if (const char* Env = std::getenv("SOULS_SAVE_PATH")) return Env;
        const char* AppData = std::getenv("APPDATA");
        if (!AppData || !fs::exists(fs::path(AppData) / "EldenRing")) return {};
        std::error_code Ec;
        for (const auto& Entry : fs::recursive_directory_iterator(fs::path(AppData) / "EldenRing", Ec)) {
            if (Entry.path().filename() == "ER0000.sl2") return Entry.path();
        }
        return {};
    }

    void TestEldenRingSave() {
        const fs::path Path = FindEldenRingSave();
        if (Path.empty() || !fs::exists(Path)) {
            std::printf("Real save checks skipped (no Elden Ring save found)\n");
            return;
        }
        std::ifstream File(Path, std::ios::binary);
        const std::vector<uint8_t> Original{std::istreambuf_iterator<char>(File), std::istreambuf_iterator<char>()};

        const BND4 Save = SL2::Read(Original, SL2::Game::EldenRing);
        std::printf("Elden Ring save: %zu files\n", Save.Files.size());
        CHECK(Save.Files.size() >= 11);
        if (Save.Files.size() >= 11) {
            CHECK(Save.Files[0].Name == "USER_DATA000" && Save.Files[0].Bytes.size() == 0x280000);
            CHECK(Save.Files[10].Name == "USER_DATA010");
        }
        // Nothing is encrypted, so writing an unchanged save gives back the same bytes.
        CHECK(SL2::Write(Save, SL2::Game::EldenRing) == Original);
        // Read as a Dark Souls III save, the hashes still match but decryption makes nonsense of the data.
        const BND4 Wrong = SL2::Read(Original, SL2::Game::DarkSouls3);
        CHECK(Wrong.Files[0].Bytes != Save.Files[0].Bytes);

        // The same through the C API.
        SoulsBinder* Binder = souls_save_read_memory(SOULS_SAVE_ELDEN_RING, Original.data(), Original.size());
        CHECK(Binder != nullptr);
        if (Binder) {
            size_t Size         = 0;
            const uint8_t* Data = souls_binder_file_bytes(Binder, 0, &Size);
            CHECK(Data && Size == Save.Files[0].Bytes.size() && std::memcmp(Data, Save.Files[0].Bytes.data(), Size) == 0);
            SoulsBuffer Again = souls_save_write_memory(SOULS_SAVE_ELDEN_RING, Binder);
            CHECK(Again.size == Original.size() && std::memcmp(Again.data, Original.data(), Again.size) == 0);
            souls_buffer_free(Again);
            souls_binder_free(Binder);
        }
        std::vector<uint8_t> Damaged = Original;
        Damaged[Damaged.size() / 2] ^= 0xFF;
        CHECK(souls_save_read_memory(SOULS_SAVE_ELDEN_RING, Damaged.data(), Damaged.size()) == nullptr && souls_last_error_code() == SOULS_ERR_PARSE);
        CHECK(souls_save_read_memory(static_cast<SoulsSaveKind>(99), Original.data(), Original.size()) == nullptr);
    }
}  // namespace

int RunSL2Tests() {
    Failures = 0;
    TestSynthetic();
    TestEldenRingSave();
    std::printf(Failures == 0 ? "SL2 tests passed\n" : "SL2 tests: %d failure(s)\n", Failures);
    return Failures;
}
