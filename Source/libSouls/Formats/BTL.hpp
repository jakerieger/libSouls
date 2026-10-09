//
// Created by Jake Rieger on 10/8/2026.
//

#pragma once

#include <libSouls/Color.hpp>
#include <libSouls/SoulsFile.hpp>

#include <array>
#include <string>
#include <vector>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of an exported class; only touched by this DLL's own code

    // BTL: point light sources in a map, used in Dark Souls II and later. Extension: .btl
    class SOULS_API BTL : public SoulsFile<BTL> {
    public:
        // Type of a light source.
        enum class LightType : uint32_t {
            Point       = 0,  // omnidirectional light
            Spot        = 1,  // cone of light
            Directional = 2,  // light at a constant angle
        };

        // An omnidirectional and/or spot light source.
        struct Light {
            std::array<uint8_t, 16> Unk00{};
            std::string Name;
            LightType Type = LightType::Point;
            bool Unk1C     = true;
            // Color of the light on diffuse surfaces (alpha is ignored).
            Color DiffuseColor = Color::White();
            // Intensity of diffuse lighting.
            float DiffusePower = 1;
            // Color of the light on reflective surfaces (alpha is ignored).
            Color SpecularColor = Color::White();
            // Whether the light casts shadows.
            bool CastShadows = false;
            // Intensity of specular lighting.
            float SpecularPower = 1;
            // Tightness of the spot light beam.
            float ConeAngle = 0;
            float Unk30     = 0;
            float Unk34     = 0;
            // Center of the light.
            Vector3 Position;
            // Rotation of a spot light.
            Vector3 Rotation;
            int32_t Unk50 = 4;
            float Unk54   = 0;
            // Distance the light shines.
            float Radius  = 10;
            int32_t Unk5C = -1;
            std::array<uint8_t, 4> Unk64{0, 0, 0, 1};
            float Unk68 = 0;
            // Color of shadows cast by the light; alpha is relative to 100.
            Color ShadowColor = {100, 0, 0, 0};
            float Unk70       = 0;
            // Minimum and maximum time between flickers.
            float FlickerIntervalMin = 0;
            float FlickerIntervalMax = 0;
            // Multiplies the brightness of the light while flickering.
            float FlickerBrightnessMult = 1;
            int32_t Unk80               = -1;
            std::array<uint8_t, 4> Unk84{};
            float Unk88 = 0;
            float Unk90 = 0;
            float Unk98 = 1;
            // Distance at which spot light beam starts.
            float NearClip = 1;
            std::array<uint8_t, 4> UnkA0{1, 0, 2, 1};
            float Sharpness = 1;
            float UnkAC     = 0;
            // Stretches the spot light beam.
            float Width = 0;
            float UnkBC = 0;
            std::array<uint8_t, 4> UnkC0{};
            float UnkC4 = 0;
            // Not present before Sekiro.
            float UnkC8   = 0;
            float UnkCC   = 0;
            float UnkD0   = 0;
            float UnkD4   = 0;
            float UnkD8   = 0;
            int32_t UnkDC = 0;
            float UnkE0   = 0;
            int32_t UnkE4 = 0;
            // Only present when ExtendedLights is set.
            int32_t UnkE8 = 0;
            int32_t UnkEC = 0;
        };

        // Indicates the version, probably.
        int32_t Version = 16;
        // Whether offsets are 64-bit; false for Dark Souls II.
        bool LongOffsets = true;
        // Elden Ring files with light size 0xF0 have two more fields per light.
        bool ExtendedLights = false;
        std::vector<Light> Lights;

    protected:
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
