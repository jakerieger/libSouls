//
// Created by Jake Rieger on 10/7/2026.
//

#pragma once

#include "BinderFileHeader.hpp"

#include <vector>

namespace Souls::BinderHashTable {
    // Checks the table header at the reader's position. The hashes themselves aren't read; they're derived from
    // the file names and are regenerated on write.
    void Assert(BinaryReader& Reader);

    // Writes the path hash table for the given files (all must have names).
    void Write(BinaryWriter& Writer, const std::vector<BinderFileHeader>& Files);
}  // namespace Souls::BinderHashTable
