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
#include <libSouls/Formats/Havok/Tagfile.hpp>

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

    std::printf(Failures == 0 ? "Havok tests passed\n" : "Havok tests: %d failure(s)\n", Failures);
    return Failures;
}
