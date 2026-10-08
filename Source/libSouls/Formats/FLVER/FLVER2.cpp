//
// Created by Jake Rieger on 10/8/2026.
//

#include "FLVER2.hpp"

#include <libSouls/TextEncoding.hpp>

#include <algorithm>
#include <charconv>
#include <map>
#include <string_view>

using namespace std::string_view_literals;

namespace Souls {
    using namespace FLVER;

    namespace {
        std::string Key(const char* Field, size_t Index) {
            return Field + std::to_string(Index);
        }

        // Rejects counts a file of this size couldn't hold, so a corrupt header can't ask for gigabytes.
        void CheckCount(const char* What, int32_t Count, int64_t MinBytesEach, BinaryReader& Reader) {
            if (Count < 0 || static_cast<int64_t>(Count) * MinBytesEach > Reader.Length()) {
                throw BinaryException(std::string("Invalid FLVER ") + What + " count " + std::to_string(Count));
            }
        }

        std::string ReadStringAt(BinaryReader& Reader, int32_t Offset, bool Unicode) {
            Reader.StepIn(Offset);
            std::string Value = Unicode ? Text::UTF16ToUTF8(Reader.ReadUTF16()) : Reader.ReadShiftJIS();
            Reader.StepOut();
            return Value;
        }

        void WriteString(BinaryWriter& Writer, const std::string& Value, bool Unicode) {
            if (Unicode) {
                Writer.WriteUTF16(Text::UTF8ToUTF16(Value), true);
            } else {
                Writer.WriteShiftJIS(Value, true);
            }
        }

        std::vector<int32_t> ReadIntsAt(BinaryReader& Reader, int32_t Offset, int32_t Count) {
            if (Count <= 0) return {};
            Reader.StepIn(Offset);
            std::vector<int32_t> Values = Reader.ReadArray<int32_t>(static_cast<size_t>(Count));
            Reader.StepOut();
            return Values;
        }

        float UVFactorFor(int32_t Version) {
            return Version >= 0x2000F ? 2048.f : 1024.f;
        }

#pragma region Raw structures
        // What the file stores before things are connected up: lists are referred to by index, so each piece is read
        // flat and then handed to its owner.
        struct RawMaterial {
            FLVER2::Material Value;
            int32_t TextureCount = 0;
            int32_t TextureIndex = 0;
        };

        struct RawMesh {
            FLVER2::Mesh Value;
            std::vector<int32_t> FaceSetIndices;
            std::vector<int32_t> VertexBufferIndices;
        };

        struct RawVertexBuffer {
            int32_t LayoutIndex  = 0;
            int32_t VertexSize   = 0;
            int32_t BufferIndex  = 0;
            int32_t VertexCount  = 0;
            int32_t BufferOffset = 0;
        };
#pragma endregion

        FLVER2::GXItem ReadGXItem(BinaryReader& Reader, const FLVER2::FLVERHeader& Header) {
            FLVER2::GXItem Item;
            if (Header.Version <= 0x20010) {
                Item.ID = std::to_string(Reader.ReadInt32());
            } else {
                Item.ID = Reader.ReadFixStr(4);
            }
            Item.Unk04           = Reader.ReadInt32();
            const int32_t Length = Reader.ReadInt32();
            if (Length < 0xC) throw BinaryException("Invalid GX item length " + std::to_string(Length));
            Item.Data = Reader.ReadBytes(static_cast<size_t>(Length - 0xC));
            return Item;
        }

        FLVER2::GXList ReadGXList(BinaryReader& Reader, const FLVER2::FLVERHeader& Header) {
            FLVER2::GXList List;
            if (Header.Version < 0x20010) {
                List.Items.push_back(ReadGXItem(Reader, Header));
            } else {
                int32_t ID;
                while ((ID = Reader.ReadAt<int32_t>(Reader.Position())) != INT_MAX && ID != -1) {
                    List.Items.push_back(ReadGXItem(Reader, Header));
                }
                List.TerminatorID     = Reader.Assert<int32_t>(ID);
                Reader.Assert<int32_t>(100);
                List.TerminatorLength = Reader.ReadInt32() - 0xC;
                if (List.TerminatorLength < 0) throw BinaryException("Invalid GX terminator length");
                Reader.AssertPattern(static_cast<size_t>(List.TerminatorLength), 0x00);
            }
            return List;
        }

        void WriteGXList(BinaryWriter& Writer, const FLVER2::FLVERHeader& Header, const FLVER2::GXList& List) {
            auto WriteItem = [&](const FLVER2::GXItem& Item) {
                if (Header.Version <= 0x20010) {
                    int32_t ID = 0;
                    const auto [Ptr, Error] = std::from_chars(Item.ID.data(), Item.ID.data() + Item.ID.size(), ID);
                    if (Error != std::errc() || Ptr != Item.ID.data() + Item.ID.size()) {
                        throw BinaryException("For Dark Souls 2, GX IDs must be convertible to int.");
                    }
                    Writer.WriteInt32(ID);
                } else {
                    Writer.WriteFixStr(Item.ID, 4);
                }
                Writer.WriteInt32(Item.Unk04);
                Writer.WriteInt32(static_cast<int32_t>(Item.Data.size()) + 0xC);
                Writer.WriteBytes(Item.Data);
            };

            if (Header.Version < 0x20010) {
                if (List.Items.empty()) throw BinaryException("A GX list for this version needs one item");
                WriteItem(List.Items[0]);
            } else {
                for (const FLVER2::GXItem& Item : List.Items) WriteItem(Item);
                Writer.WriteInt32(List.TerminatorID);
                Writer.WriteInt32(100);
                Writer.WriteInt32(List.TerminatorLength + 0xC);
                Writer.Pad(static_cast<size_t>(List.TerminatorLength));
            }
        }

