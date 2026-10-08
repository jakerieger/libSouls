//
// Created by Jake Rieger on 10/7/2026.
//

#pragma once

#include <libSouls/SoulsFile.hpp>
#include "IBinder.hpp"

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of an exported class; only touched by this DLL's own code

    // A BND4 binder: the container used by Bloodborne onwards (and a few earlier PC formats) to bundle many files.
    // Files are decompressed when read and recompressed when written, per FileFlags::Compressed.
    class SOULS_API BND4 : public SoulsFile<BND4>, public IBinder {
    public:
        // Unknown header flags. Preserved so a read/write round trip keeps them.
        bool Unk04 = false;
        bool Unk05 = false;

        // Whether multi-byte values are big-endian (as opposed to the *bit* order of the flag bytes).
        bool BigEndian    = false;
        bool BitBigEndian = false;

        // Whether names are UTF-16 (true) or Shift-JIS (false).
        bool Unicode = true;

        // 4 means the binder has a path hash table. Other seen values: 0, 1, 0x80.
        uint8_t Extended = 4;

        // A new, empty binder with the common modern settings and a current timestamp as its version.
        BND4();

    protected:
        bool IsImpl(BinaryReader& Reader) override;
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
