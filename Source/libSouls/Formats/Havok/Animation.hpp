// Copyright (C) 2026 Jake Rieger
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, version 3. This program is distributed in the hope that it will
// be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU General
// Public License along with this program. If not, see https://www.gnu.org/licenses/.

#pragma once

#include <libSouls/Formats/Havok/Tagfile.hpp>
#include <libSouls/Souls.hpp>
#include <libSouls/Matrix.hpp>
#include <libSouls/Vector.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Souls::Havok {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of exported classes; only touched by this DLL's own code

    class Animation;

    /// @brief A bone's transform relative to its parent: translation, rotation (a unit quaternion, X Y Z W) and scale.
    ///
    /// Composition follows Havok's: a parent's scale applies to its children's scale but not to their translation.
    struct SOULS_API QsTransform {
        Vector3 Translation;                    ///< The translation.
        Vector4 Rotation {0.f, 0.f, 0.f, 1.f};  ///< The rotation, as a unit quaternion (X, Y, Z, W).
        Vector3 Scale {1.f, 1.f, 1.f};          ///< The scale.

        /// @brief Parent * Child: the child's transform moved into the parent's space.
        NODISCARD static QsTransform Multiply(const QsTransform& Parent, const QsTransform& Child);
        /// @brief Interpolates translation and scale linearly and rotation along the shorter arc (normalized lerp).
        NODISCARD static QsTransform Blend(const QsTransform& A, const QsTransform& B, float T);
        /// @brief The transform as a 4x4 matrix for row vectors (v * M, translation in the last row), the convention
        ///        DirectXMath uses.
        NODISCARD Matrix4x4 ToMatrix() const;
    };

    /// @brief A skeleton (hkaSkeleton): its bones, their hierarchy and their reference pose.
    struct SOULS_API Skeleton {
        /// @brief The skeleton's name.
        std::string Name;
        /// @brief Each bone's name.
        std::vector<std::string> BoneNames;
        /// @brief Each bone's parent bone, or -1 for a root; a parent always comes before its children.
        std::vector<int16_t> Parents;
        /// @brief Each bone's local transform in the bind pose.
        std::vector<QsTransform> ReferencePose;

        /// @brief Reads an hkaSkeleton object; throws BinaryException if it isn't one.
        static Skeleton FromValue(const Value& Object);
        /// @brief The index of the bone with this name, or -1.
        NODISCARD int FindBone(std::string_view Name) const;
        /// @brief Local transforms (one per bone) to model space, by composing each bone with its parents.
        ///
        /// Throws std::invalid_argument if Local doesn't hold one transform per bone.
        NODISCARD std::vector<QsTransform> ModelSpace(std::span<const QsTransform> Local) const;
    };

    /// @brief Which animation tracks drive which skeleton bones (hkaAnimationBinding).
    struct SOULS_API AnimationBinding {
        /// @brief How the animation combines with others.
        enum class Blend : int8_t {
            Normal             = 0,  ///< The tracks are the bones' local transforms.
            AdditiveDeprecated = 1,  ///< Offsets in Havok's older convention (the games' additive animations).
            Additive           = 2,  ///< Offsets composed on top of another pose.
        };

        /// @brief The name of the skeleton the animation was made for.
        std::string SkeletonName;
        /// @brief For each transform track of the animation, the bone it drives.
        std::vector<int16_t> TrackToBone;
        /// @brief For each float track of the animation, the float slot it drives.
        std::vector<int16_t> FloatTrackToSlot;
        /// @brief How the animation combines with others.
        Blend BlendHint = Blend::Normal;

        /// @brief Reads an hkaAnimationBinding object; throws BinaryException if it isn't one.
        static AnimationBinding FromValue(const Value& Object);

        /// @brief Poses the bones an animation drives at a time in seconds, in a pose of local transforms (one per
        ///        bone, such as a copy of Skeleton::ReferencePose).
        ///
        /// A normal animation replaces the transforms of the bones it drives. An additive one is layered on top of
        /// them: for AdditiveDeprecated, translations add and rotations and scales multiply (base * offset); for
        /// Additive, each offset is composed as QsTransform::Multiply(base, offset). Bones the animation doesn't drive
        /// keep their transforms, and tracks bound to bones outside the pose are ignored.
        void Apply(const Animation& Animation, float Time, std::span<QsTransform> Pose) const;
    };

    /// @brief Root motion (hkaDefaultAnimatedReferenceFrame): how the whole character moves over an animation.
    struct SOULS_API ReferenceFrame {
        /// @brief The up axis.
        Vector4 Up;
        /// @brief The forward axis.
        Vector4 Forward;
        /// @brief The length of the motion, in seconds.
        float Duration = 0.f;
        /// @brief Evenly spaced samples over the duration: X, Y, Z translation and W the rotation about Up, in radians.
        std::vector<Vector4> Samples;

        /// @brief Reads an hkaDefaultAnimatedReferenceFrame object; throws BinaryException if it isn't one.
        static ReferenceFrame FromValue(const Value& Object);
        /// @brief The motion at a time in seconds, interpolated between samples and clamped to the duration.
        NODISCARD Vector4 Sample(float Time) const;
    };

    /// @brief A text annotation at a point in an animation.
    struct Annotation {
        float Time = 0.f;  ///< When, in seconds.
        std::string Text;  ///< The annotation.
    };

    /// @brief The annotations of one track (hkaAnnotationTrack), named after the bone they belong to.
    struct AnnotationTrack {
        std::string Name;                     ///< The track's name.
        std::vector<Annotation> Annotations;  ///< The annotations, in time order.
    };

    /// @brief An animation, decoded so it can be sampled at any time.
    ///
    /// Reads hkaSplineCompressedAnimation, which nearly every animation in the games is, and
    /// hkaInterleavedUncompressedAnimation. A spline-compressed animation is split into blocks of frames; within a
    /// block, each component of each track is either constant or a B-spline whose control points are stored quantized.
    /// Reading decodes the control points once, and sampling evaluates the splines.
    ///
    /// @code
    /// Havok::Tagfile File             = Havok::Tagfile::Read(Bytes, &Compendium);
    /// Havok::Animation Animation      = Havok::Animation::FromValue(File.ObjectsOfType("hkaAnimation")[0]);
    /// Havok::AnimationBinding Binding =
    /// Havok::AnimationBinding::FromValue(File.ObjectsOfType("hkaAnimationBinding")[0]);
    /// std::vector<Havok::QsTransform> Pose = Skeleton.ReferencePose;
    /// Binding.Apply(Animation, 0.5f, Pose);  // half a second in
    /// std::vector<Havok::QsTransform> World = Skeleton.ModelSpace(Pose);
    /// @endcode
    class SOULS_API Animation {
    public:
        /// @brief How an animation is stored.
        enum class Compression : uint8_t {
            Spline,       ///< hkaSplineCompressedAnimation
            Interleaved,  ///< hkaInterleavedUncompressedAnimation
        };

        /// @brief How the animation was stored.
        Compression Storage = Compression::Spline;
        /// @brief The length of the animation, in seconds.
        float Duration = 0.f;
        /// @brief How many frames the animation has.
        uint32_t FrameCount = 0;
        /// @brief The time between frames, in seconds.
        float FrameDuration = 0.f;
        /// @brief Spline animations: how many frames each block covers (neighbouring blocks share their boundary
        /// frame).
        uint32_t FramesPerBlock = 0;
        /// @brief How many transform tracks the animation has (see AnimationBinding::TrackToBone).
        uint32_t TrackCount = 0;
        /// @brief How many float tracks the animation has (not decoded).
        uint32_t FloatTrackCount = 0;
        /// @brief The animation's root motion, if it has any.
        std::optional<ReferenceFrame> RootMotion;
        /// @brief The annotation tracks (empty in most animations).
        std::vector<AnnotationTrack> Annotations;

        /// @brief Reads and decodes an animation object.
        ///
        /// Throws BinaryException if it isn't a supported kind of animation, or if it uses a quantization this decoder
        /// doesn't support.
        static Animation FromValue(const Value& Object);

        /// @brief Every track's transform at a time in seconds (clamped to the animation).
        NODISCARD std::vector<QsTransform> Sample(float Time) const;
        /// @brief Every track's transform at a frame (fractional frames interpolate; clamped to the animation).
        ///
        /// Out must hold TrackCount transforms.
        void SampleFrame(float Frame, std::span<QsTransform> Out) const;
        /// @brief Every track at every frame: FrameCount * TrackCount transforms, frame by frame.
        NODISCARD std::vector<QsTransform> Bake() const;

        /// @brief An animation from baked frames (FrameCount * TrackCount transforms, frame by frame), such as Bake's
        ///        result after editing.
        ///
        /// Throws std::invalid_argument if the frames don't fill whole frames of TrackCount tracks.
        static Animation FromFrames(std::vector<QsTransform> Frames, uint32_t TrackCount, float FrameDuration);

        /// @brief Writes the animation into an animation object of a tagfile, replacing what it held.
        ///
        /// An hkaSplineCompressedAnimation gets the frames re-encoded as linear splines: 16-bit translations and
        /// scales over each track's range, 48-bit rotations, and constant tracks stored once. An
        /// hkaInterleavedUncompressedAnimation gets the frames as they are. The duration, frame count and root
        /// motion samples (if both the animation and the object have root motion) are updated too; the track count
        /// must match the object's, since its binding maps tracks to bones. Values of the tagfile taken before the
        /// call are stale afterwards. Throws BinaryException if the object isn't one of those types or the track
        /// counts differ.
        void WriteTo(Tagfile& File, const Value& Object) const;

    private:
        struct Curve {
            uint8_t Degree = 0;
            std::vector<uint8_t> Knots;  ///< empty for a constant
            NODISCARD int Span(float Frame, int PointCount) const;
            void Basis(int Span, float Frame, float* N) const;
        };
        struct VectorTrack : Curve {
            std::vector<float> Points[3];  ///< per component; one point for a constant one
            NODISCARD Vector3 Evaluate(float Frame) const;
        };
        struct QuatTrack : Curve {
            std::vector<Vector4> Points;  ///< one point for a constant rotation
            NODISCARD Vector4 Evaluate(float Frame) const;
        };
        struct Track {
            VectorTrack Translation;
            QuatTrack Rotation;
            VectorTrack Scale;
        };
        struct Block {
            std::vector<Track> Tracks;
        };

        std::vector<Block> _Blocks;        ///< spline
        std::vector<QsTransform> _Frames;  ///< interleaved: FrameCount * TrackCount

        static void ReadSpline(const Value& Object, Animation& Result);
        void SampleSpline(float Frame, std::span<QsTransform> Out) const;
    };

#pragma warning(pop)

}  // namespace Souls::Havok
