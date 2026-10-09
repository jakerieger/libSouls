//
// Created by Jake Rieger on 10/8/2026.
//

#include "MSBS.hpp"

#include "MSBParamIO.hpp"

#include <algorithm>

namespace Souls {
    namespace {
        using Msb::Detail::ReadParamHeader;
        using Msb::Detail::ReambiguateName;
        using Msb::Detail::RequireNonZero;
        using Msb::Detail::Upcast;
    }  // namespace

#pragma region Models
    void MSBS::Model::Read(BinaryReader& Reader) {
        const int64_t Start      = Reader.Position();
        const int64_t NameOffset = Reader.ReadInt64();
        Reader.Assert<uint32_t>(Type());
        Reader.ReadInt32();  // ID
        const int64_t SibOffset = Reader.ReadInt64();
        InstanceCount           = Reader.ReadInt32();
        Unk1C                   = Reader.ReadInt32();
        const int64_t TypeDataOffset = Reader.ReadInt64();
        RequireNonZero(NameOffset, "nameOffset");
        RequireNonZero(SibOffset, "sibOffset");
        if (HasTypeData() != (TypeDataOffset != 0)) {
            throw BinaryException("Unexpected typeDataOffset in model");
        }
        Reader.Seek(Start + NameOffset);
        Name = Reader.ReadUTF16Text();
        Reader.Seek(Start + SibOffset);
        SibPath = Reader.ReadUTF16Text();
        if (HasTypeData()) {
            Reader.Seek(Start + TypeDataOffset);
            ReadTypeData(Reader);
        }
    }

    void MSBS::Model::Write(BinaryWriter& Writer, int32_t ID) {
        const int64_t Start = Writer.Position();
        Writer.Reserve<int64_t>("NameOffset");
        Writer.WriteUInt32(Type());
        Writer.WriteInt32(ID);
        Writer.Reserve<int64_t>("SibOffset");
        Writer.WriteInt32(InstanceCount);
        Writer.WriteInt32(Unk1C);
        Writer.Reserve<int64_t>("TypeDataOffset");
        Writer.Fill<int64_t>("NameOffset", Writer.Position() - Start);
        Writer.WriteUTF16Text(ReambiguateName(Name), true);
        Writer.Fill<int64_t>("SibOffset", Writer.Position() - Start);
        Writer.WriteUTF16Text(SibPath, true);
        Writer.Align(8);
        if (HasTypeData()) {
            Writer.Fill<int64_t>("TypeDataOffset", Writer.Position() - Start);
            WriteTypeData(Writer);
        } else {
            Writer.Fill<int64_t>("TypeDataOffset", 0);
        }
    }
#pragma endregion

#pragma region Events
    void MSBS::Event::Read(BinaryReader& Reader) {
        const int64_t Start      = Reader.Position();
        const int64_t NameOffset = Reader.ReadInt64();
        EventID                  = Reader.ReadInt32();
        Reader.Assert<uint32_t>(Type());
        Reader.ReadInt32();  // ID
        Reader.Assert<int32_t>(0);
        const int64_t BaseDataOffset = Reader.ReadInt64();
        const int64_t TypeDataOffset = Reader.ReadInt64();
        RequireNonZero(NameOffset, "nameOffset");
        RequireNonZero(BaseDataOffset, "baseDataOffset");
        if (HasTypeData() != (TypeDataOffset != 0)) {
            throw BinaryException("Unexpected typeDataOffset in event");
        }
        Reader.Seek(Start + NameOffset);
        Name = Reader.ReadUTF16Text();
        Reader.Seek(Start + BaseDataOffset);
        PartName.Index   = Reader.ReadInt32();
        RegionName.Index = Reader.ReadInt32();
        EntityID         = Reader.ReadInt32();
        Reader.Assert<int32_t>(0);
        if (HasTypeData()) {
            Reader.Seek(Start + TypeDataOffset);
            ReadTypeData(Reader);
        }
    }

