//
// Created by Jake Rieger on 10/9/2026.
//

#include "CApiCommon.hpp"

#include <algorithm>

using namespace Souls;
using namespace Souls::CApi;

static_assert(static_cast<int>(PARAMDEF::DefType::s8) == SOULS_FIELD_S8);
static_assert(static_cast<int>(PARAMDEF::DefType::b32) == SOULS_FIELD_B32);
static_assert(static_cast<int>(PARAMDEF::DefType::f64) == SOULS_FIELD_F64);
static_assert(static_cast<int>(PARAMDEF::DefType::dummy8) == SOULS_FIELD_DUMMY8);
static_assert(static_cast<int>(PARAMDEF::DefType::fixstrW) == SOULS_FIELD_FIXSTRW);

namespace {
    SoulsParam* Wrap(PARAM Value) {
        auto Result   = std::make_unique<SoulsParam>();
        Result->Value = std::move(Value);
        return Result.release();
    }

    PARAM::Row& RowAt(SoulsParam* Param, size_t Index) {
        auto& Rows = Require(Param, "param is NULL").Value.Rows;
        CheckIndex(Index, Rows.size(), "row index out of range");
        return Rows[Index];
    }

    const PARAM::Row& RowAt(const SoulsParam* Param, size_t Index) {
        const auto& Rows = Require(Param, "param is NULL").Value.Rows;
        CheckIndex(Index, Rows.size(), "row index out of range");
        return Rows[Index];
    }

    const char* FieldName(const char* Field) {
        if (!Field) ThrowInvalid("field name is NULL");
        return Field;
    }
}  // namespace

SOULS_C_API SoulsParam* souls_param_read_file(const char* path) {
    return GuardValue<SoulsParam*>(nullptr, [&] { return Wrap(PARAM::Read(PathFrom(path))); });
}

SOULS_C_API SoulsParam* souls_param_read_memory(const uint8_t* data, size_t size) {
    return GuardValue<SoulsParam*>(nullptr, [&] { return Wrap(PARAM::Read(Bytes(data, size))); });
}

SOULS_C_API void souls_param_free(SoulsParam* param) {
    delete param;
}

SOULS_C_API SoulsStatus souls_param_write_file(SoulsParam* param, const char* path) {
    return Guard([&] { Require(param, "param is NULL").Value.Write(PathFrom(path)); });
}

SOULS_C_API SoulsBuffer souls_param_write_memory(SoulsParam* param) {
    return GuardValue(SoulsBuffer{nullptr, 0}, [&] { return MakeBuffer(Require(param, "param is NULL").Value.Write()); });
}

SOULS_C_API const char* souls_param_type(const SoulsParam* param) {
    return GuardValue<const char*>(nullptr, [&] { return Require(param, "param is NULL").Value.ParamType.c_str(); });
}

SOULS_C_API size_t souls_param_row_count(const SoulsParam* param) {
    return GuardValue<size_t>(0, [&] { return Require(param, "param is NULL").Value.Rows.size(); });
}

SOULS_C_API int32_t souls_param_row_id(const SoulsParam* param, size_t index) {
    return GuardValue<int32_t>(-1, [&] { return RowAt(param, index).ID; });
}

SOULS_C_API const char* souls_param_row_name(const SoulsParam* param, size_t index) {
    return GuardValue<const char*>(nullptr, [&]() -> const char* {
        const PARAM::Row& Row = RowAt(param, index);
        return Row.Name ? Row.Name->c_str() : nullptr;
    });
}

SOULS_C_API SoulsStatus souls_param_set_row_name(SoulsParam* param, size_t index, const char* name_or_null) {
    return Guard([&] {
        PARAM::Row& Row = RowAt(param, index);
        if (name_or_null) Row.Name = std::string(name_or_null);
        else Row.Name.reset();
    });
}

SOULS_C_API SoulsStatus souls_param_set_row_id(SoulsParam* param, size_t index, int32_t id) {
    return Guard([&] { RowAt(param, index).ID = id; });
}

SOULS_C_API const uint8_t* souls_param_row_bytes(const SoulsParam* param, size_t index, size_t* out_size) {
    return GuardValue<const uint8_t*>(nullptr, [&] {
        const PARAM::Row& Row = RowAt(param, index);
        if (out_size) *out_size = Row.Bytes.size();
        return Row.Bytes.data();
    });
}

