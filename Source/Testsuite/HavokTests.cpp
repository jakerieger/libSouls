// Copyright (C) 2026 Jake Rieger
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, version 3. This program is distributed in the hope that it will
// be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU General
// Public License along with this program. If not, see https://www.gnu.org/licenses/.

#include "TestArchives.hpp"
#include "TestResult.hpp"

#include <libSouls/Binders/BND4.hpp>
#include <libSouls/Formats/FLVER/FLVER2.hpp>
#include <libSouls/Formats/Havok/Animation.hpp>
#include <libSouls/Formats/Havok/Tagfile.hpp>

#include <cmath>
#include <cstdio>
#include <map>
#include <optional>
#include <string>
#include <vector>

using namespace Souls;

namespace {
    int Failures = 0;

#define CHECK(Cond)                                                                                                    \
    do {                                                                                                               \
        if (!(Cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #Cond);                                                \
            ++Failures;                                                                                                \
        }                                                                                                              \
    } while (0)

    bool EndsWith(const std::string& Text, const std::string& Suffix) {
        return Text.size() >= Suffix.size() && Text.compare(Text.size() - Suffix.size(), Suffix.size(), Suffix) == 0;
    }

    // A skeleton's bones, parents and reference pose line up.
    void CheckSkeleton(const Havok::Tagfile& File, const std::string& Label) {
        const auto Skeletons = File.ObjectsOfType("hkaSkeleton");
        CHECK(!Skeletons.empty());
        for (const Havok::Value& Skeleton : Skeletons) {
            const Havok::Value Bones = Skeleton["bones"];
            const size_t Count       = Bones.Size();
            CHECK(Count > 0);
            CHECK(Skeleton["parentIndices"].Size() == Count);
            CHECK(Skeleton["referencePose"].Size() == Count);
            CHECK(!Bones.At(0)["name"].AsString().empty());
            const std::vector<int64_t> Parents = Skeleton["parentIndices"].AsInts();
            CHECK(Parents[0] == -1);
            bool ParentsOk = true;
            for (size_t I = 0; I < Parents.size(); ++I)
                ParentsOk &= Parents[I] >= -1 && Parents[I] < static_cast<int64_t>(I);
            CHECK(ParentsOk);
            // A reference pose rotation is a unit quaternion.
            const std::vector<float> Rotation = Skeleton["referencePose"].At(0)["rotation"]["vec"].AsFloats();
            const float Length = Rotation[0] * Rotation[0] + Rotation[1] * Rotation[1] + Rotation[2] * Rotation[2] +
                                 Rotation[3] * Rotation[3];
            CHECK(Rotation.size() == 4 && Length > 0.99f && Length < 1.01f);
            std::printf("%s: skeleton \"%s\", %zu bones, root \"%s\"\n",
                        Label.c_str(),
                        Skeleton["name"].AsString().c_str(),
                        Count,
                        Bones.At(0)["name"].AsString().c_str());
        }
    }
}  // namespace

// Reads Elden Ring skeletons and animations (Havok 2018 tagfiles, the animations' types in a compendium).
int RunHavokTests() {
    Failures = 0;
    EldenRingArchives Archives;
    if (!Archives.IsOpen()) {
        std::printf("Havok checks skipped (needs Elden Ring and the BinderKeys submodule)\n");
        return TestSkipped;
    }

    // The player's and an enemy's skeletons, each with its own types.
    for (const char* Character : {"c0000", "c4290"}) {
        const auto Anibnd = Archives.Read(std::string("/chr/") + Character + ".anibnd.dcx");
        CHECK(Anibnd.has_value());
        if (!Anibnd) continue;
        const BND4 Binder = BND4::Read(*Anibnd);  // kept alive: a range-for over Read(...).Files would outlive it
        for (const BinderFile& File : Binder.Files) {
            if (File.Name && EndsWith(*File.Name, ".hkx")) {
                const Havok::Tagfile Skeleton = Havok::Tagfile::Read(File.Bytes);
                CHECK(Skeleton.Write() == File.Bytes);  // standalone files (own types) write back byte for byte
                CHECK(Skeleton.SdkVersion == "20180100" && !Skeleton.CompendiumID);
                CheckSkeleton(Skeleton, Character);
            }
        }
    }

    // The player's animations: every one reads with the binder's compendium and describes a spline-compressed
    // animation bound to the skeleton's bones.
    const auto Anibnd = Archives.Read("/chr/c0000_a00_hi.anibnd.dcx");
    CHECK(Anibnd.has_value());
    if (Anibnd) {
        const BND4 Binder = BND4::Read(*Anibnd);
        std::optional<Havok::Compendium> Compendium;
        for (const BinderFile& File : Binder.Files) {
            if (File.Name && EndsWith(*File.Name, ".compendium")) Compendium = Havok::Compendium::Read(File.Bytes);
        }
        CHECK(Compendium.has_value() && !Compendium->IDs.empty());
        size_t Read = 0, Spline = 0, Failed = 0;
        std::map<std::string, size_t> Kinds;
        for (const BinderFile& File : Binder.Files) {
            if (!File.Name || !EndsWith(*File.Name, ".hkx") || !Compendium) continue;
            try {
                const Havok::Tagfile Anim = Havok::Tagfile::Read(File.Bytes, &*Compendium);
                ++Read;
                if (Anim.Write() != File.Bytes) ++Failed;  // and so do files whose types are in the compendium
                {
                    // Replacing an array with its own contents lays the data out again the same way.
                    Havok::Tagfile Copy                = Anim;
                    const Havok::Value Field           = Copy.ObjectsOfType("hkaAnimation")[0]["data"];
                    const std::vector<uint8_t> Content(Field.AsBytes().begin(), Field.AsBytes().end());
                    Copy.SetArray(Field, Content, Content.size());
                    if (Copy.Write() != File.Bytes) ++Failed;
                }
                for (const Havok::Value& Animation : Anim.ObjectsOfType("hkaAnimation")) {
                    ++Kinds[Animation.TypeName()];
                    const int64_t Tracks = Animation["numberOfTransformTracks"].AsInt();
                    if (Animation.Has("numFrames")) {
                        ++Spline;
                        const size_t Blocks = static_cast<size_t>(Animation["numBlocks"].AsInt());
                        if (Animation["blockOffsets"].Size() != Blocks || Animation["data"].AsBytes().empty() ||
                            Tracks <= 0)
                            ++Failed;
                    }
                }
                for (const Havok::Value& Binding : Anim.ObjectsOfType("hkaAnimationBinding")) {
                    const Havok::Value Animation = Binding["animation"].Deref();
                    if (!Animation.IsValid() || Binding["transformTrackToBoneIndices"].Size() !=
                                                  static_cast<size_t>(Animation["numberOfTransformTracks"].AsInt())) {
                        ++Failed;
                    }
                }
            } catch (const std::exception& E) {
                if (Failed++ < 3) std::printf("  %s: %s\n", File.Name->c_str(), E.what());
            }
        }
        std::printf("c0000_a00_hi: %zu animations read, %zu spline-compressed, %zu with problems\n",
                    Read,
                    Spline,
                    Failed);
        for (const auto& [Kind, Count] : Kinds)
            std::printf("  %s x%zu\n", Kind.c_str(), Count);
        CHECK(Read > 1000 && Failed == 0);

        // Without its compendium, an animation can't be read.
        for (const BinderFile& File : Binder.Files) {
            if (File.Name && EndsWith(*File.Name, ".hkx")) {
                bool Threw = false;
                try {
                    Havok::Tagfile::Read(File.Bytes);
                } catch (const BinaryException&) { Threw = true; }
                CHECK(Threw);
                break;
            }
        }
    }

    // Decoding: every player animation decodes, its splines meet at block boundaries, and its bones keep the lengths
    // the skeleton gives them.
    if (Anibnd) {
        std::optional<Havok::Skeleton> Player;
        if (const auto Chr = Archives.Read("/chr/c0000.anibnd.dcx")) {
            const BND4 Binder = BND4::Read(*Chr);
            for (const BinderFile& File : Binder.Files) {
                if (File.Name && EndsWith(*File.Name, ".hkx"))
                    Player = Havok::Skeleton::FromValue(Havok::Tagfile::Read(File.Bytes).ObjectsOfType("hkaSkeleton")[0]);
            }
        }
        CHECK(Player.has_value() && Player->BoneNames.size() == Player->Parents.size());
        // Bones that are meant to translate: the root and root motion, armor and IK helpers, and spare bones.
        std::vector<bool> Moves;
        if (Player) {
            for (const std::string& Name : Player->BoneNames) {
                bool Helper = Name == "Master" || Name == "RootPos";
                for (const char* Part : {"Armor", "Axilla", "Target", "Xtra"})
                    Helper |= Name.find(Part) != std::string::npos;
                Moves.push_back(Helper);
            }
        }

        const BND4 Binder = BND4::Read(*Anibnd);
        std::optional<Havok::Compendium> Compendium;
        for (const BinderFile& File : Binder.Files) {
            if (File.Name && EndsWith(*File.Name, ".compendium")) Compendium = Havok::Compendium::Read(File.Bytes);
        }
        size_t Decoded = 0, Failed = 0, WithMotion = 0, Boundaries = 0, Seams = 0, Bones = 0, Stretched = 0;
        float WorstSeam = 0.f, WorstMove = 0.f, WorstTurn = 0.f;
        size_t WrittenBytes = 0, OriginalBytes = 0;
        for (const BinderFile& File : Binder.Files) {
            if (!File.Name || !EndsWith(*File.Name, ".hkx") || !Compendium) continue;
            try {
                const Havok::Tagfile Anim = Havok::Tagfile::Read(File.Bytes, &*Compendium);
                const auto Animation =
                  Havok::Animation::FromValue(Anim.ObjectsOfType("hkaSplineCompressedAnimation")[0]);
                const auto Binding = Havok::AnimationBinding::FromValue(Anim.ObjectsOfType("hkaAnimationBinding")[0]);
                ++Decoded;
                if (Animation.RootMotion && !Animation.RootMotion->Samples.empty()) ++WithMotion;
                if (Binding.TrackToBone.size() != Animation.TrackCount) ++Failed;

                // Just before and at each block boundary, the two blocks' splines should agree.
                std::vector<Havok::QsTransform> Before(Animation.TrackCount), At(Animation.TrackCount);
                const uint32_t Stride = Animation.FramesPerBlock - 1;
                for (uint32_t Boundary = Stride; Boundary < Animation.FrameCount - 1; Boundary += Stride) {
                    Animation.SampleFrame(static_cast<float>(Boundary) - 0.001f, Before);
                    Animation.SampleFrame(static_cast<float>(Boundary), At);
                    ++Boundaries;
                    float Worst = 0.f;
                    for (size_t T = 0; T < At.size(); ++T) {
                        const auto& A = Before[T].Rotation;
                        const auto& B = At[T].Rotation;
                        Worst = std::max(Worst, 1.f - std::abs(A.X * B.X + A.Y * B.Y + A.Z * B.Z + A.W * B.W));
                    }
                    WorstSeam = std::max(WorstSeam, Worst);
                    if (Worst > 1e-3f) ++Seams;
                }

                // Every frame: the bones keep their reference lengths (additive animations hold offsets instead).
                if (Player && Binding.BlendHint == Havok::AnimationBinding::Blend::Normal) {
                    for (uint32_t Frame = 0; Frame < Animation.FrameCount; ++Frame) {
                        Animation.SampleFrame(static_cast<float>(Frame), At);
                        for (size_t T = 0; T < At.size(); ++T) {
                            const int16_t Bone = Binding.TrackToBone[T];
                            if (Bone < 0 || static_cast<size_t>(Bone) >= Player->ReferencePose.size() ||
                                Moves[Bone])
                                continue;
                            const auto& P = At[T].Translation;
                            const auto& R = Player->ReferencePose[Bone].Translation;
                            const float Length    = std::sqrt(P.X * P.X + P.Y * P.Y + P.Z * P.Z);
                            const float Reference = std::sqrt(R.X * R.X + R.Y * R.Y + R.Z * R.Z);
                            ++Bones;
                            if (std::abs(Length - Reference) > 0.01f + 0.05f * Reference) ++Stretched;
                        }
                    }
                }

                // Writing: re-encode the baked frames into a copy of the file, write it, read it back, and compare.
                const std::vector<Havok::QsTransform> Original = Animation.Bake();
                Havok::Animation Edited = Havok::Animation::FromFrames(Original, Animation.TrackCount, Animation.FrameDuration);
                Edited.RootMotion       = Animation.RootMotion;
                Havok::Tagfile Copy     = Anim;
                Edited.WriteTo(Copy, Copy.ObjectsOfType("hkaSplineCompressedAnimation")[0]);
                const std::vector<uint8_t> Written = Copy.Write();
                WrittenBytes += Written.size();
                OriginalBytes += File.Bytes.size();
                const Havok::Tagfile Back    = Havok::Tagfile::Read(Written, &*Compendium);
                const auto Reread            = Havok::Animation::FromValue(Back.ObjectsOfType("hkaAnimation")[0]);
                const auto Rebaked           = Reread.Bake();
                if (Rebaked.size() != Original.size() || std::abs(Reread.Duration - Animation.Duration) > 1e-4f) {
                    ++Failed;
                } else {
                    for (size_t I = 0; I < Original.size(); ++I) {
                        const auto& A = Original[I];
                        const auto& B = Rebaked[I];
                        WorstMove     = std::max({WorstMove,
                                              std::abs(A.Translation.X - B.Translation.X),
                                              std::abs(A.Translation.Y - B.Translation.Y),
                                              std::abs(A.Translation.Z - B.Translation.Z),
                                              std::abs(A.Scale.X - B.Scale.X),
                                              std::abs(A.Scale.Y - B.Scale.Y),
                                              std::abs(A.Scale.Z - B.Scale.Z)});
                        const float Dot = A.Rotation.X * B.Rotation.X + A.Rotation.Y * B.Rotation.Y +
                                          A.Rotation.Z * B.Rotation.Z + A.Rotation.W * B.Rotation.W;
                        WorstTurn = std::max(WorstTurn, 1.f - std::abs(Dot));
                    }
                }
                if (Animation.RootMotion) {
                    const auto* Motion = Reread.RootMotion ? &Reread.RootMotion->Samples : nullptr;
                    if (!Motion || *Motion != Animation.RootMotion->Samples) ++Failed;
                }
            } catch (const std::exception& E) {
                if (Failed++ < 5) std::printf("  %s: %s\n", File.Name->c_str(), E.what());
            }
        }
        std::printf("decoded %zu animations (%zu failed, %zu with root motion); %zu/%zu block seams off (worst %g); "
                    "%zu/%zu bone samples stretched\n",
                    Decoded,
                    Failed,
                    WithMotion,
                    Seams,
                    Boundaries,
                    WorstSeam,
                    Stretched,
                    Bones);
        CHECK(Decoded > 1000 && Failed == 0);
        CHECK(Seams == 0);
        CHECK(Stretched * 1000 < Bones);
        std::printf("re-encoded: worst translation/scale error %g, worst rotation error %g (1 - |dot|); files %.2fx the "
                    "originals' size\n",
                    WorstMove,
                    WorstTurn,
                    OriginalBytes ? double(WrittenBytes) / double(OriginalBytes) : 0.0);
        CHECK(WorstMove < 1e-3f && WorstTurn < 1e-6f);
    }

    // Poses: an enemy's Havok reference pose, put into model space, puts each bone where the model's FLVER does.
    {
        const auto Anibnd = Archives.Read("/chr/c4290.anibnd.dcx");
        const auto Chrbnd = Archives.Read("/chr/c4290.chrbnd.dcx");
        CHECK(Anibnd && Chrbnd);
        if (Anibnd && Chrbnd) {
            std::optional<Havok::Skeleton> Skeleton;
            const BND4 Anims = BND4::Read(*Anibnd);
            for (const BinderFile& File : Anims.Files) {
                if (File.Name && EndsWith(*File.Name, ".hkx")) {
                    const Havok::Tagfile Tag = Havok::Tagfile::Read(File.Bytes);
                    if (const auto Found = Tag.ObjectsOfType("hkaSkeleton"); !Found.empty())
                        Skeleton = Havok::Skeleton::FromValue(Found[0]);
                }
            }
            std::optional<FLVER2> Model;
            const BND4 Chr = BND4::Read(*Chrbnd);
            for (const BinderFile& File : Chr.Files) {
                if (File.Name && EndsWith(*File.Name, ".flver")) Model = FLVER2::Read(File.Bytes);
            }
            CHECK(Skeleton && Model);
            if (Skeleton && Model) {
                // The FLVER's bones in model space (row vectors: local * parent). A FLVER parent can come after its
                // child, so walk up the chain for each bone.
                std::vector<Matrix4x4> Flver(Model->Bones.size());
                for (size_t I = 0; I < Flver.size(); ++I) {
                    Flver[I] = Model->Bones[I].ComputeLocalTransform();
                    for (int16_t P = Model->Bones[I].ParentIndex, Depth = 0;
                         P >= 0 && static_cast<size_t>(P) < Flver.size() && Depth < 256;
                         P = Model->Bones[P].ParentIndex, ++Depth)
                        Flver[I] = Flver[I] * Model->Bones[P].ComputeLocalTransform();
                }
                const std::vector<Havok::QsTransform> World = Skeleton->ModelSpace(Skeleton->ReferencePose);
                size_t Compared = 0, Off = 0;
                for (size_t I = 0; I < Model->Bones.size(); ++I) {
                    const int Bone = Skeleton->FindBone(Model->Bones[I].Name);
                    if (Bone < 0) continue;
                    // Only bones attached the same way in both (some FLVER bones are unparented placeholders).
                    const int16_t FlverParent = Model->Bones[I].ParentIndex;
                    const int16_t HavokParent = Skeleton->Parents[Bone];
                    const std::string FlverParentName =
                      FlverParent >= 0 ? Model->Bones[FlverParent].Name : std::string();
                    const std::string HavokParentName =
                      HavokParent >= 0 ? Skeleton->BoneNames[HavokParent] : std::string();
                    if (FlverParentName != HavokParentName) continue;
                    ++Compared;
                    const Vector3& P = World[Bone].Translation;
                    const float D    = std::abs(P.X - Flver[I].M41) + std::abs(P.Y - Flver[I].M42) +
                                    std::abs(P.Z - Flver[I].M43);
                    if (D > 1e-3f) ++Off;
                    // ToMatrix agrees with composing the matrices too.
                    const Matrix4x4 M = World[Bone].ToMatrix();
                    if (std::abs(M.M41 - Flver[I].M41) + std::abs(M.M11 - Flver[I].M11) +
                          std::abs(M.M23 - Flver[I].M23) >
                        1e-3f)
                        ++Off;
                }
                std::printf("c4290: %zu FLVER bones matched to the Havok skeleton, %zu off\n", Compared, Off);
                CHECK(Compared > 50 && Off == 0);
            }
        }
    }

    // The game's one uncompressed animation decodes, and baking matches sampling frame by frame.
    if (const auto Anibnd = Archives.Read("/chr/c4751.anibnd.dcx")) {
        const BND4 Binder = BND4::Read(*Anibnd);
        std::optional<Havok::Compendium> Compendium;
        for (const BinderFile& File : Binder.Files) {
            if (File.Name && EndsWith(*File.Name, ".compendium")) Compendium = Havok::Compendium::Read(File.Bytes);
        }
        bool Found = false;
        for (const BinderFile& File : Binder.Files) {
            if (!File.Name || !EndsWith(*File.Name, "a000_030000.hkx") || !Compendium) continue;
            const Havok::Tagfile Tag = Havok::Tagfile::Read(File.Bytes, &*Compendium);
            const auto Animation     = Havok::Animation::FromValue(Tag.ObjectsOfType("hkaAnimation")[0]);
            Found                    = Animation.Storage == Havok::Animation::Compression::Interleaved;
            const auto Baked         = Animation.Bake();
            CHECK(Animation.FrameCount > 1 && Baked.size() == size_t(Animation.FrameCount) * Animation.TrackCount);
            const auto Last = Animation.Sample(Animation.Duration);
            bool Same       = true;
            for (size_t T = 0; T < Last.size(); ++T)
                Same &= Last[T].Translation == Baked[(Animation.FrameCount - 1) * Animation.TrackCount + T].Translation;
            CHECK(Same);
            std::printf("c4751 a000_030000: interleaved, %u frames x %u tracks\n", Animation.FrameCount, Animation.TrackCount);

            // Writing its frames back unchanged gives the same file, except the translations' unused w (Havok leaves
            // whatever was in memory there): the same size, and the same frames bit for bit.
            Havok::Animation Copy = Havok::Animation::FromFrames(Baked, Animation.TrackCount, Animation.FrameDuration);
            Copy.Duration         = Animation.Duration;
            Copy.RootMotion       = Animation.RootMotion;
            Havok::Tagfile Out    = Tag;
            Copy.WriteTo(Out, Out.ObjectsOfType("hkaAnimation")[0]);
            const std::vector<uint8_t> Written = Out.Write();
            CHECK(Written.size() == File.Bytes.size());
            const Havok::Tagfile Back = Havok::Tagfile::Read(Written, &*Compendium);
            const auto Rebaked        = Havok::Animation::FromValue(Back.ObjectsOfType("hkaAnimation")[0]).Bake();
            bool Identical            = Rebaked.size() == Baked.size();
            for (size_t I = 0; Identical && I < Baked.size(); ++I) {
                Identical = Rebaked[I].Translation == Baked[I].Translation && Rebaked[I].Rotation == Baked[I].Rotation &&
                            Rebaked[I].Scale == Baked[I].Scale;
            }
            CHECK(Identical);
        }
        CHECK(Found);
    }

    std::printf(Failures == 0 ? "Havok tests passed\n" : "Havok tests: %d failure(s)\n", Failures);
    return Failures;
}
