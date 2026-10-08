//
// Created by Jake Rieger on 10/7/2026.
//

#pragma once

#include "Souls.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

// Cryptography used by FromSoftware's regulation files, save files and game archives, backed by Windows CNG.
// All functions throw BinaryException on bad input or CNG failure.
namespace Souls::Crypto {
#pragma region AES
    // Key must be 32 bytes (AES-256). Data is a 16-byte IV followed by the ciphertext, whose length must be a
    // multiple of 16. Padding is NOT removed: the result is the same length as the ciphertext, so trailing padding
    // bytes are included (the binder formats ignore them).
    SOULS_API std::vector<uint8_t> DecryptAesCbc(std::span<const uint8_t> Key, std::span<const uint8_t> Data);

    // Key must be 32 bytes. Encrypts with a random IV and PKCS7 padding; the result is the IV followed by the
    // ciphertext.
    SOULS_API std::vector<uint8_t> EncryptAesCbc(std::span<const uint8_t> Key, std::span<const uint8_t> Data);

    // Decrypts in place with AES-ECB and no padding, as used for the encrypted ranges of files in game archives
    // (BHD5). Key must be 16, 24 or 32 bytes; the data's length must be a multiple of 16.
    SOULS_API void DecryptAesEcb(std::span<const uint8_t> Key, std::span<uint8_t> Data);
#pragma endregion

#pragma region RSA
    // Decrypts game archive headers (Data0.bhd, DLC.bhd, ...) that were encrypted with an RSA private key, using
    // the matching public key from the game's executable. Each block of modulus-size bytes is raised to the public
    // exponent with no padding; the result is the plaintext block minus its always-zero leading byte, so output is
    // one byte shorter per block. KeyPem is a "-----BEGIN RSA PUBLIC KEY-----" (PKCS#1) block. A trailing partial
    // block, if any, is ignored.
    SOULS_API std::vector<uint8_t> DecryptRsaBlocks(const std::string& KeyPem, std::span<const uint8_t> Data);

    // Finds every "-----BEGIN RSA PUBLIC KEY-----" ... "-----END RSA PUBLIC KEY-----" block embedded in the data
    // (typically a game executable), in order of appearance.
    SOULS_API std::vector<std::string> FindRsaPublicKeys(std::span<const uint8_t> Data);
#pragma endregion
}  // namespace Souls::Crypto
