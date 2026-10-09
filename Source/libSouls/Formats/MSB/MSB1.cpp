//
// Created by Jake Rieger on 10/8/2026.
//

#include "MSB1.hpp"

#include <libSouls/TextEncoding.hpp>

#include <algorithm>
#include <cctype>

namespace Souls {
    namespace Msb::Detail {
        std::string ReambiguateName(const std::string& Name) {
            // Removes every " {digits}".
            std::string Result;
            for (size_t I = 0; I < Name.size();) {
                if (Name[I] == ' ' && I + 2 < Name.size() && Name[I + 1] == '{') {
                    size_t J = I + 2;
                    while (J < Name.size() && std::isdigit(static_cast<unsigned char>(Name[J]))) {
                        ++J;
                    }
                    if (J > I + 2 && J < Name.size() && Name[J] == '}') {
                        I = J + 1;
                        continue;
                    }
                }
                Result.push_back(Name[I]);
                ++I;
            }
            return Result;
        }
    }  // namespace Msb::Detail

    namespace {
        using Msb::Detail::ReambiguateName;

        void RequireNonZero(int32_t Offset, const char* What) {
            if (Offset == 0) {
                throw BinaryException(std::string(What) + " must not be 0.");
            }
        }

        // Reads a param's header and entries; returns the order the entries were read in. TypeOffset is where the
        // entry's type number is relative to the start of the entry, or -1 if the param has only one type.
        template<typename ParamT>
        typename ParamT::FileOrder ReadParam(BinaryReader& Reader, ParamT& Param, int TypeOffset) {
            Reader.Assert<int32_t>(0);
            const int32_t NameOffset  = Reader.ReadInt32();
            const int32_t OffsetCount = Reader.ReadInt32();
            if (OffsetCount < 1) {
                throw BinaryException("Invalid MSB param entry count");
            }
            const std::vector<int32_t> EntryOffsets = Reader.ReadArray<int32_t>(static_cast<size_t>(OffsetCount - 1));
            const int32_t NextParamOffset           = Reader.ReadInt32();
            const std::string Name                  = Reader.GetCString(NameOffset);
            if (Name != ParamT::ParamName) {
                throw BinaryException(std::string("Expected param \"") + ParamT::ParamName + "\", got param \"" + Name + "\"");
            }

            typename ParamT::FileOrder Order;
            for (const int32_t Offset : EntryOffsets) {
                Reader.Seek(Offset);
                const uint32_t Type = TypeOffset < 0 ? 0 : Reader.ReadAt<uint32_t>(Reader.Position() + TypeOffset);
                if (!Param.ReadEntryOfType(Type, Reader, Order)) {
                    throw BinaryException("Unimplemented MSB entry type: " + std::to_string(Type));
                }
            }
            Reader.Seek(NextParamOffset);
            return Order;
        }

        template<typename EntryT>
        void WriteParam(BinaryWriter& Writer, const char* Name, const std::vector<EntryT*>& Entries) {
            Writer.WriteInt32(0);
            Writer.Reserve<int32_t>("ParamNameOffset");
            Writer.WriteInt32(static_cast<int32_t>(Entries.size()) + 1);
            for (size_t I = 0; I < Entries.size(); ++I) {
                Writer.Reserve<int32_t>("EntryOffset" + std::to_string(I));
            }
            Writer.Reserve<int32_t>("NextParamOffset");
            Writer.Fill<int32_t>("ParamNameOffset", static_cast<int32_t>(Writer.Position()));
            Writer.WriteString(Name, true);
            Writer.Align(4);

            int32_t Id = 0;
            std::optional<uint32_t> CurrentType;
            for (size_t I = 0; I < Entries.size(); ++I) {
                if (!CurrentType || *CurrentType != Entries[I]->Type()) {
                    CurrentType = Entries[I]->Type();
                    Id          = 0;
                }
                Writer.Fill<int32_t>("EntryOffset" + std::to_string(I), static_cast<int32_t>(Writer.Position()));
                Entries[I]->Write(Writer, Id);
                ++Id;
            }
        }

