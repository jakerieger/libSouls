//
// Created by Jake Rieger on 10/8/2026.
//

#include "NGP.hpp"

namespace Souls {
    namespace {
        using Struct5 = NGP::Struct5;

        int64_t VarintSize(BinaryWriter& Writer) {
            return Writer.VarintLong ? 8 : 4;
        }

        Struct5 ReadStruct5(BinaryReader& Reader, int64_t RootOffset, int64_t FaceIndexOffset) {
            Struct5 S;
            S.Unk00                      = Reader.ReadFloat();
            const int16_t LeftIndex      = Reader.ReadInt16();
            const int16_t RightIndex     = Reader.ReadInt16();
            const int16_t FaceIndexCount = Reader.ReadInt16();
            const int16_t FaceIndexIndex = Reader.ReadInt16();
            if (LeftIndex != -1) {
                Reader.Seek(RootOffset + LeftIndex * 0xC);
                S.Left.Emplace(ReadStruct5(Reader, RootOffset, FaceIndexOffset));
            }
            if (RightIndex != -1) {
                Reader.Seek(RootOffset + RightIndex * 0xC);
                S.Right.Emplace(ReadStruct5(Reader, RootOffset, FaceIndexOffset));
            }
            if (FaceIndexCount > 0) {
                Reader.Seek(FaceIndexOffset + FaceIndexIndex * 2);
                S.FaceIndices = Reader.ReadArray<int16_t>(static_cast<size_t>(FaceIndexCount));
            }
            return S;
        }

        void WriteStruct5(BinaryWriter& Writer, const Struct5& S, int16_t& Index) {
            const int16_t ThisIndex = Index;
            const std::string Id    = std::to_string(ThisIndex);
            Writer.WriteFloat(S.Unk00);
            Writer.Reserve<int16_t>("LeftIndex" + Id);
            Writer.Reserve<int16_t>("RightIndex" + Id);
            Writer.Reserve<int16_t>("FaceIndexCount" + Id);
            Writer.Reserve<int16_t>("FaceIndexIndex" + Id);
            if (!S.Left) {
                Writer.Fill<int16_t>("LeftIndex" + Id, -1);
            } else {
                ++Index;
                Writer.Fill<int16_t>("LeftIndex" + Id, Index);
                WriteStruct5(Writer, *S.Left, Index);
            }
            if (!S.Right) {
                Writer.Fill<int16_t>("RightIndex" + Id, -1);
            } else {
                ++Index;
                Writer.Fill<int16_t>("RightIndex" + Id, Index);
                WriteStruct5(Writer, *S.Right, Index);
            }
        }

        void WriteFaceIndices(BinaryWriter& Writer, const Struct5& S, int16_t& Index, int32_t& FaceIndexIndex) {
            const std::string Id = std::to_string(Index);
            if (!S.FaceIndices) {
                Writer.Fill<int16_t>("FaceIndexCount" + Id, 0);
                Writer.Fill<int16_t>("FaceIndexIndex" + Id, 0);
            } else {
                Writer.Fill<int16_t>("FaceIndexCount" + Id, static_cast<int16_t>(S.FaceIndices->size()));
                Writer.Fill<int16_t>("FaceIndexIndex" + Id, static_cast<int16_t>(FaceIndexIndex));
                Writer.WriteArray(*S.FaceIndices);
                FaceIndexIndex += static_cast<int32_t>(S.FaceIndices->size());
            }
            if (S.Left) {
                ++Index;
                WriteFaceIndices(Writer, *S.Left, Index, FaceIndexIndex);
            }
            if (S.Right) {
                ++Index;
                WriteFaceIndices(Writer, *S.Right, Index, FaceIndexIndex);
            }
        }
    }  // namespace

    bool NGP::IsImpl(BinaryReader& Reader) {
        if (Reader.Length() < 4) {
            return false;
        }
        return Reader.GetASCII(0, 4) == "NVG2";
    }

