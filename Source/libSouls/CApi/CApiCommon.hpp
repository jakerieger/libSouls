//
// Created by Jake Rieger on 10/9/2026.
//

#pragma once

// Internal helpers shared by the C API implementation files. Not part of the public interface.

#include "souls.h"

#include <libSouls/Binders/BND3.hpp>
#include <libSouls/Binders/BND4.hpp>
#include <libSouls/Endian.hpp>
#include <libSouls/Formats/DCX.hpp>
#include <libSouls/Formats/FMG.hpp>
#include <libSouls/Formats/PARAM.hpp>
#include <libSouls/Formats/ParamDefRepository.hpp>
#include <libSouls/Formats/ParamLayout.hpp>
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

// The opaque handle types. Each owns the C++ object it stands for.
struct SoulsBinder {
    std::unique_ptr<Souls::BND3> Bnd3;
    std::unique_ptr<Souls::BND4> Bnd4;

    Souls::IBinder& Base() { return Bnd3 ? static_cast<Souls::IBinder&>(*Bnd3) : static_cast<Souls::IBinder&>(*Bnd4); }
    const Souls::IBinder& Base() const { return Bnd3 ? static_cast<const Souls::IBinder&>(*Bnd3) : static_cast<const Souls::IBinder&>(*Bnd4); }
    Souls::DCX::Type& Compression() { return Bnd3 ? Bnd3->Compression : Bnd4->Compression; }
    const Souls::DCX::Type& Compression() const { return Bnd3 ? Bnd3->Compression : Bnd4->Compression; }
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
    // Backs the text returned by souls_param_get_string.
    std::string Scratch;
};

struct SoulsTPF {
    Souls::TPF Value;
};

#pragma warning(pop)

namespace Souls::CApi {
    // Records the failure for the calling thread.
    void SetError(SoulsStatus Status, std::string Message);

    // Runs a function that may throw and turns what it throws into a status and a recorded message.
    template<typename F>
    SoulsStatus Guard(F&& Fn) noexcept;

    SoulsStatus TranslateCurrentException() noexcept;

    template<typename F>
    SoulsStatus Guard(F&& Fn) noexcept {
        try {
            Fn();
            return SOULS_OK;
        } catch (...) {
            return TranslateCurrentException();
        }
    }

    // For functions that return a pointer, a number or a buffer: runs Fn and returns its result, or Fallback if it
    // threw.
    template<typename T, typename F>
    T GuardValue(T Fallback, F&& Fn) noexcept {
        try {
            return Fn();
        } catch (...) {
            TranslateCurrentException();
            return Fallback;
        }
    }

    [[noreturn]] void ThrowInvalid(const char* Message);

    template<typename T>
    T& Require(T* Pointer, const char* What) {
        if (!Pointer) {
            ThrowInvalid(What);
        }
        return *Pointer;
    }

    inline void CheckIndex(size_t Index, size_t Count, const char* What) {
        if (Index >= Count) {
            ThrowInvalid(What);
        }
    }

    // A UTF-8 path from the C side.
    std::filesystem::path PathFrom(const char* Utf8);

    std::span<const uint8_t> Bytes(const uint8_t* Data, size_t Size);

    // Copies bytes into a new buffer the caller owns.
    SoulsBuffer MakeBuffer(const uint8_t* Data, size_t Size);
    inline SoulsBuffer MakeBuffer(const std::vector<uint8_t>& Data) {
        return MakeBuffer(Data.data(), Data.size());
    }

    SoulsStatus Fail(SoulsStatus Status, const char* Message);

    // The C and C++ compression type lists have the same order (checked in souls_core.cpp).
    inline DCX::Type DcxFrom(SoulsDcxType Type) {
        if (static_cast<int>(Type) < SOULS_DCX_UNKNOWN || static_cast<int>(Type) > SOULS_DCX_DCX_ZSTD) {
            ThrowInvalid("invalid compression type");
        }
        return static_cast<DCX::Type>(static_cast<int>(Type));
    }
}  // namespace Souls::CApi
