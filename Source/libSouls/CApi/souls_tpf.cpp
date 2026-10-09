//
// Created by Jake Rieger on 10/9/2026.
//

#include "CApiCommon.hpp"

using namespace Souls;
using namespace Souls::CApi;

namespace {
    SoulsTPF* Wrap(TPF Value) {
        auto Result   = std::make_unique<SoulsTPF>();
        Result->Value = std::move(Value);
        return Result.release();
    }

    TPF::Texture& TextureAt(SoulsTPF* Tpf, size_t Index) {
        auto& Textures = Require(Tpf, "tpf is NULL").Value.Textures;
        CheckIndex(Index, Textures.size(), "texture index out of range");
        return Textures[Index];
    }

    const TPF::Texture& TextureAt(const SoulsTPF* Tpf, size_t Index) {
        const auto& Textures = Require(Tpf, "tpf is NULL").Value.Textures;
        CheckIndex(Index, Textures.size(), "texture index out of range");
        return Textures[Index];
    }
}  // namespace

SOULS_C_API SoulsTPF* souls_tpf_new(void) {
    return GuardValue<SoulsTPF*>(nullptr, [&] { return Wrap(TPF()); });
}

SOULS_C_API SoulsTPF* souls_tpf_read_file(const char* path) {
    return GuardValue<SoulsTPF*>(nullptr, [&] { return Wrap(TPF::Read(PathFrom(path))); });
}

SOULS_C_API SoulsTPF* souls_tpf_read_memory(const uint8_t* data, size_t size) {
    return GuardValue<SoulsTPF*>(nullptr, [&] { return Wrap(TPF::Read(Bytes(data, size))); });
}

SOULS_C_API void souls_tpf_free(SoulsTPF* tpf) {
    delete tpf;
}

SOULS_C_API SoulsStatus souls_tpf_write_file(SoulsTPF* tpf, const char* path) {
    return Guard([&] { Require(tpf, "tpf is NULL").Value.Write(PathFrom(path)); });
}

SOULS_C_API SoulsBuffer souls_tpf_write_memory(SoulsTPF* tpf) {
    return GuardValue(SoulsBuffer{nullptr, 0}, [&] { return MakeBuffer(Require(tpf, "tpf is NULL").Value.Write()); });
}

SOULS_C_API size_t souls_tpf_texture_count(const SoulsTPF* tpf) {
    return GuardValue<size_t>(0, [&] { return Require(tpf, "tpf is NULL").Value.Textures.size(); });
}

SOULS_C_API const char* souls_tpf_texture_name(const SoulsTPF* tpf, size_t index) {
    return GuardValue<const char*>(nullptr, [&] { return TextureAt(tpf, index).Name.c_str(); });
}

SOULS_C_API SoulsStatus souls_tpf_set_texture_name(SoulsTPF* tpf, size_t index, const char* name) {
    return Guard([&] {
        if (!name) ThrowInvalid("name is NULL");
        TextureAt(tpf, index).Name = name;
    });
}

SOULS_C_API const uint8_t* souls_tpf_texture_bytes(const SoulsTPF* tpf, size_t index, size_t* out_size) {
    return GuardValue<const uint8_t*>(nullptr, [&] {
        const TPF::Texture& Texture = TextureAt(tpf, index);
        if (out_size) *out_size = Texture.Bytes.size();
        return Texture.Bytes.data();
    });
}

SOULS_C_API SoulsStatus souls_tpf_set_texture_bytes(SoulsTPF* tpf, size_t index, const uint8_t* data, size_t size) {
    return Guard([&] {
        const auto Span = Bytes(data, size);
        TextureAt(tpf, index).Bytes.assign(Span.begin(), Span.end());
    });
}

SOULS_C_API int64_t souls_tpf_add_texture(SoulsTPF* tpf, const char* name, uint8_t format, uint8_t flags1,
                                          const uint8_t* dds, size_t size) {
    return GuardValue<int64_t>(-1, [&]() -> int64_t {
        if (!name) ThrowInvalid("name is NULL");
        auto& Textures  = Require(tpf, "tpf is NULL").Value.Textures;
        const auto Span = Bytes(dds, size);
        Textures.emplace_back(name, format, flags1, std::vector<uint8_t>(Span.begin(), Span.end()));
        return static_cast<int64_t>(Textures.size() - 1);
    });
}

SOULS_C_API SoulsStatus souls_tpf_remove_texture(SoulsTPF* tpf, size_t index) {
    return Guard([&] {
        auto& Textures = Require(tpf, "tpf is NULL").Value.Textures;
        CheckIndex(index, Textures.size(), "texture index out of range");
        Textures.erase(Textures.begin() + static_cast<std::ptrdiff_t>(index));
    });
}
