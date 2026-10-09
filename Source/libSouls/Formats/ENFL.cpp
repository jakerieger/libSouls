//
// Created by Jake Rieger on 10/8/2026.
//

#include "ENFL.hpp"

#include <libSouls/Util.hpp>

namespace Souls {
    bool ENFL::IsImpl(BinaryReader& Reader) {
        if (Reader.Length() < 4) {
            return false;
        }
        return Reader.GetASCII(0, 4) == "ENFL";
    }

    void ENFL::ReadImpl(BinaryReader& Source) {
        Source.Order = Endian::Little;
        Source.AssertMagic("ENFL");
        Source.Assert<int32_t>(0x10415);  // Probably 4 bytes
        const int32_t CompressedSize = Source.ReadInt32();
        Source.ReadInt32();  // uncompressed size

        BinaryReader Reader(Util::ReadZlib(Source, CompressedSize));
        Reader.Assert<int32_t>(0);
        const int32_t Count1 = Reader.ReadInt32();
        const int32_t Count2 = Reader.ReadInt32();
        Reader.Assert<int32_t>(0);

        Struct1s.clear();
        for (int32_t I = 0; I < Count1; ++I) {
            Struct1 S;
            S.Step  = Reader.ReadInt16();
            S.Index = Reader.ReadInt16();
            Struct1s.push_back(S);
        }
        Reader.Align(0x10);

        Struct2s.clear();
        for (int32_t I = 0; I < Count2; ++I) {
            Struct2s.push_back({Reader.ReadInt64()});
        }
        Reader.Align(0x10);

        Reader.Assert<int16_t>(0);
        Strings.clear();
        for (int32_t I = 0; I < Count2; ++I) {
            Strings.push_back(Reader.ReadUTF16Text());
        }
    }

    void ENFL::WriteImpl(BinaryWriter& Writer) {
        BinaryWriter Data;
        Data.WriteInt32(0);
        Data.WriteInt32(static_cast<int32_t>(Struct1s.size()));
        Data.WriteInt32(static_cast<int32_t>(Struct2s.size()));
        Data.WriteInt32(0);
        for (const Struct1& S : Struct1s) {
            Data.WriteInt16(S.Step);
            Data.WriteInt16(S.Index);
        }
        Data.Align(0x10);
        for (const Struct2& S : Struct2s) {
            Data.WriteInt64(S.Unk1);
        }
        Data.Align(0x10);
        Data.WriteInt16(0);
        for (const std::string& Text : Strings) {
            Data.WriteUTF16Text(Text, true);
        }
        Data.Align(0x10);
        const std::vector<uint8_t> Bytes = Data.ToBytes();

        Writer.WriteMagic("ENFL");
        Writer.WriteInt32(0x10415);
        Writer.Reserve<int32_t>("CompressedSize");
        Writer.WriteInt32(static_cast<int32_t>(Bytes.size()));
        const int CompressedSize = Util::WriteZlib(Writer, 0xDA, Bytes);
        Writer.Fill<int32_t>("CompressedSize", CompressedSize);
    }
}  // namespace Souls
