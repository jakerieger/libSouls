//
// Created by Jake Rieger on 10/8/2026.
//

#include "ESD.hpp"

#include <libSouls/TextEncoding.hpp>

#include <algorithm>
#include <functional>
#include <unordered_map>

namespace Souls {
    namespace {
        using CommandCall = ESD::CommandCall;
        using Condition   = ESD::Condition;
        using State       = ESD::State;

        struct RawState {
            int64_t ID = 0;
            std::vector<int64_t> ConditionOffsets;
            State Value;
        };

        struct RawCondition {
            int64_t StateOffset = 0;
            std::vector<int64_t> ConditionOffsets;
            std::shared_ptr<Condition> Value = std::make_shared<Condition>();
        };

        std::vector<int64_t> ReadVarints(BinaryReader& Reader, int64_t Count) {
            if (Count < 0 || Count > Reader.Remaining()) {
                throw BinaryException("Invalid ESD offset count");
            }
            std::vector<int64_t> Values;
            Values.reserve(static_cast<size_t>(Count));
            for (int64_t I = 0; I < Count; ++I) {
                Values.push_back(Reader.ReadVarint());
            }
            return Values;
        }

        std::vector<uint8_t> GetBytes(BinaryReader& Reader, int64_t Offset, int64_t Length) {
            if (Length < 0) {
                throw BinaryException("Invalid ESD byte count");
            }
            Reader.StepIn(Offset);
            std::vector<uint8_t> Bytes = Reader.ReadBytes(static_cast<size_t>(Length));
            Reader.StepOut();
            return Bytes;
        }

        CommandCall ReadCommandCall(BinaryReader& Reader, int64_t DataStart) {
            CommandCall C;
            C.CommandBank           = Reader.Assert<int32_t>(1, 5, 6, 7);
            C.CommandID             = Reader.ReadInt32();
            const int64_t ArgsOffset = Reader.ReadVarint();
            const int64_t ArgsCount  = Reader.ReadVarint();
            Reader.StepIn(DataStart + ArgsOffset);
            for (int64_t I = 0; I < ArgsCount; ++I) {
                const int64_t ArgOffset = Reader.ReadVarint();
                const int64_t ArgSize   = Reader.ReadVarint();
                C.Arguments.push_back(GetBytes(Reader, DataStart + ArgOffset, ArgSize));
            }
            Reader.StepOut();
            return C;
        }

        std::vector<CommandCall> ReadCommandCalls(BinaryReader& Reader, int64_t DataStart, int64_t Offset, int64_t Count) {
            std::vector<CommandCall> Calls;
            Reader.Seek(DataStart + Offset);
            for (int64_t I = 0; I < Count; ++I) {
                Calls.push_back(ReadCommandCall(Reader, DataStart));
            }
            return Calls;
        }

        RawState ReadState(BinaryReader& Reader, int64_t DataStart) {
            RawState S;
            S.ID                              = Reader.ReadVarint();
            const int64_t ConditionOffsetsOff = Reader.ReadVarint();
            const int64_t ConditionOffsetCount = Reader.ReadVarint();
            const int64_t EntryOffset         = Reader.ReadVarint();
            const int64_t EntryCount          = Reader.ReadVarint();
            const int64_t ExitOffset          = Reader.ReadVarint();
            const int64_t ExitCount           = Reader.ReadVarint();
            const int64_t WhileOffset         = Reader.ReadVarint();
            const int64_t WhileCount          = Reader.ReadVarint();

            Reader.StepIn(0);
            Reader.Seek(DataStart + ConditionOffsetsOff);
            S.ConditionOffsets      = ReadVarints(Reader, ConditionOffsetCount);
            S.Value.EntryCommands = ReadCommandCalls(Reader, DataStart, EntryOffset, EntryCount);
            S.Value.ExitCommands  = ReadCommandCalls(Reader, DataStart, ExitOffset, ExitCount);
            S.Value.WhileCommands = ReadCommandCalls(Reader, DataStart, WhileOffset, WhileCount);
            Reader.StepOut();
            return S;
        }

