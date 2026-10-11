// Copyright (C) 2026 Jake Rieger
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, version 3. This program is distributed in the hope that it will
// be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU General
// Public License along with this program. If not, see https://www.gnu.org/licenses/.

#include "MSBE.hpp"

#include "MSBParamIO.hpp"

#include <algorithm>
#include <iterator>

namespace Souls {
    namespace {
        using Msb::Detail::ReadParamHeader;
        using Msb::Detail::ReambiguateName;
        using Msb::Detail::RequireNonZero;
        using Msb::Detail::Upcast;

        void RequireMatch(bool Expected, int64_t Offset, const char* What) {
            if (Expected != (Offset != 0)) {
                throw BinaryException(std::string("Unexpected ") + What + " in MSBE entry");
            }
        }
    }  // namespace

#pragma region Models
    void MSBE::Model::Read(BinaryReader& Reader) {
        const int64_t Start      = Reader.Position();
        const int64_t NameOffset = Reader.ReadInt64();
        Reader.Assert<uint32_t>(Type());
        Reader.ReadInt32();  // ID
        const int64_t SibOffset      = Reader.ReadInt64();
        InstanceCount                = Reader.ReadInt32();
        Unk1C                        = Reader.ReadInt32();
        const int64_t TypeDataOffset = Reader.ReadInt64();
        RequireNonZero(NameOffset, "nameOffset");
        RequireNonZero(SibOffset, "sibOffset");
        RequireMatch(false, TypeDataOffset, "typeDataOffset");
        Reader.Seek(Start + NameOffset);
        Name = Reader.ReadUTF16Text();
        Reader.Seek(Start + SibOffset);
        SibPath = Reader.ReadUTF16Text();
    }

    void MSBE::Model::Write(BinaryWriter& Writer, int32_t ID) {
        const int64_t Start = Writer.Position();
        Writer.Reserve<int64_t>("NameOffset");
        Writer.WriteUInt32(Type());
        Writer.WriteInt32(ID);
        Writer.Reserve<int64_t>("SibOffset");
        Writer.WriteInt32(InstanceCount);
        Writer.WriteInt32(Unk1C);
        Writer.WriteInt64(0);  // type data offset; no Elden Ring model has any
        Writer.Fill<int64_t>("NameOffset", Writer.Position() - Start);
        Writer.WriteUTF16Text(ReambiguateName(Name), true);
        Writer.Fill<int64_t>("SibOffset", Writer.Position() - Start);
        Writer.WriteUTF16Text(SibPath, true);
        Writer.Align(8);
    }
#pragma endregion

#pragma region Events
    void MSBE::Event::Read(BinaryReader& Reader) {
        const int64_t Start      = Reader.Position();
        const int64_t NameOffset = Reader.ReadInt64();
        EventID                  = Reader.ReadInt32();
        Reader.Assert<uint32_t>(Type());
        FileID                       = Reader.ReadInt32();
        Unk14                        = Reader.ReadInt32();
        const int64_t BaseDataOffset = Reader.ReadInt64();
        const int64_t TypeDataOffset = Reader.ReadInt64();
        const int64_t Unk3Offset     = Reader.ReadInt64();
        RequireNonZero(NameOffset, "nameOffset");
        RequireNonZero(BaseDataOffset, "baseDataOffset");
        RequireMatch(HasTypeData(), TypeDataOffset, "typeDataOffset");
        RequireNonZero(Unk3Offset, "unk3Offset");

        Reader.Seek(Start + NameOffset);
        Name = Reader.ReadUTF16Text();
        Reader.Seek(Start + BaseDataOffset);
        PartName.Index   = Reader.ReadInt32();
        RegionName.Index = Reader.ReadInt32();
        EntityID         = Reader.ReadUInt32();
        UnkE0C           = Reader.ReadByte();
        Reader.AssertPattern(3, 0);
        if (HasTypeData()) {
            Reader.Seek(Start + TypeDataOffset);
            ReadTypeData(Reader);
        }
        Reader.Seek(Start + Unk3Offset);
        MapID  = Reader.ReadInt32();
        UnkS04 = Reader.ReadInt32();
        UnkS08 = Reader.ReadInt32();
        UnkS0C = Reader.ReadInt32();
        Reader.AssertPattern(0x10, 0);
    }

