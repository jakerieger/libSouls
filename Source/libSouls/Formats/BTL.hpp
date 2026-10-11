// Copyright (C) 2026 Jake Rieger
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, version 3. This program is distributed in the hope that it will
// be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU General
// Public License along with this program. If not, see https://www.gnu.org/licenses/.

#pragma once

#include <libSouls/Color.hpp>
#include <libSouls/SoulsFile.hpp>

#include <array>
#include <string>
#include <vector>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of an exported class; only touched by this DLL's own code

    /// @brief BTL: point light sources in a map, used in Dark Souls II and later.
    ///
    /// Extension: .btl
    class SOULS_API BTL : public SoulsFile<BTL> {
    public:
        /// @brief Type of a light source.
        enum class LightType : uint32_t {
            Point       = 0,  ///< omnidirectional light
            Spot        = 1,  ///< cone of light
            Directional = 2,  ///< light at a constant angle
        };

        /// @brief An omnidirectional and/or spot light source.
        struct Light {
            /// @brief Unknown.
            std::array<uint8_t, 16> Unk00 {};
            /// @brief Name of this light.
            std::string Name;
            /// @brief Unknown.
            LightType Type = LightType::Point;
            /// @brief Unknown.
            bool Unk1C = true;
            /// @brief Color of the light on diffuse surfaces (alpha is ignored).
            Color DiffuseColor = Color::White();
            /// @brief Intensity of diffuse lighting.
            float DiffusePower = 1;
            /// @brief Color of the light on reflective surfaces (alpha is ignored).
            Color SpecularColor = Color::White();
            /// @brief Whether the light casts shadows.
            bool CastShadows = false;
            /// @brief Intensity of specular lighting.
            float SpecularPower = 1;
            /// @brief Tightness of the spot light beam.
            float ConeAngle = 0;
            /// @brief Unknown.
            float Unk30 = 0;
            /// @brief Unknown.
            float Unk34 = 0;
            /// @brief Center of the light.
            Vector3 Position;
            /// @brief Rotation of a spot light.
            Vector3 Rotation;
            /// @brief Unknown.
            int32_t Unk50 = 4;
            /// @brief Unknown.
            float Unk54 = 0;
            /// @brief Distance the light shines.
            float Radius = 10;
            /// @brief Unknown.
            int32_t Unk5C = -1;
            /// @brief Unknown; 4 bytes.
            std::array<uint8_t, 4> Unk64 {0, 0, 0, 1};
            /// @brief Unknown.
            float Unk68 = 0;
            /// @brief Color of shadows cast by the light; alpha is relative to 100.
            Color ShadowColor = {100, 0, 0, 0};
            /// @brief Unknown.
            float Unk70 = 0;
            /// @brief Minimum and maximum time between flickers.
            float FlickerIntervalMin = 0;
            /// @brief Maximum time between flickers.
            float FlickerIntervalMax = 0;
            /// @brief Multiplies the brightness of the light while flickering.
            float FlickerBrightnessMult = 1;
            /// @brief Unknown.
            int32_t Unk80 = -1;
            /// @brief Unknown; 4 bytes.
            std::array<uint8_t, 4> Unk84 {};
            /// @brief Unknown.
            float Unk88 = 0;
            /// @brief Unknown.
            float Unk90 = 0;
            /// @brief Unknown.
            float Unk98 = 1;
            /// @brief Distance at which spot light beam starts.
            float NearClip = 1;
            /// @brief Unknown; 4 bytes.
            std::array<uint8_t, 4> UnkA0 {1, 0, 2, 1};
            /// @brief Unknown.
            float Sharpness = 1;
            /// @brief Unknown.
            float UnkAC = 0;
            /// @brief Stretches the spot light beam.
            float Width = 0;
            /// @brief Unknown.
            float UnkBC = 0;
            /// @brief Unknown; 4 bytes.
            std::array<uint8_t, 4> UnkC0 {};
            /// @brief Unknown.
            float UnkC4 = 0;
            /// @brief Not present before Sekiro.
            float UnkC8 = 0;
            /// @brief Unknown; not present before Sekiro.
            float UnkCC = 0;
            /// @brief Unknown; not present before Sekiro.
            float UnkD0 = 0;
            /// @brief Unknown; not present before Sekiro.
            float UnkD4 = 0;
            /// @brief Unknown; not present before Sekiro.
            float UnkD8 = 0;
            /// @brief Unknown; not present before Sekiro.
            int32_t UnkDC = 0;
            /// @brief Unknown; not present before Sekiro.
            float UnkE0 = 0;
            /// @brief Unknown; not present before Sekiro.
            int32_t UnkE4 = 0;
            /// @brief Only present when ExtendedLights is set.
            int32_t UnkE8 = 0;
            /// @brief Unknown.
            int32_t UnkEC = 0;
        };

        /// @brief Indicates the version, probably.
        int32_t Version = 16;
        /// @brief Whether offsets are 64-bit; false for Dark Souls II.
        bool LongOffsets = true;
        /// @brief Elden Ring files with light size 0xF0 have two more fields per light.
        bool ExtendedLights = false;
        /// @brief Light sources in this BTL.
        std::vector<Light> Lights;

    protected:
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