        RawCondition ReadCondition(BinaryReader& Reader, int64_t DataStart) {
            RawCondition C;
            C.StateOffset                        = Reader.ReadVarint();
            const int64_t PassCommandsOffset     = Reader.ReadVarint();
            const int64_t PassCommandCount       = Reader.ReadVarint();
            const int64_t ConditionOffsetsOffset = Reader.ReadVarint();
            const int64_t ConditionOffsetCount   = Reader.ReadVarint();
            const int64_t EvaluatorOffset        = Reader.ReadVarint();
            const int64_t EvaluatorLength        = Reader.ReadVarint();

            Reader.StepIn(0);
            C.Value->PassCommands = ReadCommandCalls(Reader, DataStart, PassCommandsOffset, PassCommandCount);
            Reader.Seek(DataStart + ConditionOffsetsOffset);
            C.ConditionOffsets  = ReadVarints(Reader, ConditionOffsetCount);
            C.Value->Evaluator = GetBytes(Reader, DataStart + EvaluatorOffset, EvaluatorLength);
            Reader.StepOut();
            return C;
        }

        void ResolveCondition(RawCondition& C, const std::map<int64_t, int64_t>& StateIDs, std::map<int64_t, RawCondition>& Conditions) {
            if (C.StateOffset == -2) {
                return;  // already processed
            }
            if (C.StateOffset == -1) {
                C.Value->TargetState.reset();
            } else {
                const auto Found = StateIDs.find(C.StateOffset);
                if (Found == StateIDs.end()) {
                    throw BinaryException("Condition target state not found.");
                }
                C.Value->TargetState = Found->second;
            }
            C.StateOffset = -2;

            const std::vector<int64_t> Offsets = std::move(C.ConditionOffsets);
            C.ConditionOffsets.clear();
            C.Value->Subconditions.clear();
            for (const int64_t Offset : Offsets) {
                const auto Found = Conditions.find(Offset);
                if (Found == Conditions.end()) {
                    throw BinaryException("Subcondition not found.");
                }
                C.Value->Subconditions.push_back(Found->second.Value);
            }
            for (const int64_t Offset : Offsets) {
                ResolveCondition(Conditions.at(Offset), StateIDs, Conditions);
            }
        }

        std::string Key(const char* Kind, int64_t Group, int64_t Index, const char* What) {
            return std::string(Kind) + std::to_string(Group) + "-" + std::to_string(Index) + ":" + What;
        }

        void WriteCommandHeader(BinaryWriter& Writer, const CommandCall& C, size_t Index) {
            Writer.WriteInt32(C.CommandBank);
            Writer.WriteInt32(C.CommandID);
            Writer.ReserveVarint("Command" + std::to_string(Index) + ":ArgsOffset");
            Writer.WriteVarint(static_cast<int64_t>(C.Arguments.size()));
        }

        // Writes one list of command calls (or fills -1 when empty), appending them to the global command list.
        void WriteCommandList(BinaryWriter& Writer, const std::string& ReservationName, const std::vector<CommandCall>& List,
                              int64_t DataStart, std::vector<const CommandCall*>& Commands) {
            if (List.empty()) {
                Writer.FillVarint(ReservationName, -1);
                return;
            }
            Writer.FillVarint(ReservationName, Writer.Position() - DataStart);
            for (const CommandCall& C : List) {
                WriteCommandHeader(Writer, C, Commands.size());
                Commands.push_back(&C);
            }
        }
    }  // namespace

    bool ESD::IsImpl(BinaryReader& Reader) {
        if (Reader.Length() < 4) {
            return false;
        }
        const std::string Magic = Reader.GetASCII(0, 4);
        return Magic == "fSSL" || Magic == "fsSL";
    }

