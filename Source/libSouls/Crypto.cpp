//
// Created by Jake Rieger on 10/7/2026.
//

#include "Crypto.hpp"
#include "Endian.hpp"

#include <algorithm>
#include <cwchar>
#include <string_view>

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

        enum class Mode { CBC, ECB };

        // An AES key ready for use; releases its CNG handles on destruction.
        class AesKey {
        public:
            explicit AesKey(std::span<const uint8_t> Key, Mode ChainingMode = Mode::CBC) {
                if (Key.size() != 16 && Key.size() != 24 && Key.size() != KeySize) {
                    throw BinaryException("AES key must be 16, 24 or 32 bytes");
                }
                if (ChainingMode == Mode::CBC && Key.size() != KeySize) {
                    throw BinaryException("AES-CBC key must be 32 bytes");
                }
                Check(BCryptOpenAlgorithmProvider(&Algorithm, BCRYPT_AES_ALGORITHM, nullptr, 0),
                      "BCryptOpenAlgorithmProvider");
                try {
                    const wchar_t* Chaining = ChainingMode == Mode::CBC ? BCRYPT_CHAIN_MODE_CBC : BCRYPT_CHAIN_MODE_ECB;
                    Check(BCryptSetProperty(Algorithm,
                                            BCRYPT_CHAINING_MODE,
                                            reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(Chaining)),
                                            static_cast<ULONG>((wcslen(Chaining) + 1) * sizeof(wchar_t)),
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

    void DecryptAesEcb(std::span<const uint8_t> Key, std::span<uint8_t> Data) {
        if (Data.size() % BlockSize != 0) {
            throw BinaryException("AES-ECB data length must be a multiple of 16");
        }
        if (Data.empty()) {
            return;
        }

        const AesKey Aes(Key, Mode::ECB);
        ULONG Written = 0;
        Check(BCryptDecrypt(Aes.Get(),
                            Data.data(),
                            static_cast<ULONG>(Data.size()),
                            nullptr,
                            nullptr,
                            0,
                            Data.data(),
                            static_cast<ULONG>(Data.size()),
                            &Written,
                            0),
              "BCryptDecrypt");
    }

    namespace {
        std::vector<uint8_t> DecodeBase64(const std::string& Text) {
            std::vector<uint8_t> Out;
            uint32_t Accumulator = 0;
            int Bits             = 0;
            for (const char C : Text) {
                int Value;
                if (C >= 'A' && C <= 'Z') Value = C - 'A';
                else if (C >= 'a' && C <= 'z') Value = C - 'a' + 26;
                else if (C >= '0' && C <= '9') Value = C - '0' + 52;
                else if (C == '+') Value = 62;
                else if (C == '/') Value = 63;
                else continue;  // padding, whitespace
                Accumulator = (Accumulator << 6) | static_cast<uint32_t>(Value);
                Bits += 6;
                if (Bits >= 8) {
                    Bits -= 8;
                    Out.push_back(static_cast<uint8_t>((Accumulator >> Bits) & 0xFF));
                }
            }
            return Out;
        }

        // Reads one DER element header at Pos, returning its content range and advancing Pos past the header.
        size_t ReadDerHeader(const std::vector<uint8_t>& Der, size_t& Pos, uint8_t ExpectedTag) {
            if (Pos + 2 > Der.size() || Der[Pos] != ExpectedTag) {
                throw BinaryException("Malformed RSA public key (unexpected DER element)");
            }
            ++Pos;
            size_t Length = Der[Pos++];
            if (Length & 0x80) {
                const size_t LengthBytes = Length & 0x7F;
                if (LengthBytes == 0 || LengthBytes > 4 || Pos + LengthBytes > Der.size()) {
                    throw BinaryException("Malformed RSA public key (bad DER length)");
                }
                Length = 0;
                for (size_t I = 0; I < LengthBytes; ++I) {
                    Length = (Length << 8) | Der[Pos++];
                }
            }
            if (Pos + Length > Der.size()) {
                throw BinaryException("Malformed RSA public key (truncated DER)");
            }
            return Length;
        }

        std::vector<uint8_t> ReadDerInteger(const std::vector<uint8_t>& Der, size_t& Pos) {
            const size_t Length = ReadDerHeader(Der, Pos, 0x02);
            std::vector<uint8_t> Value(Der.begin() + Pos, Der.begin() + Pos + Length);
            Pos += Length;
            while (Value.size() > 1 && Value.front() == 0) {
                Value.erase(Value.begin());  // drop sign padding
            }
            return Value;
        }

        // An imported RSA public key; releases its CNG handles on destruction.
        class RsaPublicKey {
        public:
            explicit RsaPublicKey(const std::string& Pem) {
                // Strip the BEGIN/END lines and decode the body.
                std::string Body;
                size_t Start = 0;
                while (Start < Pem.size()) {
                    size_t End = Pem.find('\n', Start);
                    if (End == std::string::npos) End = Pem.size();
                    std::string Line = Pem.substr(Start, End - Start);
                    if (Line.find("-----") == std::string::npos) Body += Line;
                    Start = End + 1;
                }
                const std::vector<uint8_t> Der = DecodeBase64(Body);

                size_t Pos = 0;
                ReadDerHeader(Der, Pos, 0x30);
                const std::vector<uint8_t> Modulus  = ReadDerInteger(Der, Pos);
                const std::vector<uint8_t> Exponent = ReadDerInteger(Der, Pos);
                BlockLength                         = Modulus.size();

                std::vector<uint8_t> Blob(sizeof(BCRYPT_RSAKEY_BLOB) + Exponent.size() + Modulus.size());
                auto* Header         = reinterpret_cast<BCRYPT_RSAKEY_BLOB*>(Blob.data());
                Header->Magic        = BCRYPT_RSAPUBLIC_MAGIC;
                Header->BitLength    = static_cast<ULONG>(Modulus.size() * 8);
                Header->cbPublicExp  = static_cast<ULONG>(Exponent.size());
                Header->cbModulus    = static_cast<ULONG>(Modulus.size());
                Header->cbPrime1     = 0;
                Header->cbPrime2     = 0;
                std::copy(Exponent.begin(), Exponent.end(), Blob.begin() + sizeof(BCRYPT_RSAKEY_BLOB));
                std::copy(Modulus.begin(), Modulus.end(), Blob.begin() + sizeof(BCRYPT_RSAKEY_BLOB) + Exponent.size());

                Check(BCryptOpenAlgorithmProvider(&Algorithm, BCRYPT_RSA_ALGORITHM, nullptr, 0),
                      "BCryptOpenAlgorithmProvider");
                const NTSTATUS Status = BCryptImportKeyPair(Algorithm,
                                                           nullptr,
                                                           BCRYPT_RSAPUBLIC_BLOB,
                                                           &Handle,
                                                           Blob.data(),
                                                           static_cast<ULONG>(Blob.size()),
                                                           0);
                if (!Succeeded(Status)) {
                    BCryptCloseAlgorithmProvider(Algorithm, 0);
                    Check(Status, "BCryptImportKeyPair");
                }
            }

            ~RsaPublicKey() {
                BCryptDestroyKey(Handle);
                BCryptCloseAlgorithmProvider(Algorithm, 0);
            }

            RsaPublicKey(const RsaPublicKey&)            = delete;
            RsaPublicKey& operator=(const RsaPublicKey&) = delete;

            BCRYPT_KEY_HANDLE Get() const { return Handle; }
            size_t BlockLength = 0;  // modulus size in bytes

        private:
            BCRYPT_ALG_HANDLE Algorithm = nullptr;
            BCRYPT_KEY_HANDLE Handle    = nullptr;
        };
    }  // namespace

    std::vector<uint8_t> DecryptRsaBlocks(const std::string& KeyPem, std::span<const uint8_t> Data) {
        const RsaPublicKey Key(KeyPem);
        const size_t InputSize  = Key.BlockLength;
        const size_t OutputSize = InputSize - 1;
        if (InputSize < 2) {
            throw BinaryException("RSA key is too small");
        }

        std::vector<uint8_t> Result;
        Result.reserve(Data.size() / InputSize * OutputSize);
        std::vector<uint8_t> Block(InputSize);
        for (size_t Offset = 0; Offset + InputSize <= Data.size(); Offset += InputSize) {
            ULONG Written = 0;
            Check(BCryptEncrypt(Key.Get(),
                                const_cast<PUCHAR>(Data.data() + Offset),
                                static_cast<ULONG>(InputSize),
                                nullptr,
                                nullptr,
                                0,
                                Block.data(),
                                static_cast<ULONG>(Block.size()),
                                &Written,
                                BCRYPT_PAD_NONE),
                  "BCryptEncrypt (RSA)");
            if (Written != InputSize) {
                throw BinaryException("Unexpected RSA block size");
            }
            // Plaintext blocks are one byte shorter than the modulus; the leading byte is always zero.
            Result.insert(Result.end(), Block.begin() + 1, Block.end());
        }
        return Result;
    }

    std::vector<std::string> FindRsaPublicKeys(std::span<const uint8_t> Data) {
        static constexpr std::string_view Begin = "-----BEGIN RSA PUBLIC KEY-----";
        static constexpr std::string_view End   = "-----END RSA PUBLIC KEY-----";

        const std::string_view Text(reinterpret_cast<const char*>(Data.data()), Data.size());
        std::vector<std::string> Keys;
        size_t Pos = 0;
        while ((Pos = Text.find(Begin, Pos)) != std::string_view::npos) {
            const size_t Stop = Text.find(End, Pos);
            if (Stop == std::string_view::npos) {
                break;
            }
            Keys.emplace_back(Text.substr(Pos, Stop + End.size() - Pos));
            Pos = Stop + End.size();
        }
        return Keys;
    }
}  // namespace Souls::Crypto