    void MSBE::Event::Write(BinaryWriter& Writer, int32_t ID) {
        const int64_t Start = Writer.Position();
        Writer.Reserve<int64_t>("NameOffset");
        Writer.WriteInt32(EventID);
        Writer.WriteUInt32(Type());
        Writer.WriteInt32(FileID != -1 ? FileID : ID);
        Writer.WriteInt32(Unk14);
        Writer.Reserve<int64_t>("BaseDataOffset");
        Writer.Reserve<int64_t>("TypeDataOffset");
        Writer.Reserve<int64_t>("Unk3Offset");

        Writer.Fill<int64_t>("NameOffset", Writer.Position() - Start);
        Writer.WriteUTF16Text(ReambiguateName(Name), true);
        Writer.Align(8);
        Writer.Fill<int64_t>("BaseDataOffset", Writer.Position() - Start);
        Writer.WriteInt32(PartName.Index);
        Writer.WriteInt32(RegionName.Index);
        Writer.WriteUInt32(EntityID);
        Writer.WriteByte(UnkE0C);
        Writer.Pad(3);
        if (HasTypeData()) {
            Writer.Fill<int64_t>("TypeDataOffset", Writer.Position() - Start);
            WriteTypeData(Writer);
        } else {
            Writer.Fill<int64_t>("TypeDataOffset", 0);
        }
        // Only 4-byte aligned: the type data of pseudo multiplayer and area team events ends 4 bytes off an 8-byte
        // boundary, and the game leaves it there.
        Writer.Align(4);
        Writer.Fill<int64_t>("Unk3Offset", Writer.Position() - Start);
        Writer.WriteInt32(MapID);
        Writer.WriteInt32(UnkS04);
        Writer.WriteInt32(UnkS08);
        Writer.WriteInt32(UnkS0C);
        Writer.Pad(0x10);
        Writer.Align(8);
    }

    void MSBE::Event::GetNames(const Entries& Lists) {
        Msb::Detail::NameVisitor<Entries> V {Lists};
        V(PartName);
        V(RegionName);
    }