    void NGP::ReadImpl(BinaryReader& Reader) {
        Reader.Order = Endian::Little;
        BigEndian    = Reader.ReadAt<int16_t>(4) == 0x100;
        Reader.Order = BigEndian ? Endian::Big : Endian::Little;
        Reader.AssertMagic("NVG2");
        const uint16_t RawVersion = Reader.ReadUInt16();
        if (RawVersion != 1 && RawVersion != 2) {
            throw BinaryException("Unknown NGP version " + std::to_string(RawVersion));
        }
        Version = static_cast<NGPVersion>(RawVersion);
        Reader.Assert<int16_t>(0);
        const int32_t MeshCount = Reader.ReadInt32();
        const int32_t CountA    = Reader.ReadInt32();
        const int32_t CountB    = Reader.ReadInt32();
        const int32_t CountC    = Reader.ReadInt32();
        const int32_t CountD    = Reader.ReadInt32();
        Unk1C                   = Reader.ReadInt32();
        Reader.VarintLong       = Version == NGPVersion::Scholar;
        const int64_t OffsetA   = Reader.ReadVarint();
        const int64_t OffsetB   = Reader.ReadVarint();
        const int64_t OffsetC   = Reader.ReadVarint();
        const int64_t OffsetD   = Reader.ReadVarint();
        std::vector<int64_t> MeshOffsets;
        for (int32_t I = 0; I < MeshCount; ++I) {
            MeshOffsets.push_back(Reader.ReadVarint());
        }

        Reader.Seek(OffsetA);
        StructAs.clear();
        for (int32_t I = 0; I < CountA; ++I) {
            StructA A;
            A.Unk00 = Reader.ReadVector3();
            A.Unk0C = Reader.ReadFloat();
            A.Unk10 = Reader.ReadInt32();
            A.Unk14 = Reader.ReadInt16();
            A.Unk16 = Reader.ReadInt16();
            A.Unk18 = Reader.ReadInt16();
            A.Unk1A = Reader.ReadInt16();
            A.Unk1C = Reader.ReadInt16();
            A.Unk1E = Reader.ReadInt16();
            A.Unk20 = Reader.ReadInt16();
            A.Unk22 = Reader.ReadInt16();
            StructAs.push_back(A);
        }
        Reader.Seek(OffsetB);
        StructBs.clear();
        for (int32_t I = 0; I < CountB; ++I) {
            StructBs.push_back({Reader.ReadInt32(), Reader.ReadInt32(), Reader.ReadInt32()});
        }
        Reader.Seek(OffsetC);
        StructCs = Reader.ReadArray<int32_t>(static_cast<size_t>(CountC));
        Reader.Seek(OffsetD);
        StructDs = Reader.ReadArray<int16_t>(static_cast<size_t>(CountD));

        Meshes.clear();
        for (int32_t I = 0; I < MeshCount; ++I) {
            Reader.Seek(MeshOffsets[static_cast<size_t>(I)]);
            Mesh M;
            M.Unk00 = Reader.ReadInt32();
            Reader.ReadInt32();  // length of this mesh (including child structs)
            M.Unk08 = Reader.ReadInt32();
            if (Version == NGPVersion::Scholar) {
                Reader.Assert<int32_t>(0);
            }
            M.BoundingBoxMin             = Reader.ReadVector3();
            M.BoundingBoxMax             = Reader.ReadVector3();
            const int32_t VertexCount    = Reader.ReadInt32();
            const int16_t FaceCount      = Reader.ReadInt16();
            const int16_t Count30        = Reader.ReadInt16();
            M.Unk30                      = Reader.ReadInt16();
            M.Unk32                      = Reader.ReadInt16();
            Reader.Assert<uint8_t>(1);
            Reader.Assert<uint8_t>(0);
            Reader.Assert<uint8_t>(0);
            Reader.Assert<uint8_t>(0);
            if (Version == NGPVersion::Scholar) {
                Reader.Assert<int64_t>(0);
            }
            const int64_t VerticesOffset = Reader.ReadVarint();
            const int64_t Offset48       = Reader.ReadVarint();
            const int64_t FacesOffset    = Reader.ReadVarint();
            const int64_t Offset58       = Reader.ReadVarint();
            const int64_t Offset60       = Reader.ReadVarint();
            const int64_t Offset68       = Reader.ReadVarint();

            Reader.Seek(VerticesOffset);
            for (int32_t V = 0; V < VertexCount; ++V) {
                M.Vertices.push_back(Reader.ReadVector3());
            }
            Reader.Seek(Offset48);
            M.Struct2s = Reader.ReadArray<int32_t>(static_cast<size_t>(FaceCount));
            Reader.Seek(FacesOffset);
            for (int16_t F = 0; F < FaceCount; ++F) {
                Face Fc;
                Fc.V1    = Reader.ReadInt16();
                Fc.V2    = Reader.ReadInt16();
                Fc.V3    = Reader.ReadInt16();
                Fc.Unk06 = Reader.ReadInt16();
                Fc.Unk08 = Reader.ReadInt16();
                Fc.Unk0A = Reader.ReadInt16();
                M.Faces.push_back(Fc);
            }
            Reader.Seek(Offset58);
            for (int16_t S = 0; S < Count30; ++S) {
                Struct4 S4;
                S4.Unk00 = Reader.ReadInt16();
                S4.Unk02 = Reader.ReadInt16();
                S4.Unk04 = Reader.ReadInt16();
                S4.Unk06 = Reader.ReadInt16();
                S4.Unk08 = Reader.ReadInt16();
                S4.Unk0A = Reader.ReadInt16();
                S4.Unk0C = Reader.ReadInt16();
                S4.Unk0E = Reader.ReadInt16();
                M.Struct4s.push_back(S4);
            }
            Reader.Seek(Offset60);
            M.Struct5Root = ReadStruct5(Reader, Offset60, Offset68);
            Meshes.push_back(std::move(M));
        }
    }

