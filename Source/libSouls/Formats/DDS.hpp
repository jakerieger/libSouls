//
// Created by Jake Rieger on 10/7/2026.
//

#pragma once

#include <libSouls/Souls.hpp>
#include <libSouls/BinaryReader.hpp>
#include <libSouls/BinaryWriter.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace Souls {

    // The header of a DirectDraw Surface (.dds) file, which is how PC textures are stored in a TPF. This only handles
    // the header (and its optional DX10 extension); the pixel data after it is left alone.
    class SOULS_API DDS {
    public:
        // Which header fields are valid.
        enum class DDSD : uint32_t {
            CAPS        = 0x1,
            HEIGHT      = 0x2,
            WIDTH       = 0x4,
            PITCH       = 0x8,
            PIXELFORMAT = 0x1000,
            MIPMAPCOUNT = 0x20000,
            LINEARSIZE  = 0x80000,
            DEPTH       = 0x800000,
        };

        enum class DDSCAPS : uint32_t {
            COMPLEX = 0x8,
            TEXTURE = 0x1000,
            MIPMAP  = 0x400000,
        };

        enum class DDSCAPS2 : uint32_t {
            CUBEMAP           = 0x200,
            CUBEMAP_POSITIVEX = 0x400,
            CUBEMAP_NEGATIVEX = 0x800,
            CUBEMAP_POSITIVEY = 0x1000,
            CUBEMAP_NEGATIVEY = 0x2000,
            CUBEMAP_POSITIVEZ = 0x4000,
            CUBEMAP_NEGATIVEZ = 0x8000,
            VOLUME            = 0x200000,
        };

        enum class DDPF : uint32_t {
            ALPHAPIXELS = 0x1,
            ALPHA       = 0x2,
            FOURCC      = 0x4,
            RGB         = 0x40,
            YUV         = 0x200,
            LUMINANCE   = 0x20000,
        };

        enum class Dimension : uint32_t {
            TEXTURE1D = 2,
            TEXTURE2D = 3,
            TEXTURE3D = 4,
        };

        enum class ResourceMisc : uint32_t {
            TEXTURECUBE = 0x4,
        };

        enum class AlphaMode : uint32_t {
            UNKNOWN       = 0,
            STRAIGHT      = 1,
            PREMULTIPLIED = 2,
            OPAQUE_       = 3,
            CUSTOM        = 4,
        };

        // Microsoft's DXGI_FORMAT.
        enum class DXGIFormat : uint32_t {
            UNKNOWN,
            R32G32B32A32_TYPELESS,
            R32G32B32A32_FLOAT,
            R32G32B32A32_UINT,
            R32G32B32A32_SINT,
            R32G32B32_TYPELESS,
            R32G32B32_FLOAT,
            R32G32B32_UINT,
            R32G32B32_SINT,
            R16G16B16A16_TYPELESS,
            R16G16B16A16_FLOAT,
            R16G16B16A16_UNORM,
            R16G16B16A16_UINT,
            R16G16B16A16_SNORM,
            R16G16B16A16_SINT,
            R32G32_TYPELESS,
            R32G32_FLOAT,
            R32G32_UINT,
            R32G32_SINT,
            R32G8X24_TYPELESS,
            D32_FLOAT_S8X24_UINT,
            R32_FLOAT_X8X24_TYPELESS,
            X32_TYPELESS_G8X24_UINT,
            R10G10B10A2_TYPELESS,
            R10G10B10A2_UNORM,
            R10G10B10A2_UINT,
            R11G11B10_FLOAT,
            R8G8B8A8_TYPELESS,
            R8G8B8A8_UNORM,
            R8G8B8A8_UNORM_SRGB,
            R8G8B8A8_UINT,
            R8G8B8A8_SNORM,
            R8G8B8A8_SINT,
            R16G16_TYPELESS,
            R16G16_FLOAT,
            R16G16_UNORM,
            R16G16_UINT,
            R16G16_SNORM,
            R16G16_SINT,
            R32_TYPELESS,
            D32_FLOAT,
            R32_FLOAT,
            R32_UINT,
            R32_SINT,
            R24G8_TYPELESS,
            D24_UNORM_S8_UINT,
            R24_UNORM_X8_TYPELESS,
            X24_TYPELESS_G8_UINT,
            R8G8_TYPELESS,
            R8G8_UNORM,
            R8G8_UINT,
            R8G8_SNORM,
            R8G8_SINT,
            R16_TYPELESS,
            R16_FLOAT,
            D16_UNORM,
            R16_UNORM,
            R16_UINT,
            R16_SNORM,
            R16_SINT,
            R8_TYPELESS,
            R8_UNORM,
            R8_UINT,
            R8_SNORM,
            R8_SINT,
            A8_UNORM,
            R1_UNORM,
            R9G9B9E5_SHAREDEXP,
            R8G8_B8G8_UNORM,
            G8R8_G8B8_UNORM,
            BC1_TYPELESS,
            BC1_UNORM,
            BC1_UNORM_SRGB,
            BC2_TYPELESS,
            BC2_UNORM,
            BC2_UNORM_SRGB,
            BC3_TYPELESS,
            BC3_UNORM,
            BC3_UNORM_SRGB,
            BC4_TYPELESS,
            BC4_UNORM,
            BC4_SNORM,
            BC5_TYPELESS,
            BC5_UNORM,
            BC5_SNORM,
            B5G6R5_UNORM,
            B5G5R5A1_UNORM,
            B8G8R8A8_UNORM,
            B8G8R8X8_UNORM,
            R10G10B10_XR_BIAS_A2_UNORM,
            B8G8R8A8_TYPELESS,
            B8G8R8A8_UNORM_SRGB,
            B8G8R8X8_TYPELESS,
            B8G8R8X8_UNORM_SRGB,
            BC6H_TYPELESS,
            BC6H_UF16,
            BC6H_SF16,
            BC7_TYPELESS,
            BC7_UNORM,
            BC7_UNORM_SRGB,
            AYUV,
            Y410,
            Y416,
            NV12,
            P010,
            P016,
            OPAQUE_420,  // DXGI_FORMAT_420_OPAQUE
            YUY2,
            Y210,
            Y216,
            NV11,
            AI44,
            IA44,
            P8,
            A8P8,
            B4G4R4A4_UNORM,
            P208,
            V208,
            V408,
            FORCE_UINT,
        };

        struct SOULS_API PixelFormat {
            DDPF Flags = static_cast<DDPF>(0);
            // Four characters, e.g. "DXT5" or "DX10".
            std::string FourCC = std::string(4, '\0');
            int32_t RGBBitCount = 0;
            uint32_t RBitMask = 0, GBitMask = 0, BBitMask = 0, ABitMask = 0;
        };

        // Present when the pixel format's FourCC is "DX10".
        struct SOULS_API Header10 {
            DXGIFormat Format        = DXGIFormat::UNKNOWN;
            Dimension ResourceDimension = Dimension::TEXTURE2D;
            ResourceMisc MiscFlag    = static_cast<ResourceMisc>(0);
            uint32_t ArraySize       = 1;
            AlphaMode MiscFlags2     = AlphaMode::UNKNOWN;
        };

        DDSD Flags             = static_cast<DDSD>(0x1 | 0x2 | 0x4 | 0x1000);  // CAPS | HEIGHT | WIDTH | PIXELFORMAT
        int32_t Height         = 0;
        int32_t Width          = 0;
        int32_t PitchOrLinearSize = 0;
        int32_t Depth          = 0;
        int32_t MipMapCount    = 0;
        std::array<int32_t, 11> Reserved1{};
        PixelFormat Pixels;
        DDSCAPS Caps           = DDSCAPS::TEXTURE;
        DDSCAPS2 Caps2         = static_cast<DDSCAPS2>(0);
        int32_t Caps3          = 0;
        int32_t Caps4          = 0;
        int32_t Reserved2      = 0;
        std::optional<Header10> DX10;

        DDS() = default;

        // Parses the header of a DDS file (the data after it is ignored). Throws BinaryException if it isn't one.
        static DDS Read(std::span<const uint8_t> Bytes);

        // Where the pixel data starts in a file with this header: 0x80, or 0x94 with the DX10 extension.
        int32_t DataOffset() const { return Pixels.FourCC == "DX10" ? 0x94 : 0x80; }

        bool IsCubemap() const { return (static_cast<uint32_t>(Caps2) & static_cast<uint32_t>(DDSCAPS2::CUBEMAP)) != 0; }
        bool IsVolume() const { return (static_cast<uint32_t>(Caps2) & static_cast<uint32_t>(DDSCAPS2::VOLUME)) != 0; }

        // A complete DDS file: this header followed by the pixel data.
        std::vector<uint8_t> Write(std::span<const uint8_t> PixelData) const;
    };

}  // namespace Souls
