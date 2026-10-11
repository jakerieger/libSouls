//
// Created by Jake Rieger on 10/11/2026.
//

#pragma once

#include "BND4.hpp"

#include <filesystem>
#include <span>
#include <vector>

// Reading and writing save files (.sl2). A save is a BND4 whose files (USER_DATA000, USER_DATA001, ...) each start with
// the MD5 hash of the rest of the file:
//
//     Dark Souls II, Scholar, Dark Souls III:  [MD5][16-byte IV][AES-128-CBC encrypted data]
//     Elden Ring:                              [MD5][data]              (not encrypted)
//
// Read gives back the binder with each file's Bytes replaced by its data (checked and decrypted); Write does the
// reverse. Edit copies of saves, not the originals: the game rejects a save whose hashes don't match, and a mistake
// can cost a character.
namespace Souls::SL2 {
    enum class Game {
        DarkSouls2,         // the original release
        DarkSouls2Scholar,  // Scholar of the First Sin
        DarkSouls3,
        EldenRing,
    };

    // Reads a save, checking every file's hash (throws BinaryException if one doesn't match) and decrypting it.
    SOULS_API BND4 Read(const std::filesystem::path& Path, Game For);
    SOULS_API BND4 Read(std::span<const uint8_t> Data, Game For);

    // Writes a save from a binder of decoded files: each is encrypted (with a new random IV) where the game does that,
    // and given its hash. Encrypted files must be a whole number of 16-byte blocks long, as the game's are.
    SOULS_API std::vector<uint8_t> Write(const BND4& Save, Game For);
    SOULS_API void Write(const std::filesystem::path& Path, const BND4& Save, Game For);

    // One file of a save, without the binder around it.
    SOULS_API std::vector<uint8_t> DecodeFile(std::span<const uint8_t> Stored, Game For);
    SOULS_API std::vector<uint8_t> EncodeFile(std::span<const uint8_t> Data, Game For);

    // The AES-128 key the game encrypts its save files with; empty for games that don't.
    SOULS_API std::span<const uint8_t> Key(Game For);
}  // namespace Souls::SL2
