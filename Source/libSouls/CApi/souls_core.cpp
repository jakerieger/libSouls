//
// Created by Jake Rieger on 10/9/2026.
//

#include "CApiCommon.hpp"

#include <libSouls/Formats/DCX.hpp>
#include <libSouls/Oodle26.hpp>

#include <fstream>

namespace Souls::CApi {
    namespace {
        struct ErrorState {
            SoulsStatus Status = SOULS_OK;
            std::string Message;
        };

        ErrorState& State() {
            thread_local ErrorState Instance;
            return Instance;
        }
    }  // namespace

    void SetError(SoulsStatus Status, std::string Message) {
        ErrorState& S = State();
        S.Status      = Status;
        S.Message     = std::move(Message);
    }

    SoulsStatus Fail(SoulsStatus Status, const char* Message) {
        SetError(Status, Message);
        return Status;
    }

    SoulsStatus TranslateCurrentException() noexcept {
        try {
            throw;
        } catch (const BinaryException& E) {
            const std::string Message = E.what();
            const bool IsIo           = Message.rfind("Failed to open", 0) == 0 || Message.rfind("Failed to write", 0) == 0 ||
                              Message.rfind("Failed to create", 0) == 0;
            SetError(IsIo ? SOULS_ERR_IO : SOULS_ERR_PARSE, Message);
            return State().Status;
        } catch (const std::invalid_argument& E) {
            SetError(SOULS_ERR_INVALID_ARGUMENT, E.what());
        } catch (const std::out_of_range& E) {
            SetError(SOULS_ERR_INVALID_ARGUMENT, E.what());
        } catch (const std::filesystem::filesystem_error& E) {
            SetError(SOULS_ERR_IO, E.what());
        } catch (const std::ios_base::failure& E) {
            SetError(SOULS_ERR_IO, E.what());
        } catch (const std::exception& E) {
            SetError(SOULS_ERR_INTERNAL, E.what());
        } catch (...) {
            SetError(SOULS_ERR_INTERNAL, "unknown error");
        }
        return State().Status;
    }

    void ThrowInvalid(const char* Message) {
        throw std::invalid_argument(Message);
    }

    std::filesystem::path PathFrom(const char* Utf8) {
        if (!Utf8) {
            ThrowInvalid("path is NULL");
        }
        return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(Utf8)));
    }

    std::span<const uint8_t> Bytes(const uint8_t* Data, size_t Size) {
        if (!Data && Size != 0) {
            ThrowInvalid("data is NULL");
        }
        return {Data, Size};
    }

    SoulsBuffer MakeBuffer(const uint8_t* Data, size_t Size) {
        SoulsBuffer Result{nullptr, 0};
        if (Size == 0) {
            return Result;
        }
        Result.data = static_cast<uint8_t*>(std::malloc(Size));
        if (!Result.data) {
            throw std::bad_alloc();
        }
        std::memcpy(Result.data, Data, Size);
        Result.size = Size;
        return Result;
    }
}  // namespace Souls::CApi

using namespace Souls;
using namespace Souls::CApi;

// The C and C++ type lists must agree; the conversions below are plain casts.
static_assert(static_cast<int>(DCX::Type::Unknown) == SOULS_DCX_UNKNOWN);
static_assert(static_cast<int>(DCX::Type::None) == SOULS_DCX_NONE);
static_assert(static_cast<int>(DCX::Type::Zlib) == SOULS_DCX_ZLIB);
static_assert(static_cast<int>(DCX::Type::DCP_EDGE) == SOULS_DCX_DCP_EDGE);
static_assert(static_cast<int>(DCX::Type::DCP_DFLT) == SOULS_DCX_DCP_DFLT);
static_assert(static_cast<int>(DCX::Type::DCX_EDGE) == SOULS_DCX_DCX_EDGE);
static_assert(static_cast<int>(DCX::Type::DCX_DFLT_10000_24_9) == SOULS_DCX_DCX_DFLT_10000_24_9);
static_assert(static_cast<int>(DCX::Type::DCX_DFLT_10000_44_9) == SOULS_DCX_DCX_DFLT_10000_44_9);
static_assert(static_cast<int>(DCX::Type::DCX_DFLT_11000_44_8) == SOULS_DCX_DCX_DFLT_11000_44_8);
static_assert(static_cast<int>(DCX::Type::DCX_DFLT_11000_44_9) == SOULS_DCX_DCX_DFLT_11000_44_9);
static_assert(static_cast<int>(DCX::Type::DCX_DFLT_11000_44_9_15) == SOULS_DCX_DCX_DFLT_11000_44_9_15);
static_assert(static_cast<int>(DCX::Type::DCX_KRAK_6) == SOULS_DCX_DCX_KRAK_6);
static_assert(static_cast<int>(DCX::Type::DCX_KRAK_9) == SOULS_DCX_DCX_KRAK_9);
static_assert(static_cast<int>(DCX::Type::DCX_ZSTD) == SOULS_DCX_DCX_ZSTD);