    void MSBS::Event::Write(BinaryWriter& Writer, int32_t ID) {
        const int64_t Start = Writer.Position();
        Writer.Reserve<int64_t>("NameOffset");
        Writer.WriteInt32(EventID);
        Writer.WriteUInt32(Type());
        Writer.WriteInt32(ID);
        Writer.WriteInt32(0);
        Writer.Reserve<int64_t>("BaseDataOffset");
        Writer.Reserve<int64_t>("TypeDataOffset");
        Writer.Fill<int64_t>("NameOffset", Writer.Position() - Start);
        Writer.WriteUTF16Text(Name, true);
        Writer.Align(8);
        Writer.Fill<int64_t>("BaseDataOffset", Writer.Position() - Start);
        Writer.WriteInt32(PartName.Index);
        Writer.WriteInt32(RegionName.Index);
        Writer.WriteInt32(EntityID);
        Writer.WriteInt32(0);
        if (HasTypeData()) {
            Writer.Fill<int64_t>("TypeDataOffset", Writer.Position() - Start);
            WriteTypeData(Writer);
        } else {
            Writer.Fill<int64_t>("TypeDataOffset", 0);
        }
    }

    void MSBS::Event::GetNames(const Entries& Lists) {
        Msb::Detail::NameVisitor<Entries> V{Lists};
        V(PartName);
        V(RegionName);
    }

    void MSBS::Event::GetIndices(const Entries& Lists) {
        Msb::Detail::IndexVisitor<Entries> V{Lists};
        V(PartName);
        V(RegionName);
    }
#pragma endregion

#pragma region Regions
    void MSBS::Region::Read(BinaryReader& Reader) {
        const int64_t Start      = Reader.Position();
        const int64_t NameOffset = Reader.ReadInt64();
        Reader.Assert<uint32_t>(Type());
        Reader.ReadInt32();  // ID
        const auto ShapeKind = static_cast<Msb::ShapeType>(Reader.ReadUInt32());
        Position             = Reader.ReadVector3();
        Rotation             = Reader.ReadVector3();
        Unk2C                = Reader.ReadInt32();
        const int64_t BaseDataOffset1 = Reader.ReadInt64();
        const int64_t BaseDataOffset2 = Reader.ReadInt64();
        Reader.Assert<int32_t>(-1);
        MapStudioLayer                = Reader.ReadUInt32();
        const int64_t ShapeDataOffset = Reader.ReadInt64();
        const int64_t BaseDataOffset3 = Reader.ReadInt64();
        const int64_t TypeDataOffset  = Reader.ReadInt64();
        Shape                         = Msb::CreateShape(ShapeKind);
        RequireNonZero(NameOffset, "nameOffset");
        RequireNonZero(BaseDataOffset1, "baseDataOffset1");
        RequireNonZero(BaseDataOffset2, "baseDataOffset2");
        if (Msb::HasShapeData(Shape) != (ShapeDataOffset != 0)) {
            throw BinaryException("Unexpected shapeDataOffset in region");
        }
        RequireNonZero(BaseDataOffset3, "baseDataOffset3");
        if (HasTypeData() != (TypeDataOffset != 0)) {
            throw BinaryException("Unexpected typeDataOffset in region");
        }

        Reader.Seek(Start + NameOffset);
        Name = Reader.ReadUTF16Text();
        Reader.Seek(Start + BaseDataOffset1);
        const int16_t CountA = Reader.ReadInt16();
        UnkA                 = Reader.ReadArray<int16_t>(static_cast<size_t>(CountA));
        Reader.Seek(Start + BaseDataOffset2);
        const int16_t CountB = Reader.ReadInt16();
        UnkB                 = Reader.ReadArray<int16_t>(static_cast<size_t>(CountB));
        if (Msb::HasShapeData(Shape)) {
            Reader.Seek(Start + ShapeDataOffset);
            Msb::Detail::ReadVisitor V{Reader};
            Msb::Detail::VisitShape(Shape, V);
        }
        Reader.Seek(Start + BaseDataOffset3);
        ActivationPartName.Index = Reader.ReadInt32();
        EntityID                 = Reader.ReadInt32();
        if (HasTypeData()) {
            Reader.Seek(Start + TypeDataOffset);
            ReadTypeData(Reader);
        }
    }

