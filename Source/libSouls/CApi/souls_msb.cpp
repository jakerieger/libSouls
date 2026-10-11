//
// Created by Jake Rieger on 10/11/2026.
//

#include "CApiCommon.hpp"

#include <libSouls/Formats/MSB/MSB1.hpp>
#include <libSouls/Formats/MSB/MSB2.hpp>
#include <libSouls/Formats/MSB/MSB3.hpp>
#include <libSouls/Formats/MSB/MSBE.hpp>
#include <libSouls/Formats/MSB/MSBS.hpp>

#include <concepts>
#include <mutex>
#include <set>
#include <string_view>
#include <typeinfo>
#include <variant>

using namespace Souls;
using namespace Souls::CApi;

struct SoulsMsb {
    SoulsMsbKind Kind = SOULS_MSB_DARK_SOULS_1;
    std::variant<MSB1, MSB2, MSB3, MSBS, MSBE> Value;
};

namespace {
    enum class PK { Int, String, Vec3, IntArray };

    // Strings handed out as `const char*` that must outlive any object (type and property names).
    const char* Intern(const std::string& Text) {
        static std::mutex Lock;
        static std::set<std::string> Pool;
        std::lock_guard<std::mutex> Guard(Lock);
        return Pool.insert(Text).first->c_str();
    }

    // "struct Souls::MSB1::PartsParam::Enemy" -> "Enemy"
    std::string ShortTypeName(const std::type_info& Info) {
        std::string Name = Info.name();
        if (const auto Colons = Name.rfind("::"); Colons != std::string::npos) {
            return Name.substr(Colons + 2);
        }
        for (const char* Prefix : {"struct ", "class "}) {
            if (Name.rfind(Prefix, 0) == 0) return Name.substr(std::strlen(Prefix));
        }
        return Name;
    }

    template<typename M, typename F>
    decltype(auto) WithMsb(const SoulsMsb* Handle, F&& Fn) {
        SoulsMsb& H = const_cast<SoulsMsb&>(Require(Handle, "msb is NULL"));
        return std::visit([&](auto& Msb) -> decltype(auto) { return Fn(Msb); }, H.Value);
    }

    template<typename Msb, typename F>
    void OnCategory(Msb& M, SoulsMsbCategory Category, F&& Fn) {
        switch (Category) {
            case SOULS_MSB_MODELS: Fn(M.Models); return;
            case SOULS_MSB_PARTS: Fn(M.Parts); return;
            case SOULS_MSB_EVENTS: Fn(M.Events); return;
            case SOULS_MSB_REGIONS: Fn(M.Regions); return;
        }
        ThrowInvalid("invalid category");
    }

    // Runs Fn on the entry at the index.
    template<typename Msb, typename F>
    void OnEntry(Msb& M, SoulsMsbCategory Category, size_t Index, F&& Fn) {
        OnCategory(M, Category, [&](auto& Param) {
            auto Entries = Param.GetEntries();
            CheckIndex(Index, Entries.size(), "entry index out of range");
            Fn(*Entries[Index]);
        });
    }

