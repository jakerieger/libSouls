//
// Created by Jake Rieger on 10/8/2026.
//

#include "ACB.hpp"

#include <map>

namespace Souls {
    bool ACB::IsImpl(BinaryReader& Reader) {
        if (Reader.Length() < 4) {
            return false;
        }
        return Reader.GetASCII(0, 4) == std::string("ACB\0", 4);
    }

    void ACB::ReadImpl(BinaryReader& Reader) {
        Reader.Order = Endian::Little;
        BigEndian    = Reader.ReadAt<uint32_t>(0xC) > static_cast<uint64_t>(Reader.Length());
        Reader.Order = BigEndian ? Endian::Big : Endian::Little;

        Reader.AssertMagic(std::string_view("ACB\0", 4));
        Reader.Assert<uint8_t>(2);
        Reader.Assert<uint8_t>(1);
        Reader.Assert<uint8_t>(0);
        Reader.Assert<uint8_t>(0);
        const int32_t AssetCount = Reader.ReadInt32();
        Reader.ReadInt32();  // offset index offset
        const std::vector<int32_t> AssetOffsets = Reader.ReadArray<int32_t>(static_cast<size_t>(AssetCount));

        Assets.clear();
        Assets.reserve(AssetOffsets.size());
        for (const int32_t AssetOffset : AssetOffsets) {
            Reader.Seek(AssetOffset);
            const uint16_t RawType = Reader.ReadAt<uint16_t>(Reader.Position() + 8);
            if (RawType > static_cast<uint16_t>(AssetType::Motion)) {
                throw BinaryException("Unsupported ACB asset type: " + std::to_string(RawType));
            }
            Asset A;
            A.Type                      = static_cast<AssetType>(RawType);
            const int32_t AbsoluteOffset = Reader.ReadInt32();
            const int32_t RelativeOffset = Reader.ReadInt32();
            Reader.Assert<uint16_t>(RawType);
            A.AbsolutePath = Reader.GetUTF16Text(AbsoluteOffset);
            A.RelativePath = Reader.GetUTF16Text(RelativeOffset);

            switch (A.Type) {
                case AssetType::PWV:
                case AssetType::General:
                case AssetType::Texture:
                case AssetType::Motion:
                    Reader.Assert<int16_t>(0);
                    Reader.Assert<int32_t>(0);
                    break;
                case AssetType::GITexture:
                    Reader.Assert<int16_t>(0);
                    Reader.Assert<int32_t>(0);
                    A.Unk10 = Reader.ReadInt32();
                    break;
                case AssetType::Model: {
                    ModelOptions& M = A.Model;
                    M.Unk0A                    = Reader.ReadInt16();
                    const int32_t MembersOffset = Reader.ReadInt32();
                    M.DrawDistance             = Reader.ReadInt32();
                    Reader.Assert<int32_t>(0);
                    Reader.Assert<int32_t>(0);
                    M.MeshLodRate         = Reader.ReadInt16();
                    M.Reflectible         = Reader.ReadBool();
                    M.NormalInteraction   = Reader.ReadBool();
                    M.Unk20               = Reader.ReadInt32();
                    M.RenderType          = Reader.ReadByte();
                    M.DisableShadowSource = Reader.ReadBool();
                    M.DisableShadowTarget = Reader.ReadBool();
                    M.Unk27               = Reader.ReadBool();
                    M.Unk28               = Reader.ReadFloat();
                    M.Unk2C               = Reader.ReadBool();
                    M.FixToCamera         = Reader.ReadBool();
                    M.Unk2E               = Reader.ReadBool();
                    Reader.Assert<uint8_t>(0);
                    M.LowTextureDistance  = Reader.ReadInt16();
                    M.CheapRenderDistance = Reader.ReadInt16();
                    M.Unk34               = Reader.ReadByte();
                    M.Unk35               = Reader.ReadBool();
                    M.Unk36               = Reader.ReadBool();
                    M.Unk37               = Reader.ReadBool();
                    Reader.AssertPattern(0x18, 0);
                    if (MembersOffset != 0) {
                        Reader.Seek(MembersOffset);
                        MemberList List;
                        List.Unk00                    = Reader.ReadInt16();
                        const int16_t MemberCount     = Reader.ReadInt16();
                        const int32_t MemberOffsetsOffset = Reader.ReadInt32();
                        Reader.StepIn(MemberOffsetsOffset);
                        const std::vector<int32_t> MemberOffsets = Reader.ReadArray<int32_t>(static_cast<size_t>(MemberCount));
                        for (const int32_t MemberOffset : MemberOffsets) {
                            Reader.Seek(MemberOffset);
                            Member Mem;
                            const int32_t TextOffset = Reader.ReadInt32();
                            Mem.Unk04                = Reader.ReadInt32();
                            Mem.Text                 = Reader.GetUTF16Text(TextOffset);
                            List.Members.push_back(std::move(Mem));
                        }
                        Reader.StepOut();
                        M.Members = std::move(List);
                    }
                    break;
                }
            }
            Assets.push_back(std::move(A));
        }
    }

