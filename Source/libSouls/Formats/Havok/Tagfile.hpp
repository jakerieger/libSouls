// Copyright (C) 2026 Jake Rieger
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, version 3. This program is distributed in the hope that it will
// be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU General
// Public License along with this program. If not, see https://www.gnu.org/licenses/.

#pragma once

#include <libSouls/Souls.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/// @brief Havok tagfiles (SDK 2018, as used by Elden Ring): the binary .hkx form that describes its own types.
///
/// A tagfile is a tree of sections. DATA holds the objects exactly as they sit in memory on 64-bit Windows, except that
/// pointers hold *item* indices; INDX/ITEM lists the items (an object or an array of them, with its type and where its
/// data is); TYPE describes every type by name, size and members. Animation binders keep the types of all their
/// animations once, in a separate .compendium file, and each animation names the compendium it needs (TCRF).
///
/// Reading gives a generic view: start at Root() and walk members by name.
///
/// @code
/// Havok::Tagfile File = Havok::Tagfile::Read(Bytes);
/// for (Havok::Value Skeleton : File.ObjectsOfType("hkaSkeleton")) {
///     std::string Name = Skeleton["name"].AsString();
///     Havok::Value Bones = Skeleton["bones"];
///     for (size_t I = 0; I < Bones.Size(); ++I) Bones.At(I)["name"].AsString();
/// }
/// @endcode
namespace Souls::Havok {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of exported classes; only touched by this DLL's own code

    /// @brief What kind of value a type holds (the low bits of its subtype).
    enum class Kind : uint8_t {
        Void    = 0,
        Opaque  = 1,
        Bool    = 2,
        String  = 3,
        Int     = 4,
        Float   = 5,
        Pointer = 6,
        Record  = 7,
        Array   = 8,
    };

    /// @brief A member of a record type.
    struct SOULS_API Member {
        /// @brief The member's name.
        std::string Name;
        /// @brief Member flags (not interpreted).
        uint32_t Flags = 0;
        /// @brief Where the member is, in bytes from the start of the record.
        uint32_t Offset = 0;
        /// @brief The member's type (an index into the type table).
        uint32_t Type = 0;
    };

    /// @brief One type as the file describes it.
    ///
    /// Fields a type doesn't set come from its parent (a typedef sets none).
    struct SOULS_API TypeInfo {
        /// @brief The type's name, without template arguments.
        std::string Name;
        /// @brief Template arguments: names starting with 't' are types (Value is a type index), 'v' are values.
        std::vector<std::pair<std::string, int64_t>> Templates;
        /// @brief The base type (for records) or the type this one is a typedef of; 0 for none.
        uint32_t Parent = 0;
        uint32_t Flags  = 0;  ///< which of the fields below are set
        /// @brief The kind of value (low 5 bits, see Kind) plus details such as integer size and signedness.
        uint32_t Subtype = 0;
        uint32_t Pointer = 0;  ///< the pointed-to or element type
        /// @brief The type's version.
        uint32_t Version = 0;
        /// @brief The size of a value of this type, in bytes.
        uint32_t Size = 0;
        /// @brief The alignment of a value of this type, in bytes.
        uint32_t Alignment = 0;
        /// @brief Unknown.
        uint32_t Unk10 = 0;
        /// @brief The members this type adds to its base (records only).
        std::vector<Member> Members;
        /// @brief Interfaces the type implements, as (type, value) pairs.
        std::vector<std::pair<uint32_t, uint32_t>> Interfaces;
        /// @brief The type's hash from the THSH section, or 0.
        uint32_t Hash = 0;
    };

    /// @brief The types of a tagfile or a compendium.
    ///
    /// Index 0 is "no type".
    class SOULS_API TypeTable {
    public:
        /// @brief Every type, by index.
        std::vector<TypeInfo> Types;

