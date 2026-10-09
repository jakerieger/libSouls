//
// Created by Jake Rieger on 10/8/2026.
//

#include "FFXDLSE.hpp"

#include <algorithm>
#include <cstdio>

namespace Souls {
    namespace {
        template<typename T>
        std::shared_ptr<T> ReadAs(BinaryReader& Reader, const FFXDLSE::ClassNames& Names) {
            auto Result = std::make_shared<T>();
            Result->Read(Reader, Names);
            return Result;
        }

        std::vector<int32_t> ReadVector(BinaryReader& Reader, const FFXDLSE::ClassNames& Names) {
            Reader.Assert<int16_t>(FFXDLSE::ClassIndex(Names, "DLVector"));
            const int32_t Count = Reader.ReadInt32();
            return Reader.ReadArray<int32_t>(static_cast<size_t>(Count));
        }

        void WriteVector(BinaryWriter& Writer, const FFXDLSE::ClassNames& Names, const std::vector<int32_t>& Values) {
            Writer.WriteInt16(FFXDLSE::ClassIndex(Names, "DLVector"));
            Writer.WriteInt32(static_cast<int32_t>(Values.size()));
            Writer.WriteArray(Values);
        }
    }  // namespace

    int16_t FFXDLSE::ClassIndex(const ClassNames& Names, std::string_view Name) {
        const auto Found = std::find(Names.begin(), Names.end(), Name);
        return static_cast<int16_t>(Found == Names.end() ? 0 : (Found - Names.begin()) + 1);
    }

    void FFXDLSE::AddClassName(ClassNames& Names, std::string_view Name) {
        if (std::find(Names.begin(), Names.end(), Name) == Names.end()) {
            Names.emplace_back(Name);
        }
    }

#pragma region Serializable
    void FFXDLSE::Serializable::AddClassNames(ClassNames& Names) const {
        AddClassName(Names, ClassName());
    }

    void FFXDLSE::Serializable::Read(BinaryReader& Reader, const ClassNames& Names) {
        const int64_t Start = Reader.Position();
        Reader.Assert<int16_t>(ClassIndex(Names, ClassName()));
        Reader.Assert<int32_t>(Version());
        const int32_t Length = Reader.ReadInt32();
        Deserialize(Reader, Names);
        if (Reader.Position() != Start + Length) {
            throw BinaryException("Failed to read all object data (or read too much of it).");
        }
    }

    void FFXDLSE::Serializable::Write(BinaryWriter& Writer, const ClassNames& Names) const {
        const int64_t Start = Writer.Position();
        char Label[40];
        std::snprintf(Label, sizeof(Label), "%llXLength", static_cast<unsigned long long>(Start));
        Writer.WriteInt16(ClassIndex(Names, ClassName()));
        Writer.WriteInt32(Version());
        Writer.Reserve<int32_t>(Label);
        Serialize(Writer, Names);
        Writer.Fill<int32_t>(Label, static_cast<int32_t>(Writer.Position() - Start));
    }
#pragma endregion

#pragma region Primitives
    int32_t FFXDLSE::PrimitiveTraits<FFXDLSE::PrimitiveKind::Int>::Read(BinaryReader& Reader, const ClassNames& Names) {
        struct P : Serializable {
            int32_t Value = 0;
            const char* ClassName() const override { return Name; }
            int32_t Version() const override { return 1; }
            void Deserialize(BinaryReader& R, const ClassNames&) override { Value = R.ReadInt32(); }
            void Serialize(BinaryWriter& W, const ClassNames&) const override { W.WriteInt32(Value); }
        } Object;
        Object.Read(Reader, Names);
        return Object.Value;
    }

    void FFXDLSE::PrimitiveTraits<FFXDLSE::PrimitiveKind::Int>::Write(BinaryWriter& Writer, const ClassNames& Names, int32_t Value) {
        struct P : Serializable {
            int32_t Value = 0;
            const char* ClassName() const override { return Name; }
            int32_t Version() const override { return 1; }
            void Deserialize(BinaryReader& R, const ClassNames&) override { Value = R.ReadInt32(); }
            void Serialize(BinaryWriter& W, const ClassNames&) const override { W.WriteInt32(Value); }
        } Object;
        Object.Value = Value;
        Object.Write(Writer, Names);
    }