    // The properties an entry can have: one call per property, with its name and a reference to the member. A name
    // is offered only if this game's entry type has the member.
    template<typename E, typename V>
    void ForProperties(E& e, V&& v) {
        if constexpr (requires { e.SibPath; }) v("SibPath", e.SibPath);
        if constexpr (requires { e.InstanceCount; }) v("InstanceCount", e.InstanceCount);
        if constexpr (requires { e.ModelName; }) v("ModelName", e.ModelName);
        if constexpr (requires { e.Position; }) v("Position", e.Position);
        if constexpr (requires { e.Rotation; }) v("Rotation", e.Rotation);
        if constexpr (requires { e.Scale; }) v("Scale", e.Scale);
        if constexpr (requires { e.EventID; }) v("EventID", e.EventID);
        if constexpr (requires { e.EntityID; }) v("EntityID", e.EntityID);
        if constexpr (requires { e.MapStudioLayer; }) v("MapStudioLayer", e.MapStudioLayer);
        if constexpr (requires { e.PartName; }) v("PartName", e.PartName);
        if constexpr (requires { e.RegionName; }) {
            v("RegionName", e.RegionName);
        } else if constexpr (requires { e.PointName; }) {
            v("RegionName", e.PointName);
        }
        if constexpr (requires { e.ActivationPartName; }) v("ActivationPartName", e.ActivationPartName);
        if constexpr (requires { e.DrawGroups; }) {
            v("DrawGroups", e.DrawGroups);
        } else if constexpr (requires { e.Unk1.DrawGroups; }) {  // Elden Ring keeps them in a block of their own
            v("DrawGroups", e.Unk1.DrawGroups);
        }
        if constexpr (requires { e.DispGroups; }) {
            v("DispGroups", e.DispGroups);
        } else if constexpr (requires { e.Unk1.DisplayGroups; }) {
            v("DispGroups", e.Unk1.DisplayGroups);
        }
        if constexpr (requires { e.BackreadGroups; }) v("BackreadGroups", e.BackreadGroups);
        if constexpr (requires { e.EntityGroups; }) {
            v("EntityGroups", e.EntityGroups);
        } else if constexpr (requires { e.EntityGroupIDs; }) {
            v("EntityGroups", e.EntityGroupIDs);
        }
    }

    // Lists the properties.
    struct Lister {
        std::vector<std::pair<const char*, PK>> Found;
        template<std::integral T>
        void operator()(const char* Name, T&) { Found.emplace_back(Name, PK::Int); }
        void operator()(const char* Name, std::string&) { Found.emplace_back(Name, PK::String); }
        void operator()(const char* Name, std::optional<std::string>&) { Found.emplace_back(Name, PK::String); }
        void operator()(const char* Name, Vector3&) { Found.emplace_back(Name, PK::Vec3); }
        template<std::integral T, size_t N>
        void operator()(const char* Name, std::array<T, N>&) { Found.emplace_back(Name, PK::IntArray); }
    };

    enum class Op { GetInt, SetInt, GetString, SetString, GetVec3, SetVec3, GetArray, SetArray };

    // Reads or writes one property by name.
    struct Access {
        std::string_view Name;
        Op Mode;
        bool Found    = false;
        bool Mismatch = false;

        int64_t Int = 0;
        const char* TextIn   = nullptr;
        const char* TextOut  = nullptr;
        float* VecOut        = nullptr;
        const float* VecIn   = nullptr;
        int64_t* ArrayOut    = nullptr;
        const int64_t* ArrayIn = nullptr;
        size_t Capacity = 0;
        size_t* Count   = nullptr;
        size_t CountIn  = 0;

        bool Hit(const char* Candidate) {
            if (Name != Candidate) return false;
            Found = true;
            return true;
        }

        template<std::integral T>
        void operator()(const char* N, T& Value) {
            if (!Hit(N)) return;
            if (Mode == Op::GetInt) Int = static_cast<int64_t>(Value);
            else if (Mode == Op::SetInt) Value = static_cast<T>(Int);
            else Mismatch = true;
        }

        void operator()(const char* N, std::string& Value) {
            if (!Hit(N)) return;
            if (Mode == Op::GetString) TextOut = Value.c_str();
            else if (Mode == Op::SetString) Value = TextIn ? TextIn : "";
            else Mismatch = true;
        }

        void operator()(const char* N, std::optional<std::string>& Value) {
            if (!Hit(N)) return;
            if (Mode == Op::GetString) TextOut = Value ? Value->c_str() : nullptr;
            else if (Mode == Op::SetString) {
                if (TextIn) Value = std::string(TextIn);
                else Value.reset();
            } else Mismatch = true;
        }

        void operator()(const char* N, Vector3& Value) {
            if (!Hit(N)) return;
            if (Mode == Op::GetVec3) {
                VecOut[0] = Value.X;
                VecOut[1] = Value.Y;
                VecOut[2] = Value.Z;
            } else if (Mode == Op::SetVec3) {
                Value = Vector3(VecIn[0], VecIn[1], VecIn[2]);
            } else Mismatch = true;
        }

