//
// Created by Jake Rieger on 10/8/2026.
//

#include "TAE3.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>

namespace Souls {
    namespace {
        TAE3::Event ReadEvent(BinaryReader& Reader) {
            TAE3::Event E;
            const int64_t StartTimeOffset = Reader.ReadInt64();
            const int64_t EndTimeOffset   = Reader.ReadInt64();
            const int64_t EventDataOffset = Reader.ReadInt64();
            E.StartTime                   = Reader.ReadAt<float>(StartTimeOffset);
            E.EndTime                     = Reader.ReadAt<float>(EndTimeOffset);

            Reader.StepIn(EventDataOffset);
            const auto Type = static_cast<TAE3EventType>(Reader.ReadUInt64());
            Reader.Assert<int64_t>(Reader.Position() + 8);
            E.Data = ReadTAE3EventData(Type, Reader);
            Reader.StepOut();
            return E;
        }

        bool EndsWith(const std::string& Text, const std::string& Suffix) {
            return Text.size() >= Suffix.size() && Text.compare(Text.size() - Suffix.size(), Suffix.size(), Suffix) == 0;
        }
    }  // namespace

    bool TAE3::IsImpl(BinaryReader& Reader) {
        if (Reader.Length() < 4) {
            return false;
        }
        return Reader.GetASCII(0, 4) == "TAE ";
    }

