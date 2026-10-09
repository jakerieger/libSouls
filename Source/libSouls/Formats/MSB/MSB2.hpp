//
// Created by Jake Rieger on 10/8/2026.
//

#pragma once

#include "MSBCommon.hpp"
#include "MSBParam.hpp"

#include <libSouls/Color.hpp>
#include <libSouls/SoulsFile.hpp>

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of exported classes; only touched by this DLL's own code

    // MSB2: the map layout file of Dark Souls II, both the original and Scholar of the First Sin. Extension: .msb
    //
    // The original game's files use 32-bit offsets and the Scholar's use 64-bit offsets. Entries refer to each other
    // by name.
    class SOULS_API MSB2 : public SoulsFile<MSB2> {
    public:
        using ModelList     = Msb::ModelList;
        using EventList     = Msb::EventList;
        using RegionList    = Msb::RegionList;
        using PartList      = Msb::PartList;
        using CollisionList = Msb::CollisionList;
        using BoneNameList  = Msb::BoneNameList;

        // The different formats of Dark Souls II MSBs.
        enum class MSBFormat {
            DarkSouls2LE,       // 32-bit little-endian format for the original game on PC
            DarkSouls2BE,       // 32-bit big-endian format for the original game on consoles
            DarkSouls2Scholar,  // 64-bit format for Scholar of the First Sin on all platforms
        };

        // A generic entry with a name.
        struct Entry {
            std::string Name;
            virtual ~Entry() = default;
        };

        // All entries of the file, in file order, for resolving references between them.
        struct Entries {
            std::vector<Entry*> Models, Events, Regions, Parts, Collisions, BoneNames;

            template<typename Tag>
            const std::vector<Entry*>& List() const {
                if constexpr (std::is_same_v<Tag, ModelList>) {
                    return Models;
                } else if constexpr (std::is_same_v<Tag, EventList>) {
                    return Events;
                } else if constexpr (std::is_same_v<Tag, RegionList>) {
                    return Regions;
                } else if constexpr (std::is_same_v<Tag, PartList>) {
                    return Parts;
                } else if constexpr (std::is_same_v<Tag, CollisionList>) {
                    return Collisions;
                } else {
                    return BoneNames;
                }
            }
        };

#pragma region Models
        // A model file available for parts to reference.
        struct SOULS_API Model : Entry {
            virtual uint32_t Type() const    = 0;
            virtual bool HasTypeData() const = 0;
            virtual void ReadTypeData(BinaryReader&) {}
            virtual void WriteTypeData(BinaryWriter&) {}
            void Read(BinaryReader& Reader);
            void Write(BinaryWriter& Writer, int32_t Index);
        };

        template<typename D, uint32_t Id, bool WithData>
        struct ModelImpl : Model {
            static constexpr uint32_t TypeId = Id;
            uint32_t Type() const override { return Id; }
            bool HasTypeData() const override { return WithData; }
            void ReadTypeData(BinaryReader& Reader) override {
                if constexpr (WithData) {
                    Msb::Detail::ReadVisitor V{Reader};
                    static_cast<D*>(this)->Fields(V);
                }
            }
            void WriteTypeData(BinaryWriter& Writer) override {
                if constexpr (WithData) {
                    Msb::Detail::WriteVisitor V{Writer};
                    static_cast<D*>(this)->Fields(V);
                }
            }
        };

        // Models available for parts in the map to use.
        struct ModelParam : Msb::Detail::TypedLists<ModelParam, Model> {
            // A model for a static piece of visual map geometry.
            struct MapPiece : ModelImpl<MapPiece, 0, false> {
                MapPiece() { Name = "mXXXX"; }
            };
            // A model for a dynamic or interactible part.
            struct Object : ModelImpl<Object, 1, true> {
                Object() { Name = "oXX_XXXX"; }
                template<typename V>
                void Fields(V& F) {
                    F.ConstVarint(0);
                }
            };
            // A model for a static piece of physical map geometry.
            struct Collision : ModelImpl<Collision, 3, false> {
                Collision() { Name = "hXX_XXXX"; }
            };
            // A model for an AI navigation mesh.
            struct Navmesh : ModelImpl<Navmesh, 4, false> {
                Navmesh() { Name = "nXX_XXXX"; }
            };

            std::vector<MapPiece> MapPieces;
            std::vector<Object> Objects;
            std::vector<Collision> Collisions;
            std::vector<Navmesh> Navmeshes;

            static constexpr int32_t ParamVersion  = 5;
            static constexpr const char* ParamName = "MODEL_PARAM_ST";

            template<typename F>
            void ForEachList(F&& Visit) {
                Visit(MapPieces);
                Visit(Objects);
                Visit(Collisions);
                Visit(Navmeshes);
            }
            template<typename F>
            void ForEachList(F&& Visit) const {
                Visit(MapPieces);
                Visit(Objects);
                Visit(Collisions);
                Visit(Navmeshes);
            }
        };
#pragma endregion

#pragma region Events
        // An abstract entity that controls map properties or behaviors.
        struct SOULS_API Event : Entry {
            // Uniquely identifies the event in the map.
            int32_t EventID = -1;

            virtual uint32_t Type() const = 0;
            void Read(BinaryReader& Reader);
            void Write(BinaryWriter& Writer, int32_t Index);
            virtual void ReadTypeData(BinaryReader& Reader)  = 0;
            virtual void WriteTypeData(BinaryWriter& Writer) = 0;
        };

        template<typename D, uint32_t Id>
        struct EventImpl : Event {
            static constexpr uint32_t TypeId = Id;
            uint32_t Type() const override { return Id; }
            void ReadTypeData(BinaryReader& Reader) override {
                Msb::Detail::ReadVisitor V{Reader};
                static_cast<D*>(this)->Fields(V);
            }
            void WriteTypeData(BinaryWriter& Writer) override {
                Msb::Detail::WriteVisitor V{Writer};
                static_cast<D*>(this)->Fields(V);
            }
        };

        // Abstract entities that control map properties or behaviors.
        struct EventParam : Msb::Detail::TypedLists<EventParam, Event> {
            // Unknown if this does anything.
            struct Light : EventImpl<Light, 1> {
                uint8_t UnkT00 = 0;
                float UnkT04 = 0, UnkT08 = 0;
                Color ColorT0C, ColorT10;
                float UnkT1C = 0, UnkT20 = 0;
                Color ColorT24, ColorT28, ColorT34, ColorT38, ColorT3C;
                float UnkT40   = 0;
                uint8_t UnkT44 = 0;
                Light() { Name = "Event: Light"; }
                template<typename V>
                void Fields(V& F) {
                    F(UnkT00);
                    F.template Const<uint8_t>(0);
                    F.template Const<int16_t>(-1);
                    F(UnkT04);
                    F(UnkT08);
                    F.RGBA(ColorT0C);
                    F.RGBA(ColorT10);
                    F.Pad(8);
                    F(UnkT1C);
                    F(UnkT20);
                    F.RGBA(ColorT24);
                    F.RGBA(ColorT28);
                    F.Pad(8);
                    F.RGBA(ColorT34);
                    F.RGBA(ColorT38);
                    F.RGBA(ColorT3C);
                    F(UnkT40);
                    F(UnkT44);
                    F.Pad(0x3B);
                }
            };

            // Unknown if this does anything.
            struct Shadow : EventImpl<Shadow, 2> {
                float UnkT04 = 0, UnkT08 = 0, UnkT0C = 0, UnkT14 = 0, UnkT18 = 0, UnkT20 = 0;
                Color ColorT24;
                Shadow() { Name = "Event: Shadow"; }
                template<typename V>
                void Fields(V& F) {
                    F.Pad(4);
                    F(UnkT04);
                    F(UnkT08);
                    F(UnkT0C);
                    F.Pad(4);
                    F(UnkT14);
                    F(UnkT18);
                    F.Pad(4);
                    F(UnkT20);
                    F.RGBA(ColorT24);
                    F.Pad(0x18);
                }
            };

            // Unknown if this does anything.
            struct Fog : EventImpl<Fog, 3> {
                uint8_t UnkT00 = 0;
                Color ColorT04;
                float UnkT08 = 0, UnkT0C = 0, UnkT10 = 0;
                uint8_t UnkT14 = 0, UnkT15 = 0, UnkT16 = 0;
                Fog() { Name = "Event: Fog"; }
                template<typename V>
                void Fields(V& F) {
                    F(UnkT00);
                    F.Pad(3);
                    F.RGBA(ColorT04);
                    F(UnkT08);
                    F(UnkT0C);
                    F(UnkT10);
                    F(UnkT14);
                    F(UnkT15);
                    F(UnkT16);
                    F.Pad(0x11);
                }
            };

            // Sets the background color when no models are in the way. Should only be one per map.
            struct BGColor : EventImpl<BGColor, 4> {
                Color Color_;
                BGColor() { Name = "Event: BGColor"; }
                template<typename V>
                void Fields(V& F) {
                    F.RGBA(Color_);
                    F.Pad(0x24);
                }
            };

            // Sets the origin of the map; already factored into MSB positions, but affects BTL. Should only be one per map.
            struct MapOffset : EventImpl<MapOffset, 5> {
                Vector3 Translation;
                MapOffset() { Name = "Event: MapOffset"; }
                template<typename V>
                void Fields(V& F) {
                    F(Translation);
                    F.Pad(4);
                }
            };

            // Unknown exactly what this is for.
            struct Warp : EventImpl<Warp, 6> {
                uint8_t UnkT00 = 0;
                // Presumably the position to be warped to.
                Vector3 Position;
                Warp() { Name = "Event: Warp"; }
                template<typename V>
                void Fields(V& F) {
                    F(UnkT00);
                    F.Pad(3);
                    F(Position);
                }
            };

            // Unknown if this does anything.
            struct CheapMode : EventImpl<CheapMode, 7> {
                int16_t UnkT00 = 0;
                CheapMode() { Name = "Event: CheapMode"; }
                template<typename V>
                void Fields(V& F) {
                    F(UnkT00);
                    F.Pad(0xE);
                }
            };

            std::vector<Light> Lights;
            std::vector<Shadow> Shadows;
            std::vector<Fog> Fogs;
            std::vector<BGColor> BGColors;
            std::vector<MapOffset> MapOffsets;
            std::vector<Warp> Warps;
            std::vector<CheapMode> CheapModes;

            static constexpr int32_t ParamVersion  = 5;
            static constexpr const char* ParamName = "EVENT_PARAM_ST";

            template<typename F>
            void ForEachList(F&& Visit) {
                Visit(Lights);
                Visit(Shadows);
                Visit(Fogs);
                Visit(BGColors);
                Visit(MapOffsets);
                Visit(Warps);
                Visit(CheapModes);
            }
            template<typename F>
            void ForEachList(F&& Visit) const {
                Visit(Lights);
                Visit(Shadows);
                Visit(Fogs);
                Visit(BGColors);
                Visit(MapOffsets);
                Visit(Warps);
                Visit(CheapModes);
            }
        };
#pragma endregion

#pragma region Regions
        // A point or volume that triggers some behavior.
        struct SOULS_API Region : Entry {
            int16_t Unk08 = 0;
            // Describes the space encompassed by the region. Composite shapes aren't supported in Dark Souls II.
            Msb::Shape Shape = Msb::Shapes::Point{};
            int16_t Unk0E    = 0;
            Vector3 Position;
            // Rotation of the region, in degrees.
            Vector3 Rotation;

            virtual uint32_t Type() const    = 0;
            virtual bool HasTypeData() const = 0;
            void Read(BinaryReader& Reader);
            void Write(BinaryWriter& Writer, int32_t Index);
            virtual void ReadTypeData(BinaryReader&) {}
            virtual void WriteTypeData(BinaryWriter&) {}
        };

        template<typename D, uint32_t Id, bool WithData>
        struct RegionImpl : Region {
            static constexpr uint32_t TypeId = Id;
            uint32_t Type() const override { return Id; }
            bool HasTypeData() const override { return WithData; }
            void ReadTypeData(BinaryReader& Reader) override {
                if constexpr (WithData) {
                    Msb::Detail::ReadVisitor V{Reader};
                    static_cast<D*>(this)->Fields(V);
                }
            }
            void WriteTypeData(BinaryWriter& Writer) override {
                if constexpr (WithData) {
                    Msb::Detail::WriteVisitor V{Writer};
                    static_cast<D*>(this)->Fields(V);
                }
            }
        };

        // Points or volumes that trigger some behavior.
        struct PointParam : Msb::Detail::TypedLists<PointParam, Region> {
            // Unknown, possibly walk points for enemies.
            struct Region0 : RegionImpl<Region0, 0, false> {
                Region0() { Name = "Region: Region0"; }
            };

            // Unknown if this does anything.
            struct Light : RegionImpl<Light, 3, true> {
                int32_t UnkT00 = 0;
                Color ColorT04, ColorT08;
                float UnkT0C = 0;
                Light() { Name = "Region: Light"; }
                template<typename V>
                void Fields(V& F) {
                    F(UnkT00);
                    F.RGBA(ColorT04);
                    F.RGBA(ColorT08);
                    F(UnkT0C);
                    F.Pad(0x10);
                    if (F.Long()) {
                        F.template Const<int32_t>(0);
                    }
                }
            };

            // Unknown, presumably the default spawn location for a map.
            struct StartPoint : RegionImpl<StartPoint, 5, false> {
                StartPoint() { Name = "Region: StartPoint"; }
            };

            // A sound effect that plays in a certain area.
            struct Sound : RegionImpl<Sound, 7, true> {
                int32_t UnkT00 = 0;
                // ID of the sound to play.
                int32_t SoundID = 0;
                int32_t UnkT08  = 0;
                Sound() { Name = "Region: Sound"; }
                template<typename V>
                void Fields(V& F) {
                    F(UnkT00);
                    F(SoundID);
                    F(UnkT08);
                    F.Pad(0x14);
                }
            };

            // A special effect that plays at a certain region.
            struct SFX : RegionImpl<SFX, 9, true> {
                // The effect to play at this region.
                int32_t EffectID = 0;
                int32_t UnkT04   = 0;
                SFX() { Name = "Region: SFX"; }
                template<typename V>
                void Fields(V& F) {
                    F(EffectID);
                    F(UnkT04);
                    F.Pad(0x18);
                }
            };

            // Unknown, presumably sets wind speed/direction.
            struct Wind : RegionImpl<Wind, 13, true> {
                int32_t UnkT00 = 0;
                float UnkT04 = 0, UnkT08 = 0, UnkT0C = 0, UnkT10 = 0, UnkT14 = 0, UnkT18 = 0;
                Wind() { Name = "Region: Wind"; }
                template<typename V>
                void Fields(V& F) {
                    F(UnkT00);
                    F(UnkT04);
                    F(UnkT08);
                    F(UnkT0C);
                    F(UnkT10);
                    F(UnkT14);
                    F(UnkT18);
                    F.Pad(4);
                }
            };

            // Unknown, names mention lightmaps and GI.
            struct EnvLight : RegionImpl<EnvLight, 14, true> {
                int32_t UnkT00 = 0;
                float UnkT04 = 0, UnkT08 = 0;
                EnvLight() { Name = "Region: EnvLight"; }
                template<typename V>
                void Fields(V& F) {
                    F(UnkT00);
                    F(UnkT04);
                    F(UnkT08);
                    F.Pad(0x14);
                }
            };

            // Unknown if this does anything.
            struct Fog : RegionImpl<Fog, 15, true> {
                int32_t UnkT00 = 0, UnkT04 = 0;
                Fog() { Name = "Region: Fog"; }
                template<typename V>
                void Fields(V& F) {
                    F(UnkT00);
                    F(UnkT04);
                    F.Pad(0x18);
                    if (F.Long()) {
                        F.template Const<int32_t>(0);
                    }
                }
            };

            std::vector<Region0> Region0s;
            std::vector<Light> Lights;
            std::vector<StartPoint> StartPoints;
            std::vector<Sound> Sounds;
            std::vector<SFX> SFXs;
            std::vector<Wind> Winds;
            std::vector<EnvLight> EnvLights;
            std::vector<Fog> Fogs;

            static constexpr int32_t ParamVersion  = 5;
            static constexpr const char* ParamName = "POINT_PARAM_ST";

            template<typename F>
            void ForEachList(F&& Visit) {
                Visit(Region0s);
                Visit(Lights);
                Visit(StartPoints);
                Visit(Sounds);
                Visit(SFXs);
                Visit(Winds);
                Visit(EnvLights);
                Visit(Fogs);
            }
            template<typename F>
            void ForEachList(F&& Visit) const {
                Visit(Region0s);
                Visit(Lights);
                Visit(StartPoints);
                Visit(Sounds);
                Visit(SFXs);
                Visit(Winds);
                Visit(EnvLights);
                Visit(Fogs);
            }
        };
#pragma endregion

#pragma region Parts
        // A concrete map element.
        struct SOULS_API Part : Entry {
            // The name of the part's model, referencing ModelParam.
            Msb::Ref<ModelList, int16_t> ModelName;
            Vector3 Position;
            // Rotation of the part, in degrees.
            Vector3 Rotation;
            // Scale of the part; only supported for map pieces and objects.
            Vector3 Scale{1.f, 1.f, 1.f};
            // Not confirmed; determines when the part is loaded.
            std::array<uint32_t, 4> DrawGroups{0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
            // Unknown; possibly nvm groups.
            int32_t Unk44 = 0, Unk48 = 0, Unk4C = 0, Unk50 = 0;
            // Not confirmed; determines when the part is visible.
            std::array<uint32_t, 4> DispGroups{0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
            int32_t Unk64 = 0;
            uint8_t Unk6C = 0;
            uint8_t Unk6E = 0;

            virtual uint32_t Type() const = 0;
            void Read(BinaryReader& Reader);
            void Write(BinaryWriter& Writer, int32_t Index);
            virtual void ReadTypeData(BinaryReader& Reader)  = 0;
            virtual void WriteTypeData(BinaryWriter& Writer) = 0;
            virtual void GetNames(const Entries& Lists);
            virtual void GetIndices(const Entries& Lists);
        };

        template<typename D, uint32_t Id>
        struct PartImpl : Part {
            static constexpr uint32_t TypeId = Id;
            uint32_t Type() const override { return Id; }
            void ReadTypeData(BinaryReader& Reader) override {
                Msb::Detail::ReadVisitor V{Reader};
                static_cast<D*>(this)->Fields(V);
            }
            void WriteTypeData(BinaryWriter& Writer) override {
                Msb::Detail::WriteVisitor V{Writer};
                static_cast<D*>(this)->Fields(V);
            }
            void GetNames(const Entries& Lists) override {
                Part::GetNames(Lists);
                Msb::Detail::NameVisitor<Entries> V{Lists};
                static_cast<D*>(this)->Fields(V);
            }
            void GetIndices(const Entries& Lists) override {
                Part::GetIndices(Lists);
                Msb::Detail::IndexVisitor<Entries> V{Lists};
                static_cast<D*>(this)->Fields(V);
            }
        };

        // Concrete map elements.
        struct PartsParam : Msb::Detail::TypedLists<PartsParam, Part> {
            // A visible but intangible model.
            struct MapPiece : PartImpl<MapPiece, 0> {
                int16_t UnkT00 = 0;
                uint8_t UnkT02 = 0;
                MapPiece() { Name = "mXXXX_XXXX"; }
                template<typename V>
                void Fields(V& F) {
                    F(UnkT00);
                    F(UnkT02);
                    F.template Const<uint8_t>(0);
                    if (F.Long()) {
                        F.template Const<int32_t>(0);
                    }
                }
            };

            // A dynamic or interactible element.
            struct Object : PartImpl<Object, 1> {
                int32_t MapObjectInstanceParamID = 0;
                int16_t UnkT04                   = 0;
                Object() { Name = "oXX_XXXX_XXXX"; }
                template<typename V>
                void Fields(V& F) {
                    F(MapObjectInstanceParamID);
                    F(UnkT04);
                    F.template Const<int16_t>(0);
                    F.Pad(8);
                }
            };

            // An invisible but physical surface that controls map loading and graphics settings, among other things.
            struct Collision : PartImpl<Collision, 3> {
                int32_t UnkT00 = 0, UnkT04 = 0, UnkT08 = 0, UnkT0C = 0;
                uint8_t UnkT10 = 0, UnkT11 = 0, UnkT12 = 0, UnkT13 = 0, UnkT14 = 0, UnkT15 = 0, UnkT17 = 0;
                int32_t UnkT18 = 0, UnkT1C = 0, UnkT20 = 0;
                uint8_t UnkT26 = 0, UnkT27 = 0;
                int32_t UnkT28 = 0;
                uint8_t UnkT2C = 0;
                int16_t UnkT2E = 0;
                int32_t UnkT30 = 0;
                uint8_t UnkT35 = 0;
                int16_t UnkT36 = 0;
                int32_t UnkT3C = 0;
                uint8_t UnkT40 = 0;
                int32_t UnkT44 = 0;
                Collision() { Name = "hXX_XXXX_XXXX"; }
                template<typename V>
                void Fields(V& F) {
                    F(UnkT00);
                    F(UnkT04);
                    F(UnkT08);
                    F(UnkT0C);
                    F(UnkT10);
                    F(UnkT11);
                    F(UnkT12);
                    F(UnkT13);
                    F(UnkT14);
                    F(UnkT15);
                    F.template Const<uint8_t>(0);
                    F(UnkT17);
                    F(UnkT18);
                    F(UnkT1C);
                    F(UnkT20);
                    F.template Const<int16_t>(0);
                    F(UnkT26);
                    F(UnkT27);
                    F(UnkT28);
                    F(UnkT2C);
                    F.template Const<uint8_t>(0);
                    F(UnkT2E);
                    F(UnkT30);
                    F.template Const<uint8_t>(0);
                    F(UnkT35);
                    F(UnkT36);
                    F.template Const<int32_t>(0);
                    F(UnkT3C);
                    F(UnkT40);
                    F.Pad(3);
                    F(UnkT44);
                    F.Pad(0x10);
                }
            };

            // An AI navigation mesh.
            struct Navmesh : PartImpl<Navmesh, 4> {
                // Unknown; possibly nvm groups.
                int32_t UnkT00 = 0, UnkT04 = 0, UnkT08 = 0, UnkT0C = 0;
                Navmesh() { Name = "nXX_XXXX_XXXX"; }
                template<typename V>
                void Fields(V& F) {
                    F(UnkT00);
                    F(UnkT04);
                    F(UnkT08);
                    F(UnkT0C);
                    F.Pad(0x10);
                }
            };

            // Causes another map to be loaded when standing on the referenced collision.
            struct ConnectCollision : PartImpl<ConnectCollision, 5> {
                // Name of the referenced collision part.
                Msb::Ref<CollisionList> CollisionName;
                // The map to load when on this collision.
                std::array<uint8_t, 4> MapID{};
                int32_t UnkT08 = 0;
                uint8_t UnkT0C = 0;
                ConnectCollision() { Name = "hXX_XXXX_XXXX"; }
                template<typename V>
                void Fields(V& F) {
                    F(CollisionName);
                    F(MapID);
                    F(UnkT08);
                    F(UnkT0C);
                    F.Pad(3);
                }
            };

            std::vector<MapPiece> MapPieces;
            std::vector<Object> Objects;
            std::vector<Collision> Collisions;
            std::vector<Navmesh> Navmeshes;
            std::vector<ConnectCollision> ConnectCollisions;

            static constexpr int32_t ParamVersion  = 5;
            static constexpr const char* ParamName = "PARTS_PARAM_ST";

            template<typename F>
            void ForEachList(F&& Visit) {
                Visit(MapPieces);
                Visit(Objects);
                Visit(Collisions);
                Visit(Navmeshes);
                Visit(ConnectCollisions);
            }
            template<typename F>
            void ForEachList(F&& Visit) const {
                Visit(MapPieces);
                Visit(Objects);
                Visit(Collisions);
                Visit(Navmeshes);
                Visit(ConnectCollisions);
            }
        };
#pragma endregion

        // A set of bone transforms to pose a rigged object.
        struct PartPose {
            // A transform for a single bone in an object.
            struct Bone {
                std::string Name = "Master";
                int32_t NameIndex = 0;  // only meaningful while reading and writing
                Vector3 Translation;
                // Rotation of the bone, in radians.
                Vector3 Rotation;
                Vector3 Scale{1.f, 1.f, 1.f};
            };

            // The name of the part to be posed.
            Msb::Ref<PartList, int16_t> PartName;
            std::vector<Bone> Bones;
        };

        // The format to use when writing.
        MSBFormat Format = MSBFormat::DarkSouls2Scholar;
        // Model files available for parts to use.
        ModelParam Models;
        // Abstract entities that set map properties or control behaviors.
        EventParam Events;
        // Points or volumes that trigger certain behaviors.
        PointParam Regions;
        // Concrete entities in the map.
        PartsParam Parts;
        // Predetermined poses applied to objects such as corpses.
        std::vector<PartPose> PartPoses;

    protected:
        bool IsImpl(BinaryReader& Reader) override;
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
