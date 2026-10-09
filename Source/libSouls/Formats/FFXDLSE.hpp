//
// Created by Jake Rieger on 10/8/2026.
//

#pragma once

#include <libSouls/SoulsFile.hpp>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of an exported class; only touched by this DLL's own code

    // FFXDLSE: an effect definition format used by Dark Souls and Dark Souls II. Extension: .ffx
    //
    // The file is a tree of serialized objects, each tagged with an index into a table of class names at the start of
    // the file. The table is rebuilt from the tree when writing, in the order the classes are first encountered.
    class SOULS_API FFXDLSE : public SoulsFile<FFXDLSE> {
    public:
        using ClassNames = std::vector<std::string>;

        // Index of a class in the table, plus one (the form used in the file); 0 if it isn't there.
        static int16_t ClassIndex(const ClassNames& Names, std::string_view Name);
        static void AddClassName(ClassNames& Names, std::string_view Name);

        // A serialized object: class index, version, length, then the object's own data.
        struct SOULS_API Serializable {
            virtual ~Serializable()                                       = default;
            virtual const char* ClassName() const                        = 0;
            virtual int32_t Version() const                              = 0;
            virtual void AddClassNames(ClassNames& Names) const;
            void Read(BinaryReader& Reader, const ClassNames& Names);
            void Write(BinaryWriter& Writer, const ClassNames& Names) const;

        protected:
            virtual void Deserialize(BinaryReader& Reader, const ClassNames& Names)  = 0;
            virtual void Serialize(BinaryWriter& Writer, const ClassNames& Names) const = 0;
        };

        // The basic value types objects are made of.
        enum class PrimitiveKind { Int, Float, Tick };

        template<PrimitiveKind K>
        struct PrimitiveTraits;

        // A color of four floats.
        struct SOULS_API PrimitiveColor : Serializable {
            float R = 0, G = 0, B = 0, A = 0;
            PrimitiveColor() = default;
            PrimitiveColor(float R, float G, float B, float A) : R(R), G(G), B(B), A(A) {}
            const char* ClassName() const override { return "FXSerializablePrimitive<FXColorRGBA>"; }
            int32_t Version() const override { return 1; }

        protected:
            void Deserialize(BinaryReader& Reader, const ClassNames&) override;
            void Serialize(BinaryWriter& Writer, const ClassNames&) const override;
        };

#pragma region Tick values
        struct SOULS_API TickInt {
            float Tick = 0;
            int32_t Int = 0;
            void Read(BinaryReader& Reader, const ClassNames& Names);
            void Write(BinaryWriter& Writer, const ClassNames& Names) const;
            void AddClassNames(ClassNames& Names) const;
        };
        struct SOULS_API TickFloat {
            float Tick  = 0;
            float Float = 0;
            void Read(BinaryReader& Reader, const ClassNames& Names);
            void Write(BinaryWriter& Writer, const ClassNames& Names) const;
            void AddClassNames(ClassNames& Names) const;
        };
        struct SOULS_API TickFloat3 {
            float Tick   = 0;
            float Float1 = 0, Float2 = 0, Float3 = 0;
            void Read(BinaryReader& Reader, const ClassNames& Names);
            void Write(BinaryWriter& Writer, const ClassNames& Names) const;
            void AddClassNames(ClassNames& Names) const;
        };
        struct SOULS_API TickColor {
            float Tick = 0;
            PrimitiveColor Color;
            void Read(BinaryReader& Reader, const ClassNames& Names);
            void Write(BinaryWriter& Writer, const ClassNames& Names) const;
            void AddClassNames(ClassNames& Names) const;
        };
        struct SOULS_API TickColor3 {
            float Tick = 0;
            PrimitiveColor Color1, Color2, Color3;
            void Read(BinaryReader& Reader, const ClassNames& Names);
            void Write(BinaryWriter& Writer, const ClassNames& Names) const;
            void AddClassNames(ClassNames& Names) const;
        };
#pragma endregion

#pragma region Evaluatables
        // An expression evaluated by a trigger.
        struct SOULS_API Evaluatable : Serializable {
            const char* ClassName() const override { return "FXSerializableEvaluatable<dl_int32>"; }
            int32_t Version() const override { return 1; }
            virtual int32_t Opcode() const = 0;
            virtual int32_t Type() const   = 0;
            // Reads whichever kind of evaluatable comes next.
            static std::shared_ptr<Evaluatable> ReadAny(BinaryReader& Reader, const ClassNames& Names);

        protected:
            void Deserialize(BinaryReader& Reader, const ClassNames& Names) override;
            void Serialize(BinaryWriter& Writer, const ClassNames& Names) const override;
        };

        // An evaluatable with no operands or data (current tick, whether a child exists, ...).
        template<int32_t Code>
        struct EvaluatableLeaf : Evaluatable {
            int32_t Opcode() const override { return Code; }
            int32_t Type() const override { return 3; }
        };
        using EvaluatableCurrentTick         = EvaluatableLeaf<4>;
        using EvaluatableTotalTick           = EvaluatableLeaf<5>;
        using EvaluatableChildExists         = EvaluatableLeaf<21>;
        using EvaluatableParentExists        = EvaluatableLeaf<22>;
        using EvaluatableDistanceFromCamera  = EvaluatableLeaf<23>;
        using EvaluatableEmittersStopped     = EvaluatableLeaf<24>;

        struct SOULS_API EvaluatableConstant : Evaluatable {
            int32_t Value = 0;
            int32_t Opcode() const override { return 1; }
            int32_t Type() const override { return 3; }

        protected:
            void Deserialize(BinaryReader& Reader, const ClassNames& Names) override;
            void Serialize(BinaryWriter& Writer, const ClassNames& Names) const override;
        };

        // Opcodes 2 and 3: something looked up by argument index.
        template<int32_t Code>
        struct EvaluatableArg : Evaluatable {
            int32_t Unk00    = 0;
            int32_t ArgIndex = 0;
            int32_t Opcode() const override { return Code; }
            int32_t Type() const override { return 3; }

        protected:
            void Deserialize(BinaryReader& Reader, const ClassNames& Names) override {
                Evaluatable::Deserialize(Reader, Names);
                Unk00    = Reader.ReadInt32();
                ArgIndex = Reader.ReadInt32();
            }
            void Serialize(BinaryWriter& Writer, const ClassNames& Names) const override {
                Evaluatable::Serialize(Writer, Names);
                Writer.WriteInt32(Unk00);
                Writer.WriteInt32(ArgIndex);
            }
        };
        using Evaluatable2 = EvaluatableArg<2>;
        using Evaluatable3 = EvaluatableArg<3>;

        template<int32_t Code>
        struct EvaluatableUnary : Evaluatable {
            std::shared_ptr<Evaluatable> Operand;
            int32_t Opcode() const override { return Code; }
            int32_t Type() const override { return 1; }
            void AddClassNames(ClassNames& Names) const override {
                Evaluatable::AddClassNames(Names);
                Operand->AddClassNames(Names);
            }

        protected:
            void Deserialize(BinaryReader& Reader, const ClassNames& Names) override {
                Evaluatable::Deserialize(Reader, Names);
                Operand = ReadAny(Reader, Names);
            }
            void Serialize(BinaryWriter& Writer, const ClassNames& Names) const override {
                Evaluatable::Serialize(Writer, Names);
                Operand->Write(Writer, Names);
            }
        };
        using EvaluatableNot = EvaluatableUnary<20>;

        // Binary operators; the file stores the right operand first.
        template<int32_t Code>
        struct EvaluatableBinary : Evaluatable {
            std::shared_ptr<Evaluatable> Left, Right;
            int32_t Opcode() const override { return Code; }
            int32_t Type() const override { return 1; }
            void AddClassNames(ClassNames& Names) const override {
                Evaluatable::AddClassNames(Names);
                Left->AddClassNames(Names);
                Right->AddClassNames(Names);
            }

        protected:
            void Deserialize(BinaryReader& Reader, const ClassNames& Names) override {
                Evaluatable::Deserialize(Reader, Names);
                Right = ReadAny(Reader, Names);
                Left  = ReadAny(Reader, Names);
            }
            void Serialize(BinaryWriter& Writer, const ClassNames& Names) const override {
                Evaluatable::Serialize(Writer, Names);
                Right->Write(Writer, Names);
                Left->Write(Writer, Names);
            }
        };
        using EvaluatableAnd = EvaluatableBinary<8>;
        using EvaluatableOr  = EvaluatableBinary<9>;
        using EvaluatableGE  = EvaluatableBinary<10>;
        using EvaluatableGT  = EvaluatableBinary<11>;
        using EvaluatableLE  = EvaluatableBinary<12>;
        using EvaluatableLT  = EvaluatableBinary<13>;
        using EvaluatableEQ  = EvaluatableBinary<14>;
        using EvaluatableNE  = EvaluatableBinary<15>;
#pragma endregion

#pragma region Params
        struct ParamList;

        // A value used by an action: a number, a curve over time, a color, an effect to spawn...
        struct SOULS_API Param : Serializable {
            const char* ClassName() const override { return "FXSerializableParam"; }
            int32_t Version() const override { return 2; }
            virtual int32_t Type() const = 0;
            // Reads whichever kind of param comes next.
            static std::shared_ptr<Param> ReadAny(BinaryReader& Reader, const ClassNames& Names);

        protected:
            void Deserialize(BinaryReader& Reader, const ClassNames& Names) override;
            void Serialize(BinaryWriter& Writer, const ClassNames& Names) const override;
        };

        // One primitive value (an int, float or tick).
        template<int32_t Id, PrimitiveKind K>
        struct PrimitiveParam : Param {
            decltype(PrimitiveTraits<K>::Zero()) Value = PrimitiveTraits<K>::Zero();
            int32_t Type() const override { return Id; }
            void AddClassNames(ClassNames& Names) const override {
                Param::AddClassNames(Names);
                PrimitiveTraits<K>::AddName(Names);
            }

        protected:
            void Deserialize(BinaryReader& Reader, const ClassNames& Names) override {
                Param::Deserialize(Reader, Names);
                Value = PrimitiveTraits<K>::Read(Reader, Names);
            }
            void Serialize(BinaryWriter& Writer, const ClassNames& Names) const override {
                Param::Serialize(Writer, Names);
                PrimitiveTraits<K>::Write(Writer, Names, Value);
            }
        };

        // Two primitive values of the same kind.
        template<int32_t Id, PrimitiveKind K>
        struct PrimitivePairParam : Param {
            decltype(PrimitiveTraits<K>::Zero()) First = PrimitiveTraits<K>::Zero(), Second = PrimitiveTraits<K>::Zero();
            int32_t Type() const override { return Id; }
            void AddClassNames(ClassNames& Names) const override {
                Param::AddClassNames(Names);
                PrimitiveTraits<K>::AddName(Names);
            }

        protected:
            void Deserialize(BinaryReader& Reader, const ClassNames& Names) override {
                Param::Deserialize(Reader, Names);
                First  = PrimitiveTraits<K>::Read(Reader, Names);
                Second = PrimitiveTraits<K>::Read(Reader, Names);
            }
            void Serialize(BinaryWriter& Writer, const ClassNames& Names) const override {
                Param::Serialize(Writer, Names);
                PrimitiveTraits<K>::Write(Writer, Names, First);
                PrimitiveTraits<K>::Write(Writer, Names, Second);
            }
        };

        // A list of primitives of one kind.
        template<int32_t Id, PrimitiveKind K>
        struct PrimitiveListParam : Param {
            std::vector<decltype(PrimitiveTraits<K>::Zero())> Values;
            int32_t Type() const override { return Id; }
            void AddClassNames(ClassNames& Names) const override {
                Param::AddClassNames(Names);
                PrimitiveTraits<K>::AddName(Names);
            }

        protected:
            void Deserialize(BinaryReader& Reader, const ClassNames& Names) override {
                Param::Deserialize(Reader, Names);
                const int32_t Count = Reader.ReadInt32();
                Values.clear();
                for (int32_t I = 0; I < Count; ++I) {
                    Values.push_back(PrimitiveTraits<K>::Read(Reader, Names));
                }
            }
            void Serialize(BinaryWriter& Writer, const ClassNames& Names) const override {
                Param::Serialize(Writer, Names);
                Writer.WriteInt32(static_cast<int32_t>(Values.size()));
                for (const auto& Value : Values) {
                    PrimitiveTraits<K>::Write(Writer, Names, Value);
                }
            }
        };

        // A list of values over time.
        template<int32_t Id, typename Elem>
        struct TickListParam : Param {
            std::vector<Elem> Ticks;
            int32_t Type() const override { return Id; }
            void AddClassNames(ClassNames& Names) const override {
                Param::AddClassNames(Names);
                for (const Elem& E : Ticks) {
                    E.AddClassNames(Names);
                }
            }

        protected:
            void Deserialize(BinaryReader& Reader, const ClassNames& Names) override {
                Param::Deserialize(Reader, Names);
                const int32_t Count = Reader.ReadInt32();
                Ticks.clear();
                for (int32_t I = 0; I < Count; ++I) {
                    Elem E;
                    E.Read(Reader, Names);
                    Ticks.push_back(std::move(E));
                }
            }
            void Serialize(BinaryWriter& Writer, const ClassNames& Names) const override {
                Param::Serialize(Writer, Names);
                Writer.WriteInt32(static_cast<int32_t>(Ticks.size()));
                for (const Elem& E : Ticks) {
                    E.Write(Writer, Names);
                }
            }
        };

        // A plain int32 stored directly.
        template<int32_t Id>
        struct RawIntParam : Param {
            int32_t Value = 0;
            int32_t Type() const override { return Id; }

        protected:
            void Deserialize(BinaryReader& Reader, const ClassNames& Names) override {
                Param::Deserialize(Reader, Names);
                Value = Reader.ReadInt32();
            }
            void Serialize(BinaryWriter& Writer, const ClassNames& Names) const override {
                Param::Serialize(Writer, Names);
                Writer.WriteInt32(Value);
            }
        };

        // A value read from the arguments of the effect.
        template<int32_t Id>
        struct ArgParam : Param {
            int32_t Unk04    = 0;
            int32_t ArgIndex = 0;
            int32_t Type() const override { return Id; }

        protected:
            void Deserialize(BinaryReader& Reader, const ClassNames& Names) override {
                Param::Deserialize(Reader, Names);
                Unk04    = Reader.ReadInt32();
                ArgIndex = Reader.ReadInt32();
            }
            void Serialize(BinaryWriter& Writer, const ClassNames& Names) const override {
                Param::Serialize(Writer, Names);
                Writer.WriteInt32(Unk04);
                Writer.WriteInt32(ArgIndex);
            }
        };

        using Param1  = PrimitiveParam<1, PrimitiveKind::Int>;
        using Param2  = PrimitiveListParam<2, PrimitiveKind::Int>;
        using Param5  = TickListParam<5, TickInt>;
        using Param6  = TickListParam<6, TickInt>;
        using Param7  = PrimitiveParam<7, PrimitiveKind::Float>;
        using Param9  = TickListParam<9, TickFloat>;
        using Param11 = TickListParam<11, TickFloat>;
        using Param12 = TickListParam<12, TickFloat>;
        using Param13 = TickListParam<13, TickFloat3>;
        using Param17 = TickListParam<17, TickColor>;
        using Param18 = TickListParam<18, TickColor>;
        using Param19 = TickListParam<19, TickColor>;
        using Param20 = TickListParam<20, TickColor>;
        using Param21 = TickListParam<21, TickColor3>;
        using Param40 = RawIntParam<40>;  // texture ID
        using Param41 = RawIntParam<41>;
        using Param44 = ArgParam<44>;
        using Param45 = ArgParam<45>;
        using Param46 = ArgParam<46>;
        using Param47 = ArgParam<47>;
        using Param59 = ArgParam<59>;
        using Param60 = ArgParam<60>;
        using Param66 = ArgParam<66>;
        using Param68 = RawIntParam<68>;  // sound ID
        using Param69 = RawIntParam<69>;
        using Param70 = PrimitiveParam<70, PrimitiveKind::Tick>;
        using Param71 = ArgParam<71>;
        using Param79 = PrimitivePairParam<79, PrimitiveKind::Int>;
        using Param81 = PrimitivePairParam<81, PrimitiveKind::Float>;
        using Param85 = PrimitivePairParam<85, PrimitiveKind::Tick>;
        using Param87 = ArgParam<87>;

        struct SOULS_API Param15 : Param {
            PrimitiveColor Color;
            int32_t Type() const override { return 15; }
            void AddClassNames(ClassNames& Names) const override;

        protected:
            void Deserialize(BinaryReader& Reader, const ClassNames& Names) override;
            void Serialize(BinaryWriter& Writer, const ClassNames& Names) const override;
        };

        // Spawns another effect (37) or runs an action (38) with its own params.
        template<int32_t Id>
        struct SpawnParam : Param {
            // The effect or action ID.
            int32_t ID = 0;
            std::shared_ptr<ParamList> Params;
            SpawnParam();
            int32_t Type() const override { return Id; }
            void AddClassNames(ClassNames& Names) const override;

        protected:
            void Deserialize(BinaryReader& Reader, const ClassNames& Names) override;
            void Serialize(BinaryWriter& Writer, const ClassNames& Names) const override;
        };
        using Param37 = SpawnParam<37>;
        using Param38 = SpawnParam<38>;

        // A param scaled by a float.
        struct SOULS_API Param82 : Param {
            std::shared_ptr<Param> Inner;
            float Float = 0;
            int32_t Type() const override { return 82; }
            void AddClassNames(ClassNames& Names) const override;

        protected:
            void Deserialize(BinaryReader& Reader, const ClassNames& Names) override;
            void Serialize(BinaryWriter& Writer, const ClassNames& Names) const override;
        };

        struct SOULS_API Param83 : Param {
            PrimitiveColor Color1, Color2;
            int32_t Type() const override { return 83; }
            void AddClassNames(ClassNames& Names) const override;

        protected:
            void Deserialize(BinaryReader& Reader, const ClassNames& Names) override;
            void Serialize(BinaryWriter& Writer, const ClassNames& Names) const override;
        };

        // A param tinted by a color.
        struct SOULS_API Param84 : Param {
            std::shared_ptr<Param> Inner;
            PrimitiveColor Color;
            int32_t Type() const override { return 84; }
            void AddClassNames(ClassNames& Names) const override;

        protected:
            void Deserialize(BinaryReader& Reader, const ClassNames& Names) override;
            void Serialize(BinaryWriter& Writer, const ClassNames& Names) const override;
        };

        struct SOULS_API ParamList : Serializable {
            int32_t Unk04 = 0;
            std::vector<std::shared_ptr<Param>> Params;
            const char* ClassName() const override { return "FXSerializableParamList"; }
            int32_t Version() const override { return 2; }
            void AddClassNames(ClassNames& Names) const override;

        protected:
            void Deserialize(BinaryReader& Reader, const ClassNames& Names) override;
            void Serialize(BinaryWriter& Writer, const ClassNames& Names) const override;
        };
#pragma endregion

#pragma region Structure
        struct SOULS_API Action : Serializable {
            int32_t ID = 0;
            ParamList Params;
            const char* ClassName() const override { return "FXSerializableAction"; }
            int32_t Version() const override { return 1; }
            void AddClassNames(ClassNames& Names) const override;

        protected:
            void Deserialize(BinaryReader& Reader, const ClassNames& Names) override;
            void Serialize(BinaryWriter& Writer, const ClassNames& Names) const override;
        };

        struct SOULS_API Trigger : Serializable {
            // The state to move to when the condition is met.
            int32_t StateIndex = 0;
            std::shared_ptr<Evaluatable> Evaluator;
            const char* ClassName() const override { return "FXSerializableTrigger"; }
            int32_t Version() const override { return 1; }
            void AddClassNames(ClassNames& Names) const override;

        protected:
            void Deserialize(BinaryReader& Reader, const ClassNames& Names) override;
            void Serialize(BinaryWriter& Writer, const ClassNames& Names) const override;
        };

        struct SOULS_API State : Serializable {
            std::vector<Action> Actions;
            std::vector<Trigger> Triggers;
            const char* ClassName() const override { return "FXSerializableState"; }
            int32_t Version() const override { return 1; }
            void AddClassNames(ClassNames& Names) const override;

        protected:
            void Deserialize(BinaryReader& Reader, const ClassNames& Names) override;
            void Serialize(BinaryWriter& Writer, const ClassNames& Names) const override;
        };

        struct SOULS_API StateMap : Serializable {
            std::vector<State> States;
            const char* ClassName() const override { return "FXSerializableStateMap"; }
            int32_t Version() const override { return 1; }
            void AddClassNames(ClassNames& Names) const override;

        protected:
            void Deserialize(BinaryReader& Reader, const ClassNames& Names) override;
            void Serialize(BinaryWriter& Writer, const ClassNames& Names) const override;
        };

        struct SOULS_API ResourceSet : Serializable {
            std::vector<int32_t> Vector1, Vector2, Vector3, Vector4, Vector5;
            const char* ClassName() const override { return "FXResourceSet"; }
            int32_t Version() const override { return 1; }
            void AddClassNames(ClassNames& Names) const override;

        protected:
            void Deserialize(BinaryReader& Reader, const ClassNames& Names) override;
            void Serialize(BinaryWriter& Writer, const ClassNames& Names) const override;
        };

        struct SOULS_API FXEffect : Serializable {
            int32_t ID = 0;
            ParamList ParamList1, ParamList2;
            StateMap States;
            ResourceSet Resources;
            const char* ClassName() const override { return "FXSerializableEffect"; }
            int32_t Version() const override { return 5; }
            void AddClassNames(ClassNames& Names) const override;

        protected:
            void Deserialize(BinaryReader& Reader, const ClassNames& Names) override;
            void Serialize(BinaryWriter& Writer, const ClassNames& Names) const override;
        };
#pragma endregion

        FXEffect Effect;

    protected:
        bool IsImpl(BinaryReader& Reader) override;
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

    template<>
    struct SOULS_API FFXDLSE::PrimitiveTraits<FFXDLSE::PrimitiveKind::Int> {
        static int32_t Zero() { return 0; }
        static constexpr const char* Name = "FXSerializablePrimitive<dl_int32>";
        static int32_t Read(BinaryReader& Reader, const ClassNames& Names);
        static void Write(BinaryWriter& Writer, const ClassNames& Names, int32_t Value);
        static void AddName(ClassNames& Names) { AddClassName(Names, Name); }
    };
    template<>
    struct SOULS_API FFXDLSE::PrimitiveTraits<FFXDLSE::PrimitiveKind::Float> {
        static float Zero() { return 0; }
        static constexpr const char* Name = "FXSerializablePrimitive<dl_float32>";
        static float Read(BinaryReader& Reader, const ClassNames& Names);
        static void Write(BinaryWriter& Writer, const ClassNames& Names, float Value);
        static void AddName(ClassNames& Names) { AddClassName(Names, Name); }
    };
    template<>
    struct SOULS_API FFXDLSE::PrimitiveTraits<FFXDLSE::PrimitiveKind::Tick> {
        static float Zero() { return 0; }
        static constexpr const char* Name = "FXSerializablePrimitive<FXTick>";
        static float Read(BinaryReader& Reader, const ClassNames& Names);
        static void Write(BinaryWriter& Writer, const ClassNames& Names, float Value);
        static void AddName(ClassNames& Names) { AddClassName(Names, Name); }
    };

    template<int32_t Id>
    FFXDLSE::SpawnParam<Id>::SpawnParam() : Params(std::make_shared<ParamList>()) {}

    template<int32_t Id>
    void FFXDLSE::SpawnParam<Id>::AddClassNames(ClassNames& Names) const {
        Param::AddClassNames(Names);
        Params->AddClassNames(Names);
    }

    template<int32_t Id>
    void FFXDLSE::SpawnParam<Id>::Deserialize(BinaryReader& Reader, const ClassNames& Names) {
        Param::Deserialize(Reader, Names);
        ID     = Reader.ReadInt32();
        Params = std::make_shared<ParamList>();
        Params->Read(Reader, Names);
    }

    template<int32_t Id>
    void FFXDLSE::SpawnParam<Id>::Serialize(BinaryWriter& Writer, const ClassNames& Names) const {
        Param::Serialize(Writer, Names);
        Writer.WriteInt32(ID);
        Params->Write(Writer, Names);
    }

#pragma warning(pop)

}  // namespace Souls
