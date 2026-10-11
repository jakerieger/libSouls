// Copyright (C) 2026 Jake Rieger
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, version 3. This program is distributed in the hope that it will
// be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU General
// Public License along with this program. If not, see https://www.gnu.org/licenses/.

#include "Tagfile.hpp"

#include <libSouls/Endian.hpp>

#include <algorithm>
#include <cstring>

namespace Souls::Havok {
    namespace {
        // A section: a big-endian u32 of 2 flag bits and the 30-bit size (header included), then a 4-character tag.
        // Flags 0 means the body is more sections; 1 means it's data.
        struct Section {
            std::string Tag;
            uint32_t Flags = 0;
            size_t Begin   = 0;  // the body
            size_t End     = 0;
        };

        std::vector<Section> ReadSections(std::span<const uint8_t> Data, size_t Begin, size_t End) {
            std::vector<Section> Result;
            size_t Pos = Begin;
            while (Pos + 8 <= End) {
                const uint32_t Word = (uint32_t(Data[Pos]) << 24) | (uint32_t(Data[Pos + 1]) << 16) |
                                      (uint32_t(Data[Pos + 2]) << 8) | uint32_t(Data[Pos + 3]);
                const size_t Size = Word & 0x3FFFFFFF;
                if (Size < 8 || Pos + Size > End) {
                    throw BinaryException("Invalid Havok tagfile section size at " + std::to_string(Pos));
                }
                Result.push_back({std::string(reinterpret_cast<const char*>(Data.data()) + Pos + 4, 4),
                                  Word >> 30,
                                  Pos + 8,
                                  Pos + Size});
                Pos += Size;
            }
            return Result;
        }

        const Section* Find(const std::vector<Section>& Sections, std::string_view Tag) {
            for (const Section& S : Sections) {
                if (S.Tag == Tag) return &S;
            }
            return nullptr;
        }

        const Section& Require(const std::vector<Section>& Sections, std::string_view Tag) {
            const Section* S = Find(Sections, Tag);
            if (!S) throw BinaryException("Havok tagfile has no " + std::string(Tag) + " section");
            return *S;
        }

        // Reads the variable-length integers of the type sections: the number of leading 1 bits in the first byte
        // says how many more bytes follow (big-endian).
        class VarReader {
        public:
            VarReader(std::span<const uint8_t> Data, size_t Begin, size_t End) : Data(Data), Pos(Begin), End(End) {}

            bool AtEnd() const { return Pos >= End; }

            uint32_t Next() {
                const uint8_t B = Byte();
                if ((B & 0x80) == 0) return B;
                if ((B & 0xC0) == 0x80) return (uint32_t(B & 0x3F) << 8) | Byte();
                if ((B & 0xE0) == 0xC0) {
                    const uint32_t Hi  = uint32_t(B & 0x1F) << 16;
                    const uint32_t Mid = uint32_t(Byte()) << 8;
                    return Hi | Mid | Byte();
                }
                if ((B & 0xF8) == 0xE0) {
                    uint32_t Value = uint32_t(B & 0x07) << 24;
                    Value |= uint32_t(Byte()) << 16;
                    Value |= uint32_t(Byte()) << 8;
                    return Value | Byte();
                }
                throw BinaryException("Unsupported Havok varint prefix " + std::to_string(B));
            }

            uint32_t U32() {
                uint32_t Value = 0;
                for (int I = 0; I < 4; ++I)
                    Value |= uint32_t(Byte()) << (8 * I);
                return Value;
            }

        private:
            std::span<const uint8_t> Data;
            size_t Pos;
            size_t End;

            uint8_t Byte() {
                if (Pos >= End) throw BinaryException("Havok type data ends unexpectedly");
                return Data[Pos++];
            }
        };

        std::vector<std::string> Strings(std::span<const uint8_t> Data, const Section& S) {
            std::vector<std::string> Result;
            size_t Start = S.Begin;
            for (size_t I = S.Begin; I < S.End; ++I) {
                if (Data[I] == 0) {
                    Result.emplace_back(reinterpret_cast<const char*>(Data.data()) + Start, I - Start);
                    Start = I + 1;
                }
            }
            return Result;
        }

        const std::string& StringAt(const std::vector<std::string>& List, uint32_t Index, const char* What) {
            if (Index >= List.size())
                throw BinaryException(std::string("Havok ") + What + " string index out of range");
            return List[Index];
        }