    void TAE3::ReadImpl(BinaryReader& Reader) {
        Reader.Order = Endian::Little;
        Reader.AssertMagic("TAE ");
        Reader.AssertPattern(3, 0);
        Reader.Assert<uint8_t>(0xFF);
        Reader.Assert<int32_t>(0x1000C);
        Reader.ReadInt32();  // file size
        Reader.Assert<int64_t>(0x40);
        Reader.Assert<int64_t>(1);
        Reader.Assert<int64_t>(0x50);
        Reader.Assert<int64_t>(0x80);
        Unk30 = Reader.ReadInt64();
        Reader.Assert<int64_t>(0);
        Reader.ReadInto(std::span<uint8_t>(Flags));
        Reader.Assert<int64_t>(1);
        ID                         = Reader.ReadInt32();
        const int32_t AnimCount    = Reader.ReadInt32();
        const int64_t AnimsOffset  = Reader.ReadInt64();
        Reader.ReadInt64();  // anim groups offset
        Reader.Assert<int64_t>(0xA0);
        Reader.Assert<int64_t>(AnimCount);
        Reader.ReadInt64();  // first anim offset
        Reader.Assert<int64_t>(1);
        Reader.Assert<int64_t>(0x90);
        Reader.Assert<int32_t>(ID);
        Reader.Assert<int32_t>(ID);
        Reader.Assert<int64_t>(0x50);
        Reader.Assert<int64_t>(0);
        Reader.Assert<int64_t>(0xB0);
        const int64_t SkeletonNameOffset = Reader.ReadInt64();
        const int64_t SibNameOffset      = Reader.ReadInt64();
        Reader.Assert<int64_t>(0);
        Reader.Assert<int64_t>(0);
        SkeletonName = Reader.GetUTF16Text(SkeletonNameOffset);
        SibName      = Reader.GetUTF16Text(SibNameOffset);

        Animations.clear();
        Reader.StepIn(AnimsOffset);
        for (int32_t I = 0; I < AnimCount; ++I) {
            Animation A;
            A.ID                = Reader.ReadInt64();
            const int64_t Offset = Reader.ReadInt64();
            Reader.StepIn(Offset);
            const int64_t EventHeadersOffset = Reader.ReadInt64();
            const int64_t EventGroupsOffset  = Reader.ReadInt64();
            Reader.ReadInt64();  // times offset
            const int64_t AnimFileOffset  = Reader.ReadInt64();
            const int32_t EventCount      = Reader.ReadInt32();
            const int32_t EventGroupCount = Reader.ReadInt32();
            Reader.ReadInt32();  // times count
            Reader.Assert<int32_t>(0);

            std::vector<int64_t> EventHeaderOffsets;
            Reader.StepIn(EventHeadersOffset);
            for (int32_t E = 0; E < EventCount; ++E) {
                EventHeaderOffsets.push_back(Reader.Position());
                A.Events.push_back(ReadEvent(Reader));
            }
            Reader.StepOut();

            Reader.StepIn(EventGroupsOffset);
            for (int32_t G = 0; G < EventGroupCount; ++G) {
                EventGroup Group;
                const int64_t EntryCount   = Reader.ReadInt64();
                const int64_t ValuesOffset = Reader.ReadInt64();
                const int64_t TypeOffset   = Reader.ReadInt64();
                Reader.Assert<int64_t>(0);
                Reader.StepIn(TypeOffset);
                Group.Type = static_cast<EventType>(Reader.ReadUInt64());
                Reader.Assert<int64_t>(0);
                Reader.StepOut();
                Reader.StepIn(ValuesOffset);
                for (const int32_t Offset32 : Reader.ReadArray<int32_t>(static_cast<size_t>(EntryCount))) {
                    const auto Found = std::find(EventHeaderOffsets.begin(), EventHeaderOffsets.end(), static_cast<int64_t>(Offset32));
                    Group.Indices.push_back(Found == EventHeaderOffsets.end() ? -1 : static_cast<int32_t>(Found - EventHeaderOffsets.begin()));
                }
                Reader.StepOut();
                A.EventGroups.push_back(std::move(Group));
            }
            Reader.StepOut();

            Reader.StepIn(AnimFileOffset);
            A.AnimFileReference = Reader.Assert<int64_t>(0, 1) == 1;
            Reader.Assert<int64_t>(Reader.Position() + 8);
            const int64_t AnimFileNameOffset = Reader.ReadInt64();
            A.AnimFileUnk18                  = Reader.ReadInt32();
            A.AnimFileUnk1C                  = Reader.ReadInt32();
            Reader.Assert<int64_t>(0);
            Reader.Assert<int64_t>(0);
            if (AnimFileNameOffset < Reader.Length()) {
                A.AnimFileName = Reader.GetUTF16Text(AnimFileNameOffset);
            }
            // When Reference is false, there is always a filename. When true, there usually is not, but sometimes
            // there is, for unknown reasons; dropping it here is a hack to achieve byte-perfection.
            if (!(EndsWith(A.AnimFileName, ".hkt") || EndsWith(A.AnimFileName, ".hkx"))) {
                A.AnimFileName.clear();
            }
            Reader.StepOut();

            Reader.StepOut();
            Animations.push_back(std::move(A));
        }
        Reader.StepOut();
        // Animation groups are not read.
    }

