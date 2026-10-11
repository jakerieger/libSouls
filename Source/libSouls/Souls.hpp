/// @file
/// @author Jake Rieger
/// @date 10/7/2026

#pragma once

/// @brief Marks the library's exported classes and functions (dllexport when building libSouls, dllimport when using it).
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