    float FFXDLSE::PrimitiveTraits<FFXDLSE::PrimitiveKind::Float>::Read(BinaryReader& Reader, const ClassNames& Names) {
        struct P : Serializable {
            float Value = 0;
            const char* ClassName() const override { return Name; }
            int32_t Version() const override { return 1; }
            void Deserialize(BinaryReader& R, const ClassNames&) override { Value = R.ReadFloat(); }
            void Serialize(BinaryWriter& W, const ClassNames&) const override { W.WriteFloat(Value); }
        } Object;
        Object.Read(Reader, Names);
        return Object.Value;
    }

    void FFXDLSE::PrimitiveTraits<FFXDLSE::PrimitiveKind::Float>::Write(BinaryWriter& Writer, const ClassNames& Names, float Value) {
        struct P : Serializable {
            float Value = 0;
            const char* ClassName() const override { return Name; }
            int32_t Version() const override { return 1; }
            void Deserialize(BinaryReader& R, const ClassNames&) override { Value = R.ReadFloat(); }
            void Serialize(BinaryWriter& W, const ClassNames&) const override { W.WriteFloat(Value); }
        } Object;
        Object.Value = Value;
        Object.Write(Writer, Names);
    }

    float FFXDLSE::PrimitiveTraits<FFXDLSE::PrimitiveKind::Tick>::Read(BinaryReader& Reader, const ClassNames& Names) {
        struct P : Serializable {
            float Value = 0;
            const char* ClassName() const override { return Name; }
            int32_t Version() const override { return 1; }
            void Deserialize(BinaryReader& R, const ClassNames&) override { Value = R.ReadFloat(); }
            void Serialize(BinaryWriter& W, const ClassNames&) const override { W.WriteFloat(Value); }
        } Object;
        Object.Read(Reader, Names);
        return Object.Value;
    }

    void FFXDLSE::PrimitiveTraits<FFXDLSE::PrimitiveKind::Tick>::Write(BinaryWriter& Writer, const ClassNames& Names, float Value) {
        struct P : Serializable {
            float Value = 0;
            const char* ClassName() const override { return Name; }
            int32_t Version() const override { return 1; }
            void Deserialize(BinaryReader& R, const ClassNames&) override { Value = R.ReadFloat(); }
            void Serialize(BinaryWriter& W, const ClassNames&) const override { W.WriteFloat(Value); }
        } Object;
        Object.Value = Value;
        Object.Write(Writer, Names);
    }

    void FFXDLSE::PrimitiveColor::Deserialize(BinaryReader& Reader, const ClassNames&) {
        R = Reader.ReadFloat();
        G = Reader.ReadFloat();
        B = Reader.ReadFloat();
        A = Reader.ReadFloat();
    }

    void FFXDLSE::PrimitiveColor::Serialize(BinaryWriter& Writer, const ClassNames&) const {
        Writer.WriteFloat(R);
        Writer.WriteFloat(G);
        Writer.WriteFloat(B);
        Writer.WriteFloat(A);
    }
#pragma endregion

#pragma region Tick values
    using IntTraits   = FFXDLSE::PrimitiveTraits<FFXDLSE::PrimitiveKind::Int>;
    using FloatTraits = FFXDLSE::PrimitiveTraits<FFXDLSE::PrimitiveKind::Float>;
    using TickTraits  = FFXDLSE::PrimitiveTraits<FFXDLSE::PrimitiveKind::Tick>;

    void FFXDLSE::TickInt::Read(BinaryReader& Reader, const ClassNames& Names) {
        Tick = TickTraits::Read(Reader, Names);
        Int  = IntTraits::Read(Reader, Names);
    }
    void FFXDLSE::TickInt::Write(BinaryWriter& Writer, const ClassNames& Names) const {
        TickTraits::Write(Writer, Names, Tick);
        IntTraits::Write(Writer, Names, Int);
    }
    void FFXDLSE::TickInt::AddClassNames(ClassNames& Names) const {
        TickTraits::AddName(Names);
        IntTraits::AddName(Names);
    }

