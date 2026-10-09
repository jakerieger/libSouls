//
// Created by Jake Rieger on 10/8/2026.
//

// Round-trip tests on hand-built files for formats that can't be checked against real game files here.

#include <libSouls/Formats/ACB.hpp>
#include <libSouls/Formats/BTAB.hpp>
#include <libSouls/Formats/BTPB.hpp>
#include <libSouls/Formats/CLM2.hpp>
#include <libSouls/Formats/EDGE.hpp>
#include <libSouls/Formats/F2TR.hpp>
#include <libSouls/Formats/GRASS.hpp>
#include <libSouls/Formats/NGP.hpp>
#include <libSouls/Formats/FFXDLSE.hpp>
#include <libSouls/Formats/MQB.hpp>
#include <libSouls/Formats/MSB/MSB2.hpp>
#include <libSouls/Formats/TAE3.hpp>
#include <libSouls/Formats/MSB/MSB3.hpp>

#include <cstdio>
#include <string>

namespace {
    int Failures = 0;

#define CHECK(Cond)                                                     \
    do {                                                                \
        if (!(Cond)) {                                                  \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #Cond); \
            ++Failures;                                                 \
        }                                                               \
    } while (0)

    using namespace Souls;

    MSB2 MakeMsb2(MSB2::MSBFormat Format) {
        MSB2 Msb;
        Msb.Format = Format;

        MSB2::ModelParam::MapPiece Piece;
        Piece.Name = "m1000";
        Msb.Models.Add(Piece);
        MSB2::ModelParam::Object Obj;
        Obj.Name = "o1000";
        Msb.Models.Add(Obj);
        MSB2::ModelParam::Collision Col;
        Col.Name = "h1000";
        Msb.Models.Add(Col);
        MSB2::ModelParam::Navmesh Nav;
        Nav.Name = "n1000";
        Msb.Models.Add(Nav);

        MSB2::EventParam::BGColor Bg;
        Bg.EventID = 5;
        Msb.Events.Add(Bg);
        MSB2::EventParam::Warp Warp;
        Warp.EventID  = 6;
        Warp.Position = Vector3(1.f, 2.f, 3.f);
        Msb.Events.Add(Warp);
        MSB2::EventParam::Light Light;
        Light.UnkT04 = 4.f;
        Msb.Events.Add(Light);

        MSB2::PointParam::Region0 R0;
        R0.Name     = "Region A";
        R0.Shape    = Msb::Shapes::Box{2.f, 3.f, 4.f};
        R0.Position = Vector3(10.f, 0.f, -5.f);
        Msb.Regions.Add(R0);
        MSB2::PointParam::Sound Snd;
        Snd.Name    = "Region B";
        Snd.SoundID = 77;
        Snd.Shape   = Msb::Shapes::Sphere{9.f};
        Msb.Regions.Add(Snd);
        MSB2::PointParam::Fog Fog;
        Fog.Name  = "Region C";
        Fog.UnkT04 = 3;
        Msb.Regions.Add(Fog);

        MSB2::PartsParam::MapPiece Mp;
        Mp.Name      = "m1000_0000";
        Mp.ModelName = "m1000";
        Mp.UnkT00    = 12;
        Mp.Position  = Vector3(1.f, 1.f, 1.f);
        Msb.Parts.Add(Mp);
        MSB2::PartsParam::Object Op;
        Op.Name                     = "o1000_0000";
        Op.ModelName                = "o1000";
        Op.MapObjectInstanceParamID = 42;
        Msb.Parts.Add(Op);
        MSB2::PartsParam::Collision Cp;
        Cp.Name      = "h1000_0000";
        Cp.ModelName = "h1000";
        Cp.UnkT04    = 8;
        Msb.Parts.Add(Cp);
        MSB2::PartsParam::Navmesh Np;
        Np.Name      = "n1000_0000";
        Np.ModelName = "n1000";
        Msb.Parts.Add(Np);
        MSB2::PartsParam::ConnectCollision Cc;
        Cc.Name          = "h1000_0001";
        Cc.ModelName     = "h1000";
        Cc.CollisionName = "h1000_0000";
        Cc.MapID         = {10, 1, 0, 0};
        Msb.Parts.Add(Cc);

        MSB2::PartPose Pose;
        Pose.PartName = "o1000_0000";
        MSB2::PartPose::Bone BoneA;
        BoneA.Name        = "Master";
        BoneA.Translation = Vector3(0.f, 1.f, 0.f);
        MSB2::PartPose::Bone BoneB;
        BoneB.Name = "Spine";
        BoneB.Rotation = Vector3(0.5f, 0.f, 0.f);
        Pose.Bones = {BoneA, BoneB};
        Msb.PartPoses.push_back(Pose);
        return Msb;
    }

