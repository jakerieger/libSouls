/// @file
/// @author Jake Rieger
/// @date 10/8/2026

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

/// @brief The pieces shared by the FLVER (model) versions: bones, dummy points, and the vertex data and its layouts.
namespace Souls::FLVER {

    /// @brief A vertex color, with each channel as 0 to 1.
    struct SOULS_API VertexColor {
        /// @brief Alpha component of the color.
        float A = 0;
        /// @brief Red component of the color.
        float R = 0;
        /// @brief Green component of the color.
        float G = 0;
        /// @brief Blue component of the color.
        float B = 0;

        VertexColor() = default;
        /// @brief A color from float channels (0 to 1).
        VertexColor(float A, float R, float G, float B) : A(A), R(R), G(G), B(B) {}
        /// @brief A color from byte channels (0 to 255).
        VertexColor(uint8_t A, uint8_t R, uint8_t G, uint8_t B)
            : A(A / 255.f), R(R / 255.f), G(G / 255.f), B(B / 255.f) {}

        /// @brief Channel-wise equality.
        bool operator==(const VertexColor&) const = default;
    };

#pragma region Layouts
    /// @brief How one piece of vertex data is stored.
    ///
    /// The names say how the bytes are used in the most common case, not what the type means for every semantic (a
    /// Byte4A holding a normal is not the same as one holding weights).
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

    /// @brief What a piece of vertex data is.
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

    /// @brief The name of a layout type ("Float3", ...).
    SOULS_API const char* LayoutTypeName(LayoutType Type);
    /// @brief The name of a layout semantic ("Position", ...).
    SOULS_API const char* LayoutSemanticName(LayoutSemantic Semantic);

    /// @brief One member of a vertex buffer's layout: the next piece of data in each vertex.
    struct SOULS_API LayoutMember {
        /// @brief Unknown; 0, 1, or 2.
        int32_t Unk00 = 0;
        /// @brief Format used to store this member.
        LayoutType Type = LayoutType::Float3;
        /// @brief Vertex property being stored.
        LayoutSemantic Semantic = LayoutSemantic::Position;
        /// @brief Which of several with the same semantic (UV 0, UV 1, ...).
        int32_t Index = 0;

        LayoutMember() = default;
        /// @brief A member storing a semantic in a type.
        LayoutMember(LayoutType Type, LayoutSemantic Semantic, int32_t Index = 0, int32_t Unk00 = 0)
            : Unk00(Unk00), Type(Type), Semantic(Semantic), Index(Index) {}

        /// @brief Size in bytes within a vertex.
        ///
        /// Throws for types with no defined size.
        int32_t Size() const;

        /// @brief "Type: Semantic".
        std::string ToString() const;
    };

    /// @brief The members of a vertex buffer's layout, in order.
    using LayoutMembers = std::vector<LayoutMember>;

    /// @brief Size in bytes of one vertex with this layout.
    SOULS_API int32_t LayoutSize(const LayoutMembers& Layout);
#pragma endregion

    /// @brief A vertex, with every kind of data a FLVER vertex can have.
    ///
    /// Which fields are meaningful depends on the layouts of the vertex buffers it was read from.
    struct SOULS_API Vertex {
        /// @brief Where the vertex is.
        Vector3 Position;
        /// @brief Weight of the vertex's attachment to bones; must be 4 length.
        std::array<float, 4> BoneWeights{};
        /// @brief Bones the vertex is weighted to, indexing the parent mesh's bone indices; must be 4 length.
        std::array<int32_t, 4> BoneIndices{};
        /// @brief Vector pointing away from the surface.
        Vector3 Normal;
        /// @brief The fourth component of the normal, a whole number with a meaning that depends on the game.
        int32_t NormalW = 0;
        /// @brief UV coordinates (the Z of each is 0 unless the format has three components).
        std::vector<Vector3> UVs;
        /// @brief Vector pointing perpendicular to the normal.
        std::vector<Vector4> Tangents;
        /// @brief Vector pointing perpendicular to the normal and tangent.
        Vector4 Bitangent;
        /// @brief Data used for alpha, blending, etc.
        std::vector<VertexColor> Colors;

        Vertex() = default;
        /// @brief A vertex with room reserved for the given numbers of UVs, tangents and colors.
        Vertex(size_t UVCapacity, size_t TangentCapacity, size_t ColorCapacity) {
            UVs.reserve(UVCapacity);
            Tangents.reserve(TangentCapacity);
            Colors.reserve(ColorCapacity);
        }

