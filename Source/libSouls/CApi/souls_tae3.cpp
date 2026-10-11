// Copyright (C) 2026 Jake Rieger
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, version 3. This program is distributed in the hope that it will
// be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU General
// Public License along with this program. If not, see https://www.gnu.org/licenses/.

#include "CApiCommon.hpp"

#include <libSouls/Formats/TAE3.hpp>

#include <concepts>
#include <string_view>

using namespace Souls;
using namespace Souls::CApi;

struct SoulsTae3 {
    TAE3 Value;
};

namespace {
    SoulsTae3* Wrap(TAE3 Value) {
        auto Result   = std::make_unique<SoulsTae3>();
        Result->Value = std::move(Value);
        return Result.release();
    }

    TAE3& File(const SoulsTae3* Handle) {
        return const_cast<SoulsTae3&>(Require(Handle, "tae is NULL")).Value;
    }

    TAE3::Animation& AnimationAt(const SoulsTae3* Handle, size_t Index) {
        auto& Animations = File(Handle).Animations;
        CheckIndex(Index, Animations.size(), "animation index out of range");
        return Animations[Index];
    }

    TAE3::Event& EventAt(const SoulsTae3* Handle, size_t Animation, size_t Index) {
        auto& Events = AnimationAt(Handle, Animation).Events;
        CheckIndex(Index, Events.size(), "event index out of range");
        return Events[Index];
    }

    TAE3EventType TypeFrom(int64_t Number) {
        const auto Type = static_cast<TAE3EventType>(Number);
        if (!TAE3EventTypeName(Type)) ThrowInvalid("unsupported event type number");
        return Type;
    }

    // Lists the fields of an event's data.
    struct Lister {
        std::vector<std::pair<const char*, SoulsPropertyKind>> Found;
        void operator()(const char* Name, bool&) { Found.emplace_back(Name, SOULS_PROPERTY_INT); }
        template<std::integral T>
        void operator()(const char* Name, T&) {
            Found.emplace_back(Name, SOULS_PROPERTY_INT);
        }
        void operator()(const char* Name, float&) { Found.emplace_back(Name, SOULS_PROPERTY_FLOAT); }
        void operator()(const char* Name, std::vector<uint8_t>&) { Found.emplace_back(Name, SOULS_PROPERTY_BYTES); }
        void operator()(const char* Name, std::vector<int32_t>&) { Found.emplace_back(Name, SOULS_PROPERTY_INT_ARRAY); }
    };

    enum class Op { GetNumber, SetNumber, GetBytes, SetBytes, GetInts, SetInts };

    // Reads or writes one field by name.
    struct Access {
        std::string_view Name;
        Op Mode;
        bool Found             = false;
        bool Mismatch          = false;
        double Number          = 0;
        uint8_t* BytesOut      = nullptr;
        const uint8_t* BytesIn = nullptr;
        int64_t* IntsOut       = nullptr;
        const int64_t* IntsIn  = nullptr;
        size_t Capacity        = 0;
        size_t* Count          = nullptr;
        size_t CountIn         = 0;

        bool Hit(const char* Candidate) {
            if (Name != Candidate) return false;
            Found = true;
            return true;
        }

        void operator()(const char* N, bool& Value) {
            if (!Hit(N)) return;
            if (Mode == Op::GetNumber) Number = Value ? 1 : 0;
            else if (Mode == Op::SetNumber) Value = Number != 0;
            else Mismatch = true;
        }

        template<std::integral T>
        void operator()(const char* N, T& Value) {
            if (!Hit(N)) return;
            if (Mode == Op::GetNumber) Number = static_cast<double>(Value);
            else if (Mode == Op::SetNumber) Value = static_cast<T>(Number);
            else Mismatch = true;
        }

        void operator()(const char* N, float& Value) {
            if (!Hit(N)) return;
            if (Mode == Op::GetNumber) Number = static_cast<double>(Value);
            else if (Mode == Op::SetNumber) Value = static_cast<float>(Number);
            else Mismatch = true;
        }