    void MSBE::Event::GetIndices(const Entries& Lists) {
        Msb::Detail::IndexVisitor<Entries> V {Lists};
        V(PartName);
        V(RegionName);
    }
#pragma endregion

#pragma region Regions
    void MSBE::Region::Read(BinaryReader& Reader) {
        const int64_t Start      = Reader.Position();
        const int64_t NameOffset = Reader.ReadInt64();
        Reader.Assert<uint32_t>(Type());
        Reader.ReadInt32();  // ID
        const auto ShapeKind          = static_cast<Msb::ShapeType>(Reader.ReadUInt32());
        Position                      = Reader.ReadVector3();
        Rotation                      = Reader.ReadVector3();
        RegionID                      = Reader.ReadInt32();
        const int64_t BaseDataOffset1 = Reader.ReadInt64();
        const int64_t BaseDataOffset2 = Reader.ReadInt64();
        Unk40                         = Reader.ReadInt32();
        MapStudioLayer                = Reader.ReadUInt32();
        const int64_t ShapeDataOffset = Reader.ReadInt64();
        const int64_t BaseDataOffset3 = Reader.ReadInt64();
        const int64_t TypeDataOffset  = Reader.ReadInt64();
        const int64_t Unk4Offset      = Reader.ReadInt64();
        Shape                         = Msb::CreateShape(ShapeKind);
        RequireNonZero(NameOffset, "nameOffset");
        RequireNonZero(BaseDataOffset1, "baseDataOffset1");
        RequireNonZero(BaseDataOffset2, "baseDataOffset2");
        RequireMatch(Msb::HasShapeData(Shape), ShapeDataOffset, "shapeDataOffset");
        RequireNonZero(BaseDataOffset3, "baseDataOffset3");
        RequireMatch(HasTypeData(), TypeDataOffset, "typeDataOffset");
        RequireNonZero(Unk4Offset, "unkOffset4");

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
            Msb::Detail::ReadVisitor V {Reader};
            Msb::Detail::VisitShape(Shape, V);
        }
        Reader.Seek(Start + BaseDataOffset3);
        ActivationPartName.Index = Reader.ReadInt32();
        EntityID                 = Reader.ReadUInt32();
        UnkE08                   = Reader.ReadByte();
        Reader.AssertPattern(7, 0);
        if (HasTypeData()) {
            Reader.Seek(Start + TypeDataOffset);
            ReadTypeData(Reader);
        }
        Reader.Seek(Start + Unk4Offset);
        MapID  = Reader.ReadInt32();
        UnkS04 = Reader.ReadInt32();
        Reader.Assert<int32_t>(0);
        UnkS0C = Reader.ReadInt32();
        Reader.AssertPattern(0x10, 0);
    }

    void MSBE::Region::Write(BinaryWriter& Writer, int32_t ID) {
        const int64_t Start = Writer.Position();
        Writer.Reserve<int64_t>("NameOffset");
        Writer.WriteUInt32(Type());
        Writer.WriteInt32(ID);
        Writer.WriteUInt32(static_cast<uint32_t>(Msb::TypeOf(Shape)));
        Writer.WriteVector3(Position);
        Writer.WriteVector3(Rotation);
        Writer.WriteInt32(RegionID);
        Writer.Reserve<int64_t>("BaseDataOffset1");
        Writer.Reserve<int64_t>("BaseDataOffset2");
        Writer.WriteInt32(Unk40);
        Writer.WriteUInt32(MapStudioLayer);
        Writer.Reserve<int64_t>("ShapeDataOffset");
        Writer.Reserve<int64_t>("BaseDataOffset3");
        Writer.Reserve<int64_t>("TypeDataOffset");
        Writer.Reserve<int64_t>("Unk4Offset");

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
            Msb::Detail::WriteVisitor V {Writer};
            Msb::Detail::VisitShape(Shape, V);
        } else {
            Writer.Fill<int64_t>("ShapeDataOffset", 0);
        }
        Writer.Fill<int64_t>("BaseDataOffset3", Writer.Position() - Start);
        Writer.WriteInt32(ActivationPartName.Index);
        Writer.WriteUInt32(EntityID);
        Writer.WriteByte(UnkE08);
        Writer.Pad(7);

        // The region types after DisableTumbleweed align their type data; the older ones align what follows it.
        // (Checked against every map in the game.)
        const uint32_t T        = Type();
        const bool AlignsBefore = T > PointParam::DisableTumbleweed::TypeId && T != PointParam::Other::TypeId;
        if (AlignsBefore) { Writer.Align(8); }
        if (HasTypeData()) {
            Writer.Fill<int64_t>("TypeDataOffset", Writer.Position() - Start);
            WriteTypeData(Writer);
        } else {
            Writer.Fill<int64_t>("TypeDataOffset", 0);
        }
        if (!AlignsBefore) { Writer.Align(8); }
        Writer.Fill<int64_t>("Unk4Offset", Writer.Position() - Start);
        Writer.WriteInt32(MapID);
        Writer.WriteInt32(UnkS04);
        Writer.WriteInt32(0);
        Writer.WriteInt32(UnkS0C);
        Writer.Pad(0x10);
        Writer.Align(8);
    }

    void MSBE::Region::GetNames(const Entries& Lists) {
        Msb::Detail::NameVisitor<Entries> V {Lists};
        V(ActivationPartName);
        Msb::Detail::VisitShape(Shape, V);
    }

    void MSBE::Region::GetIndices(const Entries& Lists) {
        Msb::Detail::IndexVisitor<Entries> V {Lists};
        V(ActivationPartName);
        Msb::Detail::VisitShape(Shape, V);
    }
#pragma endregion

