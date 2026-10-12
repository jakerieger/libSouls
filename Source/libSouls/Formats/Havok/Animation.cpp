// Copyright (C) 2026 Jake Rieger
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, version 3. This program is distributed in the hope that it will
// be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU General
// Public License along with this program. If not, see https://www.gnu.org/licenses/.

// The spline decoding follows HavokLib by Lukas Cone (GPL-3, https://github.com/PredatorCZ/HavokLib).

#include "Animation.hpp"

#include <libSouls/Endian.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace Souls::Havok {
    namespace {
        // hkVector4f is a fixed array of 4 floats; hkQuaternionf wraps one in "vec".
        Vector4 ToVector4(const Value& V) {
            const std::vector<float> F = V.Has("vec") ? V["vec"].AsFloats() : V.AsFloats();
            if (F.size() < 4) throw BinaryException("Havok vector has fewer than 4 components");
            return {F[0], F[1], F[2], F[3]};
        }

        Vector3 ToVector3(const Value& V) {
            const Vector4 F = ToVector4(V);
            return {F.X, F.Y, F.Z};
        }

        std::vector<int16_t> ToInt16s(const Value& V) {
            std::vector<int16_t> Result;
            for (const int64_t I : V.AsInts())
                Result.push_back(static_cast<int16_t>(I));
            return Result;
        }

        void Expect(const Value& Object, std::string_view Type) {
            if (!Object.IsA(Type))
                throw BinaryException("Havok object isn't an " + std::string(Type));
        }

        // A bounds-checked cursor over an animation's data. Alignment is relative to the start of the data, which
        // Havok allocates 16-byte aligned.
        class Cursor {
        public:
            Cursor(std::span<const uint8_t> Data, size_t Pos) : Data(Data), Pos(Pos) {}

            template<typename T>
            T Read() {
                Need(sizeof(T));
                T Result;
                std::memcpy(&Result, Data.data() + Pos, sizeof(T));
                Pos += sizeof(T);
                return Result;
            }

            std::span<const uint8_t> Bytes(size_t Count) {
                Need(Count);
                const auto Result = Data.subspan(Pos, Count);
                Pos += Count;
                return Result;
            }

            void Align(size_t Alignment) { Pos = (Pos + Alignment - 1) & ~(Alignment - 1); }

        private:
            std::span<const uint8_t> Data;
            size_t Pos;

            void Need(size_t Count) const {
                if (Pos + Count > Data.size()) throw BinaryException("Havok spline data ends early");
            }
        };

        // How each component of a track is stored in a block.
        enum class Storage { Identity, Static, Spline };

        // A track's mask: quantization of each channel, then which components are static or splines.
        struct Mask {
            uint8_t Quantization = 0;
            uint8_t Translation  = 0;  // bits 0-2: X Y Z static; bits 4-6: X Y Z spline
            uint8_t Rotation     = 0;  // low nibble: static; high nibble: spline
            uint8_t Scale        = 0;  // as Translation

            static Storage Component(uint8_t Bits, int Index) {
                if (Bits & (1 << Index)) return Storage::Static;
                if (Bits & (0x10 << Index)) return Storage::Spline;
                return Storage::Identity;
            }

            Storage RotationStorage() const {
                if (Rotation & 0xF0) return Storage::Spline;
                if (Rotation & 0x0F) return Storage::Static;
                return Storage::Identity;
            }

            int TranslationQuantization() const { return Quantization & 3; }  // 0: 8-bit, 1: 16-bit
            int RotationQuantization() const { return (Quantization >> 2) & 0xF; }
            int ScaleQuantization() const { return (Quantization >> 6) & 3; }
        };

        // Rotation quantizations (Mask::RotationQuantization).
        enum : int { Quat32 = 0, Quat40 = 1, Quat48 = 2, Quat24 = 3, Quat16 = 4, QuatFloat = 5 };

        // The largest component is dropped and rebuilt from the others; Shift says where it goes.
        Vector4 Rebuild(float A, float B, float C, int Shift, bool Negative) {
            float Missing = std::sqrt(std::max(0.f, 1.f - A * A - B * B - C * C));
            if (Negative) Missing = -Missing;
            switch (Shift) {
                case 0: return {Missing, A, B, C};
                case 1: return {A, Missing, B, C};
                case 2: return {A, B, Missing, C};
                default: return {A, B, C, Missing};
            }
        }

        Vector4 ReadQuat(Cursor& In, int Quantization) {
            switch (Quantization) {
                case Quat32: {
                    // Spherical coordinates: phi and theta packed in 18 bits, then a 10-bit radius and 4 sign bits.
                    constexpr float HalfPi = 1.5707963f;
                    const uint32_t V       = In.Read<uint32_t>();
                    float R                = static_cast<float>((V >> 18) & 0x3FF) / 1023.f;
                    R                      = 1.f - R * R;
                    const float PhiTheta   = static_cast<float>(V & 0x3FFFF);
                    float Phi              = std::floor(std::sqrt(PhiTheta));
                    float Theta            = 0.f;
                    if (Phi > 0.f) {
                        Theta = HalfPi * 0.5f * (PhiTheta - Phi * Phi) / Phi;
                        Phi   = HalfPi / 511.f * Phi;
                    }
                    const float Magnitude = std::sqrt(std::max(0.f, 1.f - R * R));
                    Vector4 Q {std::sin(Phi) * std::cos(Theta) * Magnitude,
                               std::sin(Phi) * std::sin(Theta) * Magnitude,
                               std::cos(Phi) * Magnitude,
                               R};
                    if (V & 0x10000000) Q.X = -Q.X;
                    if (V & 0x20000000) Q.Y = -Q.Y;
                    if (V & 0x40000000) Q.Z = -Q.Z;
                    if (V & 0x80000000) Q.W = -Q.W;
                    return Q;
                }
                case Quat40: {
                    // Three 12-bit components, a 2-bit shift and a sign bit.
                    const auto B   = In.Bytes(5);
                    uint64_t V     = 0;
                    for (int I = 0; I < 5; ++I)
                        V |= uint64_t(B[I]) << (8 * I);
                    constexpr float Scale = 0.000345436f;  // 1 / (2047 * sqrt(2))
                    auto Component        = [&](int Index) {
                        return static_cast<float>(static_cast<int>((V >> (12 * Index)) & 0xFFF) - 2047) * Scale;
                    };
                    return Rebuild(Component(0), Component(1), Component(2), static_cast<int>((V >> 36) & 3), ((V >> 38) & 1) != 0);
                }
                case Quat48: {
                    // Three 15-bit components; the top bits of the first two are the shift, of the third the sign.
                    const uint16_t X      = In.Read<uint16_t>();
                    const uint16_t Y      = In.Read<uint16_t>();
                    const uint16_t Z      = In.Read<uint16_t>();
                    constexpr float Scale = 0.000043161f;  // 1 / (16383 * sqrt(2))
                    auto Component        = [&](uint16_t C) { return static_cast<float>(int(C & 0x7FFF) - 16383) * Scale; };
                    const int Shift       = ((Y >> 14) & 2) | (X >> 15);
                    return Rebuild(Component(X), Component(Y), Component(Z), Shift, (Z >> 15) != 0);
                }
                case QuatFloat: {
                    Vector4 Q;
                    Q.X = In.Read<float>();
                    Q.Y = In.Read<float>();
                    Q.Z = In.Read<float>();
                    Q.W = In.Read<float>();
                    return Q;
                }
                default:
                    throw BinaryException("Unsupported Havok rotation quantization " + std::to_string(Quantization));
            }
        }

        Vector4 Normalized(Vector4 Q) {
            const float Length = std::sqrt(Q.X * Q.X + Q.Y * Q.Y + Q.Z * Q.Z + Q.W * Q.W);
            if (Length <= 0.f) return {0.f, 0.f, 0.f, 1.f};
            return {Q.X / Length, Q.Y / Length, Q.Z / Length, Q.W / Length};
        }
    }  // namespace