    void TestMsb2(MSB2::MSBFormat Format, const char* Label) {
        const int Before = Failures;
        const MSB2 Source = MakeMsb2(Format);
        MSB2 Copy         = Source;
        const auto Bytes  = Copy.Write();
        CHECK(MSB2::Is(Bytes));

        const MSB2 Read = MSB2::Read(Bytes);
        CHECK(Read.Format == Format);
        CHECK(Read.Models.GetEntries().size() == 4);
        CHECK(Read.Events.GetEntries().size() == 3);
        CHECK(Read.Regions.GetEntries().size() == 3);
        CHECK(Read.Parts.GetEntries().size() == 5);
        CHECK(Read.Regions.Region0s.size() == 1 && std::holds_alternative<Msb::Shapes::Box>(Read.Regions.Region0s[0].Shape));
        CHECK(Read.Regions.Sounds.size() == 1 && Read.Regions.Sounds[0].SoundID == 77);
        CHECK(Read.Events.Warps.size() == 1 && Read.Events.Warps[0].Position.Y == 2.f);
        CHECK(Read.Parts.MapPieces.size() == 1 && Read.Parts.MapPieces[0].ModelName == "m1000");
        CHECK(Read.Parts.Objects.size() == 1 && Read.Parts.Objects[0].MapObjectInstanceParamID == 42);
        CHECK(Read.Parts.ConnectCollisions.size() == 1 && Read.Parts.ConnectCollisions[0].CollisionName == "h1000_0000");
        CHECK(Read.PartPoses.size() == 1 && Read.PartPoses[0].PartName == "o1000_0000" && Read.PartPoses[0].Bones.size() == 2 &&
              Read.PartPoses[0].Bones[1].Name == "Spine");

        MSB2 Again = Read;
        CHECK(Again.Write() == Bytes);
        if (Failures != Before) {
            std::printf("  (in MSB2 %s)\n", Label);
        }
    }