        void operator()(const char* N, std::vector<uint8_t>& Value) {
            if (!Hit(N)) return;
            if (Mode == Op::GetBytes) {
                if (Count) *Count = Value.size();
                for (size_t I = 0; BytesOut && I < Value.size() && I < Capacity; ++I)
                    BytesOut[I] = Value[I];
            } else if (Mode == Op::SetBytes) {
                if (CountIn != Value.size()) ThrowInvalid("the array length doesn't match the field");
                for (size_t I = 0; I < CountIn; ++I)
                    Value[I] = BytesIn[I];
            } else Mismatch = true;
        }

        void operator()(const char* N, std::vector<int32_t>& Value) {
            if (!Hit(N)) return;
            if (Mode == Op::GetInts) {
                if (Count) *Count = Value.size();
                for (size_t I = 0; IntsOut && I < Value.size() && I < Capacity; ++I)
                    IntsOut[I] = Value[I];
            } else if (Mode == Op::SetInts) {
                if (CountIn != Value.size()) ThrowInvalid("the array length doesn't match the field");
                for (size_t I = 0; I < CountIn; ++I)
                    Value[I] = static_cast<int32_t>(IntsIn[I]);
            } else Mismatch = true;
        }
    };

    void Apply(const SoulsTae3* Handle, size_t Animation, size_t Event, Access& A) {
        TAE3::Event& E = EventAt(Handle, Animation, Event);
        if (E.IsRaw) throw Unsupported("The event is raw; its fields are not known.");
        std::visit([&](auto& Data) { Data.Fields(A); }, E.Data);
        if (A.Mismatch) ThrowInvalid("the field has a different kind");
        if (!A.Found) throw Unsupported("The event has no field \"" + std::string(A.Name) + "\".");
    }

    Lister ListFields(const SoulsTae3* Handle, size_t Animation, size_t Event) {
        Lister L;
        TAE3::Event& E = EventAt(Handle, Animation, Event);
        if (E.IsRaw) return L;
        std::visit([&](auto& Data) { Data.Fields(L); }, E.Data);
        return L;
    }

    const char* NameArg(const char* Name) {
        if (!Name) ThrowInvalid("name is NULL");
        return Name;
    }
}  // namespace

SOULS_C_API SoulsTae3* souls_tae3_new(void) {
    return GuardValue<SoulsTae3*>(nullptr, [&] { return Wrap(TAE3()); });
}

SOULS_C_API SoulsTae3* souls_tae3_read_file(const char* path) {
    return GuardValue<SoulsTae3*>(nullptr, [&] { return Wrap(TAE3::Read(PathFrom(path))); });
}

SOULS_C_API SoulsTae3* souls_tae3_read_memory(const uint8_t* data, size_t size) {
    return GuardValue<SoulsTae3*>(nullptr, [&] { return Wrap(TAE3::Read(Bytes(data, size))); });
}

SOULS_C_API void souls_tae3_free(SoulsTae3* tae) {
    delete tae;
}

SOULS_C_API SoulsStatus souls_tae3_write_file(SoulsTae3* tae, const char* path) {
    return Guard([&] { File(tae).Write(PathFrom(path)); });
}

SOULS_C_API SoulsBuffer souls_tae3_write_memory(SoulsTae3* tae) {
    return GuardValue(SoulsBuffer {nullptr, 0}, [&] { return MakeBuffer(File(tae).Write()); });
}

SOULS_C_API int32_t souls_tae3_version(const SoulsTae3* tae) {
    return GuardValue<int32_t>(0, [&] { return File(tae).Version; });
}

SOULS_C_API SoulsStatus souls_tae3_set_version(SoulsTae3* tae, int32_t version) {
    return Guard([&] {
        if (version != 0x1000C && version != 0x1000D) ThrowInvalid("version must be 0x1000C or 0x1000D");
        File(tae).Version = version;
    });
}