#pragma region Skeleton, binding, root motion
    Skeleton Skeleton::FromValue(const Value& Object) {
        Expect(Object, "hkaSkeleton");
        Skeleton Result;
        Result.Name          = Object["name"].AsString();
        const Value Bones    = Object["bones"];
        const Value Pose     = Object["referencePose"];
        Result.Parents       = ToInt16s(Object["parentIndices"]);
        const size_t Count   = Bones.Size();
        if (Result.Parents.size() != Count || Pose.Size() != Count)
            throw BinaryException("Havok skeleton's bones, parents and pose don't match");
        for (size_t I = 0; I < Count; ++I) {
            Result.BoneNames.push_back(Bones.At(I)["name"].AsString());
            const Value T = Pose.At(I);
            Result.ReferencePose.push_back({ToVector3(T["translation"]), ToVector4(T["rotation"]), ToVector3(T["scale"])});
        }
        return Result;
    }

    AnimationBinding AnimationBinding::FromValue(const Value& Object) {
        Expect(Object, "hkaAnimationBinding");
        AnimationBinding Result;
        Result.SkeletonName     = Object["originalSkeletonName"].AsString();
        Result.TrackToBone      = ToInt16s(Object["transformTrackToBoneIndices"]);
        Result.FloatTrackToSlot = ToInt16s(Object["floatTrackToFloatSlotIndices"]);
        Result.BlendHint        = static_cast<Blend>(Object["blendHint"].AsInt());
        return Result;
    }

    ReferenceFrame ReferenceFrame::FromValue(const Value& Object) {
        Expect(Object, "hkaDefaultAnimatedReferenceFrame");
        ReferenceFrame Result;
        Result.Up             = ToVector4(Object["up"]);
        Result.Forward        = ToVector4(Object["forward"]);
        Result.Duration       = static_cast<float>(Object["duration"].AsFloat());
        const Value Samples   = Object["referenceFrameSamples"];
        for (size_t I = 0; I < Samples.Size(); ++I)
            Result.Samples.push_back(ToVector4(Samples.At(I)));
        return Result;
    }

    Vector4 ReferenceFrame::Sample(float Time) const {
        if (Samples.empty()) return {};
        if (Samples.size() == 1 || Duration <= 0.f) return Samples.front();
        const float Position = std::clamp(Time / Duration, 0.f, 1.f) * static_cast<float>(Samples.size() - 1);
        const size_t Index   = std::min(static_cast<size_t>(Position), Samples.size() - 2);
        const float T        = Position - static_cast<float>(Index);
        const Vector4& A     = Samples[Index];
        const Vector4& B     = Samples[Index + 1];
        return {A.X + (B.X - A.X) * T, A.Y + (B.Y - A.Y) * T, A.Z + (B.Z - A.Z) * T, A.W + (B.W - A.W) * T};
    }
