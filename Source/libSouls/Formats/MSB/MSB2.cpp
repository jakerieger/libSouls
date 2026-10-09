//
// Created by Jake Rieger on 10/8/2026.
//

#include "MSB2.hpp"

#include "MSBParamIO.hpp"

#include <algorithm>

namespace Souls {
    namespace {
        using Msb::Detail::ReambiguateName;
        using Msb::Detail::RequireNonZero;
        using Msb::Detail::Upcast;

        constexpr const char* PartsPoseParamName = "MAPSTUDIO_PARTS_POSE_ST";
        constexpr const char* BoneNameParamName   = "MAPSTUDIO_BONE_NAME_STRING";
        constexpr const char* RouteParamName      = "ROUTE_PARAM_ST";
        constexpr const char* LayerParamName      = "LAYER_PARAM_ST";

        struct BoneName : MSB2::Entry {};

        int64_t VarintSize(const BinaryReader& Reader) {
            return Reader.VarintLong ? 8 : 4;
        }
        int64_t VarintSize(const BinaryWriter& Writer) {
            return Writer.VarintLong ? 8 : 4;
        }

        // Param header whose layout depends on the file being 32- or 64-bit.
        struct Header {
            std::vector<int64_t> EntryOffsets;
            int64_t NextParamOffset = 0;
        };

        Header ReadHeader(BinaryReader& Reader, int32_t Version, const char* Type) {
            Reader.Assert<int32_t>(Version);
            int32_t OffsetCount;
            int64_t NameOffset;
            if (Reader.VarintLong) {
                OffsetCount = Reader.ReadInt32();
                NameOffset  = Reader.ReadInt64();
            } else {
                NameOffset  = Reader.ReadInt32();
                OffsetCount = Reader.ReadInt32();
            }
            if (OffsetCount < 1) {
                throw BinaryException("Invalid MSB param entry count");
            }
            Header Result;
            for (int32_t I = 0; I < OffsetCount - 1; ++I) {
                Result.EntryOffsets.push_back(Reader.ReadVarint());
            }
            Result.NextParamOffset = Reader.ReadVarint();
            const std::string Name = Reader.GetUTF16Text(NameOffset);
            if (Name != Type) {
                throw BinaryException(std::string("Expected param \"") + Type + "\", got param \"" + Name + "\"");
            }
            return Result;
        }

        void WriteHeader(BinaryWriter& Writer, int32_t Version, const char* Type, size_t Count) {
            Writer.WriteInt32(Version);
            if (Writer.VarintLong) {
                Writer.WriteInt32(static_cast<int32_t>(Count) + 1);
                Writer.ReserveVarint("ParamNameOffset");
            } else {
                Writer.ReserveVarint("ParamNameOffset");
                Writer.WriteInt32(static_cast<int32_t>(Count) + 1);
            }
            for (size_t I = 0; I < Count; ++I) {
                Writer.ReserveVarint("EntryOffset" + std::to_string(I));
            }
            Writer.ReserveVarint("NextParamOffset");
            Writer.FillVarint("ParamNameOffset", Writer.Position());
            Writer.WriteUTF16Text(Type, true);
            Writer.Align(VarintSize(Writer));
        }

        // Reads a typed param; the type of each entry is the byte TypeOffset bytes into it.
        template<typename ParamT>
        typename ParamT::FileOrder ReadParam(BinaryReader& Reader, ParamT& Param, int64_t TypeOffset) {
            const Header H = ReadHeader(Reader, ParamT::ParamVersion, ParamT::ParamName);
            typename ParamT::FileOrder Order;
            for (const int64_t Offset : H.EntryOffsets) {
                Reader.Seek(Offset);
                const uint32_t Type = Reader.ReadAt<uint8_t>(Reader.Position() + TypeOffset);
                if (!Param.ReadEntryOfType(Type, Reader, Order)) {
                    throw BinaryException("Unsupported MSB entry type: " + std::to_string(Type));
                }
            }
            Reader.Seek(H.NextParamOffset);
            return Order;
        }

