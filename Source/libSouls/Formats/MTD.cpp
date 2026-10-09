//
// Created by Jake Rieger on 10/8/2026.
//

#include "MTD.hpp"

#include <libSouls/TextEncoding.hpp>

#include <algorithm>
#include <cctype>
#include <optional>

namespace Souls {
    namespace {
        void AssertMarker(BinaryReader& Reader, uint8_t Marker) {
            Reader.Assert<uint8_t>(Marker);
            Reader.Align(4);
        }

        uint8_t ReadMarker(BinaryReader& Reader) {
            const uint8_t Marker = Reader.ReadByte();
            Reader.Align(4);
            return Marker;
        }

        void WriteMarker(BinaryWriter& Writer, uint8_t Marker) {
            Writer.WriteByte(Marker);
            Writer.Align(4);
        }

        std::string ReadMarkedString(BinaryReader& Reader, uint8_t Marker) {
            const int32_t Length = Reader.ReadInt32();
            if (Length < 0) {
                throw BinaryException("Negative MTD string length");
            }
            std::string Value = Reader.ReadShiftJIS(static_cast<size_t>(Length));
            AssertMarker(Reader, Marker);
            return Value;
        }

        void WriteMarkedString(BinaryWriter& Writer, uint8_t Marker, const std::string& Value) {
            const std::string Bytes = Text::UTF8ToShiftJIS(Value);
            Writer.WriteInt32(static_cast<int32_t>(Bytes.size()));
            Writer.WriteString(Bytes, false);
            WriteMarker(Writer, Marker);
        }

        // A block is a length-prefixed section of the file with a type, version and marker.
        struct Block {
            int32_t Version = 0;
            int64_t Start   = 0;
        };

        Block ReadBlock(BinaryReader& Reader, std::optional<int32_t> Type, std::optional<int32_t> Version, std::optional<uint8_t> Marker) {
            Reader.Assert<int32_t>(0);
            Reader.ReadUInt32();  // length
            Block B;
            B.Start = Reader.Position();
            if (Type) {
                Reader.Assert<int32_t>(*Type);
            } else {
                Reader.ReadInt32();
            }
            if (Version) {
                B.Version = Reader.Assert<int32_t>(*Version);
            } else {
                B.Version = Reader.ReadInt32();
            }
            if (Marker) {
                AssertMarker(Reader, *Marker);
            } else {
                ReadMarker(Reader);
            }
            return B;
        }

        Block WriteBlock(BinaryWriter& Writer, int32_t Type, int32_t Version, uint8_t Marker) {
            Writer.WriteInt32(0);
            Block B;
            B.Start = Writer.Position() + 4;
            Writer.Reserve<uint32_t>("Block" + std::to_string(B.Start));
            Writer.WriteInt32(Type);
            Writer.WriteInt32(Version);
            WriteMarker(Writer, Marker);
            B.Version = Version;
            return B;
        }

        void FinishBlock(BinaryWriter& Writer, const Block& B) {
            Writer.Fill<uint32_t>("Block" + std::to_string(B.Start), static_cast<uint32_t>(Writer.Position() - B.Start));
        }

        const char* ParamTypeName(MTD::ParamType Type) {
            switch (Type) {
                case MTD::ParamType::Bool: return "bool";
                case MTD::ParamType::Int: return "int";
                case MTD::ParamType::Int2: return "int2";
                case MTD::ParamType::Float: return "float";
                case MTD::ParamType::Float2: return "float2";
                case MTD::ParamType::Float3: return "float3";
                case MTD::ParamType::Float4: return "float4";
            }
            return "";
        }

        MTD::ParamType ParseParamType(std::string Name) {
            std::transform(Name.begin(), Name.end(), Name.begin(), [](unsigned char C) { return static_cast<char>(std::tolower(C)); });
            for (const auto Type : {MTD::ParamType::Bool, MTD::ParamType::Int, MTD::ParamType::Int2, MTD::ParamType::Float,
                                    MTD::ParamType::Float2, MTD::ParamType::Float3, MTD::ParamType::Float4}) {
                if (Name == ParamTypeName(Type)) {
                    return Type;
                }
            }
            throw BinaryException("Unknown MTD param type: " + Name);
        }
    }  // namespace

    MTD::Param::Param(std::string Name, ParamType Type) : Name(std::move(Name)), Type(Type) {
        switch (Type) {
            case ParamType::Bool: Value = false; break;
            case ParamType::Float: Value = 0.f; break;
            case ParamType::Float2: Value = std::vector<float>(2); break;
            case ParamType::Float3: Value = std::vector<float>(3); break;
            case ParamType::Float4: Value = std::vector<float>(4); break;
            case ParamType::Int: Value = int32_t{0}; break;
            case ParamType::Int2: Value = std::vector<int32_t>(2); break;
        }
    }