        // Reads the TYPE section: names (TNA1/TNAM), bodies (TBDY/TBOD) and hashes (THSH).
        std::shared_ptr<TypeTable> ReadTypes(std::span<const uint8_t> Data, const Section& TypeSection) {
            const std::vector<Section> Parts            = ReadSections(Data, TypeSection.Begin, TypeSection.End);
            const std::vector<std::string> TypeStrings  = Strings(Data, Require(Parts, "TSTR"));
            const std::vector<std::string> FieldStrings = Strings(Data, Require(Parts, "FSTR"));

            auto Table              = std::make_shared<TypeTable>();
            const Section* NamesSec = Find(Parts, "TNA1") ? Find(Parts, "TNA1") : &Require(Parts, "TNAM");
            VarReader Names(Data, NamesSec->Begin, NamesSec->End);
            const uint32_t Count = Names.Next();
            Table->Types.resize(Count);
            for (uint32_t I = 1; I < Count; ++I) {
                TypeInfo& T         = Table->Types[I];
                T.Name              = StringAt(TypeStrings, Names.Next(), "type");
                const uint32_t Args = Names.Next();
                for (uint32_t A = 0; A < Args; ++A) {
                    const std::string& ArgName = StringAt(TypeStrings, Names.Next(), "type");
                    T.Templates.emplace_back(ArgName, Names.Next());
                }
            }

            const Section* BodySec = Find(Parts, "TBDY") ? Find(Parts, "TBDY") : &Require(Parts, "TBOD");
            VarReader Bodies(Data, BodySec->Begin, BodySec->End);
            while (!Bodies.AtEnd()) {
                const uint32_t Index = Bodies.Next();
                if (Index == 0) break;
                if (Index >= Count) throw BinaryException("Havok type body index out of range");
                TypeInfo& T = Table->Types[Index];
                T.Parent    = Bodies.Next();
                T.Flags     = Bodies.Next();
                if (T.Flags & 0x01) T.Subtype = Bodies.Next();
                if (T.Flags & 0x02) T.Pointer = Bodies.Next();
                if (T.Flags & 0x04) T.Version = Bodies.Next();
                if (T.Flags & 0x08) {
                    T.Size      = Bodies.Next();
                    T.Alignment = Bodies.Next();
                }
                if (T.Flags & 0x10) T.Unk10 = Bodies.Next();
                if (T.Flags & 0x20) {
                    const uint32_t Members = Bodies.Next() & 0xFFFF;  // the high bits are flags
                    for (uint32_t M = 0; M < Members; ++M) {
                        Member Mem;
                        Mem.Name   = StringAt(FieldStrings, Bodies.Next(), "field");
                        Mem.Flags  = Bodies.Next();
                        Mem.Offset = Bodies.Next();
                        Mem.Type   = Bodies.Next();
                        T.Members.push_back(std::move(Mem));
                    }
                }
                if (T.Flags & 0x40) {
                    const uint32_t Interfaces = Bodies.Next();
                    for (uint32_t N = 0; N < Interfaces; ++N) {
                        const uint32_t Type = Bodies.Next();
                        T.Interfaces.emplace_back(Type, Bodies.Next());
                    }
                }
                if (T.Flags & ~0x7Fu) { throw BinaryException("Unknown Havok type flags on " + T.Name); }
            }

            if (const Section* HashSec = Find(Parts, "THSH")) {
                VarReader Hashes(Data, HashSec->Begin, HashSec->End);
                const uint32_t HashCount = Hashes.Next();
                for (uint32_t H = 0; H < HashCount; ++H) {
                    const uint32_t Index = Hashes.Next();
                    const uint32_t Hash  = Hashes.U32();
                    if (Index < Count) Table->Types[Index].Hash = Hash;
                }
            }
            return Table;
        }

        uint64_t ReadU64(std::span<const uint8_t> Data, size_t At) {
            uint64_t Value = 0;
            std::memcpy(&Value, Data.data() + At, 8);
            return Value;
        }

        bool HasMagic(std::span<const uint8_t> Data, const char* Magic) {
            return Data.size() >= 8 && std::memcmp(Data.data() + 4, Magic, 4) == 0;
        }
    }  // namespace

#pragma region TypeTable
    const TypeInfo& TypeTable::At(uint32_t Index) const {
        if (Index == 0 || Index >= Types.size()) throw BinaryException("Havok type index out of range");
        return Types[Index];
    }