        template<typename Base, typename Derived>
        std::vector<Base*> Upcast(const std::vector<Derived*>& Items) {
            return std::vector<Base*>(Items.begin(), Items.end());
        }
    }  // namespace

#pragma region Models
    void MSB1::Model::Read(BinaryReader& Reader) {
        const int64_t Start = Reader.Position();
        const int32_t NameOffset = Reader.ReadInt32();
        Reader.Assert<uint32_t>(Type());
        Reader.ReadInt32();  // ID
        const int32_t SibOffset = Reader.ReadInt32();
        InstanceCount           = Reader.ReadInt32();
        Reader.Assert<int32_t>(0);
        Reader.Assert<int32_t>(0);
        Reader.Assert<int32_t>(0);
        RequireNonZero(NameOffset, "nameOffset");
        RequireNonZero(SibOffset, "sibOffset");
        Reader.Seek(Start + NameOffset);
        Name = Reader.ReadShiftJIS();
        Reader.Seek(Start + SibOffset);
        SibPath = Reader.ReadShiftJIS();
    }

    void MSB1::Model::Write(BinaryWriter& Writer, int32_t ID) {
        const int64_t Start = Writer.Position();
        Writer.Reserve<int32_t>("NameOffset");
        Writer.WriteUInt32(Type());
        Writer.WriteInt32(ID);
        Writer.Reserve<int32_t>("SibOffset");
        Writer.WriteInt32(InstanceCount);
        Writer.WriteInt32(0);
        Writer.WriteInt32(0);
        Writer.WriteInt32(0);
        Writer.Fill<int32_t>("NameOffset", static_cast<int32_t>(Writer.Position() - Start));
        Writer.WriteShiftJIS(ReambiguateName(Name), true);
        Writer.Fill<int32_t>("SibOffset", static_cast<int32_t>(Writer.Position() - Start));
        Writer.WriteShiftJIS(SibPath, true);
        Writer.Align(4);
    }
#pragma endregion

#pragma region Regions
    void MSB1::Region::Read(BinaryReader& Reader) {
        const int64_t Start      = Reader.Position();
        const int32_t NameOffset = Reader.ReadInt32();
        Reader.Assert<int32_t>(0);
        Reader.ReadInt32();  // ID
        const auto ShapeKind = static_cast<Msb::ShapeType>(Reader.ReadUInt32());
        Position             = Reader.ReadVector3();
        Rotation             = Reader.ReadVector3();
        const int32_t UnkOffsetA      = Reader.ReadInt32();
        const int32_t UnkOffsetB      = Reader.ReadInt32();
        const int32_t ShapeDataOffset = Reader.ReadInt32();
        const int32_t EntityDataOffset = Reader.ReadInt32();
        Reader.Assert<int32_t>(0);
        Shape = Msb::CreateShape(ShapeKind);
        RequireNonZero(NameOffset, "nameOffset");
        RequireNonZero(UnkOffsetA, "unkOffsetA");
        RequireNonZero(UnkOffsetB, "unkOffsetB");
        if (Msb::HasShapeData(Shape) != (ShapeDataOffset != 0)) {
            throw BinaryException("Unexpected shapeDataOffset in region");
        }

        Reader.Seek(Start + NameOffset);
        Name = Reader.ReadShiftJIS();
        Reader.Seek(Start + UnkOffsetA);
        Reader.Assert<int32_t>(0);
        Reader.Seek(Start + UnkOffsetB);
        Reader.Assert<int32_t>(0);
        if (Msb::HasShapeData(Shape)) {
            Reader.Seek(Start + ShapeDataOffset);
            Msb::Detail::ReadVisitor V{Reader};
            Msb::Detail::VisitShape(Shape, V);
        }
        Reader.Seek(Start + EntityDataOffset);
        EntityID = Reader.ReadInt32();
    }