        template<std::integral T, size_t K>
        void operator()(const char* N, std::array<T, K>& Value) {
            if (!Hit(N)) return;
            if (Mode == Op::GetArray) {
                if (Count) *Count = K;
                for (size_t I = 0; I < K && I < Capacity; ++I) ArrayOut[I] = static_cast<int64_t>(Value[I]);
            } else if (Mode == Op::SetArray) {
                if (CountIn != K) ThrowInvalid("the array length doesn't match the property");
                for (size_t I = 0; I < K; ++I) Value[I] = static_cast<T>(ArrayIn[I]);
            } else Mismatch = true;
        }
    };

    void Apply(const SoulsMsb* Handle, SoulsMsbCategory Category, size_t Index, Access& A) {
        WithMsb<void>(Handle, [&](auto& Msb) {
            OnEntry(Msb, Category, Index, [&](auto& Entry) { ForProperties(Entry, A); });
        });
        if (A.Mismatch) ThrowInvalid("the property has a different kind");
        if (!A.Found) throw BinaryException("The entry has no property \"" + std::string(A.Name) + "\".");
    }

    // Property lookups that fail because the entry lacks the property are "unsupported", not parse errors.
    template<typename F>
    SoulsStatus GuardProperty(F&& Fn) noexcept {
        try {
            Fn();
            return SOULS_OK;
        } catch (const BinaryException& E) {
            SetError(SOULS_ERR_UNSUPPORTED, E.what());
            return SOULS_ERR_UNSUPPORTED;
        } catch (...) {
            return TranslateCurrentException();
        }
    }

    const char* NameArg(const char* Name) {
        if (!Name) ThrowInvalid("name is NULL");
        return Name;
    }

    template<typename T>
    SoulsMsb* Wrap(SoulsMsbKind Kind, T Value) {
        auto Result   = std::make_unique<SoulsMsb>();
        Result->Kind  = Kind;
        Result->Value = std::move(Value);
        return Result.release();
    }

    template<typename Callable>
    SoulsMsb* ByKind(SoulsMsbKind Kind, Callable&& Make) {
        switch (Kind) {
            case SOULS_MSB_DARK_SOULS_1: return Wrap(Kind, Make(MSB1{}));
            case SOULS_MSB_DARK_SOULS_2: return Wrap(Kind, Make(MSB2{}));
            case SOULS_MSB_DARK_SOULS_3: return Wrap(Kind, Make(MSB3{}));
            case SOULS_MSB_SEKIRO: return Wrap(Kind, Make(MSBS{}));
            case SOULS_MSB_ELDEN_RING: return Wrap(Kind, Make(MSBE{}));
        }
        ThrowInvalid("invalid MSB kind");
    }

    // The type names creatable in a param, from its per-type lists.
    template<typename Param>
    std::vector<std::string> CreatableTypes() {
        Param Proto;
        std::vector<std::string> Names;
        Proto.ForEachList([&](auto& List) {
            using Element = typename std::decay_t<decltype(List)>::value_type;
            Names.push_back(ShortTypeName(typeid(Element)));
        });
        return Names;
    }

    // Creates an entry of the named type in the param's list for it; null if the param has no such type.
    template<typename Param>
    auto* CreateOfType(Param& P, const std::string& Type) {
        using Base = std::remove_pointer_t<typename decltype(P.GetEntries())::value_type>;
        Base* Created = nullptr;
        P.ForEachList([&](auto& List) {
            using Element = typename std::decay_t<decltype(List)>::value_type;
            if (!Created && ShortTypeName(typeid(Element)) == Type) {
                List.emplace_back();
                Created = &List.back();
            }
        });
        return Created;
    }