SOULS_C_API int32_t souls_tae3_id(const SoulsTae3* tae) {
    return GuardValue<int32_t>(-1, [&] { return File(tae).ID; });
}

SOULS_C_API SoulsStatus souls_tae3_set_id(SoulsTae3* tae, int32_t id) {
    return Guard([&] { File(tae).ID = id; });
}

SOULS_C_API const char* souls_tae3_skeleton_name(const SoulsTae3* tae) {
    return GuardValue<const char*>(nullptr, [&] { return File(tae).SkeletonName.c_str(); });
}

SOULS_C_API const char* souls_tae3_sib_name(const SoulsTae3* tae) {
    return GuardValue<const char*>(nullptr, [&] { return File(tae).SibName.c_str(); });
}

SOULS_C_API SoulsStatus souls_tae3_set_names(SoulsTae3* tae, const char* skeleton_name, const char* sib_name) {
    return Guard([&] {
        TAE3& T        = File(tae);
        T.SkeletonName = NameArg(skeleton_name);
        T.SibName      = NameArg(sib_name);
        T.HasNames     = true;
    });
}

SOULS_C_API size_t souls_tae3_type_count(void) {
    return GuardValue<size_t>(0, [&] { return AllTAE3EventTypes().size(); });
}

SOULS_C_API int64_t souls_tae3_type_number(size_t type_index) {
    return GuardValue<int64_t>(-1, [&] {
        CheckIndex(type_index, AllTAE3EventTypes().size(), "type index out of range");
        return static_cast<int64_t>(AllTAE3EventTypes()[type_index]);
    });
}

SOULS_C_API const char* souls_tae3_type_name(size_t type_index) {
    return GuardValue<const char*>(nullptr, [&] {
        CheckIndex(type_index, AllTAE3EventTypes().size(), "type index out of range");
        return TAE3EventTypeName(AllTAE3EventTypes()[type_index]);
    });
}

#pragma region Animations
SOULS_C_API size_t souls_tae3_animation_count(const SoulsTae3* tae) {
    return GuardValue<size_t>(0, [&] { return File(tae).Animations.size(); });
}

SOULS_C_API int64_t souls_tae3_animation_id(const SoulsTae3* tae, size_t animation) {
    return GuardValue<int64_t>(-1, [&] { return AnimationAt(tae, animation).ID; });
}

SOULS_C_API SoulsStatus souls_tae3_set_animation_id(SoulsTae3* tae, size_t animation, int64_t id) {
    return Guard([&] { AnimationAt(tae, animation).ID = id; });
}

SOULS_C_API int64_t souls_tae3_find_animation(const SoulsTae3* tae, int64_t id) {
    return GuardValue<int64_t>(-1, [&]() -> int64_t {
        const auto& Animations = File(tae).Animations;
        for (size_t I = 0; I < Animations.size(); ++I) {
            if (Animations[I].ID == id) return static_cast<int64_t>(I);
        }
        return -1;
    });
}

SOULS_C_API int64_t souls_tae3_add_animation(SoulsTae3* tae, int64_t id) {
    return GuardValue<int64_t>(-1, [&]() -> int64_t {
        auto& Animations = File(tae).Animations;
        TAE3::Animation A;
        A.ID = id;
        Animations.push_back(std::move(A));
        return static_cast<int64_t>(Animations.size() - 1);
    });
}

SOULS_C_API SoulsStatus souls_tae3_remove_animation(SoulsTae3* tae, size_t animation) {
    return Guard([&] {
        auto& Animations = File(tae).Animations;
        CheckIndex(animation, Animations.size(), "animation index out of range");
        Animations.erase(Animations.begin() + static_cast<std::ptrdiff_t>(animation));
    });
}

