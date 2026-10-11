// Copyright (C) 2026 Jake Rieger
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, version 3. This program is distributed in the hope that it will
// be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU General
// Public License along with this program. If not, see https://www.gnu.org/licenses/.

#include "SL2.hpp"

#include <libSouls/Crypto.hpp>

#include <algorithm>
#include <cstring>

namespace Souls::SL2 {
    namespace {
        constexpr size_t HashSize  = 16;
        constexpr size_t BlockSize = 16;

        bool Encrypted(Game For) {
            return For != Game::EldenRing;
        }
    }  // namespace

    std::span<const uint8_t> Key(Game For) {
        // From SoulsFormats (SL2Decryptor).
        static const std::vector<uint8_t> DarkSouls2 =
          {0xB7, 0xFD, 0x46, 0x3E, 0x4A, 0x9C, 0x11, 0x02, 0xDF, 0x17, 0x39, 0xE5, 0xF3, 0xB2, 0xA5, 0x0F};
        static const std::vector<uint8_t> Scholar =
          {0x59, 0x9F, 0x9B, 0x69, 0x96, 0x40, 0xA5, 0x52, 0x36, 0xEE, 0x2D, 0x70, 0x83, 0x5E, 0xC7, 0x44};
        static const std::vector<uint8_t> DarkSouls3 =
          {0xFD, 0x46, 0x4D, 0x69, 0x5E, 0x69, 0xA3, 0x9A, 0x10, 0xE3, 0x19, 0xA7, 0xAC, 0xE8, 0xB7, 0xFA};
        switch (For) {
            case Game::DarkSouls2:
                return DarkSouls2;
            case Game::DarkSouls2Scholar:
                return Scholar;
            case Game::DarkSouls3:
                return DarkSouls3;
            case Game::EldenRing:
                return {};
        }
        throw BinaryException("Unknown game for a save file");
    }

    std::vector<uint8_t> DecodeFile(std::span<const uint8_t> Stored, Game For) {
        if (Stored.size() < HashSize) { throw BinaryException("A save file entry is too small to hold its hash."); }
        const std::span<const uint8_t> Body = Stored.subspan(HashSize);
        const auto Hash                     = Crypto::Md5(Body);
        if (!std::equal(Hash.begin(), Hash.end(), Stored.begin())) {
            throw BinaryException(
              "A save file entry's MD5 hash doesn't match its contents (wrong game, or a damaged save).");
        }
        if (!Encrypted(For)) { return {Body.begin(), Body.end()}; }
        // The body is an IV and the ciphertext, which is what DecryptAesCbc takes.
        if (Body.size() < BlockSize || (Body.size() - BlockSize) % BlockSize != 0) {
            throw BinaryException("An encrypted save file entry isn't a whole number of AES blocks.");
        }
        return Crypto::DecryptAesCbc(Key(For), Body);
    }

    std::vector<uint8_t> EncodeFile(std::span<const uint8_t> Data, Game For) {
        std::vector<uint8_t> Body;
        if (Encrypted(For)) {
            if (Data.size() % BlockSize != 0) {
                throw BinaryException("Save data to encrypt must be a whole number of 16-byte blocks.");
            }
            Body                                 = Crypto::RandomBytes(BlockSize);
            const std::vector<uint8_t> Encrypted = Crypto::EncryptAesCbcNoPadding(Key(For), Body, Data);
            Body.insert(Body.end(), Encrypted.begin(), Encrypted.end());
        } else {
            Body.assign(Data.begin(), Data.end());
        }
        const auto Hash = Crypto::Md5(Body);
        std::vector<uint8_t> Stored(Hash.begin(), Hash.end());
        Stored.insert(Stored.end(), Body.begin(), Body.end());
        return Stored;
    }

    BND4 Read(std::span<const uint8_t> Data, Game For) {
        BND4 Save = BND4::Read(Data);
        for (BinderFile& File : Save.Files) {
            File.Bytes = DecodeFile(File.Bytes, For);
        }
        return Save;
    }

    BND4 Read(const std::filesystem::path& Path, Game For) {
        BinaryReader Reader(Path);
        const std::vector<uint8_t> Bytes = Reader.ReadBytes(static_cast<size_t>(Reader.Length()));
        return Read(Bytes, For);
    }

    std::vector<uint8_t> Write(const BND4& Save, Game For) {
        BND4 Stored = Save;
        for (BinderFile& File : Stored.Files) {
            File.Bytes = EncodeFile(File.Bytes, For);
        }
        return Stored.Write();
    }

    void Write(const std::filesystem::path& Path, const BND4& Save, Game For) {
        const std::vector<uint8_t> Bytes = Write(Save, For);
        if (Path.has_parent_path()) { std::filesystem::create_directories(Path.parent_path()); }
        BinaryWriter Writer(Path);
        Writer.WriteBytes(Bytes);
        Writer.Finish();
    }
}  // namespace Souls::SL2