    void MSBS::Region::Write(BinaryWriter& Writer, int32_t ID) {
        const int64_t Start = Writer.Position();
        Writer.Reserve<int64_t>("NameOffset");
        Writer.WriteUInt32(Type());
        Writer.WriteInt32(ID);
        Writer.WriteUInt32(static_cast<uint32_t>(Msb::TypeOf(Shape)));
        Writer.WriteVector3(Position);
        Writer.WriteVector3(Rotation);
        Writer.WriteInt32(Unk2C);
        Writer.Reserve<int64_t>("BaseDataOffset1");
        Writer.Reserve<int64_t>("BaseDataOffset2");
        Writer.WriteInt32(-1);
        Writer.WriteUInt32(MapStudioLayer);
        Writer.Reserve<int64_t>("ShapeDataOffset");
        Writer.Reserve<int64_t>("BaseDataOffset3");
        Writer.Reserve<int64_t>("TypeDataOffset");

        Writer.Fill<int64_t>("NameOffset", Writer.Position() - Start);
        Writer.WriteUTF16Text(ReambiguateName(Name), true);
        Writer.Align(4);
        Writer.Fill<int64_t>("BaseDataOffset1", Writer.Position() - Start);
        Writer.WriteInt16(static_cast<int16_t>(UnkA.size()));
        Writer.WriteArray(UnkA);
        Writer.Align(4);
        Writer.Fill<int64_t>("BaseDataOffset2", Writer.Position() - Start);
        Writer.WriteInt16(static_cast<int16_t>(UnkB.size()));
        Writer.WriteArray(UnkB);
        Writer.Align(8);
        if (Msb::HasShapeData(Shape)) {
            Writer.Fill<int64_t>("ShapeDataOffset", Writer.Position() - Start);
            Msb::Detail::WriteVisitor V{Writer};
            Msb::Detail::VisitShape(Shape, V);
        } else {
            Writer.Fill<int64_t>("ShapeDataOffset", 0);
        }
        Writer.Fill<int64_t>("BaseDataOffset3", Writer.Position() - Start);
        Writer.WriteInt32(ActivationPartName.Index);
        Writer.WriteInt32(EntityID);
        if (HasTypeData()) {
            const uint32_t T = Type();
            // Sound space overrides, parts group areas and auto draw group points align their type data.
            if (T == 23 || T == 25 || T == 26) {
                Writer.Align(8);
            }
            Writer.Fill<int64_t>("TypeDataOffset", Writer.Position() - Start);
            WriteTypeData(Writer);
        } else {
            Writer.Fill<int64_t>("TypeDataOffset", 0);
        }
        Writer.Align(8);
    }

    void MSBS::Region::GetNames(const Entries& Lists) {
        Msb::Detail::NameVisitor<Entries> V{Lists};
        V(ActivationPartName);
        Msb::Detail::VisitShape(Shape, V);
    }

    void MSBS::Region::GetIndices(const Entries& Lists) {
        Msb::Detail::IndexVisitor<Entries> V{Lists};
        V(ActivationPartName);
        Msb::Detail::VisitShape(Shape, V);
    }
#pragma endregion

#pragma region Routes
    void MSBS::Route::Read(BinaryReader& Reader) {
        const int64_t Start      = Reader.Position();
        const int64_t NameOffset = Reader.ReadInt64();
        Unk08                    = Reader.ReadInt32();
        Unk0C                    = Reader.ReadInt32();
        Reader.Assert<uint32_t>(Type());
        Reader.ReadInt32();  // ID
        Reader.AssertPattern(0x68, 0);
        RequireNonZero(NameOffset, "nameOffset");
        Reader.Seek(Start + NameOffset);
        Name = Reader.ReadUTF16Text();
    }

