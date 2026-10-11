// Copyright (C) 2026 Jake Rieger
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, version 3. This program is distributed in the hope that it will
// be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU General
// Public License along with this program. If not, see https://www.gnu.org/licenses/.

#pragma once

#include <libSouls/SoulsFile.hpp>

#include <string>
#include <variant>
#include <vector>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of an exported class; only touched by this DLL's own code

    /// @brief MTD: a material definition format used in all the Souls games.
    ///
    /// Extension: .mtd
    class SOULS_API MTD : public SoulsFile<MTD> {
    public:
        /// @brief Value types of MTD params.
        ///
        /// (The engine probably supports Bool2-4 and Int3-4 as well, but they are never used.)
        enum class ParamType {
            Bool,    ///< a one-byte boolean value
            Int,     ///< a four-byte integer
            Int2,    ///< an array of two four-byte integers
            Float,   ///< a four-byte floating point number
            Float2,  ///< an array of two floats
            Float3,  ///< an array of three floats
            Float4,  ///< an array of four floats
        };

        /// @brief A param's value: bool, int32_t or float for the single-value types, a vector of int32_t for Int2, and
        ///        a vector of float for the multi-float types.
        using ParamValue = std::variant<bool, int32_t, float, std::vector<int32_t>, std::vector<float>>;

        /// @brief The blending mode of the material, used in value g_BlendMode.
        enum class BlendMode {
            Normal      = 0,
            TexEdge     = 1,
            Blend       = 2,
            Water       = 3,
            Add         = 4,
            Sub         = 5,
            Mul         = 6,
            AddMul      = 7,
            SubMul      = 8,
            WaterWave   = 9,
            LSNormal    = 32,
            LSTexEdge   = 33,
            LSBlend     = 34,
            LSWater     = 35,
            LSAdd       = 36,
            LSSub       = 37,
            LSMul       = 38,
            LSAddMul    = 39,
            LSSubMul    = 40,
            LSWaterWave = 41,
        };

        /// @brief The lighting type of a material, used in value g_LightingType.
        enum class LightingType {
            None           = 0,
            HemDirDifSpcx3 = 1,
            HemEnvDifSpc   = 3,
        };

        /// @brief A value defining the material's properties.
        struct Param {
            /// @brief The name of the param.
            std::string Name;
            /// @brief The type of this value.
            ParamType Type = ParamType::Int;
            /// @brief The value itself.
            ParamValue Value = int32_t {0};

            Param() = default;
            /// @brief Creates a param with the default (zero) value for the type.
            Param(std::string Name, ParamType Type);
            /// @brief A param with a name, type and value.
            Param(std::string Name, ParamType Type, ParamValue Value)
                : Name(std::move(Name)), Type(Type), Value(std::move(Value)) {}
        };

        /// @brief Texture types used by the material, filled in in each FLVER.
        struct Texture {
            /// @brief The type of texture (g_Diffuse, g_Specular, etc).
            std::string Type = "g_DiffuseTexture";
            /// @brief Whether the texture has extended information for Sekiro.
            bool Extended = false;
            /// @brief Indicates the order of UVs in FLVER vertex data.
            int32_t UVNumber = 0;
            /// @brief Unknown.
            int32_t ShaderDataIndex = 0;
            /// @brief A fixed texture path for this material, only used in Sekiro.
            std::string Path;
            /// @brief Floats for an unknown purpose, only used in Sekiro.
            std::vector<float> UnkFloats;
        };

        /// @brief A path to the shader source file, which also determines which compiled shader to use for this
        ///        material.
        std::string ShaderPath = "Unknown.spx";
        /// @brief A description of this material's purpose.
        std::string Description;
        /// @brief Values determining material properties.
        std::vector<Param> Params;
        /// @brief Texture types required by the material shader.
        std::vector<Texture> Textures;

    protected:
        bool IsImpl(BinaryReader& Reader) override;
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