        template<typename EntryT>
        void WriteParam(BinaryWriter& Writer, int32_t Version, const char* Type, const std::vector<EntryT*>& Entries) {
            WriteHeader(Writer, Version, Type, Entries.size());
            int32_t Id = 0;
            std::optional<uint32_t> CurrentType;
            for (size_t I = 0; I < Entries.size(); ++I) {
                if (!CurrentType || *CurrentType != Entries[I]->Type()) {
                    CurrentType = Entries[I]->Type();
                    Id          = 0;
                }
                Writer.FillVarint("EntryOffset" + std::to_string(I), Writer.Position());
                Entries[I]->Write(Writer, Id);
                Writer.Align(VarintSize(Writer));
                ++Id;
            }
        }
    }  // namespace

#pragma region Models
    void MSB2::Model::Read(BinaryReader& Reader) {
        const int64_t Start      = Reader.Position();
        const int64_t NameOffset = Reader.ReadVarint();
        Reader.Assert<uint8_t>(static_cast<uint8_t>(Type()));
        Reader.Assert<uint8_t>(0);
        Reader.ReadInt16();  // ID
        if (Reader.VarintLong) {
            Reader.Assert<int32_t>(0);
        }
        const int64_t TypeDataOffset = Reader.ReadVarint();
        Reader.AssertVarint(0);
        RequireNonZero(NameOffset, "nameOffset");
        if (HasTypeData() != (TypeDataOffset != 0)) {
            throw BinaryException("Unexpected typeDataOffset in model");
        }
        Reader.Seek(Start + NameOffset);
        Name = Reader.ReadUTF16Text();
        if (HasTypeData()) {
            Reader.Seek(Start + TypeDataOffset);
            ReadTypeData(Reader);
        }
    }

    void MSB2::Model::Write(BinaryWriter& Writer, int32_t Index) {
        const int64_t Start = Writer.Position();
        Writer.ReserveVarint("NameOffset");
        Writer.WriteByte(static_cast<uint8_t>(Type()));
        Writer.WriteByte(0);
        Writer.WriteInt16(static_cast<int16_t>(Index));
        if (Writer.VarintLong) {
            Writer.WriteInt32(0);
        }
        Writer.ReserveVarint("TypeDataOffset");
        Writer.WriteVarint(0);
        Writer.FillVarint("NameOffset", Writer.Position() - Start);
        Writer.WriteUTF16Text(ReambiguateName(Name), true);
        Writer.Align(VarintSize(Writer));
        if (HasTypeData()) {
            Writer.FillVarint("TypeDataOffset", Writer.Position() - Start);
            WriteTypeData(Writer);
        } else {
            Writer.FillVarint("TypeDataOffset", 0);
        }
    }
#pragma endregion

#pragma region Events
    void MSB2::Event::Read(BinaryReader& Reader) {
        const int64_t Start      = Reader.Position();
        const int64_t NameOffset = Reader.ReadVarint();
        EventID                  = Reader.ReadInt32();
        Reader.Assert<uint8_t>(static_cast<uint8_t>(Type()));
        Reader.Assert<uint8_t>(0);
        Reader.ReadInt16();  // ID
        const int64_t TypeDataOffset = Reader.ReadVarint();
        if (!Reader.VarintLong) {
            Reader.Assert<int32_t>(0);
            Reader.Assert<int32_t>(0);
        }
        RequireNonZero(NameOffset, "nameOffset");
        RequireNonZero(TypeDataOffset, "typeDataOffset");
        Reader.Seek(Start + NameOffset);
        Name = Reader.ReadUTF16Text();
        Reader.Seek(Start + TypeDataOffset);
        ReadTypeData(Reader);
    }