    void MSB1::Region::Write(BinaryWriter& Writer, int32_t ID) {
        const int64_t Start = Writer.Position();
        Writer.Reserve<int32_t>("NameOffset");
        Writer.WriteInt32(0);
        Writer.WriteInt32(ID);
        Writer.WriteUInt32(static_cast<uint32_t>(Msb::TypeOf(Shape)));
        Writer.WriteVector3(Position);
        Writer.WriteVector3(Rotation);
        Writer.Reserve<int32_t>("UnkOffsetA");
        Writer.Reserve<int32_t>("UnkOffsetB");
        Writer.Reserve<int32_t>("ShapeDataOffset");
        Writer.Reserve<int32_t>("EntityDataOffset");
        Writer.WriteInt32(0);

        Writer.Fill<int32_t>("NameOffset", static_cast<int32_t>(Writer.Position() - Start));
        Writer.WriteShiftJIS(ReambiguateName(Name), true);
        Writer.Align(4);
        Writer.Fill<int32_t>("UnkOffsetA", static_cast<int32_t>(Writer.Position() - Start));
        Writer.WriteInt32(0);
        Writer.Fill<int32_t>("UnkOffsetB", static_cast<int32_t>(Writer.Position() - Start));
        Writer.WriteInt32(0);
        if (Msb::HasShapeData(Shape)) {
            Writer.Fill<int32_t>("ShapeDataOffset", static_cast<int32_t>(Writer.Position() - Start));
            Msb::Detail::WriteVisitor V{Writer};
            Msb::Detail::VisitShape(Shape, V);
        } else {
            Writer.Fill<int32_t>("ShapeDataOffset", 0);
        }
        Writer.Fill<int32_t>("EntityDataOffset", static_cast<int32_t>(Writer.Position() - Start));
        Writer.WriteInt32(EntityID);
    }
#pragma endregion

#pragma region Events
    void MSB1::Event::Read(BinaryReader& Reader) {
        const int64_t Start      = Reader.Position();
        const int32_t NameOffset = Reader.ReadInt32();
        EventID                  = Reader.ReadInt32();
        Reader.Assert<uint32_t>(Type());
        Reader.ReadInt32();  // ID
        const int32_t BaseDataOffset = Reader.ReadInt32();
        const int32_t TypeDataOffset = Reader.ReadInt32();
        Reader.Assert<int32_t>(0);
        RequireNonZero(NameOffset, "nameOffset");
        RequireNonZero(BaseDataOffset, "baseDataOffset");
        RequireNonZero(TypeDataOffset, "typeDataOffset");

        Reader.Seek(Start + NameOffset);
        Name = Reader.ReadShiftJIS();
        Reader.Seek(Start + BaseDataOffset);
        PartName.Index   = Reader.ReadInt32();
        RegionName.Index = Reader.ReadInt32();
        EntityID         = Reader.ReadInt32();
        Reader.Assert<int32_t>(0);
        Reader.Seek(Start + TypeDataOffset);
        ReadTypeData(Reader);
    }

    void MSB1::Event::Write(BinaryWriter& Writer, int32_t ID) {
        const int64_t Start = Writer.Position();
        Writer.Reserve<int32_t>("NameOffset");
        Writer.WriteInt32(EventID);
        Writer.WriteUInt32(Type());
        Writer.WriteInt32(ID);
        Writer.Reserve<int32_t>("BaseDataOffset");
        Writer.Reserve<int32_t>("TypeDataOffset");
        Writer.WriteInt32(0);
        Writer.Fill<int32_t>("NameOffset", static_cast<int32_t>(Writer.Position() - Start));
        Writer.WriteShiftJIS(Name, true);
        Writer.Align(4);
        Writer.Fill<int32_t>("BaseDataOffset", static_cast<int32_t>(Writer.Position() - Start));
        Writer.WriteInt32(PartName.Index);
        Writer.WriteInt32(RegionName.Index);
        Writer.WriteInt32(EntityID);
        Writer.WriteInt32(0);
        Writer.Fill<int32_t>("TypeDataOffset", static_cast<int32_t>(Writer.Position() - Start));
        WriteTypeData(Writer);
    }

    void MSB1::Event::GetNames(const Entries& Lists) {
        Msb::Detail::NameVisitor<Entries> V{Lists};
        V(PartName);
        V(RegionName);
    }

