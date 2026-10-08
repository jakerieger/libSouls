//
// Created by Jake Rieger on 10/8/2026.
//

#include "FlverCommon.hpp"

#include <libSouls/TextEncoding.hpp>

#include <cmath>

namespace Souls::FLVER {
    namespace {
        [[noreturn]] void NotImplemented(const char* What, LayoutType Type, LayoutSemantic Semantic) {
            throw BinaryException(std::string(What) + " not implemented for " + LayoutTypeName(Type) + " " +
                                  LayoutSemanticName(Semantic));
        }

        // Math.Round: halves go to the even neighbour, which is also the default rounding of nearbyint.
        double Round(double Value) {
            return std::nearbyint(Value);
        }

        uint8_t ToByte(double RoundedValue) {
            return static_cast<uint8_t>(static_cast<int64_t>(RoundedValue));
        }

        int8_t ToSByte(double RoundedValue) {
            return static_cast<int8_t>(static_cast<int64_t>(RoundedValue));
        }

        int16_t ToShort(double RoundedValue) {
            return static_cast<int16_t>(static_cast<int64_t>(RoundedValue));
        }

        uint16_t ToUShort(double RoundedValue) {
            return static_cast<uint16_t>(static_cast<int64_t>(RoundedValue));
        }

#pragma region Read helpers
        float ReadByteNorm(BinaryReader& R) {
            return (static_cast<int>(R.ReadByte()) - 127) / 127.f;
        }

        Vector3 ReadByteNormXYZ(BinaryReader& R) {
            const float X = ReadByteNorm(R);
            const float Y = ReadByteNorm(R);
            const float Z = ReadByteNorm(R);
            return {X, Y, Z};
        }

        Vector4 ReadByteNormXYZW(BinaryReader& R) {
            const float X = ReadByteNorm(R);
            const float Y = ReadByteNorm(R);
            const float Z = ReadByteNorm(R);
            const float W = ReadByteNorm(R);
            return {X, Y, Z, W};
        }

        float ReadSByteNorm(BinaryReader& R) {
            return R.ReadSByte() / 127.f;
        }

        Vector3 ReadSByteNormZYX(BinaryReader& R) {
            const float Z = ReadSByteNorm(R);
            const float Y = ReadSByteNorm(R);
            const float X = ReadSByteNorm(R);
            return {X, Y, Z};
        }

        float ReadShortNorm(BinaryReader& R) {
            return R.ReadInt16() / 32767.f;
        }

        Vector3 ReadShortNormXYZ(BinaryReader& R) {
            const float X = ReadShortNorm(R);
            const float Y = ReadShortNorm(R);
            const float Z = ReadShortNorm(R);
            return {X, Y, Z};
        }

        Vector4 ReadShortNormXYZW(BinaryReader& R) {
            const float X = ReadShortNorm(R);
            const float Y = ReadShortNorm(R);
            const float Z = ReadShortNorm(R);
            const float W = ReadShortNorm(R);
            return {X, Y, Z, W};
        }

        float ReadUShortNorm(BinaryReader& R) {
            return (static_cast<int>(R.ReadUInt16()) - 32767) / 32767.f;
        }

        Vector3 ReadUShortNormXYZ(BinaryReader& R) {
            const float X = ReadUShortNorm(R);
            const float Y = ReadUShortNorm(R);
            const float Z = ReadUShortNorm(R);
            return {X, Y, Z};
        }

        // Two shorts, divided down to a UV.
        Vector3 ReadShortUV(BinaryReader& R, float UVFactor) {
            const float U = static_cast<float>(R.ReadInt16());
            const float V = static_cast<float>(R.ReadInt16());
            return {U / UVFactor, V / UVFactor, 0.f};
        }

        VertexColor ReadFloatRGBA(BinaryReader& R) {
            const float Red   = R.ReadFloat();
            const float Green = R.ReadFloat();
            const float Blue  = R.ReadFloat();
            const float Alpha = R.ReadFloat();
            return {Alpha, Red, Green, Blue};
        }