    void TestFfxdlse() {
        FFXDLSE Ffx;
        Ffx.Effect.ID = 1234;

        auto Spawn = std::make_shared<FFXDLSE::Param37>();
        Spawn->ID  = 55;
        auto Inner = std::make_shared<FFXDLSE::Param7>();
        Inner->Value = 2.5f;
        Spawn->Params->Params.push_back(Inner);
        Ffx.Effect.ParamList1.Params.push_back(Spawn);

        auto Curve = std::make_shared<FFXDLSE::Param9>();
        Curve->Ticks.push_back({0.f, 1.f});
        Curve->Ticks.push_back({10.f, 3.f});
        Ffx.Effect.ParamList1.Params.push_back(Curve);

        auto ColorCurve = std::make_shared<FFXDLSE::Param17>();
        FFXDLSE::TickColor Tc;
        Tc.Tick  = 5.f;
        Tc.Color = FFXDLSE::PrimitiveColor(1.f, 0.5f, 0.25f, 1.f);
        ColorCurve->Ticks.push_back(Tc);
        Ffx.Effect.ParamList2.Params.push_back(ColorCurve);

        auto Scaled = std::make_shared<FFXDLSE::Param82>();
        Scaled->Inner = std::make_shared<FFXDLSE::Param1>();
        Scaled->Float = 0.5f;
        Ffx.Effect.ParamList2.Params.push_back(Scaled);

        FFXDLSE::State State;
        FFXDLSE::Action Action;
        Action.ID = 7;
        auto Arg = std::make_shared<FFXDLSE::Param44>();
        Arg->ArgIndex = 3;
        Action.Params.Params.push_back(Arg);
        State.Actions.push_back(Action);
        FFXDLSE::Trigger Trigger;
        Trigger.StateIndex = 1;
        auto And           = std::make_shared<FFXDLSE::EvaluatableAnd>();
        And->Left          = std::make_shared<FFXDLSE::EvaluatableCurrentTick>();
        auto Not           = std::make_shared<FFXDLSE::EvaluatableNot>();
        auto Constant      = std::make_shared<FFXDLSE::EvaluatableConstant>();
        Constant->Value    = 9;
        Not->Operand       = Constant;
        And->Right         = Not;
        Trigger.Evaluator  = And;
        State.Triggers.push_back(Trigger);
        Ffx.Effect.States.States.push_back(State);
        Ffx.Effect.States.States.push_back(FFXDLSE::State{});
        Ffx.Effect.Resources.Vector3 = {1, 2, 3};

        const auto Bytes = Ffx.Write();
        CHECK(FFXDLSE::Is(Bytes));
        FFXDLSE Read = FFXDLSE::Read(Bytes);
        CHECK(Read.Effect.ID == 1234);
        CHECK(Read.Effect.ParamList1.Params.size() == 2 && Read.Effect.ParamList1.Params[0]->Type() == 37);
        CHECK(Read.Effect.States.States.size() == 2 && Read.Effect.States.States[0].Triggers.size() == 1);
        CHECK(Read.Effect.Resources.Vector3.size() == 3);
        CHECK(Read.Write() == Bytes);
    }

    MQB MakeMqb(MQB::MQBVersion Version, bool BigEndian) {
        MQB Mqb;
        Mqb.Version           = Version;
        Mqb.BigEndian         = BigEndian;
        Mqb.Name              = "TestCutscene";
        Mqb.Framerate         = 30.f;
        Mqb.ResourceDirectory = "N:\\Resources\\";

        MQB::Resource Res;
        Res.Name = "Camera";
        Res.Path = "cut0010\\camera.sibcam";
        MQB::CustomData Flag;
        Flag.Name  = "Enabled";
        Flag.Type  = MQB::CustomData::DataType::Bool;
        Flag.Value = true;
        Res.CustomData.push_back(Flag);
        MQB::CustomData Text;
        Text.Name  = "Label";
        Text.Type  = MQB::CustomData::DataType::String;
        Text.Value = std::string("hello");
        Res.CustomData.push_back(Text);
        MQB::CustomData Tint;
        Tint.Name  = "Tint";
        Tint.Type  = MQB::CustomData::DataType::Color;
        Tint.Value = Color::FromArgb(255, 10, 20, 30);
        Res.CustomData.push_back(Tint);
        Mqb.Resources.push_back(Res);
        MQB::Resource Child;
        Child.Name        = "Child";
        Child.ParentIndex = 0;
        Mqb.Resources.push_back(Child);

        MQB::Cut Cut;
        Cut.Name     = "cut0010";
        Cut.Duration = 120;
        MQB::Timeline Timeline;
        MQB::Disposition Dispos;
        Dispos.ID         = 1;
        Dispos.StartFrame = 0;
        Dispos.Duration   = 120;
        MQB::CustomData Speed;
        Speed.Name  = "Speed";
        Speed.Type  = MQB::CustomData::DataType::Float;
        Speed.Value = 1.5f;
        MQB::CustomData::Sequence Seq;
        Seq.ValueType = MQB::CustomData::DataType::Float;
        Seq.PointType = 2;
        MQB::CustomData::Point P0;
        P0.Value = 0.5f;
        P0.Unk10 = 1.f;
        Seq.Points.push_back(P0);
        Speed.Sequences.push_back(Seq);
        Dispos.CustomData.push_back(Speed);
        MQB::Transform Tr;
        Tr.Frame       = 12.f;
        Tr.Translation = Vector3(1.f, 2.f, 3.f);
        Dispos.Transforms.push_back(Tr);
        Timeline.Dispositions.push_back(Dispos);
        MQB::CustomData Counter;
        Counter.Name  = "Counter";
        Counter.Type  = MQB::CustomData::DataType::Int;
        Counter.Value = int32_t{7};
        Timeline.CustomData.push_back(Counter);
        Cut.Timelines.push_back(Timeline);
        Mqb.Cuts.push_back(Cut);
        return Mqb;
    }

