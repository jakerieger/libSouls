// Copyright (C) 2026 Jake Rieger
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, version 3. This program is distributed in the hope that it will
// be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU General
// Public License along with this program. If not, see https://www.gnu.org/licenses/.

#include "CApiCommon.hpp"

using namespace Souls;
using namespace Souls::CApi;

namespace {
    double Number(const PARAMDEF::EditorValue& Value, int* Has) {
        *Has = 1;
        if (const auto* I = std::get_if<int32_t>(&Value)) return *I;
        if (const auto* F = std::get_if<float>(&Value)) return *F;
        if (const auto* D = std::get_if<double>(&Value)) return *D;
        *Has = 0;
        return 0;
    }

    void RecordErrors(std::vector<std::string>& Target, const MetadataLoadResult& Result) {
        Target.clear();
        for (const MetadataLoadError& Error : Result.Errors) {
            Target.push_back(Error.Path.string() + ": " + Error.Message);
        }
    }

    const char* NameArg(const char* Name) {
        if (!Name) ThrowInvalid("name is NULL");
        return Name;
    }

    int64_t ToInt64(const PARAMTDF::Value& Value) {
        return std::visit([](auto V) { return static_cast<int64_t>(V); }, Value);
    }

    const PARAMTDF& EnumOf(const SoulsParamEnums* Handle, const char* Name) {
        const PARAMTDF* Found = Require(Handle, "enums is NULL").Repository.Find(NameArg(Name));
        if (!Found) throw NotFound(std::string("no such enum: ") + Name);
        return *Found;
    }
}  // namespace

SOULS_C_API SoulsStatus souls_param_layout_field_info(const SoulsParamLayout* layout,
                                                      size_t field,
                                                      SoulsFieldInfo* out_info) {
    return Guard([&] {
        if (!out_info) ThrowInvalid("out_info is NULL");
        if (out_info->struct_size < sizeof(SoulsFieldInfo)) ThrowInvalid("struct_size is too small");
        const ParamLayout& L = Require(layout, "layout is NULL").Value;
        CheckIndex(field, L.FieldCount(), "field index out of range");
        const PARAMDEF::Field& F = L.Definition(field);
        out_info->display_name   = F.DisplayName.c_str();
        out_info->description    = F.Description ? F.Description->c_str() : nullptr;
        out_info->internal_type  = F.InternalType.c_str();
        out_info->display_format = F.DisplayFormat.c_str();
        out_info->array_length   = F.ArrayLength;
        out_info->bit_size       = F.BitSize;
        out_info->sort_id        = F.SortID;
        out_info->edit_flags     = static_cast<int32_t>(F.EditorFlags);
        out_info->default_value  = Number(F.Default, &out_info->has_default);
        out_info->minimum        = Number(F.Minimum, &out_info->has_minimum);
        out_info->maximum        = Number(F.Maximum, &out_info->has_maximum);
        out_info->increment      = Number(F.Increment, &out_info->has_increment);
    });
}

#pragma region Names
SOULS_C_API SoulsParamNames* souls_param_names_new(void) {
    return GuardValue<SoulsParamNames*>(nullptr, [&] { return new SoulsParamNames(); });
}

SOULS_C_API void souls_param_names_free(SoulsParamNames* names) {
    delete names;
}

SOULS_C_API SoulsStatus souls_param_names_add_folder(SoulsParamNames* names, const char* folder) {
    return Guard([&] {
        SoulsParamNames& N = Require(names, "names is NULL");
        RecordErrors(N.LoadErrors, N.Repository.AddFolder(PathFrom(folder)));
    });
}

SOULS_C_API size_t souls_param_names_param_count(const SoulsParamNames* names) {
    return GuardValue<size_t>(0, [&] { return Require(names, "names is NULL").Repository.Size(); });
}

SOULS_C_API const char* souls_param_names_find(const SoulsParamNames* names, const char* param_name, int32_t id) {
    return GuardValue<const char*>(nullptr, [&]() -> const char* {
        const auto* Table = Require(names, "names is NULL").Repository.Find(NameArg(param_name));
        if (!Table) return nullptr;
        const auto Found = Table->find(id);
        return Found == Table->end() ? nullptr : Found->second.c_str();
    });
}

SOULS_C_API int64_t souls_param_names_apply(const SoulsParamNames* names,
                                            SoulsParam* param,
                                            const char* param_name,
                                            int overwrite) {
    return GuardValue<int64_t>(-1, [&]() -> int64_t {
        return static_cast<int64_t>(
          Require(names, "names is NULL")
            .Repository.Apply(Require(param, "param is NULL").Value, NameArg(param_name), overwrite != 0));
    });
}

SOULS_C_API size_t souls_param_names_load_error_count(const SoulsParamNames* names) {
    return GuardValue<size_t>(0, [&] { return Require(names, "names is NULL").LoadErrors.size(); });
}