#pragma endregion

#pragma region Spline evaluation
    // Algorithm A2.1 of The NURBS Book (2nd edition): the knot span a frame falls in.
    int Animation::Curve::Span(float Frame, int PointCount) const {
        if (Frame >= Knots[PointCount]) return PointCount - 1;
        int Low  = Degree;
        int High = PointCount;
        int Mid  = (Low + High) / 2;
        while (Frame < Knots[Mid] || Frame >= Knots[Mid + 1]) {
            if (Frame < Knots[Mid]) High = Mid;
            else Low = Mid;
            Mid = (Low + High) / 2;
        }
        return Mid;
    }

    // The nonzero basis functions at a frame (N[0] weighs the point at Span, N[I] the one at Span - I).
    void Animation::Curve::Basis(int Span, float Frame, float* N) const {
        N[0] = 1.f;
        for (int I = 1; I <= Degree; ++I)
            N[I] = 0.f;
        for (int I = 1; I <= Degree; ++I) {
            for (int J = I - 1; J >= 0; --J) {
                const float Left  = Knots[Span - J];
                const float Right = Knots[Span + I - J];
                const float A     = Right > Left ? (Frame - Left) / (Right - Left) : 0.f;
                const float Tmp   = N[J] * A;
                N[J + 1] += N[J] - Tmp;
                N[J] = Tmp;
            }
        }
    }

    Vector3 Animation::VectorTrack::Evaluate(float Frame) const {
        float Result[3];
        int SpanIndex = -1;
        float N[16];
        for (int C = 0; C < 3; ++C) {
            const auto& P = Points[C];
            if (P.size() == 1) {
                Result[C] = P[0];
                continue;
            }
            if (SpanIndex < 0) {
                SpanIndex = Span(Frame, static_cast<int>(P.size()));
                Basis(SpanIndex, Frame, N);
            }
            float Sum = 0.f;
            for (int I = 0; I <= Degree; ++I)
                Sum += P[SpanIndex - I] * N[I];
            Result[C] = Sum;
        }
        return {Result[0], Result[1], Result[2]};
    }

    Vector4 Animation::QuatTrack::Evaluate(float Frame) const {
        if (Points.size() == 1) return Points[0];
        const int SpanIndex = Span(Frame, static_cast<int>(Points.size()));
        float N[16];
        Basis(SpanIndex, Frame, N);
        Vector4 Sum;
        for (int I = 0; I <= Degree; ++I) {
            const Vector4& P = Points[SpanIndex - I];
            Sum.X += P.X * N[I];
            Sum.Y += P.Y * N[I];
            Sum.Z += P.Z * N[I];
            Sum.W += P.W * N[I];
        }
        return Normalized(Sum);
    }
#pragma endregion