    void TestMqb(MQB::MQBVersion Version, bool BigEndian, const char* Label) {
        const int Before = Failures;
        MQB Source       = MakeMqb(Version, BigEndian);
        const auto Bytes = Source.Write();
        CHECK(MQB::Is(Bytes));
        MQB Read = MQB::Read(Bytes);
        CHECK(Read.Version == Version);
        CHECK(Read.Name == "TestCutscene");
        CHECK(Read.Resources.size() == 2 && Read.Resources[0].CustomData.size() == 3);
        CHECK(Read.Resources[0].Path == "cut0010\\camera.sibcam" && !Read.Resources[1].Path);
        CHECK(Read.Cuts.size() == 1 && Read.Cuts[0].Timelines.size() == 1 && Read.Cuts[0].Timelines[0].Dispositions.size() == 1);
        CHECK(Read.Cuts[0].Timelines[0].Dispositions[0].CustomData[0].Sequences.size() == 1);
        CHECK(Read.Write() == Bytes);
        if (Failures != Before) {
            std::printf("  (in MQB %s)\n", Label);
        }
    }

    void TestMsb3() {
        MSB3 Msb;
        MSB3::ModelParam::MapPiece Mp;
        Mp.Name = "m1000";
        Msb.Models.Add(Mp);
        MSB3::ModelParam::Collision Col;
        Col.Name = "h1000";
        Msb.Models.Add(Col);
        MSB3::ModelParam::Player Pl;
        Pl.Name = "c0000";
        Msb.Models.Add(Pl);

        MSB3::EventParam::MapOffset Off;
        Off.Name     = "Offset";
        Off.Position = Vector3(1.f, 2.f, 3.f);
        Msb.Events.Add(Off);

        MSB3::PointParam::Event Ev;
        Ev.Name     = "Region A";
        Ev.Shape    = Msb::Shapes::Box{4.f, 5.f, 6.f};
        Ev.Position = Vector3(9.f, 8.f, 7.f);
        Msb.Regions.Add(Ev);
        MSB3::PointParam::PatrolRoute Route;
        Route.Name = "Region B";
        Msb.Regions.Add(Route);

        MSB3::PartsParam::MapPiece MapPiece;
        MapPiece.Name      = "m1000_0000";
        MapPiece.ModelName = "m1000";
        Msb.Parts.Add(MapPiece);
        MSB3::PartsParam::Collision CollisionPart;
        CollisionPart.Name      = "h1000_0000";
        CollisionPart.ModelName = "h1000";
        Msb.Parts.Add(CollisionPart);
        MSB3::PartsParam::Player Player;
        Player.Name      = "c0000_0000";
        Player.ModelName = "c0000";
        Msb.Parts.Add(Player);

        const auto Bytes = Msb.Write();
        CHECK(MSB3::Is(Bytes));
        MSB3 Read = MSB3::Read(Bytes);
        CHECK(Read.Models.GetEntries().size() == 3);
        CHECK(Read.Events.MapOffsets.size() == 1 && Read.Events.MapOffsets[0].Position.Z == 3.f);
        CHECK(Read.Regions.GetEntries().size() == 2);
        CHECK(Read.Parts.GetEntries().size() == 3);
        CHECK(Read.Parts.MapPieces.size() == 1 && Read.Parts.MapPieces[0].ModelName == "m1000");
        CHECK(Read.Write() == Bytes);
    }

