//
// Created by Jake Rieger on 10/8/2026.
//

#pragma once

#include <libSouls/BinaryReader.hpp>
#include <libSouls/BinaryWriter.hpp>
#include <libSouls/Color.hpp>
#include <libSouls/Vector.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <variant>
#include <vector>

// Pieces shared by the MSB (map layout) formats of the different games.
//
// Entries in an MSB refer to each other by index in the file, but are exposed here by name (as upstream SoulsFormats
// does), so renaming or reordering entries doesn't break references. A reference is a Ref: it behaves like an
// optional string holding the target's name, and also carries the on-disk index, which is only meaningful while
// reading and writing.
//
// Each entry type describes the layout of its type-specific data once, in a Fields function that takes a "visitor".
// The visitors read, write, resolve indices to names and resolve names to indices, so the layout can't get out of sync
// between reading and writing.
namespace Souls::Msb {

    // Tags naming which list of entries a Ref points into.
    struct ModelList {};
    struct EventList {};
    struct RegionList {};
    struct PartList {};
    struct CollisionList {};  // just the collision parts (a sublist of PartList); MSB1 connect collisions use it
    struct BoneNameList {};

    // A reference to another entry in the MSB. Name is empty for "no reference" (index -1).
    template<typename ListTag, typename Storage = int32_t>
    struct Ref : std::optional<std::string> {
        using std::optional<std::string>::optional;
        using std::optional<std::string>::operator=;

        Ref() = default;
        Ref(const char* Text) : std::optional<std::string>(std::string(Text)) {}

        // The index in the file; set while reading (before names are resolved) and while writing (after indices are
        // resolved). Not meaningful in between.
        Storage Index = static_cast<Storage>(-1);
    };

    // The shape of a region.
    enum class ShapeType : uint32_t {
        None      = 0xFFFFFFFF,
        Point     = 0,
        Circle    = 1,
        Sphere    = 2,
        Cylinder  = 3,
        Rect      = 4,
        Box       = 5,
        Composite = 6,
    };

    namespace Shapes {
        // A single point.
        struct Point {
            static constexpr ShapeType Type = ShapeType::Point;
            template<typename V>
            void Fields(V&) {}
        };

        // A flat circle.
        struct Circle {
            static constexpr ShapeType Type = ShapeType::Circle;
            float Radius                    = 1;
            template<typename V>
            void Fields(V& F) {
                F(Radius);
            }
        };

        // A volumetric sphere.
        struct Sphere {
            static constexpr ShapeType Type = ShapeType::Sphere;
            float Radius                    = 1;
            template<typename V>
            void Fields(V& F) {
                F(Radius);
            }
        };

        // A volumetric cylinder.
        struct Cylinder {
            static constexpr ShapeType Type = ShapeType::Cylinder;
            float Radius                    = 1;
            float Height                    = 1;
            template<typename V>
            void Fields(V& F) {
                F(Radius);
                F(Height);
            }
        };

        // A flat rectangle.
        struct Rect {
            static constexpr ShapeType Type = ShapeType::Rect;
            float Width                     = 1;
            float Depth                     = 1;
            template<typename V>
            void Fields(V& F) {
                F(Width);
                F(Depth);
            }
        };

        // A rectangular prism.
        struct Box {
            static constexpr ShapeType Type = ShapeType::Box;
            float Width                     = 1;
            float Depth                     = 1;
            float Height                    = 1;
            template<typename V>
            void Fields(V& F) {
                F(Width);
                F(Depth);
                F(Height);
            }
        };

        // A shape composed of references to other regions' shapes.
        struct Composite {
            static constexpr ShapeType Type = ShapeType::Composite;

            struct Child {
                // The child region.
                Ref<RegionList> Region;
                int32_t Unk04 = 0;
            };

            std::array<Child, 8> Children;

            template<typename V>
            void Fields(V& F) {
                for (Child& C : Children) {
                    F(C.Region);
                    F(C.Unk04);
                }
            }
        };
    }  // namespace Shapes

    // Describes the space that a region occupies.
    using Shape = std::variant<Shapes::Point, Shapes::Circle, Shapes::Sphere, Shapes::Cylinder, Shapes::Rect, Shapes::Box,
                               Shapes::Composite>;

    inline ShapeType TypeOf(const Shape& S) {
        return std::visit([](const auto& Value) { return std::decay_t<decltype(Value)>::Type; }, S);
    }

    // Whether the shape has extra data after the region's common data (everything but a point).
    inline bool HasShapeData(const Shape& S) {
        return !std::holds_alternative<Shapes::Point>(S);
    }