#pragma region Decoding
    namespace {
        // A curve's header: the last control point's index, the degree, then the knots.
        template<typename CurveT>
        size_t ReadCurveHeader(Cursor& In, CurveT& Curve) {
            const uint16_t Last = In.Read<uint16_t>();
            Curve.Degree        = In.Read<uint8_t>();
            if (Curve.Degree == 0 || Curve.Degree > 15) throw BinaryException("Unsupported Havok spline degree");
            const auto Knots = In.Bytes(size_t(Last) + Curve.Degree + 2);
            Curve.Knots.assign(Knots.begin(), Knots.end());
            return size_t(Last) + 1;
        }

        // A translation or scale track: constant components, or splines of quantized control points.
        template<typename TrackT>
        void ReadVectorTrack(Cursor& In, TrackT& Track, uint8_t Bits, int Quantization, float Default) {
            bool AnySpline = false;
            for (int C = 0; C < 3; ++C)
                AnySpline |= Mask::Component(Bits, C) == Storage::Spline;

            if (!AnySpline) {
                for (int C = 0; C < 3; ++C)
                    Track.Points[C] = {Mask::Component(Bits, C) == Storage::Static ? In.Read<float>() : Default};
                return;
            }

            const size_t Count = ReadCurveHeader(In, Track);
            In.Align(4);
            float Min[3] {}, Max[3] {};
            for (int C = 0; C < 3; ++C) {
                switch (Mask::Component(Bits, C)) {
                    case Storage::Spline:
                        Min[C] = In.Read<float>();
                        Max[C] = In.Read<float>();
                        Track.Points[C].resize(Count);
                        break;
                    case Storage::Static: Track.Points[C] = {In.Read<float>()}; break;
                    case Storage::Identity: Track.Points[C] = {Default}; break;
                }
            }
            for (size_t P = 0; P < Count; ++P) {
                for (int C = 0; C < 3; ++C) {
                    if (Mask::Component(Bits, C) != Storage::Spline) continue;
                    const float T = Quantization == 0 ? static_cast<float>(In.Read<uint8_t>()) / 255.f
                                                      : static_cast<float>(In.Read<uint16_t>()) / 65535.f;
                    Track.Points[C][P] = Min[C] + (Max[C] - Min[C]) * T;
                }
            }
            In.Align(4);
        }
    }  // namespace

    void Animation::ReadSpline(const Value& Object, Animation& Result) {
        Result.FramesPerBlock = static_cast<uint32_t>(Object["maxFramesPerBlock"].AsInt());

        const std::span<const uint8_t> Data = Object["data"].AsBytes();
        const std::vector<int64_t> Offsets  = Object["blockOffsets"].AsInts();
        if (Offsets.size() != static_cast<size_t>(Object["numBlocks"].AsInt()))
            throw BinaryException("Havok spline animation's block count doesn't match its offsets");

        for (const int64_t Offset : Offsets) {
            Cursor In(Data, static_cast<size_t>(Offset));
            std::vector<Mask> Masks(Result.TrackCount);
            for (Mask& M : Masks) {
                M.Quantization = In.Read<uint8_t>();
                M.Translation  = In.Read<uint8_t>();
                M.Rotation     = In.Read<uint8_t>();
                M.Scale        = In.Read<uint8_t>();
            }
            In.Bytes(Result.FloatTrackCount);  // the float tracks' masks
            In.Align(4);

            Block& B = Result.Blocks.emplace_back();
            B.Tracks.resize(Result.TrackCount);
            for (size_t T = 0; T < Masks.size(); ++T) {
                const Mask& M = Masks[T];
                Track& Out    = B.Tracks[T];
                ReadVectorTrack(In, Out.Translation, M.Translation, M.TranslationQuantization(), 0.f);

                const int Quantization = M.RotationQuantization();
                switch (M.RotationStorage()) {
                    case Storage::Spline: {
                        const size_t Count = ReadCurveHeader(In, Out.Rotation);
                        if (Quantization == Quat48 || Quantization == Quat16) In.Align(2);
                        else if (Quantization == Quat32 || Quantization == QuatFloat) In.Align(4);
                        for (size_t P = 0; P < Count; ++P)
                            Out.Rotation.Points.push_back(ReadQuat(In, Quantization));
                        break;
                    }
                    case Storage::Static: Out.Rotation.Points = {ReadQuat(In, Quantization)}; break;
                    case Storage::Identity: Out.Rotation.Points = {Vector4 {0.f, 0.f, 0.f, 1.f}}; break;
                }
                In.Align(4);

                ReadVectorTrack(In, Out.Scale, M.Scale, M.ScaleQuantization(), 1.f);
            }
        }
    }

    Animation Animation::FromValue(const Value& Object) {
        Expect(Object, "hkaAnimation");
        Animation Result;
        Result.Duration        = static_cast<float>(Object["duration"].AsFloat());
        Result.TrackCount      = static_cast<uint32_t>(Object["numberOfTransformTracks"].AsInt());
        Result.FloatTrackCount = static_cast<uint32_t>(Object["numberOfFloatTracks"].AsInt());
        if (const Value Motion = Object["extractedMotion"].Deref(); Motion.IsValid())
            Result.RootMotion = ReferenceFrame::FromValue(Motion);

        const Value Tracks = Object["annotationTracks"];
        for (size_t I = 0; I < Tracks.Size(); ++I) {
            const Value Track     = Tracks.At(I);
            AnnotationTrack& Into = Result.Annotations.emplace_back();
            Into.Name             = Track["trackName"].AsString();
            const Value List      = Track["annotations"];
            for (size_t J = 0; J < List.Size(); ++J)
                Into.Annotations.push_back(
                  {static_cast<float>(List.At(J)["time"].AsFloat()), List.At(J)["text"].AsString()});
        }

        if (Object.IsA("hkaSplineCompressedAnimation")) {
            Result.Storage       = Compression::Spline;
            Result.FrameCount    = static_cast<uint32_t>(Object["numFrames"].AsInt());
            Result.FrameDuration = static_cast<float>(Object["frameDuration"].AsFloat());
            ReadSpline(Object, Result);
        } else if (Object.IsA("hkaInterleavedUncompressedAnimation")) {
            Result.Storage         = Compression::Interleaved;
            const Value Transforms = Object["transforms"];
            if (Result.TrackCount == 0 || Transforms.Size() % Result.TrackCount != 0)
                throw BinaryException("Havok interleaved animation's transforms don't fill its frames");
            Result.FrameCount    = static_cast<uint32_t>(Transforms.Size() / Result.TrackCount);
            Result.FrameDuration = Result.FrameCount > 1 ? Result.Duration / static_cast<float>(Result.FrameCount - 1)
                                                         : Result.Duration;
            Result.Frames.reserve(Transforms.Size());
            for (size_t I = 0; I < Transforms.Size(); ++I) {
                const Value T = Transforms.At(I);
                Result.Frames.push_back({ToVector3(T["translation"]), ToVector4(T["rotation"]), ToVector3(T["scale"])});
            }
        } else {
            throw BinaryException("Unsupported Havok animation type " + Object.TypeName());
        }
        return Result;
    }
