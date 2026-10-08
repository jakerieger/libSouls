//
// Created by Jake Rieger on 10/7/2026.
//

#pragma once

#include "IBinder.hpp"

#include <filesystem>
#include <span>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of an exported class; only touched by this DLL's own code

    // A BXF3 binder: a BND3-style container split across two files, a header file (BHF3, e.g. .tpfbhd) and a data
    // file (BDF3, e.g. .tpfbdt). Used by Demon's Souls through Dark Souls II.
    class SOULS_API BXF3 : public IBinder {
    public:
        // Whether multi-byte values are big-endian (as opposed to the *bit* order of the flag bytes).
        bool BigEndian    = false;
        bool BitBigEndian = false;

        // A new, empty binder with the common settings and a current timestamp as its version.
        BXF3();

#pragma region Is
        // Whether the data (after any DCX decompression) is a header (BHF3) or data (BDF3) file.
        static bool IsBHD(std::span<const uint8_t> Data);
        static bool IsBHD(const std::filesystem::path& Path);
        static bool IsBDT(std::span<const uint8_t> Data);
        static bool IsBDT(const std::filesystem::path& Path);
#pragma endregion

#pragma region Read
        // Reads from a header/data pair. Mix sources by constructing the BinaryReaders yourself.
        static BXF3 Read(BinaryReader& HeaderReader, BinaryReader& DataReader);
        static BXF3 Read(std::span<const uint8_t> Header, std::span<const uint8_t> Data);
        static BXF3 Read(const std::filesystem::path& HeaderPath, const std::filesystem::path& DataPath);
#pragma endregion

#pragma region Write
        void Write(BinaryWriter& HeaderWriter, BinaryWriter& DataWriter);
        BXFBytes Write();
        // Creates missing folders.
        void Write(const std::filesystem::path& HeaderPath, const std::filesystem::path& DataPath);
#pragma endregion
    };

#pragma warning(pop)

}  // namespace Souls