    // Creates a shape of the given type; throws for types that don't exist.
    inline Shape CreateShape(ShapeType Type) {
        switch (Type) {
            case ShapeType::Point: return Shapes::Point{};
            case ShapeType::Circle: return Shapes::Circle{};
            case ShapeType::Sphere: return Shapes::Sphere{};
            case ShapeType::Cylinder: return Shapes::Cylinder{};
            case ShapeType::Rect: return Shapes::Rect{};
            case ShapeType::Box: return Shapes::Box{};
            case ShapeType::Composite: return Shapes::Composite{};
            default: throw BinaryException("Unimplemented MSB shape type: " + std::to_string(static_cast<uint32_t>(Type)));
        }
    }

    namespace Detail {
        // Reads fields in order.
        struct ReadVisitor {
            BinaryReader& Reader;

            template<Scalar T>
            void operator()(T& Value) {
                if constexpr (std::is_same_v<T, bool>) {
                    Value = Reader.ReadBool();
                } else {
                    Value = Reader.Read<T>();
                }
            }
            void operator()(Vector2& Value) { Value = Reader.ReadVector2(); }
            void operator()(Vector3& Value) { Value = Reader.ReadVector3(); }
            void operator()(Vector4& Value) { Value = Reader.ReadVector4(); }
            template<typename T, size_t N>
            void operator()(std::array<T, N>& Values) {
                for (T& Value : Values) {
                    (*this)(Value);
                }
            }
            template<typename Tag, typename Storage>
            void operator()(Ref<Tag, Storage>& Reference) {
                Reference.Index = Reader.Read<Storage>();
            }

            // A value that must be one of the given constants.
            template<Scalar T, typename... Rest>
            void Const(T First, Rest... Others) {
                Reader.Assert<T>(First, static_cast<T>(Others)...);
            }
            // A boolean stored as a 32-bit 0 or 1.
            void AsInt32(bool& Value) { Value = Reader.Assert<int32_t>(0, 1) == 1; }
            // Whether the file uses 64-bit offsets.
            bool Long() const { return Reader.VarintLong; }
            // A 32- or 64-bit constant, depending on the file.
            void ConstVarint(int64_t Value) { Reader.AssertVarint(Value); }
            // A color stored as red, green, blue, alpha bytes.
            void RGBA(Color& Value) { Value = ReadRGBA(Reader); }
            // Count bytes that must be zero.
            void Pad(size_t Count) { Reader.AssertPattern(Count, 0); }
            // Count bytes that must all equal Value.
            void Pattern(size_t Count, uint8_t Value) { Reader.AssertPattern(Count, Value); }
        };

        // Writes fields in order.
        struct WriteVisitor {
            BinaryWriter& Writer;

            template<Scalar T>
            void operator()(T& Value) {
                if constexpr (std::is_same_v<T, bool>) {
                    Writer.WriteBool(Value);
                } else {
                    Writer.Write<T>(Value);
                }
            }
            void operator()(Vector2& Value) { Writer.WriteVector2(Value); }
            void operator()(Vector3& Value) { Writer.WriteVector3(Value); }
            void operator()(Vector4& Value) { Writer.WriteVector4(Value); }
            template<typename T, size_t N>
            void operator()(std::array<T, N>& Values) {
                for (T& Value : Values) {
                    (*this)(Value);
                }
            }
            template<typename Tag, typename Storage>
            void operator()(Ref<Tag, Storage>& Reference) {
                Writer.Write<Storage>(Reference.Index);
            }

            template<Scalar T, typename... Rest>
            void Const(T First, Rest...) {
                Writer.Write<T>(First);
            }
            void AsInt32(bool& Value) { Writer.WriteInt32(Value ? 1 : 0); }
            bool Long() const { return Writer.VarintLong; }
            void ConstVarint(int64_t Value) { Writer.WriteVarint(Value); }
            void RGBA(Color& Value) { WriteRGBA(Writer, Value); }
            void Pad(size_t Count) { Writer.Pad(Count); }
            void Pattern(size_t Count, uint8_t Value) { Writer.Pad(Count, Value); }
        };

        // Looks up the name of the entry at an index (nullopt for -1).
        template<typename EntryT>
        std::optional<std::string> FindName(const std::vector<EntryT*>& List, int64_t Index) {
            if (Index == -1) {
                return std::nullopt;
            }
            if (Index < 0 || static_cast<size_t>(Index) >= List.size()) {
                throw BinaryException("MSB reference index " + std::to_string(Index) + " is out of range");
            }
            return List[static_cast<size_t>(Index)]->Name;
        }

