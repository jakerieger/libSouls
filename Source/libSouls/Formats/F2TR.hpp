// Copyright (C) 2026 Jake Rieger
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, version 3. This program is distributed in the hope that it will
// be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU General
// Public License along with this program. If not, see https://www.gnu.org/licenses/.

#pragma once

#include <libSouls/SoulsFile.hpp>

#include <string>
#include <vector>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of an exported class; only touched by this DLL's own code

    /// @brief F2TR: a format that indicates which vertices of a FLVER are relevant for FaceGen.
    ///
    /// Extension: .flver2tri
    class SOULS_API F2TR : public SoulsFile<F2TR> {
    public:
        /// @brief A collection of indices, probably corresponding to a mesh.
        struct Entry {
            /// @brief Name of the relevant FaceGen file, I think.
            std::string Name;
            /// @brief Presumably vertex indices in the FLVER.
            std::vector<int16_t> Indices;
        };

        /// @brief Whether the file is big-endian.
        bool BigEndian = false;
        /// @brief Entries in the file, probably meshes.
        std::vector<Entry> Entries;

    protected:
        bool IsImpl(BinaryReader& Reader) override;
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