    template<typename Param, typename Base>
    void EraseEntry(Param& P, Base* Target) {
        bool Removed = false;
        P.ForEachList([&](auto& List) {
            for (size_t I = 0; !Removed && I < List.size(); ++I) {
                if (static_cast<Base*>(&List[I]) == Target) {
                    List.erase(List.begin() + static_cast<std::ptrdiff_t>(I));
                    Removed = true;
                }
            }
        });
    }
}  // namespace

SOULS_C_API SoulsMsb* souls_msb_new(SoulsMsbKind kind) {
    return GuardValue<SoulsMsb*>(nullptr, [&] { return ByKind(kind, [](auto Empty) { return Empty; }); });
}

SOULS_C_API SoulsMsb* souls_msb_read_file(SoulsMsbKind kind, const char* path) {
    return GuardValue<SoulsMsb*>(nullptr, [&] {
        const std::filesystem::path P = PathFrom(path);
        return ByKind(kind, [&](auto Empty) { return std::decay_t<decltype(Empty)>::Read(P); });
    });
}

SOULS_C_API SoulsMsb* souls_msb_read_memory(SoulsMsbKind kind, const uint8_t* data, size_t size) {
    return GuardValue<SoulsMsb*>(nullptr, [&] {
        const auto Span = Bytes(data, size);
        return ByKind(kind, [&](auto Empty) { return std::decay_t<decltype(Empty)>::Read(Span); });
    });
}

SOULS_C_API void souls_msb_free(SoulsMsb* msb) {
    delete msb;
}

SOULS_C_API SoulsStatus souls_msb_write_file(SoulsMsb* msb, const char* path) {
    return Guard([&] {
        const std::filesystem::path P = PathFrom(path);
        WithMsb<void>(msb, [&](auto& Value) { Value.Write(P); });
    });
}

SOULS_C_API SoulsBuffer souls_msb_write_memory(SoulsMsb* msb) {
    return GuardValue(SoulsBuffer{nullptr, 0}, [&] {
        return WithMsb<SoulsBuffer>(msb, [&](auto& Value) { return MakeBuffer(Value.Write()); });
    });
}

SOULS_C_API SoulsMsbKind souls_msb_kind(const SoulsMsb* msb) {
    return GuardValue(SOULS_MSB_DARK_SOULS_1, [&] { return Require(msb, "msb is NULL").Kind; });
}

SOULS_C_API size_t souls_msb_entry_count(const SoulsMsb* msb, SoulsMsbCategory category) {
    return GuardValue<size_t>(0, [&] {
        size_t Count = 0;
        WithMsb<void>(msb, [&](auto& Value) { OnCategory(Value, category, [&](auto& Param) { Count = Param.GetEntries().size(); }); });
        return Count;
    });
}

SOULS_C_API const char* souls_msb_entry_name(const SoulsMsb* msb, SoulsMsbCategory category, size_t index) {
    return GuardValue<const char*>(nullptr, [&] {
        const char* Result = nullptr;
        WithMsb<void>(msb, [&](auto& Value) { OnEntry(Value, category, index, [&](auto& Entry) { Result = Entry.Name.c_str(); }); });
        return Result;
    });
}

SOULS_C_API SoulsStatus souls_msb_set_entry_name(SoulsMsb* msb, SoulsMsbCategory category, size_t index, const char* name) {
    return Guard([&] {
        const char* N = NameArg(name);
        WithMsb<void>(msb, [&](auto& Value) { OnEntry(Value, category, index, [&](auto& Entry) { Entry.Name = N; }); });
    });
}

SOULS_C_API const char* souls_msb_entry_type(const SoulsMsb* msb, SoulsMsbCategory category, size_t index) {
    return GuardValue<const char*>(nullptr, [&] {
        const char* Result = nullptr;
        WithMsb<void>(msb, [&](auto& Value) {
            OnEntry(Value, category, index, [&](auto& Entry) { Result = Intern(ShortTypeName(typeid(Entry))); });
        });
        return Result;
    });
}

