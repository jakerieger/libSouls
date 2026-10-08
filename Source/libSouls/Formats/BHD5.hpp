//
// Created by Jake Rieger on 10/7/2026.
//

#pragma once

#include <libSouls/Souls.hpp>
#include <libSouls/BinaryReader.hpp>
#include <libSouls/BinaryWriter.hpp>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of exported classes; only touched by this DLL's own code

    // The header file (Data0.bhd, DLC.bhd, ...) of the "dvdbnd" container that packages a game's files under hashed
    // names. The data lives in the matching .bdt file; each FileHeader says where. In most games the header file is
    // RSA-encrypted on disk and must be decrypted first (see Crypto::DecryptRsaBlocks).
    class SOULS_API BHD5 {
    public:
        // Which game's layout the file uses.
        enum class Game {
            DarkSouls1,  // PC and console
            DarkSouls2,  // PC, including Scholar of the First Sin
            DarkSouls3,  // PC, and Sekiro
            EldenRing,   // PC
        };

        // A byte range of a file, start inclusive and end exclusive.
        struct SOULS_API Range {
            int64_t StartOffset = 0;
            int64_t EndOffset   = 0;
        };

        // Salted SHA hash of a file and the ranges of it that are hashed.
        struct SOULS_API SHAHash {
            std::vector<uint8_t> Hash = std::vector<uint8_t>(32);  // 32 bytes
            std::vector<Range> Ranges;
        };

        // The AES key a file is encrypted with and the ranges of it that are encrypted.
        struct SOULS_API AESKey {
            std::vector<uint8_t> Key = std::vector<uint8_t>(16);  // 16 bytes
            std::vector<Range> Ranges;

            // Decrypts the encrypted ranges of Bytes in place (AES-128-ECB, no padding).
            void Decrypt(std::span<uint8_t> Bytes) const;
        };

        // A file's entry in the archive.
        struct SOULS_API FileHeader {
            // Hash of the full file path; see Util::FromPathHash for the older 32-bit form.
            uint64_t FileNameHash = 0;
            // Size of the file's data in the BDT, including any encryption padding.
            int32_t PaddedFileSize = 0;
            // Size after decryption; only stored by DS3 and Elden Ring, otherwise -1.
            int64_t UnpaddedFileSize = -1;
            // Where the file's data begins in the BDT.
            int64_t FileOffset = 0;
            std::optional<SHAHash> SHA;
            std::optional<AESKey> AES;

            // Reads this file's data from the BDT and decrypts it, if it's encrypted.
            std::vector<uint8_t> ReadFile(BinaryReader& BdtReader) const;
        };

        // Files grouped by hash for faster lookup.
        using Bucket = std::vector<FileHeader>;

        Game Format;
        bool BigEndian = false;
        // Unknown; possibly whether crypto is allowed (offsets are present regardless).
        bool Unk05 = false;
        // Salt for the file data's SHA hashes. DS2 onwards.
        std::string Salt;
        std::vector<Bucket> Buckets;

        // An empty header for the given game.
        explicit BHD5(Game Format) : Format(Format) {}

        // Reads a header that is already decrypted.
        static BHD5 Read(BinaryReader& Reader, Game Format);
        static BHD5 Read(std::span<const uint8_t> Data, Game Format);
        static BHD5 Read(const std::filesystem::path& Path, Game Format);

        void Write(BinaryWriter& Writer);
        std::vector<uint8_t> Write();
        void Write(const std::filesystem::path& Path);
    };

#pragma warning(pop)

}  // namespace Souls
