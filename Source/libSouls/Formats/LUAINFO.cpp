//
// Created by Jake Rieger on 10/8/2026.
//

#include "LUAINFO.hpp"

namespace Souls {
    bool LUAINFO::IsImpl(BinaryReader& Reader) {
        if (Reader.Length() < 4) {
            return false;
        }
        return Reader.GetASCII(0, 4) == "LUAI";
    }

    void LUAINFO::ReadImpl(BinaryReader& Reader) {
        Reader.Order = Endian::Little;
        Reader.AssertMagic("LUAI");
        BigEndian               = Reader.Assert<int32_t>(1, 0x1000000) == 0x1000000;
        Reader.Order            = BigEndian ? Endian::Big : Endian::Little;
        const int32_t GoalCount = Reader.ReadInt32();
        Reader.Assert<int32_t>(0);

        if (GoalCount == 0) {
            throw BinaryException("LUAINFO format cannot be detected on files with 0 goals.");
        } else if (GoalCount >= 2) {
            LongFormat = Reader.ReadAt<int32_t>(0x24) == 0;
        } else if (Reader.ReadAt<int32_t>(0x18) == 0x10 + 0x18 * GoalCount) {
            LongFormat = true;
        } else if (Reader.ReadAt<int32_t>(0x14) == 0x10 + 0x10 * GoalCount) {
            LongFormat = false;
        } else {
            throw BinaryException("Could not detect LUAINFO format.");
        }

        Goals.clear();
        Goals.reserve(static_cast<size_t>(GoalCount));
        for (int32_t I = 0; I < GoalCount; ++I) {
            Goal G;
            G.ID = Reader.ReadInt32();
            if (LongFormat) {
                G.BattleInterrupt = Reader.ReadBool();
                G.LogicInterrupt  = Reader.ReadBool();
                Reader.Assert<int16_t>(0);
                const int64_t NameOffset          = Reader.ReadInt64();
                const int64_t InterruptNameOffset = Reader.ReadInt64();
                G.Name                            = Reader.GetUTF16Text(NameOffset);
                if (InterruptNameOffset != 0) {
                    G.LogicInterruptName = Reader.GetUTF16Text(InterruptNameOffset);
                }
            } else {
                const uint32_t NameOffset          = Reader.ReadUInt32();
                const uint32_t InterruptNameOffset = Reader.ReadUInt32();
                G.BattleInterrupt                  = Reader.ReadBool();
                G.LogicInterrupt                   = Reader.ReadBool();
                Reader.Assert<int16_t>(0);
                G.Name = Reader.GetShiftJIS(NameOffset);
                if (InterruptNameOffset != 0) {
                    G.LogicInterruptName = Reader.GetShiftJIS(InterruptNameOffset);
                }
            }
            Goals.push_back(std::move(G));
        }
    }

    void LUAINFO::WriteImpl(BinaryWriter& Writer) {
        Writer.Order = BigEndian ? Endian::Big : Endian::Little;
        Writer.WriteMagic("LUAI");
        Writer.WriteInt32(1);
        Writer.WriteInt32(static_cast<int32_t>(Goals.size()));
        Writer.WriteInt32(0);

        for (size_t I = 0; I < Goals.size(); ++I) {
            const Goal& G           = Goals[I];
            const std::string Index = std::to_string(I);
            Writer.WriteInt32(G.ID);
            if (LongFormat) {
                Writer.WriteBool(G.BattleInterrupt);
                Writer.WriteBool(G.LogicInterrupt);
                Writer.WriteInt16(0);
                Writer.Reserve<int64_t>("NameOffset" + Index);
                Writer.Reserve<int64_t>("LogicInterruptNameOffset" + Index);
            } else {
                Writer.Reserve<uint32_t>("NameOffset" + Index);
                Writer.Reserve<uint32_t>("LogicInterruptNameOffset" + Index);
                Writer.WriteBool(G.BattleInterrupt);
                Writer.WriteBool(G.LogicInterrupt);
                Writer.WriteInt16(0);
            }
        }

        for (size_t I = 0; I < Goals.size(); ++I) {
            const Goal& G           = Goals[I];
            const std::string Index = std::to_string(I);
            if (LongFormat) {
                Writer.Fill<int64_t>("NameOffset" + Index, Writer.Position());
                Writer.WriteUTF16Text(G.Name, true);
                if (!G.LogicInterruptName) {
                    Writer.Fill<int64_t>("LogicInterruptNameOffset" + Index, 0);
                } else {
                    Writer.Fill<int64_t>("LogicInterruptNameOffset" + Index, Writer.Position());
                    Writer.WriteUTF16Text(*G.LogicInterruptName, true);
                }
            } else {
                Writer.Fill<uint32_t>("NameOffset" + Index, static_cast<uint32_t>(Writer.Position()));
                Writer.WriteShiftJIS(G.Name, true);
                if (!G.LogicInterruptName) {
                    Writer.Fill<uint32_t>("LogicInterruptNameOffset" + Index, 0);
                } else {
                    Writer.Fill<uint32_t>("LogicInterruptNameOffset" + Index, static_cast<uint32_t>(Writer.Position()));
                    Writer.WriteShiftJIS(*G.LogicInterruptName, true);
                }
            }
        }
        Writer.Align(0x10);
    }
}  // namespace Souls