SOULS_C_API int64_t souls_msb_find_entry(const SoulsMsb* msb, SoulsMsbCategory category, const char* name) {
    return GuardValue<int64_t>(-1, [&]() -> int64_t {
        const char* N  = NameArg(name);
        int64_t Result = -1;
        WithMsb<void>(msb, [&](auto& Value) {
            OnCategory(Value, category, [&](auto& Param) {
                const auto Entries = Param.GetEntries();
                for (size_t I = 0; I < Entries.size(); ++I) {
                    if (Entries[I]->Name == N) {
                        Result = static_cast<int64_t>(I);
                        return;
                    }
                }
            });
        });
        return Result;
    });
}

SOULS_C_API size_t souls_msb_type_count(const SoulsMsb* msb, SoulsMsbCategory category) {
    return GuardValue<size_t>(0, [&] {
        size_t Count = 0;
        WithMsb<void>(msb, [&](auto& Value) {
            OnCategory(Value, category, [&](auto& Param) { Count = CreatableTypes<std::decay_t<decltype(Param)>>().size(); });
        });
        return Count;
    });
}

SOULS_C_API const char* souls_msb_type_name(const SoulsMsb* msb, SoulsMsbCategory category, size_t type_index) {
    return GuardValue<const char*>(nullptr, [&] {
        const char* Result = nullptr;
        WithMsb<void>(msb, [&](auto& Value) {
            OnCategory(Value, category, [&](auto& Param) {
                const auto Names = CreatableTypes<std::decay_t<decltype(Param)>>();
                CheckIndex(type_index, Names.size(), "type index out of range");
                Result = Intern(Names[type_index]);
            });
        });
        return Result;
    });
}

SOULS_C_API int64_t souls_msb_add_entry(SoulsMsb* msb, SoulsMsbCategory category, const char* type_name, const char* name_or_null) {
    return GuardValue<int64_t>(-1, [&]() -> int64_t {
        const std::string Type = NameArg(type_name);
        int64_t Result         = -1;
        WithMsb<void>(msb, [&](auto& Value) {
            OnCategory(Value, category, [&](auto& Param) {
                auto* Created = CreateOfType(Param, Type);
                if (!Created) throw std::invalid_argument("unknown entry type \"" + Type + "\" for this category");
                if (name_or_null) Created->Name = name_or_null;
                const auto Entries = Param.GetEntries();
                for (size_t I = 0; I < Entries.size(); ++I) {
                    if (Entries[I] == Created) Result = static_cast<int64_t>(I);
                }
            });
        });
        return Result;
    });
}

SOULS_C_API SoulsStatus souls_msb_remove_entry(SoulsMsb* msb, SoulsMsbCategory category, size_t index) {
    return Guard([&] {
        WithMsb<void>(msb, [&](auto& Value) {
            OnCategory(Value, category, [&](auto& Param) {
                auto Entries = Param.GetEntries();
                CheckIndex(index, Entries.size(), "entry index out of range");
                EraseEntry(Param, Entries[index]);
            });
        });
    });
}

#pragma region Properties
namespace {
    Lister ListProperties(const SoulsMsb* Msb, SoulsMsbCategory Category, size_t Index) {
        Lister L;
        WithMsb<void>(Msb, [&](auto& Value) { OnEntry(Value, Category, Index, [&](auto& Entry) { ForProperties(Entry, L); }); });
        return L;
    }
}  // namespace

SOULS_C_API size_t souls_msb_property_count(const SoulsMsb* msb, SoulsMsbCategory category, size_t index) {
    return GuardValue<size_t>(0, [&] { return ListProperties(msb, category, index).Found.size(); });
}

SOULS_C_API const char* souls_msb_property_name(const SoulsMsb* msb, SoulsMsbCategory category, size_t index, size_t property) {
    return GuardValue<const char*>(nullptr, [&] {
        const Lister L = ListProperties(msb, category, index);
        CheckIndex(property, L.Found.size(), "property index out of range");
        return L.Found[property].first;
    });
}

