// Copyright (C) 2026 Jake Rieger
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, version 3. This program is distributed in the hope that it will
// be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU General
// Public License along with this program. If not, see https://www.gnu.org/licenses/.

#pragma once

/// @brief Marks the library's exported classes and functions (dllexport when building libSouls, dllimport when using
/// it).
#ifdef LIBSOULS_EXPORTS
    #define SOULS_API __declspec(dllexport)
#else
    #define SOULS_API __declspec(dllimport)
#endif

/// @brief Shorthand for [[nodiscard]].
#ifndef NODISCARD
    #define NODISCARD [[nodiscard]]
#endif

#include <cstdint>

/// @brief Everything in libSouls.
namespace Souls {}  // namespace Souls