    void MSB2::Event::Write(BinaryWriter& Writer, int32_t Index) {
        const int64_t Start = Writer.Position();
        Writer.ReserveVarint("NameOffset");
        Writer.WriteInt32(EventID);
        Writer.WriteByte(static_cast<uint8_t>(Type()));
        Writer.WriteByte(0);
        Writer.WriteInt16(static_cast<int16_t>(Index));
        Writer.ReserveVarint("TypeDataOffset");
        if (!Writer.VarintLong) {
            Writer.WriteInt32(0);
            Writer.WriteInt32(0);
        }
        Writer.FillVarint("NameOffset", Writer.Position() - Start);
        Writer.WriteUTF16Text(Name, true);
        Writer.Align(VarintSize(Writer));
        Writer.FillVarint("TypeDataOffset", Writer.Position() - Start);
        WriteTypeData(Writer);
    }
#pragma endregion

#pragma region Regions
    void MSB2::Region::Read(BinaryReader& Reader) {
        const int64_t Start      = Reader.Position();
        const int64_t NameOffset = Reader.ReadVarint();
        Unk08                    = Reader.ReadInt16();
        Reader.Assert<uint8_t>(static_cast<uint8_t>(Type()));
        const auto ShapeKind = static_cast<Msb::ShapeType>(Reader.ReadByte());
        Reader.ReadInt16();  // ID
        Unk0E                         = Reader.ReadInt16();
        Position                      = Reader.ReadVector3();
        Rotation                      = Reader.ReadVector3();
        const int64_t UnkOffsetA      = Reader.ReadVarint();
        const int64_t UnkOffsetB      = Reader.ReadVarint();
        Reader.Assert<int32_t>(-1);
        Reader.AssertPattern(0x24, 0);
        const int64_t ShapeDataOffset = Reader.ReadVarint();
        const int64_t TypeDataOffset  = Reader.ReadVarint();
        Reader.Assert<int64_t>(0);
        Reader.Assert<int64_t>(0);
        if (!Reader.VarintLong) {
            Reader.Assert<int64_t>(0);
            Reader.Assert<int64_t>(0);
            Reader.Assert<int32_t>(0);
        }
        Shape = Msb::CreateShape(ShapeKind);
        if (std::holds_alternative<Msb::Shapes::Composite>(Shape)) {
            throw BinaryException("Dark Souls 2 does not support composite shapes.");
        }
        RequireNonZero(NameOffset, "nameOffset");
        RequireNonZero(UnkOffsetA, "unkOffsetA");
        RequireNonZero(UnkOffsetB, "unkOffsetB");
        if (Msb::HasShapeData(Shape) != (ShapeDataOffset != 0)) {
            throw BinaryException("Unexpected shapeDataOffset in region");
        }
        if (HasTypeData() != (TypeDataOffset != 0)) {
            throw BinaryException("Unexpected typeDataOffset in region");
        }

        Reader.Seek(Start + NameOffset);
        Name = Reader.ReadUTF16Text();
        Reader.Seek(Start + UnkOffsetA);
        Reader.Assert<int32_t>(0);
        Reader.Seek(Start + UnkOffsetB);
        Reader.Assert<int32_t>(0);
        if (Msb::HasShapeData(Shape)) {
            Reader.Seek(Start + ShapeDataOffset);
            Msb::Detail::ReadVisitor V{Reader};
            Msb::Detail::VisitShape(Shape, V);
        }
        if (HasTypeData()) {
            Reader.Seek(Start + TypeDataOffset);
            ReadTypeData(Reader);
        }
    }