    void NGP::WriteImpl(BinaryWriter& Writer) {
        Writer.Order      = BigEndian ? Endian::Big : Endian::Little;
        Writer.VarintLong = Version == NGPVersion::Scholar;
        Writer.WriteMagic("NVG2");
        Writer.WriteUInt16(static_cast<uint16_t>(Version));
        Writer.WriteInt16(0);
        Writer.WriteInt32(static_cast<int32_t>(Meshes.size()));
        Writer.WriteInt32(static_cast<int32_t>(StructAs.size()));
        Writer.WriteInt32(static_cast<int32_t>(StructBs.size()));
        Writer.WriteInt32(static_cast<int32_t>(StructCs.size()));
        Writer.WriteInt32(static_cast<int32_t>(StructDs.size()));
        Writer.WriteInt32(Unk1C);
        Writer.ReserveVarint("OffsetA");
        Writer.ReserveVarint("OffsetB");
        Writer.ReserveVarint("OffsetC");
        Writer.ReserveVarint("OffsetD");
        for (size_t I = 0; I < Meshes.size(); ++I) {
            Writer.ReserveVarint("MeshOffset" + std::to_string(I));
        }

        const auto WriteMeshes = [&] {
            for (size_t I = 0; I < Meshes.size(); ++I) {
                const Mesh& M = Meshes[I];
                Writer.Align(VarintSize(Writer));
                Writer.FillVarint("MeshOffset" + std::to_string(I), Writer.Position());

                const int64_t Start = Writer.Position();
                Writer.WriteInt32(M.Unk00);
                Writer.Reserve<int32_t>("MeshLength");
                Writer.WriteInt32(M.Unk08);
                if (Version == NGPVersion::Scholar) {
                    Writer.WriteInt32(0);
                }
                Writer.WriteVector3(M.BoundingBoxMin);
                Writer.WriteVector3(M.BoundingBoxMax);
                Writer.WriteInt32(static_cast<int32_t>(M.Vertices.size()));
                Writer.WriteInt16(static_cast<int16_t>(M.Faces.size()));
                Writer.WriteInt16(static_cast<int16_t>(M.Struct4s.size()));
                Writer.WriteInt16(M.Unk30);
                Writer.WriteInt16(M.Unk32);
                Writer.WriteByte(1);
                Writer.WriteByte(0);
                Writer.WriteByte(0);
                Writer.WriteByte(0);
                if (Version == NGPVersion::Scholar) {
                    Writer.WriteInt64(0);
                }
                Writer.ReserveVarint("VerticesOffset");
                Writer.ReserveVarint("Struct2sOffset");
                Writer.ReserveVarint("FacesOffset");
                Writer.ReserveVarint("Struct4sOffset");
                Writer.ReserveVarint("Struct5sOffset");
                Writer.ReserveVarint("Struct6sOffset");

                Writer.FillVarint("VerticesOffset", Writer.Position());
                for (const Vector3& V : M.Vertices) {
                    Writer.WriteVector3(V);
                }
                Writer.Align(VarintSize(Writer));
                Writer.FillVarint("Struct2sOffset", Writer.Position());
                for (const int32_t S : M.Struct2s) {
                    Writer.WriteInt32(S);
                }
                Writer.Align(VarintSize(Writer));
                Writer.FillVarint("FacesOffset", Writer.Position());
                for (const Face& F : M.Faces) {
                    Writer.WriteInt16(F.V1);
                    Writer.WriteInt16(F.V2);
                    Writer.WriteInt16(F.V3);
                    Writer.WriteInt16(F.Unk06);
                    Writer.WriteInt16(F.Unk08);
                    Writer.WriteInt16(F.Unk0A);
                }
                Writer.Align(VarintSize(Writer));
                Writer.FillVarint("Struct4sOffset", Writer.Position());
                for (const Struct4& S : M.Struct4s) {
                    Writer.WriteInt16(S.Unk00);
                    Writer.WriteInt16(S.Unk02);
                    Writer.WriteInt16(S.Unk04);
                    Writer.WriteInt16(S.Unk06);
                    Writer.WriteInt16(S.Unk08);
                    Writer.WriteInt16(S.Unk0A);
                    Writer.WriteInt16(S.Unk0C);
                    Writer.WriteInt16(S.Unk0E);
                }
                Writer.Align(VarintSize(Writer));
                Writer.FillVarint("Struct5sOffset", Writer.Position());
                int16_t Index = 0;
                WriteStruct5(Writer, M.Struct5Root, Index);
                Writer.Align(VarintSize(Writer));
                Writer.FillVarint("Struct6sOffset", Writer.Position());
                Index                = 0;
                int32_t FaceIndexIdx = 0;
                WriteFaceIndices(Writer, M.Struct5Root, Index, FaceIndexIdx);
                Writer.Align(VarintSize(Writer));
                Writer.Fill<int32_t>("MeshLength", static_cast<int32_t>(Writer.Position() - Start));
            }
        };

        if (Version == NGPVersion::Vanilla) {
            WriteMeshes();
        }
        Writer.Align(VarintSize(Writer));
        Writer.FillVarint("OffsetA", Writer.Position());
        for (const StructA& A : StructAs) {
            Writer.WriteVector3(A.Unk00);
            Writer.WriteFloat(A.Unk0C);
            Writer.WriteInt32(A.Unk10);
            Writer.WriteInt16(A.Unk14);
            Writer.WriteInt16(A.Unk16);
            Writer.WriteInt16(A.Unk18);
            Writer.WriteInt16(A.Unk1A);
            Writer.WriteInt16(A.Unk1C);
            Writer.WriteInt16(A.Unk1E);
            Writer.WriteInt16(A.Unk20);
            Writer.WriteInt16(A.Unk22);
        }
        Writer.Align(VarintSize(Writer));
        Writer.FillVarint("OffsetB", Writer.Position());
        for (const StructB& B : StructBs) {
            Writer.WriteInt32(B.Unk00);
            Writer.WriteInt32(B.Unk04);
            Writer.WriteInt32(B.Unk08);
        }
        Writer.Align(VarintSize(Writer));
        Writer.FillVarint("OffsetC", Writer.Position());
        Writer.WriteArray(StructCs);
        Writer.Align(VarintSize(Writer));
        Writer.FillVarint("OffsetD", Writer.Position());
        Writer.WriteArray(StructDs);
        if (Version == NGPVersion::Scholar) {
            WriteMeshes();
        }
    }
}  // namespace Souls
