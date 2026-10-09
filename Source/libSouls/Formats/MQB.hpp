//
// Created by Jake Rieger on 10/8/2026.
//

#pragma once

#include <libSouls/Color.hpp>
#include <libSouls/SoulsFile.hpp>
#include <libSouls/Vector.hpp>

#include <string>
#include <variant>
#include <vector>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of exported classes; only touched by this DLL's own code

    // MQB: a cutscene format used in Dark Souls II and III (and Bloodborne). Extension: .mqb
    class SOULS_API MQB : public SoulsFile<MQB> {
    public:
        enum class MQBVersion : uint32_t {
            DarkSouls2        = 0x94,
            DarkSouls2Scholar = 0xCA,
            Bloodborne        = 0xCB,
            DarkSouls3        = 0xCC,
        };

        // A named, typed value that can carry animation sequences.
        struct SOULS_API CustomData {
            enum class DataType : uint32_t {
                Bool   = 1,
                SByte  = 2,
                Byte   = 3,
                Short  = 4,
                Int    = 6,
                UInt   = 7,
                Float  = 8,
                String = 10,
                Custom = 11,
                Color  = 13,
            };

            // The value of a custom data entry; which alternative is active depends on Type.
            using Value_t = std::variant<bool, int8_t, uint8_t, int16_t, int32_t, uint32_t, float, std::string, std::vector<uint8_t>, Souls::Color>;

            // A single keyed value in a sequence.
            struct Point {
                std::variant<uint8_t, float> Value = uint8_t{0};
                int32_t Unk08                      = 0;
                float Unk10 = 0, Unk14 = 0;
            };

            // A series of keyed values for one part of the data (a byte of it, or the float).
            struct Sequence {
                DataType ValueType = DataType::Byte;
                int32_t PointType  = 1;
                // Which byte of the parent value the sequence animates (Byte sequences only).
                int32_t ValueIndex = 0;
                std::vector<Point> Points;
            };

            std::string Name;
            DataType Type = DataType::Int;
            Value_t Value = int32_t{0};
            std::vector<Sequence> Sequences;
        };

        // An object used by the cutscene.
        struct Resource {
            std::string Name;
            int32_t ParentIndex = 0;
            int32_t Unk48       = 0;
            std::vector<MQB::CustomData> CustomData;
            // Where the resource is loaded from; absent if it has no path.
            std::optional<std::string> Path;
        };

        // Position, rotation and scale at a frame.
        struct Transform {
            float Frame = 0;
            Vector3 Translation, Unk10, Unk1C, Rotation, Unk34, Unk40;
            Vector3 Scale{1.f, 1.f, 1.f};
            Vector3 Unk58, Unk64;
        };

        // A resource's presence during a stretch of a cut.
        struct Disposition {
            int32_t ID            = 0;
            int32_t ResourceIndex = 0;
            int32_t Unk08 = 0;
            int32_t StartFrame = 0;
            int32_t Duration   = 0;
            int32_t Unk14 = 0, Unk18 = 0, Unk1C = 0;
            int32_t Unk20 = 0;  // 0 or 1
            std::vector<MQB::CustomData> CustomData;
            int32_t Unk28 = 0;
            std::vector<Transform> Transforms;
        };

        struct Timeline {
            std::vector<Disposition> Dispositions;
            std::vector<MQB::CustomData> CustomData;
            int32_t Unk10 = 0;
        };

        struct Cut {
            std::string Name;
            int32_t Unk44    = 0;
            int32_t Duration = 0;
            std::vector<Timeline> Timelines;
        };

        bool BigEndian       = false;
        MQBVersion Version   = MQBVersion::DarkSouls3;
        std::string Name;
        float Framerate = 30;
        std::vector<Resource> Resources;
        std::vector<Cut> Cuts;
        std::string ResourceDirectory;

    protected:
        bool IsImpl(BinaryReader& Reader) override;
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