#pragma endregion

#pragma region Sampling
    void Animation::SampleSpline(float Frame, std::span<QsTransform> Out) const {
        // Neighbouring blocks share their boundary frame, so each block starts FramesPerBlock - 1 frames after the last.
        const float Stride = static_cast<float>(FramesPerBlock > 1 ? FramesPerBlock - 1 : 1);
        const size_t Index = std::min(static_cast<size_t>(Frame / Stride), Blocks.size() - 1);
        const float Local  = Frame - static_cast<float>(Index) * Stride;
        const Block& B     = Blocks[Index];
        for (size_t T = 0; T < TrackCount; ++T) {
            Out[T].Translation = B.Tracks[T].Translation.Evaluate(Local);
            Out[T].Rotation    = B.Tracks[T].Rotation.Evaluate(Local);
            Out[T].Scale       = B.Tracks[T].Scale.Evaluate(Local);
        }
    }

    void Animation::SampleFrame(float Frame, std::span<QsTransform> Out) const {
        if (Out.size() < TrackCount) throw std::invalid_argument("SampleFrame needs one transform per track");
        if (FrameCount == 0) return;
        Frame = std::clamp(Frame, 0.f, static_cast<float>(FrameCount - 1));
        if (Storage == Compression::Spline) {
            if (!Blocks.empty()) SampleSpline(Frame, Out);
            return;
        }
        const size_t Index = std::min(static_cast<size_t>(Frame), static_cast<size_t>(FrameCount - 1));
        const size_t Next  = std::min<size_t>(Index + 1, FrameCount - 1);
        const float T      = Frame - static_cast<float>(Index);
        for (size_t Track = 0; Track < TrackCount; ++Track)
            Out[Track] = QsTransform::Blend(Frames[Index * TrackCount + Track], Frames[Next * TrackCount + Track], T);
    }

    std::vector<QsTransform> Animation::Sample(float Time) const {
        std::vector<QsTransform> Result(TrackCount);
        SampleFrame(FrameDuration > 0.f ? Time / FrameDuration : 0.f, Result);
        return Result;
    }

    std::vector<QsTransform> Animation::Bake() const {
        if (Storage == Compression::Interleaved) return Frames;
        std::vector<QsTransform> Result(size_t(FrameCount) * TrackCount);
        for (uint32_t Frame = 0; Frame < FrameCount; ++Frame)
            SampleFrame(static_cast<float>(Frame), std::span(Result).subspan(size_t(Frame) * TrackCount, TrackCount));
        return Result;
    }
#pragma endregion