#pragma region Routes
    void MSBE::Route::Read(BinaryReader& Reader) {
        const int64_t Start      = Reader.Position();
        const int64_t NameOffset = Reader.ReadInt64();
        Unk08                    = Reader.ReadInt32();
        Unk0C                    = Reader.ReadInt32();
        Reader.Assert<uint32_t>(Type());
        FileID = Reader.ReadInt32();
        Reader.AssertPattern(0x68, 0);
        RequireNonZero(NameOffset, "nameOffset");
        Reader.Seek(Start + NameOffset);
        Name = Reader.ReadUTF16Text();
    }

    void MSBE::Route::Write(BinaryWriter& Writer, int32_t ID) {
        const int64_t Start = Writer.Position();
        Writer.Reserve<int64_t>("NameOffset");
        Writer.WriteInt32(Unk08);
        Writer.WriteInt32(Unk0C);
        Writer.WriteUInt32(Type());
        Writer.WriteInt32(FileID != -1 ? FileID : ID);
        Writer.Pad(0x68);
        Writer.Fill<int64_t>("NameOffset", Writer.Position() - Start);
        Writer.WriteUTF16Text(Name, true);
        Writer.Align(8);
    }
#pragma endregion

#pragma region Parts
    namespace {
        // The optional blocks in the order of their offsets in the part header.
        constexpr unsigned BlockOrder[] = {
          MSBE::HasUnk1,
          MSBE::HasUnk2,
          MSBE::HasGparam,
          MSBE::HasSceneGparam,
          MSBE::HasGrass,
          MSBE::HasUnk8,
          MSBE::HasUnk9,
          MSBE::HasTileLoad,
          MSBE::HasUnk11,
        };
    }  // namespace

    void MSBE::Part::Read(BinaryReader& Reader) {
        const int64_t Start      = Reader.Position();
        const int64_t NameOffset = Reader.ReadInt64();
        InstanceID               = Reader.ReadInt32();
        Reader.Assert<uint32_t>(Type());
        Reader.ReadInt32();  // ID
        ModelName.Index         = Reader.ReadInt32();
        const int64_t SibOffset = Reader.ReadInt64();
        Position                = Reader.ReadVector3();
        Rotation                = Reader.ReadVector3();
        Scale                   = Reader.ReadVector3();
        GameEditionDisable      = Reader.ReadUInt32();
        MapStudioLayer          = Reader.ReadUInt32();
        Reader.Assert<int32_t>(0);
        // Offsets: unk1, unk2, entity data, type data, gparam, scene gparam, grass, unk8, unk9, tile load, unk11.
        int64_t Offsets[11];
        for (int64_t& Offset : Offsets) {
            Offset = Reader.ReadInt64();
        }
        Reader.AssertPattern(0x18, 0);
        const int64_t EntityDataOffset = Offsets[2];
        const int64_t TypeDataOffset   = Offsets[3];
        const auto BlockOffset         = [&](size_t Index) { return Offsets[Index < 2 ? Index : Index + 2]; };

        const unsigned Has = Blocks();
        RequireNonZero(NameOffset, "nameOffset");
        RequireNonZero(SibOffset, "sibOffset");
        RequireNonZero(EntityDataOffset, "entityDataOffset");
        RequireNonZero(TypeDataOffset, "typeDataOffset");
        for (size_t I = 0; I < std::size(BlockOrder); ++I) {
            RequireMatch((Has & BlockOrder[I]) != 0, BlockOffset(I), "part block offset");
        }

        Reader.Seek(Start + NameOffset);
        Name = Reader.ReadUTF16Text();
        Reader.Seek(Start + SibOffset);
        SibPath = Reader.ReadUTF16Text();

        Reader.Seek(Start + EntityDataOffset);
        EntityID              = Reader.ReadUInt32();
        IsUsePartsDrawParamID = Reader.ReadByte();
        Reader.AssertPattern(3, 0);
        PartsDrawParamID        = Reader.ReadInt16();
        IsPointLightShadowSrc   = Reader.ReadSByte();
        UnkE0B                  = Reader.ReadByte();
        IsShadowSrc             = Reader.ReadBool();
        IsStaticShadowSrc       = Reader.ReadByte();
        IsCascade3ShadowSrc     = Reader.ReadByte();
        UnkE0F                  = Reader.ReadByte();
        UnkE10                  = Reader.ReadByte();
        IsShadowDest            = Reader.ReadBool();
        IsShadowOnly            = Reader.ReadBool();
        DrawByReflectCam        = Reader.ReadBool();
        DrawOnlyReflectCam      = Reader.ReadBool();
        EnableOnAboveShadow     = Reader.ReadByte();
        DisablePointLightEffect = Reader.ReadBool();
        UnkE17                  = Reader.ReadByte();
        UnkE18                  = Reader.ReadInt32();
        Reader.ReadInto(std::span<uint32_t>(EntityGroupIDs));
        UnkE3C = Reader.ReadInt16();
        UnkE3E = Reader.ReadInt16();

        Reader.Seek(Start + TypeDataOffset);
        ReadTypeData(Reader);
        for (size_t I = 0; I < std::size(BlockOrder); ++I) {
            if (Has & BlockOrder[I]) {
                Reader.Seek(Start + BlockOffset(I));
                ReadBlock(BlockOrder[I], Reader);
            }
        }
    }

    void MSBE::Part::Write(BinaryWriter& Writer, int32_t ID) {
        static constexpr const char* BlockNames[] = {"UnkOffset1",
                                                     "UnkOffset2",
                                                     "GparamOffset",
                                                     "SceneGparamOffset",
                                                     "GrassOffset",
                                                     "UnkOffset8",
                                                     "UnkOffset9",
                                                     "TileLoadOffset",
                                                     "UnkOffset11"};
        const int64_t Start                       = Writer.Position();
        Writer.Reserve<int64_t>("NameOffset");
        Writer.WriteInt32(InstanceID);
        Writer.WriteUInt32(Type());
        Writer.WriteInt32(ID);
        Writer.WriteInt32(ModelName.Index);
        Writer.Reserve<int64_t>("SibOffset");
        Writer.WriteVector3(Position);
        Writer.WriteVector3(Rotation);
        Writer.WriteVector3(Scale);
        Writer.WriteUInt32(GameEditionDisable);
        Writer.WriteUInt32(MapStudioLayer);
        Writer.WriteInt32(0);
        Writer.Reserve<int64_t>(BlockNames[0]);
        Writer.Reserve<int64_t>(BlockNames[1]);
        Writer.Reserve<int64_t>("EntityDataOffset");
        Writer.Reserve<int64_t>("TypeDataOffset");
        for (size_t I = 2; I < std::size(BlockNames); ++I) {
            Writer.Reserve<int64_t>(BlockNames[I]);
        }
        Writer.Pad(0x18);

        const unsigned Has = Blocks();
        Writer.Fill<int64_t>("NameOffset", Writer.Position() - Start);
        Writer.WriteUTF16Text(ReambiguateName(Name), true);
        Writer.Fill<int64_t>("SibOffset", Writer.Position() - Start);
        Writer.WriteUTF16Text(SibPath, true);
        Writer.Align(8);

        const auto OptionalBlock = [&](size_t Index) {
            if (Has & BlockOrder[Index]) {
                Writer.Fill<int64_t>(BlockNames[Index], Writer.Position() - Start);
                WriteBlock(BlockOrder[Index], Writer);
            } else {
                Writer.Fill<int64_t>(BlockNames[Index], 0);
            }
        };
        OptionalBlock(0);
        OptionalBlock(1);

        Writer.Fill<int64_t>("EntityDataOffset", Writer.Position() - Start);
        Writer.WriteUInt32(EntityID);
        Writer.WriteByte(IsUsePartsDrawParamID);
        Writer.Pad(3);
        Writer.WriteInt16(PartsDrawParamID);
        Writer.WriteSByte(IsPointLightShadowSrc);
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
        Writer.WriteArray(std::span<const uint32_t>(EntityGroupIDs));
        Writer.WriteInt16(UnkE3C);
        Writer.WriteInt16(UnkE3E);
        Writer.Align(8);

        Writer.Fill<int64_t>("TypeDataOffset", Writer.Position() - Start);
        WriteTypeData(Writer);
        for (size_t I = 2; I < std::size(BlockOrder); ++I) {
            OptionalBlock(I);
        }
    }

    void MSBE::Part::GetNames(const Entries& Lists) {
        Msb::Detail::NameVisitor<Entries> V {Lists};
        V(ModelName);
    }

    void MSBE::Part::GetIndices(const Entries& Lists) {
        Msb::Detail::IndexVisitor<Entries> V {Lists};
        V(ModelName);
    }