    std::string TypeTable::FullName(uint32_t Index) const {
        const TypeInfo& T  = At(Index);
        std::string Result = T.Name;
        if (!T.Templates.empty()) {
            Result += '<';
            for (size_t I = 0; I < T.Templates.size(); ++I) {
                if (I) Result += ", ";
                const auto& [Name, Arg] = T.Templates[I];
                Result += !Name.empty() && Name[0] == 't' && Arg != 0 ? FullName(static_cast<uint32_t>(Arg))
                                                                      : std::to_string(Arg);
            }
            Result += '>';
        }
        return Result;
    }

    uint32_t TypeTable::Find(std::string_view Name) const {
        for (uint32_t I = 1; I < Types.size(); ++I) {
            if (Types[I].Name == Name) return I;
        }
        return 0;
    }

    bool TypeTable::IsA(uint32_t Index, uint32_t Base) const {
        for (int Depth = 0; Index != 0 && Depth < 64; ++Depth) {
            if (Index == Base) return true;
            Index = At(Index).Parent;
        }
        return false;
    }

    uint32_t TypeTable::SubtypeOf(uint32_t Index) const {
        for (int Depth = 0; Index != 0 && Depth < 64; ++Depth) {
            const TypeInfo& T = At(Index);
            if (T.Flags & 0x01) return T.Subtype;
            Index = T.Parent;
        }
        return 0;
    }

    Kind TypeTable::KindOf(uint32_t Index) const {
        return static_cast<Kind>(SubtypeOf(Index) & 0x1F);
    }

    uint32_t TypeTable::SizeOf(uint32_t Index) const {
        for (int Depth = 0; Index != 0 && Depth < 64; ++Depth) {
            const TypeInfo& T = At(Index);
            if (T.Flags & 0x08) return T.Size;
            Index = T.Parent;
        }
        return 0;
    }

    uint32_t TypeTable::PointerOf(uint32_t Index) const {
        for (int Depth = 0; Index != 0 && Depth < 64; ++Depth) {
            const TypeInfo& T = At(Index);
            if (T.Flags & 0x02) return T.Pointer;
            Index = T.Parent;
        }
        return 0;
    }

    const Member* TypeTable::FindMember(uint32_t Index, std::string_view Name) const {
        for (int Depth = 0; Index != 0 && Depth < 64; ++Depth) {
            const TypeInfo& T = At(Index);
            for (const Member& M : T.Members) {
                if (M.Name == Name) return &M;
            }
            Index = T.Parent;
        }
        return nullptr;
    }

    std::vector<const Member*> TypeTable::AllMembers(uint32_t Index) const {
        std::vector<uint32_t> Chain;
        for (int Depth = 0; Index != 0 && Depth < 64; ++Depth) {
            Chain.push_back(Index);
            Index = At(Index).Parent;
        }
        std::vector<const Member*> Result;
        for (auto It = Chain.rbegin(); It != Chain.rend(); ++It) {
            for (const Member& M : At(*It).Members)
                Result.push_back(&M);
        }
        return Result;
    }
#pragma endregion

#pragma region Compendium
    bool Compendium::Is(std::span<const uint8_t> Data) {
        return HasMagic(Data, "TCM0");
    }

    Compendium Compendium::Read(std::span<const uint8_t> Data) {
        const std::vector<Section> Top = ReadSections(Data, 0, Data.size());
        if (Top.size() != 1 || Top[0].Tag != "TCM0") throw BinaryException("Not a Havok compendium (TCM0)");
        const std::vector<Section> Parts = ReadSections(Data, Top[0].Begin, Top[0].End);
        Compendium Result;
        const Section& Ids = Require(Parts, "TCID");
        for (size_t At = Ids.Begin; At + 8 <= Ids.End; At += 8) {
            Result.IDs.push_back(ReadU64(Data, At));
        }
        Result.Types = ReadTypes(Data, Require(Parts, "TYPE"));
        return Result;
    }
#pragma endregion

#pragma region Tagfile
    bool Tagfile::Is(std::span<const uint8_t> Data) {
        return HasMagic(Data, "TAG0");
    }

