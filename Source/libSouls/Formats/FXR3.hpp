/// @file
/// @author Jake Rieger
/// @date 10/8/2026

#pragma once

#include <libSouls/SoulsFile.hpp>

#include <vector>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of an exported class; only touched by this DLL's own code

    /// @brief FXR3: an SFX (particle effect) definition file used in Dark Souls III and Sekiro.
    ///
    /// Extension: .fxr. Most of the structure is not understood yet; the nested types keep their section numbers from
    /// the file.
    class SOULS_API FXR3 : public SoulsFile<FXR3> {
    public:
        /// @brief Which game's layout the file uses.
        enum class FXRVersion : uint16_t {
            DarkSouls3 = 4,
            Sekiro     = 5,
        };

        /// @brief Unknown; the third section of the file.
        struct Section3 {
            /// @brief Unknown.
            int32_t Unk08 = 0;
            /// @brief Unknown.
            int32_t Unk10 = 0;
            /// @brief Unknown.
            int32_t Unk38 = 0;
            /// @brief Unknown data from section 11.
            int32_t Section11Data1 = 0;
            /// @brief Unknown data from section 11.
            int32_t Section11Data2 = 0;
        };

        /// @brief Unknown; the second section of the file.
        struct Section2 {
            /// @brief Unknown.
            std::vector<Section3> Section3s;
        };

        /// @brief Unknown; the first section of the file.
        struct Section1 {
            /// @brief Unknown.
            std::vector<Section2> Section2s;
        };

        /// @brief Unknown; the ninth section of the file.
        struct Section9 {
            /// @brief Unknown.
            int32_t Unk04 = 0;
            /// @brief Unknown section 11 data.
            std::vector<int32_t> Section11s;
        };

        /// @brief Unknown; the eighth section of the file.
        struct Section8 {
            /// @brief Unknown.
            int16_t Unk00 = 0;
            /// @brief Unknown.
            int32_t Unk04 = 0;
            /// @brief Unknown.
            std::vector<Section9> Section9s;
            /// @brief Unknown section 11 data.
            std::vector<int32_t> Section11s;
        };

        /// @brief A property of an effect (the file's seventh section); not understood.
        struct FFXProperty {
            /// @brief Unknown.
            int16_t Unk00 = 0;
            /// @brief Unknown.
            int32_t Unk04 = 0;
            /// @brief Unknown.
            std::vector<Section8> Section8s;
            /// @brief Unknown section 11 data.
            std::vector<int32_t> Section11s;
        };

        /// @brief Unknown; the tenth section of the file.
        struct Section10 {
            /// @brief Unknown section 11 data.
            std::vector<int32_t> Section11s;
        };

        /// @brief Something that draws (the file's sixth section); not understood.
        struct FFXDrawEntityHost {
            /// @brief Unknown.
            int16_t Unk00 = 0;
            /// @brief Unknown.
            bool Unk02    = false;
            /// @brief Unknown.
            bool Unk03    = false;
            /// @brief Unknown.
            int32_t Unk04 = 0;
            /// @brief Unknown.
            std::vector<FFXProperty> Properties1;
            /// @brief Unknown.
            std::vector<FFXProperty> Properties2;
            /// @brief Unknown.
            std::vector<Section10> Section10s;
            /// @brief Unknown section 11 data.
            std::vector<int32_t> Section11s1;
            /// @brief Unknown section 11 data.
            std::vector<int32_t> Section11s2;
        };

        /// @brief Unknown; the fifth section of the file.
        struct Section5 {
            /// @brief Unknown.
            int16_t Unk00 = 0;
            /// @brief Unknown.
            std::vector<FFXDrawEntityHost> Section6s;
        };

        /// @brief Unknown; the fourth section of the file, which can nest.
        struct Section4 {
            /// @brief Unknown.
            int16_t Unk00 = 0;
            /// @brief Nested section 4s.
            std::vector<Section4> Section4s;
            /// @brief Unknown.
            std::vector<Section5> Section5s;
            /// @brief Unknown.
            std::vector<FFXDrawEntityHost> Section6s;
        };

        /// @brief The game the file is from.
        FXRVersion Version = FXRVersion::DarkSouls3;
        /// @brief The effect's ID.
        int32_t ID         = 0;
        /// @brief Unknown.
        Section1 Section1Tree;
        /// @brief Unknown.
        Section4 Section4Tree;
        /// @brief Only present in Sekiro.
        std::vector<int32_t> Section12s;
        /// @brief Unknown; only present in Sekiro.
        std::vector<int32_t> Section13s;

    protected:
        bool IsImpl(BinaryReader& Reader) override;
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
