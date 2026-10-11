/// @file
/// @author Jake Rieger
/// @date 10/8/2026

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

    /// @brief MQB: a cutscene format used in Dark Souls II and III (and Bloodborne).
    ///
    /// Extension: .mqb
    class SOULS_API MQB : public SoulsFile<MQB> {
    public:
        /// @brief Which game's layout the file uses.
        enum class MQBVersion : uint32_t {
            DarkSouls2        = 0x94,
            DarkSouls2Scholar = 0xCA,
            Bloodborne        = 0xCB,
            DarkSouls3        = 0xCC,
        };

        /// @brief A named, typed value that can carry animation sequences.
        struct SOULS_API CustomData {
            /// @brief The type of a custom data value.
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

            /// @brief The value of a custom data entry; which alternative is active depends on Type.
            using Value_t = std::variant<bool, int8_t, uint8_t, int16_t, int32_t, uint32_t, float, std::string, std::vector<uint8_t>, Souls::Color>;

            /// @brief A single keyed value in a sequence.
            struct Point {
                /// @brief The value: a byte or a float, matching the sequence's ValueType.
                std::variant<uint8_t, float> Value = uint8_t{0};
                /// @brief Unknown.
                int32_t Unk08                      = 0;
                /// @brief Unknown.
                float Unk10 = 0;
                /// @brief Unknown.
                float Unk14 = 0;
            };

            /// @brief A series of keyed values for one part of the data (a byte of it, or the float).
            struct Sequence {
                /// @brief Whether the points hold bytes or floats.
                DataType ValueType = DataType::Byte;
                /// @brief 1 or 2; decides the size of each point (0x10 or 0x18 bytes).
                int32_t PointType  = 1;
                /// @brief Which byte of the parent value the sequence animates (Byte sequences only).
                int32_t ValueIndex = 0;
                /// @brief The keyed values.
                std::vector<Point> Points;
            };

            /// @brief The data's name.
            std::string Name;
            /// @brief The type of the value.
            DataType Type = DataType::Int;
            /// @brief The value; the alternative matches Type.
            Value_t Value = int32_t{0};
            /// @brief Animation of the value over time.
            std::vector<Sequence> Sequences;
        };

        /// @brief An object used by the cutscene.
        struct Resource {
            /// @brief The resource's name.
            std::string Name;
            /// @brief The index of the parent resource.
            int32_t ParentIndex = 0;
            /// @brief Unknown.
            int32_t Unk48       = 0;
            /// @brief The resource's custom data.
            std::vector<MQB::CustomData> CustomData;
            /// @brief Where the resource is loaded from; absent if it has no path.
            std::optional<std::string> Path;
        };

        /// @brief Position, rotation and scale at a frame.
        struct Transform {
            /// @brief The frame this transform applies at.
            float Frame = 0;
            /// @brief The position.
            Vector3 Translation;
            /// @brief Unknown.
            Vector3 Unk10;
            /// @brief Unknown.
            Vector3 Unk1C;
            /// @brief The rotation.
            Vector3 Rotation;
            /// @brief Unknown.
            Vector3 Unk34;
            /// @brief Unknown.
            Vector3 Unk40;
            /// @brief The scale.
            Vector3 Scale{1.f, 1.f, 1.f};
            /// @brief Unknown.
            Vector3 Unk58;
            /// @brief Unknown.
            Vector3 Unk64;
        };

        /// @brief A resource's presence during a stretch of a cut.
        struct Disposition {
            /// @brief The disposition's ID.
            int32_t ID            = 0;
            /// @brief The resource this disposition places, by its index in Resources.
            int32_t ResourceIndex = 0;
            /// @brief Unknown; possibly a timeline index.
            int32_t Unk08 = 0;
            /// @brief The frame the disposition starts at.
            int32_t StartFrame = 0;
            /// @brief Duration in frames.
            int32_t Duration   = 0;
            /// @brief Unknown.
            int32_t Unk14 = 0;
            /// @brief Unknown.
            int32_t Unk18 = 0;
            /// @brief Unknown.
            int32_t Unk1C = 0;
            int32_t Unk20 = 0;  ///< 0 or 1
            /// @brief The disposition's custom data.
            std::vector<MQB::CustomData> CustomData;
            /// @brief Unknown.
            int32_t Unk28 = 0;
            /// @brief Where the resource is over time.
            std::vector<Transform> Transforms;
        };

        /// @brief A track of dispositions within a cut.
        struct Timeline {
            /// @brief The dispositions on this timeline.
            std::vector<Disposition> Dispositions;
            /// @brief The timeline's custom data.
            std::vector<MQB::CustomData> CustomData;
            /// @brief Unknown; possibly a timeline index.
            int32_t Unk10 = 0;
        };

        /// @brief A shot of the cutscene.
        struct Cut {
            /// @brief The cut's name.
            std::string Name;
            /// @brief Unknown.
            int32_t Unk44    = 0;
            /// @brief Duration of the cut in frames.
            int32_t Duration = 0;
            /// @brief The cut's timelines.
            std::vector<Timeline> Timelines;
        };

        /// @brief Whether the file is big-endian (console versions).
        bool BigEndian       = false;
        /// @brief The game the file is from.
        MQBVersion Version   = MQBVersion::DarkSouls3;
        /// @brief The cutscene's name.
        std::string Name;
        /// @brief Frames per second.
        float Framerate = 30;
        /// @brief The objects the cutscene uses.
        std::vector<Resource> Resources;
        /// @brief The shots, in order.
        std::vector<Cut> Cuts;
        /// @brief The folder the resources are loaded from.
        std::string ResourceDirectory;

    protected:
        bool IsImpl(BinaryReader& Reader) override;
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
