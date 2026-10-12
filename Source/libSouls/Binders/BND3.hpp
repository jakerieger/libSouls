// Copyright (C) 2026 Jake Rieger
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, version 3. This program is distributed in the hope that it will
// be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU General
// Public License along with this program. If not, see https://www.gnu.org/licenses/.

#pragma once

#include <libSouls/SoulsFile.hpp>
#include "IBinder.hpp"

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of an exported class; only touched by this DLL's own code
#pragma warning(disable : 4275)  // IBinder is header-only, so its not being exported is harmless

    /// @brief A BND3 binder: the container used by Demon's Souls through Dark Souls II.
    ///
    /// Files are decompressed when read and recompressed when written, per FileFlags::Compressed.
    class SOULS_API BND3 : public SoulsFile<BND3>,
                           public IBinder {
    public:
        /// @brief Whether multi-byte values are big-endian (as opposed to the *bit* order of the flag bytes).
        bool BigEndian = false;
        /// @brief Unknown; usually false.
        bool BitBigEndian = false;

        /// @brief Unknown header value (0 or 0x80000000).
        ///
        /// Preserved so a read/write round trip keeps it.
        int32_t Unk18 = 0;

        /// @brief A new, empty binder with the common settings and a current timestamp as its version.
        BND3();

    protected:
        bool IsImpl(BinaryReader& Reader) override;
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
