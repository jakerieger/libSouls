//
// Created by Jake Rieger on 10/7/2026.
//

#pragma once

#include "Souls.hpp"

#include <cstdint>
#include <span>
#include <vector>

// AES-CBC as used by FromSoftware's regulation and save files, backed by Windows CNG.
namespace Souls::Crypto {
    // Key must be 32 bytes (AES-256). Data is a 16-byte IV followed by the ciphertext, whose length must be a
    // multiple of 16. Padding is NOT removed: the result is the same length as the ciphertext, so trailing padding
    // bytes are included (the binder formats ignore them). Throws BinaryException on bad input.
    SOULS_API std::vector<uint8_t> DecryptAesCbc(std::span<const uint8_t> Key, std::span<const uint8_t> Data);

    // Key must be 32 bytes. Encrypts with a random IV and PKCS7 padding; the result is the IV followed by the
    // ciphertext.
    SOULS_API std::vector<uint8_t> EncryptAesCbc(std::span<const uint8_t> Key, std::span<const uint8_t> Data);
}  // namespace Souls::Crypto