SOULS_C_API int64_t souls_param_find_row(const SoulsParam* param, int32_t id) {
    return GuardValue<int64_t>(-1, [&]() -> int64_t {
        const auto& Rows = Require(param, "param is NULL").Value.Rows;
        for (size_t I = 0; I < Rows.size(); ++I) {
            if (Rows[I].ID == id) return static_cast<int64_t>(I);
        }
        return -1;
    });
}

SOULS_C_API SoulsStatus souls_param_remove_row(SoulsParam* param, size_t index) {
    return Guard([&] {
        auto& Rows = Require(param, "param is NULL").Value.Rows;
        CheckIndex(index, Rows.size(), "row index out of range");
        Rows.erase(Rows.begin() + static_cast<std::ptrdiff_t>(index));
    });
}

#pragma region Paramdefs
SOULS_C_API SoulsParamDefs* souls_paramdefs_new(void) {
    return GuardValue<SoulsParamDefs*>(nullptr, [&] { return new SoulsParamDefs(); });
}

SOULS_C_API void souls_paramdefs_free(SoulsParamDefs* defs) {
    delete defs;
}

SOULS_C_API SoulsStatus souls_paramdefs_add_folder(SoulsParamDefs* defs, const char* folder, int recursive) {
    return Guard([&] {
        SoulsParamDefs& D = Require(defs, "defs is NULL");
        D.LoadErrors.clear();
        const auto Result = D.Repository.AddFolder(PathFrom(folder), recursive != 0);
        for (const auto& Error : Result.Errors) {
            D.LoadErrors.push_back(Error.Path.string() + ": " + Error.Message);
        }
    });
}

SOULS_C_API size_t souls_paramdefs_count(const SoulsParamDefs* defs) {
    return GuardValue<size_t>(0, [&] { return Require(defs, "defs is NULL").Repository.Size(); });
}

SOULS_C_API size_t souls_paramdefs_load_error_count(const SoulsParamDefs* defs) {
    return GuardValue<size_t>(0, [&] { return Require(defs, "defs is NULL").LoadErrors.size(); });
}

SOULS_C_API const char* souls_paramdefs_load_error(const SoulsParamDefs* defs, size_t index) {
    return GuardValue<const char*>(nullptr, [&] {
        const auto& Errors = Require(defs, "defs is NULL").LoadErrors;
        CheckIndex(index, Errors.size(), "error index out of range");
        return Errors[index].c_str();
    });
}
#pragma endregion

#pragma region Layouts
SOULS_C_API SoulsParamLayout* souls_param_layout_create(const SoulsParam* param, const SoulsParamDefs* defs) {
    return GuardValue<SoulsParamLayout*>(nullptr, [&]() -> SoulsParamLayout* {
        const PARAM& P        = Require(param, "param is NULL").Value;
        const auto& Repo      = Require(defs, "defs is NULL").Repository;
        const PARAMDEF* Found = Repo.Find(P);
        std::optional<ParamLayout> Layout;
        if (Found) Layout = ParamLayout::TryCreate(P, *Found);
        if (!Layout) {
            SetError(SOULS_ERR_NOT_FOUND, "No paramdef matches param type \"" + P.ParamType + "\" with this data version and row size.");
            return nullptr;
        }
        return new SoulsParamLayout(std::move(*Layout));
    });
}

SOULS_C_API void souls_param_layout_free(SoulsParamLayout* layout) {
    delete layout;
}

SOULS_C_API size_t souls_param_layout_row_size(const SoulsParamLayout* layout) {
    return GuardValue<size_t>(0, [&] { return Require(layout, "layout is NULL").Value.RowSize(); });
}

SOULS_C_API size_t souls_param_layout_field_count(const SoulsParamLayout* layout) {
    return GuardValue<size_t>(0, [&] { return Require(layout, "layout is NULL").Value.FieldCount(); });
}

SOULS_C_API const char* souls_param_layout_field_name(const SoulsParamLayout* layout, size_t field) {
    return GuardValue<const char*>(nullptr, [&] {
        const ParamLayout& L = Require(layout, "layout is NULL").Value;
        CheckIndex(field, L.FieldCount(), "field index out of range");
        return L.InternalName(field).c_str();
    });
}