    void TAE3::WriteImpl(BinaryWriter& Writer) {
        Writer.Order = Endian::Little;
        Writer.WriteMagic("TAE ");
        Writer.WriteByte(0);
        Writer.WriteByte(0);
        Writer.WriteByte(0);
        Writer.WriteByte(0xFF);
        Writer.WriteInt32(0x1000C);
        Writer.Reserve<int32_t>("FileSize");
        Writer.WriteInt64(0x40);
        Writer.WriteInt64(1);
        Writer.WriteInt64(0x50);
        Writer.WriteInt64(0x80);
        Writer.WriteInt64(Unk30);
        Writer.WriteInt64(0);
        Writer.WriteBytes(Flags);
        Writer.WriteInt64(1);
        Writer.WriteInt32(ID);
        Writer.WriteInt32(static_cast<int32_t>(Animations.size()));
        Writer.Reserve<int64_t>("AnimsOffset");
        Writer.Reserve<int64_t>("AnimGroupsOffset");
        Writer.WriteInt64(0xA0);
        Writer.WriteInt64(static_cast<int64_t>(Animations.size()));
        Writer.Reserve<int64_t>("FirstAnimOffset");
        Writer.WriteInt64(1);
        Writer.WriteInt64(0x90);
        Writer.WriteInt32(ID);
        Writer.WriteInt32(ID);
        Writer.WriteInt64(0x50);
        Writer.WriteInt64(0);
        Writer.WriteInt64(0xB0);
        Writer.Reserve<int64_t>("SkeletonName");
        Writer.Reserve<int64_t>("SibName");
        Writer.WriteInt64(0);
        Writer.WriteInt64(0);
        Writer.Fill<int64_t>("SkeletonName", Writer.Position());
        Writer.WriteUTF16Text(SkeletonName, true);
        Writer.Align(0x10);
        Writer.Fill<int64_t>("SibName", Writer.Position());
        Writer.WriteUTF16Text(SibName, true);
        Writer.Align(0x10);

        std::stable_sort(Animations.begin(), Animations.end(), [](const Animation& A, const Animation& B) { return A.ID < B.ID; });
        std::vector<int64_t> AnimOffsets;
        if (Animations.empty()) {
            Writer.Fill<int64_t>("AnimsOffset", 0);
        } else {
            Writer.Fill<int64_t>("AnimsOffset", Writer.Position());
            for (size_t I = 0; I < Animations.size(); ++I) {
                AnimOffsets.push_back(Writer.Position());
                Writer.WriteInt64(Animations[I].ID);
                Writer.Reserve<int64_t>("AnimationOffset" + std::to_string(I));
            }
        }

        Writer.Fill<int64_t>("AnimGroupsOffset", Writer.Position());
        Writer.Reserve<int64_t>("AnimGroupsCount");
        Writer.Reserve<int64_t>("AnimGroupsOffset");
        int64_t GroupCount      = 0;
        const int64_t GroupStart = Writer.Position();
        for (size_t I = 0; I < Animations.size(); ++I) {
            const size_t FirstIndex = I;
            Writer.WriteInt32(static_cast<int32_t>(Animations[I].ID));
            while (I + 1 < Animations.size() && Animations[I + 1].ID == Animations[I].ID + 1) {
                ++I;
            }
            Writer.WriteInt32(static_cast<int32_t>(Animations[I].ID));
            Writer.WriteInt64(AnimOffsets[FirstIndex]);
            ++GroupCount;
        }
        Writer.Fill<int64_t>("AnimGroupsCount", GroupCount);
        Writer.Fill<int64_t>("AnimGroupsOffset", GroupCount == 0 ? 0 : GroupStart);

        if (Animations.empty()) {
            Writer.Fill<int64_t>("FirstAnimOffset", 0);
        } else {
            Writer.Fill<int64_t>("FirstAnimOffset", Writer.Position());
            for (size_t I = 0; I < Animations.size(); ++I) {
                const std::string Idx = std::to_string(I);
                Writer.Fill<int64_t>("AnimationOffset" + Idx, Writer.Position());
                Writer.Reserve<int64_t>("EventHeadersOffset" + Idx);
                Writer.Reserve<int64_t>("EventGroupHeadersOffset" + Idx);
                Writer.Reserve<int64_t>("TimesOffset" + Idx);
                Writer.Reserve<int64_t>("AnimFileOffset" + Idx);
                Writer.WriteInt32(static_cast<int32_t>(Animations[I].Events.size()));
                Writer.WriteInt32(static_cast<int32_t>(Animations[I].EventGroups.size()));
                Writer.Reserve<int32_t>("TimesCount" + Idx);
                Writer.WriteInt32(0);
            }
        }

        for (size_t I = 0; I < Animations.size(); ++I) {
            const Animation& A      = Animations[I];
            const std::string Idx = std::to_string(I);

            // Anim file reference.
            Writer.Fill<int64_t>("AnimFileOffset" + Idx, Writer.Position());
            Writer.WriteInt64(A.AnimFileReference ? 1 : 0);
            Writer.WriteInt64(Writer.Position() + 8);
            Writer.Reserve<int64_t>("AnimFileNameOffset");
            Writer.WriteInt32(A.AnimFileUnk18);
            Writer.WriteInt32(A.AnimFileUnk1C);
            Writer.WriteInt64(0);
            Writer.WriteInt64(0);
            Writer.Fill<int64_t>("AnimFileNameOffset", Writer.Position());
            if (!A.AnimFileName.empty()) {
                Writer.WriteUTF16Text(A.AnimFileName, true);
                Writer.Align(0x10);
            }

            // Times.
            std::set<float> Times;
            for (const Event& E : A.Events) {
                Times.insert(E.StartTime);
                Times.insert(E.EndTime);
            }
            Writer.Fill<int32_t>("TimesCount" + Idx, static_cast<int32_t>(Times.size()));
            Writer.Fill<int64_t>("TimesOffset" + Idx, Times.empty() ? 0 : Writer.Position());
            std::map<float, int64_t> TimeOffsets;
            for (const float Time : Times) {
                TimeOffsets[Time] = Writer.Position();
                Writer.WriteFloat(Time);
            }
            Writer.Align(0x10);

            // Event headers.
            std::vector<int64_t> EventHeaderOffsets;
            if (!A.Events.empty()) {
                Writer.Fill<int64_t>("EventHeadersOffset" + Idx, Writer.Position());
                for (size_t E = 0; E < A.Events.size(); ++E) {
                    EventHeaderOffsets.push_back(Writer.Position());
                    Writer.WriteInt64(TimeOffsets.at(A.Events[E].StartTime));
                    Writer.WriteInt64(TimeOffsets.at(A.Events[E].EndTime));
                    Writer.Reserve<int64_t>("EventDataOffset" + Idx + ":" + std::to_string(E));
                }
            } else {
                Writer.Fill<int64_t>("EventHeadersOffset" + Idx, 0);
            }

            // Event data.
            for (size_t E = 0; E < A.Events.size(); ++E) {
                Writer.Fill<int64_t>("EventDataOffset" + Idx + ":" + std::to_string(E), Writer.Position());
                Writer.WriteUInt64(static_cast<uint64_t>(A.Events[E].Type()));
                Writer.WriteInt64(Writer.Position() + 8);
                WriteTAE3EventData(A.Events[E].Data, Writer);
                Writer.Align(0x10);
            }

            // Event group headers.
            if (!A.EventGroups.empty()) {
                Writer.Fill<int64_t>("EventGroupHeadersOffset" + Idx, Writer.Position());
                for (size_t G = 0; G < A.EventGroups.size(); ++G) {
                    Writer.WriteInt64(static_cast<int64_t>(A.EventGroups[G].Indices.size()));
                    Writer.Reserve<int64_t>("EventGroupValuesOffset" + Idx + ":" + std::to_string(G));
                    Writer.Reserve<int64_t>("EventGroupTypeOffset" + Idx + ":" + std::to_string(G));
                    Writer.WriteInt64(0);
                }
            } else {
                Writer.Fill<int64_t>("EventGroupHeadersOffset" + Idx, 0);
            }

            // Event group data.
            for (size_t G = 0; G < A.EventGroups.size(); ++G) {
                const std::string Key = Idx + ":" + std::to_string(G);
                Writer.Fill<int64_t>("EventGroupTypeOffset" + Key, Writer.Position());
                Writer.WriteUInt64(static_cast<uint64_t>(A.EventGroups[G].Type));
                Writer.WriteInt64(0);
                Writer.Fill<int64_t>("EventGroupValuesOffset" + Key, Writer.Position());
                for (const int32_t Index : A.EventGroups[G].Indices) {
                    Writer.WriteInt32(static_cast<int32_t>(EventHeaderOffsets.at(static_cast<size_t>(Index))));
                }
                Writer.Align(0x10);
            }
        }
        Writer.Fill<int32_t>("FileSize", static_cast<int32_t>(Writer.Position()));
    }
}  // namespace Souls