        VertexColor ReadByteRGBA(BinaryReader& R) {
            const uint8_t Red   = R.ReadByte();
            const uint8_t Green = R.ReadByte();
            const uint8_t Blue  = R.ReadByte();
            const uint8_t Alpha = R.ReadByte();
            return {Alpha, Red, Green, Blue};
        }
#pragma endregion

#pragma region Write helpers
        void WriteByteNorm(BinaryWriter& W, float Value) {
            W.WriteByte(ToByte(Round(Value * 127.f + 127.f)));
        }

        void WriteByteNormXYZ(BinaryWriter& W, const Vector3& V) {
            WriteByteNorm(W, V.X);
            WriteByteNorm(W, V.Y);
            WriteByteNorm(W, V.Z);
        }

        void WriteByteNormXYZW(BinaryWriter& W, const Vector4& V) {
            WriteByteNorm(W, V.X);
            WriteByteNorm(W, V.Y);
            WriteByteNorm(W, V.Z);
            WriteByteNorm(W, V.W);
        }

        void WriteSByteNorm(BinaryWriter& W, float Value) {
            W.WriteSByte(ToSByte(Round(Value * 127.f)));
        }

        void WriteSByteNormZYX(BinaryWriter& W, const Vector3& V) {
            WriteSByteNorm(W, V.Z);
            WriteSByteNorm(W, V.Y);
            WriteSByteNorm(W, V.X);
        }

        void WriteShortNorm(BinaryWriter& W, float Value) {
            W.WriteInt16(ToShort(Round(Value * 32767.f)));
        }

        void WriteShortNormXYZ(BinaryWriter& W, const Vector3& V) {
            WriteShortNorm(W, V.X);
            WriteShortNorm(W, V.Y);
            WriteShortNorm(W, V.Z);
        }

        void WriteShortNormXYZW(BinaryWriter& W, const Vector4& V) {
            WriteShortNorm(W, V.X);
            WriteShortNorm(W, V.Y);
            WriteShortNorm(W, V.Z);
            WriteShortNorm(W, V.W);
        }

        void WriteUShortNorm(BinaryWriter& W, float Value) {
            W.WriteUInt16(ToUShort(Round(Value * 32767.f + 32767.f)));
        }

        void WriteUShortNormXYZ(BinaryWriter& W, const Vector3& V) {
            WriteUShortNorm(W, V.X);
            WriteUShortNorm(W, V.Y);
            WriteUShortNorm(W, V.Z);
        }

        void WriteShortUV(BinaryWriter& W, const Vector3& Scaled) {
            W.WriteInt16(ToShort(Round(Scaled.X)));
            W.WriteInt16(ToShort(Round(Scaled.Y)));
        }

        void WriteFloatRGBA(BinaryWriter& W, const VertexColor& C) {
            W.WriteFloat(C.R);
            W.WriteFloat(C.G);
            W.WriteFloat(C.B);
            W.WriteFloat(C.A);
        }

        void WriteByteRGBA(BinaryWriter& W, const VertexColor& C) {
            W.WriteByte(ToByte(Round(C.R * 255.f)));
            W.WriteByte(ToByte(Round(C.G * 255.f)));
            W.WriteByte(ToByte(Round(C.B * 255.f)));
            W.WriteByte(ToByte(Round(C.A * 255.f)));
        }

        Vector3 Scaled(const Vector3& V, float Factor) {
            return {V.X * Factor, V.Y * Factor, V.Z * Factor};
        }
#pragma endregion
    }  // namespace

    const char* LayoutTypeName(LayoutType Type) {
        switch (Type) {
            case LayoutType::Float2: return "Float2";
            case LayoutType::Float3: return "Float3";
            case LayoutType::Float4: return "Float4";
            case LayoutType::Byte4A: return "Byte4A";
            case LayoutType::Byte4B: return "Byte4B";
            case LayoutType::Short2toFloat2: return "Short2toFloat2";
            case LayoutType::Byte4C: return "Byte4C";
            case LayoutType::UV: return "UV";
            case LayoutType::UVPair: return "UVPair";
            case LayoutType::ShortBoneIndices: return "ShortBoneIndices";
            case LayoutType::Short4toFloat4A: return "Short4toFloat4A";
            case LayoutType::Short4toFloat4B: return "Short4toFloat4B";
            case LayoutType::Byte4E: return "Byte4E";
            case LayoutType::EdgeCompressed: return "EdgeCompressed";
        }
        return "Unknown";
    }

