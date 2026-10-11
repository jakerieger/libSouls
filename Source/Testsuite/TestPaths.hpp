//
// Created by Jake Rieger on 10/11/2026.
//

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
