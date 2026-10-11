/// @file
/// @author Jake Rieger
/// @date 10/8/2026

#pragma once

#include <libSouls/SoulsFile.hpp>

#include <optional>
#include <string>
#include <vector>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of an exported class; only touched by this DLL's own code

    /// @brief ACB: a rendering configuration file for various game assets, only used in Dark Souls II.
    ///
    /// Extension: .acb
    class SOULS_API ACB : public SoulsFile<ACB> {
    public:
        /// @brief The specific type of an asset.
        enum class AssetType : uint16_t {
            PWV       = 0,
            General   = 1,  ///< miscellaneous assets including collisions and lighting configs
            Model     = 2,  ///< rendering options for 3D models
            Texture   = 3,  ///< diffuse, normal and specular maps
            GITexture = 4,  ///< lightmaps and envmaps
            Motion    = 5,  ///< animation files used in cutscenes
        };

        /// @brief Unknown item of a model's member list.
        struct Member {
            /// @brief Unknown.
            std::string Text;
            /// @brief Unknown.
            int32_t Unk04 = 0;
        };

        /// @brief Unknown collection of unknown items.
        struct MemberList {
            /// @brief Unknown; usually -1.
            int16_t Unk00 = -1;
            /// @brief The members of the list.
            std::vector<Member> Members;
        };

        /// @brief Rendering options for 3D models (only meaningful for Model assets).
        struct ModelOptions {
            /// @brief 0 for objects and characters, 1 for map pieces.
            int16_t Unk0A = 0;
            /// @brief Unknown; may be absent.
            std::optional<MemberList> Members;
            /// @brief Distance at which the model becomes invisible.
            int32_t DrawDistance = 0;
            /// @brief Indirectly determines when LOD face sets are used; observed values 0-3.
            int16_t MeshLodRate = 0;
            /// @brief Whether the model appears in reflective surfaces like water.
            bool Reflectible = true;
            /// @brief Enables interaction normals for water.
            bool NormalInteraction = false;
            /// @brief Unknown.
            int32_t Unk20          = 0;
            /// @brief Unknown; alters rendering mode somehow.
            uint8_t RenderType = 0;
            /// @brief If true, the model does not cast shadows.
            bool DisableShadowSource = false;
            /// @brief If true, shadows will not be cast on the model.
            bool DisableShadowTarget = false;
            /// @brief Unknown; makes things render in reverse order or reverses culling or something.
            bool Unk27  = false;
            /// @brief Unknown.
            float Unk28 = 0;
            /// @brief Unknown.
            bool Unk2C  = false;
            /// @brief If true, the model is always centered on the camera position.
            ///
            /// Used for skyboxes.
            bool FixToCamera = false;
            /// @brief Unknown.
            bool Unk2E       = false;
            /// @brief Distance at which low textures are used.
            int16_t LowTextureDistance = 0;
            /// @brief Distance at which the model uses simplified rendering.
            int16_t CheapRenderDistance = 0;
            /// @brief Unknown.
            uint8_t Unk34               = 0;
            /// @brief Disables lighting on water/transparencies.
            bool Unk35 = false;
            /// @brief Unknown.
            bool Unk36 = false;
            /// @brief Unknown.
            bool Unk37 = false;
        };

        /// @brief A model, texture, or miscellaneous asset configuration.
        struct Asset {
            /// @brief The specific type of this asset.
            AssetType Type = AssetType::General;
            /// @brief Full network path to the source file.
            std::string AbsolutePath;
            /// @brief Relative path to the source file.
            std::string RelativePath;
            /// @brief GITexture only; probably 4 bytes.
            int32_t Unk10 = 0;
            /// @brief Model only.
            ModelOptions Model;
        };

        /// @brief True for PS3/X360, false otherwise.
        bool BigEndian = false;
        /// @brief Assets configured by this ACB.
        std::vector<Asset> Assets;

    protected:
        bool IsImpl(BinaryReader& Reader) override;
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
