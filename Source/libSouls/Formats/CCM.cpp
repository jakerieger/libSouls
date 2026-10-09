//
// Created by Jake Rieger on 10/8/2026.
//

#include "CCM.hpp"

#include <cmath>
#include <tuple>

namespace Souls {
    namespace {
        struct CodeGroup {
            int32_t StartCode, EndCode, GlyphIndex;
        };

        struct TexRegion {
            int16_t X1, Y1, X2, Y2;
            bool operator==(const TexRegion&) const = default;
        };
    }  // namespace

    void CCM::ReadImpl(BinaryReader& Reader) {
        Reader.Order = Endian::Little;
        const uint32_t RawVersion = Reader.ReadUInt32();
        if (RawVersion != static_cast<uint32_t>(CCMVer::DemonsSouls) && RawVersion != static_cast<uint32_t>(CCMVer::DarkSouls1) &&
            RawVersion != static_cast<uint32_t>(CCMVer::DarkSouls2)) {
            throw BinaryException("Unknown CCM version " + std::to_string(RawVersion));
        }
        Version = static_cast<CCMVer>(RawVersion);
        if (Version == CCMVer::DemonsSouls) {
            Reader.Order = Endian::Big;
        }

        Reader.ReadInt32();  // file size
        FullWidth = Reader.ReadInt16();
        TexWidth  = Reader.ReadInt16();
        TexHeight = Reader.ReadInt16();

        int16_t CodeGroupCount = -1, TexRegionCount = -1, GlyphCount;
        if (Version == CCMVer::DemonsSouls || Version == CCMVer::DarkSouls1) {
            Unk0E          = Reader.ReadInt16();
            CodeGroupCount = Reader.ReadInt16();
            GlyphCount     = Reader.ReadInt16();
        } else {
            Unk0E          = 0;
            TexRegionCount = Reader.ReadInt16();
            GlyphCount     = Reader.ReadInt16();
            Reader.Assert<int16_t>(0);
        }
        Reader.Assert<int32_t>(0x20);
        Reader.ReadInt32();  // glyph offset
        Unk1C    = Reader.ReadByte();
        Unk1D    = Reader.ReadByte();
        TexCount = Reader.ReadByte();
        Reader.Assert<uint8_t>(0);

        Glyphs.clear();
        if (Version == CCMVer::DemonsSouls || Version == CCMVer::DarkSouls1) {
            std::vector<CodeGroup> Groups;
            for (int I = 0; I < CodeGroupCount; ++I) {
                CodeGroup Group;
                Group.StartCode  = Reader.ReadInt32();
                Group.EndCode    = Reader.ReadInt32();
                Group.GlyphIndex = Reader.ReadInt32();
                Groups.push_back(Group);
            }

            std::vector<Glyph> List;
            for (int I = 0; I < GlyphCount; ++I) {
                Glyph G;
                G.UV1      = Reader.ReadVector2();
                G.UV2      = Reader.ReadVector2();
                G.PreSpace = Reader.ReadInt16();
                G.Width    = Reader.ReadInt16();
                G.Advance  = Reader.ReadInt16();
                G.TexIndex = Reader.ReadInt16();
                List.push_back(G);
            }

            for (const CodeGroup& Group : Groups) {
                const int32_t CodeCount = Group.EndCode - Group.StartCode + 1;
                for (int32_t I = 0; I < CodeCount; ++I) {
                    const int64_t Index = static_cast<int64_t>(Group.GlyphIndex) + I;
                    if (Index < 0 || Index >= static_cast<int64_t>(List.size())) {
                        throw BinaryException("CCM glyph index out of range");
                    }
                    Glyphs[Group.StartCode + I] = List[static_cast<size_t>(Index)];
                }
            }
        } else {
            std::map<int64_t, TexRegion> Regions;
            for (int I = 0; I < TexRegionCount; ++I) {
                TexRegion Region;
                const int64_t At = Reader.Position();
                Region.X1        = Reader.ReadInt16();
                Region.Y1        = Reader.ReadInt16();
                Region.X2        = Reader.ReadInt16();
                Region.Y2        = Reader.ReadInt16();
                Regions[At]      = Region;
            }

            for (int I = 0; I < GlyphCount; ++I) {
                const int32_t Code          = Reader.ReadInt32();
                const int32_t RegionOffset  = Reader.ReadInt32();
                Glyph G;
                G.TexIndex = Reader.ReadInt16();
                G.PreSpace = Reader.ReadInt16();
                G.Width    = Reader.ReadInt16();
                G.Advance  = Reader.ReadInt16();
                Reader.Assert<int32_t>(0);
                Reader.Assert<int32_t>(0);

                const auto Found = Regions.find(RegionOffset);
                if (Found == Regions.end()) {
                    throw BinaryException("CCM glyph refers to a missing texture region");
                }
                const TexRegion& Region = Found->second;
                G.UV1                   = {Region.X1 / static_cast<float>(TexWidth), Region.Y1 / static_cast<float>(TexHeight)};
                G.UV2                   = {Region.X2 / static_cast<float>(TexWidth), Region.Y2 / static_cast<float>(TexHeight)};
                Glyphs[Code]            = G;
            }
        }
    }