    bool MTD::IsImpl(BinaryReader& Reader) {
        if (Reader.Length() < 0x30) {
            return false;
        }
        return Reader.GetASCII(0x2C, 4) == "MTD ";
    }

    void MTD::ReadImpl(BinaryReader& Reader) {
        Reader.Order = Endian::Little;
        ReadBlock(Reader, 0, 3, 0x01);  // file
        {
            ReadBlock(Reader, 1, 2, 0xB0);  // header
            {
                const std::string Magic = ReadMarkedString(Reader, 0x34);
                if (Magic != "MTD ") {
                    throw BinaryException("Not an MTD: marked string was " + Magic);
                }
                Reader.Assert<int32_t>(1000);
            }
            AssertMarker(Reader, 0x01);
            ReadBlock(Reader, 2, 4, 0xA3);  // data
            {
                ShaderPath  = ReadMarkedString(Reader, 0xA3);
                Description = ReadMarkedString(Reader, 0x03);
                Reader.Assert<int32_t>(1);
                ReadBlock(Reader, 3, 4, 0xA3);  // lists
                {
                    Reader.Assert<int32_t>(0);
                    AssertMarker(Reader, 0x03);
                    const int32_t ParamCount = Reader.ReadInt32();
                    Params.clear();
                    for (int32_t I = 0; I < ParamCount; ++I) {
                        Param P;
                        ReadBlock(Reader, 4, 4, 0xA3);  // param
                        P.Name = ReadMarkedString(Reader, 0xA3);
                        P.Type = ParseParamType(ReadMarkedString(Reader, 0x04));
                        Reader.Assert<int32_t>(1);
                        ReadBlock(Reader, std::nullopt, 1, std::nullopt);  // value
                        Reader.ReadInt32();                                 // value count
                        switch (P.Type) {
                            case ParamType::Int: P.Value = Reader.ReadInt32(); break;
                            case ParamType::Int2: P.Value = Reader.ReadArray<int32_t>(2); break;
                            case ParamType::Bool: P.Value = Reader.ReadBool(); break;
                            case ParamType::Float: P.Value = Reader.ReadFloat(); break;
                            case ParamType::Float2: P.Value = Reader.ReadArray<float>(2); break;
                            case ParamType::Float3: P.Value = Reader.ReadArray<float>(3); break;
                            case ParamType::Float4: P.Value = Reader.ReadArray<float>(4); break;
                        }
                        AssertMarker(Reader, 0x04);
                        Reader.Assert<int32_t>(0);
                        Params.push_back(std::move(P));
                    }
                    AssertMarker(Reader, 0x03);
                    const int32_t TextureCount = Reader.ReadInt32();
                    Textures.clear();
                    for (int32_t I = 0; I < TextureCount; ++I) {
                        Texture T;
                        const Block TextureBlock = ReadBlock(Reader, 0x2000, std::nullopt, 0xA3);
                        if (TextureBlock.Version == 3) {
                            T.Extended = false;
                        } else if (TextureBlock.Version == 5) {
                            T.Extended = true;
                        } else {
                            throw BinaryException("Texture block version is expected to be 3 or 5, but it was " +
                                                  std::to_string(TextureBlock.Version) + ".");
                        }
                        T.Type     = ReadMarkedString(Reader, 0x35);
                        T.UVNumber = Reader.ReadInt32();
                        AssertMarker(Reader, 0x35);
                        T.ShaderDataIndex = Reader.ReadInt32();
                        if (T.Extended) {
                            Reader.Assert<int32_t>(0xA3);
                            T.Path                = ReadMarkedString(Reader, 0xBA);
                            const int32_t FloatCount = Reader.ReadInt32();
                            T.UnkFloats           = Reader.ReadArray<float>(static_cast<size_t>(FloatCount));
                        }
                        Textures.push_back(std::move(T));
                    }
                    AssertMarker(Reader, 0x04);
                    Reader.Assert<int32_t>(0);
                }
                AssertMarker(Reader, 0x04);
                Reader.Assert<int32_t>(0);
            }
            AssertMarker(Reader, 0x04);
            Reader.Assert<int32_t>(0);
        }
    }