SOULS_C_API int souls_tae3_animation_is_reference(const SoulsTae3* tae, size_t animation) {
    return GuardValue(0, [&] { return AnimationAt(tae, animation).AnimFileReference ? 1 : 0; });
}

SOULS_C_API const char* souls_tae3_animation_file_name(const SoulsTae3* tae, size_t animation) {
    return GuardValue<const char*>(nullptr, [&] { return AnimationAt(tae, animation).AnimFileName.c_str(); });
}

SOULS_C_API SoulsStatus souls_tae3_set_animation_reference(SoulsTae3* tae,
                                                           size_t animation,
                                                           int is_reference,
                                                           const char* file_name) {
    return Guard([&] {
        TAE3::Animation& A  = AnimationAt(tae, animation);
        A.AnimFileReference = is_reference != 0;
        A.AnimFileName      = file_name ? file_name : "";
    });
}
#pragma endregion

#pragma region Events
SOULS_C_API size_t souls_tae3_event_count(const SoulsTae3* tae, size_t animation) {
    return GuardValue<size_t>(0, [&] { return AnimationAt(tae, animation).Events.size(); });
}

SOULS_C_API double souls_tae3_event_start(const SoulsTae3* tae, size_t animation, size_t event) {
    return GuardValue<double>(0, [&] { return static_cast<double>(EventAt(tae, animation, event).StartTime); });
}

SOULS_C_API double souls_tae3_event_end(const SoulsTae3* tae, size_t animation, size_t event) {
    return GuardValue<double>(0, [&] { return static_cast<double>(EventAt(tae, animation, event).EndTime); });
}

SOULS_C_API SoulsStatus
souls_tae3_set_event_times(SoulsTae3* tae, size_t animation, size_t event, double start, double end) {
    return Guard([&] {
        TAE3::Event& E = EventAt(tae, animation, event);
        E.StartTime    = static_cast<float>(start);
        E.EndTime      = static_cast<float>(end);
    });
}

SOULS_C_API int64_t souls_tae3_event_type(const SoulsTae3* tae, size_t animation, size_t event) {
    return GuardValue<int64_t>(-1, [&] { return static_cast<int64_t>(EventAt(tae, animation, event).TypeNumber()); });
}

SOULS_C_API const char* souls_tae3_event_type_name(const SoulsTae3* tae, size_t animation, size_t event) {
    return GuardValue<const char*>(nullptr, [&]() -> const char* {
        const TAE3::Event& E = EventAt(tae, animation, event);
        return E.IsRaw ? nullptr : TAE3EventTypeName(E.Type());
    });
}

SOULS_C_API int64_t
souls_tae3_add_event(SoulsTae3* tae, size_t animation, int64_t type_number, double start, double end) {
    return GuardValue<int64_t>(-1, [&]() -> int64_t {
        auto& Events = AnimationAt(tae, animation).Events;
        TAE3::Event E;
        E.StartTime = static_cast<float>(start);
        E.EndTime   = static_cast<float>(end);
        E.Data      = CreateTAE3EventData(TypeFrom(type_number));
        Events.push_back(std::move(E));
        return static_cast<int64_t>(Events.size() - 1);
    });
}

SOULS_C_API int souls_tae3_event_is_raw(const SoulsTae3* tae, size_t animation, size_t event) {
    return GuardValue(0, [&] { return EventAt(tae, animation, event).IsRaw ? 1 : 0; });
}

SOULS_C_API const uint8_t*
souls_tae3_event_raw_data(const SoulsTae3* tae, size_t animation, size_t event, size_t* out_size) {
    return GuardValue<const uint8_t*>(nullptr, [&]() -> const uint8_t* {
        const TAE3::Event& E = EventAt(tae, animation, event);
        if (!E.IsRaw) throw Unsupported("The event is not raw.");
        if (out_size) *out_size = E.RawData.size();
        return E.RawData.data();
    });
}