SOULS_C_API const char* souls_version(void) {
    return "0.1.0";
}

SOULS_C_API const char* souls_last_error(void) {
    return CApi::State().Message.c_str();
}

SOULS_C_API SoulsStatus souls_last_error_code(void) {
    return CApi::State().Status;
}

SOULS_C_API void souls_buffer_free(SoulsBuffer buffer) {
    std::free(buffer.data);
}

SOULS_C_API SoulsBuffer souls_read_file(const char* path) {
    return GuardValue(SoulsBuffer{nullptr, 0}, [&] {
        const std::filesystem::path P = PathFrom(path);
        std::ifstream File(P, std::ios::binary | std::ios::ate);
        if (!File) {
            throw BinaryException("Failed to open file for reading: " + P.string());
        }
        const std::streamsize Size = File.tellg();
        File.seekg(0);
        std::vector<uint8_t> Data(static_cast<size_t>(Size));
        if (Size > 0 && !File.read(reinterpret_cast<char*>(Data.data()), Size)) {
            throw BinaryException("Failed to read file: " + P.string());
        }
        return MakeBuffer(Data);
    });
}

SOULS_C_API int souls_oodle_set_dll_path(const char* path) {
    return GuardValue(0, [&] { return Oodle26::SetDLLPath(PathFrom(path)) ? 1 : 0; });
}

SOULS_C_API int souls_oodle_is_available(void) {
    return GuardValue(0, [&] { return Oodle26::IsAvailable() ? 1 : 0; });
}

SOULS_C_API SoulsDcxType souls_dcx_default_type(SoulsGame game) {
    return GuardValue(SOULS_DCX_UNKNOWN, [&] {
        switch (game) {
            case SOULS_GAME_DEMONS_SOULS: return static_cast<SoulsDcxType>(DCX::DefaultType::DemonsSouls);
            case SOULS_GAME_DARK_SOULS_1: return static_cast<SoulsDcxType>(DCX::DefaultType::DarkSouls1);
            case SOULS_GAME_DARK_SOULS_2: return static_cast<SoulsDcxType>(DCX::DefaultType::DarkSouls2);
            case SOULS_GAME_BLOODBORNE: return static_cast<SoulsDcxType>(DCX::DefaultType::Bloodborne);
            case SOULS_GAME_DARK_SOULS_3: return static_cast<SoulsDcxType>(DCX::DefaultType::DarkSouls3);
            case SOULS_GAME_SEKIRO: return static_cast<SoulsDcxType>(DCX::DefaultType::Sekiro);
            case SOULS_GAME_ELDEN_RING: return static_cast<SoulsDcxType>(DCX::DefaultType::EldenRing);
            case SOULS_GAME_ARMORED_CORE_6: return static_cast<SoulsDcxType>(DCX::DefaultType::ArmoredCore6);
        }
        ThrowInvalid("invalid game");
    });
}

SOULS_C_API int souls_dcx_is(const uint8_t* data, size_t size) {
    return GuardValue(0, [&] { return DCX::Is(Bytes(data, size)) ? 1 : 0; });
}

SOULS_C_API SoulsBuffer souls_dcx_decompress(const uint8_t* data, size_t size, SoulsDcxType* out_type) {
    return GuardValue(SoulsBuffer{nullptr, 0}, [&] {
        DCX::Type Kind = DCX::Type::Unknown;
        const std::vector<uint8_t> Result = DCX::Decompress(Bytes(data, size), Kind);
        if (out_type) {
            *out_type = static_cast<SoulsDcxType>(static_cast<int>(Kind));
        }
        return MakeBuffer(Result);
    });
}

SOULS_C_API SoulsBuffer souls_dcx_compress(const uint8_t* data, size_t size, SoulsDcxType type) {
    return GuardValue(SoulsBuffer{nullptr, 0}, [&] { return MakeBuffer(DCX::Compress(Bytes(data, size), DcxFrom(type))); });
}
