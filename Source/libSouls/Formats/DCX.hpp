//
// Created by Jake Rieger on 10/7/2026.
//

#pragma once

#include <libSouls/Souls.hpp>
#include <libSouls/BinaryReader.hpp>
#include <libSouls/BinaryWriter.hpp>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <vector>

// DCX: the single-file compression container used by FromSoftware games. All multi-byte fields are big-endian.
// Decompress/Compress throw BinaryException on malformed data, unsupported variants or compressor failure.
namespace Souls::DCX {
    enum class Type {
        Unknown,                 // Not detected; cannot be used to compress.
        None,                    // No compression.
        Zlib,                    // A bare zlib stream (not a real DCX).
        DCP_EDGE,                // DCP header, chunked deflate (ACE:R TPFs). Decompress only.
        DCP_DFLT,                // DCP header, one deflate stream (DeS test maps).
        DCX_EDGE,                // DCX header, chunked deflate (mostly Demon's Souls).
        DCX_DFLT_10000_24_9,     // DCX header, deflate (mostly DS1, DS2).
        DCX_DFLT_10000_44_9,     // DCX header, deflate (mostly BB, DS3).
        DCX_DFLT_11000_44_8,     // DCX header, deflate level 8 (DS3 backup regulation).
        DCX_DFLT_11000_44_9,     // DCX header, deflate (Sekiro).
        DCX_DFLT_11000_44_9_15,  // DCX header, deflate (Elden Ring regulation).
        DCX_KRAK_6,              // DCX header, Oodle Kraken level 6 (Sekiro, Elden Ring).
        DCX_KRAK_9,              // DCX header, Oodle Kraken level 9 (AC6).
        DCX_ZSTD,                // DCX header, Zstandard (Elden Ring after the DLC).
    };

    // The usual compression type for each game's files.
    enum class DefaultType {
        DemonsSouls  = static_cast<int>(Type::DCX_EDGE),
        DarkSouls1   = static_cast<int>(Type::DCX_DFLT_10000_24_9),
        DarkSouls2   = static_cast<int>(Type::DCX_DFLT_10000_24_9),
        Bloodborne   = static_cast<int>(Type::DCX_DFLT_10000_44_9),
        DarkSouls3   = static_cast<int>(Type::DCX_DFLT_10000_44_9),
        Sekiro       = static_cast<int>(Type::DCX_KRAK_6),
        EldenRing    = static_cast<int>(Type::DCX_KRAK_6),
        ArmoredCore6 = static_cast<int>(Type::DCX_KRAK_9),
    };

    constexpr Type ToType(DefaultType Default) {
        return static_cast<Type>(Default);
    }

#pragma region Detection
    // True if the data starts with a DCX or DCP magic. Doesn't move the reader.
    SOULS_API bool Is(BinaryReader& Reader);
    SOULS_API bool Is(std::span<const uint8_t> Data);
    SOULS_API bool Is(const std::filesystem::path& Path);
#pragma endregion

#pragma region Decompress
    // Decompresses a DCX/DCP container or bare zlib stream, reporting what it was. Sets Reader.Order to big-endian.
    SOULS_API std::vector<uint8_t> Decompress(BinaryReader& Reader, Type& OutType);
    SOULS_API std::vector<uint8_t> Decompress(std::span<const uint8_t> Data, Type& OutType);
    SOULS_API std::vector<uint8_t> Decompress(std::span<const uint8_t> Data);
    SOULS_API std::vector<uint8_t> Decompress(const std::filesystem::path& Path, Type& OutType);
    SOULS_API std::vector<uint8_t> Decompress(const std::filesystem::path& Path);
#pragma endregion

#pragma warning(push)
#pragma warning(disable : 4251)  // STL member of an exported class; only touched by this DLL's own code

    // Gives format code a reader over the *decompressed* contents of a source that may or may not be DCX. If the
    // source is a DCX, this decompresses it into an owned in-memory little-endian reader; otherwise it just refers
    // to the source reader, untouched. (SoulsFormats' SFUtil.GetDecompressedBR.)
    class SOULS_API DecompressedReader {
    public:
        explicit DecompressedReader(BinaryReader& Source);

        DecompressedReader(const DecompressedReader&)            = delete;
        DecompressedReader& operator=(const DecompressedReader&) = delete;

        // The reader to parse from. Valid for the lifetime of this object (and of Source, if not DCX).
        BinaryReader& Reader() { return *Active; }
        // The compression found, or Type::None if the source wasn't a DCX.
        Type Compression() const { return Detected; }

    private:
        std::unique_ptr<BinaryReader> Owned;
        BinaryReader* Active = nullptr;
        Type Detected        = Type::None;
    };

#pragma warning(pop)

#pragma region Compress
    // Sets Writer.Order to big-endian.
    SOULS_API void Compress(std::span<const uint8_t> Data, BinaryWriter& Writer, Type Compression);
    SOULS_API std::vector<uint8_t> Compress(std::span<const uint8_t> Data, Type Compression);
    SOULS_API void Compress(std::span<const uint8_t> Data, Type Compression, const std::filesystem::path& Path);
#pragma endregion
}  // namespace Souls::DCX