    const char* LayoutSemanticName(LayoutSemantic Semantic) {
        switch (Semantic) {
            case LayoutSemantic::Position: return "Position";
            case LayoutSemantic::BoneWeights: return "BoneWeights";
            case LayoutSemantic::BoneIndices: return "BoneIndices";
            case LayoutSemantic::Normal: return "Normal";
            case LayoutSemantic::UV: return "UV";
            case LayoutSemantic::Tangent: return "Tangent";
            case LayoutSemantic::Bitangent: return "Bitangent";
            case LayoutSemantic::VertexColor: return "VertexColor";
        }
        return "Unknown";
    }

    int32_t LayoutMember::Size() const {
        switch (Type) {
            case LayoutType::EdgeCompressed: return 1;
            case LayoutType::Byte4A:
            case LayoutType::Byte4B:
            case LayoutType::Short2toFloat2:
            case LayoutType::Byte4C:
            case LayoutType::UV:
            case LayoutType::Byte4E: return 4;
            case LayoutType::Float2:
            case LayoutType::UVPair:
            case LayoutType::ShortBoneIndices:
            case LayoutType::Short4toFloat4A:
            case LayoutType::Short4toFloat4B: return 8;
            case LayoutType::Float3: return 12;
            case LayoutType::Float4: return 16;
        }
        throw BinaryException("No size defined for buffer layout type " + std::to_string(static_cast<uint32_t>(Type)));
    }

    std::string LayoutMember::ToString() const {
        return std::string(LayoutTypeName(Type)) + ": " + LayoutSemanticName(Semantic);
    }

    int32_t LayoutSize(const LayoutMembers& Layout) {
        int32_t Size = 0;
        for (const LayoutMember& Member : Layout) Size += Member.Size();
        return Size;
    }