    void FFXDLSE::TickFloat::Read(BinaryReader& Reader, const ClassNames& Names) {
        Tick  = TickTraits::Read(Reader, Names);
        Float = FloatTraits::Read(Reader, Names);
    }
    void FFXDLSE::TickFloat::Write(BinaryWriter& Writer, const ClassNames& Names) const {
        TickTraits::Write(Writer, Names, Tick);
        FloatTraits::Write(Writer, Names, Float);
    }
    void FFXDLSE::TickFloat::AddClassNames(ClassNames& Names) const {
        TickTraits::AddName(Names);
        FloatTraits::AddName(Names);
    }

    void FFXDLSE::TickFloat3::Read(BinaryReader& Reader, const ClassNames& Names) {
        Tick   = TickTraits::Read(Reader, Names);
        Float1 = FloatTraits::Read(Reader, Names);
        Float2 = FloatTraits::Read(Reader, Names);
        Float3 = FloatTraits::Read(Reader, Names);
    }
    void FFXDLSE::TickFloat3::Write(BinaryWriter& Writer, const ClassNames& Names) const {
        TickTraits::Write(Writer, Names, Tick);
        FloatTraits::Write(Writer, Names, Float1);
        FloatTraits::Write(Writer, Names, Float2);
        FloatTraits::Write(Writer, Names, Float3);
    }
    void FFXDLSE::TickFloat3::AddClassNames(ClassNames& Names) const {
        TickTraits::AddName(Names);
        FloatTraits::AddName(Names);
    }

    void FFXDLSE::TickColor::Read(BinaryReader& Reader, const ClassNames& Names) {
        Tick = TickTraits::Read(Reader, Names);
        Color.Read(Reader, Names);
    }
    void FFXDLSE::TickColor::Write(BinaryWriter& Writer, const ClassNames& Names) const {
        TickTraits::Write(Writer, Names, Tick);
        Color.Write(Writer, Names);
    }
    void FFXDLSE::TickColor::AddClassNames(ClassNames& Names) const {
        TickTraits::AddName(Names);
        Color.AddClassNames(Names);
    }

    void FFXDLSE::TickColor3::Read(BinaryReader& Reader, const ClassNames& Names) {
        Tick = TickTraits::Read(Reader, Names);
        Color1.Read(Reader, Names);
        Color2.Read(Reader, Names);
        Color3.Read(Reader, Names);
    }
    void FFXDLSE::TickColor3::Write(BinaryWriter& Writer, const ClassNames& Names) const {
        TickTraits::Write(Writer, Names, Tick);
        Color1.Write(Writer, Names);
        Color2.Write(Writer, Names);
        Color3.Write(Writer, Names);
    }
    void FFXDLSE::TickColor3::AddClassNames(ClassNames& Names) const {
        TickTraits::AddName(Names);
        Color1.AddClassNames(Names);
        Color2.AddClassNames(Names);
        Color3.AddClassNames(Names);
    }
#pragma endregion

#pragma region Evaluatables
    void FFXDLSE::Evaluatable::Deserialize(BinaryReader& Reader, const ClassNames&) {
        Reader.Assert<int32_t>(Opcode());
        Reader.Assert<int32_t>(Type());
    }

    void FFXDLSE::Evaluatable::Serialize(BinaryWriter& Writer, const ClassNames&) const {
        Writer.WriteInt32(Opcode());
        Writer.WriteInt32(Type());
    }

    void FFXDLSE::EvaluatableConstant::Deserialize(BinaryReader& Reader, const ClassNames& Names) {
        Evaluatable::Deserialize(Reader, Names);
        Value = Reader.ReadInt32();
    }

    void FFXDLSE::EvaluatableConstant::Serialize(BinaryWriter& Writer, const ClassNames& Names) const {
        Evaluatable::Serialize(Writer, Names);
        Writer.WriteInt32(Value);
    }

