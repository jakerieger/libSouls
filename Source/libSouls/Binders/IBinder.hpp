//
// Created by Jake Rieger on 10/7/2026.
//

#pragma once

#include "Binder.hpp"
#include "BinderFile.hpp"

#include <string>
#include <vector>

namespace Souls {
    // What every binder format (BND3, BND4, ...) has in common.
    class IBinder {
    public:
        virtual ~IBinder() = default;

        Binder::Format Format = Binder::Format::None;
        // Binder timestamp/version string, at most 8 bytes.
        std::string Version;
        std::vector<BinderFile> Files;

    protected:
        IBinder()                          = default;
        IBinder(const IBinder&)            = default;
        IBinder(IBinder&&)                 = default;
        IBinder& operator=(const IBinder&) = default;
        IBinder& operator=(IBinder&&)      = default;
    };

    // The two halves of a BXF3/BXF4 pair as bytes: the header file (.bhd, .tpfbhd, ...) and the data file (.bdt, ...).
    struct BXFBytes {
        std::vector<uint8_t> Header;
        std::vector<uint8_t> Data;
    };
}  // namespace Souls
