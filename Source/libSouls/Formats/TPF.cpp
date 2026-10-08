//
// Created by Jake Rieger on 10/7/2026.
//

#include "TPF.hpp"

#include <libSouls/Formats/DCX.hpp>
#include <libSouls/TextEncoding.hpp>

#include <string_view>

using namespace std::string_view_literals;

namespace Souls {
    namespace {
        using Platform = TPF::TPFPlatform;

        bool IsKnownPlatform(uint8_t Value) {
            return Value == 0 || Value == 1 || Value == 2 || Value == 4 || Value == 5;
        }

        bool IsBigEndian(Platform Value) {
            return Value == Platform::Xbox360 || Value == Platform::PS3;
        }

        bool IsDXGIPlatform(Platform Value) {
            return Value == Platform::PS4 || Value == Platform::Xbone;
        }

        TPF::Texture ReadTexture(BinaryReader& Reader, Platform Target, uint8_t Flag2, uint8_t Encoding, uint32_t& DataOffset) {
            TPF::Texture Result;

            const uint32_t FileOffset = Reader.ReadUInt32();
            const int32_t FileSize    = Reader.ReadInt32();
            DataOffset                = FileSize > 0 ? FileOffset : 0;  // 0 means "no data": satisfies any alignment

            Result.Format = Reader.ReadByte();
            const uint8_t Type = Reader.ReadByte();
            if (Type > static_cast<uint8_t>(TPF::TexType::Volume)) {
                throw BinaryException("Unknown TPF texture type " + std::to_string(Type));
            }
            Result.Type    = static_cast<TPF::TexType>(Type);
            Result.Mipmaps = Reader.ReadByte();
            Result.Flags1  = Reader.Assert<uint8_t>(0, 1, 2, 3);

            if (Target != Platform::PC) {
                TPF::TexHeader Header;
                Header.Width  = Reader.ReadInt16();
                Header.Height = Reader.ReadInt16();

                if (Target == Platform::Xbox360) {
                    Reader.Assert<int32_t>(0);
                } else if (Target == Platform::PS3) {
                    Header.Unk1 = Reader.ReadInt32();
                    if (Flag2 != 0) {
                        Header.Unk2 = Reader.Assert<int32_t>(0, 0x69E0, 0xAAE4);
                    }
                } else if (IsDXGIPlatform(Target)) {
                    Header.TextureCount = Reader.Assert<int32_t>(1, 6);
                    Header.Unk2         = Reader.Assert<int32_t>(0xD);
                }
                Result.Header = Header;
            }

            const uint32_t NameOffset  = Reader.ReadUInt32();
            const bool HasFloatStruct  = Reader.Assert<int32_t>(0, 1) == 1;

            if (IsDXGIPlatform(Target)) {
                Result.Header->DXGIFormat = Reader.ReadInt32();
            }

            if (HasFloatStruct) {
                TPF::FloatStruct Floats;
                Floats.Unk00        = Reader.ReadInt32();
                const int32_t Length = Reader.ReadInt32();
                if (Length < 0 || Length % 4 != 0 || Length > Reader.Remaining()) {
                    throw BinaryException("Unexpected FloatStruct length: " + std::to_string(Length));
                }
                Floats.Values = Reader.ReadArray<float>(static_cast<size_t>(Length / 4));
                Result.Floats = std::move(Floats);
            }

            if (FileSize < 0 || static_cast<int64_t>(FileOffset) + FileSize > Reader.Length()) {
                throw BinaryException("TPF texture data lies outside the file");
            }
            Reader.StepIn(FileOffset);
            Result.Bytes = Reader.ReadBytes(static_cast<size_t>(FileSize));
            Reader.StepOut();

            if (Result.Flags1 == 2 || Result.Flags1 == 3) {
                DCX::Type CompressionType = DCX::Type::Unknown;
                Result.Bytes              = DCX::Decompress(Result.Bytes, CompressionType);
                if (CompressionType != DCX::Type::DCP_EDGE) {
                    throw BinaryException("TPF compression is expected to be DCP_EDGE");
                }
            }

            Reader.StepIn(NameOffset);
            if (Encoding == 1) {
                Result.Name = Text::UTF16ToUTF8(Reader.ReadUTF16());
            } else {
                Result.Name = Reader.ReadShiftJIS();
            }
            Reader.StepOut();

            return Result;
        }