    void MSB2::Region::Write(BinaryWriter& Writer, int32_t Index) {
        const int64_t Start = Writer.Position();
        Writer.ReserveVarint("NameOffset");
        Writer.WriteInt16(Unk08);
        Writer.WriteByte(static_cast<uint8_t>(Type()));
        Writer.WriteByte(static_cast<uint8_t>(Msb::TypeOf(Shape)));
        Writer.WriteInt16(static_cast<int16_t>(Index));
        Writer.WriteInt16(Unk0E);
        Writer.WriteVector3(Position);
        Writer.WriteVector3(Rotation);
        Writer.ReserveVarint("UnkOffsetA");
        Writer.ReserveVarint("UnkOffsetB");
        Writer.WriteInt32(-1);
        Writer.Pad(0x24);
        Writer.ReserveVarint("ShapeDataOffset");
        Writer.ReserveVarint("TypeDataOffset");
        Writer.WriteInt64(0);
        Writer.WriteInt64(0);
        if (!Writer.VarintLong) {
            Writer.WriteInt64(0);
            Writer.WriteInt64(0);
            Writer.WriteInt32(0);
        }
        Writer.FillVarint("NameOffset", Writer.Position() - Start);
        Writer.WriteUTF16Text(Name, true);
        Writer.Align(4);
        Writer.FillVarint("UnkOffsetA", Writer.Position() - Start);
        Writer.WriteInt32(0);
        Writer.FillVarint("UnkOffsetB", Writer.Position() - Start);
        Writer.WriteInt32(0);
        Writer.Align(VarintSize(Writer));
        if (Msb::HasShapeData(Shape)) {
            Writer.FillVarint("ShapeDataOffset", Writer.Position() - Start);
            Msb::Detail::WriteVisitor V{Writer};
            Msb::Detail::VisitShape(Shape, V);
        } else {
            Writer.FillVarint("ShapeDataOffset", 0);
        }
        if (HasTypeData()) {
            Writer.FillVarint("TypeDataOffset", Writer.Position() - Start);
            WriteTypeData(Writer);
        } else {
            Writer.FillVarint("TypeDataOffset", 0);
        }
    }
#pragma endregion

#pragma region Parts
    void MSB2::Part::Read(BinaryReader& Reader) {
        const int64_t Start      = Reader.Position();
        const int64_t NameOffset = Reader.ReadVarint();
        Reader.Assert<uint8_t>(static_cast<uint8_t>(Type()));
        Reader.Assert<uint8_t>(0);
        Reader.ReadInt16();  // ID
        ModelName.Index = Reader.ReadInt16();
        Reader.Assert<int16_t>(0);
        Position = Reader.ReadVector3();
        Rotation = Reader.ReadVector3();
        Scale    = Reader.ReadVector3();
        Reader.ReadInto(std::span<uint32_t>(DrawGroups));
        Unk44 = Reader.ReadInt32();
        Unk48 = Reader.ReadInt32();
        Unk4C = Reader.ReadInt32();
        Unk50 = Reader.ReadInt32();
        Reader.ReadInto(std::span<uint32_t>(DispGroups));
        Unk64 = Reader.ReadInt32();
        Reader.Assert<int32_t>(0);
        Unk6C = Reader.ReadByte();
        Reader.Assert<uint8_t>(0);
        Unk6E = Reader.ReadByte();
        Reader.Assert<uint8_t>(0);
        const int64_t TypeDataOffset = Reader.ReadVarint();
        if (Reader.VarintLong) {
            Reader.Assert<int64_t>(0);
        }
        RequireNonZero(NameOffset, "nameOffset");
        RequireNonZero(TypeDataOffset, "typeDataOffset");
        Reader.Seek(Start + NameOffset);
        Name = Reader.ReadUTF16Text();
        Reader.Seek(Start + TypeDataOffset);
        ReadTypeData(Reader);
    }