SOULS_C_API SoulsFieldType souls_param_layout_field_type(const SoulsParamLayout* layout, size_t field) {
    return GuardValue(SOULS_FIELD_S8, [&] {
        const ParamLayout& L = Require(layout, "layout is NULL").Value;
        CheckIndex(field, L.FieldCount(), "field index out of range");
        return static_cast<SoulsFieldType>(static_cast<int>(L.Type(field)));
    });
}

SOULS_C_API int64_t souls_param_layout_find_field(const SoulsParamLayout* layout, const char* name) {
    return GuardValue<int64_t>(-1, [&]() -> int64_t {
        const auto Index = Require(layout, "layout is NULL").Value.IndexOf(FieldName(name));
        return Index ? static_cast<int64_t>(*Index) : -1;
    });
}

SOULS_C_API SoulsStatus souls_param_get_number(const SoulsParamLayout* layout, const SoulsParam* param, size_t row,
                                               const char* field, double* out_value) {
    return Guard([&] {
        if (!out_value) ThrowInvalid("out_value is NULL");
        *out_value = Require(layout, "layout is NULL").Value.GetNumber(RowAt(param, row), FieldName(field));
    });
}

SOULS_C_API SoulsStatus souls_param_set_number(const SoulsParamLayout* layout, SoulsParam* param, size_t row,
                                               const char* field, double value) {
    return Guard([&] { Require(layout, "layout is NULL").Value.SetNumber(RowAt(param, row), FieldName(field), value); });
}

SOULS_C_API const char* souls_param_get_string(const SoulsParamLayout* layout, const SoulsParam* param, size_t row,
                                               const char* field) {
    return GuardValue<const char*>(nullptr, [&] {
        SoulsParamLayout& L = const_cast<SoulsParamLayout&>(Require(layout, "layout is NULL"));
        L.Scratch           = L.Value.Get<std::string>(RowAt(param, row), FieldName(field));
        return L.Scratch.c_str();
    });
}

SOULS_C_API SoulsStatus souls_param_set_string(const SoulsParamLayout* layout, SoulsParam* param, size_t row,
                                               const char* field, const char* text) {
    return Guard([&] {
        if (!text) ThrowInvalid("text is NULL");
        Require(layout, "layout is NULL").Value.Set(RowAt(param, row), FieldName(field), CellValue(std::string(text)));
    });
}

SOULS_C_API SoulsStatus souls_param_get_bytes(const SoulsParamLayout* layout, const SoulsParam* param, size_t row,
                                              const char* field, uint8_t* out, size_t capacity, size_t* out_size) {
    return Guard([&] {
        const CellValue Value = Require(layout, "layout is NULL").Value.Get(RowAt(param, row), FieldName(field));
        std::vector<uint8_t> Data;
        if (const auto* Array = std::get_if<std::vector<uint8_t>>(&Value)) {
            Data = *Array;
        } else if (const auto* Single = std::get_if<uint8_t>(&Value)) {
            Data = {*Single};  // a bitfield's storage byte
        } else {
            ThrowInvalid("the field is not a dummy8 field");
        }
        if (out_size) *out_size = Data.size();
        if (out) std::memcpy(out, Data.data(), (std::min)(capacity, Data.size()));
    });
}

SOULS_C_API SoulsStatus souls_param_set_bytes(const SoulsParamLayout* layout, SoulsParam* param, size_t row,
                                              const char* field, const uint8_t* data, size_t size) {
    return Guard([&] {
        const auto Span = Bytes(data, size);
        Require(layout, "layout is NULL")
            .Value.Set(RowAt(param, row), FieldName(field), CellValue(std::vector<uint8_t>(Span.begin(), Span.end())));
    });
}

SOULS_C_API int64_t souls_param_add_row(const SoulsParamLayout* layout, SoulsParam* param, int32_t id, const char* name_or_null) {
    return GuardValue<int64_t>(-1, [&]() -> int64_t {
        auto& Rows = Require(param, "param is NULL").Value.Rows;
        Rows.push_back(Require(layout, "layout is NULL")
                           .Value.MakeRow(id, name_or_null ? std::optional<std::string>(name_or_null) : std::nullopt));
        return static_cast<int64_t>(Rows.size() - 1);
    });
}
#pragma endregion