    void ACB::WriteImpl(BinaryWriter& Writer) {
        std::vector<int32_t> OffsetIndex;
        std::map<int32_t, std::vector<int32_t>> MembersOffsetIndex;

        Writer.Order = BigEndian ? Endian::Big : Endian::Little;
        Writer.WriteMagic(std::string_view("ACB\0", 4));
        Writer.WriteByte(2);
        Writer.WriteByte(1);
        Writer.WriteByte(0);
        Writer.WriteByte(0);
        Writer.WriteInt32(static_cast<int32_t>(Assets.size()));
        Writer.Reserve<int32_t>("OffsetIndexOffset");
        for (size_t I = 0; I < Assets.size(); ++I) {
            OffsetIndex.push_back(static_cast<int32_t>(Writer.Position()));
            Writer.Reserve<int32_t>("AssetOffset" + std::to_string(I));
        }

        for (size_t I = 0; I < Assets.size(); ++I) {
            const Asset& A          = Assets[I];
            const std::string Index = std::to_string(I);
            Writer.Fill<int32_t>("AssetOffset" + Index, static_cast<int32_t>(Writer.Position()));
            OffsetIndex.push_back(static_cast<int32_t>(Writer.Position()));
            Writer.Reserve<int32_t>("AbsolutePathOffset" + Index);
            OffsetIndex.push_back(static_cast<int32_t>(Writer.Position()));
            Writer.Reserve<int32_t>("RelativePathOffset" + Index);
            Writer.WriteUInt16(static_cast<uint16_t>(A.Type));

            if (A.Type == AssetType::Model) {
                const ModelOptions& M = A.Model;
                Writer.WriteInt16(M.Unk0A);
                MembersOffsetIndex[static_cast<int32_t>(I)] = {};
                if (M.Members) {
                    MembersOffsetIndex[static_cast<int32_t>(I)].push_back(static_cast<int32_t>(Writer.Position()));
                }
                Writer.Reserve<int32_t>("MembersOffset" + Index);
                Writer.WriteInt32(M.DrawDistance);
                Writer.WriteInt32(0);
                Writer.WriteInt32(0);
                Writer.WriteInt16(M.MeshLodRate);
                Writer.WriteBool(M.Reflectible);
                Writer.WriteBool(M.NormalInteraction);
                Writer.WriteInt32(M.Unk20);
                Writer.WriteByte(M.RenderType);
                Writer.WriteBool(M.DisableShadowSource);
                Writer.WriteBool(M.DisableShadowTarget);
                Writer.WriteBool(M.Unk27);
                Writer.WriteFloat(M.Unk28);
                Writer.WriteBool(M.Unk2C);
                Writer.WriteBool(M.FixToCamera);
                Writer.WriteBool(M.Unk2E);
                Writer.WriteByte(0);
                Writer.WriteInt16(M.LowTextureDistance);
                Writer.WriteInt16(M.CheapRenderDistance);
                Writer.WriteByte(M.Unk34);
                Writer.WriteBool(M.Unk35);
                Writer.WriteBool(M.Unk36);
                Writer.WriteBool(M.Unk37);
                Writer.Pad(0x18);
            } else {
                Writer.WriteInt16(0);
                Writer.WriteInt32(0);
                if (A.Type == AssetType::GITexture) {
                    Writer.WriteInt32(A.Unk10);
                }
            }
        }

        for (size_t I = 0; I < Assets.size(); ++I) {
            if (Assets[I].Type != AssetType::Model) {
                continue;
            }
            const std::string Index = std::to_string(I);
            const ModelOptions& M   = Assets[I].Model;
            if (!M.Members) {
                Writer.Fill<int32_t>("MembersOffset" + Index, 0);
                continue;
            }
            Writer.Fill<int32_t>("MembersOffset" + Index, static_cast<int32_t>(Writer.Position()));
            std::vector<int32_t>& MemberOffsets = MembersOffsetIndex[static_cast<int32_t>(I)];
            Writer.WriteInt16(M.Members->Unk00);
            Writer.WriteInt16(static_cast<int16_t>(M.Members->Members.size()));
            MemberOffsets.push_back(static_cast<int32_t>(Writer.Position()));
            Writer.Reserve<int32_t>("MemberOffsetsOffset" + Index);
            Writer.Fill<int32_t>("MemberOffsetsOffset" + Index, static_cast<int32_t>(Writer.Position()));
            for (size_t J = 0; J < M.Members->Members.size(); ++J) {
                MemberOffsets.push_back(static_cast<int32_t>(Writer.Position()));
                Writer.Reserve<int32_t>("MemberOffset" + Index + ":" + std::to_string(J));
            }
            for (size_t J = 0; J < M.Members->Members.size(); ++J) {
                Writer.Fill<int32_t>("MemberOffset" + Index + ":" + std::to_string(J), static_cast<int32_t>(Writer.Position()));
                OffsetIndex.push_back(static_cast<int32_t>(Writer.Position()));
                Writer.Reserve<int32_t>("MemberTextOffset" + Index + ":" + std::to_string(J));
                Writer.WriteInt32(M.Members->Members[J].Unk04);
            }
        }

        for (size_t I = 0; I < Assets.size(); ++I) {
            const std::string Index = std::to_string(I);
            Writer.Fill<int32_t>("AbsolutePathOffset" + Index, static_cast<int32_t>(Writer.Position()));
            Writer.WriteUTF16Text(Assets[I].AbsolutePath, true);
            Writer.Fill<int32_t>("RelativePathOffset" + Index, static_cast<int32_t>(Writer.Position()));
            Writer.WriteUTF16Text(Assets[I].RelativePath, true);
        }

        for (size_t I = 0; I < Assets.size(); ++I) {
            if (Assets[I].Type == AssetType::Model && Assets[I].Model.Members) {
                const auto& List = Assets[I].Model.Members->Members;
                for (size_t J = 0; J < List.size(); ++J) {
                    Writer.Fill<int32_t>("MemberTextOffset" + std::to_string(I) + ":" + std::to_string(J), static_cast<int32_t>(Writer.Position()));
                    Writer.WriteUTF16Text(List[J].Text, true);
                }
            }
        }

        Writer.Align(4);
        Writer.Fill<int32_t>("OffsetIndexOffset", static_cast<int32_t>(Writer.Position()));
        Writer.WriteArray(OffsetIndex);
        for (const auto& [AssetIndex, Offsets] : MembersOffsetIndex) {
            Writer.WriteArray(Offsets);
        }
    }
}  // namespace Souls
