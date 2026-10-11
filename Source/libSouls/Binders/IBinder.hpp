/// @file
/// @author Jake Rieger
/// @date 10/7/2026

#pragma once

#include "Binder.hpp"
#include "BinderFile.hpp"

#include <string>
#include <vector>

namespace Souls {
    /// @brief What every binder format (BND3, BND4, ...) has in common.
    class IBinder {
    public:
        virtual ~IBinder() = default;

        /// @brief Flags indicating features of the binder.
        Binder::Format Format = Binder::Format::None;
        /// @brief Binder timestamp/version string, at most 8 bytes.
        std::string Version;
        /// @brief Files in this binder.
        std::vector<BinderFile> Files;

    protected:
        IBinder()                          = default;
        /// @brief Binders are copyable and movable values.
        IBinder(const IBinder&)            = default;
        /// @brief Binders are copyable and movable values.
        IBinder(IBinder&&)                 = default;
        /// @brief Binders are copyable and movable values.
        IBinder& operator=(const IBinder&) = default;
        /// @brief Binders are copyable and movable values.
        IBinder& operator=(IBinder&&)      = default;
    };

    /// @brief The two halves of a BXF3/BXF4 pair as bytes: the header file (.bhd, .tpfbhd, ...) and the data file
    ///        (.bdt, ...).
    struct BXFBytes {
        /// @brief The header file (.bhd, .tpfbhd, ...).
        std::vector<uint8_t> Header;
        /// @brief The data file (.bdt, .tpfbdt, ...).
        std::vector<uint8_t> Data;
    };
}  // namespace Souls
