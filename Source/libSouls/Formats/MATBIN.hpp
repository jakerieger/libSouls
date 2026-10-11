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

    /// @brief MATBIN: a material config format introduced in Elden Ring.
    ///
    /// Extension: .matbin
    class SOULS_API MATBIN : public SoulsFile<MATBIN> {
    public:
        /// @brief Available types for param values.
        enum class ParamType : uint32_t {
            Bool   = 0,   ///< a 1-byte boolean
            Int    = 4,   ///< a 32-bit integer
            Int2   = 5,   ///< two 32-bit integers
            Float  = 8,   ///< a 32-bit float
            Float2 = 9,   ///< two 32-bit floats
            Float3 = 10,  ///< three 32-bit floats
            Float4 = 11,  ///< four 32-bit floats
            Float5 = 12,  ///< five 32-bit floats
        };

        /// @brief A param's value: bool, int32_t or float for the single-value types, otherwise a vector of int32_t
        ///        (Int2) or float (the multi-float types) of the right length.
        using ParamValue = std::variant<bool, int32_t, float, std::vector<int32_t>, std::vector<float>>;

        /// @brief A parameter set per material.
        struct Param {
            /// @brief The name of the parameter.
            std::string Name;
            /// @brief The value to be used.
            ParamValue Value = int32_t {0};
            /// @brief Unknown, presumed to be an identifier for documentation.
            uint32_t Key = 0;
            /// @brief The type of value provided.
            ParamType Type = ParamType::Int;
        };

        /// @brief A texture sampler used by a material.
        struct Sampler {
            /// @brief The type of the sampler.
            std::string Type;
            /// @brief An optional network path to the texture, if not specified in the FLVER.
            std::string Path;
            /// @brief Unknown, presumed to be an identifier for documentation.
            uint32_t Key = 0;
            /// @brief Unknown; most likely to be the scale, but typically 0, 0.
            Vector2 Unk14;
        };

        /// @brief Network path to the shader source file.
        std::string ShaderPath;
        /// @brief Network path to the material source file, either a matxml or an mtd.
        std::string SourcePath;
        /// @brief Unknown, presumed to be an identifier for documentation.
        uint32_t Key = 0;
        /// @brief Parameters set by this material.
        std::vector<Param> Params;
        /// @brief Texture samplers used by this material.
        std::vector<Sampler> Samplers;

    protected:
        bool IsImpl(BinaryReader& Reader) override;
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