    void Vertex::Read(BinaryReader& Reader, const LayoutMembers& Layout, float UVFactor) {
        for (const LayoutMember& Member : Layout) {
            const LayoutType Type = Member.Type;
            switch (Member.Semantic) {
                case LayoutSemantic::Position:
                    if (Type == LayoutType::Float3) {
                        Position = Reader.ReadVector3();
                    } else if (Type == LayoutType::Float4) {
                        Position = Reader.ReadVector3();
                        Reader.Assert<float>(0.f);
                    } else if (Type == LayoutType::EdgeCompressed) {
                        // Edge-compressed vertices (PS3) have nothing to read here.
                    } else {
                        NotImplemented("Read", Type, Member.Semantic);
                    }
                    break;

                case LayoutSemantic::BoneWeights:
                    if (Type == LayoutType::Byte4A) {
                        for (float& Weight : BoneWeights) Weight = Reader.ReadSByte() / 127.f;
                    } else if (Type == LayoutType::Byte4C) {
                        for (float& Weight : BoneWeights) Weight = Reader.ReadByte() / 255.f;
                    } else if (Type == LayoutType::UVPair || Type == LayoutType::Short4toFloat4A) {
                        for (float& Weight : BoneWeights) Weight = Reader.ReadInt16() / 32767.f;
                    } else {
                        NotImplemented("Read", Type, Member.Semantic);
                    }
                    break;

                case LayoutSemantic::BoneIndices:
                    if (Type == LayoutType::Byte4B || Type == LayoutType::Byte4E) {
                        for (int32_t& Index : BoneIndices) Index = Reader.ReadByte();
                    } else if (Type == LayoutType::ShortBoneIndices) {
                        for (int32_t& Index : BoneIndices) Index = Reader.ReadUInt16();
                    } else {
                        NotImplemented("Read", Type, Member.Semantic);
                    }
                    break;

                case LayoutSemantic::Normal:
                    if (Type == LayoutType::Float3) {
                        Normal = Reader.ReadVector3();
                    } else if (Type == LayoutType::Float4) {
                        Normal              = Reader.ReadVector3();
                        const float W       = Reader.ReadFloat();
                        NormalW             = static_cast<int32_t>(W);
                        if (W != static_cast<float>(NormalW)) {
                            throw BinaryException("Float4 Normal W was not a whole number: " + std::to_string(W));
                        }
                    } else if (Type == LayoutType::Byte4A || Type == LayoutType::Byte4B || Type == LayoutType::Byte4C ||
                               Type == LayoutType::Byte4E) {
                        Normal  = ReadByteNormXYZ(Reader);
                        NormalW = Reader.ReadByte();
                    } else if (Type == LayoutType::Short2toFloat2) {
                        NormalW = Reader.ReadByte();
                        Normal  = ReadSByteNormZYX(Reader);
                    } else if (Type == LayoutType::Short4toFloat4A) {
                        Normal  = ReadShortNormXYZ(Reader);
                        NormalW = Reader.ReadInt16();
                    } else if (Type == LayoutType::Short4toFloat4B) {
                        Normal  = ReadUShortNormXYZ(Reader);
                        NormalW = Reader.ReadInt16();
                    } else {
                        NotImplemented("Read", Type, Member.Semantic);
                    }
                    break;

                case LayoutSemantic::UV:
                    if (Type == LayoutType::Float2) {
                        const Vector2 UV = Reader.ReadVector2();
                        UVs.push_back({UV.X, UV.Y, 0.f});
                    } else if (Type == LayoutType::Float3) {
                        UVs.push_back(Reader.ReadVector3());
                    } else if (Type == LayoutType::Float4) {
                        const Vector2 First  = Reader.ReadVector2();
                        const Vector2 Second = Reader.ReadVector2();
                        UVs.push_back({First.X, First.Y, 0.f});
                        UVs.push_back({Second.X, Second.Y, 0.f});
                    } else if (Type == LayoutType::Byte4A || Type == LayoutType::Byte4B || Type == LayoutType::Short2toFloat2 ||
                               Type == LayoutType::Byte4C || Type == LayoutType::UV) {
                        UVs.push_back(ReadShortUV(Reader, UVFactor));
                    } else if (Type == LayoutType::UVPair) {
                        UVs.push_back(ReadShortUV(Reader, UVFactor));
                        UVs.push_back(ReadShortUV(Reader, UVFactor));
                    } else if (Type == LayoutType::Short4toFloat4B) {
                        const float X = static_cast<float>(Reader.ReadInt16());
                        const float Y = static_cast<float>(Reader.ReadInt16());
                        const float Z = static_cast<float>(Reader.ReadInt16());
                        UVs.push_back({X / UVFactor, Y / UVFactor, Z / UVFactor});
                        Reader.Assert<int16_t>(0);
                    } else {
                        NotImplemented("Read", Type, Member.Semantic);
                    }
                    break;

                case LayoutSemantic::Tangent:
                    if (Type == LayoutType::Float4) {
                        Tangents.push_back(Reader.ReadVector4());
                    } else if (Type == LayoutType::Byte4A || Type == LayoutType::Byte4B || Type == LayoutType::Byte4C ||
                               Type == LayoutType::Byte4E) {
                        Tangents.push_back(ReadByteNormXYZW(Reader));
                    } else if (Type == LayoutType::Short4toFloat4A) {
                        Tangents.push_back(ReadShortNormXYZW(Reader));
                    } else {
                        NotImplemented("Read", Type, Member.Semantic);
                    }
                    break;

                case LayoutSemantic::Bitangent:
                    if (Type == LayoutType::Byte4A || Type == LayoutType::Byte4B || Type == LayoutType::Byte4C ||
                        Type == LayoutType::Byte4E) {
                        Bitangent = ReadByteNormXYZW(Reader);
                    } else {
                        NotImplemented("Read", Type, Member.Semantic);
                    }
                    break;

                case LayoutSemantic::VertexColor:
                    if (Type == LayoutType::Float4) {
                        Colors.push_back(ReadFloatRGBA(Reader));
                    } else if (Type == LayoutType::Byte4A || Type == LayoutType::Byte4C) {
                        Colors.push_back(ReadByteRGBA(Reader));  // RGBA in both Demon's Souls and Dark Souls
                    } else {
                        NotImplemented("Read", Type, Member.Semantic);
                    }
                    break;

                default: NotImplemented("Read", Type, Member.Semantic);
            }
        }
    }

