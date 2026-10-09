//
// Created by Jake Rieger on 10/8/2026.
//

#pragma once

#include <libSouls/SoulsFile.hpp>

#include <map>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of an exported class; only touched by this DLL's own code

    // CCM: a font layout file used in Demon's Souls and the Dark Souls games; determines the texture region used for
    // each character code. Extension: .ccm
    class SOULS_API CCM : public SoulsFile<CCM> {
    public:
        // Which game the CCM should be formatted for. (The values are really two shorts.)
        enum class CCMVer : uint32_t {
            DemonsSouls = 0x100,
            DarkSouls1  = 0x10001,
            DarkSouls2  = 0x20000,  // and Dark Souls III
        };

        // An individual character in the font.
        struct Glyph {
            // UV of the top-left corner of the glyph's texture region.
            Vector2 UV1;
            // UV of the bottom-right corner.
            Vector2 UV2;
            // Padding before the character.
            int16_t PreSpace = 0;
            // Width of the character texture.
            int16_t Width = 0;
            // Distance to the next character.
            int16_t Advance = 0;
            // Index of the font texture with this character.
            int16_t TexIndex = 0;
        };

        CCMVer Version = CCMVer::DarkSouls1;
        // Maximum width of a glyph; glyph widths are relative to this.
        int16_t FullWidth = 0;
        // Size of the font textures.
        int16_t TexWidth  = 0;
        int16_t TexHeight = 0;
        // Only meaningful in Demon's Souls and Dark Souls; always 0 or 0x20.
        int16_t Unk0E = 0;
        // Always 1 in Demon's Souls, 1 or 4 in Dark Souls, and 4 in Dark Souls II.
        uint8_t Unk1C = 0;
        // Always 1 in Demon's Souls and 0 in Dark Souls and II.
        uint8_t Unk1D = 0;
        // Number of separate font textures.
        uint8_t TexCount = 0;
        // Individual characters in this font, keyed by their code.
        std::map<int32_t, Glyph> Glyphs;

    protected:
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
