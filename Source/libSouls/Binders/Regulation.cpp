//
// Created by Jake Rieger on 10/7/2026.
//

#include "Regulation.hpp"

#include <libSouls/Crypto.hpp>
#include <libSouls/Util.hpp>

#include <string_view>

namespace Souls::Regulation {
    namespace {
        BND4 DecryptFile(std::span<const uint8_t> Key, const std::filesystem::path& Path) {
            BinaryReader Reader(Path);
            const std::vector<uint8_t> Encrypted = Reader.ReadBytes(static_cast<size_t>(Reader.Length()));
            return DecryptBytes(Key, Encrypted);
        }

        void EncryptFile(std::span<const uint8_t> Key, const std::filesystem::path& Path, BND4& Bnd) {
            const std::vector<uint8_t> Encrypted = EncryptBytes(Key, Bnd);
            if (Path.has_parent_path()) {
                std::filesystem::create_directories(Path.parent_path());
            }
            BinaryWriter Writer(Path);
            Writer.WriteBytes(Encrypted);
            Writer.Finish();
        }
    }  // namespace

    std::span<const uint8_t> DS3Key() {
        static constexpr std::string_view Text = "ds3#jn/8_7(rsY9pg55GFN7VFL#+3n/)";
        static const std::vector<uint8_t> Key(Text.begin(), Text.end());
        return Key;
    }

    std::span<const uint8_t> ERKey() {
        static const std::vector<uint8_t> Key = Util::ParseHexString(
          "99 BF FC 36 6A 6B C8 C6 F5 82 7D 09 36 02 D6 76 C4 28 92 A0 1C 20 7F B0 24 D3 AF 4E 49 3F EF 99");
        return Key;
    }

    BND4 DecryptBytes(std::span<const uint8_t> Key, std::span<const uint8_t> Encrypted) {
        const std::vector<uint8_t> Decrypted = Crypto::DecryptAesCbc(Key, Encrypted);
        return BND4::Read(Decrypted);
    }

    std::vector<uint8_t> EncryptBytes(std::span<const uint8_t> Key, BND4& Bnd) {
        return Crypto::EncryptAesCbc(Key, Bnd.Write());
    }

    BND4 DecryptDS3(const std::filesystem::path& Path) {
        return DecryptFile(DS3Key(), Path);
    }

    void EncryptDS3(const std::filesystem::path& Path, BND4& Bnd) {
        EncryptFile(DS3Key(), Path, Bnd);
    }

    BND4 DecryptER(const std::filesystem::path& Path) {
        return DecryptFile(ERKey(), Path);
    }

    void EncryptER(const std::filesystem::path& Path, BND4& Bnd) {
        EncryptFile(ERKey(), Path, Bnd);
    }
}  // namespace Souls::Regulation
