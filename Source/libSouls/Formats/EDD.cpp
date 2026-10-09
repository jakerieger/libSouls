//
// Created by Jake Rieger on 10/8/2026.
//

#include "EDD.hpp"

#include <map>

namespace Souls {
    namespace {
        // Takes `Count` consecutive objects of size `ObjectSize` starting at Offset out of the map. Each object may
        // only be used once.
        template<typename T>
        std::vector<T> GetUniqueOffsetList(int64_t Offset, int64_t Count, std::map<int64_t, T>& Offsets, int64_t ObjectSize) {
            std::vector<T> Objects;
            for (int64_t I = 0; I < Count; ++I) {
                const auto Found = Offsets.find(Offset);
                if (Found == Offsets.end()) {
                    throw BinaryException("Nonexistent or reused EDD object at index " + std::to_string(I) + "/" +
                                          std::to_string(Count) + ", offset " + std::to_string(Offset));
                }
                Objects.push_back(std::move(Found->second));
                Offsets.erase(Found);
                Offset += ObjectSize;
            }
            return Objects;
        }

        const std::string& StringAt(const std::vector<std::string>& Strings, int64_t Index) {
            if (Index < 0 || static_cast<size_t>(Index) >= Strings.size()) {
                throw BinaryException("EDD string index out of range");
            }
            return Strings[static_cast<size_t>(Index)];
        }
    }  // namespace

    bool EDD::IsImpl(BinaryReader& Reader) {
        if (Reader.Length() < 4) {
            return false;
        }
        const std::string Magic = Reader.GetASCII(0, 4);
        return Magic == "fSSL" || Magic == "fsSL";
    }