        FLVER2::Texture ReadTexture(BinaryReader& Reader, const FLVER2::FLVERHeader& Header) {
            FLVER2::Texture Result;
            const int32_t PathOffset = Reader.ReadInt32();
            const int32_t TypeOffset = Reader.ReadInt32();
            const Vector2 Scale      = Reader.ReadVector2();
            Result.Scale             = Scale;

            Result.Unk10 = Reader.Assert<uint8_t>(0, 1, 2);
            Result.Unk11 = Reader.ReadBool();
            Reader.AssertPattern(2, 0);

            Result.Unk14 = Reader.ReadFloat();
            Result.Unk18 = Reader.ReadFloat();
            Result.Unk1C = Reader.ReadFloat();

            Result.Type = ReadStringAt(Reader, TypeOffset, Header.Unicode);
            Result.Path = ReadStringAt(Reader, PathOffset, Header.Unicode);
            return Result;
        }

        RawMaterial ReadMaterial(BinaryReader& Reader,
                                 const FLVER2::FLVERHeader& Header,
                                 std::vector<FLVER2::GXList>& GXLists,
                                 std::map<int32_t, int32_t>& GXListIndices) {
            RawMaterial Raw;
            FLVER2::Material& Result = Raw.Value;

            const int32_t NameOffset = Reader.ReadInt32();
            const int32_t MtdOffset  = Reader.ReadInt32();
            Raw.TextureCount         = Reader.ReadInt32();
            Raw.TextureIndex         = Reader.ReadInt32();
            Result.Flags             = Reader.ReadInt32();
            const int32_t GXOffset   = Reader.ReadInt32();
            Result.Unk18             = Reader.ReadInt32();
            Reader.Assert<int32_t>(0);

            Result.Name = ReadStringAt(Reader, NameOffset, Header.Unicode);
            Result.MTD  = ReadStringAt(Reader, MtdOffset, Header.Unicode);

            if (GXOffset == 0) {
                Result.GXIndex = -1;
            } else {
                const auto Known = GXListIndices.find(GXOffset);
                if (Known == GXListIndices.end()) {
                    Reader.StepIn(GXOffset);
                    GXListIndices[GXOffset] = static_cast<int32_t>(GXLists.size());
                    GXLists.push_back(ReadGXList(Reader, Header));
                    Reader.StepOut();
                }
                Result.GXIndex = GXListIndices[GXOffset];
            }
            return Raw;
        }

        FLVER2::FaceSet ReadFaceSet(BinaryReader& Reader, const FLVER2::FLVERHeader& Header, int32_t HeaderIndexSize, int32_t DataOffset) {
            FLVER2::FaceSet Result;
            Result.Flags         = static_cast<FLVER2::FaceSet::FSFlags>(Reader.ReadUInt32());
            Result.TriangleStrip = Reader.ReadBool();
            Result.CullBackfaces = Reader.ReadBool();
            Result.Unk06         = Reader.ReadInt16();
            const int32_t IndexCount   = Reader.ReadInt32();
            const int32_t IndicesOffset = Reader.ReadInt32();

            int32_t IndexSize = 0;
            if (Header.Version > 0x20005) {
                Reader.ReadInt32();  // indices length
                Reader.Assert<int32_t>(0);
                IndexSize = Reader.Assert<int32_t>(0, 16, 32);
                Reader.Assert<int32_t>(0);
            }
            if (IndexSize == 0) IndexSize = HeaderIndexSize;

            if ((IndexSize == 8) != Result.HasFlag(FLVER2::FaceSet::FSFlags::EdgeCompressed)) {
                throw BinaryException("FSFlags.EdgeCompressed probably doesn't mean edge compression after all.");
            }
            if (IndexSize == 8) {
                throw BinaryException("Edge-compressed face sets (PlayStation 3) are not supported");
            }
            if (IndexSize != 16 && IndexSize != 32) {
                throw BinaryException("Unsupported index size: " + std::to_string(IndexSize));
            }
            if (IndexCount < 0 || static_cast<int64_t>(IndexCount) * (IndexSize / 8) + DataOffset + IndicesOffset > Reader.Length()) {
                throw BinaryException("FLVER face set indices lie outside the file");
            }

            Reader.StepIn(static_cast<int64_t>(DataOffset) + IndicesOffset);
            Result.Indices.reserve(static_cast<size_t>(IndexCount));
            if (IndexSize == 16) {
                for (const uint16_t Index : Reader.ReadArray<uint16_t>(static_cast<size_t>(IndexCount))) Result.Indices.push_back(Index);
            } else {
                Result.Indices = Reader.ReadArray<int32_t>(static_cast<size_t>(IndexCount));
            }
            Reader.StepOut();
            return Result;
        }

        FLVER::LayoutMembers ReadBufferLayout(BinaryReader& Reader) {
            const int32_t MemberCount = Reader.ReadInt32();
            Reader.Assert<int32_t>(0);
            Reader.Assert<int32_t>(0);
            const int32_t MemberOffset = Reader.ReadInt32();
            CheckCount("layout member", MemberCount, 20, Reader);

            LayoutMembers Layout;
            Layout.reserve(static_cast<size_t>(MemberCount));
            Reader.StepIn(MemberOffset);
            int32_t StructOffset = 0;
            for (int32_t I = 0; I < MemberCount; ++I) {
                LayoutMember Member;
                Member.Unk00 = Reader.ReadInt32();
                Reader.Assert<int32_t>(StructOffset);
                const uint32_t Type     = Reader.ReadUInt32();
                const uint32_t Semantic = Reader.ReadUInt32();
                Member.Type             = static_cast<LayoutType>(Type);
                Member.Semantic         = static_cast<LayoutSemantic>(Semantic);
                if (std::string_view(LayoutTypeName(Member.Type)) == "Unknown" ||
                    std::string_view(LayoutSemanticName(Member.Semantic)) == "Unknown") {
                    throw BinaryException("Unknown vertex layout member (type " + std::to_string(Type) + ", semantic " + std::to_string(Semantic) + ")");
                }
                Member.Index = Reader.ReadInt32();
                StructOffset += Member.Size();
                Layout.push_back(Member);
            }
            Reader.StepOut();
            return Layout;
        }
    }  // namespace

#pragma region FaceSet and Mesh helpers
    std::vector<int32_t> FLVER2::FaceSet::Triangulate(bool AllowPrimitiveRestarts, bool IncludeDegenerateFaces) const {
        if (!TriangleStrip) return Indices;

        std::vector<int32_t> Triangles;
        bool Flip = false;
        for (size_t I = 0; I + 2 < Indices.size(); ++I) {
            const int32_t A = Indices[I];
            const int32_t B = Indices[I + 1];
            const int32_t C = Indices[I + 2];

            if (AllowPrimitiveRestarts && (A == 0xFFFF || B == 0xFFFF || C == 0xFFFF)) {
                Flip = false;
            } else {
                if (IncludeDegenerateFaces || (A != B && B != C && C != A)) {
                    if (Flip) {
                        Triangles.insert(Triangles.end(), {C, B, A});
                    } else {
                        Triangles.insert(Triangles.end(), {A, B, C});
                    }
                }
                Flip = !Flip;
            }
        }
        return Triangles;
    }