    void Vertex::Write(BinaryWriter& Writer, const LayoutMembers& Layout, float UVFactor, WriteCursor& Cursor) const {
        auto NextUV = [&]() -> Vector3 {
            if (Cursor.UV >= UVs.size()) throw BinaryException("A vertex has fewer UVs than its layout needs");
            return Scaled(UVs[Cursor.UV++], UVFactor);
        };

        for (const LayoutMember& Member : Layout) {
            const LayoutType Type = Member.Type;
            switch (Member.Semantic) {
                case LayoutSemantic::Position:
                    if (Type == LayoutType::Float3) {
                        Writer.WriteVector3(Position);
                    } else if (Type == LayoutType::Float4) {
                        Writer.WriteVector3(Position);
                        Writer.WriteFloat(0.f);
                    } else {
                        NotImplemented("Write", Type, Member.Semantic);
                    }
                    break;

                case LayoutSemantic::BoneWeights:
                    if (Type == LayoutType::Byte4A) {
                        for (const float Weight : BoneWeights) Writer.WriteSByte(ToSByte(Round(Weight * 127.f)));
                    } else if (Type == LayoutType::Byte4C) {
                        for (const float Weight : BoneWeights) Writer.WriteByte(ToByte(Round(Weight * 255.f)));
                    } else if (Type == LayoutType::UVPair || Type == LayoutType::Short4toFloat4A) {
                        for (const float Weight : BoneWeights) Writer.WriteInt16(ToShort(Round(Weight * 32767.f)));
                    } else {
                        NotImplemented("Write", Type, Member.Semantic);
                    }
                    break;

                case LayoutSemantic::BoneIndices:
                    if (Type == LayoutType::Byte4B || Type == LayoutType::Byte4E) {
                        for (const int32_t Index : BoneIndices) Writer.WriteByte(static_cast<uint8_t>(Index));
                    } else if (Type == LayoutType::ShortBoneIndices) {
                        for (const int32_t Index : BoneIndices) Writer.WriteUInt16(static_cast<uint16_t>(Index));
                    } else {
                        NotImplemented("Write", Type, Member.Semantic);
                    }
                    break;

                case LayoutSemantic::Normal:
                    if (Type == LayoutType::Float3) {
                        Writer.WriteVector3(Normal);
                    } else if (Type == LayoutType::Float4) {
                        Writer.WriteVector3(Normal);
                        Writer.WriteFloat(static_cast<float>(NormalW));
                    } else if (Type == LayoutType::Byte4A || Type == LayoutType::Byte4B || Type == LayoutType::Byte4C ||
                               Type == LayoutType::Byte4E) {
                        WriteByteNormXYZ(Writer, Normal);
                        Writer.WriteByte(static_cast<uint8_t>(NormalW));
                    } else if (Type == LayoutType::Short2toFloat2) {
                        Writer.WriteByte(static_cast<uint8_t>(NormalW));
                        WriteSByteNormZYX(Writer, Normal);
                    } else if (Type == LayoutType::Short4toFloat4A) {
                        WriteShortNormXYZ(Writer, Normal);
                        Writer.WriteInt16(static_cast<int16_t>(NormalW));
                    } else if (Type == LayoutType::Short4toFloat4B) {
                        WriteUShortNormXYZ(Writer, Normal);
                        Writer.WriteInt16(static_cast<int16_t>(NormalW));
                    } else {
                        NotImplemented("Write", Type, Member.Semantic);
                    }
                    break;

                case LayoutSemantic::UV: {
                    Vector3 UV = NextUV();
                    if (Type == LayoutType::Float2) {
                        Writer.WriteFloat(UV.X);
                        Writer.WriteFloat(UV.Y);
                    } else if (Type == LayoutType::Float3) {
                        Writer.WriteVector3(UV);
                    } else if (Type == LayoutType::Float4) {
                        Writer.WriteFloat(UV.X);
                        Writer.WriteFloat(UV.Y);
                        UV = NextUV();
                        Writer.WriteFloat(UV.X);
                        Writer.WriteFloat(UV.Y);
                    } else if (Type == LayoutType::Byte4A || Type == LayoutType::Byte4B || Type == LayoutType::Short2toFloat2 ||
                               Type == LayoutType::Byte4C || Type == LayoutType::UV) {
                        WriteShortUV(Writer, UV);
                    } else if (Type == LayoutType::UVPair) {
                        WriteShortUV(Writer, UV);
                        UV = NextUV();
                        WriteShortUV(Writer, UV);
                    } else if (Type == LayoutType::Short4toFloat4B) {
                        Writer.WriteInt16(ToShort(Round(UV.X)));
                        Writer.WriteInt16(ToShort(Round(UV.Y)));
                        Writer.WriteInt16(ToShort(Round(UV.Z)));
                        Writer.WriteInt16(0);
                    } else {
                        NotImplemented("Write", Type, Member.Semantic);
                    }
                    break;
                }

                case LayoutSemantic::Tangent: {
                    if (Cursor.Tangent >= Tangents.size()) throw BinaryException("A vertex has fewer tangents than its layout needs");
                    const Vector4& Tangent = Tangents[Cursor.Tangent++];
                    if (Type == LayoutType::Float4) {
                        Writer.WriteVector4(Tangent);
                    } else if (Type == LayoutType::Byte4A || Type == LayoutType::Byte4B || Type == LayoutType::Byte4C ||
                               Type == LayoutType::Byte4E) {
                        WriteByteNormXYZW(Writer, Tangent);
                    } else if (Type == LayoutType::Short4toFloat4A) {
                        WriteShortNormXYZW(Writer, Tangent);
                    } else {
                        NotImplemented("Write", Type, Member.Semantic);
                    }
                    break;
                }

                case LayoutSemantic::Bitangent:
                    if (Type == LayoutType::Byte4A || Type == LayoutType::Byte4B || Type == LayoutType::Byte4C ||
                        Type == LayoutType::Byte4E) {
                        WriteByteNormXYZW(Writer, Bitangent);
                    } else {
                        NotImplemented("Write", Type, Member.Semantic);
                    }
                    break;

                case LayoutSemantic::VertexColor: {
                    if (Cursor.Color >= Colors.size()) throw BinaryException("A vertex has fewer colors than its layout needs");
                    const VertexColor& Color = Colors[Cursor.Color++];
                    if (Type == LayoutType::Float4) {
                        WriteFloatRGBA(Writer, Color);
                    } else if (Type == LayoutType::Byte4A || Type == LayoutType::Byte4C) {
                        WriteByteRGBA(Writer, Color);
                    } else {
                        NotImplemented("Write", Type, Member.Semantic);
                    }
                    break;
                }

                default: NotImplemented("Write", Type, Member.Semantic);
            }
        }
    }