    void MSB1::Event::GetIndices(const Entries& Lists) {
        Msb::Detail::IndexVisitor<Entries> V{Lists};
        V(PartName);
        V(RegionName);
    }
#pragma endregion

#pragma region Parts
    void MSB1::Part::Read(BinaryReader& Reader) {
        const int64_t Start      = Reader.Position();
        const int32_t NameOffset = Reader.ReadInt32();
        Reader.Assert<uint32_t>(Type());
        Reader.ReadInt32();  // ID
        ModelName.Index         = Reader.ReadInt32();
        const int32_t SibOffset = Reader.ReadInt32();
        Position                = Reader.ReadVector3();
        Rotation                = Reader.ReadVector3();
        Scale                   = Reader.ReadVector3();
        Reader.ReadInto(std::span<uint32_t>(DrawGroups));
        Reader.ReadInto(std::span<uint32_t>(DispGroups));
        const int32_t EntityDataOffset = Reader.ReadInt32();
        const int32_t TypeDataOffset   = Reader.ReadInt32();
        Reader.Assert<int32_t>(0);
        RequireNonZero(NameOffset, "nameOffset");
        RequireNonZero(SibOffset, "sibOffset");
        RequireNonZero(EntityDataOffset, "entityDataOffset");
        RequireNonZero(TypeDataOffset, "typeDataOffset");

        Reader.Seek(Start + NameOffset);
        Name = Reader.ReadShiftJIS();
        Reader.Seek(Start + SibOffset);
        SibPath = Reader.ReadShiftJIS();

        Reader.Seek(Start + EntityDataOffset);
        EntityID    = Reader.ReadInt32();
        LightID     = Reader.ReadByte();
        FogID       = Reader.ReadByte();
        ScatterID   = Reader.ReadByte();
        LensFlareID = Reader.ReadByte();
        ShadowID    = Reader.ReadByte();
        DofID       = Reader.ReadByte();
        ToneMapID   = Reader.ReadByte();
        ToneCorrectID = Reader.ReadByte();
        LanternID   = Reader.ReadByte();
        LodParamID  = Reader.ReadByte();
        Reader.Assert<uint8_t>(0);
        IsShadowSrc        = Reader.ReadByte();
        IsShadowDest       = Reader.ReadByte();
        IsShadowOnly       = Reader.ReadByte();
        DrawByReflectCam   = Reader.ReadByte();
        DrawOnlyReflectCam = Reader.ReadByte();
        UseDepthBiasFloat  = Reader.ReadByte();
        DisablePointLightEffect = Reader.ReadByte();
        Reader.Assert<uint8_t>(0);
        Reader.Assert<uint8_t>(0);

        Reader.Seek(Start + TypeDataOffset);
        ReadTypeData(Reader);
    }