    Tagfile Tagfile::Read(std::span<const uint8_t> Data, const Compendium* Types) {
        const std::vector<Section> Top = ReadSections(Data, 0, Data.size());
        if (Top.empty() || Top[0].Tag != "TAG0") throw BinaryException("Not a Havok tagfile (TAG0)");
        const std::vector<Section> Parts = ReadSections(Data, Top[0].Begin, Top[0].End);

        Tagfile Result;
        if (const Section* Sdk = Find(Parts, "SDKV")) {
            Result.SdkVersion.assign(reinterpret_cast<const char*>(Data.data()) + Sdk->Begin, Sdk->End - Sdk->Begin);
        }
        const Section& DataSec = Require(Parts, "DATA");
        Result.Data.assign(Data.begin() + static_cast<std::ptrdiff_t>(DataSec.Begin),
                           Data.begin() + static_cast<std::ptrdiff_t>(DataSec.End));

        if (const Section* TypeSec = Find(Parts, "TYPE")) {
            Result.Types = ReadTypes(Data, *TypeSec);
        } else if (const Section* Ref = Find(Parts, "TCRF")) {
            if (Ref->End - Ref->Begin < 8) throw BinaryException("Invalid Havok compendium reference");
            Result.CompendiumID = ReadU64(Data, Ref->Begin);
            if (!Types)
                throw BinaryException(
                  "This Havok tagfile keeps its types in a compendium; read the compendium and pass it in");
            if (std::find(Types->IDs.begin(), Types->IDs.end(), *Result.CompendiumID) == Types->IDs.end()) {
                throw BinaryException("The compendium given isn't the one this Havok tagfile uses");
            }
            Result.Types = Types->Types;
        } else {
            throw BinaryException("Havok tagfile has neither types nor a compendium reference");
        }

        const Section& Index                  = Require(Parts, "INDX");
        const std::vector<Section> IndexParts = ReadSections(Data, Index.Begin, Index.End);
        const Section& Items                  = Require(IndexParts, "ITEM");
        for (size_t At = Items.Begin; At + 12 <= Items.End; At += 12) {
            uint32_t Words[3];
            std::memcpy(Words, Data.data() + At, 12);
            Item I;
            I.Type   = Words[0] & 0xFFFFFF;
            I.Flags  = static_cast<uint8_t>(Words[0] >> 24);
            I.Offset = Words[1];
            I.Count  = Words[2];
            if (I.Type >= Result.Types->Types.size()) throw BinaryException("Havok item type out of range");
            Result.Items.push_back(I);
        }
        return Result;
    }

    Value Tagfile::ItemValue(size_t Index) const {
        if (Index == 0 || Index >= Items.size()) return {};
        return Value(this, Items[Index].Type, Items[Index].Offset);
    }

    Value Tagfile::Root() const {
        return ItemValue(1);
    }

    std::vector<Value> Tagfile::ObjectsOfType(std::string_view TypeName) const {
        std::vector<Value> Result;
        const uint32_t Base = Types->Find(TypeName);
        if (Base == 0) return Result;
        for (size_t I = 1; I < Items.size(); ++I) {
            if ((Items[I].Flags & 0x10) && Types->IsA(Items[I].Type, Base)) Result.push_back(ItemValue(I));
        }
        return Result;
    }
#pragma endregion

#pragma region Value
    std::string Value::TypeName() const {
        return IsValid() ? File->Types->FullName(Type) : std::string();
    }

    Kind Value::GetKind() const {
        return IsValid() ? File->Types->KindOf(Type) : Kind::Void;
    }

    const uint8_t* Value::Bytes(size_t Count) const {
        if (!IsValid()) throw BinaryException("Use of a null Havok value");
        if (Offset + Count > File->Data.size()) throw BinaryException("Havok value lies outside the data");
        return File->Data.data() + Offset;
    }

    bool Value::Has(std::string_view Name) const {
        return IsValid() && File->Types->FindMember(Type, Name) != nullptr;
    }

    Value Value::operator[](std::string_view Name) const {
        if (!IsValid()) throw BinaryException("Use of a null Havok value");
        const Member* M = File->Types->FindMember(Type, Name);
        if (!M) throw BinaryException("Havok type " + TypeName() + " has no member \"" + std::string(Name) + "\"");
        return Value(File, M->Type, Offset + M->Offset);
    }