    void MSBS::Route::Write(BinaryWriter& Writer, int32_t ID) {
        const int64_t Start = Writer.Position();
        Writer.Reserve<int64_t>("NameOffset");
        Writer.WriteInt32(Unk08);
        Writer.WriteInt32(Unk0C);
        Writer.WriteUInt32(Type());
        Writer.WriteInt32(ID);
        Writer.Pad(0x68);
        Writer.Fill<int64_t>("NameOffset", Writer.Position() - Start);
        Writer.WriteUTF16Text(Name, true);
        Writer.Align(8);
    }
#pragma endregion

#pragma region Parts
    void MSBS::Part::Read(BinaryReader& Reader) {
        const int64_t Start      = Reader.Position();
        const int64_t NameOffset = Reader.ReadInt64();
        Reader.Assert<uint32_t>(Type());
        Reader.ReadInt32();  // ID
        ModelName.Index = Reader.ReadInt32();
        Reader.Assert<int32_t>(0);
        const int64_t SibOffset = Reader.ReadInt64();
        Position                = Reader.ReadVector3();
        Rotation                = Reader.ReadVector3();
        Scale                   = Reader.ReadVector3();
        Reader.Assert<int32_t>(-1);
        Reader.Assert<int32_t>(-1);
        Reader.Assert<int32_t>(0);
        const int64_t UnkOffset1       = Reader.ReadInt64();
        const int64_t UnkOffset2       = Reader.ReadInt64();
        const int64_t EntityDataOffset = Reader.ReadInt64();
        const int64_t TypeDataOffset   = Reader.ReadInt64();
        const int64_t GparamOffset     = Reader.ReadInt64();
        const int64_t UnkOffset6       = Reader.ReadInt64();
        const int64_t UnkOffset7       = Reader.ReadInt64();
        Reader.Assert<int64_t>(0);
        Reader.Assert<int64_t>(0);
        Reader.Assert<int64_t>(0);

        const unsigned Has = Blocks();
        RequireNonZero(NameOffset, "nameOffset");
        RequireNonZero(SibOffset, "sibOffset");
        if (((Has & HasUnk1) != 0) != (UnkOffset1 != 0)) throw BinaryException("Unexpected unkOffset1 in part");
        if (((Has & HasUnk2) != 0) != (UnkOffset2 != 0)) throw BinaryException("Unexpected unkOffset2 in part");
        RequireNonZero(EntityDataOffset, "entityDataOffset");
        RequireNonZero(TypeDataOffset, "typeDataOffset");
        if (((Has & HasGparam) != 0) != (GparamOffset != 0)) throw BinaryException("Unexpected gparamOffset in part");
        if (((Has & HasScene) != 0) != (UnkOffset6 != 0)) throw BinaryException("Unexpected unkOffset6 in part");
        if (((Has & HasUnk7) != 0) != (UnkOffset7 != 0)) throw BinaryException("Unexpected unkOffset7 in part");

        Reader.Seek(Start + NameOffset);
        Name = Reader.ReadUTF16Text();
        Reader.Seek(Start + SibOffset);
        SibPath = Reader.ReadUTF16Text();
        if (Has & HasUnk1) {
            Reader.Seek(Start + UnkOffset1);
            ReadBlock(HasUnk1, Reader);
        }
        if (Has & HasUnk2) {
            Reader.Seek(Start + UnkOffset2);
            ReadBlock(HasUnk2, Reader);
        }

        Reader.Seek(Start + EntityDataOffset);
        EntityID = Reader.ReadInt32();
        UnkE04   = Reader.ReadByte();
        UnkE05   = Reader.ReadByte();
        UnkE06   = Reader.ReadByte();
        LanternID = Reader.ReadByte();
        LodParamID = Reader.ReadByte();
        UnkE09     = Reader.ReadByte();
        IsPointLightShadowSrc = Reader.ReadBool();
        UnkE0B                = Reader.ReadByte();
        IsShadowSrc           = Reader.ReadBool();
        IsStaticShadowSrc     = Reader.ReadByte();
        IsCascade3ShadowSrc   = Reader.ReadByte();
        UnkE0F                = Reader.ReadByte();
        UnkE10                = Reader.ReadByte();
        IsShadowDest          = Reader.ReadBool();
        IsShadowOnly          = Reader.ReadBool();
        DrawByReflectCam      = Reader.ReadBool();
        DrawOnlyReflectCam    = Reader.ReadBool();
        EnableOnAboveShadow   = Reader.ReadByte();
        DisablePointLightEffect = Reader.ReadBool();
        UnkE17                  = Reader.ReadByte();
        UnkE18                  = Reader.ReadInt32();
        Reader.ReadInto(std::span<int32_t>(EntityGroupIDs));
        UnkE3C = Reader.ReadInt32();
        UnkE40 = Reader.ReadInt32();
        Reader.AssertPattern(0x10, 0);

        Reader.Seek(Start + TypeDataOffset);
        ReadTypeData(Reader);
        if (Has & HasGparam) {
            Reader.Seek(Start + GparamOffset);
            ReadBlock(HasGparam, Reader);
        }
        if (Has & HasScene) {
            Reader.Seek(Start + UnkOffset6);
            ReadBlock(HasScene, Reader);
        }
        if (Has & HasUnk7) {
            Reader.Seek(Start + UnkOffset7);
            ReadBlock(HasUnk7, Reader);
        }
    }