SOULS_C_API SoulsStatus
souls_tae3_set_event_raw_data(SoulsTae3* tae, size_t animation, size_t event, const uint8_t* data, size_t size) {
    return Guard([&] {
        TAE3::Event& E = EventAt(tae, animation, event);
        if (!E.IsRaw) throw Unsupported("The event is not raw.");
        const auto Span = Bytes(data, size);
        E.RawData.assign(Span.begin(), Span.end());
    });
}

SOULS_C_API int64_t souls_tae3_add_raw_event(
  SoulsTae3* tae, size_t animation, int64_t type_number, double start, double end, const uint8_t* data, size_t size) {
    return GuardValue<int64_t>(-1, [&]() -> int64_t {
        const auto Span = Bytes(data, size);
        auto& Events    = AnimationAt(tae, animation).Events;
        TAE3::Event E;
        E.StartTime = static_cast<float>(start);
        E.EndTime   = static_cast<float>(end);
        E.IsRaw     = true;
        E.RawType   = static_cast<uint64_t>(type_number);
        E.RawData.assign(Span.begin(), Span.end());
        Events.push_back(std::move(E));
        return static_cast<int64_t>(Events.size() - 1);
    });
}

SOULS_C_API SoulsStatus souls_tae3_remove_event(SoulsTae3* tae, size_t animation, size_t event) {
    return Guard([&] {
        auto& Events = AnimationAt(tae, animation).Events;
        CheckIndex(event, Events.size(), "event index out of range");
        Events.erase(Events.begin() + static_cast<std::ptrdiff_t>(event));
    });
}
#pragma endregion

#pragma region Fields
SOULS_C_API size_t souls_tae3_field_count(const SoulsTae3* tae, size_t animation, size_t event) {
    return GuardValue<size_t>(0, [&] { return ListFields(tae, animation, event).Found.size(); });
}

SOULS_C_API const char* souls_tae3_field_name(const SoulsTae3* tae, size_t animation, size_t event, size_t field) {
    return GuardValue<const char*>(nullptr, [&] {
        const Lister L = ListFields(tae, animation, event);
        CheckIndex(field, L.Found.size(), "field index out of range");
        return L.Found[field].first;
    });
}

SOULS_C_API SoulsPropertyKind souls_tae3_field_kind(const SoulsTae3* tae,
                                                    size_t animation,
                                                    size_t event,
                                                    size_t field) {
    return GuardValue(SOULS_PROPERTY_INT, [&] {
        const Lister L = ListFields(tae, animation, event);
        CheckIndex(field, L.Found.size(), "field index out of range");
        return L.Found[field].second;
    });
}

namespace {
    // Missing fields are "unsupported", not parse errors.
    template<typename F>
    SoulsStatus GuardField(F&& Fn) noexcept {
        return Guard(std::forward<F>(Fn));
    }
}  // namespace

SOULS_C_API SoulsStatus
souls_tae3_get_number(const SoulsTae3* tae, size_t animation, size_t event, const char* field, double* out_value) {
    return GuardField([&] {
        if (!out_value) ThrowInvalid("out_value is NULL");
        Access A {NameArg(field), Op::GetNumber};
        Apply(tae, animation, event, A);
        *out_value = A.Number;
    });
}

SOULS_C_API SoulsStatus
souls_tae3_set_number(SoulsTae3* tae, size_t animation, size_t event, const char* field, double value) {
    return GuardField([&] {
        Access A {NameArg(field), Op::SetNumber};
        A.Number = value;
        Apply(tae, animation, event, A);
    });
}

SOULS_C_API SoulsStatus souls_tae3_get_bytes(const SoulsTae3* tae,
                                             size_t animation,
                                             size_t event,
                                             const char* field,
                                             uint8_t* out,
                                             size_t capacity,
                                             size_t* out_count) {
    return GuardField([&] {
        Access A {NameArg(field), Op::GetBytes};
        A.BytesOut = out;
        A.Capacity = capacity;
        A.Count    = out_count;
        Apply(tae, animation, event, A);
    });
}

