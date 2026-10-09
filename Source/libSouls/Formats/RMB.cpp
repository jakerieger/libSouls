//
// Created by Jake Rieger on 10/8/2026.
//

#include "RMB.hpp"

namespace Souls {
    static std::vector<RMB::State> ReadStates(BinaryReader& Reader, int16_t Count, int32_t Offset) {
        std::vector<RMB::State> States;
        if (Count > 0) {
            Reader.Seek(Offset);
            for (int16_t I = 0; I < Count; ++I) {
                RMB::State S;
                S.Start         = Reader.ReadInt16();
                S.Duration      = Reader.ReadInt16();
                S.BeginStrength = Reader.ReadByte();
                S.EndStrength   = Reader.ReadByte();
                Reader.Assert<int16_t>(0);
                States.push_back(S);
            }
        }
        return States;
    }

    void RMB::ReadImpl(BinaryReader& Reader) {
        Reader.Order = Endian::Little;
        BigEndian    = Reader.ReadAt<int32_t>(4) == 0x10000000;
        Reader.Order = BigEndian ? Endian::Big : Endian::Little;

        const int16_t RumbleCount = Reader.ReadInt16();
        Reader.Assert<int16_t>(0);
        Reader.Assert<int32_t>(0x10);
        Reader.Assert<int32_t>(0);
        Reader.Assert<int32_t>(0);

        Rumbles.clear();
        Rumbles.reserve(static_cast<size_t>(RumbleCount));
        for (int16_t I = 0; I < RumbleCount; ++I) {
            Rumble R;
            const int16_t HeavyCount = Reader.ReadInt16();
            const int16_t LightCount = Reader.ReadInt16();
            Reader.Assert<int32_t>(0);
            const int32_t HeavyOffset = Reader.ReadInt32();
            const int32_t LightOffset = Reader.ReadInt32();

            const int64_t Position = Reader.Position();
            R.HeavyStates          = ReadStates(Reader, HeavyCount, HeavyOffset);
            R.LightStates          = ReadStates(Reader, LightCount, LightOffset);
            Reader.Seek(Position);
            Rumbles.push_back(std::move(R));
        }
    }

    void RMB::WriteImpl(BinaryWriter& Writer) {
        Writer.Order = BigEndian ? Endian::Big : Endian::Little;
        Writer.WriteInt16(static_cast<int16_t>(Rumbles.size()));
        Writer.WriteInt16(0);
        Writer.WriteInt32(0x10);
        Writer.WriteInt32(0);
        Writer.WriteInt32(0);

        for (size_t I = 0; I < Rumbles.size(); ++I) {
            Writer.WriteInt16(static_cast<int16_t>(Rumbles[I].HeavyStates.size()));
            Writer.WriteInt16(static_cast<int16_t>(Rumbles[I].LightStates.size()));
            Writer.WriteInt32(0);
            Writer.Reserve<int32_t>("HeavyOffset[" + std::to_string(I) + "]");
            Writer.Reserve<int32_t>("LightOffset[" + std::to_string(I) + "]");
        }

        const auto WriteStates = [&](const std::vector<State>& States) -> int32_t {
            if (States.empty()) {
                return 0;
            }
            const int32_t Offset = static_cast<int32_t>(Writer.Position());
            for (const State& S : States) {
                Writer.WriteInt16(S.Start);
                Writer.WriteInt16(S.Duration);
                Writer.WriteByte(S.BeginStrength);
                Writer.WriteByte(S.EndStrength);
                Writer.WriteInt16(0);
            }
            return Offset;
        };
        for (size_t I = 0; I < Rumbles.size(); ++I) {
            Writer.Fill<int32_t>("HeavyOffset[" + std::to_string(I) + "]", WriteStates(Rumbles[I].HeavyStates));
            Writer.Fill<int32_t>("LightOffset[" + std::to_string(I) + "]", WriteStates(Rumbles[I].LightStates));
        }
    }
}  // namespace Souls