    void MSBS::Part::Write(BinaryWriter& Writer, int32_t ID) {
        const int64_t Start = Writer.Position();
        Writer.Reserve<int64_t>("NameOffset");
        Writer.WriteUInt32(Type());
        Writer.WriteInt32(ID);
        Writer.WriteInt32(ModelName.Index);
        Writer.WriteInt32(0);
        Writer.Reserve<int64_t>("SibOffset");
        Writer.WriteVector3(Position);
        Writer.WriteVector3(Rotation);
        Writer.WriteVector3(Scale);
        Writer.WriteInt32(-1);
        Writer.WriteInt32(-1);
        Writer.WriteInt32(0);
        Writer.Reserve<int64_t>("UnkOffset1");
        Writer.Reserve<int64_t>("UnkOffset2");
        Writer.Reserve<int64_t>("EntityDataOffset");
        Writer.Reserve<int64_t>("TypeDataOffset");
        Writer.Reserve<int64_t>("GparamOffset");
        Writer.Reserve<int64_t>("SceneGparamOffset");
        Writer.Reserve<int64_t>("UnkOffset7");
        Writer.WriteInt64(0);
        Writer.WriteInt64(0);
        Writer.WriteInt64(0);

        const unsigned Has = Blocks();
        Writer.Fill<int64_t>("NameOffset", Writer.Position() - Start);
        Writer.WriteUTF16Text(ReambiguateName(Name), true);
        Writer.Fill<int64_t>("SibOffset", Writer.Position() - Start);
        Writer.WriteUTF16Text(SibPath, true);
        Writer.Align(8);

        const auto OptionalBlock = [&](unsigned Bit, const char* OffsetName) {
            if (Has & Bit) {
                Writer.Fill<int64_t>(OffsetName, Writer.Position() - Start);
                WriteBlock(Bit, Writer);
            } else {
                Writer.Fill<int64_t>(OffsetName, 0);
            }
        };
        OptionalBlock(HasUnk1, "UnkOffset1");
        OptionalBlock(HasUnk2, "UnkOffset2");

        Writer.Fill<int64_t>("EntityDataOffset", Writer.Position() - Start);
        Writer.WriteInt32(EntityID);
        Writer.WriteByte(UnkE04);
        Writer.WriteByte(UnkE05);
        Writer.WriteByte(UnkE06);
        Writer.WriteByte(LanternID);
        Writer.WriteByte(LodParamID);
        Writer.WriteByte(UnkE09);
        Writer.WriteBool(IsPointLightShadowSrc);
        Writer.WriteByte(UnkE0B);
        Writer.WriteBool(IsShadowSrc);
        Writer.WriteByte(IsStaticShadowSrc);
        Writer.WriteByte(IsCascade3ShadowSrc);
        Writer.WriteByte(UnkE0F);
        Writer.WriteByte(UnkE10);
        Writer.WriteBool(IsShadowDest);
        Writer.WriteBool(IsShadowOnly);
        Writer.WriteBool(DrawByReflectCam);
        Writer.WriteBool(DrawOnlyReflectCam);
        Writer.WriteByte(EnableOnAboveShadow);
        Writer.WriteBool(DisablePointLightEffect);
        Writer.WriteByte(UnkE17);
        Writer.WriteInt32(UnkE18);
        Writer.WriteArray(std::span<const int32_t>(EntityGroupIDs));
        Writer.WriteInt32(UnkE3C);
        Writer.WriteInt32(UnkE40);
        Writer.Pad(0x10);
        Writer.Align(8);

        Writer.Fill<int64_t>("TypeDataOffset", Writer.Position() - Start);
        WriteTypeData(Writer);
        OptionalBlock(HasGparam, "GparamOffset");
        OptionalBlock(HasScene, "SceneGparamOffset");
        OptionalBlock(HasUnk7, "UnkOffset7");
    }

