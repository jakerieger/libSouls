//
// Created by Jake Rieger on 10/8/2026.
//

#include "MATBIN.hpp"

namespace Souls {
    bool MATBIN::IsImpl(BinaryReader& Reader) {
        if (Reader.Length() < 4) {
            return false;
        }
        return Reader.GetASCII(0, 4) == std::string("MAB\0", 4);
    }

    void MATBIN::ReadImpl(BinaryReader& Reader) {
        Reader.Order = Endian::Little;
        Reader.AssertMagic(std::string_view("MAB\0", 4));
        Reader.Assert<int32_t>(2);
        ShaderPath                = Reader.GetUTF16Text(Reader.ReadInt64());
        SourcePath                = Reader.GetUTF16Text(Reader.ReadInt64());
        Key                       = Reader.ReadUInt32();
        const int32_t ParamCount   = Reader.ReadInt32();
        const int32_t SamplerCount = Reader.ReadInt32();
        Reader.AssertPattern(0x14, 0);

        Params.clear();
        Params.reserve(static_cast<size_t>(ParamCount));
        for (int32_t I = 0; I < ParamCount; ++I) {
            Param P;
            P.Name                    = Reader.GetUTF16Text(Reader.ReadInt64());
            const int64_t ValueOffset = Reader.ReadInt64();
            P.Key                     = Reader.ReadUInt32();
            P.Type                    = static_cast<ParamType>(Reader.ReadUInt32());
            Reader.AssertPattern(0x10, 0);

            Reader.StepIn(ValueOffset);
            switch (P.Type) {
                case ParamType::Bool: P.Value = Reader.ReadBool(); break;
                case ParamType::Int: P.Value = Reader.ReadInt32(); break;
                case ParamType::Int2: P.Value = Reader.ReadArray<int32_t>(2); break;
                case ParamType::Float: P.Value = Reader.ReadFloat(); break;
                case ParamType::Float2: P.Value = Reader.ReadArray<float>(2); break;
                // Colors using Float3 actually have five floats in the file; the extras seem to be useless and are
                // discarded (they are always written back as 1).
                case ParamType::Float3: P.Value = Reader.ReadArray<float>(3); break;
                case ParamType::Float4: P.Value = Reader.ReadArray<float>(4); break;
                case ParamType::Float5: P.Value = Reader.ReadArray<float>(5); break;
                default: throw BinaryException("Unimplemented MATBIN value type");
            }
            Reader.StepOut();
            Params.push_back(std::move(P));
        }

        Samplers.clear();
        Samplers.reserve(static_cast<size_t>(SamplerCount));
        for (int32_t I = 0; I < SamplerCount; ++I) {
            Sampler S;
            S.Type  = Reader.GetUTF16Text(Reader.ReadInt64());
            S.Path  = Reader.GetUTF16Text(Reader.ReadInt64());
            S.Key   = Reader.ReadUInt32();
            S.Unk14 = Reader.ReadVector2();
            Reader.AssertPattern(0x14, 0);
            Samplers.push_back(std::move(S));
        }
    }

    void MATBIN::WriteImpl(BinaryWriter& Writer) {
        Writer.Order = Endian::Little;
        Writer.WriteMagic(std::string_view("MAB\0", 4));
        Writer.WriteInt32(2);
        Writer.Reserve<int64_t>("ShaderPathOffset");
        Writer.Reserve<int64_t>("SourcePathOffset");
        Writer.WriteUInt32(Key);
        Writer.WriteInt32(static_cast<int32_t>(Params.size()));
        Writer.WriteInt32(static_cast<int32_t>(Samplers.size()));
        Writer.Pad(0x14);

        for (size_t I = 0; I < Params.size(); ++I) {
            const std::string Index = "[" + std::to_string(I) + "]";
            Writer.Reserve<int64_t>("ParamNameOffset" + Index);
            Writer.Reserve<int64_t>("ParamValueOffset" + Index);
            Writer.WriteUInt32(Params[I].Key);
            Writer.WriteUInt32(static_cast<uint32_t>(Params[I].Type));
            Writer.Pad(0x10);
        }
        for (size_t I = 0; I < Samplers.size(); ++I) {
            const std::string Index = "[" + std::to_string(I) + "]";
            Writer.Reserve<int64_t>("SamplerTypeOffset" + Index);
            Writer.Reserve<int64_t>("SamplerPathOffset" + Index);
            Writer.WriteUInt32(Samplers[I].Key);
            Writer.WriteVector2(Samplers[I].Unk14);
            Writer.Pad(0x14);
        }

        for (size_t I = 0; I < Params.size(); ++I) {
            const Param& P          = Params[I];
            const std::string Index = "[" + std::to_string(I) + "]";
            Writer.Fill<int64_t>("ParamNameOffset" + Index, Writer.Position());
            Writer.WriteUTF16Text(P.Name, true);
            Writer.Fill<int64_t>("ParamValueOffset" + Index, Writer.Position());
            switch (P.Type) {
                case ParamType::Bool: Writer.WriteBool(std::get<bool>(P.Value)); break;
                case ParamType::Int: Writer.WriteInt32(std::get<int32_t>(P.Value)); break;
                case ParamType::Int2: Writer.WriteArray(std::get<std::vector<int32_t>>(P.Value)); break;
                case ParamType::Float: Writer.WriteFloat(std::get<float>(P.Value)); break;
                case ParamType::Float2:
                case ParamType::Float4:
                case ParamType::Float5: Writer.WriteArray(std::get<std::vector<float>>(P.Value)); break;
                case ParamType::Float3:
                    Writer.WriteArray(std::get<std::vector<float>>(P.Value));
                    // Included on the slim chance that the extra floats do anything, since they are always 1 when present.
                    Writer.WriteFloat(1);
                    Writer.WriteFloat(1);
                    break;
                default: throw BinaryException("Unimplemented MATBIN value type");
            }
        }
        for (size_t I = 0; I < Samplers.size(); ++I) {
            const std::string Index = "[" + std::to_string(I) + "]";
            Writer.Fill<int64_t>("SamplerTypeOffset" + Index, Writer.Position());
            Writer.WriteUTF16Text(Samplers[I].Type, true);
            Writer.Fill<int64_t>("SamplerPathOffset" + Index, Writer.Position());
            Writer.WriteUTF16Text(Samplers[I].Path, true);
        }

        Writer.Fill<int64_t>("ShaderPathOffset", Writer.Position());
        Writer.WriteUTF16Text(ShaderPath, true);
        Writer.Fill<int64_t>("SourcePathOffset", Writer.Position());
        Writer.WriteUTF16Text(SourcePath, true);
    }
}  // namespace Souls
