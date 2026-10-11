/// @file
/// @author Jake Rieger
/// @date 10/7/2026

#pragma once

#include <libSouls/SoulsFile.hpp>
#include <libSouls/Formats/DDS.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of an exported class; only touched by this DLL's own code

    /// @brief TPF: the multi-file texture container used throughout the series.
    ///
    /// Extension: .tpf. On PC each texture is a complete DDS file; the console versions store headerless pixel data
    /// plus a little metadata.
    class SOULS_API TPF : public SoulsFile<TPF> {
    public:
        /// @brief The platform of the game a TPF is for.
        enum class TPFPlatform : uint8_t {
            PC      = 0,  ///< headered DDS with minimal metadata
            Xbox360 = 1,  ///< headerless DDS with pre-DX10 metadata
            PS3     = 2,  ///< headerless DDS with pre-DX10 metadata
            PS4     = 4,  ///< headerless DDS with DX10 metadata
            Xbone   = 5,  ///< headerless DDS with DX10 metadata
        };

        /// @brief What a texture is made of.
        enum class TexType : uint8_t {
            Texture = 0,  ///< one 2D texture
            Cubemap = 1,  ///< six 2D textures
            Volume  = 2,  ///< one 3D texture
        };

        /// @brief Extra metadata for the headerless textures of the console versions.
        struct SOULS_API TexHeader {
            /// @brief Width of the texture, in pixels.
            int16_t Width  = 0;
            /// @brief Height of the texture, in pixels.
            int16_t Height = 0;
            /// @brief 1 for normal textures or 6 for cubemaps (PS4 and Xbox One).
            int32_t TextureCount = 0;
            int32_t Unk1         = 0;  ///< PS3 only
            int32_t Unk2         = 0;  ///< 0, 0x69E0 or 0xAAE4 in Demon's Souls, 0xD in Dark Souls III
            int32_t DXGIFormat   = 0;  ///< Microsoft DXGI_FORMAT; PS4 and Xbox One
        };

        /// @brief Unknown optional data some textures carry.
        struct SOULS_API FloatStruct {
            int32_t Unk00 = 0;  ///< probably some kind of ID
            std::vector<float> Values;  ///< not confirmed to always be floats
        };

        /// @brief A DDS texture in a TPF container.
        struct SOULS_API Texture {
            /// @brief Should not include a path or extension.
            std::string Name = "Unnamed";
            /// @brief Indicates format of the texture.
            uint8_t Format   = 0;
            /// @brief Whether this texture is a cubemap.
            TexType Type     = TexType::Texture;
            /// @brief Number of mipmap levels in this texture.
            uint8_t Mipmaps  = 0;
            uint8_t Flags1   = 0;  ///< 2 and 3 mean the data is compressed with DCP_EDGE (Demon's Souls and ACE:R)
            /// @brief The texture's data: a complete DDS file on PC.
            std::vector<uint8_t> Bytes;
            std::optional<TexHeader> Header;  ///< console platforms
            /// @brief Unknown optional data some textures carry.
            std::optional<FloatStruct> Floats;

            Texture() = default;
            /// @brief A PC texture.
            ///
            /// The type (cubemap, volume or plain) and mipmap count are taken from the DDS header, which throws if
            /// Bytes isn't a DDS.
            Texture(std::string Name, uint8_t Format, uint8_t Flags1, std::vector<uint8_t> Bytes);

            /// @brief The DDS header of a PC texture.
            DDS ReadDDS() const { return DDS::Read(Bytes); }

            /// @brief "[format Type] name", where Type is Texture, Cubemap or Volume.
            std::string ToString() const;
        };

        /// @brief The textures contained within this TPF.
        std::vector<Texture> Textures;
        /// @brief The platform this TPF will be used on.
        TPFPlatform Platform = TPFPlatform::PC;
        /// @brief Name encoding: 0 and 2 are Shift-JIS, 1 is UTF-16.
        uint8_t Encoding = 1;
        uint8_t Flag2    = 3;  ///< unknown
        /// @brief Texture data is padded to a multiple of this when written.
        ///
        /// Padding varies between games (Elden Ring's files have none at all), so a TPF that was read gets the largest
        /// of 16, 8, 4, 2 or 1 that its texture offsets already satisfy, which makes an unchanged file write back the
        /// way it was. New ones use 4.
        uint8_t DataAlignment = 4;

        /// @brief An empty TPF configured for Dark Souls III.
        TPF() = default;

    protected:
        bool IsImpl(BinaryReader& Reader) override;
        void ReadImpl(BinaryReader& Reader) override;
        /// @brief For PC TPFs the textures' types and mipmap counts are refreshed from their DDS headers first.
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