    void CCM::WriteImpl(BinaryWriter& Writer) {
        Writer.Order = Endian::Little;
        Writer.WriteUInt32(static_cast<uint32_t>(Version));
        Writer.Order = Version == CCMVer::DemonsSouls ? Endian::Big : Endian::Little;

        Writer.Reserve<int32_t>("FileSize");
        Writer.WriteInt16(FullWidth);
        Writer.WriteInt16(TexWidth);
        Writer.WriteInt16(TexHeight);
        if (Version == CCMVer::DemonsSouls || Version == CCMVer::DarkSouls1) {
            Writer.WriteInt16(Unk0E);
            Writer.Reserve<int16_t>("CodeGroupCount");
            Writer.WriteInt16(static_cast<int16_t>(Glyphs.size()));
        } else {
            Writer.Reserve<int16_t>("TexRegionCount");
            Writer.WriteInt16(static_cast<int16_t>(Glyphs.size()));
            Writer.WriteInt16(0);
        }
        Writer.WriteInt32(0x20);
        Writer.Reserve<int32_t>("GlyphOffset");
        Writer.WriteByte(Unk1C);
        Writer.WriteByte(Unk1D);
        Writer.WriteByte(TexCount);
        Writer.WriteByte(0);

        std::vector<int32_t> Codes;
        for (const auto& [Code, G] : Glyphs) {
            Codes.push_back(Code);  // std::map keeps them sorted
        }

        if (Version == CCMVer::DemonsSouls || Version == CCMVer::DarkSouls1) {
            std::vector<CodeGroup> Groups;
            for (size_t I = 0; I < Codes.size();) {
                const int32_t StartCode = Codes[I];
                const int32_t GlyphIdx  = static_cast<int32_t>(I);
                for (++I; I < Codes.size() && Codes[I] == Codes[I - 1] + 1; ++I) {
                }
                Groups.push_back({StartCode, Codes[I - 1], GlyphIdx});
            }
            Writer.Fill<int16_t>("CodeGroupCount", static_cast<int16_t>(Groups.size()));
            for (const CodeGroup& Group : Groups) {
                Writer.WriteInt32(Group.StartCode);
                Writer.WriteInt32(Group.EndCode);
                Writer.WriteInt32(Group.GlyphIndex);
            }

            Writer.Fill<int32_t>("GlyphOffset", static_cast<int32_t>(Writer.Position()));
            for (const int32_t Code : Codes) {
                const Glyph& G = Glyphs.at(Code);
                Writer.WriteVector2(G.UV1);
                Writer.WriteVector2(G.UV2);
                Writer.WriteInt16(G.PreSpace);
                Writer.WriteInt16(G.Width);
                Writer.WriteInt16(G.Advance);
                Writer.WriteInt16(G.TexIndex);
            }
        } else {
            // Identical regions are shared between glyphs; keep them in first-use order.
            std::vector<TexRegion> Regions;
            std::map<int32_t, size_t> RegionIndexByCode;
            for (const int32_t Code : Codes) {
                const Glyph& G = Glyphs.at(Code);
                const TexRegion Region{static_cast<int16_t>(std::nearbyint(G.UV1.X * TexWidth)),
                                       static_cast<int16_t>(std::nearbyint(G.UV1.Y * TexHeight)),
                                       static_cast<int16_t>(std::nearbyint(G.UV2.X * TexWidth)),
                                       static_cast<int16_t>(std::nearbyint(G.UV2.Y * TexHeight))};
                size_t Index = 0;
                while (Index < Regions.size() && !(Regions[Index] == Region)) {
                    ++Index;
                }
                if (Index == Regions.size()) {
                    Regions.push_back(Region);
                }
                RegionIndexByCode[Code] = Index;
            }

            Writer.Fill<int16_t>("TexRegionCount", static_cast<int16_t>(Regions.size()));
            std::vector<int32_t> RegionOffsets;
            for (const TexRegion& Region : Regions) {
                RegionOffsets.push_back(static_cast<int32_t>(Writer.Position()));
                Writer.WriteInt16(Region.X1);
                Writer.WriteInt16(Region.Y1);
                Writer.WriteInt16(Region.X2);
                Writer.WriteInt16(Region.Y2);
            }

            Writer.Fill<int32_t>("GlyphOffset", static_cast<int32_t>(Writer.Position()));
            for (const int32_t Code : Codes) {
                const Glyph& G = Glyphs.at(Code);
                Writer.WriteInt32(Code);
                Writer.WriteInt32(RegionOffsets[RegionIndexByCode[Code]]);
                Writer.WriteInt16(G.TexIndex);
                Writer.WriteInt16(G.PreSpace);
                Writer.WriteInt16(G.Width);
                Writer.WriteInt16(G.Advance);
                Writer.WriteInt32(0);
                Writer.WriteInt32(0);
            }
        }
        Writer.Fill<int32_t>("FileSize", static_cast<int32_t>(Writer.Position()));
    }
}  // namespace Souls
