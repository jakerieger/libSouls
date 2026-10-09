//
// Created by Jake Rieger on 10/8/2026.
//

#pragma once

#include <libSouls/Souls.hpp>
#include <libSouls/BinaryReader.hpp>
#include <libSouls/BinaryWriter.hpp>
#include <libSouls/Color.hpp>
#include <libSouls/Matrix.hpp>
#include <libSouls/Vector.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of exported classes; only touched by this DLL's own code

// The pieces shared by the FLVER (model) versions: bones, dummy points, and the vertex data and its layouts.
namespace Souls::FLVER {

    // A vertex color, with each channel as 0 to 1.
    struct SOULS_API VertexColor {
        float A = 0, R = 0, G = 0, B = 0;

        VertexColor() = default;
        VertexColor(float A, float R, float G, float B) : A(A), R(R), G(G), B(B) {}
        VertexColor(uint8_t A, uint8_t R, uint8_t G, uint8_t B)
            : A(A / 255.f), R(R / 255.f), G(G / 255.f), B(B / 255.f) {}

        bool operator==(const VertexColor&) const = default;
    };

#pragma region Layouts
    // How one piece of vertex data is stored. The names say how the bytes are used in the most common case, not
    // what the type means for every semantic (a Byte4A holding a normal is not the same as one holding weights).
    enum class LayoutType : uint32_t {
        Float2           = 0x01,
        Float3           = 0x02,
        Float4           = 0x03,
        Byte4A           = 0x10,
        Byte4B           = 0x11,
        Short2toFloat2   = 0x12,
        Byte4C           = 0x13,
        UV               = 0x15,
        UVPair           = 0x16,
        ShortBoneIndices = 0x18,
        Short4toFloat4A  = 0x1A,
        Short4toFloat4B  = 0x2E,
        Byte4E           = 0x2F,
        EdgeCompressed   = 0xF0,
    };

    // What a piece of vertex data is.
    enum class LayoutSemantic : uint32_t {
        Position    = 0,
        BoneWeights = 1,
        BoneIndices = 2,
        Normal      = 3,
        UV          = 5,
        Tangent     = 6,
        Bitangent   = 7,
        VertexColor = 10,
    };

    SOULS_API const char* LayoutTypeName(LayoutType Type);
    SOULS_API const char* LayoutSemanticName(LayoutSemantic Semantic);

    // One member of a vertex buffer's layout: the next piece of data in each vertex.
    struct SOULS_API LayoutMember {
        int32_t Unk00 = 0;
        LayoutType Type = LayoutType::Float3;
        LayoutSemantic Semantic = LayoutSemantic::Position;
        // Which of several with the same semantic (UV 0, UV 1, ...).
        int32_t Index = 0;

        LayoutMember() = default;
        LayoutMember(LayoutType Type, LayoutSemantic Semantic, int32_t Index = 0, int32_t Unk00 = 0)
            : Unk00(Unk00), Type(Type), Semantic(Semantic), Index(Index) {}

        // Size in bytes within a vertex. Throws for types with no defined size.
        int32_t Size() const;

        std::string ToString() const;
    };

    // The members of a vertex buffer's layout, in order.
    using LayoutMembers = std::vector<LayoutMember>;

    // Size in bytes of one vertex with this layout.
    SOULS_API int32_t LayoutSize(const LayoutMembers& Layout);
#pragma endregion

    // A vertex, with every kind of data a FLVER vertex can have. Which fields are meaningful depends on the layouts of
    // the vertex buffers it was read from.
    struct SOULS_API Vertex {
        Vector3 Position;
        std::array<float, 4> BoneWeights{};
        std::array<int32_t, 4> BoneIndices{};
        Vector3 Normal;
        // The fourth component of the normal, a whole number with a meaning that depends on the game.
        int32_t NormalW = 0;
        // UV coordinates (the Z of each is 0 unless the format has three components).
        std::vector<Vector3> UVs;
        std::vector<Vector4> Tangents;
        Vector4 Bitangent;
        std::vector<VertexColor> Colors;

        Vertex() = default;
        Vertex(size_t UVCapacity, size_t TangentCapacity, size_t ColorCapacity) {
            UVs.reserve(UVCapacity);
            Tangents.reserve(TangentCapacity);
            Colors.reserve(ColorCapacity);
        }

        // Where in the UV, tangent and color lists the next vertex buffer's members start: several buffers of one mesh
        // each take their share of the lists, in order.
        struct WriteCursor {
            size_t UV = 0, Tangent = 0, Color = 0;
        };

        void Read(BinaryReader& Reader, const LayoutMembers& Layout, float UVFactor);
        // Writes this vertex's data for one buffer and advances the cursor past what that buffer used.
        void Write(BinaryWriter& Writer, const LayoutMembers& Layout, float UVFactor, WriteCursor& Cursor) const;
    };

    // A bone of the model's skeleton.
    struct SOULS_API Bone {
        std::string Name;
        // Indices into the bone list; -1 for none.
        int16_t ParentIndex          = -1;
        int16_t ChildIndex           = -1;
        int16_t NextSiblingIndex     = -1;
        int16_t PreviousSiblingIndex = -1;
        Vector3 Translation;
        // Euler angles in radians.
        Vector3 Rotation;
        Vector3 Scale{1.f, 1.f, 1.f};
        Vector3 BoundingBoxMin;
        Vector3 BoundingBoxMax;
        int32_t Unk3C = 0;

        // The bone's transform relative to its parent: scale, then X, Z and Y rotation, then translation.
        Matrix4x4 ComputeLocalTransform() const;

        void Read(BinaryReader& Reader, bool Unicode);
        void Write(BinaryWriter& Writer, int Index) const;
        void WriteStrings(BinaryWriter& Writer, bool Unicode, int Index) const;

        std::string ToString() const { return Name; }
    };

    // A dummy point: a position (and direction) that other systems attach things to, like a weapon's hand or an effect.
    struct SOULS_API Dummy {
        Vector3 Position;
        Vector3 Forward;
        Vector3 Upward;
        int16_t ReferenceID     = 0;
        int16_t ParentBoneIndex = -1;
        int16_t AttachBoneIndex = -1;
        Color Tint;
        bool Flag1           = false;
        bool UseUpwardVector = false;
        int32_t Unk30        = 0;
        int32_t Unk34        = 0;

        void Read(BinaryReader& Reader, int32_t Version);
        void Write(BinaryWriter& Writer, int32_t Version) const;

        std::string ToString() const { return std::to_string(ReferenceID); }
    };

}  // namespace Souls::FLVER

#pragma warning(pop)
