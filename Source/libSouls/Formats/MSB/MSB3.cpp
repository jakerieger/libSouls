//
// Created by Jake Rieger on 10/8/2026.
//

#include "MSB3.hpp"

#include "MSBParamIO.hpp"

#include <algorithm>

namespace Souls {
    namespace {
        using Msb::Detail::ReambiguateName;

        using Msb::Detail::ParamHeader;
        using Msb::Detail::ReadParamHeader;
        using Msb::Detail::RequireNonZero;
        using Msb::Detail::Upcast;
        using Msb::Detail::WriteParamHeader;

        // The pose and bone name params, which have no type numbers.
        constexpr const char* PartsPoseParamName = "MAPSTUDIO_PARTS_POSE_ST";
        constexpr const char* BoneNameParamName   = "MAPSTUDIO_BONE_NAME_STRING";

        struct BoneName : MSB3::Entry {};
    }  // namespace

#pragma region Models
    void MSB3::Model::Read(BinaryReader& Reader) {
        const int64_t Start      = Reader.Position();
        const int64_t NameOffset = Reader.ReadInt64();
        Reader.Assert<uint32_t>(Type());
        Reader.ReadInt32();  // ID
        const int64_t SibOffset = Reader.ReadInt64();
        InstanceCount           = Reader.ReadInt32();
        Reader.Assert<int32_t>(0);
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

    void MSB3::Model::Write(BinaryWriter& Writer, int32_t ID) {
        const int64_t Start = Writer.Position();
        Writer.Reserve<int64_t>("NameOffset");
        Writer.WriteUInt32(Type());
        Writer.WriteInt32(ID);
        Writer.Reserve<int64_t>("SibOffset");
        Writer.WriteInt32(InstanceCount);
        Writer.WriteInt32(0);
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
    void MSB3::Event::Read(BinaryReader& Reader) {
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
        RequireNonZero(TypeDataOffset, "typeDataOffset");

        Reader.Seek(Start + NameOffset);
        Name = Reader.ReadUTF16Text();
        Reader.Seek(Start + BaseDataOffset);
        PartName.Index  = Reader.ReadInt32();
        PointName.Index = Reader.ReadInt32();
        EntityID        = Reader.ReadInt32();
        Reader.Assert<int32_t>(0);
        Reader.Seek(Start + TypeDataOffset);
        ReadTypeData(Reader);
    }

    void MSB3::Event::Write(BinaryWriter& Writer, int32_t ID) {
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
        Writer.WriteInt32(PointName.Index);
        Writer.WriteInt32(EntityID);
        Writer.WriteInt32(0);
        Writer.Fill<int64_t>("TypeDataOffset", Writer.Position() - Start);
        WriteTypeData(Writer);
    }

    void MSB3::Event::GetNames(const Entries& Lists) {
        Msb::Detail::NameVisitor<Entries> V{Lists};
        V(PartName);
        V(PointName);
    }

    void MSB3::Event::GetIndices(const Entries& Lists) {
        Msb::Detail::IndexVisitor<Entries> V{Lists};
        V(PartName);
        V(PointName);
    }
#pragma endregion

#pragma region Regions
    void MSB3::Region::Read(BinaryReader& Reader) {
        ResetForRead();
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
        if (std::holds_alternative<Msb::Shapes::Composite>(Shape)) {
            throw BinaryException("Dark Souls 3 does not support composite shapes.");
        }
        RequireNonZero(NameOffset, "nameOffset");
        RequireNonZero(BaseDataOffset1, "baseDataOffset1");
        RequireNonZero(BaseDataOffset2, "baseDataOffset2");
        if (Msb::HasShapeData(Shape) != (ShapeDataOffset != 0)) {
            throw BinaryException("Unexpected shapeDataOffset in region");
        }
        RequireNonZero(BaseDataOffset3, "baseDataOffset3");
        const TypeDataPresence Presence = ShouldHaveTypeData();
        if ((Presence == TypeDataPresence::Never && TypeDataOffset != 0) || (Presence == TypeDataPresence::Always && TypeDataOffset == 0) ||
            (Presence == TypeDataPresence::AlwaysNull && TypeDataOffset != 0)) {
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
        if (TypeDataOffset != 0 || Presence == TypeDataPresence::AlwaysNull) {
            if (TypeDataOffset != 0) {
                Reader.Seek(Start + TypeDataOffset);
            }
            ReadTypeData(Reader);
        }
    }

    void MSB3::Region::Write(BinaryWriter& Writer, int32_t ID) {
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
        if (DoesHaveTypeData() && ShouldHaveTypeData() != TypeDataPresence::AlwaysNull) {
            Writer.Fill<int64_t>("TypeDataOffset", Writer.Position() - Start);
        } else {
            Writer.Fill<int64_t>("TypeDataOffset", 0);
        }
        if (DoesHaveTypeData()) {
            WriteTypeData(Writer);
        }
        Writer.Align(8);
    }

    void MSB3::Region::GetNames(const Entries& Lists) {
        Msb::Detail::NameVisitor<Entries> V{Lists};
        V(ActivationPartName);
        // Composite shapes aren't supported, so the shape has no references.
    }

    void MSB3::Region::GetIndices(const Entries& Lists) {
        Msb::Detail::IndexVisitor<Entries> V{Lists};
        V(ActivationPartName);
    }
#pragma endregion

#pragma region Routes and layers
    void MSB3::Route::Read(BinaryReader& Reader) {
        const int64_t Start      = Reader.Position();
        const int64_t NameOffset = Reader.ReadInt64();
        Unk08                    = Reader.ReadInt32();
        Unk0C                    = Reader.ReadInt32();
        Reader.Assert<int32_t>(4);  // type
        Reader.ReadInt32();         // ID
        Reader.AssertPattern(0x68, 0);
        RequireNonZero(NameOffset, "nameOffset");
        Reader.Seek(Start + NameOffset);
        Name = Reader.ReadUTF16Text();
    }

    void MSB3::Route::Write(BinaryWriter& Writer, int32_t ID) {
        const int64_t Start = Writer.Position();
        Writer.Reserve<int64_t>("NameOffset");
        Writer.WriteInt32(Unk08);
        Writer.WriteInt32(Unk0C);
        Writer.WriteInt32(4);
        Writer.WriteInt32(ID);
        Writer.Pad(0x68);
        Writer.Fill<int64_t>("NameOffset", Writer.Position() - Start);
        Writer.WriteUTF16Text(Name, true);
        Writer.Align(8);
    }

    void MSB3::Layer::Read(BinaryReader& Reader) {
        const int64_t Start      = Reader.Position();
        const int64_t NameOffset = Reader.ReadInt64();
        Unk08                    = Reader.ReadInt32();
        Unk0C                    = Reader.ReadInt32();
        Unk10                    = Reader.ReadInt32();
        RequireNonZero(NameOffset, "nameOffset");
        Reader.Seek(Start + NameOffset);
        Name = Reader.ReadUTF16Text();
    }

    void MSB3::Layer::Write(BinaryWriter& Writer, int32_t) {
        const int64_t Start = Writer.Position();
        Writer.Reserve<int64_t>("NameOffset");
        Writer.WriteInt32(Unk08);
        Writer.WriteInt32(Unk0C);
        Writer.WriteInt32(Unk10);
        Writer.Fill<int64_t>("NameOffset", Writer.Position() - Start);
        Writer.WriteUTF16Text(Name, true);
        Writer.Align(8);
    }
#pragma endregion

#pragma region Parts
    void MSB3::Part::Read(BinaryReader& Reader) {
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
        MapStudioLayer = Reader.ReadUInt32();
        Reader.ReadInto(std::span<uint32_t>(DrawGroups));
        Reader.ReadInto(std::span<uint32_t>(DispGroups));
        Reader.ReadInto(std::span<uint32_t>(BackreadGroups));
        Reader.Assert<int32_t>(0);
        const int64_t EntityDataOffset = Reader.ReadInt64();
        const int64_t TypeDataOffset   = Reader.ReadInt64();
        const int64_t GparamOffset     = Reader.ReadInt64();
        const int64_t SceneGparamOffset = Reader.ReadInt64();
        RequireNonZero(NameOffset, "nameOffset");
        RequireNonZero(SibOffset, "sibOffset");
        RequireNonZero(EntityDataOffset, "entityDataOffset");
        RequireNonZero(TypeDataOffset, "typeDataOffset");
        if (HasGparamConfig() != (GparamOffset != 0)) {
            throw BinaryException("Unexpected gparamOffset in part");
        }
        if (HasSceneGparamConfig() != (SceneGparamOffset != 0)) {
            throw BinaryException("Unexpected sceneGparamOffset in part");
        }

        Reader.Seek(Start + NameOffset);
        Name = Reader.ReadUTF16Text();
        Reader.Seek(Start + SibOffset);
        SibPath = Reader.ReadUTF16Text();

        Reader.Seek(Start + EntityDataOffset);
        EntityID = Reader.ReadInt32();
        UnkE04   = Reader.ReadSByte();
        UnkE05   = Reader.ReadSByte();
        Reader.Assert<int16_t>(0);
        Reader.Assert<int32_t>(0);
        LanternID               = Reader.ReadSByte();
        LodParamID              = Reader.ReadSByte();
        UnkE0E                  = Reader.ReadSByte();
        PointLightShadowSource  = Reader.ReadBool();
        ShadowSource            = Reader.ReadBool();
        ShadowDest              = Reader.ReadBool();
        IsShadowOnly            = Reader.ReadBool();
        DrawByReflectCam        = Reader.ReadBool();
        DrawOnlyReflectCam      = Reader.ReadBool();
        UseDepthBiasFloat       = Reader.ReadBool();
        DisablePointLightEffect = Reader.ReadBool();
        Reader.Assert<uint8_t>(0);
        UnkE18 = Reader.ReadInt32();
        Reader.ReadInto(std::span<int32_t>(EntityGroups));
        Reader.Assert<int32_t>(0);

        Reader.Seek(Start + TypeDataOffset);
        ReadTypeData(Reader);
        if (HasGparamConfig()) {
            Reader.Seek(Start + GparamOffset);
            ReadGparamConfig(Reader);
        }
        if (HasSceneGparamConfig()) {
            Reader.Seek(Start + SceneGparamOffset);
            ReadSceneGparamConfig(Reader);
        }
    }

    void MSB3::Part::Write(BinaryWriter& Writer, int32_t ID) {
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
        Writer.WriteUInt32(MapStudioLayer);
        Writer.WriteArray(std::span<const uint32_t>(DrawGroups));
        Writer.WriteArray(std::span<const uint32_t>(DispGroups));
        Writer.WriteArray(std::span<const uint32_t>(BackreadGroups));
        Writer.WriteInt32(0);
        Writer.Reserve<int64_t>("EntityDataOffset");
        Writer.Reserve<int64_t>("TypeDataOffset");
        Writer.Reserve<int64_t>("GparamOffset");
        Writer.Reserve<int64_t>("SceneGparamOffset");

        Writer.Fill<int64_t>("NameOffset", Writer.Position() - Start);
        Writer.WriteUTF16Text(ReambiguateName(Name), true);
        Writer.Fill<int64_t>("SibOffset", Writer.Position() - Start);
        Writer.WriteUTF16Text(SibPath, true);
        // This is purely here for byte-perfect writes.
        if (SibPath.empty()) {
            Writer.Pad(0x24);
        }
        Writer.Align(8);

        Writer.Fill<int64_t>("EntityDataOffset", Writer.Position() - Start);
        Writer.WriteInt32(EntityID);
        Writer.WriteSByte(UnkE04);
        Writer.WriteSByte(UnkE05);
        Writer.WriteInt16(0);
        Writer.WriteInt32(0);
        Writer.WriteSByte(LanternID);
        Writer.WriteSByte(LodParamID);
        Writer.WriteSByte(UnkE0E);
        Writer.WriteBool(PointLightShadowSource);
        Writer.WriteBool(ShadowSource);
        Writer.WriteBool(ShadowDest);
        Writer.WriteBool(IsShadowOnly);
        Writer.WriteBool(DrawByReflectCam);
        Writer.WriteBool(DrawOnlyReflectCam);
        Writer.WriteBool(UseDepthBiasFloat);
        Writer.WriteBool(DisablePointLightEffect);
        Writer.WriteByte(0);
        Writer.WriteInt32(UnkE18);
        Writer.WriteArray(std::span<const int32_t>(EntityGroups));
        Writer.WriteInt32(0);
        Writer.Align(8);

        Writer.Fill<int64_t>("TypeDataOffset", Writer.Position() - Start);
        WriteTypeData(Writer);
        if (HasGparamConfig()) {
            Writer.Fill<int64_t>("GparamOffset", Writer.Position() - Start);
            WriteGparamConfig(Writer);
        } else {
            Writer.Fill<int64_t>("GparamOffset", 0);
        }
        if (HasSceneGparamConfig()) {
            Writer.Fill<int64_t>("SceneGparamOffset", Writer.Position() - Start);
            WriteSceneGparamConfig(Writer);
        } else {
            Writer.Fill<int64_t>("SceneGparamOffset", 0);
        }
    }

    void MSB3::Part::GetNames(const Entries& Lists) {
        Msb::Detail::NameVisitor<Entries> V{Lists};
        V(ModelName);
    }

    void MSB3::Part::GetIndices(const Entries& Lists) {
        Msb::Detail::IndexVisitor<Entries> V{Lists};
        V(ModelName);
    }
#pragma endregion

    bool MSB3::IsImpl(BinaryReader& Reader) {
        if (Reader.Length() < 4) {
            return false;
        }
        return Reader.GetASCII(0, 4) == "MSB ";
    }

    void MSB3::ReadImpl(BinaryReader& Reader) {
        Reader.Order = Endian::Little;
        Msb::Detail::AssertHeader(Reader);

        Models  = {};
        Events  = {};
        Regions = {};
        Parts   = {};
        Routes.clear();
        Layers.clear();
        PartsPoses.clear();

        const auto ModelOrder  = Msb::Detail::ReadTypedParam(Reader, Models, 8, ModelParam::ParamVersion);
        const auto EventOrder  = Msb::Detail::ReadTypedParam(Reader, Events, 0xC, EventParam::ParamVersion);
        const auto RegionOrder = Msb::Detail::ReadTypedParam(Reader, Regions, 8, PointParam::ParamVersion);

        {
            const ParamHeader Header = ReadParamHeader(Reader, 3, "ROUTE_PARAM_ST");
            for (const int64_t Offset : Header.EntryOffsets) {
                Reader.Seek(Offset);
                Route R;
                R.Read(Reader);
                Routes.push_back(std::move(R));
            }
            Reader.Seek(Header.NextParamOffset);
        }
        {
            const ParamHeader Header = ReadParamHeader(Reader, 3, "LAYER_PARAM_ST");
            for (const int64_t Offset : Header.EntryOffsets) {
                Reader.Seek(Offset);
                Layer L;
                L.Read(Reader);
                Layers.push_back(std::move(L));
            }
            Reader.Seek(Header.NextParamOffset);
        }

        const auto PartOrder = Msb::Detail::ReadTypedParam(Reader, Parts, 8, PartsParam::ParamVersion);

        std::vector<PartsPose> Poses;
        {
            const ParamHeader Header = ReadParamHeader(Reader, 0, PartsPoseParamName);
            for (const int64_t Offset : Header.EntryOffsets) {
                Reader.Seek(Offset);
                PartsPose P;
                P.PartName.Index        = Reader.ReadInt16();
                const int16_t BoneCount = Reader.ReadInt16();
                Reader.Assert<int32_t>(0);
                Reader.Assert<int64_t>(0x10);
                for (int16_t I = 0; I < BoneCount; ++I) {
                    PartsPose::Bone B;
                    B.NameIndex   = Reader.ReadInt32();
                    B.Translation = Reader.ReadVector3();
                    B.Rotation    = Reader.ReadVector3();
                    B.Scale       = Reader.ReadVector3();
                    P.Bones.push_back(std::move(B));
                }
                Poses.push_back(std::move(P));
            }
            Reader.Seek(Header.NextParamOffset);
        }

        std::vector<BoneName> BoneNames;
        {
            const ParamHeader Header = ReadParamHeader(Reader, 0, BoneNameParamName);
            for (const int64_t Offset : Header.EntryOffsets) {
                Reader.Seek(Offset);
                BoneName B;
                B.Name = Reader.ReadUTF16Text();
                BoneNames.push_back(std::move(B));
            }
            Reader.Seek(Header.NextParamOffset);
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
        for (EventParam::PatrolInfo& P : Events.PatrolInfos) {
            Lists.PatrolInfos.push_back(&P);
        }
        for (BoneName& B : BoneNames) {
            Lists.BoneNames.push_back(&B);
        }

        Msb::Detail::DisambiguateNames(Lists.Models);
        Msb::Detail::DisambiguateNames(Lists.Parts);
        Msb::Detail::DisambiguateNames(Lists.Regions);
        Msb::Detail::DisambiguateNames(Lists.BoneNames);
        for (Event* E : EventPtrs) {
            E->GetNames(Lists);
        }
        for (Region* R : RegionPtrs) {
            R->GetNames(Lists);
        }
        for (Part* P : PartPtrs) {
            P->GetNames(Lists);
        }
        for (PartsPose& P : Poses) {
            Msb::Detail::NameVisitor<Entries> V{Lists};
            V(P.PartName);
            for (PartsPose::Bone& B : P.Bones) {
                B.Name = Msb::Detail::FindName(Lists.BoneNames, B.NameIndex).value_or("");
            }
        }
        PartsPoses = std::move(Poses);
    }

    void MSB3::WriteImpl(BinaryWriter& Writer) {
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
        for (EventParam::PatrolInfo& P : Events.PatrolInfos) {
            Lists.PatrolInfos.push_back(&P);
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

        // Bone names are collected from the poses as they are written.
        std::vector<BoneName> BoneNames;
        for (PartsPose& P : PartsPoses) {
            Msb::Detail::IndexVisitor<Entries> V{Lists};
            V(P.PartName);
            for (PartsPose::Bone& B : P.Bones) {
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

        Writer.Order = Endian::Little;
        Msb::Detail::WriteHeader(Writer);
        Msb::Detail::WriteTypedParam(Writer, ModelParam::ParamVersion, ModelParam::ParamName, ModelPtrs);
        Writer.Fill<int64_t>("NextParamOffset", Writer.Position());
        Msb::Detail::WriteTypedParam(Writer, EventParam::ParamVersion, EventParam::ParamName, EventPtrs);
        Writer.Fill<int64_t>("NextParamOffset", Writer.Position());
        Msb::Detail::WriteTypedParam(Writer, PointParam::ParamVersion, PointParam::ParamName, RegionPtrs);
        Writer.Fill<int64_t>("NextParamOffset", Writer.Position());

        WriteParamHeader(Writer, 3, "ROUTE_PARAM_ST", Routes.size());
        for (size_t I = 0; I < Routes.size(); ++I) {
            Writer.Fill<int64_t>("EntryOffset" + std::to_string(I), Writer.Position());
            Routes[I].Write(Writer, static_cast<int32_t>(I));
        }
        Writer.Fill<int64_t>("NextParamOffset", Writer.Position());

        WriteParamHeader(Writer, 3, "LAYER_PARAM_ST", Layers.size());
        for (size_t I = 0; I < Layers.size(); ++I) {
            Writer.Fill<int64_t>("EntryOffset" + std::to_string(I), Writer.Position());
            Layers[I].Write(Writer, static_cast<int32_t>(I));
        }
        Writer.Fill<int64_t>("NextParamOffset", Writer.Position());

        Msb::Detail::WriteTypedParam(Writer, PartsParam::ParamVersion, PartsParam::ParamName, PartPtrs);
        Writer.Fill<int64_t>("NextParamOffset", Writer.Position());

        WriteParamHeader(Writer, 0, PartsPoseParamName, PartsPoses.size());
        for (size_t I = 0; I < PartsPoses.size(); ++I) {
            const PartsPose& P = PartsPoses[I];
            Writer.Fill<int64_t>("EntryOffset" + std::to_string(I), Writer.Position());
            Writer.WriteInt16(P.PartName.Index);
            Writer.WriteInt16(static_cast<int16_t>(P.Bones.size()));
            Writer.WriteInt32(0);
            Writer.WriteInt64(0x10);
            for (const PartsPose::Bone& B : P.Bones) {
                Writer.WriteInt32(B.NameIndex);
                Writer.WriteVector3(B.Translation);
                Writer.WriteVector3(B.Rotation);
                Writer.WriteVector3(B.Scale);
            }
        }
        Writer.Fill<int64_t>("NextParamOffset", Writer.Position());

        WriteParamHeader(Writer, 0, BoneNameParamName, BoneNames.size());
        for (size_t I = 0; I < BoneNames.size(); ++I) {
            Writer.Fill<int64_t>("EntryOffset" + std::to_string(I), Writer.Position());
            Writer.WriteUTF16Text(BoneNames[I].Name, true);
            Writer.Align(8);
        }
        Writer.Fill<int64_t>("NextParamOffset", 0);
    }
}  // namespace Souls
