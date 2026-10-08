//
// Created by Jake Rieger on 10/7/2026.
//

#include "Crypto.hpp"
#include "Endian.hpp"

#include <algorithm>

#include <windows.h>
#include <bcrypt.h>

#include <string>

namespace Souls::Crypto {
    namespace {
        constexpr size_t BlockSize = 16;
        constexpr size_t KeySize   = 32;

        bool Succeeded(NTSTATUS Status) {
            return Status >= 0;
        }

        void Check(NTSTATUS Status, const char* What) {
            if (!Succeeded(Status)) {
                throw BinaryException(std::string(What) + " failed (NTSTATUS 0x" + std::to_string(static_cast<unsigned>(Status)) + ")");
            }
        }

        // An AES-CBC key ready for use; releases its CNG handles on destruction.
        class AesKey {
        public:
            explicit AesKey(std::span<const uint8_t> Key) {
                if (Key.size() != KeySize) {
                    throw BinaryException("AES key must be 32 bytes");
                }
                Check(BCryptOpenAlgorithmProvider(&Algorithm, BCRYPT_AES_ALGORITHM, nullptr, 0),
                      "BCryptOpenAlgorithmProvider");
                try {
                    Check(BCryptSetProperty(Algorithm,
                                            BCRYPT_CHAINING_MODE,
                                            reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_CBC)),
                                            sizeof(BCRYPT_CHAIN_MODE_CBC),
                                            0),
                          "BCryptSetProperty");
                    Check(BCryptGenerateSymmetricKey(Algorithm,
                                                     &Handle,
                                                     nullptr,
                                                     0,
                                                     const_cast<PUCHAR>(Key.data()),
                                                     static_cast<ULONG>(Key.size()),
                                                     0),
                          "BCryptGenerateSymmetricKey");
                } catch (...) {
                    BCryptCloseAlgorithmProvider(Algorithm, 0);
                    throw;
                }
            }

            ~AesKey() {
                BCryptDestroyKey(Handle);
                BCryptCloseAlgorithmProvider(Algorithm, 0);
            }

            AesKey(const AesKey&)            = delete;
            AesKey& operator=(const AesKey&) = delete;

            BCRYPT_KEY_HANDLE Get() const { return Handle; }

        private:
            BCRYPT_ALG_HANDLE Algorithm = nullptr;
            BCRYPT_KEY_HANDLE Handle    = nullptr;
        };
    }  // namespace

    std::vector<uint8_t> DecryptAesCbc(std::span<const uint8_t> Key, std::span<const uint8_t> Data) {
        if (Data.size() < BlockSize || (Data.size() - BlockSize) % BlockSize != 0) {
            throw BinaryException("Encrypted data must be a 16-byte IV followed by whole 16-byte blocks");
        }

        const AesKey Aes(Key);
        uint8_t IV[BlockSize];
        std::copy(Data.begin(), Data.begin() + BlockSize, IV);

        const auto Cipher = Data.subspan(BlockSize);
        std::vector<uint8_t> Plain(Cipher.size());
        if (Cipher.empty()) {
            return Plain;
        }

        ULONG Written = 0;
        Check(BCryptDecrypt(Aes.Get(),
                            const_cast<PUCHAR>(Cipher.data()),
                            static_cast<ULONG>(Cipher.size()),
                            nullptr,
                            IV,
                            static_cast<ULONG>(BlockSize),
                            Plain.data(),
                            static_cast<ULONG>(Plain.size()),
                            &Written,
                            0),
              "BCryptDecrypt");
        Plain.resize(Written);
        return Plain;
    }

    std::vector<uint8_t> EncryptAesCbc(std::span<const uint8_t> Key, std::span<const uint8_t> Data) {
        const AesKey Aes(Key);

        uint8_t IV[BlockSize];
        Check(BCryptGenRandom(nullptr, IV, static_cast<ULONG>(BlockSize), BCRYPT_USE_SYSTEM_PREFERRED_RNG),
              "BCryptGenRandom");

        uint8_t IVForCipher[BlockSize];  // CNG updates the IV it's given, so keep the original to prepend
        std::copy(IV, IV + BlockSize, IVForCipher);

        // PKCS7 always adds 1..16 bytes of padding.
        std::vector<uint8_t> Output(BlockSize + (Data.size() / BlockSize + 1) * BlockSize);
        std::copy(IV, IV + BlockSize, Output.begin());

        ULONG Written = 0;
        Check(BCryptEncrypt(Aes.Get(),
                            const_cast<PUCHAR>(Data.data()),
                            static_cast<ULONG>(Data.size()),
                            nullptr,
                            IVForCipher,
                            static_cast<ULONG>(BlockSize),
                            Output.data() + BlockSize,
                            static_cast<ULONG>(Output.size() - BlockSize),
                            &Written,
                            BCRYPT_BLOCK_PADDING),
              "BCryptEncrypt");
        Output.resize(BlockSize + Written);
        return Output;
    }
}  // namespace Souls::Crypto