        // Looks up the index of the entry with a name (-1 for none).
        template<typename EntryT>
        int64_t FindIndex(const std::vector<EntryT*>& List, const std::optional<std::string>& Name) {
            if (!Name) {
                return -1;
            }
            for (size_t I = 0; I < List.size(); ++I) {
                if (List[I]->Name == *Name) {
                    return static_cast<int64_t>(I);
                }
            }
            throw BinaryException("MSB name not found: " + *Name);
        }

        // Turns the indices read from the file into names. Entries supplies List<Tag>() for each kind of reference.
        template<typename Entries>
        struct NameVisitor {
            const Entries& Lists;

            template<typename T>
            void operator()(T&) {}
            template<typename T, size_t N>
            void operator()(std::array<T, N>& Values) {
                for (T& Value : Values) {
                    (*this)(Value);
                }
            }
            template<typename Tag, typename Storage>
            void operator()(Ref<Tag, Storage>& Reference) {
                static_cast<std::optional<std::string>&>(Reference) = FindName(Lists.template List<Tag>(), Reference.Index);
            }
            template<Scalar T, typename... Rest>
            void Const(T, Rest...) {}
            void AsInt32(bool&) {}
            bool Long() const { return false; }
            void ConstVarint(int64_t) {}
            void RGBA(Color&) {}
            void Pad(size_t) {}
            void Pattern(size_t, uint8_t) {}
        };

        // Turns names into the indices to write.
        template<typename Entries>
        struct IndexVisitor {
            const Entries& Lists;

            template<typename T>
            void operator()(T&) {}
            template<typename T, size_t N>
            void operator()(std::array<T, N>& Values) {
                for (T& Value : Values) {
                    (*this)(Value);
                }
            }
            template<typename Tag, typename Storage>
            void operator()(Ref<Tag, Storage>& Reference) {
                Reference.Index = static_cast<Storage>(FindIndex(Lists.template List<Tag>(), Reference));
            }
            template<Scalar T, typename... Rest>
            void Const(T, Rest...) {}
            void AsInt32(bool&) {}
            bool Long() const { return false; }
            void ConstVarint(int64_t) {}
            void RGBA(Color&) {}
            void Pad(size_t) {}
            void Pattern(size_t, uint8_t) {}
        };

        // Visits the data of a shape with any visitor.
        template<typename V>
        void VisitShape(Shape& S, V& F) {
            std::visit([&](auto& Value) { Value.Fields(F); }, S);
        }

        // Entry names in a map can repeat; upstream makes them unique by adding " {2}", " {3}"... on reading (so
        // names can be used as keys) and removes those when writing.
        template<typename EntryT>
        void DisambiguateNames(const std::vector<EntryT*>& Entries) {
            bool Ambiguous;
            do {
                Ambiguous = false;
                std::unordered_map<std::string, int> Counts;
                for (EntryT* Entry : Entries) {
                    const auto Found = Counts.find(Entry->Name);
                    if (Found == Counts.end()) {
                        Counts.emplace(Entry->Name, 1);
                    } else {
                        Ambiguous = true;
                        ++Found->second;
                        Entry->Name = Entry->Name + " {" + std::to_string(Found->second) + "}";
                    }
                }
            } while (Ambiguous);
        }

        // The header shared by the 64-bit MSB formats (Dark Souls II and later).
        inline void AssertHeader(BinaryReader& Reader) {
            Reader.AssertMagic("MSB ");
            Reader.Assert<int32_t>(1);
            Reader.Assert<int32_t>(0x10);
            Reader.Assert<uint8_t>(0);  // is big endian
            Reader.Assert<uint8_t>(0);  // is bit big endian
            Reader.Assert<uint8_t>(1);  // text encoding
            Reader.Assert<uint8_t>(0xFF);  // is 64-bit offset
        }

        inline void WriteHeader(BinaryWriter& Writer) {
            Writer.WriteMagic("MSB ");
            Writer.WriteInt32(1);
            Writer.WriteInt32(0x10);
            Writer.WriteBool(false);
            Writer.WriteBool(false);
            Writer.WriteByte(1);
            Writer.WriteByte(0xFF);
        }

        // Removes the " {n}" suffix DisambiguateNames adds.
        std::string ReambiguateName(const std::string& Name);
    }  // namespace Detail

}  // namespace Souls::Msb
