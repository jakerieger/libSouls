// Copyright (C) 2026 Jake Rieger
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, version 3. This program is distributed in the hope that it will
// be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU General
// Public License along with this program. If not, see https://www.gnu.org/licenses/.

#pragma once

#include <libSouls/SoulsFile.hpp>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of an exported class; only touched by this DLL's own code

    /// @brief FFXDLSE: an effect definition format used by Dark Souls and Dark Souls II.
    ///
    /// Extension: .ffx
    ///
    /// The file is a tree of serialized objects, each tagged with an index into a table of class names at the start of
    /// the file. The table is rebuilt from the tree when writing, in the order the classes are first encountered.
    class SOULS_API FFXDLSE : public SoulsFile<FFXDLSE> {
    public:
        /// @brief The class name table: the name of every class used in the file, in order.
        using ClassNames = std::vector<std::string>;

        /// @brief Index of a class in the table, plus one (the form used in the file); 0 if it isn't there.
        static int16_t ClassIndex(const ClassNames& Names, std::string_view Name);
        /// @brief Adds a name to the table if it isn't there yet.
        static void AddClassName(ClassNames& Names, std::string_view Name);

        /// @brief A serialized object: class index, version, length, then the object's own data.
        struct SOULS_API Serializable {
            virtual ~Serializable() = default;
            /// @brief The class's name, as listed in the file's class name table.
            virtual const char* ClassName() const = 0;
            /// @brief The class version written with each object of this class.
            virtual int32_t Version() const = 0;
            /// @brief Adds the names of this class and of the classes it contains to the class name table.
            virtual void AddClassNames(ClassNames& Names) const;
            /// @brief Reads the object: its class index, version and length, then its own fields.
            void Read(BinaryReader& Reader, const ClassNames& Names);
            /// @brief Writes the object: its class index, version and length, then its own fields.
            void Write(BinaryWriter& Writer, const ClassNames& Names) const;

        protected:
            /// @brief Reads the class's own fields.
            virtual void Deserialize(BinaryReader& Reader, const ClassNames& Names) = 0;
            /// @brief Writes the class's own fields.
            virtual void Serialize(BinaryWriter& Writer, const ClassNames& Names) const = 0;
        };

        /// @brief The basic value types objects are made of.
        enum class PrimitiveKind { Int, Float, Tick };

        template<PrimitiveKind K>
        struct PrimitiveTraits;

        /// @brief A color of four floats.
        struct SOULS_API PrimitiveColor : Serializable {
            /// @brief Red.
            float R = 0;
            /// @brief Green.
            float G = 0;
            /// @brief Blue.
            float B = 0;
            /// @brief Alpha.
            float A          = 0;
            PrimitiveColor() = default;
            /// @brief A color from its channels.
            PrimitiveColor(float R, float G, float B, float A) : R(R), G(G), B(B), A(A) {}
            /// @brief The class's name, as listed in the file's class name table.
            const char* ClassName() const override { return "FXSerializablePrimitive<FXColorRGBA>"; }
            /// @brief The class version written with each object of this class.
            int32_t Version() const override { return 1; }

        protected:
            /// @brief Reads the class's own fields.
            void Deserialize(BinaryReader& Reader, const ClassNames&) override;
            /// @brief Writes the class's own fields.
            void Serialize(BinaryWriter& Writer, const ClassNames&) const override;
        };

#pragma region Tick values
        /// @brief An int at a point in time.
        struct SOULS_API TickInt {
            /// @brief The time.
            float Tick = 0;
            /// @brief The value.
            int32_t Int = 0;
            /// @brief Reads the tick and value.
            void Read(BinaryReader& Reader, const ClassNames& Names);
            /// @brief Writes the tick and value.
            void Write(BinaryWriter& Writer, const ClassNames& Names) const;
            /// @brief Adds the names of this class and of the classes it contains to the class name table.
            void AddClassNames(ClassNames& Names) const;
        };
        /// @brief A float at a point in time.
        struct SOULS_API TickFloat {
            /// @brief The time.
            float Tick = 0;
            /// @brief The value.
            float Float = 0;
            /// @brief Reads the tick and value.
            void Read(BinaryReader& Reader, const ClassNames& Names);
            /// @brief Writes the tick and value.
            void Write(BinaryWriter& Writer, const ClassNames& Names) const;
            /// @brief Adds the names of this class and of the classes it contains to the class name table.
            void AddClassNames(ClassNames& Names) const;
        };
        /// @brief Three floats at a point in time.
        struct SOULS_API TickFloat3 {
            /// @brief The time.
            float Tick = 0;
            /// @brief The first value.
            float Float1 = 0;
            /// @brief The second value.
            float Float2 = 0;
            /// @brief The third value.
            float Float3 = 0;
            /// @brief Reads the tick and values.
            void Read(BinaryReader& Reader, const ClassNames& Names);
            /// @brief Writes the tick and values.
            void Write(BinaryWriter& Writer, const ClassNames& Names) const;
            /// @brief Adds the names of this class and of the classes it contains to the class name table.
            void AddClassNames(ClassNames& Names) const;
        };
        /// @brief A color at a point in time.
        struct SOULS_API TickColor {
            /// @brief The time.
            float Tick = 0;
            /// @brief The color.
            PrimitiveColor Color;
            /// @brief Reads the tick and color.
            void Read(BinaryReader& Reader, const ClassNames& Names);
            /// @brief Writes the tick and color.
            void Write(BinaryWriter& Writer, const ClassNames& Names) const;
            /// @brief Adds the names of this class and of the classes it contains to the class name table.
            void AddClassNames(ClassNames& Names) const;
        };
        /// @brief Three colors at a point in time.
        struct SOULS_API TickColor3 {
            /// @brief The time.
            float Tick = 0;
            /// @brief The first color.
            PrimitiveColor Color1;
            /// @brief The second color.
            PrimitiveColor Color2;
            /// @brief The third color.
            PrimitiveColor Color3;
            /// @brief Reads the tick and colors.
            void Read(BinaryReader& Reader, const ClassNames& Names);
            /// @brief Writes the tick and colors.
            void Write(BinaryWriter& Writer, const ClassNames& Names) const;
            /// @brief Adds the names of this class and of the classes it contains to the class name table.
            void AddClassNames(ClassNames& Names) const;
        };
#pragma endregion

#pragma region Evaluatables
        /// @brief An expression evaluated by a trigger.
        struct SOULS_API Evaluatable : Serializable {
            /// @brief The class's name, as listed in the file's class name table.
            const char* ClassName() const override { return "FXSerializableEvaluatable<dl_int32>"; }
            /// @brief The class version written with each object of this class.
            int32_t Version() const override { return 1; }
            /// @brief The opcode that identifies this evaluatable's operation.
            virtual int32_t Opcode() const = 0;
            /// @brief A code written after the opcode; which kind of evaluatable this is.
            virtual int32_t Type() const = 0;
            /// @brief Reads whichever kind of evaluatable comes next.
            static std::shared_ptr<Evaluatable> ReadAny(BinaryReader& Reader, const ClassNames& Names);

        protected:
            /// @brief Reads the class's own fields.
            void Deserialize(BinaryReader& Reader, const ClassNames& Names) override;
            /// @brief Writes the class's own fields.
            void Serialize(BinaryWriter& Writer, const ClassNames& Names) const override;
        };

        /// @brief An evaluatable with no operands or data (current tick, whether a child exists, ...).
        template<int32_t Code>
        struct EvaluatableLeaf : Evaluatable {
            /// @brief The opcode that identifies this evaluatable's operation.
            int32_t Opcode() const override { return Code; }
            /// @brief A code written after the opcode; which kind of evaluatable this is.
            int32_t Type() const override { return 3; }
        };
        /// @brief Opcode 4: the current tick.
        using EvaluatableCurrentTick = EvaluatableLeaf<4>;
        /// @brief Opcode 5: the total tick.
        using EvaluatableTotalTick = EvaluatableLeaf<5>;
        /// @brief Opcode 21: whether a child exists.
        using EvaluatableChildExists = EvaluatableLeaf<21>;
        /// @brief Opcode 22: whether the parent exists.
        using EvaluatableParentExists = EvaluatableLeaf<22>;
        /// @brief Opcode 23: the distance from the camera.
        using EvaluatableDistanceFromCamera = EvaluatableLeaf<23>;
        /// @brief Opcode 24: whether the emitters have stopped.
        using EvaluatableEmittersStopped = EvaluatableLeaf<24>;

        /// @brief Opcode 1: a constant int.
        struct SOULS_API EvaluatableConstant : Evaluatable {
            /// @brief The constant.
            int32_t Value = 0;
            /// @brief The opcode that identifies this evaluatable's operation.
            int32_t Opcode() const override { return 1; }
            /// @brief A code written after the opcode; which kind of evaluatable this is.
            int32_t Type() const override { return 3; }

        protected:
            /// @brief Reads the class's own fields.
            void Deserialize(BinaryReader& Reader, const ClassNames& Names) override;
            /// @brief Writes the class's own fields.
            void Serialize(BinaryWriter& Writer, const ClassNames& Names) const override;
        };

        /// @brief Opcodes 2 and 3: something looked up by argument index.
        template<int32_t Code>
        struct EvaluatableArg : Evaluatable {
            /// @brief Unknown.
            int32_t Unk00 = 0;
            /// @brief Which argument to read.
            int32_t ArgIndex = 0;
            /// @brief The opcode that identifies this evaluatable's operation.
            int32_t Opcode() const override { return Code; }
            /// @brief A code written after the opcode; which kind of evaluatable this is.
            int32_t Type() const override { return 3; }

        protected:
            /// @brief Reads the class's own fields.
            void Deserialize(BinaryReader& Reader, const ClassNames& Names) override {
                Evaluatable::Deserialize(Reader, Names);
                Unk00    = Reader.ReadInt32();
                ArgIndex = Reader.ReadInt32();
            }
            /// @brief Writes the class's own fields.
            void Serialize(BinaryWriter& Writer, const ClassNames& Names) const override {
                Evaluatable::Serialize(Writer, Names);
                Writer.WriteInt32(Unk00);
                Writer.WriteInt32(ArgIndex);
            }
        };
        /// @brief Opcode 2: an argument lookup.
        using Evaluatable2 = EvaluatableArg<2>;
        /// @brief Opcode 3: an argument lookup.
        using Evaluatable3 = EvaluatableArg<3>;

        /// @brief Unary operators.
        template<int32_t Code>
        struct EvaluatableUnary : Evaluatable {
            /// @brief The operand.
            std::shared_ptr<Evaluatable> Operand;
            /// @brief The opcode that identifies this evaluatable's operation.
            int32_t Opcode() const override { return Code; }
            /// @brief A code written after the opcode; which kind of evaluatable this is.
            int32_t Type() const override { return 1; }
            /// @brief Adds the names of this class and of the classes it contains to the class name table.
            void AddClassNames(ClassNames& Names) const override {
                Evaluatable::AddClassNames(Names);
                Operand->AddClassNames(Names);
            }

        protected:
            /// @brief Reads the class's own fields.
            void Deserialize(BinaryReader& Reader, const ClassNames& Names) override {
                Evaluatable::Deserialize(Reader, Names);
                Operand = ReadAny(Reader, Names);
            }
            /// @brief Writes the class's own fields.
            void Serialize(BinaryWriter& Writer, const ClassNames& Names) const override {
                Evaluatable::Serialize(Writer, Names);
                Operand->Write(Writer, Names);
            }
        };
        /// @brief Opcode 20: logical not.
        using EvaluatableNot = EvaluatableUnary<20>;

        /// @brief Binary operators; the file stores the right operand first.
        template<int32_t Code>
        struct EvaluatableBinary : Evaluatable {
            /// @brief The left operand.
            std::shared_ptr<Evaluatable> Left;
            /// @brief The right operand.
            std::shared_ptr<Evaluatable> Right;
            /// @brief The opcode that identifies this evaluatable's operation.
            int32_t Opcode() const override { return Code; }
            /// @brief A code written after the opcode; which kind of evaluatable this is.
            int32_t Type() const override { return 1; }
            /// @brief Adds the names of this class and of the classes it contains to the class name table.
            void AddClassNames(ClassNames& Names) const override {
                Evaluatable::AddClassNames(Names);
                Left->AddClassNames(Names);
                Right->AddClassNames(Names);
            }

        protected:
            /// @brief Reads the class's own fields.
            void Deserialize(BinaryReader& Reader, const ClassNames& Names) override {
                Evaluatable::Deserialize(Reader, Names);
                Right = ReadAny(Reader, Names);
                Left  = ReadAny(Reader, Names);
            }
            /// @brief Writes the class's own fields.
            void Serialize(BinaryWriter& Writer, const ClassNames& Names) const override {
                Evaluatable::Serialize(Writer, Names);
                Right->Write(Writer, Names);
                Left->Write(Writer, Names);
            }
        };
        /// @brief Opcode 8: logical and.
        using EvaluatableAnd = EvaluatableBinary<8>;
        /// @brief Opcode 9: logical or.
        using EvaluatableOr = EvaluatableBinary<9>;
        /// @brief Opcode 10: greater than or equal.
        using EvaluatableGE = EvaluatableBinary<10>;
        /// @brief Opcode 11: greater than.
        using EvaluatableGT = EvaluatableBinary<11>;
        /// @brief Opcode 12: less than or equal.
        using EvaluatableLE = EvaluatableBinary<12>;
        /// @brief Opcode 13: less than.
        using EvaluatableLT = EvaluatableBinary<13>;
        /// @brief Opcode 14: equal.
        using EvaluatableEQ = EvaluatableBinary<14>;
        /// @brief Opcode 15: not equal.
        using EvaluatableNE = EvaluatableBinary<15>;
#pragma endregion

#pragma region Params
        struct ParamList;

        /// @brief A value used by an action: a number, a curve over time, a color, an effect to spawn...
        struct SOULS_API Param : Serializable {
            /// @brief The class's name, as listed in the file's class name table.
            const char* ClassName() const override { return "FXSerializableParam"; }
            /// @brief The class version written with each object of this class.
            int32_t Version() const override { return 2; }
            /// @brief The param's type number (the N of ParamN).
            virtual int32_t Type() const = 0;
            /// @brief Reads whichever kind of param comes next.
            static std::shared_ptr<Param> ReadAny(BinaryReader& Reader, const ClassNames& Names);

        protected:
            /// @brief Reads the class's own fields.
            void Deserialize(BinaryReader& Reader, const ClassNames& Names) override;
            /// @brief Writes the class's own fields.
            void Serialize(BinaryWriter& Writer, const ClassNames& Names) const override;
        };

        /// @brief One primitive value (an int, float or tick).
        template<int32_t Id, PrimitiveKind K>
        struct PrimitiveParam : Param {
            /// @brief The value.
            decltype(PrimitiveTraits<K>::Zero()) Value = PrimitiveTraits<K>::Zero();
            /// @brief The param's type number (the N of ParamN).
            int32_t Type() const override { return Id; }
            /// @brief Adds the names of this class and of the classes it contains to the class name table.
            void AddClassNames(ClassNames& Names) const override {
                Param::AddClassNames(Names);
                PrimitiveTraits<K>::AddName(Names);
            }

        protected:
            /// @brief Reads the class's own fields.
            void Deserialize(BinaryReader& Reader, const ClassNames& Names) override {
                Param::Deserialize(Reader, Names);
                Value = PrimitiveTraits<K>::Read(Reader, Names);
            }
            /// @brief Writes the class's own fields.
            void Serialize(BinaryWriter& Writer, const ClassNames& Names) const override {
                Param::Serialize(Writer, Names);
                PrimitiveTraits<K>::Write(Writer, Names, Value);
            }
        };

        /// @brief Two primitive values of the same kind.
        template<int32_t Id, PrimitiveKind K>
        struct PrimitivePairParam : Param {
            /// @brief The first value.
            decltype(PrimitiveTraits<K>::Zero()) First = PrimitiveTraits<K>::Zero();
            /// @brief The second value.
            decltype(PrimitiveTraits<K>::Zero()) Second = PrimitiveTraits<K>::Zero();
            /// @brief The param's type number (the N of ParamN).
            int32_t Type() const override { return Id; }
            /// @brief Adds the names of this class and of the classes it contains to the class name table.
            void AddClassNames(ClassNames& Names) const override {
                Param::AddClassNames(Names);
                PrimitiveTraits<K>::AddName(Names);
            }

        protected:
            /// @brief Reads the class's own fields.
            void Deserialize(BinaryReader& Reader, const ClassNames& Names) override {
                Param::Deserialize(Reader, Names);
                First  = PrimitiveTraits<K>::Read(Reader, Names);
                Second = PrimitiveTraits<K>::Read(Reader, Names);
            }
            /// @brief Writes the class's own fields.
            void Serialize(BinaryWriter& Writer, const ClassNames& Names) const override {
                Param::Serialize(Writer, Names);
                PrimitiveTraits<K>::Write(Writer, Names, First);
                PrimitiveTraits<K>::Write(Writer, Names, Second);
            }
        };

        /// @brief A list of primitives of one kind.
        template<int32_t Id, PrimitiveKind K>
        struct PrimitiveListParam : Param {
            /// @brief The values.
            std::vector<decltype(PrimitiveTraits<K>::Zero())> Values;
            /// @brief The param's type number (the N of ParamN).
            int32_t Type() const override { return Id; }
            /// @brief Adds the names of this class and of the classes it contains to the class name table.
            void AddClassNames(ClassNames& Names) const override {
                Param::AddClassNames(Names);
                PrimitiveTraits<K>::AddName(Names);
            }

        protected:
            /// @brief Reads the class's own fields.
            void Deserialize(BinaryReader& Reader, const ClassNames& Names) override {
                Param::Deserialize(Reader, Names);
                const int32_t Count = Reader.ReadInt32();
                Values.clear();
                for (int32_t I = 0; I < Count; ++I) {
                    Values.push_back(PrimitiveTraits<K>::Read(Reader, Names));
                }
            }
            /// @brief Writes the class's own fields.
            void Serialize(BinaryWriter& Writer, const ClassNames& Names) const override {
                Param::Serialize(Writer, Names);
                Writer.WriteInt32(static_cast<int32_t>(Values.size()));
                for (const auto& Value : Values) {
                    PrimitiveTraits<K>::Write(Writer, Names, Value);
                }
            }
        };

        /// @brief A list of values over time.
        template<int32_t Id, typename Elem>
        struct TickListParam : Param {
            /// @brief The values over time.
            std::vector<Elem> Ticks;
            /// @brief The param's type number (the N of ParamN).
            int32_t Type() const override { return Id; }
            /// @brief Adds the names of this class and of the classes it contains to the class name table.
            void AddClassNames(ClassNames& Names) const override {
                Param::AddClassNames(Names);
                for (const Elem& E : Ticks) {
                    E.AddClassNames(Names);
                }
            }

        protected:
            /// @brief Reads the class's own fields.
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
            /// @brief Writes the class's own fields.
            void Serialize(BinaryWriter& Writer, const ClassNames& Names) const override {
                Param::Serialize(Writer, Names);
                Writer.WriteInt32(static_cast<int32_t>(Ticks.size()));
                for (const Elem& E : Ticks) {
                    E.Write(Writer, Names);
                }
            }
        };

        /// @brief A plain int32 stored directly.
        template<int32_t Id>
        struct RawIntParam : Param {
            /// @brief The value.
            int32_t Value = 0;
            /// @brief The param's type number (the N of ParamN).
            int32_t Type() const override { return Id; }

        protected:
            /// @brief Reads the class's own fields.
            void Deserialize(BinaryReader& Reader, const ClassNames& Names) override {
                Param::Deserialize(Reader, Names);
                Value = Reader.ReadInt32();
            }
            /// @brief Writes the class's own fields.
            void Serialize(BinaryWriter& Writer, const ClassNames& Names) const override {
                Param::Serialize(Writer, Names);
                Writer.WriteInt32(Value);
            }
        };

        /// @brief A value read from the arguments of the effect.
        template<int32_t Id>
        struct ArgParam : Param {
            /// @brief Unknown.
            int32_t Unk04 = 0;
            /// @brief Which argument to read.
            int32_t ArgIndex = 0;
            /// @brief The param's type number (the N of ParamN).
            int32_t Type() const override { return Id; }

        protected:
            /// @brief Reads the class's own fields.
            void Deserialize(BinaryReader& Reader, const ClassNames& Names) override {
                Param::Deserialize(Reader, Names);
                Unk04    = Reader.ReadInt32();
                ArgIndex = Reader.ReadInt32();
            }
            /// @brief Writes the class's own fields.
            void Serialize(BinaryWriter& Writer, const ClassNames& Names) const override {
                Param::Serialize(Writer, Names);
                Writer.WriteInt32(Unk04);
                Writer.WriteInt32(ArgIndex);
            }
        };

        /// @brief Param type 1: an int.
        using Param1 = PrimitiveParam<1, PrimitiveKind::Int>;
        /// @brief Param type 2: a list of ints.
        using Param2 = PrimitiveListParam<2, PrimitiveKind::Int>;
        /// @brief Param type 5: ints over time.
        using Param5 = TickListParam<5, TickInt>;
        /// @brief Param type 6: ints over time.
        using Param6 = TickListParam<6, TickInt>;
        /// @brief Param type 7: a float.
        using Param7 = PrimitiveParam<7, PrimitiveKind::Float>;
        /// @brief Param type 9: floats over time.
        using Param9 = TickListParam<9, TickFloat>;
        /// @brief Param type 11: floats over time.
        using Param11 = TickListParam<11, TickFloat>;
        /// @brief Param type 12: floats over time.
        using Param12 = TickListParam<12, TickFloat>;
        /// @brief Param type 13: three floats over time.
        using Param13 = TickListParam<13, TickFloat3>;
        /// @brief Param type 17: colors over time.
        using Param17 = TickListParam<17, TickColor>;
        /// @brief Param type 18: colors over time.
        using Param18 = TickListParam<18, TickColor>;
        /// @brief Param type 19: colors over time.
        using Param19 = TickListParam<19, TickColor>;
        /// @brief Param type 20: colors over time.
        using Param20 = TickListParam<20, TickColor>;
        /// @brief Param type 21: three colors over time.
        using Param21 = TickListParam<21, TickColor3>;
        using Param40 = RawIntParam<40>;  ///< texture ID
        /// @brief Param type 41: an int (meaning unknown).
        using Param41 = RawIntParam<41>;
        /// @brief Param type 44: a value read from the effect's arguments.
        using Param44 = ArgParam<44>;
        /// @brief Param type 45: a value read from the effect's arguments.
        using Param45 = ArgParam<45>;
        /// @brief Param type 46: a value read from the effect's arguments.
        using Param46 = ArgParam<46>;
        /// @brief Param type 47: a value read from the effect's arguments.
        using Param47 = ArgParam<47>;
        /// @brief Param type 59: a value read from the effect's arguments.
        using Param59 = ArgParam<59>;
        /// @brief Param type 60: a value read from the effect's arguments.
        using Param60 = ArgParam<60>;
        /// @brief Param type 66: a value read from the effect's arguments.
        using Param66 = ArgParam<66>;
        using Param68 = RawIntParam<68>;  ///< sound ID
        /// @brief Param type 69: an int (meaning unknown).
        using Param69 = RawIntParam<69>;
        /// @brief Param type 70: a tick.
        using Param70 = PrimitiveParam<70, PrimitiveKind::Tick>;
        /// @brief Param type 71: a value read from the effect's arguments.
        using Param71 = ArgParam<71>;
        /// @brief Param type 79: two ints.
        using Param79 = PrimitivePairParam<79, PrimitiveKind::Int>;
        /// @brief Param type 81: two floats.
        using Param81 = PrimitivePairParam<81, PrimitiveKind::Float>;
        /// @brief Param type 85: two ticks.
        using Param85 = PrimitivePairParam<85, PrimitiveKind::Tick>;
        /// @brief Param type 87: a value read from the effect's arguments.
        using Param87 = ArgParam<87>;

        /// @brief Param type 15: a color.
        struct SOULS_API Param15 : Param {
            /// @brief The color.
            PrimitiveColor Color;
            /// @brief The param's type number (the N of ParamN).
            int32_t Type() const override { return 15; }
            /// @brief Adds the names of this class and of the classes it contains to the class name table.
            void AddClassNames(ClassNames& Names) const override;

        protected:
            /// @brief Reads the class's own fields.
            void Deserialize(BinaryReader& Reader, const ClassNames& Names) override;
            /// @brief Writes the class's own fields.
            void Serialize(BinaryWriter& Writer, const ClassNames& Names) const override;
        };

        /// @brief Spawns another effect (37) or runs an action (38) with its own params.
        template<int32_t Id>
        struct SpawnParam : Param {
            /// @brief The effect or action ID.
            int32_t ID = 0;
            /// @brief The params passed to the spawned effect or action.
            std::shared_ptr<ParamList> Params;
            SpawnParam();
            /// @brief The param's type number (the N of ParamN).
            int32_t Type() const override { return Id; }
            /// @brief Adds the names of this class and of the classes it contains to the class name table.
            void AddClassNames(ClassNames& Names) const override;

        protected:
            /// @brief Reads the class's own fields.
            void Deserialize(BinaryReader& Reader, const ClassNames& Names) override;
            /// @brief Writes the class's own fields.
            void Serialize(BinaryWriter& Writer, const ClassNames& Names) const override;
        };
        /// @brief Param type 37: spawns another effect.
        using Param37 = SpawnParam<37>;
        /// @brief Param type 38: runs an action.
        using Param38 = SpawnParam<38>;

        /// @brief A param scaled by a float.
        struct SOULS_API Param82 : Param {
            /// @brief The param being scaled.
            std::shared_ptr<Param> Inner;
            /// @brief The scale.
            float Float = 0;
            /// @brief The param's type number (the N of ParamN).
            int32_t Type() const override { return 82; }
            /// @brief Adds the names of this class and of the classes it contains to the class name table.
            void AddClassNames(ClassNames& Names) const override;

        protected:
            /// @brief Reads the class's own fields.
            void Deserialize(BinaryReader& Reader, const ClassNames& Names) override;
            /// @brief Writes the class's own fields.
            void Serialize(BinaryWriter& Writer, const ClassNames& Names) const override;
        };

        /// @brief Param type 83: two colors.
        struct SOULS_API Param83 : Param {
            /// @brief The first color.
            PrimitiveColor Color1;
            /// @brief The second color.
            PrimitiveColor Color2;
            /// @brief The param's type number (the N of ParamN).
            int32_t Type() const override { return 83; }
            /// @brief Adds the names of this class and of the classes it contains to the class name table.
            void AddClassNames(ClassNames& Names) const override;

        protected:
            /// @brief Reads the class's own fields.
            void Deserialize(BinaryReader& Reader, const ClassNames& Names) override;
            /// @brief Writes the class's own fields.
            void Serialize(BinaryWriter& Writer, const ClassNames& Names) const override;
        };

        /// @brief A param tinted by a color.
        struct SOULS_API Param84 : Param {
            /// @brief The param being tinted.
            std::shared_ptr<Param> Inner;
            /// @brief The tint.
            PrimitiveColor Color;
            /// @brief The param's type number (the N of ParamN).
            int32_t Type() const override { return 84; }
            /// @brief Adds the names of this class and of the classes it contains to the class name table.
            void AddClassNames(ClassNames& Names) const override;

        protected:
            /// @brief Reads the class's own fields.
            void Deserialize(BinaryReader& Reader, const ClassNames& Names) override;
            /// @brief Writes the class's own fields.
            void Serialize(BinaryWriter& Writer, const ClassNames& Names) const override;
        };

        /// @brief A list of params.
        struct SOULS_API ParamList : Serializable {
            /// @brief Unknown.
            int32_t Unk04 = 0;
            /// @brief The params.
            std::vector<std::shared_ptr<Param>> Params;
            /// @brief The class's name, as listed in the file's class name table.
            const char* ClassName() const override { return "FXSerializableParamList"; }
            /// @brief The class version written with each object of this class.
            int32_t Version() const override { return 2; }
            /// @brief Adds the names of this class and of the classes it contains to the class name table.
            void AddClassNames(ClassNames& Names) const override;

        protected:
            /// @brief Reads the class's own fields.
            void Deserialize(BinaryReader& Reader, const ClassNames& Names) override;
            /// @brief Writes the class's own fields.
            void Serialize(BinaryWriter& Writer, const ClassNames& Names) const override;
        };
#pragma endregion

#pragma region Structure
        /// @brief An action run while its state is active.
        struct SOULS_API Action : Serializable {
            /// @brief The action's ID.
            int32_t ID = 0;
            /// @brief The action's params.
            ParamList Params;
            /// @brief The class's name, as listed in the file's class name table.
            const char* ClassName() const override { return "FXSerializableAction"; }
            /// @brief The class version written with each object of this class.
            int32_t Version() const override { return 1; }
            /// @brief Adds the names of this class and of the classes it contains to the class name table.
            void AddClassNames(ClassNames& Names) const override;

        protected:
            /// @brief Reads the class's own fields.
            void Deserialize(BinaryReader& Reader, const ClassNames& Names) override;
            /// @brief Writes the class's own fields.
            void Serialize(BinaryWriter& Writer, const ClassNames& Names) const override;
        };

        /// @brief A condition that moves the effect to another state.
        struct SOULS_API Trigger : Serializable {
            /// @brief The state to move to when the condition is met.
            int32_t StateIndex = 0;
            /// @brief The condition.
            std::shared_ptr<Evaluatable> Evaluator;
            /// @brief The class's name, as listed in the file's class name table.
            const char* ClassName() const override { return "FXSerializableTrigger"; }
            /// @brief The class version written with each object of this class.
            int32_t Version() const override { return 1; }
            /// @brief Adds the names of this class and of the classes it contains to the class name table.
            void AddClassNames(ClassNames& Names) const override;

        protected:
            /// @brief Reads the class's own fields.
            void Deserialize(BinaryReader& Reader, const ClassNames& Names) override;
            /// @brief Writes the class's own fields.
            void Serialize(BinaryWriter& Writer, const ClassNames& Names) const override;
        };

        /// @brief A state of the effect: the actions it runs and the triggers that leave it.
        struct SOULS_API State : Serializable {
            /// @brief The actions run in this state.
            std::vector<Action> Actions;
            /// @brief The conditions that move to other states.
            std::vector<Trigger> Triggers;
            /// @brief The class's name, as listed in the file's class name table.
            const char* ClassName() const override { return "FXSerializableState"; }
            /// @brief The class version written with each object of this class.
            int32_t Version() const override { return 1; }
            /// @brief Adds the names of this class and of the classes it contains to the class name table.
            void AddClassNames(ClassNames& Names) const override;

        protected:
            /// @brief Reads the class's own fields.
            void Deserialize(BinaryReader& Reader, const ClassNames& Names) override;
            /// @brief Writes the class's own fields.
            void Serialize(BinaryWriter& Writer, const ClassNames& Names) const override;
        };

        /// @brief The effect's states.
        struct SOULS_API StateMap : Serializable {
            /// @brief The states.
            std::vector<State> States;
            /// @brief The class's name, as listed in the file's class name table.
            const char* ClassName() const override { return "FXSerializableStateMap"; }
            /// @brief The class version written with each object of this class.
            int32_t Version() const override { return 1; }
            /// @brief Adds the names of this class and of the classes it contains to the class name table.
            void AddClassNames(ClassNames& Names) const override;

        protected:
            /// @brief Reads the class's own fields.
            void Deserialize(BinaryReader& Reader, const ClassNames& Names) override;
            /// @brief Writes the class's own fields.
            void Serialize(BinaryWriter& Writer, const ClassNames& Names) const override;
        };

        /// @brief Resources the effect uses: lists of IDs whose meanings are unknown.
        struct SOULS_API ResourceSet : Serializable {
            /// @brief Unknown list of IDs.
            std::vector<int32_t> Vector1;
            /// @brief Unknown list of IDs.
            std::vector<int32_t> Vector2;
            /// @brief Unknown list of IDs.
            std::vector<int32_t> Vector3;
            /// @brief Unknown list of IDs.
            std::vector<int32_t> Vector4;
            /// @brief Unknown list of IDs.
            std::vector<int32_t> Vector5;
            /// @brief The class's name, as listed in the file's class name table.
            const char* ClassName() const override { return "FXResourceSet"; }
            /// @brief The class version written with each object of this class.
            int32_t Version() const override { return 1; }
            /// @brief Adds the names of this class and of the classes it contains to the class name table.
            void AddClassNames(ClassNames& Names) const override;

        protected:
            /// @brief Reads the class's own fields.
            void Deserialize(BinaryReader& Reader, const ClassNames& Names) override;
            /// @brief Writes the class's own fields.
            void Serialize(BinaryWriter& Writer, const ClassNames& Names) const override;
        };

        /// @brief The effect: its ID, params, states and resources.
        struct SOULS_API FXEffect : Serializable {
            /// @brief The effect's ID.
            int32_t ID = 0;
            /// @brief Unknown params.
            ParamList ParamList1;
            /// @brief Unknown params.
            ParamList ParamList2;
            /// @brief The effect's state machine.
            StateMap States;
            /// @brief The resources the effect uses.
            ResourceSet Resources;
            /// @brief The class's name, as listed in the file's class name table.
            const char* ClassName() const override { return "FXSerializableEffect"; }
            /// @brief The class version written with each object of this class.
            int32_t Version() const override { return 5; }
            /// @brief Adds the names of this class and of the classes it contains to the class name table.
            void AddClassNames(ClassNames& Names) const override;

        protected:
            /// @brief Reads the class's own fields.
            void Deserialize(BinaryReader& Reader, const ClassNames& Names) override;
            /// @brief Writes the class's own fields.
            void Serialize(BinaryWriter& Writer, const ClassNames& Names) const override;
        };
#pragma endregion

        /// @brief The effect the file holds.
        FXEffect Effect;

    protected:
        bool IsImpl(BinaryReader& Reader) override;
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

    /// @brief How int primitives are stored.
    template<>
    struct SOULS_API FFXDLSE::PrimitiveTraits<FFXDLSE::PrimitiveKind::Int> {
        /// @brief The zero value.
        static int32_t Zero() { return 0; }
        /// @brief The primitive's class name.
        static constexpr const char* Name = "FXSerializablePrimitive<dl_int32>";
        /// @brief Reads one value.
        static int32_t Read(BinaryReader& Reader, const ClassNames& Names);
        /// @brief Writes one value.
        static void Write(BinaryWriter& Writer, const ClassNames& Names, int32_t Value);
        /// @brief Adds the primitive's class name to the table.
        static void AddName(ClassNames& Names) { AddClassName(Names, Name); }
    };
    /// @brief How float primitives are stored.
    template<>
    struct SOULS_API FFXDLSE::PrimitiveTraits<FFXDLSE::PrimitiveKind::Float> {
        /// @brief The zero value.
        static float Zero() { return 0; }
        /// @brief The primitive's class name.
        static constexpr const char* Name = "FXSerializablePrimitive<dl_float32>";
        /// @brief Reads one value.
        static float Read(BinaryReader& Reader, const ClassNames& Names);
        /// @brief Writes one value.
        static void Write(BinaryWriter& Writer, const ClassNames& Names, float Value);
        /// @brief Adds the primitive's class name to the table.
        static void AddName(ClassNames& Names) { AddClassName(Names, Name); }
    };
    /// @brief How tick primitives are stored.
    template<>
    struct SOULS_API FFXDLSE::PrimitiveTraits<FFXDLSE::PrimitiveKind::Tick> {
        /// @brief The zero value.
        static float Zero() { return 0; }
        /// @brief The primitive's class name.
        static constexpr const char* Name = "FXSerializablePrimitive<FXTick>";
        /// @brief Reads one value.
        static float Read(BinaryReader& Reader, const ClassNames& Names);
        /// @brief Writes one value.
        static void Write(BinaryWriter& Writer, const ClassNames& Names, float Value);
        /// @brief Adds the primitive's class name to the table.
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
