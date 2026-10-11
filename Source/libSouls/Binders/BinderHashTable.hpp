// Copyright (C) 2026 Jake Rieger
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, version 3. This program is distributed in the hope that it will
// be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU General
// Public License along with this program. If not, see https://www.gnu.org/licenses/.

#pragma once

#include "BinderFileHeader.hpp"

#include <vector>

namespace Souls::BinderHashTable {
    /// @brief Checks the table header at the reader's position.
    ///
    /// The hashes themselves aren't read; they're derived from the file names and are regenerated on write.
    void Assert(BinaryReader& Reader);

    /// @brief Writes the path hash table for the given files (all must have names).
    void Write(BinaryWriter& Writer, const std::vector<BinderFileHeader>& Files);
}  // namespace Souls::BinderHashTable