#pragma endregion

    bool MSBE::IsImpl(BinaryReader& Reader) {
        if (Reader.Length() < 4) { return false; }
        return Reader.GetASCII(0, 4) == "MSB ";
    }

    namespace {
        MSBE::Entries ListsOf(MSBE& Msb,
                              const std::vector<MSBE::Model*>& Models,
                              const std::vector<MSBE::Event*>& Events,
                              const std::vector<MSBE::Region*>& Regions,
                              const std::vector<MSBE::Part*>& Parts) {
            MSBE::Entries Lists;
            Lists.Models  = Upcast<MSBE::Entry>(Models);
            Lists.Events  = Upcast<MSBE::Entry>(Events);
            Lists.Regions = Upcast<MSBE::Entry>(Regions);
            Lists.Parts   = Upcast<MSBE::Entry>(Parts);
            for (MSBE::PartsParam::Collision& C : Msb.Parts.Collisions) {
                Lists.Collisions.push_back(&C);
            }
            for (MSBE::EventParam::PatrolInfo& P : Msb.Events.PatrolInfos) {
                Lists.PatrolInfos.push_back(&P);
            }
            return Lists;
        }
    }  // namespace

    void MSBE::ReadImpl(BinaryReader& Reader) {
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

        const Msb::Detail::ParamHeader LayerHeader = ReadParamHeader(Reader, std::nullopt, "LAYER_PARAM_ST");
        Layers.Version                             = LayerHeader.Version;
        if (!LayerHeader.EntryOffsets.empty()) {
            throw BinaryException("Expected param \"LAYER_PARAM_ST\" to be empty, but it was not.");
        }
        Reader.Seek(LayerHeader.NextParamOffset);

        const auto PartOrder = Msb::Detail::ReadTypedParam(Reader, Parts, 0xC, std::nullopt, &Parts.Version);
        if (Reader.Position() != 0) {
            throw BinaryException("The next param offset of the final param should be 0, but it was not.");
        }

        const auto ModelPtrs  = Models.EntriesInFileOrder(ModelOrder);
        const auto EventPtrs  = Events.EntriesInFileOrder(EventOrder);
        const auto RegionPtrs = Regions.EntriesInFileOrder(RegionOrder);
        const auto PartPtrs   = Parts.EntriesInFileOrder(PartOrder);
        const Entries Lists   = ListsOf(*this, ModelPtrs, EventPtrs, RegionPtrs, PartPtrs);

        Msb::Detail::DisambiguateNames(Lists.Models);
        Msb::Detail::DisambiguateNames(Lists.Regions);
        Msb::Detail::DisambiguateNames(Lists.Parts);
        Msb::Detail::DisambiguateNames(Lists.Events);
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

    void MSBE::WriteImpl(BinaryWriter& Writer) {
        const std::vector<Model*> ModelPtrs   = Models.GetEntries();
        const std::vector<Event*> EventPtrs   = Events.GetEntries();
        const std::vector<Region*> RegionPtrs = Regions.GetEntries();
        const std::vector<Route*> RoutePtrs   = Routes.GetEntries();
        const std::vector<Part*> PartPtrs     = Parts.GetEntries();
        const Entries Lists                   = ListsOf(*this, ModelPtrs, EventPtrs, RegionPtrs, PartPtrs);

        for (Model* M : ModelPtrs) {
            M->InstanceCount = static_cast<int32_t>(std::count_if(PartPtrs.begin(), PartPtrs.end(), [&](const Part* P) {
                return P->ModelName.has_value() && *P->ModelName == M->Name;
            }));
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
        Writer.Fill<int64_t>("NextParamOffset", 0);
    }
}  // namespace Souls