SOULS_C_API SoulsStatus souls_tae3_set_bytes(
  SoulsTae3* tae, size_t animation, size_t event, const char* field, const uint8_t* data, size_t count) {
    return GuardField([&] {
        if (!data && count) ThrowInvalid("data is NULL");
        Access A {NameArg(field), Op::SetBytes};
        A.BytesIn = data;
        A.CountIn = count;
        Apply(tae, animation, event, A);
    });
}

SOULS_C_API SoulsStatus souls_tae3_get_int_array(const SoulsTae3* tae,
                                                 size_t animation,
                                                 size_t event,
                                                 const char* field,
                                                 int64_t* out,
                                                 size_t capacity,
                                                 size_t* out_count) {
    return GuardField([&] {
        Access A {NameArg(field), Op::GetInts};
        A.IntsOut  = out;
        A.Capacity = capacity;
        A.Count    = out_count;
        Apply(tae, animation, event, A);
    });
}

SOULS_C_API SoulsStatus souls_tae3_set_int_array(
  SoulsTae3* tae, size_t animation, size_t event, const char* field, const int64_t* values, size_t count) {
    return GuardField([&] {
        if (!values && count) ThrowInvalid("values is NULL");
        Access A {NameArg(field), Op::SetInts};
        A.IntsIn  = values;
        A.CountIn = count;
        Apply(tae, animation, event, A);
    });
}
#pragma endregion

#pragma region Groups
SOULS_C_API size_t souls_tae3_group_count(const SoulsTae3* tae, size_t animation) {
    return GuardValue<size_t>(0, [&] { return AnimationAt(tae, animation).EventGroups.size(); });
}

SOULS_C_API int64_t souls_tae3_group_type(const SoulsTae3* tae, size_t animation, size_t group) {
    return GuardValue<int64_t>(-1, [&] {
        const auto& Groups = AnimationAt(tae, animation).EventGroups;
        CheckIndex(group, Groups.size(), "group index out of range");
        return static_cast<int64_t>(Groups[group].Type);
    });
}

SOULS_C_API SoulsStatus souls_tae3_group_indices(
  const SoulsTae3* tae, size_t animation, size_t group, int32_t* out, size_t capacity, size_t* out_count) {
    return Guard([&] {
        const auto& Groups = AnimationAt(tae, animation).EventGroups;
        CheckIndex(group, Groups.size(), "group index out of range");
        const auto& Indices = Groups[group].Indices;
        if (out_count) *out_count = Indices.size();
        for (size_t I = 0; out && I < Indices.size() && I < capacity; ++I)
            out[I] = Indices[I];
    });
}

SOULS_C_API int64_t
souls_tae3_add_group(SoulsTae3* tae, size_t animation, int64_t type_number, const int32_t* indices, size_t count) {
    return GuardValue<int64_t>(-1, [&]() -> int64_t {
        if (!indices && count) ThrowInvalid("indices is NULL");
        auto& A = AnimationAt(tae, animation);
        for (size_t I = 0; I < count; ++I) {
            if (indices[I] < 0 || static_cast<size_t>(indices[I]) >= A.Events.size())
                ThrowInvalid("an index is not an event of the animation");
        }
        TAE3::EventGroup G;
        G.Type = TypeFrom(type_number);
        G.Indices.assign(indices, indices + count);
        A.EventGroups.push_back(std::move(G));
        return static_cast<int64_t>(A.EventGroups.size() - 1);
    });
}

SOULS_C_API SoulsStatus souls_tae3_remove_group(SoulsTae3* tae, size_t animation, size_t group) {
    return Guard([&] {
        auto& Groups = AnimationAt(tae, animation).EventGroups;
        CheckIndex(group, Groups.size(), "group index out of range");
        Groups.erase(Groups.begin() + static_cast<std::ptrdiff_t>(group));
    });
}
#pragma endregion
