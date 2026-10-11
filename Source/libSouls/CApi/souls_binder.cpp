//
// Created by Jake Rieger on 10/9/2026.
//

#include "CApiCommon.hpp"

#include <libSouls/Binders/Regulation.hpp>
#include <libSouls/Binders/SL2.hpp>

using namespace Souls;
using namespace Souls::CApi;

namespace {
    BinderFile& FileAt(SoulsBinder* Binder, size_t Index) {
        std::vector<BinderFile>& Files = Require(Binder, "binder is NULL").Base().Files;
        CheckIndex(Index, Files.size(), "file index out of range");
        return Files[Index];
    }

    const BinderFile& FileAt(const SoulsBinder* Binder, size_t Index) {
        const std::vector<BinderFile>& Files = Require(Binder, "binder is NULL").Base().Files;
        CheckIndex(Index, Files.size(), "file index out of range");
        return Files[Index];
    }

    template<typename T>
    SoulsBinder* Wrap(T Value) {
        auto Result = std::make_unique<SoulsBinder>();
        if constexpr (std::is_same_v<T, BND3>) {
            Result->Bnd3 = std::make_unique<BND3>(std::move(Value));
        } else if constexpr (std::is_same_v<T, BND4>) {
            Result->Bnd4 = std::make_unique<BND4>(std::move(Value));
        } else if constexpr (std::is_same_v<T, BXF3>) {
            Result->Bxf3 = std::make_unique<BXF3>(std::move(Value));
        } else {
            Result->Bxf4 = std::make_unique<BXF4>(std::move(Value));
        }
        return Result.release();
    }

    SoulsBinder* ReadAny(std::span<const uint8_t> Data) {
        if (BND4::Is(Data)) {
            return Wrap(BND4::Read(Data));
        }
        if (BND3::Is(Data)) {
            return Wrap(BND3::Read(Data));
        }
        throw BinaryException("The data is not a BND3 or BND4 binder.");
    }

    BND4 DecryptRegulation(SoulsGame Game, const std::filesystem::path& Path) {
        switch (Game) {
            case SOULS_GAME_DARK_SOULS_3: return Regulation::DecryptDS3(Path);
            case SOULS_GAME_ELDEN_RING: return Regulation::DecryptER(Path);
            default: ThrowInvalid("regulation files are supported for Dark Souls III and Elden Ring");
        }
    }
}  // namespace

SOULS_C_API SoulsBinder* souls_binder_new(SoulsBinderKind kind) {
    return GuardValue<SoulsBinder*>(nullptr, [&] {
        if (kind == SOULS_BINDER_BND3) return Wrap(BND3());
        if (kind == SOULS_BINDER_BND4) return Wrap(BND4());
        if (kind == SOULS_BINDER_BXF3) return Wrap(BXF3());
        if (kind == SOULS_BINDER_BXF4) return Wrap(BXF4());
        ThrowInvalid("invalid binder kind");
    });
}

SOULS_C_API SoulsBinder* souls_binder_read_file(const char* path) {
    return GuardValue<SoulsBinder*>(nullptr, [&] {
        const std::filesystem::path P = PathFrom(path);
        if (BND4::Is(P)) return Wrap(BND4::Read(P));
        if (BND3::Is(P)) return Wrap(BND3::Read(P));
        throw BinaryException("The file is not a BND3 or BND4 binder: " + P.string());
    });
}

SOULS_C_API SoulsBinder* souls_binder_read_memory(const uint8_t* data, size_t size) {
    return GuardValue<SoulsBinder*>(nullptr, [&] { return ReadAny(Bytes(data, size)); });
}

SOULS_C_API void souls_binder_free(SoulsBinder* binder) {
    delete binder;
}

SOULS_C_API SoulsBinderKind souls_binder_kind(const SoulsBinder* binder) {
    return GuardValue(SOULS_BINDER_BND4, [&] {
        const SoulsBinder& B = Require(binder, "binder is NULL");
        if (B.Bnd3) return SOULS_BINDER_BND3;
        if (B.Bxf3) return SOULS_BINDER_BXF3;
        if (B.Bxf4) return SOULS_BINDER_BXF4;
        return SOULS_BINDER_BND4;
    });
}

SOULS_C_API SoulsStatus souls_binder_write_file(SoulsBinder* binder, const char* path) {
    return Guard([&] {
        SoulsBinder& B = Require(binder, "binder is NULL");
        if (B.IsBxf()) throw Unsupported("Split binders are written with souls_bxf_write_files.");
        if (B.Bnd3) B.Bnd3->Write(PathFrom(path));
        else B.Bnd4->Write(PathFrom(path));
    });
}

