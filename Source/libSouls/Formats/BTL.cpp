//
// Created by Jake Rieger on 10/8/2026.
//

#include "BTL.hpp"

namespace Souls {
    template<size_t N>
    static std::array<uint8_t, N> ReadBytesArray(BinaryReader& Reader) {
        std::array<uint8_t, N> Result{};
        Reader.ReadInto(std::span<uint8_t>(Result));
        return Result;
    }

    template<size_t N>
    static void WriteBytesArray(BinaryWriter& Writer, const std::array<uint8_t, N>& Bytes) {
        Writer.WriteBytes(Bytes);
    }

    void BTL::ReadImpl(BinaryReader& Reader) {
        Reader.Order = Endian::Little;
        Reader.Assert<int32_t>(2);
        Version                     = Reader.Assert<int32_t>(1, 2, 5, 6, 15, 16, 18);
        const int32_t LightCount    = Reader.ReadInt32();
        const int32_t NamesLength   = Reader.ReadInt32();
        Reader.Assert<int32_t>(0);
        const int32_t LightSize = Reader.Assert<int32_t>(0xC0, 0xC8, 0xE8, 0xF0);
        Reader.AssertPattern(0x24, 0);
        LongOffsets       = LightSize != 0xC0;
        ExtendedLights    = LightSize == 0xF0;
        Reader.VarintLong = LongOffsets;

        const int64_t NamesStart = Reader.Position();
        Reader.Skip(NamesLength);

        Lights.clear();
        Lights.reserve(static_cast<size_t>(LightCount));
        for (int32_t I = 0; I < LightCount; ++I) {
            Light L;
            L.Unk00 = ReadBytesArray<16>(Reader);
            L.Name  = Reader.GetUTF16Text(NamesStart + Reader.ReadVarint());
            L.Type  = static_cast<LightType>(Reader.ReadUInt32());
            L.Unk1C = Reader.ReadBool();
            L.DiffuseColor  = ReadRGB(Reader);
            L.DiffusePower  = Reader.ReadFloat();
            L.SpecularColor = ReadRGB(Reader);
            L.CastShadows   = Reader.ReadBool();
            L.SpecularPower = Reader.ReadFloat();
            L.ConeAngle     = Reader.ReadFloat();
            L.Unk30         = Reader.ReadFloat();
            L.Unk34         = Reader.ReadFloat();
            L.Position      = Reader.ReadVector3();
            L.Rotation      = Reader.ReadVector3();
            L.Unk50         = Reader.ReadInt32();
            L.Unk54         = Reader.ReadFloat();
            L.Radius        = Reader.ReadFloat();
            L.Unk5C         = Reader.ReadInt32();
            Reader.Assert<int32_t>(0);
            L.Unk64 = ReadBytesArray<4>(Reader);
            L.Unk68 = Reader.ReadFloat();
            L.ShadowColor = ReadRGBA(Reader);
            L.Unk70                 = Reader.ReadFloat();
            L.FlickerIntervalMin    = Reader.ReadFloat();
            L.FlickerIntervalMax    = Reader.ReadFloat();
            L.FlickerBrightnessMult = Reader.ReadFloat();
            L.Unk80                 = Reader.ReadInt32();
            L.Unk84                 = ReadBytesArray<4>(Reader);
            L.Unk88                 = Reader.ReadFloat();
            Reader.Assert<int32_t>(0);
            L.Unk90 = Reader.ReadFloat();
            Reader.Assert<int32_t>(0);
            L.Unk98    = Reader.ReadFloat();
            L.NearClip = Reader.ReadFloat();
            L.UnkA0    = ReadBytesArray<4>(Reader);
            L.Sharpness = Reader.ReadFloat();
            Reader.Assert<int32_t>(0);
            L.UnkAC = Reader.ReadFloat();
            Reader.AssertVarint(0);
            L.Width = Reader.ReadFloat();
            L.UnkBC = Reader.ReadFloat();
            L.UnkC0 = ReadBytesArray<4>(Reader);
            L.UnkC4 = Reader.ReadFloat();
            if (Version >= 16) {
                L.UnkC8 = Reader.ReadFloat();
                L.UnkCC = Reader.ReadFloat();
                L.UnkD0 = Reader.ReadFloat();
                L.UnkD4 = Reader.ReadFloat();
                L.UnkD8 = Reader.ReadFloat();
                L.UnkDC = Reader.ReadInt32();
                L.UnkE0 = Reader.ReadFloat();
                L.UnkE4 = Reader.ReadInt32();
            }
            if (ExtendedLights) {
                L.UnkE8 = Reader.ReadInt32();
                L.UnkEC = Reader.ReadInt32();
            }
            Lights.push_back(std::move(L));
        }
    }