SOULS_C_API const char* souls_param_names_load_error(const SoulsParamNames* names, size_t index) {
    return GuardValue<const char*>(nullptr, [&] {
        const auto& Errors = Require(names, "names is NULL").LoadErrors;
        CheckIndex(index, Errors.size(), "error index out of range");
        return Errors[index].c_str();
    });
}
#pragma endregion

#pragma region Enums
SOULS_C_API SoulsParamEnums* souls_param_enums_new(void) {
    return GuardValue<SoulsParamEnums*>(nullptr, [&] { return new SoulsParamEnums(); });
}

SOULS_C_API void souls_param_enums_free(SoulsParamEnums* enums) {
    delete enums;
}

SOULS_C_API SoulsStatus souls_param_enums_add_folder(SoulsParamEnums* enums, const char* folder) {
    return Guard([&] {
        SoulsParamEnums& E = Require(enums, "enums is NULL");
        RecordErrors(E.LoadErrors, E.Repository.AddFolder(PathFrom(folder)));
    });
}

SOULS_C_API size_t souls_param_enums_count(const SoulsParamEnums* enums) {
    return GuardValue<size_t>(0, [&] { return Require(enums, "enums is NULL").Repository.Size(); });
}

SOULS_C_API const char* souls_param_enums_name(const SoulsParamEnums* enums, size_t index) {
    return GuardValue<const char*>(nullptr, [&] {
        const auto All = Require(enums, "enums is NULL").Repository.All();
        CheckIndex(index, All.size(), "enum index out of range");
        return All[index]->Name.c_str();
    });
}

SOULS_C_API size_t souls_param_enums_entry_count(const SoulsParamEnums* enums, const char* enum_name) {
    return GuardValue<size_t>(0, [&] {
        const PARAMTDF* Found = Require(enums, "enums is NULL").Repository.Find(NameArg(enum_name));
        return Found ? Found->Entries.size() : size_t(0);
    });
}

SOULS_C_API int64_t souls_param_enums_entry_value(const SoulsParamEnums* enums, const char* enum_name, size_t entry) {
    return GuardValue<int64_t>(0, [&] {
        const PARAMTDF& E = EnumOf(enums, enum_name);
        CheckIndex(entry, E.Entries.size(), "entry index out of range");
        return ToInt64(E.Entries[entry].Val);
    });
}

SOULS_C_API const char*
souls_param_enums_entry_name(const SoulsParamEnums* enums, const char* enum_name, size_t entry) {
    return GuardValue<const char*>(nullptr, [&]() -> const char* {
        const PARAMTDF& E = EnumOf(enums, enum_name);
        CheckIndex(entry, E.Entries.size(), "entry index out of range");
        const auto& Name = E.Entries[entry].Name;
        return Name ? Name->c_str() : nullptr;
    });
}

SOULS_C_API const char*
souls_param_enums_find_name(const SoulsParamEnums* enums, const char* enum_name, int64_t value) {
    return GuardValue<const char*>(nullptr, [&]() -> const char* {
        const PARAMTDF* E = Require(enums, "enums is NULL").Repository.Find(NameArg(enum_name));
        if (!E) return nullptr;
        for (const auto& Entry : E->Entries) {
            if (ToInt64(Entry.Val) == value) { return Entry.Name ? Entry.Name->c_str() : nullptr; }
        }
        return nullptr;
    });
}

SOULS_C_API SoulsStatus souls_param_enums_find_value(const SoulsParamEnums* enums,
                                                     const char* enum_name,
                                                     const char* entry_name,
                                                     int64_t* out_value) {
    return Guard([&] {
        if (!out_value) ThrowInvalid("out_value is NULL");
        const std::string Wanted = NameArg(entry_name);
        const PARAMTDF* E        = Require(enums, "enums is NULL").Repository.Find(NameArg(enum_name));
        if (E) {
            for (const auto& Entry : E->Entries) {
                if (Entry.Name && *Entry.Name == Wanted) {
                    *out_value = ToInt64(Entry.Val);
                    return;
                }
            }
        }
        throw NotFound("no such enum entry");
    });
}

SOULS_C_API size_t souls_param_enums_load_error_count(const SoulsParamEnums* enums) {
    return GuardValue<size_t>(0, [&] { return Require(enums, "enums is NULL").LoadErrors.size(); });
}

SOULS_C_API const char* souls_param_enums_load_error(const SoulsParamEnums* enums, size_t index) {
    return GuardValue<const char*>(nullptr, [&] {
        const auto& Errors = Require(enums, "enums is NULL").LoadErrors;
        CheckIndex(index, Errors.size(), "error index out of range");
        return Errors[index].c_str();
    });
}
#pragma endregion
