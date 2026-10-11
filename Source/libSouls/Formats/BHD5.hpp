// Copyright (C) 2026 Jake Rieger
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, version 3. This program is distributed in the hope that it will
// be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU General
// Public License along with this program. If not, see https://www.gnu.org/licenses/.

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

    /// @brief The header file (Data0.bhd, DLC.bhd, ...) of the "dvdbnd" container that packages a game's files under
    ///        hashed names.
    ///
    /// The data lives in the matching .bdt file; each FileHeader says where. In most games the header file is
    /// RSA-encrypted on disk and must be decrypted first (see Crypto::DecryptRsaBlocks).
    class SOULS_API BHD5 {
    public:
        /// @brief Which game's layout the file uses.
        enum class Game {
            DarkSouls1,  ///< PC and console
            DarkSouls2,  ///< PC, including Scholar of the First Sin
            DarkSouls3,  ///< PC, and Sekiro
            EldenRing,   ///< PC
        };

        /// @brief A byte range of a file, start inclusive and end exclusive.
        struct SOULS_API Range {
            /// @brief The beginning of the range, inclusive.
            int64_t StartOffset = 0;
            /// @brief The end of the range, exclusive.
            int64_t EndOffset = 0;
        };

        /// @brief Salted SHA hash of a file and the ranges of it that are hashed.
        struct SOULS_API SHAHash {
            std::vector<uint8_t> Hash = std::vector<uint8_t>(32);  ///< 32 bytes
            /// @brief Hashed sections of the file.
            std::vector<Range> Ranges;
        };

        /// @brief The AES key a file is encrypted with and the ranges of it that are encrypted.
        struct SOULS_API AESKey {
            std::vector<uint8_t> Key = std::vector<uint8_t>(16);  ///< 16 bytes
            /// @brief Encrypted sections of the file.
            std::vector<Range> Ranges;

            /// @brief Decrypts the encrypted ranges of Bytes in place (AES-128-ECB, no padding).
            void Decrypt(std::span<uint8_t> Bytes) const;
        };

        /// @brief A file's entry in the archive.
        struct SOULS_API FileHeader {
            /// @brief Hash of the full file path; see Util::FromPathHash for the older 32-bit form.
            uint64_t FileNameHash = 0;
            /// @brief Size of the file's data in the BDT, including any encryption padding.
            int32_t PaddedFileSize = 0;
            /// @brief Size after decryption; only stored by DS3 and Elden Ring, otherwise -1.
            int64_t UnpaddedFileSize = -1;
            /// @brief Where the file's data begins in the BDT.
            int64_t FileOffset = 0;
            /// @brief The salted SHA-256 hash of the file and the ranges it covers, if the archive stores one.
            std::optional<SHAHash> SHA;
            /// @brief The AES key and encrypted ranges, if the file is encrypted.
            std::optional<AESKey> AES;

            /// @brief Reads this file's data from the BDT and decrypts it, if it's encrypted.
            std::vector<uint8_t> ReadFile(BinaryReader& BdtReader) const;
        };

        /// @brief Files grouped by hash for faster lookup.
        using Bucket = std::vector<FileHeader>;

        /// @brief Format the file should be written in.
        Game Format;
        /// @brief Whether the header is big-endian.
        bool BigEndian = false;
        /// @brief Unknown; possibly whether crypto is allowed (offsets are present regardless).
        bool Unk05 = false;
        /// @brief Salt for the file data's SHA hashes.
        ///
        /// DS2 onwards.
        std::string Salt;
        /// @brief Collections of files grouped by their hash value for faster lookup.
        std::vector<Bucket> Buckets;

        /// @brief An empty header for the given game.
        explicit BHD5(Game Format) : Format(Format) {}

        /// @brief Reads a header that is already decrypted.
        static BHD5 Read(BinaryReader& Reader, Game Format);
        /// @brief Reads an already-decrypted header from memory.
        static BHD5 Read(std::span<const uint8_t> Data, Game Format);
        /// @brief Reads an already-decrypted header from disk.
        static BHD5 Read(const std::filesystem::path& Path, Game Format);

        /// @brief Writes the header (unencrypted).
        void Write(BinaryWriter& Writer);
        /// @brief Writes the header (unencrypted) to memory.
        std::vector<uint8_t> Write();
        /// @brief Writes the header (unencrypted) to disk.
        void Write(const std::filesystem::path& Path);
    };

#pragma warning(pop)

}  // namespace Souls