        /// @brief Where in the UV, tangent and color lists the next vertex buffer's members start: several buffers of
        ///        one mesh each take their share of the lists, in order.
        struct WriteCursor {
            /// @brief The next UV to write.
            size_t UV = 0;
            /// @brief The next tangent to write.
            size_t Tangent = 0;
            /// @brief The next color to write.
            size_t Color = 0;
        };

        /// @brief Reads this vertex's data for one buffer, appending to the UV, tangent and color lists.
        void Read(BinaryReader& Reader, const LayoutMembers& Layout, float UVFactor);
        /// @brief Writes this vertex's data for one buffer and advances the cursor past what that buffer used.
        void Write(BinaryWriter& Writer, const LayoutMembers& Layout, float UVFactor, WriteCursor& Cursor) const;
    };

    /// @brief A bone of the model's skeleton.
    struct SOULS_API Bone {
        /// @brief Corresponds to the name of a bone in the parent skeleton, if present.
        std::string Name;
        /// @brief Indices into the bone list; -1 for none.
        int16_t ParentIndex          = -1;
        /// @brief Index of the first child in this FLVER's bone collection, or -1 for none.
        int16_t ChildIndex           = -1;
        /// @brief Index of the next child of this bone's parent, or -1 for none.
        int16_t NextSiblingIndex     = -1;
        /// @brief Index of the previous child of this bone's parent, or -1 for none.
        int16_t PreviousSiblingIndex = -1;
        /// @brief Translation of this bone.
        Vector3 Translation;
        /// @brief Euler angles in radians.
        Vector3 Rotation;
        /// @brief Scale of this bone.
        Vector3 Scale{1.f, 1.f, 1.f};
        /// @brief Minimum extent of the vertices weighted to this bone.
        Vector3 BoundingBoxMin;
        /// @brief Maximum extent of the vertices weighted to this bone.
        Vector3 BoundingBoxMax;
        /// @brief Unknown; only 0 or 1 before Sekiro.
        int32_t Unk3C = 0;

        /// @brief The bone's transform relative to its parent: scale, then X, Z and Y rotation, then translation.
        Matrix4x4 ComputeLocalTransform() const;

        /// @brief Reads the bone.
        void Read(BinaryReader& Reader, bool Unicode);
        /// @brief Writes the bone, reserving its name offset.
        void Write(BinaryWriter& Writer, int Index) const;
        /// @brief Writes the bone's name and fills its offset.
        void WriteStrings(BinaryWriter& Writer, bool Unicode, int Index) const;

        /// @brief The bone's name.
        std::string ToString() const { return Name; }
    };

    /// @brief A dummy point: a position (and direction) that other systems attach things to, like a weapon's hand or an
    ///        effect.
    struct SOULS_API Dummy {
        /// @brief Location of the dummy point.
        Vector3 Position;
        /// @brief Vector indicating the dummy point's forward direction.
        Vector3 Forward;
        /// @brief Vector indicating the dummy point's upward direction.
        Vector3 Upward;
        /// @brief Indicates the type of dummy point this is (hitbox, sfx, etc).
        int16_t ReferenceID     = 0;
        /// @brief Index of a bone that the dummy point is initially transformed to before binding to the attach bone.
        int16_t ParentBoneIndex = -1;
        /// @brief Index of the bone that the dummy point follows physically.
        int16_t AttachBoneIndex = -1;
        /// @brief Unknown; only used in Sekiro.
        Color Tint;
        /// @brief Unknown.
        bool Flag1           = false;
        /// @brief If false, the upward vector is not read.
        bool UseUpwardVector = false;
        /// @brief Unknown; only used in Sekiro.
        int32_t Unk30        = 0;
        /// @brief Unknown; only used in Sekiro.
        int32_t Unk34        = 0;

        /// @brief Reads the dummy point; the FLVER version decides the layout.
        void Read(BinaryReader& Reader, int32_t Version);
        /// @brief Writes the dummy point; the FLVER version decides the layout.
        void Write(BinaryWriter& Writer, int32_t Version) const;

        /// @brief The reference ID.
        std::string ToString() const { return std::to_string(ReferenceID); }
    };

}  // namespace Souls::FLVER

#pragma warning(pop)
