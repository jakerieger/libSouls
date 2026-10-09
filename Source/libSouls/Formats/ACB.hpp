//
// Created by Jake Rieger on 10/8/2026.
//

#pragma once

#include <libSouls/SoulsFile.hpp>

#include <optional>
#include <string>
#include <vector>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of an exported class; only touched by this DLL's own code

    // ACB: a rendering configuration file for various game assets, only used in Dark Souls II. Extension: .acb
    class SOULS_API ACB : public SoulsFile<ACB> {
    public:
        // The specific type of an asset.
        enum class AssetType : uint16_t {
            PWV       = 0,
            General   = 1,  // miscellaneous assets including collisions and lighting configs
            Model     = 2,  // rendering options for 3D models
            Texture   = 3,  // diffuse, normal and specular maps
            GITexture = 4,  // lightmaps and envmaps
            Motion    = 5,  // animation files used in cutscenes
        };

        // Unknown item of a model's member list.
        struct Member {
            std::string Text;
            int32_t Unk04 = 0;
        };

        // Unknown collection of unknown items.
        struct MemberList {
            // Unknown; usually -1.
            int16_t Unk00 = -1;
            std::vector<Member> Members;
        };

        // Rendering options for 3D models (only meaningful for Model assets).
        struct ModelOptions {
            // 0 for objects and characters, 1 for map pieces.
            int16_t Unk0A = 0;
            // Unknown; may be absent.
            std::optional<MemberList> Members;
            // Distance at which the model becomes invisible.
            int32_t DrawDistance = 0;
            // Indirectly determines when LOD face sets are used; observed values 0-3.
            int16_t MeshLodRate = 0;
            // Whether the model appears in reflective surfaces like water.
            bool Reflectible = true;
            // Enables interaction normals for water.
            bool NormalInteraction = false;
            int32_t Unk20          = 0;
            // Unknown; alters rendering mode somehow.
            uint8_t RenderType = 0;
            // If true, the model does not cast shadows.
            bool DisableShadowSource = false;
            // If true, shadows will not be cast on the model.
            bool DisableShadowTarget = false;
            // Unknown; makes things render in reverse order or reverses culling or something.
            bool Unk27  = false;
            float Unk28 = 0;
            bool Unk2C  = false;
            // If true, the model is always centered on the camera position. Used for skyboxes.
            bool FixToCamera = false;
            bool Unk2E       = false;
            // Distance at which low textures are used.
            int16_t LowTextureDistance = 0;
            // Distance at which the model uses simplified rendering.
            int16_t CheapRenderDistance = 0;
            uint8_t Unk34               = 0;
            // Disables lighting on water/transparencies.
            bool Unk35 = false;
            bool Unk36 = false;
            bool Unk37 = false;
        };

        // A model, texture, or miscellaneous asset configuration.
        struct Asset {
            AssetType Type = AssetType::General;
            // Full network path to the source file.
            std::string AbsolutePath;
            // Relative path to the source file.
            std::string RelativePath;
            // GITexture only; probably 4 bytes.
            int32_t Unk10 = 0;
            // Model only.
            ModelOptions Model;
        };

        // True for PS3/X360, false otherwise.
        bool BigEndian = false;
        std::vector<Asset> Assets;

    protected:
        bool IsImpl(BinaryReader& Reader) override;
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
