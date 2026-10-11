// Copyright (C) 2026 Jake Rieger
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, version 3. This program is distributed in the hope that it will
// be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU General
// Public License along with this program. If not, see https://www.gnu.org/licenses/.

#pragma once

#include <cstdlib>
#include <filesystem>

// Where the tests find community data: the Vendor submodules of this repository (set up with
// `git submodule update --init`). The build passes the Vendor folder in as SOULS_VENDOR_DIR.
namespace TestPaths {
    inline std::filesystem::path Vendor() {
#ifdef SOULS_VENDOR_DIR
        return SOULS_VENDOR_DIR;
#else
        return "Vendor";
#endif
    }

    // Paramdex: paramdefs, row names and enums. PARAMDEX_PATH overrides the submodule.
    inline std::filesystem::path Paramdex() {
        if (const char* Env = std::getenv("PARAMDEX_PATH")) return Env;
        return Vendor() / "Paramdex";
    }

    // BinderKeys: the RSA keys of the games' archive headers and dictionaries of their file paths.
    inline std::filesystem::path BinderKeys() {
        if (const char* Env = std::getenv("BINDERKEYS_PATH")) return Env;
        return Vendor() / "BinderKeys";
    }
}  // namespace TestPaths
