/// @file
/// @author Jake Rieger
/// @date 10/7/2026

#pragma once

#include "BND4.hpp"

#include <filesystem>

/// @brief Reading and writing the encrypted regulation (param) binder of each game: AES-CBC over a BND4.
namespace Souls::Regulation {
    /// @brief Decrypts and unpacks Dark Souls III's regulation BND4.
    SOULS_API BND4 DecryptDS3(const std::filesystem::path& Path);
    /// @brief Repacks and encrypts Dark Souls III's regulation BND4, creating the folder if needed.
    SOULS_API void EncryptDS3(const std::filesystem::path& Path, BND4& Bnd);

    /// @brief Decrypts and unpacks Elden Ring's regulation BND4 (regulation.bin).
    SOULS_API BND4 DecryptER(const std::filesystem::path& Path);
    /// @brief Repacks and encrypts Elden Ring's regulation BND4, creating the folder if needed.
    SOULS_API void EncryptER(const std::filesystem::path& Path, BND4& Bnd);

    /// @brief The AES-256 keys used for each game's regulation file.
    SOULS_API std::span<const uint8_t> DS3Key();
    /// @brief The AES-256 key of Elden Ring's regulation.bin.
    SOULS_API std::span<const uint8_t> ERKey();

    /// @brief Lower-level pieces, for working with the bytes rather than files.
    SOULS_API BND4 DecryptBytes(std::span<const uint8_t> Key, std::span<const uint8_t> Encrypted);
    /// @brief Writes and encrypts a regulation binder (the reverse of DecryptBytes).
    SOULS_API std::vector<uint8_t> EncryptBytes(std::span<const uint8_t> Key, BND4& Bnd);
}  // namespace Souls::Regulation