    std::shared_ptr<FFXDLSE::Evaluatable> FFXDLSE::Evaluatable::ReadAny(BinaryReader& Reader, const ClassNames& Names) {
        const int32_t Opcode = Reader.ReadAt<int32_t>(Reader.Position() + 0xA);
        switch (Opcode) {
            case 1: return ReadAs<EvaluatableConstant>(Reader, Names);
            case 2: return ReadAs<Evaluatable2>(Reader, Names);
            case 3: return ReadAs<Evaluatable3>(Reader, Names);
            case 4: return ReadAs<EvaluatableCurrentTick>(Reader, Names);
            case 5: return ReadAs<EvaluatableTotalTick>(Reader, Names);
            case 8: return ReadAs<EvaluatableAnd>(Reader, Names);
            case 9: return ReadAs<EvaluatableOr>(Reader, Names);
            case 10: return ReadAs<EvaluatableGE>(Reader, Names);
            case 11: return ReadAs<EvaluatableGT>(Reader, Names);
            case 12: return ReadAs<EvaluatableLE>(Reader, Names);
            case 13: return ReadAs<EvaluatableLT>(Reader, Names);
            case 14: return ReadAs<EvaluatableEQ>(Reader, Names);
            case 15: return ReadAs<EvaluatableNE>(Reader, Names);
            case 20: return ReadAs<EvaluatableNot>(Reader, Names);
            case 21: return ReadAs<EvaluatableChildExists>(Reader, Names);
            case 22: return ReadAs<EvaluatableParentExists>(Reader, Names);
            case 23: return ReadAs<EvaluatableDistanceFromCamera>(Reader, Names);
            case 24: return ReadAs<EvaluatableEmittersStopped>(Reader, Names);
            default: throw BinaryException("Unimplemented evaluatable opcode: " + std::to_string(Opcode));
        }
    }
#pragma endregion

#pragma region Params
    void FFXDLSE::Param::Deserialize(BinaryReader& Reader, const ClassNames&) {
        Reader.Assert<int32_t>(Type());
    }

    void FFXDLSE::Param::Serialize(BinaryWriter& Writer, const ClassNames&) const {
        Writer.WriteInt32(Type());
    }

    void FFXDLSE::Param15::AddClassNames(ClassNames& Names) const {
        Param::AddClassNames(Names);
        Color.AddClassNames(Names);
    }
    void FFXDLSE::Param15::Deserialize(BinaryReader& Reader, const ClassNames& Names) {
        Param::Deserialize(Reader, Names);
        Color.Read(Reader, Names);
    }
    void FFXDLSE::Param15::Serialize(BinaryWriter& Writer, const ClassNames& Names) const {
        Param::Serialize(Writer, Names);
        Color.Write(Writer, Names);
    }

    void FFXDLSE::Param82::AddClassNames(ClassNames& Names) const {
        Param::AddClassNames(Names);
        Inner->AddClassNames(Names);
        FloatTraits::AddName(Names);
    }
    void FFXDLSE::Param82::Deserialize(BinaryReader& Reader, const ClassNames& Names) {
        Param::Deserialize(Reader, Names);
        Inner = ReadAny(Reader, Names);
        Float = FloatTraits::Read(Reader, Names);
    }
    void FFXDLSE::Param82::Serialize(BinaryWriter& Writer, const ClassNames& Names) const {
        Param::Serialize(Writer, Names);
        Inner->Write(Writer, Names);
        FloatTraits::Write(Writer, Names, Float);
    }

    void FFXDLSE::Param83::AddClassNames(ClassNames& Names) const {
        Param::AddClassNames(Names);
        Color1.AddClassNames(Names);
        Color2.AddClassNames(Names);
    }
    void FFXDLSE::Param83::Deserialize(BinaryReader& Reader, const ClassNames& Names) {
        Param::Deserialize(Reader, Names);
        Color1.Read(Reader, Names);
        Color2.Read(Reader, Names);
    }
    void FFXDLSE::Param83::Serialize(BinaryWriter& Writer, const ClassNames& Names) const {
        Param::Serialize(Writer, Names);
        Color1.Write(Writer, Names);
        Color2.Write(Writer, Names);
    }