    void ESD::ReadImpl(BinaryReader& Reader) {
        Reader.Order = Endian::Little;
        const std::string Magic = Reader.ReadString(4);
        if (Magic != "fSSL" && Magic != "fsSL") {
            throw BinaryException("Not an ESD: bad magic");
        }
        LongFormat        = Magic == "fsSL";
        Reader.VarintLong = LongFormat;

        Reader.Assert<int32_t>(1);
        DarkSoulsCount = Reader.Assert<int32_t>(1, 2, 3);
        Reader.Assert<int32_t>(DarkSoulsCount);
        Reader.Assert<int32_t>(0x54);
        Reader.ReadInt32();  // data size
        Reader.Assert<int32_t>(6);
        Reader.Assert<int32_t>(LongFormat ? 0x48 : 0x2C);
        Reader.Assert<int32_t>(1);
        Reader.Assert<int32_t>(LongFormat ? 0x20 : 0x10);
        const int32_t StateGroupCount = Reader.ReadInt32();
        const int32_t StateSize       = Reader.Assert<int32_t>(LongFormat ? 0x48 : 0x24);
        const int32_t StateCount      = Reader.ReadInt32();
        Reader.Assert<int32_t>(LongFormat ? 0x38 : 0x1C);
        const int32_t ConditionCount = Reader.ReadInt32();
        Reader.Assert<int32_t>(LongFormat ? 0x18 : 0x10);
        Reader.ReadInt32();  // command call count
        Reader.Assert<int32_t>(LongFormat ? 0x10 : 0x8);
        Reader.ReadInt32();  // command arg count
        Reader.ReadInt32();  // condition offsets offset
        Reader.ReadInt32();  // condition offsets count
        Reader.ReadInt32();  // name block offset
        const int32_t NameLength = Reader.ReadInt32();
        Reader.ReadInt32();  // unknown offset 1
        Reader.Assert<int32_t>(0);
        Reader.ReadInt32();  // unknown offset 2
        Reader.Assert<int32_t>(0);

        const int64_t DataStart = Reader.Position();
        Reader.Assert<int32_t>(1);
        Unk70 = Reader.ReadInt32();
        Unk74 = Reader.ReadInt32();
        Unk78 = Reader.ReadInt32();
        Unk7C = Reader.ReadInt32();
        if (LongFormat) {
            Reader.Assert<int32_t>(0);
        }
        Reader.ReadVarint();  // state groups offset
        Reader.AssertVarint(StateGroupCount);
        const int64_t NameOffset = Reader.ReadVarint();
        Reader.AssertVarint(NameLength);
        const int64_t UnkNull = DarkSoulsCount == 1 ? 0 : -1;
        Reader.AssertVarint(UnkNull);
        Reader.AssertVarint(UnkNull);

        if (NameLength > 0) {
            Name = Reader.GetUTF16Text(DataStart + NameOffset);
        } else {
            Name.reset();
        }

        // State groups, in file order.
        std::vector<std::pair<int64_t, std::vector<int64_t>>> GroupOffsets;
        for (int32_t I = 0; I < StateGroupCount; ++I) {
            const int64_t ID           = Reader.ReadVarint();
            const int64_t StatesOffset = Reader.ReadVarint();
            const int64_t StateCountG  = Reader.ReadVarint();
            Reader.AssertVarint(StatesOffset);

            std::vector<int64_t> StateOffsets;
            for (int64_t S = 0; S < StateCountG; ++S) {
                StateOffsets.push_back(StatesOffset + S * StateSize);
            }
            // Every state group with more than one state has a dummy state after the end identical to the first.
            if (StateCountG > 1) {
                const std::vector<uint8_t> First = GetBytes(Reader, DataStart + StatesOffset, StateSize);
                const std::vector<uint8_t> Dummy = GetBytes(Reader, DataStart + StatesOffset + static_cast<int64_t>(StateSize) * StateCountG, StateSize);
                if (First != Dummy) {
                    throw BinaryException("ESD dummy state does not match the first state");
                }
            }
            for (const auto& [Existing, Offsets] : GroupOffsets) {
                if (Existing == ID) {
                    throw BinaryException("Duplicate state group ID.");
                }
            }
            GroupOffsets.emplace_back(ID, std::move(StateOffsets));
        }

        std::map<int64_t, RawState> States;
        for (int32_t I = 0; I < StateCount; ++I) {
            const int64_t At = Reader.Position() - DataStart;
            States[At]       = ReadState(Reader, DataStart);
        }
        std::map<int64_t, RawCondition> Conditions;
        std::unordered_map<const Condition*, int64_t> RawByPointer;
        for (int32_t I = 0; I < ConditionCount; ++I) {
            const int64_t At = Reader.Position() - DataStart;
            Conditions[At]   = ReadCondition(Reader, DataStart);
            RawByPointer[Conditions[At].Value.get()] = At;
        }
        for (auto& [Offset, S] : States) {
            for (const int64_t ConditionOffset : S.ConditionOffsets) {
                const auto Found = Conditions.find(ConditionOffset);
                if (Found == Conditions.end()) {
                    throw BinaryException("State condition not found.");
                }
                S.Value.Conditions.push_back(Found->second.Value);
            }
            S.ConditionOffsets.clear();
        }

        StateGroups.clear();
        for (const auto& [GroupID, StateOffsets] : GroupOffsets) {
            std::map<int64_t, int64_t> StateIDs;  // state offset -> state ID
            if (StateOffsets.size() > 1) {
                const int64_t WeirdOffset = StateOffsets[0] + static_cast<int64_t>(StateSize) * static_cast<int64_t>(StateOffsets.size());
                if (States.erase(WeirdOffset) == 0) {
                    throw BinaryException("Weird state not found.");
                }
            }
            std::map<int64_t, State> Group;
            for (const int64_t Offset : StateOffsets) {
                const auto Found = States.find(Offset);
                if (Found == States.end()) {
                    throw BinaryException("State not found.");
                }
                if (Group.contains(Found->second.ID)) {
                    throw BinaryException("Duplicate state ID.");
                }
                Group[Found->second.ID] = std::move(Found->second.Value);
                StateIDs[Offset]        = Found->second.ID;
                States.erase(Found);
            }
            for (auto& [ID, S] : Group) {
                for (const std::shared_ptr<Condition>& C : S.Conditions) {
                    ResolveCondition(Conditions.at(RawByPointer.at(C.get())), StateIDs, Conditions);
                }
            }
            StateGroups[GroupID] = std::move(Group);
        }
        if (!States.empty()) {
            throw BinaryException("Orphaned states found.");
        }
    }