    int32_t FLVER2::FaceSet::GetVertexIndexSize() const {
        for (const int32_t Index : Indices) {
            if (Index > 0xFFFF + 1) return 32;
        }
        return 16;
    }

    void FLVER2::FaceSet::AddFaceCounts(bool AllowPrimitiveRestarts, int32_t& TrueFaceCount, int32_t& TotalFaceCount) const {
        if (TriangleStrip) {
            for (size_t I = 0; I + 2 < Indices.size(); ++I) {
                const int32_t A = Indices[I];
                const int32_t B = Indices[I + 1];
                const int32_t C = Indices[I + 2];
                if (!AllowPrimitiveRestarts || (A != 0xFFFF && B != 0xFFFF && C != 0xFFFF)) {
                    ++TotalFaceCount;
                    if (!HasFlag(FSFlags::MotionBlur) && A != B && B != C && C != A) ++TrueFaceCount;
                }
            }
        } else {
            TotalFaceCount += static_cast<int32_t>(Indices.size() / 3);
            TrueFaceCount += static_cast<int32_t>(Indices.size() / 3);
        }
    }

    std::vector<std::array<const Vertex*, 3>> FLVER2::Mesh::GetFaces(FaceSet::FSFlags Flags) const {
        std::vector<std::array<const Vertex*, 3>> Faces;
        if (FaceSets.empty()) return Faces;

        const auto Found = std::find_if(FaceSets.begin(), FaceSets.end(), [&](const FaceSet& Set) { return Set.Flags == Flags; });
        const FaceSet& Chosen = Found != FaceSets.end() ? *Found : FaceSets.front();
        const std::vector<int32_t> Indices = Chosen.Triangulate(Vertices.size() < 0xFFFF);
        for (size_t I = 0; I + 2 < Indices.size(); I += 3) {
            const auto Index = [&](size_t Which) -> const Vertex* {
                const int32_t V = Indices[I + Which];
                if (V < 0 || static_cast<size_t>(V) >= Vertices.size()) throw BinaryException("Face index " + std::to_string(V) + " is out of range");
                return &Vertices[static_cast<size_t>(V)];
            };
            Faces.push_back({Index(0), Index(1), Index(2)});
        }
        return Faces;
    }
#pragma endregion

    bool FLVER2::IsImpl(BinaryReader& Reader) {
        if (Reader.Length() < 0xC) return false;

        static constexpr char Expected[6] = {'F', 'L', 'V', 'E', 'R', '\0'};
        for (int64_t I = 0; I < 6; ++I) {
            if (Reader.ReadAt<uint8_t>(I) != static_cast<uint8_t>(Expected[I])) return false;
        }
        const bool Big = Reader.ReadAt<uint8_t>(6) == 'B' && Reader.ReadAt<uint8_t>(7) == 0;
        Reader.Order   = Big ? Endian::Big : Endian::Little;
        return Reader.ReadAt<int32_t>(8) >= 0x20000;
    }