    Matrix4x4 Bone::ComputeLocalTransform() const {
        return Matrix4x4::CreateScale(Scale) * Matrix4x4::CreateRotationX(Rotation.X) * Matrix4x4::CreateRotationZ(Rotation.Z) *
               Matrix4x4::CreateRotationY(Rotation.Y) * Matrix4x4::CreateTranslation(Translation);
    }

    void Bone::Read(BinaryReader& Reader, bool Unicode) {
        Translation              = Reader.ReadVector3();
        const int32_t NameOffset = Reader.ReadInt32();
        Rotation                 = Reader.ReadVector3();
        ParentIndex              = Reader.ReadInt16();
        ChildIndex               = Reader.ReadInt16();
        Scale                    = Reader.ReadVector3();
        NextSiblingIndex         = Reader.ReadInt16();
        PreviousSiblingIndex     = Reader.ReadInt16();
        BoundingBoxMin           = Reader.ReadVector3();
        Unk3C                    = Reader.ReadInt32();
        BoundingBoxMax           = Reader.ReadVector3();
        Reader.AssertPattern(0x34, 0x00);

        Reader.StepIn(NameOffset);
        Name = Unicode ? Text::UTF16ToUTF8(Reader.ReadUTF16()) : Reader.ReadShiftJIS();
        Reader.StepOut();
    }

