//
// Created by Jake Rieger on 10/7/2026.
//

#pragma once

#include <libSouls/SoulsFile.hpp>
#include "IBinder.hpp"

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of an exported class; only touched by this DLL's own code

    // A BND3 binder: the container used by Demon's Souls through Dark Souls II. Files are decompressed when read
    // and recompressed when written, per FileFlags::Compressed.
    class SOULS_API BND3 : public SoulsFile<BND3>, public IBinder {
    public:
        // Whether multi-byte values are big-endian (as opposed to the *bit* order of the flag bytes).
        bool BigEndian    = false;
        bool BitBigEndian = false;

        // Unknown header value (0 or 0x80000000). Preserved so a read/write round trip keeps it.
        int32_t Unk18 = 0;

        // A new, empty binder with the common settings and a current timestamp as its version.
        BND3();

    protected:
        bool IsImpl(BinaryReader& Reader) override;
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