    void MSBS::Part::GetNames(const Entries& Lists) {
        Msb::Detail::NameVisitor<Entries> V{Lists};
        V(ModelName);
    }

    void MSBS::Part::GetIndices(const Entries& Lists) {
        Msb::Detail::IndexVisitor<Entries> V{Lists};
        V(ModelName);
    }
#pragma endregion

    bool MSBS::IsImpl(BinaryReader& Reader) {
        if (Reader.Length() < 4) {
            return false;
        }
        return Reader.GetASCII(0, 4) == "MSB ";
    }

    void MSBS::ReadImpl(BinaryReader& Reader) {
        Reader.Order = Endian::Little;
        Msb::Detail::AssertHeader(Reader);

        Models  = {};
        Events  = {};
        Regions = {};
        Routes  = {};
        Parts   = {};

        const auto ModelOrder  = Msb::Detail::ReadTypedParam(Reader, Models, 8, std::nullopt, &Models.Version);
        const auto EventOrder  = Msb::Detail::ReadTypedParam(Reader, Events, 0xC, std::nullopt, &Events.Version);
        const auto RegionOrder = Msb::Detail::ReadTypedParam(Reader, Regions, 8, std::nullopt, &Regions.Version);
        Msb::Detail::ReadTypedParam(Reader, Routes, 0x10, std::nullopt, &Routes.Version);

        const auto ReadEmpty = [&](EmptyParam& Param, const char* Name) {
            const Msb::Detail::ParamHeader Header = ReadParamHeader(Reader, std::nullopt, Name);
            Param.Version                         = Header.Version;
            if (!Header.EntryOffsets.empty()) {
                throw BinaryException(std::string("Expected param \"") + Name + "\" to be empty, but it was not.");
            }
            Reader.Seek(Header.NextParamOffset);
        };
        ReadEmpty(Layers, "LAYER_PARAM_ST");
        const auto PartOrder = Msb::Detail::ReadTypedParam(Reader, Parts, 8, std::nullopt, &Parts.Version);
        ReadEmpty(PartsPoses, "MAPSTUDIO_PARTS_POSE_ST");
        ReadEmpty(BoneNames, "MAPSTUDIO_BONE_NAME_STRING");
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
        for (PartsParam::Enemy& E : Parts.Enemies) {
            Lists.Enemies.push_back(&E);
        }
        for (PointParam::AutoDrawGroupPoint& P : Regions.AutoDrawGroupPoints) {
            Lists.AutoDrawGroupPoints.push_back(&P);
        }

        Msb::Detail::DisambiguateNames(Lists.Models);
        Msb::Detail::DisambiguateNames(Lists.Regions);
        Msb::Detail::DisambiguateNames(Lists.Parts);
        for (Event* E : EventPtrs) {
            E->GetNames(Lists);
        }
        for (Region* R : RegionPtrs) {
            R->GetNames(Lists);
        }
        for (Part* P : PartPtrs) {
            P->GetNames(Lists);
        }
    }

