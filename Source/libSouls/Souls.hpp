//
// Created by Jake Rieger on 10/7/2026.
//

#pragma once

#ifdef LIBSOULS_EXPORTS
    #define SOULS_API __declspec(dllexport)
#else
    #define SOULS_API __declspec(dllimport)
#endif

#include <cstdint>

namespace Souls {}  // namespace Souls