SOULS_C_API SoulsBuffer souls_binder_write_memory(SoulsBinder* binder) {
    return GuardValue(SoulsBuffer{nullptr, 0}, [&] {
        SoulsBinder& B = Require(binder, "binder is NULL");
        if (B.IsBxf()) throw Unsupported("Split binders are written with souls_bxf_write_memory.");
        return MakeBuffer(B.Bnd3 ? B.Bnd3->Write() : B.Bnd4->Write());
    });
}

SOULS_C_API SoulsDcxType souls_binder_compression(const SoulsBinder* binder) {
    return GuardValue(SOULS_DCX_UNKNOWN, [&] {
        return static_cast<SoulsDcxType>(static_cast<int>(Require(binder, "binder is NULL").Compression()));
    });
}

SOULS_C_API SoulsStatus souls_binder_set_compression(SoulsBinder* binder, SoulsDcxType type) {
    return Guard([&] {
        SoulsBinder& B = Require(binder, "binder is NULL");
        if (B.IsBxf()) throw Unsupported("Split binders have no compression of their own.");
        B.Compression() = DcxFrom(type);
    });
}

SOULS_C_API size_t souls_binder_file_count(const SoulsBinder* binder) {
    return GuardValue<size_t>(0, [&] { return Require(binder, "binder is NULL").Base().Files.size(); });
}

SOULS_C_API int32_t souls_binder_file_id(const SoulsBinder* binder, size_t index) {
    return GuardValue<int32_t>(-1, [&] { return FileAt(binder, index).ID; });
}

SOULS_C_API const char* souls_binder_file_name(const SoulsBinder* binder, size_t index) {
    return GuardValue<const char*>(nullptr, [&]() -> const char* {
        const BinderFile& File = FileAt(binder, index);
        return File.Name ? File.Name->c_str() : nullptr;
    });
}

SOULS_C_API uint8_t souls_binder_file_flags(const SoulsBinder* binder, size_t index) {
    return GuardValue<uint8_t>(0, [&] { return static_cast<uint8_t>(FileAt(binder, index).Flags); });
}

SOULS_C_API const uint8_t* souls_binder_file_bytes(const SoulsBinder* binder, size_t index, size_t* out_size) {
    return GuardValue<const uint8_t*>(nullptr, [&]() -> const uint8_t* {
        const BinderFile& File = FileAt(binder, index);
        if (out_size) *out_size = File.Bytes.size();
        return File.Bytes.data();
    });
}

SOULS_C_API int64_t souls_binder_find_file(const SoulsBinder* binder, const char* name) {
    return GuardValue<int64_t>(-1, [&]() -> int64_t {
        if (!name) ThrowInvalid("name is NULL");
        const auto& Files = Require(binder, "binder is NULL").Base().Files;
        for (size_t I = 0; I < Files.size(); ++I) {
            if (Files[I].Name && *Files[I].Name == name) return static_cast<int64_t>(I);
        }
        return -1;
    });
}

SOULS_C_API int64_t souls_binder_find_file_suffix(const SoulsBinder* binder, const char* suffix) {
    return GuardValue<int64_t>(-1, [&]() -> int64_t {
        if (!suffix) ThrowInvalid("suffix is NULL");
        const std::string Suffix(suffix);
        const auto& Files = Require(binder, "binder is NULL").Base().Files;
        for (size_t I = 0; I < Files.size(); ++I) {
            const auto& Name = Files[I].Name;
            if (Name && Name->size() >= Suffix.size() && Name->compare(Name->size() - Suffix.size(), Suffix.size(), Suffix) == 0) {
                return static_cast<int64_t>(I);
            }
        }
        return -1;
    });
}

SOULS_C_API SoulsStatus souls_binder_set_file_bytes(SoulsBinder* binder, size_t index, const uint8_t* data, size_t size) {
    return Guard([&] {
        const auto Span = Bytes(data, size);
        FileAt(binder, index).Bytes.assign(Span.begin(), Span.end());
    });
}

SOULS_C_API SoulsStatus souls_binder_set_file_id(SoulsBinder* binder, size_t index, int32_t id) {
    return Guard([&] { FileAt(binder, index).ID = id; });
}

SOULS_C_API SoulsStatus souls_binder_set_file_name(SoulsBinder* binder, size_t index, const char* name_or_null) {
    return Guard([&] {
        BinderFile& File = FileAt(binder, index);
        if (name_or_null) File.Name = std::string(name_or_null);
        else File.Name.reset();
    });
}