    void Bone::Write(BinaryWriter& Writer, int Index) const {
        Writer.WriteVector3(Translation);
        Writer.Reserve<int32_t>("BoneNameOffset" + std::to_string(Index));
        Writer.WriteVector3(Rotation);
        Writer.WriteInt16(ParentIndex);
        Writer.WriteInt16(ChildIndex);
        Writer.WriteVector3(Scale);
        Writer.WriteInt16(NextSiblingIndex);
        Writer.WriteInt16(PreviousSiblingIndex);
        Writer.WriteVector3(BoundingBoxMin);
        Writer.WriteInt32(Unk3C);
        Writer.WriteVector3(BoundingBoxMax);
        Writer.Pad(0x34);
    }

    void Bone::WriteStrings(BinaryWriter& Writer, bool Unicode, int Index) const {
        Writer.Fill<int32_t>("BoneNameOffset" + std::to_string(Index), static_cast<int32_t>(Writer.Position()));
        if (Unicode) {
            Writer.WriteUTF16(Text::UTF8ToUTF16(Name), true);
        } else {
            Writer.WriteShiftJIS(Name, true);
        }
    }

    void Dummy::Read(BinaryReader& Reader, int32_t Version) {
        Position = Reader.ReadVector3();
        // Not certain about the ordering of the color channels here.
        const uint8_t First  = Reader.ReadByte();
        const uint8_t Second = Reader.ReadByte();
        const uint8_t Third  = Reader.ReadByte();
        const uint8_t Fourth = Reader.ReadByte();
        if (Version == 0x20010) {  // BGRA
            Tint = {Fourth, Third, Second, First};
        } else {  // ARGB
            Tint = {First, Second, Third, Fourth};
        }
        Forward         = Reader.ReadVector3();
        ReferenceID     = Reader.ReadInt16();
        ParentBoneIndex = Reader.ReadInt16();
        Upward          = Reader.ReadVector3();
        AttachBoneIndex = Reader.ReadInt16();
        Flag1           = Reader.ReadBool();
        UseUpwardVector = Reader.ReadBool();
        Unk30           = Reader.ReadInt32();
        Unk34           = Reader.ReadInt32();
        Reader.Assert<int32_t>(0);
        Reader.Assert<int32_t>(0);
    }

    void Dummy::Write(BinaryWriter& Writer, int32_t Version) const {
        Writer.WriteVector3(Position);
        if (Version == 0x20010) {
            Writer.WriteByte(Tint.B);
            Writer.WriteByte(Tint.G);
            Writer.WriteByte(Tint.R);
            Writer.WriteByte(Tint.A);
        } else {
            Writer.WriteByte(Tint.A);
            Writer.WriteByte(Tint.R);
            Writer.WriteByte(Tint.G);
            Writer.WriteByte(Tint.B);
        }
        Writer.WriteVector3(Forward);
        Writer.WriteInt16(ReferenceID);
        Writer.WriteInt16(ParentBoneIndex);
        Writer.WriteVector3(Upward);
        Writer.WriteInt16(AttachBoneIndex);
        Writer.WriteBool(Flag1);
        Writer.WriteBool(UseUpwardVector);
        Writer.WriteInt32(Unk30);
        Writer.WriteInt32(Unk34);
        Writer.WriteInt32(0);
        Writer.WriteInt32(0);
    }
}  // namespace Souls::FLVER