    void FFXDLSE::Param84::AddClassNames(ClassNames& Names) const {
        Param::AddClassNames(Names);
        Inner->AddClassNames(Names);
        Color.AddClassNames(Names);
    }
    void FFXDLSE::Param84::Deserialize(BinaryReader& Reader, const ClassNames& Names) {
        Param::Deserialize(Reader, Names);
        Inner = ReadAny(Reader, Names);
        Color.Read(Reader, Names);
    }
    void FFXDLSE::Param84::Serialize(BinaryWriter& Writer, const ClassNames& Names) const {
        Param::Serialize(Writer, Names);
        Inner->Write(Writer, Names);
        Color.Write(Writer, Names);
    }

    std::shared_ptr<FFXDLSE::Param> FFXDLSE::Param::ReadAny(BinaryReader& Reader, const ClassNames& Names) {
        const int32_t Type = Reader.ReadAt<int32_t>(Reader.Position() + 0xA);
        switch (Type) {
            case 1: return ReadAs<Param1>(Reader, Names);
            case 2: return ReadAs<Param2>(Reader, Names);
            case 5: return ReadAs<Param5>(Reader, Names);
            case 6: return ReadAs<Param6>(Reader, Names);
            case 7: return ReadAs<Param7>(Reader, Names);
            case 9: return ReadAs<Param9>(Reader, Names);
            case 11: return ReadAs<Param11>(Reader, Names);
            case 12: return ReadAs<Param12>(Reader, Names);
            case 13: return ReadAs<Param13>(Reader, Names);
            case 15: return ReadAs<Param15>(Reader, Names);
            case 17: return ReadAs<Param17>(Reader, Names);
            case 18: return ReadAs<Param18>(Reader, Names);
            case 19: return ReadAs<Param19>(Reader, Names);
            case 20: return ReadAs<Param20>(Reader, Names);
            case 21: return ReadAs<Param21>(Reader, Names);
            case 37: return ReadAs<Param37>(Reader, Names);
            case 38: return ReadAs<Param38>(Reader, Names);
            case 40: return ReadAs<Param40>(Reader, Names);
            case 41: return ReadAs<Param41>(Reader, Names);
            case 44: return ReadAs<Param44>(Reader, Names);
            case 45: return ReadAs<Param45>(Reader, Names);
            case 46: return ReadAs<Param46>(Reader, Names);
            case 47: return ReadAs<Param47>(Reader, Names);
            case 59: return ReadAs<Param59>(Reader, Names);
            case 60: return ReadAs<Param60>(Reader, Names);
            case 66: return ReadAs<Param66>(Reader, Names);
            case 68: return ReadAs<Param68>(Reader, Names);
            case 69: return ReadAs<Param69>(Reader, Names);
            case 70: return ReadAs<Param70>(Reader, Names);
            case 71: return ReadAs<Param71>(Reader, Names);
            case 79: return ReadAs<Param79>(Reader, Names);
            case 81: return ReadAs<Param81>(Reader, Names);
            case 82: return ReadAs<Param82>(Reader, Names);
            case 83: return ReadAs<Param83>(Reader, Names);
            case 84: return ReadAs<Param84>(Reader, Names);
            case 85: return ReadAs<Param85>(Reader, Names);
            case 87: return ReadAs<Param87>(Reader, Names);
            default: throw BinaryException("Unimplemented param type: " + std::to_string(Type));
        }
    }