        std::string Key(const char* Field, size_t Index) {
            return Field + std::to_string(Index);
        }
    }  // namespace

    TPF::Texture::Texture(std::string TextureName, uint8_t TextureFormat, uint8_t TextureFlags1, std::vector<uint8_t> TextureBytes)
        : Name(std::move(TextureName)), Format(TextureFormat), Flags1(TextureFlags1), Bytes(std::move(TextureBytes)) {
        const DDS Header = ReadDDS();
        if (Header.IsCubemap()) {
            Type = TexType::Cubemap;
        } else if (Header.IsVolume()) {
            Type = TexType::Volume;
        } else {
            Type = TexType::Texture;
        }
        Mipmaps = static_cast<uint8_t>(Header.MipMapCount);
    }

    std::string TPF::Texture::ToString() const {
        const char* TypeName = Type == TexType::Cubemap ? "Cubemap" : (Type == TexType::Volume ? "Volume" : "Texture");
        return "[" + std::to_string(Format) + " " + TypeName + "] " + Name;
    }

    bool TPF::IsImpl(BinaryReader& Reader) {
        if (Reader.Length() < 4) {
            return false;
        }
        return Reader.ReadAt<uint8_t>(0) == 'T' && Reader.ReadAt<uint8_t>(1) == 'P' && Reader.ReadAt<uint8_t>(2) == 'F' &&
               Reader.ReadAt<uint8_t>(3) == 0;
    }

    void TPF::ReadImpl(BinaryReader& Reader) {
        Reader.Order = Endian::Little;
        Reader.AssertMagic("TPF\0"sv);

        const uint8_t RawPlatform = Reader.ReadAt<uint8_t>(0xC);
        if (!IsKnownPlatform(RawPlatform)) {
            throw BinaryException("Unknown TPF platform " + std::to_string(RawPlatform));
        }
        Platform     = static_cast<TPFPlatform>(RawPlatform);
        Reader.Order = IsBigEndian(Platform) ? Endian::Big : Endian::Little;

        Reader.ReadInt32();  // data length
        const int32_t FileCount = Reader.ReadInt32();
        Reader.Skip(1);  // platform
        Flag2    = Reader.Assert<uint8_t>(0, 1, 2, 3);
        Encoding = Reader.Assert<uint8_t>(0, 1, 2);
        Reader.AssertPattern(1, 0);

        // Each texture header is at least 0x14 bytes, so this also rejects absurd counts from corrupt files.
        if (FileCount < 0 || static_cast<int64_t>(FileCount) * 0x14 > Reader.Remaining()) {
            throw BinaryException("Invalid TPF texture count " + std::to_string(FileCount));
        }

        Textures.clear();
        Textures.reserve(static_cast<size_t>(FileCount));
        uint8_t Alignment = 16;
        for (int32_t I = 0; I < FileCount; ++I) {
            uint32_t DataOffset = 0;
            Textures.push_back(ReadTexture(Reader, Platform, Flag2, Encoding, DataOffset));
            while (Alignment > 1 && DataOffset % Alignment != 0) {
                Alignment /= 2;
            }
        }
        DataAlignment = FileCount > 0 ? Alignment : uint8_t{4};
    }