#pragma region Poses
    namespace {
        Vector4 QuatMultiply(const Vector4& A, const Vector4& B) {
            return {A.W * B.X + A.X * B.W + A.Y * B.Z - A.Z * B.Y,
                    A.W * B.Y - A.X * B.Z + A.Y * B.W + A.Z * B.X,
                    A.W * B.Z + A.X * B.Y - A.Y * B.X + A.Z * B.W,
                    A.W * B.W - A.X * B.X - A.Y * B.Y - A.Z * B.Z};
        }

        Vector3 Rotate(const Vector4& Q, const Vector3& V) {
            // v + 2w(q x v) + 2(q x (q x v))
            const Vector3 U {Q.X, Q.Y, Q.Z};
            auto Cross = [](const Vector3& A, const Vector3& B) {
                return Vector3 {A.Y * B.Z - A.Z * B.Y, A.Z * B.X - A.X * B.Z, A.X * B.Y - A.Y * B.X};
            };
            const Vector3 C1 = Cross(U, V);
            const Vector3 C2 = Cross(U, C1);
            return {V.X + 2.f * (Q.W * C1.X + C2.X), V.Y + 2.f * (Q.W * C1.Y + C2.Y), V.Z + 2.f * (Q.W * C1.Z + C2.Z)};
        }
    }  // namespace

    QsTransform QsTransform::Multiply(const QsTransform& Parent, const QsTransform& Child) {
        const Vector3 Moved = Rotate(Parent.Rotation, Child.Translation);
        return {{Parent.Translation.X + Moved.X, Parent.Translation.Y + Moved.Y, Parent.Translation.Z + Moved.Z},
                Normalized(QuatMultiply(Parent.Rotation, Child.Rotation)),
                {Parent.Scale.X * Child.Scale.X, Parent.Scale.Y * Child.Scale.Y, Parent.Scale.Z * Child.Scale.Z}};
    }

    QsTransform QsTransform::Blend(const QsTransform& A, const QsTransform& B, float T) {
        auto Lerp = [T](float X, float Y) { return X + (Y - X) * T; };
        const Vector4& P = A.Rotation;
        Vector4 Q        = B.Rotation;
        if (P.X * Q.X + P.Y * Q.Y + P.Z * Q.Z + P.W * Q.W < 0.f) Q = {-Q.X, -Q.Y, -Q.Z, -Q.W};
        return {{Lerp(A.Translation.X, B.Translation.X),
                 Lerp(A.Translation.Y, B.Translation.Y),
                 Lerp(A.Translation.Z, B.Translation.Z)},
                Normalized({Lerp(P.X, Q.X), Lerp(P.Y, Q.Y), Lerp(P.Z, Q.Z), Lerp(P.W, Q.W)}),
                {Lerp(A.Scale.X, B.Scale.X), Lerp(A.Scale.Y, B.Scale.Y), Lerp(A.Scale.Z, B.Scale.Z)}};
    }

    Matrix4x4 QsTransform::ToMatrix() const {
        const float X = Rotation.X, Y = Rotation.Y, Z = Rotation.Z, W = Rotation.W;
        // Rows are the rotated, scaled basis vectors.
        return {(1.f - 2.f * (Y * Y + Z * Z)) * Scale.X,
                2.f * (X * Y + Z * W) * Scale.X,
                2.f * (X * Z - Y * W) * Scale.X,
                0.f,
                2.f * (X * Y - Z * W) * Scale.Y,
                (1.f - 2.f * (X * X + Z * Z)) * Scale.Y,
                2.f * (Y * Z + X * W) * Scale.Y,
                0.f,
                2.f * (X * Z + Y * W) * Scale.Z,
                2.f * (Y * Z - X * W) * Scale.Z,
                (1.f - 2.f * (X * X + Y * Y)) * Scale.Z,
                0.f,
                Translation.X,
                Translation.Y,
                Translation.Z,
                1.f};
    }

    int Skeleton::FindBone(std::string_view Name) const {
        for (size_t I = 0; I < BoneNames.size(); ++I) {
            if (BoneNames[I] == Name) return static_cast<int>(I);
        }
        return -1;
    }

    std::vector<QsTransform> Skeleton::ModelSpace(std::span<const QsTransform> Local) const {
        if (Local.size() != Parents.size()) throw std::invalid_argument("ModelSpace needs one transform per bone");
        std::vector<QsTransform> Result(Local.begin(), Local.end());
        for (size_t I = 0; I < Result.size(); ++I) {
            const int16_t Parent = Parents[I];
            if (Parent >= 0 && static_cast<size_t>(Parent) < I)
                Result[I] = QsTransform::Multiply(Result[Parent], Local[I]);
        }
        return Result;
    }

    void AnimationBinding::Apply(const Animation& Animation, float Time, std::span<QsTransform> Pose) const {
        const std::vector<QsTransform> Tracks = Animation.Sample(Time);
        const size_t Count                    = std::min(Tracks.size(), TrackToBone.size());
        for (size_t T = 0; T < Count; ++T) {
            const int16_t Bone = TrackToBone[T];
            if (Bone < 0 || static_cast<size_t>(Bone) >= Pose.size()) continue;
            QsTransform& Base        = Pose[Bone];
            const QsTransform& Track = Tracks[T];
            switch (BlendHint) {
                case Blend::Normal: Base = Track; break;
                case Blend::AdditiveDeprecated:
                    Base.Translation = {Base.Translation.X + Track.Translation.X,
                                        Base.Translation.Y + Track.Translation.Y,
                                        Base.Translation.Z + Track.Translation.Z};
                    Base.Rotation    = Normalized(QuatMultiply(Base.Rotation, Track.Rotation));
                    Base.Scale       = {Base.Scale.X * Track.Scale.X, Base.Scale.Y * Track.Scale.Y, Base.Scale.Z * Track.Scale.Z};
                    break;
                case Blend::Additive: Base = QsTransform::Multiply(Base, Track); break;
            }
        }
    }
#pragma endregion