    void FFXDLSE::ParamList::AddClassNames(ClassNames& Names) const {
        Serializable::AddClassNames(Names);
        for (const auto& P : Params) {
            P->AddClassNames(Names);
        }
    }
    void FFXDLSE::ParamList::Deserialize(BinaryReader& Reader, const ClassNames& Names) {
        const int32_t Count = Reader.ReadInt32();
        Unk04               = Reader.ReadInt32();
        Params.clear();
        for (int32_t I = 0; I < Count; ++I) {
            Params.push_back(Param::ReadAny(Reader, Names));
        }
    }
    void FFXDLSE::ParamList::Serialize(BinaryWriter& Writer, const ClassNames& Names) const {
        Writer.WriteInt32(static_cast<int32_t>(Params.size()));
        Writer.WriteInt32(Unk04);
        for (const auto& P : Params) {
            P->Write(Writer, Names);
        }
    }
#pragma endregion

#pragma region Structure
    void FFXDLSE::Action::AddClassNames(ClassNames& Names) const {
        Serializable::AddClassNames(Names);
        Params.AddClassNames(Names);
    }
    void FFXDLSE::Action::Deserialize(BinaryReader& Reader, const ClassNames& Names) {
        ID = Reader.ReadInt32();
        Params.Read(Reader, Names);
    }
    void FFXDLSE::Action::Serialize(BinaryWriter& Writer, const ClassNames& Names) const {
        Writer.WriteInt32(ID);
        Params.Write(Writer, Names);
    }

    void FFXDLSE::Trigger::AddClassNames(ClassNames& Names) const {
        Serializable::AddClassNames(Names);
        Evaluator->AddClassNames(Names);
    }
    void FFXDLSE::Trigger::Deserialize(BinaryReader& Reader, const ClassNames& Names) {
        StateIndex = Reader.ReadInt32();
        Evaluator  = Evaluatable::ReadAny(Reader, Names);
    }
    void FFXDLSE::Trigger::Serialize(BinaryWriter& Writer, const ClassNames& Names) const {
        Writer.WriteInt32(StateIndex);
        Evaluator->Write(Writer, Names);
    }

    void FFXDLSE::State::AddClassNames(ClassNames& Names) const {
        Serializable::AddClassNames(Names);
        for (const Action& A : Actions) {
            A.AddClassNames(Names);
        }
        for (const Trigger& T : Triggers) {
            T.AddClassNames(Names);
        }
    }
    void FFXDLSE::State::Deserialize(BinaryReader& Reader, const ClassNames& Names) {
        const int32_t ActionCount  = Reader.ReadInt32();
        const int32_t TriggerCount = Reader.ReadInt32();
        Actions.clear();
        Triggers.clear();
        for (int32_t I = 0; I < ActionCount; ++I) {
            Action A;
            A.Read(Reader, Names);
            Actions.push_back(std::move(A));
        }
        for (int32_t I = 0; I < TriggerCount; ++I) {
            Trigger T;
            T.Read(Reader, Names);
            Triggers.push_back(std::move(T));
        }
    }
    void FFXDLSE::State::Serialize(BinaryWriter& Writer, const ClassNames& Names) const {
        Writer.WriteInt32(static_cast<int32_t>(Actions.size()));
        Writer.WriteInt32(static_cast<int32_t>(Triggers.size()));
        for (const Action& A : Actions) {
            A.Write(Writer, Names);
        }
        for (const Trigger& T : Triggers) {
            T.Write(Writer, Names);
        }
    }

    void FFXDLSE::StateMap::AddClassNames(ClassNames& Names) const {
        Serializable::AddClassNames(Names);
        for (const State& S : States) {
            S.AddClassNames(Names);
        }
    }
    void FFXDLSE::StateMap::Deserialize(BinaryReader& Reader, const ClassNames& Names) {
        const int32_t Count = Reader.ReadInt32();
        States.clear();
        for (int32_t I = 0; I < Count; ++I) {
            State S;
            S.Read(Reader, Names);
            States.push_back(std::move(S));
        }
    }
    void FFXDLSE::StateMap::Serialize(BinaryWriter& Writer, const ClassNames& Names) const {
        Writer.WriteInt32(static_cast<int32_t>(States.size()));
        for (const State& S : States) {
            S.Write(Writer, Names);
        }
    }

    void FFXDLSE::ResourceSet::AddClassNames(ClassNames& Names) const {
        Serializable::AddClassNames(Names);
        AddClassName(Names, "DLVector");
    }
    void FFXDLSE::ResourceSet::Deserialize(BinaryReader& Reader, const ClassNames& Names) {
        Vector1 = ReadVector(Reader, Names);
        Vector2 = ReadVector(Reader, Names);
        Vector3 = ReadVector(Reader, Names);
        Vector4 = ReadVector(Reader, Names);
        Vector5 = ReadVector(Reader, Names);
    }
    void FFXDLSE::ResourceSet::Serialize(BinaryWriter& Writer, const ClassNames& Names) const {
        WriteVector(Writer, Names, Vector1);
        WriteVector(Writer, Names, Vector2);
        WriteVector(Writer, Names, Vector3);
        WriteVector(Writer, Names, Vector4);
        WriteVector(Writer, Names, Vector5);
    }