    void TPF::WriteImpl(BinaryWriter& Writer) {
        if (DataAlignment == 0) {
            throw BinaryException("TPF DataAlignment must be at least 1");
        }
        Writer.Order = IsBigEndian(Platform) ? Endian::Big : Endian::Little;
        Writer.WriteMagic("TPF\0"sv);
        Writer.Reserve<int32_t>("DataSize");
        Writer.WriteInt32(static_cast<int32_t>(Textures.size()));
        Writer.WriteByte(static_cast<uint8_t>(Platform));
        Writer.WriteByte(Flag2);
        Writer.WriteByte(Encoding);
        Writer.WriteByte(0);

        for (size_t I = 0; I < Textures.size(); ++I) {
            Texture& Current = Textures[I];

            if (Platform == TPFPlatform::PC) {
                const DDS Header = Current.ReadDDS();
                Current.Type     = Header.IsCubemap() ? TexType::Cubemap : (Header.IsVolume() ? TexType::Volume : TexType::Texture);
                Current.Mipmaps  = static_cast<uint8_t>(Header.MipMapCount);
            } else if (!Current.Header) {
                throw BinaryException("Texture \"" + Current.Name + "\" needs a Header for a console TPF");
            }

            Writer.Reserve<uint32_t>(Key("FileData", I));
            Writer.Reserve<int32_t>(Key("FileSize", I));

            Writer.WriteByte(Current.Format);
            Writer.WriteByte(static_cast<uint8_t>(Current.Type));
            Writer.WriteByte(Current.Mipmaps);
            Writer.WriteByte(Current.Flags1);

            if (Platform != TPFPlatform::PC) {
                const TexHeader& Header = *Current.Header;
                Writer.WriteInt16(Header.Width);
                Writer.WriteInt16(Header.Height);

                if (Platform == TPFPlatform::Xbox360) {
                    Writer.WriteInt32(0);
                } else if (Platform == TPFPlatform::PS3) {
                    Writer.WriteInt32(Header.Unk1);
                    if (Flag2 != 0) {
                        Writer.WriteInt32(Header.Unk2);
                    }
                } else if (IsDXGIPlatform(Platform)) {
                    Writer.WriteInt32(Header.TextureCount);
                    Writer.WriteInt32(Header.Unk2);
                }
            }

            Writer.Reserve<uint32_t>(Key("FileName", I));
            Writer.WriteInt32(Current.Floats ? 1 : 0);

            if (IsDXGIPlatform(Platform)) {
                Writer.WriteInt32(Current.Header->DXGIFormat);
            }

            if (Current.Floats) {
                Writer.WriteInt32(Current.Floats->Unk00);
                Writer.WriteInt32(static_cast<int32_t>(Current.Floats->Values.size() * 4));
                Writer.WriteArray(Current.Floats->Values);
            }
        }

        for (size_t I = 0; I < Textures.size(); ++I) {
            Writer.Fill<uint32_t>(Key("FileName", I), static_cast<uint32_t>(Writer.Position()));
            if (Encoding == 1) {
                Writer.WriteUTF16(Text::UTF8ToUTF16(Textures[I].Name), true);
            } else {
                Writer.WriteShiftJIS(Textures[I].Name, true);
            }
        }

        const int64_t DataStart = Writer.Position();
        for (size_t I = 0; I < Textures.size(); ++I) {
            // Padding for texture data varies wildly across games, so don't worry about this too much.
            if (!Textures[I].Bytes.empty() && DataAlignment > 1) {
                Writer.Align(DataAlignment);
            }

            Writer.Fill<uint32_t>(Key("FileData", I), static_cast<uint32_t>(Writer.Position()));

            if (Textures[I].Flags1 == 2 || Textures[I].Flags1 == 3) {
                // Writing DCP_EDGE isn't supported (upstream can't either).
                throw BinaryException("Writing TPF textures compressed with DCP_EDGE is not supported");
            }
            Writer.Fill<int32_t>(Key("FileSize", I), static_cast<int32_t>(Textures[I].Bytes.size()));
            Writer.WriteBytes(Textures[I].Bytes);
        }
        Writer.Fill<int32_t>("DataSize", static_cast<int32_t>(Writer.Position() - DataStart));
    }
}  // namespace Souls