    void TestTae3() {
        TAE3 Tae;
        Tae.ID           = 1234;
        Tae.SkeletonName = "skeleton.hkt";
        Tae.SibName      = "c0000.sib";

        TAE3::Animation A;
        A.ID = 5;
        TAE3::Event JumpEvent;
        JumpEvent.StartTime = 0.5f;
        JumpEvent.EndTime   = 1.5f;
        TAE3Events::JumpTable Jump;
        Jump.JumpTableID = 9;
        JumpEvent.Data   = Jump;
        A.Events.push_back(JumpEvent);
        TAE3::Event OtherEvent;
        OtherEvent.StartTime = 2.f;
        OtherEvent.EndTime   = 3.f;
        TAE3Events::Unk001 Other;
        Other.Condition = 4;
        OtherEvent.Data = Other;
        A.Events.push_back(OtherEvent);
        TAE3::EventGroup Group;
        Group.Type    = TAE3EventType::JumpTable;
        Group.Indices = {0};
        A.EventGroups.push_back(Group);
        Tae.Animations.push_back(A);

        TAE3::Animation Ref;
        Ref.ID                = 6;
        Ref.AnimFileReference = true;
        Ref.AnimFileName      = "a000_003000.hkt";
        Tae.Animations.push_back(Ref);

        const auto Bytes = Tae.Write();
        CHECK(TAE3::Is(Bytes));
        TAE3 Read = TAE3::Read(Bytes);
        CHECK(Read.ID == 1234 && Read.SkeletonName == "skeleton.hkt");
        CHECK(Read.Animations.size() == 2 && Read.Animations[0].Events.size() == 2 && Read.Animations[0].EventGroups.size() == 1);
        CHECK(Read.Animations[1].AnimFileReference && Read.Animations[1].AnimFileName == "a000_003000.hkt");
        CHECK(Read.Write() == Bytes);
    }

    // An empty file of the format must survive writing, reading and writing again.
    template<typename T>
    void TestEmpty(const char* Label) {
        try {
            T Empty;
            const auto Bytes = Empty.Write();
            T Read           = T::Read(Bytes);
            const bool Same  = Read.Write() == Bytes;
            if (!Same) {
                std::printf("FAIL %s: empty file didn't round-trip\n", Label);
                ++Failures;
            }
        } catch (const std::exception& E) {
            std::printf("FAIL %s: %s\n", Label, E.what());
            ++Failures;
        }
    }
}  // namespace

int RunSyntheticFormatTests() {
    TestEmpty<ACB>("ACB");
    TestEmpty<BTAB>("BTAB");
    TestEmpty<BTPB>("BTPB");
    TestEmpty<CLM2>("CLM2");
    TestEmpty<EDGE>("EDGE");
    TestEmpty<F2TR>("F2TR");
    TestEmpty<GRASS>("GRASS");
    TestEmpty<NGP>("NGP");
    TestTae3();
    TestMsb3();
    TestMqb(MQB::MQBVersion::DarkSouls3, false, "DS3");
    TestMqb(MQB::MQBVersion::DarkSouls2, false, "DS2");
    TestMqb(MQB::MQBVersion::DarkSouls2Scholar, false, "Scholar");
    TestMqb(MQB::MQBVersion::Bloodborne, true, "BB");
    TestFfxdlse();
    TestMsb2(MSB2::MSBFormat::DarkSouls2Scholar, "Scholar");
    TestMsb2(MSB2::MSBFormat::DarkSouls2LE, "LE");
    TestMsb2(MSB2::MSBFormat::DarkSouls2BE, "BE");
    std::printf("Synthetic format tests: %s\n", Failures ? "FAILED" : "ok");
    return Failures ? 1 : 0;
}