    void MSBS::WriteImpl(BinaryWriter& Writer) {
        const std::vector<Model*> ModelPtrs   = Models.GetEntries();
        const std::vector<Event*> EventPtrs   = Events.GetEntries();
        const std::vector<Region*> RegionPtrs = Regions.GetEntries();
        const std::vector<Route*> RoutePtrs   = Routes.GetEntries();
        const std::vector<Part*> PartPtrs     = Parts.GetEntries();

        Entries Lists;
        Lists.Models  = Upcast<Entry>(ModelPtrs);
        Lists.Events  = Upcast<Entry>(EventPtrs);
        Lists.Regions = Upcast<Entry>(RegionPtrs);
        Lists.Parts   = Upcast<Entry>(PartPtrs);
        for (PartsParam::Collision& C : Parts.Collisions) {
            Lists.Collisions.push_back(&C);
        }
        for (PartsParam::Enemy& E : Parts.Enemies) {
            Lists.Enemies.push_back(&E);
        }
        for (PointParam::AutoDrawGroupPoint& P : Regions.AutoDrawGroupPoints) {
            Lists.AutoDrawGroupPoints.push_back(&P);
        }

        for (Model* M : ModelPtrs) {
            M->InstanceCount = static_cast<int32_t>(
                std::count_if(PartPtrs.begin(), PartPtrs.end(), [&](const Part* P) { return P->ModelName.has_value() && *P->ModelName == M->Name; }));
        }
        for (Event* E : EventPtrs) {
            E->GetIndices(Lists);
        }
        for (Region* R : RegionPtrs) {
            R->GetIndices(Lists);
        }
        for (Part* P : PartPtrs) {
            P->GetIndices(Lists);
        }

        Writer.Order = Endian::Little;
        Msb::Detail::WriteHeader(Writer);
        Msb::Detail::WriteTypedParam(Writer, Models.Version, ModelParam::ParamName, ModelPtrs);
        Writer.Fill<int64_t>("NextParamOffset", Writer.Position());
        Msb::Detail::WriteTypedParam(Writer, Events.Version, EventParam::ParamName, EventPtrs);
        Writer.Fill<int64_t>("NextParamOffset", Writer.Position());
        Msb::Detail::WriteTypedParam(Writer, Regions.Version, PointParam::ParamName, RegionPtrs);
        Writer.Fill<int64_t>("NextParamOffset", Writer.Position());
        Msb::Detail::WriteTypedParam(Writer, Routes.Version, RouteParam::ParamName, RoutePtrs);
        Writer.Fill<int64_t>("NextParamOffset", Writer.Position());
        Msb::Detail::WriteParamHeader(Writer, Layers.Version, "LAYER_PARAM_ST", 0);
        Writer.Fill<int64_t>("NextParamOffset", Writer.Position());
        Msb::Detail::WriteTypedParam(Writer, Parts.Version, PartsParam::ParamName, PartPtrs);
        Writer.Fill<int64_t>("NextParamOffset", Writer.Position());
        Msb::Detail::WriteParamHeader(Writer, PartsPoses.Version, "MAPSTUDIO_PARTS_POSE_ST", 0);
        Writer.Fill<int64_t>("NextParamOffset", Writer.Position());
        Msb::Detail::WriteParamHeader(Writer, BoneNames.Version, "MAPSTUDIO_BONE_NAME_STRING", 0);
        Writer.Fill<int64_t>("NextParamOffset", 0);
    }
}  // namespace Souls
