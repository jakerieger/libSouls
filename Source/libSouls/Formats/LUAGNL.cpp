//
// Created by Jake Rieger on 10/8/2026.
//

#include "LUAGNL.hpp"

namespace Souls {
    void LUAGNL::ReadImpl(BinaryReader& Reader) {
        BigEndian    = Reader.ReadAt<int16_t>(0) == 0;
        Reader.Order = BigEndian ? Endian::Big : Endian::Little;
        LongFormat   = Reader.ReadAt<int32_t>(BigEndian ? 0 : 4) == 0;

        Globals.clear();
        int64_t Offset;
        do {
            Offset = LongFormat ? Reader.ReadInt64() : static_cast<int64_t>(Reader.ReadUInt32());
            if (Offset != 0) {
                Globals.push_back(LongFormat ? Reader.GetUTF16Text(Offset) : Reader.GetShiftJIS(Offset));
            }
        } while (Offset != 0);
    }

    void LUAGNL::WriteImpl(BinaryWriter& Writer) {
        Writer.Order = BigEndian ? Endian::Big : Endian::Little;

        for (size_t I = 0; I < Globals.size(); ++I) {
            if (LongFormat) {
                Writer.Reserve<int64_t>("Offset" + std::to_string(I));
            } else {
                Writer.Reserve<uint32_t>("Offset" + std::to_string(I));
            }
        }
        if (LongFormat) {
            Writer.WriteInt64(0);
        } else {
            Writer.WriteUInt32(0);
        }

        for (size_t I = 0; I < Globals.size(); ++I) {
            const std::string Name = "Offset" + std::to_string(I);
            if (LongFormat) {
                Writer.Fill<int64_t>(Name, Writer.Position());
                Writer.WriteUTF16Text(Globals[I], true);
            } else {
                Writer.Fill<uint32_t>(Name, static_cast<uint32_t>(Writer.Position()));
                Writer.WriteShiftJIS(Globals[I], true);
            }
        }
        Writer.Align(0x10);
    }
}  // namespace Souls