    void FFXDLSE::FXEffect::AddClassNames(ClassNames& Names) const {
        Serializable::AddClassNames(Names);
        AddClassName(Names, "DLVector");
        ParamList1.AddClassNames(Names);
        ParamList2.AddClassNames(Names);
        States.AddClassNames(Names);
        Resources.AddClassNames(Names);
    }
    void FFXDLSE::FXEffect::Deserialize(BinaryReader& Reader, const ClassNames& Names) {
        Reader.Assert<int32_t>(0);
        ID = Reader.ReadInt32();
        Reader.Assert<int32_t>(0);
        Reader.Assert<int32_t>(0);
        Reader.Assert<int32_t>(2);  // param list count?
        Reader.Assert<int16_t>(0);
        Reader.Assert<int16_t>(2);  // judging by the class name order, an always-empty DLVector
        Reader.Assert<int32_t>(0);
        ParamList1.Read(Reader, Names);
        ParamList2.Read(Reader, Names);
        States.Read(Reader, Names);
        Resources.Read(Reader, Names);
        Reader.Assert<uint8_t>(0);
    }
    void FFXDLSE::FXEffect::Serialize(BinaryWriter& Writer, const ClassNames& Names) const {
        Writer.WriteInt32(0);
        Writer.WriteInt32(ID);
        Writer.WriteInt32(0);
        Writer.WriteInt32(0);
        Writer.WriteInt32(2);
        Writer.WriteInt16(0);
        Writer.WriteInt16(2);
        Writer.WriteInt32(0);
        ParamList1.Write(Writer, Names);
        ParamList2.Write(Writer, Names);
        States.Write(Writer, Names);
        Resources.Write(Writer, Names);
        Writer.WriteByte(0);
    }
#pragma endregion

    bool FFXDLSE::IsImpl(BinaryReader& Reader) {
        return Reader.Length() >= 4 && Reader.GetASCII(0, 4) == "DLsE";
    }

    void FFXDLSE::ReadImpl(BinaryReader& Reader) {
        Reader.Order = Endian::Little;
        Reader.AssertMagic("DLsE");
        Reader.Assert<uint8_t>(1);
        Reader.Assert<uint8_t>(3);
        Reader.Assert<uint8_t>(0);
        Reader.Assert<uint8_t>(0);
        Reader.Assert<int32_t>(0);
        Reader.Assert<int32_t>(0);
        Reader.Assert<uint8_t>(0);
        Reader.Assert<int32_t>(1);
        const int16_t NameCount = Reader.ReadInt16();
        ClassNames Names;
        for (int16_t I = 0; I < NameCount; ++I) {
            const int32_t Length = Reader.ReadInt32();
            Names.push_back(Reader.ReadString(static_cast<size_t>(Length)));
        }
        Effect = {};
        Effect.Read(Reader, Names);
    }

    void FFXDLSE::WriteImpl(BinaryWriter& Writer) {
        ClassNames Names;
        Effect.AddClassNames(Names);
        Writer.Order = Endian::Little;
        Writer.WriteMagic("DLsE");
        Writer.WriteByte(1);
        Writer.WriteByte(3);
        Writer.WriteByte(0);
        Writer.WriteByte(0);
        Writer.WriteInt32(0);
        Writer.WriteInt32(0);
        Writer.WriteByte(0);
        Writer.WriteInt32(1);
        Writer.WriteInt16(static_cast<int16_t>(Names.size()));
        for (const std::string& Name : Names) {
            Writer.WriteInt32(static_cast<int32_t>(Name.size()));
            Writer.WriteString(Name, false);
        }
        Effect.Write(Writer, Names);
    }
}  // namespace Souls