        /// @brief The type at an index; throws BinaryException for 0 or an index out of range.
        NODISCARD const TypeInfo& At(uint32_t Index) const;
        /// @brief The name with its template arguments, such as "hkArray<hkaBone, hkContainerHeapAllocator>".
        NODISCARD std::string FullName(uint32_t Index) const;
        /// @brief The first type with this name (no template arguments), or 0.
        NODISCARD uint32_t Find(std::string_view Name) const;
        /// @brief Whether Index is the type Base or derives from it.
        NODISCARD bool IsA(uint32_t Index, uint32_t Base) const;

        /// @brief Fields resolved through the parent chain.
        NODISCARD uint32_t SubtypeOf(uint32_t Index) const;
        /// @brief The kind of value a type holds.
        NODISCARD Kind KindOf(uint32_t Index) const;
        /// @brief The size of a value of the type, in bytes.
        NODISCARD uint32_t SizeOf(uint32_t Index) const;
        /// @brief The pointed-to type of a pointer, or the element type of an array.
        NODISCARD uint32_t PointerOf(uint32_t Index) const;
        /// @brief A member of the type or of one of its bases; nullptr if there is none.
        NODISCARD const Member* FindMember(uint32_t Index, std::string_view Name) const;
        /// @brief All members, bases first.
        NODISCARD std::vector<const Member*> AllMembers(uint32_t Index) const;
    };

    /// @brief The shared types of an animation binder (.compendium).
    class SOULS_API Compendium {
    public:
        /// @brief The IDs tagfiles use to name this compendium.
        std::vector<uint64_t> IDs;
        /// @brief The types every animation of the binder uses.
        std::shared_ptr<const TypeTable> Types;

        /// @brief Reads a compendium; throws BinaryException if the data isn't one.
        static Compendium Read(std::span<const uint8_t> Data);
        /// @brief Whether the data is a compendium (TCM0).
        static bool Is(std::span<const uint8_t> Data);
    };

    class Tagfile;

    /// @brief A value inside a tagfile: a type and where its data is.
    ///
    /// Cheap to copy; valid while the tagfile is.
    class SOULS_API Value {
    public:
        Value() = default;
        /// @brief A value of a type at an offset in a tagfile's data.
        Value(const Tagfile* File, uint32_t Type, size_t Offset) : _File(File), _Type(Type), _Offset(Offset) {}

        /// @brief Whether this refers to a value (false for a default-constructed Value or a null pointer's target).
        NODISCARD bool IsValid() const { return _File != nullptr && _Type != 0; }
        /// @brief Where the value is in the tagfile's data, in bytes.
        NODISCARD size_t DataOffset() const { return _Offset; }
        /// @brief The value's type, as an index into the tagfile's type table.
        NODISCARD uint32_t TypeIndex() const { return _Type; }
        /// @brief The type's name, as SoulsFormats calls it.
        NODISCARD std::string TypeName() const;
        /// @brief The kind of value (Kind::Void for an invalid one).
        NODISCARD Kind GetKind() const;
        /// @brief Whether the value's type is the named type or derives from it (false for an invalid value).
        NODISCARD bool IsA(std::string_view TypeName) const;

        /// @brief A member of a record, by name; throws BinaryException if there is no such member.
        Value operator[](std::string_view Name) const;
        /// @brief Whether the record has a member with this name.
        NODISCARD bool Has(std::string_view Name) const;

        /// @brief The value of a bool or integer, as a bool.
        NODISCARD bool AsBool() const;
        /// @brief The value of an integer (or enum, or bool), sign-extended as its type says.
        NODISCARD int64_t AsInt() const;
        /// @brief The value of a float (integers are converted).
        NODISCARD double AsFloat() const;
        /// @brief Strings (char* and hkStringPtr); empty for a null string.
        NODISCARD std::string AsString() const;
        /// @brief What a pointer (T*, hkRefPtr, hkRefVariant) points to; an invalid value for null.
        NODISCARD Value Deref() const;