SOULS_C_API SoulsStatus souls_binder_set_file_flags(SoulsBinder* binder, size_t index, uint8_t flags) {
    return Guard([&] { FileAt(binder, index).Flags = static_cast<Binder::FileFlags>(flags); });
}

SOULS_C_API int64_t souls_binder_add_file(SoulsBinder* binder, int32_t id, const char* name_or_null, uint8_t flags,
                                          const uint8_t* data, size_t size) {
    return GuardValue<int64_t>(-1, [&]() -> int64_t {
        auto& Files     = Require(binder, "binder is NULL").Base().Files;
        const auto Span = Bytes(data, size);
        BinderFile File;
        File.Flags = static_cast<Binder::FileFlags>(flags);
        File.ID    = id;
        if (name_or_null) File.Name = std::string(name_or_null);
        File.Bytes.assign(Span.begin(), Span.end());
        Files.push_back(std::move(File));
        return static_cast<int64_t>(Files.size() - 1);
    });
}

SOULS_C_API SoulsStatus souls_binder_remove_file(SoulsBinder* binder, size_t index) {
    return Guard([&] {
        auto& Files = Require(binder, "binder is NULL").Base().Files;
        CheckIndex(index, Files.size(), "file index out of range");
        Files.erase(Files.begin() + static_cast<std::ptrdiff_t>(index));
    });
}

SOULS_C_API SoulsBinder* souls_regulation_decrypt(SoulsGame game, const char* path) {
    return GuardValue<SoulsBinder*>(nullptr, [&] { return Wrap(DecryptRegulation(game, PathFrom(path))); });
}

SOULS_C_API SoulsStatus souls_regulation_encrypt(SoulsGame game, const char* path, SoulsBinder* binder) {
    return Guard([&] {
        SoulsBinder& B = Require(binder, "binder is NULL");
        if (!B.Bnd4) ThrowInvalid("regulation files are BND4 binders");
        switch (game) {
            case SOULS_GAME_DARK_SOULS_3: Regulation::EncryptDS3(PathFrom(path), *B.Bnd4); break;
            case SOULS_GAME_ELDEN_RING: Regulation::EncryptER(PathFrom(path), *B.Bnd4); break;
            default: ThrowInvalid("regulation files are supported for Dark Souls III and Elden Ring");
        }
    });
}

#pragma region Saves
namespace {
    SL2::Game SaveGame(SoulsSaveKind Kind) {
        switch (Kind) {
            case SOULS_SAVE_DARK_SOULS_2: return SL2::Game::DarkSouls2;
            case SOULS_SAVE_DARK_SOULS_2_SCHOLAR: return SL2::Game::DarkSouls2Scholar;
            case SOULS_SAVE_DARK_SOULS_3: return SL2::Game::DarkSouls3;
            case SOULS_SAVE_ELDEN_RING: return SL2::Game::EldenRing;
        }
        ThrowInvalid("invalid save kind");
    }

    const BND4& SaveBinder(const SoulsBinder* Binder) {
        const SoulsBinder& B = Require(Binder, "binder is NULL");
        if (!B.Bnd4) ThrowInvalid("save files are BND4 binders");
        return *B.Bnd4;
    }
}  // namespace

SOULS_C_API SoulsBinder* souls_save_read_file(SoulsSaveKind kind, const char* path) {
    return GuardValue<SoulsBinder*>(nullptr, [&] { return Wrap(SL2::Read(PathFrom(path), SaveGame(kind))); });
}

SOULS_C_API SoulsBinder* souls_save_read_memory(SoulsSaveKind kind, const uint8_t* data, size_t size) {
    return GuardValue<SoulsBinder*>(nullptr, [&] { return Wrap(SL2::Read(Bytes(data, size), SaveGame(kind))); });
}

SOULS_C_API SoulsStatus souls_save_write_file(SoulsSaveKind kind, const char* path, const SoulsBinder* binder) {
    return Guard([&] { SL2::Write(PathFrom(path), SaveBinder(binder), SaveGame(kind)); });
}

SOULS_C_API SoulsBuffer souls_save_write_memory(SoulsSaveKind kind, const SoulsBinder* binder) {
    return GuardValue(SoulsBuffer{nullptr, 0}, [&] { return MakeBuffer(SL2::Write(SaveBinder(binder), SaveGame(kind))); });
}
#pragma endregion