    void ESD::WriteImpl(BinaryWriter& Writer) {
        Writer.Order      = Endian::Little;
        Writer.VarintLong = LongFormat;
        Writer.WriteMagic(LongFormat ? "fsSL" : "fSSL");
        Writer.WriteInt32(1);
        Writer.WriteInt32(DarkSoulsCount);
        Writer.WriteInt32(DarkSoulsCount);
        Writer.WriteInt32(0x54);
        Writer.Reserve<int32_t>("DataSize");
        Writer.WriteInt32(6);
        Writer.WriteInt32(LongFormat ? 0x48 : 0x2C);
        Writer.WriteInt32(1);
        Writer.WriteInt32(LongFormat ? 0x20 : 0x10);
        Writer.WriteInt32(static_cast<int32_t>(StateGroups.size()));
        const int32_t StateSize = LongFormat ? 0x48 : 0x24;
        Writer.WriteInt32(StateSize);
        int32_t StateTotal = 0;
        for (const auto& [ID, Group] : StateGroups) {
            StateTotal += static_cast<int32_t>(Group.size()) + (Group.size() == 1 ? 0 : 1);
        }
        Writer.WriteInt32(StateTotal);
        Writer.WriteInt32(LongFormat ? 0x38 : 0x1C);
        Writer.Reserve<int32_t>("ConditionCount");
        Writer.WriteInt32(LongFormat ? 0x18 : 0x10);
        Writer.Reserve<int32_t>("CommandCallCount");
        Writer.WriteInt32(LongFormat ? 0x10 : 0x8);
        Writer.Reserve<int32_t>("CommandArgCount");
        Writer.Reserve<int32_t>("ConditionOffsetsOffset");
        Writer.Reserve<int32_t>("ConditionOffsetsCount");
        Writer.Reserve<int32_t>("NameBlockOffset");
        const std::u16string Name16 = Name ? Text::UTF8ToUTF16(*Name) : std::u16string();
        Writer.WriteInt32(Name ? static_cast<int32_t>(Name16.size()) + 1 : 0);
        Writer.Reserve<int32_t>("UnkOffset1");
        Writer.WriteInt32(0);
        Writer.Reserve<int32_t>("UnkOffset2");
        Writer.WriteInt32(0);

        const int64_t DataStart = Writer.Position();
        Writer.WriteInt32(1);
        Writer.WriteInt32(Unk70);
        Writer.WriteInt32(Unk74);
        Writer.WriteInt32(Unk78);
        Writer.WriteInt32(Unk7C);
        if (LongFormat) {
            Writer.WriteInt32(0);
        }
        Writer.ReserveVarint("StateGroupsOffset");
        Writer.WriteVarint(static_cast<int64_t>(StateGroups.size()));
        Writer.ReserveVarint("NameOffset");
        Writer.WriteVarint(Name ? static_cast<int64_t>(Name16.size()) + 1 : 0);
        const int64_t UnkNull = DarkSoulsCount == 1 ? 0 : -1;
        Writer.WriteVarint(UnkNull);
        Writer.WriteVarint(UnkNull);

        // std::map iterates group and state IDs in ascending order, which is the order everything is written in.
        if (StateGroups.empty()) {
            Writer.FillVarint("StateGroupsOffset", -1);
        } else {
            Writer.FillVarint("StateGroupsOffset", Writer.Position() - DataStart);
            for (const auto& [GroupID, Group] : StateGroups) {
                Writer.WriteVarint(GroupID);
                Writer.ReserveVarint("StateGroup" + std::to_string(GroupID) + ":StatesOffset1");
                Writer.WriteVarint(static_cast<int64_t>(Group.size()));
                Writer.ReserveVarint("StateGroup" + std::to_string(GroupID) + ":StatesOffset2");
            }
        }

        std::map<int64_t, std::map<int64_t, int64_t>> StateOffsets;
        std::vector<std::pair<int64_t, int64_t>> WeirdStateOffsets;
        for (const auto& [GroupID, Group] : StateGroups) {
            const std::string GroupKey = "StateGroup" + std::to_string(GroupID);
            Writer.FillVarint(GroupKey + ":StatesOffset1", Writer.Position() - DataStart);
            Writer.FillVarint(GroupKey + ":StatesOffset2", Writer.Position() - DataStart);
            const int64_t FirstStateOffset = Writer.Position();
            for (const auto& [StateID, S] : Group) {
                StateOffsets[GroupID][StateID] = Writer.Position() - DataStart;
                Writer.WriteVarint(StateID);
                Writer.ReserveVarint(Key("State", GroupID, StateID, "ConditionsOffset"));
                Writer.WriteVarint(static_cast<int64_t>(S.Conditions.size()));
                Writer.ReserveVarint(Key("State", GroupID, StateID, "EntryCommandsOffset"));
                Writer.WriteVarint(static_cast<int64_t>(S.EntryCommands.size()));
                Writer.ReserveVarint(Key("State", GroupID, StateID, "ExitCommandsOffset"));
                Writer.WriteVarint(static_cast<int64_t>(S.ExitCommands.size()));
                Writer.ReserveVarint(Key("State", GroupID, StateID, "WhileCommandsOffset"));
                Writer.WriteVarint(static_cast<int64_t>(S.WhileCommands.size()));
            }
            if (Group.size() > 1) {
                WeirdStateOffsets.emplace_back(FirstStateOffset, Writer.Position());
                Writer.Pad(static_cast<size_t>(StateSize));  // filled with a copy of the first state at the end
            }
        }

        // Every unique condition (by identity) of each group, parents before their subconditions.
        std::map<int64_t, std::vector<const Condition*>> Conditions;
        for (const auto& [GroupID, Group] : StateGroups) {
            std::vector<const Condition*>& List = Conditions[GroupID];
            const std::function<void(const Condition*)> Add = [&](const Condition* C) {
                if (std::find(List.begin(), List.end(), C) == List.end()) {
                    List.push_back(C);
                    for (const std::shared_ptr<Condition>& Sub : C->Subconditions) {
                        Add(Sub.get());
                    }
                }
            };
            for (const auto& [StateID, S] : Group) {
                for (const std::shared_ptr<Condition>& C : S.Conditions) {
                    Add(C.get());
                }
            }
        }
        int32_t ConditionTotal = 0;
        for (const auto& [GroupID, List] : Conditions) {
            ConditionTotal += static_cast<int32_t>(List.size());
        }
        Writer.Fill<int32_t>("ConditionCount", ConditionTotal);

        std::unordered_map<const Condition*, int64_t> ConditionOffsets;
        for (const auto& [GroupID, List] : Conditions) {
            for (size_t I = 0; I < List.size(); ++I) {
                const Condition* C = List[I];
                ConditionOffsets[C] = Writer.Position() - DataStart;
                if (C->TargetState) {
                    Writer.WriteVarint(StateOffsets.at(GroupID).at(*C->TargetState));
                } else {
                    Writer.WriteVarint(-1);
                }
                Writer.ReserveVarint(Key("Condition", GroupID, static_cast<int64_t>(I), "PassCommandsOffset"));
                Writer.WriteVarint(static_cast<int64_t>(C->PassCommands.size()));
                Writer.ReserveVarint(Key("Condition", GroupID, static_cast<int64_t>(I), "ConditionsOffset"));
                Writer.WriteVarint(static_cast<int64_t>(C->Subconditions.size()));
                Writer.ReserveVarint(Key("Condition", GroupID, static_cast<int64_t>(I), "EvaluatorOffset"));
                Writer.WriteVarint(static_cast<int64_t>(C->Evaluator.size()));
            }
        }

        std::vector<const CommandCall*> Commands;
        for (const auto& [GroupID, Group] : StateGroups) {
            for (const auto& [StateID, S] : Group) {
                WriteCommandList(Writer, Key("State", GroupID, StateID, "EntryCommandsOffset"), S.EntryCommands, DataStart, Commands);
                WriteCommandList(Writer, Key("State", GroupID, StateID, "ExitCommandsOffset"), S.ExitCommands, DataStart, Commands);
                WriteCommandList(Writer, Key("State", GroupID, StateID, "WhileCommandsOffset"), S.WhileCommands, DataStart, Commands);
            }
            const auto& List = Conditions.at(GroupID);
            for (size_t I = 0; I < List.size(); ++I) {
                WriteCommandList(Writer, Key("Condition", GroupID, static_cast<int64_t>(I), "PassCommandsOffset"), List[I]->PassCommands, DataStart, Commands);
            }
        }
        Writer.Fill<int32_t>("CommandCallCount", static_cast<int32_t>(Commands.size()));
        int32_t ArgTotal = 0;
        for (const CommandCall* C : Commands) {
            ArgTotal += static_cast<int32_t>(C->Arguments.size());
        }
        Writer.Fill<int32_t>("CommandArgCount", ArgTotal);

        for (size_t I = 0; I < Commands.size(); ++I) {
            Writer.FillVarint("Command" + std::to_string(I) + ":ArgsOffset", Writer.Position() - DataStart);
            for (size_t A = 0; A < Commands[I]->Arguments.size(); ++A) {
                Writer.ReserveVarint("Command" + std::to_string(I) + "-" + std::to_string(A) + ":BytecodeOffset");
                Writer.WriteVarint(static_cast<int64_t>(Commands[I]->Arguments[A].size()));
            }
        }

        Writer.Fill<int32_t>("ConditionOffsetsOffset", static_cast<int32_t>(Writer.Position() - DataStart));
        int32_t ConditionOffsetsCount = 0;
        for (const auto& [GroupID, Group] : StateGroups) {
            for (const auto& [StateID, S] : Group) {
                Writer.FillVarint(Key("State", GroupID, StateID, "ConditionsOffset"), Writer.Position() - DataStart);
                for (const std::shared_ptr<Condition>& C : S.Conditions) {
                    Writer.WriteVarint(ConditionOffsets.at(C.get()));
                }
                ConditionOffsetsCount += static_cast<int32_t>(S.Conditions.size());
            }
            const auto& List = Conditions.at(GroupID);
            for (size_t I = 0; I < List.size(); ++I) {
                const std::string Name_ = Key("Condition", GroupID, static_cast<int64_t>(I), "ConditionsOffset");
                if (List[I]->Subconditions.empty()) {
                    Writer.FillVarint(Name_, -1);
                } else {
                    Writer.FillVarint(Name_, Writer.Position() - DataStart);
                    for (const std::shared_ptr<Condition>& Sub : List[I]->Subconditions) {
                        Writer.WriteVarint(ConditionOffsets.at(Sub.get()));
                    }
                }
                ConditionOffsetsCount += static_cast<int32_t>(List[I]->Subconditions.size());
            }
        }
        Writer.Fill<int32_t>("ConditionOffsetsCount", ConditionOffsetsCount);

        for (const auto& [GroupID, List] : Conditions) {
            for (size_t I = 0; I < List.size(); ++I) {
                Writer.FillVarint(Key("Condition", GroupID, static_cast<int64_t>(I), "EvaluatorOffset"), Writer.Position() - DataStart);
                Writer.WriteBytes(List[I]->Evaluator);
            }
        }
        for (size_t I = 0; I < Commands.size(); ++I) {
            for (size_t A = 0; A < Commands[I]->Arguments.size(); ++A) {
                Writer.FillVarint("Command" + std::to_string(I) + "-" + std::to_string(A) + ":BytecodeOffset", Writer.Position() - DataStart);
                Writer.WriteBytes(Commands[I]->Arguments[A]);
            }
        }

        Writer.Fill<int32_t>("NameBlockOffset", static_cast<int32_t>(Writer.Position() - DataStart));
        if (!Name) {
            Writer.FillVarint("NameOffset", -1);
        } else {
            Writer.Align(2);
            Writer.FillVarint("NameOffset", Writer.Position() - DataStart);
            Writer.WriteUTF16(Name16, true);
        }
        Writer.Fill<int32_t>("UnkOffset1", static_cast<int32_t>(Writer.Position() - DataStart));
        Writer.Fill<int32_t>("UnkOffset2", static_cast<int32_t>(Writer.Position() - DataStart));
        Writer.Fill<int32_t>("DataSize", static_cast<int32_t>(Writer.Position() - DataStart));
        if (DarkSoulsCount == 1) {
            Writer.Align(4);
        } else if (DarkSoulsCount == 2) {
            Writer.Align(0x10);
        }

        // The dummy state after each group (with more than one state) is a copy of that group's first state.
        for (const auto& [First, Dummy] : WeirdStateOffsets) {
            const std::vector<uint8_t> Bytes = Writer.ReadBack(First, static_cast<size_t>(StateSize));
            const int64_t End                = Writer.Position();
            Writer.Seek(Dummy);
            Writer.WriteBytes(Bytes);
            Writer.Seek(End);
        }
    }
}  // namespace Souls
