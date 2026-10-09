//
// Created by Jake Rieger on 10/8/2026.
//

#pragma once

#include <libSouls/SoulsFile.hpp>

#include <string>
#include <variant>
#include <vector>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of an exported class; only touched by this DLL's own code

    // MATBIN: a material config format introduced in Elden Ring. Extension: .matbin
    class SOULS_API MATBIN : public SoulsFile<MATBIN> {
    public:
        // Available types for param values.
        enum class ParamType : uint32_t {
            Bool   = 0,   // a 1-byte boolean
            Int    = 4,   // a 32-bit integer
            Int2   = 5,   // two 32-bit integers
            Float  = 8,   // a 32-bit float
            Float2 = 9,   // two 32-bit floats
            Float3 = 10,  // three 32-bit floats
            Float4 = 11,  // four 32-bit floats
            Float5 = 12,  // five 32-bit floats
        };

        // A param's value: bool, int32_t or float for the single-value types, otherwise a vector of int32_t (Int2)
        // or float (the multi-float types) of the right length.
        using ParamValue = std::variant<bool, int32_t, float, std::vector<int32_t>, std::vector<float>>;

        // A parameter set per material.
        struct Param {
            std::string Name;
            ParamValue Value = int32_t{0};
            // Unknown, presumed to be an identifier for documentation.
            uint32_t Key = 0;
            ParamType Type = ParamType::Int;
        };

        // A texture sampler used by a material.
        struct Sampler {
            // The type of the sampler.
            std::string Type;
            // An optional network path to the texture, if not specified in the FLVER.
            std::string Path;
            // Unknown, presumed to be an identifier for documentation.
            uint32_t Key = 0;
            // Unknown; most likely to be the scale, but typically 0, 0.
            Vector2 Unk14;
        };

        // Network path to the shader source file.
        std::string ShaderPath;
        // Network path to the material source file, either a matxml or an mtd.
        std::string SourcePath;
        // Unknown, presumed to be an identifier for documentation.
        uint32_t Key = 0;
        std::vector<Param> Params;
        std::vector<Sampler> Samplers;

    protected:
        bool IsImpl(BinaryReader& Reader) override;
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