    void MSB1::Part::Write(BinaryWriter& Writer, int32_t ID) {
        const int64_t Start = Writer.Position();
        Writer.Reserve<int32_t>("NameOffset");
        Writer.WriteUInt32(Type());
        Writer.WriteInt32(ID);
        Writer.WriteInt32(ModelName.Index);
        Writer.Reserve<int32_t>("SibOffset");
        Writer.WriteVector3(Position);
        Writer.WriteVector3(Rotation);
        Writer.WriteVector3(Scale);
        Writer.WriteArray(std::span<const uint32_t>(DrawGroups));
        Writer.WriteArray(std::span<const uint32_t>(DispGroups));
        Writer.Reserve<int32_t>("EntityDataOffset");
        Writer.Reserve<int32_t>("TypeDataOffset");
        Writer.WriteInt32(0);

        const int64_t StringsStart = Writer.Position();
        Writer.Fill<int32_t>("NameOffset", static_cast<int32_t>(Writer.Position() - Start));
        Writer.WriteShiftJIS(ReambiguateName(Name), true);
        Writer.Fill<int32_t>("SibOffset", static_cast<int32_t>(Writer.Position() - Start));
        Writer.WriteShiftJIS(SibPath, true);
        Writer.Align(4);
        if (Writer.Position() - StringsStart < 0x14) {
            Writer.Pad(static_cast<size_t>(0x14 - (Writer.Position() - StringsStart)));
        }

        Writer.Fill<int32_t>("EntityDataOffset", static_cast<int32_t>(Writer.Position() - Start));
        Writer.WriteInt32(EntityID);
        Writer.WriteByte(LightID);
        Writer.WriteByte(FogID);
        Writer.WriteByte(ScatterID);
        Writer.WriteByte(LensFlareID);
        Writer.WriteByte(ShadowID);
        Writer.WriteByte(DofID);
        Writer.WriteByte(ToneMapID);
        Writer.WriteByte(ToneCorrectID);
        Writer.WriteByte(LanternID);
        Writer.WriteByte(LodParamID);
        Writer.WriteByte(0);
        Writer.WriteByte(IsShadowSrc);
        Writer.WriteByte(IsShadowDest);
        Writer.WriteByte(IsShadowOnly);
        Writer.WriteByte(DrawByReflectCam);
        Writer.WriteByte(DrawOnlyReflectCam);
        Writer.WriteByte(UseDepthBiasFloat);
        Writer.WriteByte(DisablePointLightEffect);
        Writer.WriteByte(0);
        Writer.WriteByte(0);

        Writer.Fill<int32_t>("TypeDataOffset", static_cast<int32_t>(Writer.Position() - Start));
        WriteTypeData(Writer);
    }

    void MSB1::Part::GetNames(const Entries& Lists) {
        Msb::Detail::NameVisitor<Entries> V{Lists};
        V(ModelName);
    }

    void MSB1::Part::GetIndices(const Entries& Lists) {
        Msb::Detail::IndexVisitor<Entries> V{Lists};
        V(ModelName);
    }
#pragma endregion

    void MSB1::ReadImpl(BinaryReader& Reader) {
        Reader.Order = Endian::Little;
        BigEndian    = Reader.ReadAt<uint32_t>(4) > 0xFFFF;
        Reader.Order = BigEndian ? Endian::Big : Endian::Little;

        Models  = {};
        Events  = {};
        Regions = {};
        Parts   = {};

        const auto ModelOrder  = ReadParam(Reader, Models, 4);
        const auto EventOrder  = ReadParam(Reader, Events, 8);
        const auto RegionOrder = ReadParam(Reader, Regions, -1);
        const auto PartOrder   = ReadParam(Reader, Parts, 4);
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

        Msb::Detail::DisambiguateNames(Lists.Models);
        Msb::Detail::DisambiguateNames(Lists.Regions);
        Msb::Detail::DisambiguateNames(Lists.Parts);
        for (Event* E : EventPtrs) {
            E->GetNames(Lists);
        }
        for (Part* P : PartPtrs) {
            P->GetNames(Lists);
        }
    }

    void MSB1::WriteImpl(BinaryWriter& Writer) {
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

        for (Model* M : ModelPtrs) {
            M->InstanceCount = static_cast<int32_t>(
                std::count_if(PartPtrs.begin(), PartPtrs.end(), [&](const Part* P) { return P->ModelName.has_value() && *P->ModelName == M->Name; }));
        }
        for (Event* E : EventPtrs) {
            E->GetIndices(Lists);
        }
        for (Part* P : PartPtrs) {
            P->GetIndices(Lists);
        }

        Writer.Order = BigEndian ? Endian::Big : Endian::Little;
        WriteParam(Writer, ModelParam::ParamName, ModelPtrs);
        Writer.Fill<int32_t>("NextParamOffset", static_cast<int32_t>(Writer.Position()));
        WriteParam(Writer, EventParam::ParamName, EventPtrs);
        Writer.Fill<int32_t>("NextParamOffset", static_cast<int32_t>(Writer.Position()));
        WriteParam(Writer, PointParam::ParamName, RegionPtrs);
        Writer.Fill<int32_t>("NextParamOffset", static_cast<int32_t>(Writer.Position()));
        WriteParam(Writer, PartsParam::ParamName, PartPtrs);
        Writer.Fill<int32_t>("NextParamOffset", 0);
    }
}  // namespace Souls
