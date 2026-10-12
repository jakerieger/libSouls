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

    /// @brief A BND4 binder: the container used by Bloodborne onwards (and a few earlier PC formats) to bundle many
    ///        files.
    ///
    /// Files are decompressed when read and recompressed when written, per FileFlags::Compressed.
    class SOULS_API BND4 : public SoulsFile<BND4>,
                           public IBinder {
    public:
        /// @brief Unknown header flags.
        ///
        /// Preserved so a read/write round trip keeps them.
        bool Unk04 = false;
        /// @brief Unknown.
        bool Unk05 = false;

        /// @brief Whether multi-byte values are big-endian (as opposed to the *bit* order of the flag bytes).
        bool BigEndian = false;
        /// @brief Controls ordering of flag bits.
        bool BitBigEndian = false;

        /// @brief Whether names are UTF-16 (true) or Shift-JIS (false).
        bool Unicode = true;

        /// @brief 4 means the binder has a path hash table.
        ///
        /// Other seen values: 0, 1, 0x80.
        uint8_t Extended = 4;

        /// @brief Whether the header's "end of headers" offset is rounded up to 16 bytes, as save files do.
        bool AlignHeadersEnd = false;

        /// @brief A new, empty binder with the common modern settings and a current timestamp as its version.
        BND4();

    protected:
        bool IsImpl(BinaryReader& Reader) override;
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