SOULS_C_API SoulsPropertyKind souls_msb_property_kind(const SoulsMsb* msb, SoulsMsbCategory category, size_t index, size_t property) {
    return GuardValue(SOULS_PROPERTY_INT, [&] {
        const Lister L = ListProperties(msb, category, index);
        CheckIndex(property, L.Found.size(), "property index out of range");
        switch (L.Found[property].second) {
            case PK::Int: return SOULS_PROPERTY_INT;
            case PK::String: return SOULS_PROPERTY_STRING;
            case PK::Vec3: return SOULS_PROPERTY_VEC3;
            case PK::IntArray: return SOULS_PROPERTY_INT_ARRAY;
        }
        return SOULS_PROPERTY_INT;
    });
}

SOULS_C_API SoulsStatus souls_msb_get_int(const SoulsMsb* msb, SoulsMsbCategory category, size_t index, const char* property, int64_t* out_value) {
    return GuardProperty([&] {
        if (!out_value) ThrowInvalid("out_value is NULL");
        Access A{NameArg(property), Op::GetInt};
        Apply(msb, category, index, A);
        *out_value = A.Int;
    });
}

SOULS_C_API SoulsStatus souls_msb_set_int(SoulsMsb* msb, SoulsMsbCategory category, size_t index, const char* property, int64_t value) {
    return GuardProperty([&] {
        Access A{NameArg(property), Op::SetInt};
        A.Int = value;
        Apply(msb, category, index, A);
    });
}

SOULS_C_API SoulsStatus souls_msb_get_string(const SoulsMsb* msb, SoulsMsbCategory category, size_t index, const char* property, const char** out_text) {
    return GuardProperty([&] {
        if (!out_text) ThrowInvalid("out_text is NULL");
        Access A{NameArg(property), Op::GetString};
        Apply(msb, category, index, A);
        *out_text = A.TextOut;
    });
}

SOULS_C_API SoulsStatus souls_msb_set_string(SoulsMsb* msb, SoulsMsbCategory category, size_t index, const char* property, const char* text_or_null) {
    return GuardProperty([&] {
        Access A{NameArg(property), Op::SetString};
        A.TextIn = text_or_null;
        Apply(msb, category, index, A);
    });
}

SOULS_C_API SoulsStatus souls_msb_get_vec3(const SoulsMsb* msb, SoulsMsbCategory category, size_t index, const char* property, float out_xyz[3]) {
    return GuardProperty([&] {
        if (!out_xyz) ThrowInvalid("out_xyz is NULL");
        Access A{NameArg(property), Op::GetVec3};
        A.VecOut = out_xyz;
        Apply(msb, category, index, A);
    });
}

SOULS_C_API SoulsStatus souls_msb_set_vec3(SoulsMsb* msb, SoulsMsbCategory category, size_t index, const char* property, const float xyz[3]) {
    return GuardProperty([&] {
        if (!xyz) ThrowInvalid("xyz is NULL");
        Access A{NameArg(property), Op::SetVec3};
        A.VecIn = xyz;
        Apply(msb, category, index, A);
    });
}

SOULS_C_API SoulsStatus souls_msb_get_int_array(const SoulsMsb* msb, SoulsMsbCategory category, size_t index, const char* property,
                                                int64_t* out, size_t capacity, size_t* out_count) {
    return GuardProperty([&] {
        Access A{NameArg(property), Op::GetArray};
        A.ArrayOut = out;
        A.Capacity = out ? capacity : 0;
        A.Count    = out_count;
        Apply(msb, category, index, A);
    });
}

SOULS_C_API SoulsStatus souls_msb_set_int_array(SoulsMsb* msb, SoulsMsbCategory category, size_t index, const char* property,
                                                const int64_t* values, size_t count) {
    return GuardProperty([&] {
        if (!values && count) ThrowInvalid("values is NULL");
        Access A{NameArg(property), Op::SetArray};
        A.ArrayIn = values;
        A.CountIn = count;
        Apply(msb, category, index, A);
    });
}
#pragma endregion

