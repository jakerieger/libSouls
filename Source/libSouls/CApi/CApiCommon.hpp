// Copyright (C) 2026 Jake Rieger
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, version 3. This program is distributed in the hope that it will
// be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU General
// Public License along with this program. If not, see https://www.gnu.org/licenses/.

#pragma once

/// @brief Internal helpers shared by the C API implementation files.
///
/// Not part of the public interface.

#include "souls.h"

#include <libSouls/Binders/BND3.hpp>
#include <libSouls/Binders/BND4.hpp>
#include <libSouls/Binders/BXF3.hpp>
#include <libSouls/Binders/BXF4.hpp>
#include <libSouls/Endian.hpp>
#include <libSouls/Formats/DCX.hpp>
#include <libSouls/Formats/FMG.hpp>
#include <libSouls/Formats/PARAM.hpp>
#include <libSouls/Formats/ParamDefRepository.hpp>
#include <libSouls/Formats/ParamLayout.hpp>
#include <libSouls/Formats/ParamMetadata.hpp>
#include <libSouls/Formats/TPF.hpp>

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#pragma warning(push)
#pragma warning(disable : 4251)

/// @brief The opaque handle types.
///
/// Each owns the C++ object it stands for.
struct SoulsBinder {
    std::unique_ptr<Souls::BND3> Bnd3;
    std::unique_ptr<Souls::BND4> Bnd4;
    std::unique_ptr<Souls::BXF3> Bxf3;
    std::unique_ptr<Souls::BXF4> Bxf4;
    /// @brief BXF binders have no compression of their own.
    Souls::DCX::Type NoCompression = Souls::DCX::Type::None;

    Souls::IBinder& Base() {
        if (Bnd3) return *Bnd3;
        if (Bnd4) return *Bnd4;
        if (Bxf3) return *Bxf3;
        return *Bxf4;
    }
    const Souls::IBinder& Base() const { return const_cast<SoulsBinder*>(this)->Base(); }
    bool IsBxf() const { return Bxf3 || Bxf4; }
    Souls::DCX::Type& Compression() {
        if (Bnd3) return Bnd3->Compression;
        if (Bnd4) return Bnd4->Compression;
        return NoCompression;
    }
    const Souls::DCX::Type& Compression() const { return const_cast<SoulsBinder*>(this)->Compression(); }
};

struct SoulsFMG {
    Souls::FMG Value;
};

struct SoulsParam {
    Souls::PARAM Value;
};

struct SoulsParamDefs {
    Souls::ParamDefRepository Repository;
    std::vector<std::string> LoadErrors;
};

struct SoulsParamLayout {
    explicit SoulsParamLayout(Souls::ParamLayout Layout) : Value(std::move(Layout)) {}
    Souls::ParamLayout Value;
    /// @brief Backs the text returned by souls_param_get_string.
    std::string Scratch;
};

struct SoulsParamNames {
    Souls::ParamNameRepository Repository;
    std::vector<std::string> LoadErrors;
};

struct SoulsParamEnums {
    Souls::ParamEnumRepository Repository;
    std::vector<std::string> LoadErrors;
};

struct SoulsTPF {
    Souls::TPF Value;
};

#pragma warning(pop)

namespace Souls::CApi {
    /// @brief Records the failure for the calling thread.
    void SetError(SoulsStatus Status, std::string Message);

    /// @brief Runs a function that may throw and turns what it throws into a status and a recorded message.
    template<typename F>
    SoulsStatus Guard(F&& Fn) noexcept;

    SoulsStatus TranslateCurrentException() noexcept;

    template<typename F>
    SoulsStatus Guard(F&& Fn) noexcept {
        try {
            Fn();
            return SOULS_OK;
        } catch (...) { return TranslateCurrentException(); }
    }

    /// @brief For functions that return a pointer, a number or a buffer: runs Fn and returns its result, or Fallback if
    ///        it threw.
    template<typename T, typename F>
    T GuardValue(T Fallback, F&& Fn) noexcept {
        try {
            return Fn();
        } catch (...) {
            TranslateCurrentException();
            return Fallback;
        }
    }

    /// @brief A request that is valid but not supported for this object; reported as SOULS_ERR_UNSUPPORTED.
    struct Unsupported : std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    /// @brief A lookup that found nothing; reported as SOULS_ERR_NOT_FOUND.
    struct NotFound : std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    [[noreturn]] void ThrowInvalid(const char* Message);

    template<typename T>
    T& Require(T* Pointer, const char* What) {
        if (!Pointer) { ThrowInvalid(What); }
        return *Pointer;
    }

    inline void CheckIndex(size_t Index, size_t Count, const char* What) {
        if (Index >= Count) { ThrowInvalid(What); }
    }

    /// @brief A UTF-8 path from the C side.
    std::filesystem::path PathFrom(const char* Utf8);

    std::span<const uint8_t> Bytes(const uint8_t* Data, size_t Size);

    /// @brief Copies bytes into a new buffer the caller owns.
    SoulsBuffer MakeBuffer(const uint8_t* Data, size_t Size);
    inline SoulsBuffer MakeBuffer(const std::vector<uint8_t>& Data) {
        return MakeBuffer(Data.data(), Data.size());
    }

    SoulsStatus Fail(SoulsStatus Status, const char* Message);

    /// @brief The whole contents of a file.
    std::vector<uint8_t> ReadAllBytes(const std::filesystem::path& Path);

    /// @brief The C and C++ compression type lists have the same order (checked in souls_core.cpp).
    inline DCX::Type DcxFrom(SoulsDcxType Type) {
        if (static_cast<int>(Type) < SOULS_DCX_UNKNOWN || static_cast<int>(Type) > SOULS_DCX_DCX_ZSTD) {
            ThrowInvalid("invalid compression type");
        }
        return static_cast<DCX::Type>(static_cast<int>(Type));
    }
}  // namespace Souls::CApi
