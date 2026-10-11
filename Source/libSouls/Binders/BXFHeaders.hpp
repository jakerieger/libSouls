// Copyright (C) 2026 Jake Rieger
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, version 3. This program is distributed in the hope that it will
// be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU General
// Public License along with this program. If not, see https://www.gnu.org/licenses/.

#pragma once

#include "BinderFileHeader.hpp"
#include "BXF3.hpp"
#include "BXF4.hpp"

#include <vector>

/// @brief The header-parsing halves of BXF3 and BXF4, shared with BXFReader (which reads files on demand instead of all
///        at once).
namespace Souls::BXFHeaders {
    /// @brief Checks the data file's header; BXF4's also sets the reader's byte order.
    void ReadBDF3(BinaryReader& Reader);
    /// @brief Checks a BXF4 data file's header and sets the reader's byte order.
    void ReadBDF4(BinaryReader& Reader);

    /// @brief Reads the header file, filling in the container's settings (everything but the files' data) and returning
    ///        the file headers.
    std::vector<BinderFileHeader> ReadBHF3(BXF3& Bxf, BinaryReader& Reader);
    /// @brief Reads a BXF4 header file into Bxf's settings and returns the file headers.
    std::vector<BinderFileHeader> ReadBHF4(BXF4& Bxf, BinaryReader& Reader);
}  // namespace Souls::BXFHeaders