#pragma region Shapes
namespace {
    template<typename F>
    void RegionShape(SoulsMsb& Handle, size_t Index, F&& Fn) {
        std::visit([&](auto& Msb) {
            auto Entries = Msb.Regions.GetEntries();
            CheckIndex(Index, Entries.size(), "region index out of range");
            Fn(Entries[Index]->Shape);
        }, Handle.Value);
    }
}  // namespace

SOULS_C_API SoulsShapeType souls_msb_region_shape(const SoulsMsb* msb, size_t region_index) {
    return GuardValue(SOULS_SHAPE_POINT, [&] {
        SoulsShapeType Result = SOULS_SHAPE_POINT;
        RegionShape(const_cast<SoulsMsb&>(Require(msb, "msb is NULL")), region_index, [&](Msb::Shape& S) {
            const auto Type = Msb::TypeOf(S);
            if (Type == Msb::ShapeType::Composite) throw BinaryException("Composite shapes are not supported by the C API.");
            Result = static_cast<SoulsShapeType>(static_cast<int>(Type));
        });
        return Result;
    });
}

SOULS_C_API SoulsStatus souls_msb_region_shape_dims(const SoulsMsb* msb, size_t region_index, float out_dims[3]) {
    return Guard([&] {
        if (!out_dims) ThrowInvalid("out_dims is NULL");
        out_dims[0] = out_dims[1] = out_dims[2] = 0;
        RegionShape(const_cast<SoulsMsb&>(Require(msb, "msb is NULL")), region_index, [&](Msb::Shape& S) {
            std::visit([&](auto& Shape) {
                using T = std::decay_t<decltype(Shape)>;
                if constexpr (std::is_same_v<T, Msb::Shapes::Circle> || std::is_same_v<T, Msb::Shapes::Sphere>) {
                    out_dims[0] = Shape.Radius;
                } else if constexpr (std::is_same_v<T, Msb::Shapes::Cylinder>) {
                    out_dims[0] = Shape.Radius;
                    out_dims[1] = Shape.Height;
                } else if constexpr (std::is_same_v<T, Msb::Shapes::Rect>) {
                    out_dims[0] = Shape.Width;
                    out_dims[1] = Shape.Depth;
                } else if constexpr (std::is_same_v<T, Msb::Shapes::Box>) {
                    out_dims[0] = Shape.Width;
                    out_dims[1] = Shape.Depth;
                    out_dims[2] = Shape.Height;
                } else if constexpr (std::is_same_v<T, Msb::Shapes::Composite>) {
                    throw BinaryException("Composite shapes are not supported by the C API.");
                }
            }, S);
        });
    });
}

SOULS_C_API SoulsStatus souls_msb_set_region_shape(SoulsMsb* msb, size_t region_index, SoulsShapeType shape, const float dims[3]) {
    return Guard([&] {
        if (shape < SOULS_SHAPE_POINT || shape > SOULS_SHAPE_BOX) ThrowInvalid("invalid shape type");
        if (!dims && shape != SOULS_SHAPE_POINT) ThrowInvalid("dims is NULL");
        RegionShape(Require(msb, "msb is NULL"), region_index, [&](Msb::Shape& S) {
            switch (shape) {
                case SOULS_SHAPE_POINT: S = Msb::Shapes::Point{}; break;
                case SOULS_SHAPE_CIRCLE: S = Msb::Shapes::Circle{dims[0]}; break;
                case SOULS_SHAPE_SPHERE: S = Msb::Shapes::Sphere{dims[0]}; break;
                case SOULS_SHAPE_CYLINDER: S = Msb::Shapes::Cylinder{dims[0], dims[1]}; break;
                case SOULS_SHAPE_RECT: S = Msb::Shapes::Rect{dims[0], dims[1]}; break;
                case SOULS_SHAPE_BOX: S = Msb::Shapes::Box{dims[0], dims[1], dims[2]}; break;
            }
        });
    });
}
#pragma endregion