    void FLVER2::ReadImpl(BinaryReader& Reader) {
        Reader.Order = Endian::Little;
        Header       = FLVERHeader{};
        Reader.AssertMagic("FLVER\0"sv);
        const std::string Endianness = Reader.ReadString(2);
        if (Endianness != "L\0"sv && Endianness != "B\0"sv) {
            throw BinaryException("Unknown FLVER byte order marker");
        }
        Header.BigEndian = Endianness == "B\0"sv;
        Reader.Order     = Header.BigEndian ? Endian::Big : Endian::Little;

        // Gundam Unicorn: 0x20005, 0x2000E. DS1: 0x2000B (PS3), 0x2000C, 0x2000D. DS2: 0x2000F, 0x20010 (0x20009 for some
        // armor). BB and DS3: 0x20013, 0x20014. Sekiro: 0x2001A, and 0x20016 for a test character.
        Header.Version = Reader.Assert<int32_t>(0x20005, 0x20007, 0x20009, 0x2000B, 0x2000C, 0x2000D, 0x2000E, 0x2000F,
                                                0x20010, 0x20013, 0x20014, 0x20016, 0x2001A);

        const int32_t DataOffset = Reader.ReadInt32();
        Reader.ReadInt32();  // data length
        const int32_t DummyCount        = Reader.ReadInt32();
        const int32_t MaterialCount     = Reader.ReadInt32();
        const int32_t BoneCount         = Reader.ReadInt32();
        const int32_t MeshCount         = Reader.ReadInt32();
        const int32_t VertexBufferCount = Reader.ReadInt32();

        Header.BoundingBoxMin = Reader.ReadVector3();
        Header.BoundingBoxMax = Reader.ReadVector3();

        Reader.ReadInt32();  // face count, not including motion blur meshes or degenerate faces
        Reader.ReadInt32();  // total face count

        const int32_t VertexIndicesSize = Reader.Assert<uint8_t>(0, 8, 16, 32);
        Header.Unicode                  = Reader.ReadBool();
        Header.Unk4A                    = Reader.ReadBool();
        Reader.AssertPattern(1, 0);

        Header.Unk4C = Reader.ReadInt32();

        const int32_t FaceSetCount     = Reader.ReadInt32();
        const int32_t BufferLayoutCount = Reader.ReadInt32();
        const int32_t TextureCount     = Reader.ReadInt32();

        Header.Unk5C = Reader.ReadByte();
        Header.Unk5D = Reader.ReadByte();
        Reader.AssertPattern(2, 0);

        Reader.Assert<int32_t>(0);
        Reader.Assert<int32_t>(0);
        Header.Unk68 = Reader.Assert<int32_t>(0, 1, 2, 3, 4);
        for (int I = 0; I < 5; ++I) Reader.Assert<int32_t>(0);

        CheckCount("dummy", DummyCount, 0x40, Reader);
        CheckCount("material", MaterialCount, 0x20, Reader);
        CheckCount("bone", BoneCount, 0x80, Reader);
        CheckCount("mesh", MeshCount, 0x30, Reader);
        CheckCount("vertex buffer", VertexBufferCount, 0x20, Reader);
        CheckCount("face set", FaceSetCount, 0x1C, Reader);
        CheckCount("buffer layout", BufferLayoutCount, 0x10, Reader);
        CheckCount("texture", TextureCount, 0x20, Reader);

        Dummies.clear();
        Dummies.reserve(static_cast<size_t>(DummyCount));
        for (int32_t I = 0; I < DummyCount; ++I) {
            Dummy Item;
            Item.Read(Reader, Header.Version);
            Dummies.push_back(std::move(Item));
        }

        std::vector<RawMaterial> RawMaterials;
        RawMaterials.reserve(static_cast<size_t>(MaterialCount));
        std::map<int32_t, int32_t> GXListIndices;
        GXLists.clear();
        for (int32_t I = 0; I < MaterialCount; ++I) {
            RawMaterials.push_back(ReadMaterial(Reader, Header, GXLists, GXListIndices));
        }

        Bones.clear();
        Bones.reserve(static_cast<size_t>(BoneCount));
        for (int32_t I = 0; I < BoneCount; ++I) {
            Bone Item;
            Item.Read(Reader, Header.Unicode);
            Bones.push_back(std::move(Item));
        }

        std::vector<RawMesh> RawMeshes;
        RawMeshes.reserve(static_cast<size_t>(MeshCount));
        for (int32_t I = 0; I < MeshCount; ++I) {
            RawMesh Raw;
            Mesh& Result = Raw.Value;
            Result.Dynamic = Reader.Assert<uint8_t>(0, 1);
            Reader.AssertPattern(3, 0);

            Result.MaterialIndex = Reader.ReadInt32();
            Reader.Assert<int32_t>(0);
            Reader.Assert<int32_t>(0);
            Result.DefaultBoneIndex          = Reader.ReadInt32();
            const int32_t MeshBoneCount      = Reader.ReadInt32();
            const int32_t BoundingBoxOffset  = Reader.ReadInt32();
            const int32_t BoneOffset         = Reader.ReadInt32();
            const int32_t MeshFaceSetCount   = Reader.ReadInt32();
            const int32_t FaceSetOffset      = Reader.ReadInt32();
            const int32_t MeshBufferCount    = Reader.Assert<int32_t>(1, 2, 3);
            const int32_t VertexBufferOffset = Reader.ReadInt32();
            CheckCount("mesh bone", MeshBoneCount, 4, Reader);
            CheckCount("mesh face set", MeshFaceSetCount, 4, Reader);

            if (BoundingBoxOffset != 0) {
                Reader.StepIn(BoundingBoxOffset);
                BoundingBoxes Box;
                Box.Min = Reader.ReadVector3();
                Box.Max = Reader.ReadVector3();
                if (Header.Version >= 0x2001A) Box.Unk = Reader.ReadVector3();
                Result.BoundingBox = Box;
                Reader.StepOut();
            }

            Result.BoneIndices      = ReadIntsAt(Reader, BoneOffset, MeshBoneCount);
            Raw.FaceSetIndices      = ReadIntsAt(Reader, FaceSetOffset, MeshFaceSetCount);
            Raw.VertexBufferIndices = ReadIntsAt(Reader, VertexBufferOffset, MeshBufferCount);
            RawMeshes.push_back(std::move(Raw));
        }

        std::vector<FaceSet> FaceSets;
        FaceSets.reserve(static_cast<size_t>(FaceSetCount));
        for (int32_t I = 0; I < FaceSetCount; ++I) {
            FaceSets.push_back(ReadFaceSet(Reader, Header, VertexIndicesSize, DataOffset));
        }

        std::vector<RawVertexBuffer> VertexBuffers;
        VertexBuffers.reserve(static_cast<size_t>(VertexBufferCount));
        for (int32_t I = 0; I < VertexBufferCount; ++I) {
            RawVertexBuffer Raw;
            Raw.BufferIndex = Reader.ReadInt32();
            Raw.LayoutIndex = Reader.ReadInt32();
            Raw.VertexSize  = Reader.ReadInt32();
            Raw.VertexCount = Reader.ReadInt32();
            Reader.Assert<int32_t>(0);
            Reader.Assert<int32_t>(0);
            Reader.ReadInt32();  // buffer length
            Raw.BufferOffset = Reader.ReadInt32();
            VertexBuffers.push_back(Raw);
        }

        BufferLayouts.clear();
        BufferLayouts.reserve(static_cast<size_t>(BufferLayoutCount));
        for (int32_t I = 0; I < BufferLayoutCount; ++I) {
            BufferLayouts.push_back(ReadBufferLayout(Reader));
        }

        std::vector<Texture> Textures;
        Textures.reserve(static_cast<size_t>(TextureCount));
        for (int32_t I = 0; I < TextureCount; ++I) {
            Textures.push_back(ReadTexture(Reader, Header));
        }

        SekiroUnk.reset();
        if (Header.Version >= 0x2001A) {
            SekiroUnkStruct Unk;
            const int16_t Count1 = Reader.ReadInt16();
            const int16_t Count2 = Reader.ReadInt16();
            const uint32_t Offset1 = Reader.ReadUInt32();
            const uint32_t Offset2 = Reader.ReadUInt32();
            for (int I = 0; I < 5; ++I) Reader.Assert<int32_t>(0);

            auto ReadMembers = [&](uint32_t Offset, int16_t Count, std::vector<SekiroUnkStruct::Member>& Members) {
                CheckCount("Sekiro member", Count, 16, Reader);
                Reader.StepIn(Offset);
                for (int16_t I = 0; I < Count; ++I) {
                    SekiroUnkStruct::Member Member;
                    for (int16_t& Value : Member.Unk00) Value = Reader.ReadInt16();
                    Member.Index = Reader.ReadInt32();
                    Reader.Assert<int32_t>(0);
                    Members.push_back(Member);
                }
                Reader.StepOut();
            };
            ReadMembers(Offset1, Count1, Unk.Members1);
            ReadMembers(Offset2, Count2, Unk.Members2);
            SekiroUnk = std::move(Unk);
        }

        // Hand textures to their materials, each used exactly once.
        std::vector<bool> TextureUsed(Textures.size(), false);
        Materials.clear();
        Materials.reserve(RawMaterials.size());
        for (RawMaterial& Raw : RawMaterials) {
            if (Raw.TextureCount < 0 || Raw.TextureIndex < 0 ||
                static_cast<int64_t>(Raw.TextureIndex) + Raw.TextureCount > static_cast<int64_t>(Textures.size())) {
                throw BinaryException("FLVER material refers to textures that don't exist");
            }
            for (int32_t I = Raw.TextureIndex; I < Raw.TextureIndex + Raw.TextureCount; ++I) {
                if (TextureUsed[static_cast<size_t>(I)]) throw BinaryException("Texture already taken: " + std::to_string(I));
                TextureUsed[static_cast<size_t>(I)] = true;
                Raw.Value.Textures.push_back(Textures[static_cast<size_t>(I)]);
            }
            Materials.push_back(std::move(Raw.Value));
        }
        if (std::find(TextureUsed.begin(), TextureUsed.end(), false) != TextureUsed.end()) {
            throw BinaryException("Orphaned textures found.");
        }

        // Hand face sets, vertex buffers and vertices to their meshes.
        std::vector<bool> FaceSetUsed(FaceSets.size(), false);
        std::vector<bool> BufferUsed(VertexBuffers.size(), false);
        Meshes.clear();
        Meshes.reserve(RawMeshes.size());
        for (RawMesh& Raw : RawMeshes) {
            Mesh& Result = Raw.Value;

            for (const int32_t Index : Raw.FaceSetIndices) {
                if (Index < 0 || static_cast<size_t>(Index) >= FaceSets.size() || FaceSetUsed[static_cast<size_t>(Index)]) {
                    throw BinaryException("Face set not found or already taken: " + std::to_string(Index));
                }
                FaceSetUsed[static_cast<size_t>(Index)] = true;
                Result.FaceSets.push_back(std::move(FaceSets[static_cast<size_t>(Index)]));
            }

            std::vector<const RawVertexBuffer*> MeshBuffers;
            for (const int32_t Index : Raw.VertexBufferIndices) {
                if (Index < 0 || static_cast<size_t>(Index) >= VertexBuffers.size() || BufferUsed[static_cast<size_t>(Index)]) {
                    throw BinaryException("Vertex buffer not found or already taken: " + std::to_string(Index));
                }
                BufferUsed[static_cast<size_t>(Index)] = true;
                MeshBuffers.push_back(&VertexBuffers[static_cast<size_t>(Index)]);
                Result.VertexBuffers.push_back({VertexBuffers[static_cast<size_t>(Index)].LayoutIndex});
            }

            // Semantics other than the repeatable ones may appear in only one of a mesh's buffers.
            std::vector<LayoutSemantic> Semantics;
            for (const RawVertexBuffer* Buffer : MeshBuffers) {
                if (Buffer->LayoutIndex < 0 || static_cast<size_t>(Buffer->LayoutIndex) >= BufferLayouts.size()) {
                    throw BinaryException("Vertex buffer refers to a layout that doesn't exist");
                }
                for (const LayoutMember& Member : BufferLayouts[static_cast<size_t>(Buffer->LayoutIndex)]) {
                    if (Member.Semantic != LayoutSemantic::UV && Member.Semantic != LayoutSemantic::Tangent &&
                        Member.Semantic != LayoutSemantic::VertexColor && Member.Semantic != LayoutSemantic::Position &&
                        Member.Semantic != LayoutSemantic::Normal) {
                        if (std::find(Semantics.begin(), Semantics.end(), Member.Semantic) != Semantics.end()) {
                            throw BinaryException("Unexpected semantic list.");
                        }
                        Semantics.push_back(Member.Semantic);
                    }
                }
            }
            for (size_t I = 0; I < MeshBuffers.size(); ++I) {
                // Some flag on edge-compressed vertex buffers lives in the top bits of the index.
                if (static_cast<size_t>(MeshBuffers[I]->BufferIndex & ~0x60000000) != I) {
                    throw BinaryException("Unexpected vertex buffer index.");
                }
            }

            // Read the vertices from each buffer.
            if (MeshBuffers.empty()) throw BinaryException("A FLVER mesh has no vertex buffers");
            const int32_t VertexCount = MeshBuffers[0]->VertexCount;
            if (VertexCount < 0) throw BinaryException("Invalid vertex count");
            Result.Vertices.resize(static_cast<size_t>(VertexCount));
            for (const RawVertexBuffer* Buffer : MeshBuffers) {
                const LayoutMembers& Layout = BufferLayouts[static_cast<size_t>(Buffer->LayoutIndex)];
                if (Buffer->VertexSize != LayoutSize(Layout)) {
                    throw BinaryException("Mismatched vertex buffer and buffer layout sizes.");
                }
                if (Buffer->VertexCount != VertexCount) {
                    throw BinaryException("A mesh's vertex buffers disagree on the vertex count");
                }
                const int64_t Start = static_cast<int64_t>(DataOffset) + Buffer->BufferOffset;
                if (Buffer->BufferOffset < 0 || Start + static_cast<int64_t>(VertexCount) * Buffer->VertexSize > Reader.Length()) {
                    throw BinaryException("FLVER vertex buffer lies outside the file");
                }

                Reader.StepIn(Start);
                const float UVFactor = UVFactorFor(Header.Version);
                for (Vertex& Item : Result.Vertices) Item.Read(Reader, Layout, UVFactor);
                Reader.StepOut();
            }
            Meshes.push_back(std::move(Result));
        }
        if (std::find(FaceSetUsed.begin(), FaceSetUsed.end(), false) != FaceSetUsed.end()) {
            throw BinaryException("Orphaned face sets found.");
        }
        if (std::find(BufferUsed.begin(), BufferUsed.end(), false) != BufferUsed.end()) {
            throw BinaryException("Orphaned vertex buffers found.");
        }
    }