        /// @brief Arrays (hkArray and fixed-size ones such as hkVector4f): their length and elements.
        NODISCARD size_t Size() const;
        /// @brief An element of an array; throws BinaryException if Index is out of range.
        NODISCARD Value At(size_t Index) const;
        /// @brief An array of numbers, as floats or integers.
        NODISCARD std::vector<float> AsFloats() const;
        /// @brief An array of integers.
        NODISCARD std::vector<int64_t> AsInts() const;
        /// @brief The raw bytes of an hkArray of 8-bit integers (such as compressed animation data).
        NODISCARD std::span<const uint8_t> AsBytes() const;

    private:
        const Tagfile* _File = nullptr;
        uint32_t _Type       = 0;
        size_t _Offset       = 0;

        NODISCARD uint64_t ItemIndex() const;  ///< the item a pointer-like value refers to
        NODISCARD const uint8_t* Bytes(size_t Count) const;

        friend class Tagfile;
    };

    /// @brief A Havok tagfile: its objects' data, the types that describe it, and the item table that locates each
    /// object.
    class SOULS_API Tagfile {
    public:
        /// @brief An entry of the item table: one object, or an array of elements.
        struct Item {
            /// @brief The type of the object or of each element.
            uint32_t Type   = 0;
            uint8_t Flags   = 0;  ///< 0x10: an object; 0x20: an array of Count elements
            uint32_t Offset = 0;  ///< in Data
            /// @brief How many elements (1 for an object).
            uint32_t Count = 0;
        };

        /// @brief The Havok SDK version, such as "20180100".
        std::string SdkVersion;
        /// @brief The DATA section: every object as it sits in memory.
        std::vector<uint8_t> Data;
        /// @brief The types, from the file itself or from its compendium.
        std::shared_ptr<const TypeTable> Types;
        /// @brief The item table; item 0 is "null" and item 1 is the root object.
        std::vector<Item> Items;
        /// @brief Set for tagfiles whose types are in a compendium.
        std::optional<uint64_t> CompendiumID;
        /// @brief For tagfiles that carry their own types, the TYPE section as read (header included), which Write
        ///        puts back unchanged.
        std::vector<uint8_t> TypeSection;

        /// @brief Reads a tagfile.
        ///
        /// Files whose types are in a compendium need it; throws BinaryException if it's missing or doesn't have the ID
        /// the file asks for.
        static Tagfile Read(std::span<const uint8_t> Data, const Compendium* Types = nullptr);
        /// @brief Whether the data is a tagfile (TAG0).
        static bool Is(std::span<const uint8_t> Data);

        /// @brief The root object (normally an hkRootLevelContainer).
        NODISCARD Value Root() const;
        /// @brief The object of an item.
        NODISCARD Value ItemValue(size_t Index) const;
        /// @brief Every object (not array element) whose type is, or derives from, the named type.
        NODISCARD std::vector<Value> ObjectsOfType(std::string_view TypeName) const;

        /// @brief Writes the tagfile: its data, its types (or compendium reference) and an item table and pointer
        ///        patch list regenerated from the data.
        ///
        /// A tagfile that was read and not changed writes back byte for byte.
        NODISCARD std::vector<uint8_t> Write() const;

        /// @brief Sets an integer, enum or bool value of this tagfile, at its type's size.
        void SetInt(const Value& Field, int64_t NewValue);
        /// @brief Sets a float value of this tagfile, at its type's size.
        void SetFloat(const Value& Field, double NewValue);
        /// @brief Replaces the elements of an hkArray of this tagfile.
        ///
        /// Elements holds Count elements as they lie in the data (any pointers in them are item indices). The data is
        /// laid out again, so Values taken before the call point at stale offsets; take them again afterwards. An
        /// empty array gets a new item, and Count 0 empties the array. Throws BinaryException if Field isn't an hkArray
        /// of this tagfile or Elements isn't Count elements long.
        void SetArray(const Value& Field, std::span<const uint8_t> Elements, size_t Count);

    private:
        void Relayout(size_t Replaced, std::span<const uint8_t> Bytes);
        NODISCARD size_t ItemBytes(const Item& I) const;
    };

#pragma warning(pop)

}  // namespace Souls::Havok
