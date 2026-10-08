//
// Created by Jake Rieger on 10/7/2026.
//

#include "DDS.hpp"

#include <string_view>

using namespace std::string_view_literals;

namespace Souls {
    DDS DDS::Read(std::span<const uint8_t> Bytes) {
        BinaryReader Reader(Bytes, Endian::Little);
        DDS Result;

        Reader.AssertMagic("DDS "sv);
        Reader.Assert<int32_t>(0x7C);  // header size
        Result.Flags             = static_cast<DDSD>(Reader.ReadUInt32());
        Result.Height            = Reader.ReadInt32();
        Result.Width             = Reader.ReadInt32();
        Result.PitchOrLinearSize = Reader.ReadInt32();
        Result.Depth             = Reader.ReadInt32();
        Result.MipMapCount       = Reader.ReadInt32();
        for (int32_t& Value : Result.Reserved1) Value = Reader.ReadInt32();

        Reader.Assert<int32_t>(32);  // pixel format size
        Result.Pixels.Flags       = static_cast<DDPF>(Reader.ReadUInt32());
        Result.Pixels.FourCC      = Reader.ReadString(4);
        Result.Pixels.RGBBitCount = Reader.ReadInt32();
        Result.Pixels.RBitMask    = Reader.ReadUInt32();
        Result.Pixels.GBitMask    = Reader.ReadUInt32();
        Result.Pixels.BBitMask    = Reader.ReadUInt32();
        Result.Pixels.ABitMask    = Reader.ReadUInt32();

        Result.Caps      = static_cast<DDSCAPS>(Reader.ReadUInt32());
        Result.Caps2     = static_cast<DDSCAPS2>(Reader.ReadUInt32());
        Result.Caps3     = Reader.ReadInt32();
        Result.Caps4     = Reader.ReadInt32();
        Result.Reserved2 = Reader.ReadInt32();

        if (Result.Pixels.FourCC == "DX10") {
            Header10 Extension;
            Extension.Format           = static_cast<DXGIFormat>(Reader.ReadUInt32());
            Extension.ResourceDimension = static_cast<Dimension>(Reader.ReadUInt32());
            Extension.MiscFlag         = static_cast<ResourceMisc>(Reader.ReadUInt32());
            Extension.ArraySize        = Reader.ReadUInt32();
            Extension.MiscFlags2       = static_cast<AlphaMode>(Reader.ReadUInt32());
            Result.DX10                = Extension;
        }
        return Result;
    }

    std::vector<uint8_t> DDS::Write(std::span<const uint8_t> PixelData) const {
        BinaryWriter Writer(Endian::Little);

        Writer.WriteMagic("DDS "sv);
        Writer.WriteInt32(0x7C);
        Writer.WriteUInt32(static_cast<uint32_t>(Flags));
        Writer.WriteInt32(Height);
        Writer.WriteInt32(Width);
        Writer.WriteInt32(PitchOrLinearSize);
        Writer.WriteInt32(Depth);
        Writer.WriteInt32(MipMapCount);
        for (const int32_t Value : Reserved1) Writer.WriteInt32(Value);

        Writer.WriteInt32(32);
        Writer.WriteUInt32(static_cast<uint32_t>(Pixels.Flags));
        std::string FourCC = Pixels.FourCC;  // make sure it's 4 characters
        FourCC.resize(4, '\0');
        Writer.WriteString(FourCC, false);
        Writer.WriteInt32(Pixels.RGBBitCount);
        Writer.WriteUInt32(Pixels.RBitMask);
        Writer.WriteUInt32(Pixels.GBitMask);
        Writer.WriteUInt32(Pixels.BBitMask);
        Writer.WriteUInt32(Pixels.ABitMask);

        Writer.WriteUInt32(static_cast<uint32_t>(Caps));
        Writer.WriteUInt32(static_cast<uint32_t>(Caps2));
        Writer.WriteInt32(Caps3);
        Writer.WriteInt32(Caps4);
        Writer.WriteInt32(Reserved2);

        if (Pixels.FourCC == "DX10") {
            const Header10 Extension = DX10.value_or(Header10{});
            Writer.WriteUInt32(static_cast<uint32_t>(Extension.Format));
            Writer.WriteUInt32(static_cast<uint32_t>(Extension.ResourceDimension));
            Writer.WriteUInt32(static_cast<uint32_t>(Extension.MiscFlag));
            Writer.WriteUInt32(Extension.ArraySize);
            Writer.WriteUInt32(static_cast<uint32_t>(Extension.MiscFlags2));
        }

        Writer.WriteBytes(PixelData);
        return Writer.ToBytes();
    }
}  // namespace Souls
