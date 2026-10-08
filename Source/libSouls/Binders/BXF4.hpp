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

    // A BXF4 binder: a BND4-style container split across two files, a header file (BHF4, e.g. .tpfbhd) and a data
    // file (BDF4, e.g. .tpfbdt). Used by Bloodborne onwards.
    class SOULS_API BXF4 : public IBinder {
    public:
        // Unknown header flags. Preserved so a read/write round trip keeps them.
        bool Unk04 = false;
        bool Unk05 = false;

        // Whether multi-byte values are big-endian (as opposed to the *bit* order of the flag bytes).
        bool BigEndian    = false;
        bool BitBigEndian = false;

        // Whether names are UTF-16 (true) or Shift-JIS (false).
        bool Unicode = true;

        // 4 means the binder has a path hash table; the other seen value is 0.
        uint8_t Extended = 4;

        // A new, empty binder with the common modern settings and a current timestamp as its version.
        BXF4();

#pragma region Is
        // Whether the data (after any DCX decompression) is a header (BHF4) or data (BDF4) file.
        static bool IsBHD(std::span<const uint8_t> Data);
        static bool IsBHD(const std::filesystem::path& Path);
        static bool IsBDT(std::span<const uint8_t> Data);
        static bool IsBDT(const std::filesystem::path& Path);
#pragma endregion

#pragma region Read
        // Reads from a header/data pair. Mix sources by constructing the BinaryReaders yourself.
        static BXF4 Read(BinaryReader& HeaderReader, BinaryReader& DataReader);
        static BXF4 Read(std::span<const uint8_t> Header, std::span<const uint8_t> Data);
        static BXF4 Read(const std::filesystem::path& HeaderPath, const std::filesystem::path& DataPath);
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