    void EDD::ReadImpl(BinaryReader& Reader) {
        Reader.Order            = Endian::Little;
        const std::string Magic = Reader.ReadString(4);
        if (Magic != "fSSL" && Magic != "fsSL") {
            throw BinaryException("Not an EDD: bad magic");
        }
        LongFormat        = Magic == "fsSL";
        Reader.VarintLong = LongFormat;

        Reader.Assert<int32_t>(1);
        Reader.Assert<int32_t>(1);
        Reader.Assert<int32_t>(1);
        Reader.Assert<int32_t>(0x7C);
        const int32_t DataSize = Reader.ReadInt32();
        Reader.Assert<int32_t>(11);
        Reader.Assert<int32_t>(LongFormat ? 0x58 : 0x34);
        Reader.Assert<int32_t>(1);
        Reader.Assert<int32_t>(LongFormat ? 0x10 : 8);
        const int32_t StringCount = Reader.ReadInt32();
        Reader.Assert<int32_t>(4);
        Reader.Assert<int32_t>(0);
        Reader.Assert<int32_t>(8);
        const int32_t FunctionSpecCount = Reader.ReadInt32();
        const int32_t ConditionSize     = Reader.Assert<int32_t>(LongFormat ? 0x10 : 8);
        const int32_t ConditionCount    = Reader.ReadInt32();
        Reader.Assert<int32_t>(LongFormat ? 0x10 : 8);
        Reader.Assert<int32_t>(0);
        Reader.Assert<int32_t>(LongFormat ? 0x18 : 0x10);
        const int32_t CommandSpecCount = Reader.ReadInt32();
        const int32_t CommandSize      = Reader.Assert<int32_t>(4);
        const int32_t CommandCount     = Reader.ReadInt32();
        const int32_t PassCommandSize  = Reader.Assert<int32_t>(LongFormat ? 0x10 : 8);
        const int32_t PassCommandCount = Reader.ReadInt32();
        const int32_t StateSize        = Reader.Assert<int32_t>(LongFormat ? 0x78 : 0x3C);
        const int32_t StateCount       = Reader.ReadInt32();
        Reader.Assert<int32_t>(LongFormat ? 0x48 : 0x30);
        const int32_t MachineCount  = Reader.ReadInt32();
        const int32_t StringsOffset = Reader.ReadInt32();
        Reader.Assert<int32_t>(0);
        Reader.Assert<int32_t>(StringsOffset);
        Unk80 = Reader.ReadInt32();
        Reader.Assert<int32_t>(DataSize);
        Reader.Assert<int32_t>(0);
        Reader.Assert<int32_t>(DataSize);
        Reader.Assert<int32_t>(0);

        const int64_t DataStart = Reader.Position();
        Reader.AssertVarint(0);
        Reader.ReadVarint();  // command spec offset
        Reader.AssertVarint(CommandSpecCount);
        Reader.ReadVarint();  // function spec offset
        Reader.AssertVarint(FunctionSpecCount);
        Reader.ReadVarint();  // machine offset
        Reader.Assert<int32_t>(MachineCount);
        for (int32_t& Value : UnkB0) {
            Value = Reader.ReadInt32();
        }
        if (LongFormat) {
            Reader.Assert<int32_t>(0);
        }
        Reader.AssertVarint(LongFormat ? 0x58 : 0x34);
        Reader.AssertVarint(StringCount);

        std::vector<std::string> Strings;
        for (int32_t I = 0; I < StringCount; ++I) {
            const int64_t StringOffset = Reader.ReadVarint();
            Reader.ReadVarint();  // char count; not needed as all strings are null-terminated
            Strings.push_back(Reader.GetUTF16Text(DataStart + StringOffset));
        }

        FunctionSpecs.clear();
        for (int32_t I = 0; I < FunctionSpecCount; ++I) {
            FunctionSpec F;
            F.ID                       = Reader.ReadInt32();
            const int16_t NameIndex    = Reader.ReadInt16();
            F.Unk06                    = Reader.ReadByte();
            F.Unk07                    = Reader.ReadByte();
            F.Name                     = StringAt(Strings, NameIndex);
            FunctionSpecs.push_back(std::move(F));
        }

        std::map<int64_t, ConditionDesc> Conditions;
        for (int32_t I = 0; I < ConditionCount; ++I) {
            const int64_t At = Reader.Position() - DataStart;
            Reader.AssertVarint(-1);
            Reader.AssertVarint(0);
            Conditions[At] = ConditionDesc{};
        }

        CommandSpecs.clear();
        for (int32_t I = 0; I < CommandSpecCount; ++I) {
            CommandSpec C;
            C.ID = Reader.ReadVarint();
            Reader.AssertVarint(-1);
            Reader.Assert<int32_t>(0);
            const int16_t NameIndex = Reader.ReadInt16();
            C.Unk0E                 = Reader.ReadInt16();
            C.Name                  = StringAt(Strings, NameIndex);
            CommandSpecs.push_back(std::move(C));
        }

        std::map<int64_t, CommandDesc> Commands;
        for (int32_t I = 0; I < CommandCount; ++I) {
            const int64_t At        = Reader.Position() - DataStart;
            const int16_t NameIndex = Reader.ReadInt16();
            Reader.Assert<uint8_t>(1);
            Reader.Assert<uint8_t>(0xFF);
            Commands[At] = CommandDesc{StringAt(Strings, NameIndex)};
        }

        if (LongFormat) {
            // Data-start-aligned padding.
            const int64_t Offset = Reader.Position() - DataStart;
            if (Offset % 8 > 0) {
                Reader.Skip(8 - Offset % 8);
            }
        }

        std::map<int64_t, PassCommandDesc> PassCommands;
        for (int32_t I = 0; I < PassCommandCount; ++I) {
            const int64_t At            = Reader.Position() - DataStart;
            const int32_t CommandOffset = Reader.ReadInt32();
            const int32_t Count         = Reader.ReadInt32();
            PassCommands[At].PassCommands = GetUniqueOffsetList(CommandOffset, Count, Commands, CommandSize);
        }

        std::map<int64_t, StateDesc> States;
        for (int32_t I = 0; I < StateCount; ++I) {
            const int64_t At = Reader.Position() - DataStart;
            StateDesc S;
            S.ID                             = Reader.ReadVarint();
            const int64_t NameIndexOffset    = Reader.ReadVarint();
            Reader.AssertVarint(1);
            const int64_t EntryOffset        = Reader.ReadVarint();
            const int64_t EntryCountS        = Reader.ReadVarint();
            const int64_t ExitOffset         = Reader.ReadVarint();
            const int64_t ExitCountS         = Reader.ReadVarint();
            const int64_t WhileOffset        = Reader.ReadVarint();
            const int64_t WhileCountS        = Reader.ReadVarint();
            const int64_t PassOffset         = Reader.ReadVarint();
            const int64_t PassCountS         = Reader.ReadVarint();
            const int64_t ConditionOffset    = Reader.ReadVarint();
            const int64_t ConditionCountS    = Reader.ReadVarint();
            Reader.AssertVarint(-1);
            Reader.AssertVarint(0);
            S.Name          = StringAt(Strings, Reader.ReadAt<int16_t>(DataStart + NameIndexOffset));
            S.EntryCommands = GetUniqueOffsetList(EntryOffset, EntryCountS, Commands, CommandSize);
            S.ExitCommands  = GetUniqueOffsetList(ExitOffset, ExitCountS, Commands, CommandSize);
            S.WhileCommands = GetUniqueOffsetList(WhileOffset, WhileCountS, Commands, CommandSize);
            S.PassCommands  = GetUniqueOffsetList(PassOffset, PassCountS, PassCommands, PassCommandSize);
            S.Conditions    = GetUniqueOffsetList(ConditionOffset, ConditionCountS, Conditions, ConditionSize);
            States[At]      = std::move(S);
        }

        Machines.clear();
        for (int32_t I = 0; I < MachineCount; ++I) {
            MachineDesc M;
            M.ID                    = Reader.ReadInt32();
            const int16_t NameIndex = Reader.ReadInt16();
            M.Unk06                 = Reader.ReadInt16();
            const std::vector<int16_t> ParamIndices = Reader.ReadArray<int16_t>(8);
            Reader.AssertVarint(-1);
            Reader.AssertVarint(0);
            Reader.AssertVarint(-1);
            Reader.AssertVarint(0);
            const int64_t StateOffset = Reader.ReadVarint();
            const int64_t StateCountM = Reader.ReadVarint();
            M.States                  = GetUniqueOffsetList(StateOffset, StateCountM, States, StateSize);
            M.Name                    = StringAt(Strings, NameIndex);
            for (size_t P = 0; P < 8; ++P) {
                if (ParamIndices[P] >= 0) {
                    M.ParamNames[P] = StringAt(Strings, ParamIndices[P]);
                }
            }
            Machines.push_back(std::move(M));
        }

        if (!Conditions.empty() || !Commands.empty() || !PassCommands.empty() || !States.empty()) {
            throw BinaryException("Orphaned ESD descriptions found");
        }
    }
}  // namespace Souls