    void MTD::WriteImpl(BinaryWriter& Writer) {
        Writer.Order = Endian::Little;
        const Block FileBlock = WriteBlock(Writer, 0, 3, 0x01);
        {
            const Block HeaderBlock = WriteBlock(Writer, 1, 2, 0xB0);
            {
                WriteMarkedString(Writer, 0x34, "MTD ");
                Writer.WriteInt32(1000);
            }
            FinishBlock(Writer, HeaderBlock);
            WriteMarker(Writer, 0x01);
            const Block DataBlock = WriteBlock(Writer, 2, 4, 0xA3);
            {
                WriteMarkedString(Writer, 0xA3, ShaderPath);
                WriteMarkedString(Writer, 0x03, Description);
                Writer.WriteInt32(1);
                const Block ListsBlock = WriteBlock(Writer, 3, 4, 0xA3);
                {
                    Writer.WriteInt32(0);
                    WriteMarker(Writer, 0x03);
                    Writer.WriteInt32(static_cast<int32_t>(Params.size()));
                    for (const Param& P : Params) {
                        const Block ParamBlock = WriteBlock(Writer, 4, 4, 0xA3);
                        {
                            WriteMarkedString(Writer, 0xA3, P.Name);
                            WriteMarkedString(Writer, 0x04, ParamTypeName(P.Type));
                            Writer.WriteInt32(1);

                            int32_t ValueBlockType = -1;
                            uint8_t ValueMarker    = 0xFF;
                            int32_t ValueCount     = -1;
                            switch (P.Type) {
                                case ParamType::Bool: ValueBlockType = 0x1000; ValueMarker = 0xC0; ValueCount = 1; break;
                                case ParamType::Int: ValueBlockType = 0x1001; ValueMarker = 0xC5; ValueCount = 1; break;
                                case ParamType::Int2: ValueBlockType = 0x1001; ValueMarker = 0xC5; ValueCount = 2; break;
                                case ParamType::Float: ValueBlockType = 0x1002; ValueMarker = 0xCA; ValueCount = 1; break;
                                case ParamType::Float2: ValueBlockType = 0x1002; ValueMarker = 0xCA; ValueCount = 2; break;
                                case ParamType::Float3: ValueBlockType = 0x1002; ValueMarker = 0xCA; ValueCount = 3; break;
                                case ParamType::Float4: ValueBlockType = 0x1002; ValueMarker = 0xCA; ValueCount = 4; break;
                            }
                            const Block ValueBlock = WriteBlock(Writer, ValueBlockType, 1, ValueMarker);
                            {
                                Writer.WriteInt32(ValueCount);
                                switch (P.Type) {
                                    case ParamType::Int: Writer.WriteInt32(std::get<int32_t>(P.Value)); break;
                                    case ParamType::Int2: Writer.WriteArray(std::get<std::vector<int32_t>>(P.Value)); break;
                                    case ParamType::Bool: Writer.WriteBool(std::get<bool>(P.Value)); break;
                                    case ParamType::Float: Writer.WriteFloat(std::get<float>(P.Value)); break;
                                    case ParamType::Float2:
                                    case ParamType::Float3:
                                    case ParamType::Float4: Writer.WriteArray(std::get<std::vector<float>>(P.Value)); break;
                                }
                            }
                            FinishBlock(Writer, ValueBlock);
                            WriteMarker(Writer, 0x04);
                            Writer.WriteInt32(0);
                        }
                        FinishBlock(Writer, ParamBlock);
                    }
                    WriteMarker(Writer, 0x03);
                    Writer.WriteInt32(static_cast<int32_t>(Textures.size()));
                    for (const Texture& T : Textures) {
                        const Block TextureBlock = WriteBlock(Writer, 0x2000, T.Extended ? 5 : 3, 0xA3);
                        {
                            WriteMarkedString(Writer, 0x35, T.Type);
                            Writer.WriteInt32(T.UVNumber);
                            WriteMarker(Writer, 0x35);
                            Writer.WriteInt32(T.ShaderDataIndex);
                            if (T.Extended) {
                                Writer.WriteInt32(0xA3);
                                WriteMarkedString(Writer, 0xBA, T.Path);
                                Writer.WriteInt32(static_cast<int32_t>(T.UnkFloats.size()));
                                Writer.WriteArray(T.UnkFloats);
                            }
                        }
                        FinishBlock(Writer, TextureBlock);
                    }
                    WriteMarker(Writer, 0x04);
                    Writer.WriteInt32(0);
                }
                FinishBlock(Writer, ListsBlock);
                WriteMarker(Writer, 0x04);
                Writer.WriteInt32(0);
            }
            FinishBlock(Writer, DataBlock);
            WriteMarker(Writer, 0x04);
            Writer.WriteInt32(0);
        }
        FinishBlock(Writer, FileBlock);
    }
}  // namespace Souls
