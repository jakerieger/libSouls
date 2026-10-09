//
// Created by Jake Rieger on 10/9/2026.
//

#include "CApiCommon.hpp"

using namespace Souls;
using namespace Souls::CApi;

namespace {
    SoulsFMG* Wrap(FMG Value) {
        auto Result   = std::make_unique<SoulsFMG>();
        Result->Value = std::move(Value);
        return Result.release();
    }

    const FMG::Entry& EntryAt(const SoulsFMG* Fmg, size_t Index) {
        const auto& Entries = Require(Fmg, "fmg is NULL").Value.Entries;
        CheckIndex(Index, Entries.size(), "entry index out of range");
        return Entries[Index];
    }
}  // namespace

SOULS_C_API SoulsFMG* souls_fmg_new(SoulsFmgVersion version) {
    return GuardValue<SoulsFMG*>(nullptr, [&] {
        if (version < SOULS_FMG_DEMONS_SOULS || version > SOULS_FMG_DARK_SOULS_3) ThrowInvalid("invalid FMG version");
        return Wrap(FMG(static_cast<FMG::FMGVersion>(version)));
    });
}

SOULS_C_API SoulsFMG* souls_fmg_read_file(const char* path) {
    return GuardValue<SoulsFMG*>(nullptr, [&] { return Wrap(FMG::Read(PathFrom(path))); });
}

SOULS_C_API SoulsFMG* souls_fmg_read_memory(const uint8_t* data, size_t size) {
    return GuardValue<SoulsFMG*>(nullptr, [&] { return Wrap(FMG::Read(Bytes(data, size))); });
}

SOULS_C_API void souls_fmg_free(SoulsFMG* fmg) {
    delete fmg;
}

SOULS_C_API SoulsStatus souls_fmg_write_file(SoulsFMG* fmg, const char* path) {
    return Guard([&] { Require(fmg, "fmg is NULL").Value.Write(PathFrom(path)); });
}

SOULS_C_API SoulsBuffer souls_fmg_write_memory(SoulsFMG* fmg) {
    return GuardValue(SoulsBuffer{nullptr, 0}, [&] { return MakeBuffer(Require(fmg, "fmg is NULL").Value.Write()); });
}

SOULS_C_API SoulsFmgVersion souls_fmg_version(const SoulsFMG* fmg) {
    return GuardValue(SOULS_FMG_DARK_SOULS_1, [&] { return static_cast<SoulsFmgVersion>(Require(fmg, "fmg is NULL").Value.Version); });
}

SOULS_C_API size_t souls_fmg_entry_count(const SoulsFMG* fmg) {
    return GuardValue<size_t>(0, [&] { return Require(fmg, "fmg is NULL").Value.Entries.size(); });
}

SOULS_C_API int32_t souls_fmg_entry_id(const SoulsFMG* fmg, size_t index) {
    return GuardValue<int32_t>(-1, [&] { return EntryAt(fmg, index).ID; });
}

SOULS_C_API const char* souls_fmg_entry_text(const SoulsFMG* fmg, size_t index) {
    return GuardValue<const char*>(nullptr, [&]() -> const char* {
        const FMG::Entry& Entry = EntryAt(fmg, index);
        return Entry.Text ? Entry.Text->c_str() : nullptr;
    });
}

SOULS_C_API const char* souls_fmg_get_text(const SoulsFMG* fmg, int32_t id) {
    return GuardValue<const char*>(nullptr, [&]() -> const char* {
        const FMG::Entry* Entry = Require(fmg, "fmg is NULL").Value.Find(id);
        return Entry && Entry->Text ? Entry->Text->c_str() : nullptr;
    });
}

SOULS_C_API SoulsStatus souls_fmg_set_text(SoulsFMG* fmg, int32_t id, const char* text_or_null) {
    return Guard([&] {
        Require(fmg, "fmg is NULL").Value.SetText(id, text_or_null ? std::optional<std::string>(text_or_null) : std::nullopt);
    });
}

SOULS_C_API SoulsStatus souls_fmg_remove_entry(SoulsFMG* fmg, size_t index) {
    return Guard([&] {
        auto& Entries = Require(fmg, "fmg is NULL").Value.Entries;
        CheckIndex(index, Entries.size(), "entry index out of range");
        Entries.erase(Entries.begin() + static_cast<std::ptrdiff_t>(index));
    });
}