    void FLVER2::WriteImpl(BinaryWriter& Writer) {
        Writer.Order = Header.BigEndian ? Endian::Big : Endian::Little;
        Writer.WriteMagic("FLVER\0"sv);
        Writer.WriteMagic(Header.BigEndian ? "B\0"sv : "L\0"sv);
        Writer.WriteInt32(Header.Version);

        Writer.Reserve<int32_t>("DataOffset");
        Writer.Reserve<int32_t>("DataSize");
        Writer.WriteInt32(static_cast<int32_t>(Dummies.size()));
        Writer.WriteInt32(static_cast<int32_t>(Materials.size()));
        Writer.WriteInt32(static_cast<int32_t>(Bones.size()));
        Writer.WriteInt32(static_cast<int32_t>(Meshes.size()));
        int32_t TotalBuffers = 0, TotalFaceSets = 0, TotalTextures = 0;
        for (const Mesh& Item : Meshes) {
            TotalBuffers += static_cast<int32_t>(Item.VertexBuffers.size());
            TotalFaceSets += static_cast<int32_t>(Item.FaceSets.size());
        }
        for (const Material& Item : Materials) TotalTextures += static_cast<int32_t>(Item.Textures.size());
        Writer.WriteInt32(TotalBuffers);
        Writer.WriteVector3(Header.BoundingBoxMin);
        Writer.WriteVector3(Header.BoundingBoxMax);

        int32_t TrueFaceCount = 0, TotalFaceCount = 0;
        for (const Mesh& Item : Meshes) {
            const bool AllowPrimitiveRestarts = Item.Vertices.size() < 0xFFFF;
            for (const FaceSet& Set : Item.FaceSets) Set.AddFaceCounts(AllowPrimitiveRestarts, TrueFaceCount, TotalFaceCount);
        }
        Writer.WriteInt32(TrueFaceCount);
        Writer.WriteInt32(TotalFaceCount);

        uint8_t VertexIndicesSize = 0;
        if (Header.Version < 0x20013) {
            VertexIndicesSize = 16;
            for (const Mesh& Item : Meshes) {
                for (const FaceSet& Set : Item.FaceSets) {
                    VertexIndicesSize = static_cast<uint8_t>(std::max<int32_t>(VertexIndicesSize, Set.GetVertexIndexSize()));
                }
            }
        }

        Writer.WriteByte(VertexIndicesSize);
        Writer.WriteBool(Header.Unicode);
        Writer.WriteBool(Header.Unk4A);
        Writer.WriteByte(0);

        Writer.WriteInt32(Header.Unk4C);

        Writer.WriteInt32(TotalFaceSets);
        Writer.WriteInt32(static_cast<int32_t>(BufferLayouts.size()));
        Writer.WriteInt32(TotalTextures);

        Writer.WriteByte(Header.Unk5C);
        Writer.WriteByte(Header.Unk5D);
        Writer.WriteByte(0);
        Writer.WriteByte(0);

        Writer.WriteInt32(0);
        Writer.WriteInt32(0);
        Writer.WriteInt32(Header.Unk68);
        for (int I = 0; I < 5; ++I) Writer.WriteInt32(0);

        for (const Dummy& Item : Dummies) Item.Write(Writer, Header.Version);

        // Materials.
        for (size_t I = 0; I < Materials.size(); ++I) {
            const Material& Item = Materials[I];
            Writer.Reserve<int32_t>(Key("MaterialName", I));
            Writer.Reserve<int32_t>(Key("MaterialMTD", I));
            Writer.WriteInt32(static_cast<int32_t>(Item.Textures.size()));
            Writer.Reserve<int32_t>(Key("TextureIndex", I));
            Writer.WriteInt32(Item.Flags);
            Writer.Reserve<int32_t>(Key("GXOffset", I));
            Writer.WriteInt32(Item.Unk18);
            Writer.WriteInt32(0);
        }

        for (size_t I = 0; I < Bones.size(); ++I) Bones[I].Write(Writer, static_cast<int>(I));

        // Meshes.
        for (size_t I = 0; I < Meshes.size(); ++I) {
            const Mesh& Item = Meshes[I];
            Writer.WriteByte(Item.Dynamic);
            Writer.Pad(3);
            Writer.WriteInt32(Item.MaterialIndex);
            Writer.WriteInt32(0);
            Writer.WriteInt32(0);
            Writer.WriteInt32(Item.DefaultBoneIndex);
            Writer.WriteInt32(static_cast<int32_t>(Item.BoneIndices.size()));
            Writer.Reserve<int32_t>(Key("MeshBoundingBox", I));
            Writer.Reserve<int32_t>(Key("MeshBoneIndices", I));
            Writer.WriteInt32(static_cast<int32_t>(Item.FaceSets.size()));
            Writer.Reserve<int32_t>(Key("MeshFaceSetIndices", I));
            Writer.WriteInt32(static_cast<int32_t>(Item.VertexBuffers.size()));
            Writer.Reserve<int32_t>(Key("MeshVertexBufferIndices", I));
        }

        // Face set headers.
        size_t FaceSetIndex = 0;
        for (const Mesh& Item : Meshes) {
            for (size_t I = 0; I < Item.FaceSets.size(); ++I) {
                const FaceSet& Set = Item.FaceSets[I];
                int32_t IndexSize  = VertexIndicesSize;
                if (IndexSize == 0) IndexSize = Set.GetVertexIndexSize();

                Writer.WriteUInt32(static_cast<uint32_t>(Set.Flags));
                Writer.WriteBool(Set.TriangleStrip);
                Writer.WriteBool(Set.CullBackfaces);
                Writer.WriteInt16(Set.Unk06);
                Writer.WriteInt32(static_cast<int32_t>(Set.Indices.size()));
                Writer.Reserve<int32_t>(Key("FaceSetVertices", FaceSetIndex + I));

                if (Header.Version > 0x20005) {
                    Writer.WriteInt32(static_cast<int32_t>(Set.Indices.size()) * (IndexSize / 8));
                    Writer.WriteInt32(0);
                    Writer.WriteInt32(Header.Version >= 0x20013 ? IndexSize : 0);
                    Writer.WriteInt32(0);
                }
            }
            FaceSetIndex += Item.FaceSets.size();
        }

        // Vertex buffer headers.
        size_t VertexBufferIndex = 0;
        for (const Mesh& Item : Meshes) {
            for (size_t I = 0; I < Item.VertexBuffers.size(); ++I) {
                const VertexBuffer& Buffer = Item.VertexBuffers[I];
                if (Buffer.LayoutIndex < 0 || static_cast<size_t>(Buffer.LayoutIndex) >= BufferLayouts.size()) {
                    throw BinaryException("A vertex buffer refers to a layout that doesn't exist");
                }
                const int32_t LayoutBytes = LayoutSize(BufferLayouts[static_cast<size_t>(Buffer.LayoutIndex)]);
                const int32_t VertexCount = static_cast<int32_t>(Item.Vertices.size());

                Writer.WriteInt32(static_cast<int32_t>(I));
                Writer.WriteInt32(Buffer.LayoutIndex);
                Writer.WriteInt32(LayoutBytes);
                Writer.WriteInt32(VertexCount);
                Writer.WriteInt32(0);
                Writer.WriteInt32(0);
                Writer.WriteInt32(Header.Version > 0x20005 ? LayoutBytes * VertexCount : 0);
                Writer.Reserve<int32_t>(Key("VertexBufferOffset", VertexBufferIndex + I));
            }
            VertexBufferIndex += Item.VertexBuffers.size();
        }

        // Buffer layout headers.
        for (size_t I = 0; I < BufferLayouts.size(); ++I) {
            Writer.WriteInt32(static_cast<int32_t>(BufferLayouts[I].size()));
            Writer.WriteInt32(0);
            Writer.WriteInt32(0);
            Writer.Reserve<int32_t>(Key("VertexStructLayout", I));
        }

        // Textures.
        size_t TextureIndex = 0;
        for (size_t I = 0; I < Materials.size(); ++I) {
            Writer.Fill<int32_t>(Key("TextureIndex", I), static_cast<int32_t>(TextureIndex));
            for (size_t J = 0; J < Materials[I].Textures.size(); ++J) {
                const Texture& Item = Materials[I].Textures[J];
                const std::string Index = std::to_string(TextureIndex + J);
                Writer.Reserve<int32_t>("TexturePath" + Index);
                Writer.Reserve<int32_t>("TextureType" + Index);
                Writer.WriteVector2(Item.Scale);

                Writer.WriteByte(Item.Unk10);
                Writer.WriteBool(Item.Unk11);
                Writer.WriteByte(0);
                Writer.WriteByte(0);

                Writer.WriteFloat(Item.Unk14);
                Writer.WriteFloat(Item.Unk18);
                Writer.WriteFloat(Item.Unk1C);
            }
            TextureIndex += Materials[I].Textures.size();
        }

        if (Header.Version >= 0x2001A) {
            const SekiroUnkStruct Unk = SekiroUnk.value_or(SekiroUnkStruct{});
            Writer.WriteInt16(static_cast<int16_t>(Unk.Members1.size()));
            Writer.WriteInt16(static_cast<int16_t>(Unk.Members2.size()));
            Writer.Reserve<uint32_t>("SekiroUnkOffset1");
            Writer.Reserve<uint32_t>("SekiroUnkOffset2");
            for (int I = 0; I < 5; ++I) Writer.WriteInt32(0);

            auto WriteMembers = [&](const char* OffsetName, const std::vector<SekiroUnkStruct::Member>& Members) {
                Writer.Fill<uint32_t>(OffsetName, static_cast<uint32_t>(Writer.Position()));
                for (const SekiroUnkStruct::Member& Member : Members) {
                    for (const int16_t Value : Member.Unk00) Writer.WriteInt16(Value);
                    Writer.WriteInt32(Member.Index);
                    Writer.WriteInt32(0);
                }
            };
            WriteMembers("SekiroUnkOffset1", Unk.Members1);
            WriteMembers("SekiroUnkOffset2", Unk.Members2);
        }

        Writer.Align(0x10);
        for (size_t I = 0; I < BufferLayouts.size(); ++I) {
            Writer.Fill<int32_t>(Key("VertexStructLayout", I), static_cast<int32_t>(Writer.Position()));
            int32_t StructOffset = 0;
            for (const LayoutMember& Member : BufferLayouts[I]) {
                Writer.WriteInt32(Member.Unk00);
                Writer.WriteInt32(StructOffset);
                Writer.WriteUInt32(static_cast<uint32_t>(Member.Type));
                Writer.WriteUInt32(static_cast<uint32_t>(Member.Semantic));
                Writer.WriteInt32(Member.Index);
                StructOffset += Member.Size();
            }
        }

        Writer.Align(0x10);
        for (size_t I = 0; I < Meshes.size(); ++I) {
            const std::optional<BoundingBoxes>& Box = Meshes[I].BoundingBox;
            if (!Box) {
                Writer.Fill<int32_t>(Key("MeshBoundingBox", I), 0);
            } else {
                Writer.Fill<int32_t>(Key("MeshBoundingBox", I), static_cast<int32_t>(Writer.Position()));
                Writer.WriteVector3(Box->Min);
                Writer.WriteVector3(Box->Max);
                if (Header.Version >= 0x2001A) Writer.WriteVector3(Box->Unk);
            }
        }

        Writer.Align(0x10);
        const int32_t BoneIndicesStart = static_cast<int32_t>(Writer.Position());
        for (size_t I = 0; I < Meshes.size(); ++I) {
            if (Meshes[I].BoneIndices.empty()) {
                // Just a weird case for byte-perfect writing.
                Writer.Fill<int32_t>(Key("MeshBoneIndices", I), BoneIndicesStart);
            } else {
                Writer.Fill<int32_t>(Key("MeshBoneIndices", I), static_cast<int32_t>(Writer.Position()));
                Writer.WriteArray(Meshes[I].BoneIndices);
            }
        }

        Writer.Align(0x10);
        FaceSetIndex = 0;
        for (size_t I = 0; I < Meshes.size(); ++I) {
            Writer.Fill<int32_t>(Key("MeshFaceSetIndices", I), static_cast<int32_t>(Writer.Position()));
            for (size_t J = 0; J < Meshes[I].FaceSets.size(); ++J) Writer.WriteInt32(static_cast<int32_t>(FaceSetIndex + J));
            FaceSetIndex += Meshes[I].FaceSets.size();
        }

        Writer.Align(0x10);
        VertexBufferIndex = 0;
        for (size_t I = 0; I < Meshes.size(); ++I) {
            Writer.Fill<int32_t>(Key("MeshVertexBufferIndices", I), static_cast<int32_t>(Writer.Position()));
            for (size_t J = 0; J < Meshes[I].VertexBuffers.size(); ++J) Writer.WriteInt32(static_cast<int32_t>(VertexBufferIndex + J));
            VertexBufferIndex += Meshes[I].VertexBuffers.size();
        }

        Writer.Align(0x10);
        std::vector<int32_t> GXOffsets;
        for (const GXList& List : GXLists) {
            GXOffsets.push_back(static_cast<int32_t>(Writer.Position()));
            WriteGXList(Writer, Header, List);
        }
        for (size_t I = 0; I < Materials.size(); ++I) {
            const int32_t GXIndex = Materials[I].GXIndex;
            if (GXIndex == -1) {
                Writer.Fill<int32_t>(Key("GXOffset", I), 0);
            } else {
                if (GXIndex < 0 || static_cast<size_t>(GXIndex) >= GXOffsets.size()) {
                    throw BinaryException("A material refers to a GX list that doesn't exist");
                }
                Writer.Fill<int32_t>(Key("GXOffset", I), GXOffsets[static_cast<size_t>(GXIndex)]);
            }
        }

        Writer.Align(0x10);
        TextureIndex = 0;
        for (size_t I = 0; I < Materials.size(); ++I) {
            const Material& Item = Materials[I];
            Writer.Fill<int32_t>(Key("MaterialName", I), static_cast<int32_t>(Writer.Position()));
            WriteString(Writer, Item.Name, Header.Unicode);
            Writer.Fill<int32_t>(Key("MaterialMTD", I), static_cast<int32_t>(Writer.Position()));
            WriteString(Writer, Item.MTD, Header.Unicode);

            for (size_t J = 0; J < Item.Textures.size(); ++J) {
                const std::string Index = std::to_string(TextureIndex + J);
                Writer.Fill<int32_t>("TexturePath" + Index, static_cast<int32_t>(Writer.Position()));
                WriteString(Writer, Item.Textures[J].Path, Header.Unicode);
                Writer.Fill<int32_t>("TextureType" + Index, static_cast<int32_t>(Writer.Position()));
                WriteString(Writer, Item.Textures[J].Type, Header.Unicode);
            }
            TextureIndex += Item.Textures.size();
        }

        Writer.Align(0x10);
        for (size_t I = 0; I < Bones.size(); ++I) Bones[I].WriteStrings(Writer, Header.Unicode, static_cast<int>(I));

        const int64_t Alignment = Header.Version <= 0x2000E ? 0x20 : 0x10;
        Writer.Align(Alignment);
        if (Header.Version == 0x2000F || Header.Version == 0x20010) Writer.Align(0x20);

        const int32_t DataStart = static_cast<int32_t>(Writer.Position());
        Writer.Fill<int32_t>("DataOffset", DataStart);

        FaceSetIndex      = 0;
        VertexBufferIndex = 0;
        for (const Mesh& Item : Meshes) {
            for (size_t J = 0; J < Item.FaceSets.size(); ++J) {
                const FaceSet& Set = Item.FaceSets[J];
                int32_t IndexSize  = VertexIndicesSize;
                if (IndexSize == 0) IndexSize = Set.GetVertexIndexSize();

                Writer.Align(Alignment);
                Writer.Fill<int32_t>(Key("FaceSetVertices", FaceSetIndex + J), static_cast<int32_t>(Writer.Position()) - DataStart);
                if (IndexSize == 16) {
                    for (const int32_t Index : Set.Indices) Writer.WriteUInt16(static_cast<uint16_t>(Index));
                } else if (IndexSize == 32) {
                    Writer.WriteArray(Set.Indices);
                } else {
                    throw BinaryException("Unsupported index size: " + std::to_string(IndexSize));
                }
            }
            FaceSetIndex += Item.FaceSets.size();

            Vertex::WriteCursor Cursor;
            for (size_t J = 0; J < Item.VertexBuffers.size(); ++J) {
                const LayoutMembers& Layout = BufferLayouts[static_cast<size_t>(Item.VertexBuffers[J].LayoutIndex)];
                Writer.Align(Alignment);
                Writer.Fill<int32_t>(Key("VertexBufferOffset", VertexBufferIndex + J), static_cast<int32_t>(Writer.Position()) - DataStart);

                const float UVFactor = UVFactorFor(Header.Version);
                Vertex::WriteCursor BufferCursor = Cursor;  // every vertex starts from the same place for this buffer
                for (const Vertex& Vert : Item.Vertices) {
                    BufferCursor = Cursor;
                    Vert.Write(Writer, Layout, UVFactor, BufferCursor);
                }
                Cursor = BufferCursor;  // the next buffer carries on from what this one used
            }
            VertexBufferIndex += Item.VertexBuffers.size();
        }

        Writer.Align(Alignment);
        Writer.Fill<int32_t>("DataSize", static_cast<int32_t>(Writer.Position()) - DataStart);
        if (Header.Version == 0x2000F || Header.Version == 0x20010) Writer.Align(0x20);
    }
}  // namespace Souls