    void MSB2::Part::Write(BinaryWriter& Writer, int32_t Index) {
        const int64_t Start = Writer.Position();
        Writer.ReserveVarint("NameOffset");
        Writer.WriteByte(static_cast<uint8_t>(Type()));
        Writer.WriteByte(0);
        Writer.WriteInt16(static_cast<int16_t>(Index));
        Writer.WriteInt16(ModelName.Index);
        Writer.WriteInt16(0);
        Writer.WriteVector3(Position);
        Writer.WriteVector3(Rotation);
        Writer.WriteVector3(Scale);
        Writer.WriteArray(std::span<const uint32_t>(DrawGroups));
        Writer.WriteInt32(Unk44);
        Writer.WriteInt32(Unk48);
        Writer.WriteInt32(Unk4C);
        Writer.WriteInt32(Unk50);
        Writer.WriteArray(std::span<const uint32_t>(DispGroups));
        Writer.WriteInt32(Unk64);
        Writer.WriteInt32(0);
        Writer.WriteByte(Unk6C);
        Writer.WriteByte(0);
        Writer.WriteByte(Unk6E);
        Writer.WriteByte(0);
        Writer.ReserveVarint("TypeDataOffset");
        if (Writer.VarintLong) {
            Writer.WriteInt64(0);
        }
        const int64_t NameStart = Writer.Position();
        const int64_t NamePad   = Writer.VarintLong ? 0x20 : 0x2C;
        Writer.FillVarint("NameOffset", NameStart - Start);
        Writer.WriteUTF16Text(ReambiguateName(Name), true);
        if (Writer.Position() - NameStart < NamePad) {
            Writer.Pad(static_cast<size_t>(NamePad - (Writer.Position() - NameStart)));
        }
        Writer.Align(VarintSize(Writer));
        Writer.FillVarint("TypeDataOffset", Writer.Position() - Start);
        WriteTypeData(Writer);
    }

    void MSB2::Part::GetNames(const Entries& Lists) {
        Msb::Detail::NameVisitor<Entries> V{Lists};
        V(ModelName);
    }

    void MSB2::Part::GetIndices(const Entries& Lists) {
        Msb::Detail::IndexVisitor<Entries> V{Lists};
        V(ModelName);
    }
#pragma endregion

    bool MSB2::IsImpl(BinaryReader& Reader) {
        if (Reader.Length() < 0x14) {
            return false;
        }
        Reader.Order = Endian::Little;
        if (Reader.GetASCII(0, 4) == "MSB ") {
            return Reader.ReadAt<int32_t>(0x10) == 5;
        }
        // The 32-bit formats have no header; they start with the model param, in either byte order.
        for (const Endian Order : {Endian::Little, Endian::Big}) {
            Reader.Order = Order;
            if (Reader.ReadAt<uint32_t>(0) != 5) {
                continue;
            }
            const int64_t NameOffset = Reader.ReadAt<uint32_t>(4);
            if (NameOffset >= 0 && NameOffset + 30 <= Reader.Length() && Reader.GetUTF16Text(NameOffset) == "MODEL_PARAM_ST") {
                return true;
            }
        }
        return false;
    }