    void BTL::WriteImpl(BinaryWriter& Writer) {
        Writer.Order      = Endian::Little;
        Writer.VarintLong = LongOffsets;
        Writer.WriteInt32(2);
        Writer.WriteInt32(Version);
        Writer.WriteInt32(static_cast<int32_t>(Lights.size()));
        Writer.Reserve<int32_t>("NamesLength");
        Writer.WriteInt32(0);
        Writer.WriteInt32(ExtendedLights ? 0xF0 : (Version >= 16 ? 0xE8 : (LongOffsets ? 0xC8 : 0xC0)));
        Writer.Pad(0x24);

        const int64_t NamesStart = Writer.Position();
        std::vector<int64_t> NameOffsets;
        NameOffsets.reserve(Lights.size());
        for (const Light& L : Lights) {
            const int64_t NameOffset = Writer.Position() - NamesStart;
            NameOffsets.push_back(NameOffset);
            Writer.WriteUTF16Text(L.Name, true);
            if (NameOffset % 0x10 != 0) {
                Writer.Pad(static_cast<size_t>(0x10 - (NameOffset % 0x10)));
            }
        }
        Writer.Fill<int32_t>("NamesLength", static_cast<int32_t>(Writer.Position() - NamesStart));

        for (size_t I = 0; I < Lights.size(); ++I) {
            const Light& L = Lights[I];
            WriteBytesArray(Writer, L.Unk00);
            Writer.WriteVarint(NameOffsets[I]);
            Writer.WriteUInt32(static_cast<uint32_t>(L.Type));
            Writer.WriteBool(L.Unk1C);
            WriteRGB(Writer, L.DiffuseColor);
            Writer.WriteFloat(L.DiffusePower);
            WriteRGB(Writer, L.SpecularColor);
            Writer.WriteBool(L.CastShadows);
            Writer.WriteFloat(L.SpecularPower);
            Writer.WriteFloat(L.ConeAngle);
            Writer.WriteFloat(L.Unk30);
            Writer.WriteFloat(L.Unk34);
            Writer.WriteVector3(L.Position);
            Writer.WriteVector3(L.Rotation);
            Writer.WriteInt32(L.Unk50);
            Writer.WriteFloat(L.Unk54);
            Writer.WriteFloat(L.Radius);
            Writer.WriteInt32(L.Unk5C);
            Writer.WriteInt32(0);
            WriteBytesArray(Writer, L.Unk64);
            Writer.WriteFloat(L.Unk68);
            WriteRGBA(Writer, L.ShadowColor);
            Writer.WriteFloat(L.Unk70);
            Writer.WriteFloat(L.FlickerIntervalMin);
            Writer.WriteFloat(L.FlickerIntervalMax);
            Writer.WriteFloat(L.FlickerBrightnessMult);
            Writer.WriteInt32(L.Unk80);
            WriteBytesArray(Writer, L.Unk84);
            Writer.WriteFloat(L.Unk88);
            Writer.WriteInt32(0);
            Writer.WriteFloat(L.Unk90);
            Writer.WriteInt32(0);
            Writer.WriteFloat(L.Unk98);
            Writer.WriteFloat(L.NearClip);
            WriteBytesArray(Writer, L.UnkA0);
            Writer.WriteFloat(L.Sharpness);
            Writer.WriteInt32(0);
            Writer.WriteFloat(L.UnkAC);
            Writer.WriteVarint(0);
            Writer.WriteFloat(L.Width);
            Writer.WriteFloat(L.UnkBC);
            WriteBytesArray(Writer, L.UnkC0);
            Writer.WriteFloat(L.UnkC4);
            if (Version >= 16) {
                Writer.WriteFloat(L.UnkC8);
                Writer.WriteFloat(L.UnkCC);
                Writer.WriteFloat(L.UnkD0);
                Writer.WriteFloat(L.UnkD4);
                Writer.WriteFloat(L.UnkD8);
                Writer.WriteInt32(L.UnkDC);
                Writer.WriteFloat(L.UnkE0);
                Writer.WriteInt32(L.UnkE4);
            }
            if (ExtendedLights) {
                Writer.WriteInt32(L.UnkE8);
                Writer.WriteInt32(L.UnkEC);
            }
        }
    }
}  // namespace Souls
