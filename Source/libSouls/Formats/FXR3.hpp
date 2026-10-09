//
// Created by Jake Rieger on 10/8/2026.
//

#pragma once

#include <libSouls/SoulsFile.hpp>

#include <vector>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of an exported class; only touched by this DLL's own code

    // FXR3: an SFX (particle effect) definition file used in Dark Souls III and Sekiro. Extension: .fxr. Most of the
    // structure is not understood yet; the nested types keep their section numbers from the file.
    class SOULS_API FXR3 : public SoulsFile<FXR3> {
    public:
        enum class FXRVersion : uint16_t {
            DarkSouls3 = 4,
            Sekiro     = 5,
        };

        struct Section3 {
            int32_t Unk08 = 0;
            int32_t Unk10 = 0;
            int32_t Unk38 = 0;
            int32_t Section11Data1 = 0;
            int32_t Section11Data2 = 0;
        };

        struct Section2 {
            std::vector<Section3> Section3s;
        };

        struct Section1 {
            std::vector<Section2> Section2s;
        };

        struct Section9 {
            int32_t Unk04 = 0;
            std::vector<int32_t> Section11s;
        };

        struct Section8 {
            int16_t Unk00 = 0;
            int32_t Unk04 = 0;
            std::vector<Section9> Section9s;
            std::vector<int32_t> Section11s;
        };

        struct FFXProperty {
            int16_t Unk00 = 0;
            int32_t Unk04 = 0;
            std::vector<Section8> Section8s;
            std::vector<int32_t> Section11s;
        };

        struct Section10 {
            std::vector<int32_t> Section11s;
        };

        struct FFXDrawEntityHost {
            int16_t Unk00 = 0;
            bool Unk02    = false;
            bool Unk03    = false;
            int32_t Unk04 = 0;
            std::vector<FFXProperty> Properties1;
            std::vector<FFXProperty> Properties2;
            std::vector<Section10> Section10s;
            std::vector<int32_t> Section11s1;
            std::vector<int32_t> Section11s2;
        };

        struct Section5 {
            int16_t Unk00 = 0;
            std::vector<FFXDrawEntityHost> Section6s;
        };

        struct Section4 {
            int16_t Unk00 = 0;
            std::vector<Section4> Section4s;
            std::vector<Section5> Section5s;
            std::vector<FFXDrawEntityHost> Section6s;
        };

        FXRVersion Version = FXRVersion::DarkSouls3;
        int32_t ID         = 0;
        Section1 Section1Tree;
        Section4 Section4Tree;
        // Only present in Sekiro.
        std::vector<int32_t> Section12s;
        std::vector<int32_t> Section13s;

    protected:
        bool IsImpl(BinaryReader& Reader) override;
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