    void MSB2::ReadImpl(BinaryReader& Reader) {
        Reader.Order      = Endian::Little;
        Reader.VarintLong = false;
        if (Reader.GetASCII(0, 4) == "MSB ") {
            Format            = MSBFormat::DarkSouls2Scholar;
            Reader.VarintLong = true;
            Msb::Detail::AssertHeader(Reader);
        } else if (Reader.ReadAt<uint32_t>(0) == 5) {
            Format = MSBFormat::DarkSouls2LE;
        } else {
            Format       = MSBFormat::DarkSouls2BE;
            Reader.Order = Endian::Big;
        }

        Models     = {};
        Events     = {};
        Regions    = {};
        Parts      = {};
        PartPoses.clear();

        const int64_t VS         = VarintSize(Reader);
        const auto ModelOrder  = ReadParam(Reader, Models, VS);
        const auto EventOrder  = ReadParam(Reader, Events, VS + 4);
        const auto RegionOrder = ReadParam(Reader, Regions, VS + 2);

        // Route and layer params are always empty.
        for (const char* Param : {RouteParamName, LayerParamName}) {
            const Header H = ReadHeader(Reader, 5, Param);
            if (!H.EntryOffsets.empty()) {
                throw BinaryException(std::string(Param) + " should always be empty in Dark Souls 2.");
            }
            Reader.Seek(H.NextParamOffset);
        }

        const auto PartOrder = ReadParam(Reader, Parts, VS);

        std::vector<PartPose> Poses;
        {
            const Header H = ReadHeader(Reader, 0, PartsPoseParamName);
            for (const int64_t Offset : H.EntryOffsets) {
                Reader.Seek(Offset);
                const int64_t Start = Reader.Position();
                PartPose P;
                P.PartName.Index        = Reader.ReadInt16();
                const int16_t BoneCount = Reader.ReadInt16();
                if (Reader.VarintLong) {
                    Reader.Assert<int32_t>(0);
                }
                const int64_t BonesOffset = Reader.ReadVarint();
                Reader.Seek(Start + BonesOffset);
                for (int16_t I = 0; I < BoneCount; ++I) {
                    PartPose::Bone B;
                    B.NameIndex   = Reader.ReadInt32();
                    B.Translation = Reader.ReadVector3();
                    B.Rotation    = Reader.ReadVector3();
                    B.Scale       = Reader.ReadVector3();
                    P.Bones.push_back(std::move(B));
                }
                Poses.push_back(std::move(P));
            }
            Reader.Seek(H.NextParamOffset);
        }

        std::vector<BoneName> BoneNames;
        {
            const Header H = ReadHeader(Reader, 0, BoneNameParamName);
            for (const int64_t Offset : H.EntryOffsets) {
                Reader.Seek(Offset);
                BoneName B;
                B.Name = Reader.ReadUTF16Text();
                BoneNames.push_back(std::move(B));
            }
            Reader.Seek(H.NextParamOffset);
        }
        if (Reader.Position() != 0) {
            throw BinaryException("The next param offset of the final param should be 0, but it was not.");
        }

        Entries Lists;
        const auto ModelPtrs  = Models.EntriesInFileOrder(ModelOrder);
        const auto EventPtrs  = Events.EntriesInFileOrder(EventOrder);
        const auto RegionPtrs = Regions.EntriesInFileOrder(RegionOrder);
        const auto PartPtrs   = Parts.EntriesInFileOrder(PartOrder);
        Lists.Models          = Upcast<Entry>(ModelPtrs);
        Lists.Events          = Upcast<Entry>(EventPtrs);
        Lists.Regions         = Upcast<Entry>(RegionPtrs);
        Lists.Parts           = Upcast<Entry>(PartPtrs);
        for (PartsParam::Collision& C : Parts.Collisions) {
            Lists.Collisions.push_back(&C);
        }
        for (BoneName& B : BoneNames) {
            Lists.BoneNames.push_back(&B);
        }

        Msb::Detail::DisambiguateNames(Lists.Models);
        Msb::Detail::DisambiguateNames(Lists.Parts);
        Msb::Detail::DisambiguateNames(Lists.BoneNames);
        for (Part* P : PartPtrs) {
            P->GetNames(Lists);
        }
        for (PartPose& P : Poses) {
            Msb::Detail::NameVisitor<Entries> V{Lists};
            V(P.PartName);
            for (PartPose::Bone& B : P.Bones) {
                B.Name = Msb::Detail::FindName(Lists.BoneNames, B.NameIndex).value_or("");
            }
        }
        PartPoses = std::move(Poses);
    }