    int64_t Value::AsInt() const {
        const uint32_t Size = File ? File->Types->SizeOf(Type) : 0;
        const bool Signed   = (File->Types->SubtypeOf(Type) & 0x200) != 0;
        const uint8_t* P    = Bytes(Size);
        switch (Size) {
            case 1:
                return Signed ? int64_t(int8_t(P[0])) : int64_t(P[0]);
            case 2: {
                uint16_t V;
                std::memcpy(&V, P, 2);
                return Signed ? int64_t(int16_t(V)) : int64_t(V);
            }
            case 4: {
                uint32_t V;
                std::memcpy(&V, P, 4);
                return Signed ? int64_t(int32_t(V)) : int64_t(V);
            }
            case 8: {
                int64_t V;
                std::memcpy(&V, P, 8);
                return V;
            }
            default:
                throw BinaryException("Havok integer of unexpected size in " + TypeName());
        }
    }

    bool Value::AsBool() const {
        return AsInt() != 0;
    }

    double Value::AsFloat() const {
        if (GetKind() == Kind::Int || GetKind() == Kind::Bool) return static_cast<double>(AsInt());
        const uint32_t Size = File->Types->SizeOf(Type);
        const uint8_t* P    = Bytes(Size);
        if (Size == 4) {
            float V;
            std::memcpy(&V, P, 4);
            return V;
        }
        if (Size == 8) {
            double V;
            std::memcpy(&V, P, 8);
            return V;
        }
        throw BinaryException("Havok float of unexpected size in " + TypeName());
    }

    uint64_t Value::ItemIndex() const {
        uint64_t Index = 0;
        std::memcpy(&Index, Bytes(8), 8);
        if (Index >= File->Items.size()) throw BinaryException("Havok item index out of range");
        return Index;
    }

    std::string Value::AsString() const {
        if (GetKind() != Kind::String) throw BinaryException("Havok value of type " + TypeName() + " isn't a string");
        const uint64_t Index = ItemIndex();
        if (Index == 0) return {};
        const Tagfile::Item& I = File->Items[Index];
        if (I.Offset + I.Count > File->Data.size()) throw BinaryException("Havok string lies outside the data");
        const char* Text = reinterpret_cast<const char*>(File->Data.data()) + I.Offset;
        return std::string(Text, strnlen(Text, I.Count));
    }

    Value Value::Deref() const {
        if (GetKind() != Kind::Pointer) throw BinaryException("Havok value of type " + TypeName() + " isn't a pointer");
        return File->ItemValue(ItemIndex());
    }

    size_t Value::Size() const {
        if (GetKind() != Kind::Array) throw BinaryException("Havok value of type " + TypeName() + " isn't an array");
        const uint32_t Sub = File->Types->SubtypeOf(Type);
        if (Sub & 0x20) return Sub >> 8;  // fixed size
        const uint64_t Index = ItemIndex();
        return Index == 0 ? 0 : File->Items[Index].Count;
    }

    Value Value::At(size_t Index) const {
        if (Index >= Size()) throw BinaryException("Havok array index out of range");
        const uint32_t Sub = File->Types->SubtypeOf(Type);
        if (Sub & 0x20) {
            const uint32_t Element = File->Types->PointerOf(Type);
            return Value(File, Element, Offset + Index * File->Types->SizeOf(Element));
        }
        const Tagfile::Item& I = File->Items[ItemIndex()];
        return Value(File, I.Type, I.Offset + Index * File->Types->SizeOf(I.Type));
    }

    std::vector<float> Value::AsFloats() const {
        std::vector<float> Result(Size());
        for (size_t I = 0; I < Result.size(); ++I)
            Result[I] = static_cast<float>(At(I).AsFloat());
        return Result;
    }

    std::vector<int64_t> Value::AsInts() const {
        std::vector<int64_t> Result(Size());
        for (size_t I = 0; I < Result.size(); ++I)
            Result[I] = At(I).AsInt();
        return Result;
    }

    std::span<const uint8_t> Value::AsBytes() const {
        const size_t Count = Size();
        if (Count == 0) return {};
        if (File->Types->SubtypeOf(Type) & 0x20) throw BinaryException("AsBytes is for hkArrays");
        const Tagfile::Item& I = File->Items[ItemIndex()];
        if (File->Types->SizeOf(I.Type) != 1) throw BinaryException("Havok array isn't of bytes");
        if (I.Offset + Count > File->Data.size()) throw BinaryException("Havok array lies outside the data");
        return {File->Data.data() + I.Offset, Count};
    }
#pragma endregion
}  // namespace Souls::Havok