#pragma region Writing
    namespace {
        class ByteWriter {
        public:
            std::vector<uint8_t> Bytes;

            template<typename T>
            void Put(T Value) {
                const size_t At = Bytes.size();
                Bytes.resize(At + sizeof(T));
                std::memcpy(Bytes.data() + At, &Value, sizeof(T));
            }

            void Align(size_t Alignment) { Bytes.resize((Bytes.size() + Alignment - 1) / Alignment * Alignment); }
        };

        float Component(const Vector3& V, int C) { return C == 0 ? V.X : C == 1 ? V.Y : V.Z; }

        // The inverse of ReadQuat's Quat48: the largest component is dropped (made positive by negating the whole
        // quaternion, which is the same rotation) and the other three stored in 15 bits each.
        void PutQuat48(ByteWriter& Out, Vector4 Q) {
            float C[4]   = {Q.X, Q.Y, Q.Z, Q.W};
            int Largest  = 0;
            for (int I = 1; I < 4; ++I) {
                if (std::abs(C[I]) > std::abs(C[Largest])) Largest = I;
            }
            if (C[Largest] < 0.f) {
                for (float& F : C)
                    F = -F;
            }
            constexpr float Scale = 0.000043161f;
            uint16_t Stored[3];
            for (int I = 0, N = 0; I < 4; ++I) {
                if (I == Largest) continue;
                const long V = std::lround(C[I] / Scale) + 16383;
                Stored[N++]  = static_cast<uint16_t>(std::clamp(V, 0L, 32767L));
            }
            Stored[0] |= static_cast<uint16_t>((Largest & 1) << 15);
            Stored[1] |= static_cast<uint16_t>((Largest & 2) << 14);
            for (const uint16_t S : Stored)
                Out.Put(S);
        }

        // Knots of a degree-1 spline through Count points, one per frame: 0, 0, 1, ..., Count - 2, Count - 1, Count - 1.
        void PutLinearCurve(ByteWriter& Out, size_t Count) {
            Out.Put(static_cast<uint16_t>(Count - 1));
            Out.Put(uint8_t {1});
            Out.Put(uint8_t {0});
            for (size_t K = 0; K < Count; ++K)
                Out.Put(static_cast<uint8_t>(K));
            Out.Put(static_cast<uint8_t>(Count - 1));
        }

        // A translation or scale track over a block's frames; returns its mask bits (static: 1 << C; spline:
        // 0x10 << C).
        uint8_t PutVectorTrack(ByteWriter& Out, const std::vector<Vector3>& Values, float Default) {
            float Min[3], Max[3];
            uint8_t Bits   = 0;
            bool AnySpline = false;
            for (int C = 0; C < 3; ++C) {
                Min[C] = Max[C] = Component(Values[0], C);
                for (const Vector3& V : Values) {
                    Min[C] = std::min(Min[C], Component(V, C));
                    Max[C] = std::max(Max[C], Component(V, C));
                }
                if (Max[C] > Min[C]) {
                    Bits |= 0x10 << C;
                    AnySpline = true;
                } else if (Min[C] != Default) {
                    Bits |= 1 << C;
                }
            }
            if (!AnySpline) {
                for (int C = 0; C < 3; ++C) {
                    if (Bits & (1 << C)) Out.Put(Min[C]);
                }
                return Bits;
            }

            PutLinearCurve(Out, Values.size());
            Out.Align(4);
            for (int C = 0; C < 3; ++C) {
                if (Bits & (0x10 << C)) {
                    Out.Put(Min[C]);
                    Out.Put(Max[C]);
                } else if (Bits & (1 << C)) {
                    Out.Put(Min[C]);
                }
            }
            for (const Vector3& V : Values) {
                for (int C = 0; C < 3; ++C) {
                    if (!(Bits & (0x10 << C))) continue;
                    const float T = (Component(V, C) - Min[C]) / (Max[C] - Min[C]);
                    Out.Put(static_cast<uint16_t>(std::clamp(std::lround(T * 65535.f), 0L, 65535L)));
                }
            }
            Out.Align(4);
            return Bits;
        }

        // A rotation track over a block's frames; returns its mask bits (static: 0x0F, spline: 0xF0).
        uint8_t PutRotationTrack(ByteWriter& Out, std::vector<Vector4> Values) {
            // Neighbouring points on the same side of the hypersphere, so the spline doesn't swing the long way.
            for (size_t I = 1; I < Values.size(); ++I) {
                const Vector4& P = Values[I - 1];
                Vector4& Q       = Values[I];
                if (P.X * Q.X + P.Y * Q.Y + P.Z * Q.Z + P.W * Q.W < 0.f) Q = {-Q.X, -Q.Y, -Q.Z, -Q.W};
            }
            bool Constant = true;
            for (const Vector4& Q : Values)
                Constant &= Q == Values[0];
            uint8_t Bits = 0;
            if (Constant) {
                const Vector4& Q = Values[0];
                if (!(Q.X == 0.f && Q.Y == 0.f && Q.Z == 0.f && std::abs(Q.W) == 1.f)) {
                    PutQuat48(Out, Q);
                    Bits = 0x0F;
                }
            } else {
                PutLinearCurve(Out, Values.size());
                Out.Align(2);
                for (const Vector4& Q : Values)
                    PutQuat48(Out, Q);
                Bits = 0xF0;
            }
            Out.Align(4);
            return Bits;
        }

        // The item an object value is, so it can be found again after the data is laid out anew.
        size_t ItemOf(const Tagfile& File, const Value& Object) {
            for (size_t I = 1; I < File.Items.size(); ++I) {
                if ((File.Items[I].Flags & 0x10) && File.Items[I].Offset == Object.DataOffset()) return I;
            }
            throw BinaryException("Havok value isn't an object of this tagfile");
        }

        template<typename T>
        std::span<const uint8_t> AsBytes(const std::vector<T>& Values) {
            return {reinterpret_cast<const uint8_t*>(Values.data()), Values.size() * sizeof(T)};
        }
    }  // namespace

    Animation Animation::FromFrames(std::vector<QsTransform> Frames, uint32_t TrackCount, float FrameDuration) {
        if (TrackCount == 0 || Frames.empty() || Frames.size() % TrackCount != 0)
            throw std::invalid_argument("Animation frames must fill whole frames of TrackCount tracks");
        Animation Result;
        Result.Storage       = Compression::Interleaved;
        Result.TrackCount    = TrackCount;
        Result.FrameCount    = static_cast<uint32_t>(Frames.size() / TrackCount);
        Result.FrameDuration = FrameDuration;
        Result.Duration      = FrameDuration * static_cast<float>(Result.FrameCount - 1);
        Result.Frames        = std::move(Frames);
        return Result;
    }

    void Animation::WriteTo(Tagfile& File, const Value& Object) const {
        const size_t ObjectItem = ItemOf(File, Object);
        auto Target             = [&] { return File.ItemValue(ObjectItem); };
        if (Target()["numberOfTransformTracks"].AsInt() != TrackCount)
            throw BinaryException("The animation's track count doesn't match the Havok object's");
        if (FrameCount == 0) throw BinaryException("The animation has no frames");
        const std::vector<QsTransform> Baked = Bake();
        auto At = [&](uint32_t Frame, uint32_t Track) -> const QsTransform& {
            return Baked[size_t(Frame) * TrackCount + Track];
        };

        File.SetFloat(Target()["duration"], Duration);
        if (Target().IsA("hkaSplineCompressedAnimation")) {
            constexpr uint32_t PerBlock = 256;  // the knots are bytes, so a block's frames are numbered 0-255
            const uint32_t Blocks       = FrameCount <= 1 ? 1 : (FrameCount - 2) / (PerBlock - 1) + 1;
            const uint32_t FloatTracks  = static_cast<uint32_t>(Target()["numberOfFloatTracks"].AsInt());

            ByteWriter Out;
            std::vector<uint32_t> BlockOffsets, FloatOffsets;
            for (uint32_t B = 0; B < Blocks; ++B) {
                Out.Align(16);
                const size_t Start = Out.Bytes.size();
                BlockOffsets.push_back(static_cast<uint32_t>(Start));
                const uint32_t First = B * (PerBlock - 1);
                const uint32_t Last  = std::min(First + PerBlock - 1, FrameCount - 1);

                Out.Bytes.resize(Start + size_t(TrackCount) * 4 + FloatTracks);  // masks, filled in below
                Out.Align(4);
                for (uint32_t T = 0; T < TrackCount; ++T) {
                    std::vector<Vector3> Translations, Scales;
                    std::vector<Vector4> Rotations;
                    for (uint32_t F = First; F <= Last; ++F) {
                        Translations.push_back(At(F, T).Translation);
                        Rotations.push_back(At(F, T).Rotation);
                        Scales.push_back(At(F, T).Scale);
                    }
                    const uint8_t TranslationBits = PutVectorTrack(Out, Translations, 0.f);
                    const uint8_t RotationBits    = PutRotationTrack(Out, std::move(Rotations));
                    const uint8_t ScaleBits       = PutVectorTrack(Out, Scales, 1.f);
                    uint8_t* Mask                 = Out.Bytes.data() + Start + size_t(T) * 4;
                    Mask[0]                       = 1 | (Quat48 << 2) | (1 << 6);  // 16-bit, 48-bit, 16-bit
                    Mask[1]                       = TranslationBits;
                    Mask[2]                       = RotationBits;
                    Mask[3]                       = ScaleBits;
                }
                FloatOffsets.push_back(static_cast<uint32_t>(Out.Bytes.size() - Start));
            }
            Out.Align(16);

            File.SetInt(Target()["numFrames"], FrameCount);
            File.SetInt(Target()["numBlocks"], Blocks);
            File.SetInt(Target()["maxFramesPerBlock"], PerBlock);
            File.SetInt(Target()["maskAndQuantizationSize"], TrackCount * 4 + FloatTracks);
            File.SetFloat(Target()["frameDuration"], FrameDuration);
            File.SetFloat(Target()["blockDuration"], FrameDuration * (PerBlock - 1));
            File.SetFloat(Target()["blockInverseDuration"], 1.0 / (double(FrameDuration) * (PerBlock - 1)));
            File.SetArray(Target()["blockOffsets"], AsBytes(BlockOffsets), BlockOffsets.size());
            File.SetArray(Target()["floatBlockOffsets"], AsBytes(FloatOffsets), FloatOffsets.size());
            File.SetArray(Target()["transformOffsets"], {}, 0);
            File.SetArray(Target()["floatOffsets"], {}, 0);
            File.SetArray(Target()["data"], Out.Bytes, Out.Bytes.size());
        } else if (Target().IsA("hkaInterleavedUncompressedAnimation")) {
            ByteWriter Out;
            for (const QsTransform& T : Baked) {
                for (const float F : {T.Translation.X, T.Translation.Y, T.Translation.Z, 1.f})  // w is unused
                    Out.Put(F);
                for (const float F : {T.Rotation.X, T.Rotation.Y, T.Rotation.Z, T.Rotation.W})
                    Out.Put(F);
                for (const float F : {T.Scale.X, T.Scale.Y, T.Scale.Z, 0.f})
                    Out.Put(F);
            }
            File.SetArray(Target()["transforms"], Out.Bytes, Baked.size());
        } else {
            throw BinaryException("Can't write an animation into a Havok " + Target().TypeName());
        }

        // Root motion, into the object's reference frame if it has one.
        const Value Motion = Target()["extractedMotion"].Deref();
        if (RootMotion && Motion.IsValid() && Motion.IsA("hkaDefaultAnimatedReferenceFrame")) {
            const size_t MotionItem = ItemOf(File, Motion);
            File.SetFloat(File.ItemValue(MotionItem)["duration"], RootMotion->Duration);
            File.SetArray(File.ItemValue(MotionItem)["referenceFrameSamples"],
                          AsBytes(RootMotion->Samples),
                          RootMotion->Samples.size());
        }
    }
#pragma endregion
}  // namespace Souls::Havok