    void MSB2::WriteImpl(BinaryWriter& Writer) {
        const std::vector<Model*> ModelPtrs   = Models.GetEntries();
        const std::vector<Event*> EventPtrs   = Events.GetEntries();
        const std::vector<Region*> RegionPtrs = Regions.GetEntries();
        const std::vector<Part*> PartPtrs     = Parts.GetEntries();

        Entries Lists;
        Lists.Models  = Upcast<Entry>(ModelPtrs);
        Lists.Events  = Upcast<Entry>(EventPtrs);
        Lists.Regions = Upcast<Entry>(RegionPtrs);
        Lists.Parts   = Upcast<Entry>(PartPtrs);
        for (PartsParam::Collision& C : Parts.Collisions) {
            Lists.Collisions.push_back(&C);
        }

        for (Part* P : PartPtrs) {
            P->GetIndices(Lists);
        }

        // Bone names are collected from the poses as they are written.
        std::vector<BoneName> BoneNames;
        for (PartPose& P : PartPoses) {
            Msb::Detail::IndexVisitor<Entries> V{Lists};
            V(P.PartName);
            for (PartPose::Bone& B : P.Bones) {
                auto Found = std::find_if(BoneNames.begin(), BoneNames.end(), [&](const BoneName& N) { return N.Name == B.Name; });
                if (Found == BoneNames.end()) {
                    BoneName N;
                    N.Name = B.Name;
                    BoneNames.push_back(std::move(N));
                    Found = BoneNames.end() - 1;
                }
                B.NameIndex = static_cast<int32_t>(Found - BoneNames.begin());
            }
        }

        Writer.Order      = Format == MSBFormat::DarkSouls2BE ? Endian::Big : Endian::Little;
        Writer.VarintLong = Format == MSBFormat::DarkSouls2Scholar;
        if (Format == MSBFormat::DarkSouls2Scholar) {
            Msb::Detail::WriteHeader(Writer);
        }
        WriteParam(Writer, ModelParam::ParamVersion, ModelParam::ParamName, ModelPtrs);
        Writer.FillVarint("NextParamOffset", Writer.Position());
        WriteParam(Writer, EventParam::ParamVersion, EventParam::ParamName, EventPtrs);
        Writer.FillVarint("NextParamOffset", Writer.Position());
        WriteParam(Writer, PointParam::ParamVersion, PointParam::ParamName, RegionPtrs);
        Writer.FillVarint("NextParamOffset", Writer.Position());
        WriteHeader(Writer, 5, RouteParamName, 0);
        Writer.FillVarint("NextParamOffset", Writer.Position());
        WriteHeader(Writer, 5, LayerParamName, 0);
        Writer.FillVarint("NextParamOffset", Writer.Position());
        WriteParam(Writer, PartsParam::ParamVersion, PartsParam::ParamName, PartPtrs);
        Writer.FillVarint("NextParamOffset", Writer.Position());

        WriteHeader(Writer, 0, PartsPoseParamName, PartPoses.size());
        for (size_t I = 0; I < PartPoses.size(); ++I) {
            const PartPose& P   = PartPoses[I];
            const int64_t Start = Writer.Position();
            Writer.FillVarint("EntryOffset" + std::to_string(I), Start);
            Writer.WriteInt16(P.PartName.Index);
            Writer.WriteInt16(static_cast<int16_t>(P.Bones.size()));
            if (Writer.VarintLong) {
                Writer.WriteInt32(0);
            }
            Writer.ReserveVarint("BonesOffset");
            Writer.FillVarint("BonesOffset", Writer.Position() - Start);
            for (const PartPose::Bone& B : P.Bones) {
                Writer.WriteInt32(B.NameIndex);
                Writer.WriteVector3(B.Translation);
                Writer.WriteVector3(B.Rotation);
                Writer.WriteVector3(B.Scale);
            }
            Writer.Align(VarintSize(Writer));
        }
        Writer.FillVarint("NextParamOffset", Writer.Position());

        WriteHeader(Writer, 0, BoneNameParamName, BoneNames.size());
        for (size_t I = 0; I < BoneNames.size(); ++I) {
            Writer.FillVarint("EntryOffset" + std::to_string(I), Writer.Position());
            Writer.WriteUTF16Text(ReambiguateName(BoneNames[I].Name), true);
            Writer.Align(VarintSize(Writer));
        }
        Writer.FillVarint("NextParamOffset", 0);
    }
}  // namespace Souls
