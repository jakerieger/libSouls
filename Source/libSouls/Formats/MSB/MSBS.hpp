//
// Created by Jake Rieger on 10/8/2026.
//

#pragma once

#include "MSBCommon.hpp"
#include "MSBParam.hpp"

#include <libSouls/SoulsFile.hpp>

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of exported classes; only touched by this DLL's own code

    // MSBS: the map layout file of Sekiro. Extension: .msb
    //
    // An MSBS has params of models (the model files parts can use), events, regions, routes, layers (unused), parts,
    // and two more unused ones. Entries refer to each other by name.
    class SOULS_API MSBS : public SoulsFile<MSBS> {
    public:
        using ModelList     = Msb::ModelList;
        using EventList     = Msb::EventList;
        using RegionList    = Msb::RegionList;
        using PartList      = Msb::PartList;
        using CollisionList = Msb::CollisionList;
        struct EnemyList {};               // just the enemy parts (a sublist of the parts)
        struct AutoDrawGroupPointList {};  // just the auto draw group point regions

        // A generic entry with a name.
        struct Entry {
            std::string Name;
            virtual ~Entry() = default;
        };

        // All entries of the file, in file order, for resolving references between them.
        struct Entries {
            std::vector<Entry*> Models, Events, Regions, Parts, Collisions, Enemies, AutoDrawGroupPoints;

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
                } else if constexpr (std::is_same_v<Tag, EnemyList>) {
                    return Enemies;
                } else {
                    return AutoDrawGroupPoints;
                }
            }
        };

#pragma region Models
        // A model file available for parts to reference.
        struct SOULS_API Model : Entry {
            // A path to a .sib file, presumed to be some kind of editor placeholder.
            std::string SibPath;
            // How many parts use the model; recalculated when writing.
            int32_t InstanceCount = 0;
            int32_t Unk1C         = 0;

            virtual uint32_t Type() const    = 0;
            virtual bool HasTypeData() const = 0;
            virtual void ReadTypeData(BinaryReader&) {}
            virtual void WriteTypeData(BinaryWriter&) {}
            void Read(BinaryReader& Reader);
            void Write(BinaryWriter& Writer, int32_t ID);
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

        // Model files that are available for parts to use.
        struct ModelParam : Msb::Detail::TypedLists<ModelParam, Model> {
            // A model for fixed terrain or scenery.
            struct MapPiece : ModelImpl<MapPiece, 0, true> {
                bool UnkT00 = false, UnkT01 = false, UnkT02 = false;
                float UnkT04 = 0, UnkT08 = 0, UnkT0C = 0, UnkT10 = 0, UnkT14 = 0, UnkT18 = 0;
                MapPiece() { Name = "mXXXXXX"; }
                template<typename V>
                void Fields(V& F) {
                    F(UnkT00);
                    F(UnkT01);
                    F(UnkT02);
                    F.template Const<uint8_t>(0);
                    F(UnkT04);
                    F(UnkT08);
                    F(UnkT0C);
                    F(UnkT10);
                    F(UnkT14);
                    F(UnkT18);
                    F.template Const<int32_t>(0);
                }
            };
            // A model for a dynamic prop.
            struct Object : ModelImpl<Object, 1, false> {
                Object() { Name = "oXXXXXX"; }
            };
            // A model for a non-player entity.
            struct Enemy : ModelImpl<Enemy, 2, false> {
                Enemy() { Name = "cXXXX"; }
            };
            // A model for a player spawn point?
            struct Player : ModelImpl<Player, 4, false> {
                Player() { Name = "c0000"; }
            };
            // A model for collision physics.
            struct Collision : ModelImpl<Collision, 5, false> {
                Collision() { Name = "hXXXXXX"; }
            };

            // Unknown; probably some kind of version number.
            int32_t Version = 35;
            std::vector<MapPiece> MapPieces;
            std::vector<Object> Objects;
            std::vector<Enemy> Enemies;
            std::vector<Player> Players;
            std::vector<Collision> Collisions;

            static constexpr const char* ParamName = "MODEL_PARAM_ST";

            template<typename F>
            void ForEachList(F&& Visit) {
                Visit(MapPieces);
                Visit(Objects);
                Visit(Enemies);
                Visit(Players);
                Visit(Collisions);
            }
            template<typename F>
            void ForEachList(F&& Visit) const {
                Visit(MapPieces);
                Visit(Objects);
                Visit(Enemies);
                Visit(Players);
                Visit(Collisions);
            }
        };
#pragma endregion

#pragma region Events
        // A dynamic or interactive system.
        struct SOULS_API Event : Entry {
            int32_t EventID = -1;
            Msb::Ref<PartList> PartName;
            Msb::Ref<RegionList> RegionName;
            // Identifies the event in event scripts.
            int32_t EntityID = -1;

            virtual uint32_t Type() const    = 0;
            virtual bool HasTypeData() const = 0;
            void Read(BinaryReader& Reader);
            void Write(BinaryWriter& Writer, int32_t ID);
            virtual void ReadTypeData(BinaryReader&) {}
            virtual void WriteTypeData(BinaryWriter&) {}
            virtual void GetNames(const Entries& Lists);
            virtual void GetIndices(const Entries& Lists);
        };

        template<typename D, uint32_t Id, bool WithData = true>
        struct EventImpl : Event {
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
            void GetNames(const Entries& Lists) override {
                Event::GetNames(Lists);
                if constexpr (WithData) {
                    Msb::Detail::NameVisitor<Entries> V{Lists};
                    static_cast<D*>(this)->Fields(V);
                }
            }
            void GetIndices(const Entries& Lists) override {
                Event::GetIndices(Lists);
                if constexpr (WithData) {
                    Msb::Detail::IndexVisitor<Entries> V{Lists};
                    static_cast<D*>(this)->Fields(V);
                }
            }
        };

        // Dynamic or interactive systems such as item pickups, levers, enemy spawners, etc.
        struct EventParam : Msb::Detail::TypedLists<EventParam, Event> {
            // An item pickup in the open or inside a container.
            struct Treasure : EventImpl<Treasure, 4> {
                // The part that the treasure is attached to.
                Msb::Ref<PartList> TreasurePartName;
                // The item lot to be given.
                int32_t ItemLotID = -1;
                // If not -1, uses an entry from ActionButtonParam for the pickup prompt.
                int32_t ActionButtonID = -1;
                // Animation to play when taking this treasure.
                int32_t PickupAnimID = -1;
                // Changes the text of the pickup prompt.
                bool InChest = false;
                // Whether the treasure should be hidden by default.
                bool StartDisabled = false;
                Treasure() { Name = "Event: Treasure"; }
                template<typename V>
                void Fields(V& F) {
                    F.Pad(8);
                    F(TreasurePartName);
                    F.template Const<int32_t>(0);
                    F(ItemLotID);
                    F.Pattern(0x24, 0xFF);
                    F(ActionButtonID);
                    F(PickupAnimID);
                    F(InChest);
                    F(StartDisabled);
                    F.template Const<int16_t>(0);
                    F.Pad(12);
                }
            };

            // An enemy spawner.
            struct Generator : EventImpl<Generator, 5> {
                uint8_t MaxNum   = 0;
                int8_t GenType   = 0;
                int16_t LimitNum = 0, MinGenNum = 0, MaxGenNum = 0;
                float MinInterval = 0, MaxInterval = 0;
                uint8_t InitialSpawnCount = 0;
                float UnkT14 = 0, UnkT18 = 0;
                // Regions where parts will spawn from.
                std::array<Msb::Ref<RegionList>, 8> SpawnRegionNames;
                // Parts that will be respawned.
                std::array<Msb::Ref<PartList>, 32> SpawnPartNames;
                Generator() { Name = "Event: Generator"; }
                template<typename V>
                void Fields(V& F) {
                    F(MaxNum);
                    F(GenType);
                    F(LimitNum);
                    F(MinGenNum);
                    F(MaxGenNum);
                    F(MinInterval);
                    F(MaxInterval);
                    F(InitialSpawnCount);
                    F.Pad(3);
                    F(UnkT14);
                    F(UnkT18);
                    F.Pad(0x14);
                    F(SpawnRegionNames);
                    F.Pad(0x10);
                    F(SpawnPartNames);
                    F.Pad(0x20);
                }
            };

            // An interactive object.
            struct ObjAct : EventImpl<ObjAct, 7> {
                // Unknown why objacts need an extra entity ID.
                int32_t ObjActEntityID = -1;
                // The part to be interacted with.
                Msb::Ref<PartList> ObjActPartName;
                // A row in ObjActParam.
                int32_t ObjActID = -1;
                uint8_t StateType = 0;
                int32_t EventFlagID = -1;
                ObjAct() { Name = "Event: ObjAct"; }
                template<typename V>
                void Fields(V& F) {
                    F(ObjActEntityID);
                    F(ObjActPartName);
                    F(ObjActID);
                    F(StateType);
                    F.template Const<uint8_t>(0);
                    F.template Const<int16_t>(0);
                    F(EventFlagID);
                    F.Pad(12);
                }
            };

            // Shifts the entire map; already accounted for in MSB coordinates.
            struct MapOffset : EventImpl<MapOffset, 9> {
                Vector3 Position;
                float Degree = 0;
                MapOffset() { Name = "Event: MapOffset"; }
                template<typename V>
                void Fields(V& F) {
                    F(Position);
                    F(Degree);
                }
            };

            // Unknown.
            struct PatrolInfo : EventImpl<PatrolInfo, 14> {
                struct WREntry {
                    Msb::Ref<RegionList, int16_t> RegionName;
                    int32_t Unk04 = 0;
                    int32_t Unk08 = 0;
                    template<typename V>
                    void Fields(V& F) {
                        F(RegionName);
                        F.template Const<int16_t>(0);
                        F(Unk04);
                        F(Unk08);
                    }
                };
                int32_t UnkT00 = 0;
                std::array<Msb::Ref<RegionList, int16_t>, 32> WalkRegionNames;
                std::array<WREntry, 5> WREntries;
                PatrolInfo() { Name = "Event: PatrolInfo"; }
                template<typename V>
                void Fields(V& F) {
                    F(UnkT00);
                    F.Pad(12);
                    F(WalkRegionNames);
                    for (WREntry& E : WREntries) {
                        E.Fields(F);
                    }
                    F.Pad(0x14);
                }
            };

            // Unknown.
            struct PlatoonInfo : EventImpl<PlatoonInfo, 15> {
                int32_t PlatoonIDScriptActive = 0;
                int32_t State                 = 0;
                std::array<Msb::Ref<PartList>, 32> GroupPartNames;
                PlatoonInfo() { Name = "Event: PlatoonInfo"; }
                template<typename V>
                void Fields(V& F) {
                    F(PlatoonIDScriptActive);
                    F(State);
                    F.Pad(8);
                    F(GroupPartNames);
                }
            };

            // A resource item placed in the map; uses the base event's region for positioning.
            struct ResourceItemInfo : EventImpl<ResourceItemInfo, 17> {
                // ID of a row in ResourceItemLotParam that determines the resource(s) to give.
                int32_t ResourceItemLotParamID = 0;
                ResourceItemInfo() { Name = "Event: ResourceItemInfo"; }
                template<typename V>
                void Fields(V& F) {
                    F(ResourceItemLotParamID);
                    F.Pad(0x1C);
                }
            };

            // Sets the grass lod parameters for the map.
            struct GrassLodParam : EventImpl<GrassLodParam, 18> {
                // ID of a row in GrassLodRangeParam.
                int32_t GrassLodRangeParamID = 0;
                GrassLodParam() { Name = "Event: GrassLodParam"; }
                template<typename V>
                void Fields(V& F) {
                    F(GrassLodRangeParamID);
                    F.Pad(0x1C);
                }
            };

            // Unknown.
            struct SkitInfo : EventImpl<SkitInfo, 20> {
                int32_t UnkT00 = 0;
                uint8_t UnkT04 = 0, UnkT05 = 0, UnkT06 = 0, UnkT07 = 0;
                SkitInfo() { Name = "Event: SkitInfo"; }
                template<typename V>
                void Fields(V& F) {
                    F(UnkT00);
                    F(UnkT04);
                    F(UnkT05);
                    F(UnkT06);
                    F(UnkT07);
                    F.Pad(0x18);
                }
            };

            // Unknown.
            struct PlacementGroup : EventImpl<PlacementGroup, 21> {
                std::array<Msb::Ref<PartList>, 32> Event21PartNames;
                PlacementGroup() { Name = "Event: PlacementGroup"; }
                template<typename V>
                void Fields(V& F) {
                    F(Event21PartNames);
                }
            };

            // Unknown.
            struct PartsGroup : EventImpl<PartsGroup, 22, false> {
                PartsGroup() { Name = "Event: PartsGroup"; }
            };

            // Unknown.
            struct Talk : EventImpl<Talk, 23> {
                int32_t UnkT00 = 0;
                std::array<Msb::Ref<EnemyList>, 8> EnemyNames;
                // IDs of talk ESDs.
                std::array<int32_t, 8> TalkIDs{};
                int16_t UnkT44 = 0, UnkT46 = 0;
                int32_t UnkT48 = 0;
                Talk() { Name = "Event: Talk"; }
                template<typename V>
                void Fields(V& F) {
                    F(UnkT00);
                    F(EnemyNames);
                    F(TalkIDs);
                    F(UnkT44);
                    F(UnkT46);
                    F(UnkT48);
                    F.Pad(0x34);
                }
            };

            // Specifies a collision to which an autodrawgroup filming point belongs, whatever that means.
            struct AutoDrawGroupCollision : EventImpl<AutoDrawGroupCollision, 24> {
                // Name of the filming point for the autodrawgroup capture, probably.
                Msb::Ref<AutoDrawGroupPointList> AutoDrawGroupPointName;
                // The collision that the filming point belongs to, presumably.
                Msb::Ref<CollisionList> OwningCollisionName;
                AutoDrawGroupCollision() { Name = "Event: AutoDrawGroupCollision"; }
                template<typename V>
                void Fields(V& F) {
                    F(AutoDrawGroupPointName);
                    F(OwningCollisionName);
                    F.Pad(0x18);
                }
            };

            // Unknown.
            struct Other : EventImpl<Other, 0xFFFFFFFF, false> {
                Other() { Name = "Event: Other"; }
            };

            int32_t Version = 35;
            std::vector<Treasure> Treasures;
            std::vector<Generator> Generators;
            std::vector<ObjAct> ObjActs;
            std::vector<MapOffset> MapOffsets;
            std::vector<PatrolInfo> PatrolInfos;
            std::vector<PlatoonInfo> PlatoonInfos;
            std::vector<ResourceItemInfo> ResourceItemInfos;
            std::vector<GrassLodParam> GrassLodParams;
            std::vector<SkitInfo> SkitInfos;
            std::vector<PlacementGroup> PlacementGroups;
            std::vector<PartsGroup> PartsGroups;
            std::vector<Talk> Talks;
            std::vector<AutoDrawGroupCollision> AutoDrawGroupCollisions;
            std::vector<Other> Others;

            static constexpr const char* ParamName = "EVENT_PARAM_ST";

            template<typename F>
            void ForEachList(F&& Visit) {
                Visit(Treasures);
                Visit(Generators);
                Visit(ObjActs);
                Visit(MapOffsets);
                Visit(PatrolInfos);
                Visit(PlatoonInfos);
                Visit(ResourceItemInfos);
                Visit(GrassLodParams);
                Visit(SkitInfos);
                Visit(PlacementGroups);
                Visit(PartsGroups);
                Visit(Talks);
                Visit(AutoDrawGroupCollisions);
                Visit(Others);
            }
            template<typename F>
            void ForEachList(F&& Visit) const {
                Visit(Treasures);
                Visit(Generators);
                Visit(ObjActs);
                Visit(MapOffsets);
                Visit(PatrolInfos);
                Visit(PlatoonInfos);
                Visit(ResourceItemInfos);
                Visit(GrassLodParams);
                Visit(SkitInfos);
                Visit(PlacementGroups);
                Visit(PartsGroups);
                Visit(Talks);
                Visit(AutoDrawGroupCollisions);
                Visit(Others);
            }
        };
#pragma endregion

#pragma region Regions
        // A point or volume that triggers some sort of interaction.
        struct SOULS_API Region : Entry {
            Msb::Shape Shape = Msb::Shapes::Point{};
            Vector3 Position;
            // The rotation of the region, in degrees.
            Vector3 Rotation;
            int32_t Unk2C = 0;
            // Controls whether the region is active in different ceremonies.
            uint32_t MapStudioLayer = 0xFFFFFFFF;
            std::vector<int16_t> UnkA, UnkB;
            // If specified, the region is only active when the part is loaded.
            Msb::Ref<PartList> ActivationPartName;
            // Identifies the region in event scripts.
            int32_t EntityID = -1;

            virtual uint32_t Type() const    = 0;
            virtual bool HasTypeData() const = 0;
            void Read(BinaryReader& Reader);
            void Write(BinaryWriter& Writer, int32_t ID);
            virtual void ReadTypeData(BinaryReader&) {}
            virtual void WriteTypeData(BinaryWriter&) {}
            virtual void GetNames(const Entries& Lists);
            virtual void GetIndices(const Entries& Lists);
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
            void GetNames(const Entries& Lists) override {
                Region::GetNames(Lists);
                if constexpr (WithData) {
                    Msb::Detail::NameVisitor<Entries> V{Lists};
                    static_cast<D*>(this)->Fields(V);
                }
            }
            void GetIndices(const Entries& Lists) override {
                Region::GetIndices(Lists);
                if constexpr (WithData) {
                    Msb::Detail::IndexVisitor<Entries> V{Lists};
                    static_cast<D*>(this)->Fields(V);
                }
            }
        };

        // Points and volumes used to trigger various effects.
        struct PointParam : Msb::Detail::TypedLists<PointParam, Region> {
            // Previously points where players will appear when invading; may not do anything in Sekiro.
            struct InvasionPoint : RegionImpl<InvasionPoint, 1, true> {
                int32_t Priority = 0;
                InvasionPoint() { Name = "Region: InvasionPoint"; }
                template<typename V>
                void Fields(V& F) {
                    F(Priority);
                }
            };

            // Unknown.
            struct EnvironmentMapPoint : RegionImpl<EnvironmentMapPoint, 2, true> {
                float UnkT00  = 0;
                int32_t UnkT04 = 0;
                int32_t UnkT0C = 0;
                float UnkT10 = 0, UnkT14 = 0;
                int32_t UnkT18 = 0, UnkT1C = 0, UnkT20 = 0, UnkT24 = 0, UnkT28 = 0;
                EnvironmentMapPoint() { Name = "Region: EnvironmentMapPoint"; }
                template<typename V>
                void Fields(V& F) {
                    F(UnkT00);
                    F(UnkT04);
                    F.template Const<int32_t>(-1);
                    F(UnkT0C);
                    F(UnkT10);
                    F(UnkT14);
                    F(UnkT18);
                    F(UnkT1C);
                    F(UnkT20);
                    F(UnkT24);
                    F(UnkT28);
                    F.template Const<int32_t>(-1);
                    F.Pad(0x10);
                }
            };

            // An area where a sound plays.
            struct Sound : RegionImpl<Sound, 4, true> {
                // The category of the sound.
                int32_t SoundType = 0;
                // The ID of the sound.
                int32_t SoundID = 0;
                // References to other regions used to build a composite shape.
                std::array<Msb::Ref<RegionList>, 16> ChildRegionNames;
                int32_t UnkT48 = 0;
                Sound() { Name = "Region: Sound"; }
                template<typename V>
                void Fields(V& F) {
                    F(SoundType);
                    F(SoundID);
                    F(ChildRegionNames);
                    F(UnkT48);
                }
            };

            // A point where a particle effect can play.
            struct SFX : RegionImpl<SFX, 5, true> {
                int32_t EffectID = 0;
                int32_t UnkT04   = 0;
                // Whether the effect is off until activated.
                int32_t StartDisabled = 0;
                SFX() { Name = "Region: SFX"; }
                template<typename V>
                void Fields(V& F) {
                    F(EffectID);
                    F(UnkT04);
                    F.template Const<int32_t>(-1);
                    F.template Const<int32_t>(-1);
                    F.template Const<int32_t>(-1);
                    F(StartDisabled);
                }
            };

            // Unknown.
            struct WindSFX : RegionImpl<WindSFX, 6, true> {
                int32_t EffectID = 0;
                // Reference to a WindArea region.
                Msb::Ref<RegionList> WindAreaName;
                float UnkT18 = 0;
                WindSFX() { Name = "Region: WindSFX"; }
                template<typename V>
                void Fields(V& F) {
                    F(EffectID);
                    F.Pattern(0x10, 0xFF);
                    F(WindAreaName);
                    F(UnkT18);
                    F.template Const<int32_t>(0);
                }
            };

            // A point where the player can spawn into the map.
            struct SpawnPoint : RegionImpl<SpawnPoint, 8, true> {
                SpawnPoint() { Name = "Region: SpawnPoint"; }
                template<typename V>
                void Fields(V& F) {
                    F.template Const<int32_t>(-1);
                    F.Pad(12);
                }
            };

            // Regions with no type-specific data.
            struct PatrolRoute : RegionImpl<PatrolRoute, 11, false> {  // a point along an NPC patrol path
                PatrolRoute() { Name = "Region: PatrolRoute"; }
            };
            struct WarpPoint : RegionImpl<WarpPoint, 13, false> {  // a point the player can be warped to
                WarpPoint() { Name = "Region: WarpPoint"; }
            };
            struct ActivationArea : RegionImpl<ActivationArea, 14, false> {  // an area that triggers enemies when entered
                ActivationArea() { Name = "Region: ActivationArea"; }
            };
            struct Event : RegionImpl<Event, 15, false> {  // a generic area used by event scripts
                Event() { Name = "Region: Event"; }
            };
            struct Logic : RegionImpl<Logic, 0, false> {  // unknown
                Logic() { Name = "Region: Logic"; }
            };

            // Unknown.
            struct EnvironmentMapEffectBox : RegionImpl<EnvironmentMapEffectBox, 17, true> {
                float UnkT00  = 0;
                float Compare = 0;
                uint8_t UnkT08 = 0, UnkT09 = 0;
                int16_t UnkT0A = 0;
                int32_t UnkT24 = 0;
                float UnkT28 = 0, UnkT2C = 0;
                EnvironmentMapEffectBox() { Name = "Region: EnvironmentMapEffectBox"; }
                template<typename V>
                void Fields(V& F) {
                    F(UnkT00);
                    F(Compare);
                    F(UnkT08);
                    F(UnkT09);
                    F(UnkT0A);
                    F.Pad(0x18);
                    F(UnkT24);
                    F(UnkT28);
                    F(UnkT2C);
                    F.template Const<int32_t>(0);
                }
            };

            struct WindArea : RegionImpl<WindArea, 18, false> {  // unknown
                WindArea() { Name = "Region: WindArea"; }
            };

            // An area where sound is muffled.
            struct MufflingBox : RegionImpl<MufflingBox, 20, true> {
                int32_t UnkT00 = 0;
                MufflingBox() { Name = "Region: MufflingBox"; }
                template<typename V>
                void Fields(V& F) {
                    F(UnkT00);
                }
            };

            // An entrance to a muffling box.
            struct MufflingPortal : RegionImpl<MufflingPortal, 21, true> {
                int32_t UnkT00 = 0;
                MufflingPortal() { Name = "Region: MufflingPortal"; }
                template<typename V>
                void Fields(V& F) {
                    F(UnkT00);
                    F.template Const<int32_t>(0);
                }
            };

            // Unknown.
            struct SoundSpaceOverride : RegionImpl<SoundSpaceOverride, 23, true> {
                // Unknown, probably soundspace types.
                uint8_t UnkT00 = 0, UnkT01 = 0;
                SoundSpaceOverride() { Name = "Region: SoundSpaceOverride"; }
                template<typename V>
                void Fields(V& F) {
                    F(UnkT00);
                    F(UnkT01);
                    F.Pad(0x1E);
                }
            };

            struct MufflingPlane : RegionImpl<MufflingPlane, 24, false> {  // unknown
                MufflingPlane() { Name = "Region: MufflingPlane"; }
            };

            // Unknown.
            struct PartsGroupArea : RegionImpl<PartsGroupArea, 25, true> {
                int64_t UnkT00 = 0;
                PartsGroupArea() { Name = "Region: PartsGroupArea"; }
                template<typename V>
                void Fields(V& F) {
                    F(UnkT00);
                }
            };

            // Unknown.
            struct AutoDrawGroupPoint : RegionImpl<AutoDrawGroupPoint, 26, true> {
                int64_t UnkT00 = 0;
                AutoDrawGroupPoint() { Name = "Region: AutoDrawGroupPoint"; }
                template<typename V>
                void Fields(V& F) {
                    F(UnkT00);
                    F.Pad(0x18);
                }
            };

            // Most likely an unused region.
            struct Other : RegionImpl<Other, 0xFFFFFFFF, false> {
                Other() { Name = "Region: Other"; }
            };

            int32_t Version = 35;
            std::vector<InvasionPoint> InvasionPoints;
            std::vector<EnvironmentMapPoint> EnvironmentMapPoints;
            std::vector<Sound> Sounds;
            std::vector<SFX> SFXs;
            std::vector<WindSFX> WindSFXs;
            std::vector<SpawnPoint> SpawnPoints;
            std::vector<PatrolRoute> PatrolRoutes;
            std::vector<WarpPoint> WarpPoints;
            std::vector<ActivationArea> ActivationAreas;
            std::vector<Event> Events;
            std::vector<Logic> Logics;
            std::vector<EnvironmentMapEffectBox> EnvironmentMapEffectBoxes;
            std::vector<WindArea> WindAreas;
            std::vector<MufflingBox> MufflingBoxes;
            std::vector<MufflingPortal> MufflingPortals;
            std::vector<SoundSpaceOverride> SoundSpaceOverrides;
            std::vector<MufflingPlane> MufflingPlanes;
            std::vector<PartsGroupArea> PartsGroupAreas;
            std::vector<AutoDrawGroupPoint> AutoDrawGroupPoints;
            std::vector<Other> Others;

            static constexpr const char* ParamName = "POINT_PARAM_ST";

            template<typename F>
            void ForEachList(F&& Visit) {
                Visit(InvasionPoints);
                Visit(EnvironmentMapPoints);
                Visit(Sounds);
                Visit(SFXs);
                Visit(WindSFXs);
                Visit(SpawnPoints);
                Visit(PatrolRoutes);
                Visit(WarpPoints);
                Visit(ActivationAreas);
                Visit(Events);
                Visit(Logics);
                Visit(EnvironmentMapEffectBoxes);
                Visit(WindAreas);
                Visit(MufflingBoxes);
                Visit(MufflingPortals);
                Visit(SoundSpaceOverrides);
                Visit(MufflingPlanes);
                Visit(PartsGroupAreas);
                Visit(AutoDrawGroupPoints);
                Visit(Others);
            }
            template<typename F>
            void ForEachList(F&& Visit) const {
                Visit(InvasionPoints);
                Visit(EnvironmentMapPoints);
                Visit(Sounds);
                Visit(SFXs);
                Visit(WindSFXs);
                Visit(SpawnPoints);
                Visit(PatrolRoutes);
                Visit(WarpPoints);
                Visit(ActivationAreas);
                Visit(Events);
                Visit(Logics);
                Visit(EnvironmentMapEffectBoxes);
                Visit(WindAreas);
                Visit(MufflingBoxes);
                Visit(MufflingPortals);
                Visit(SoundSpaceOverrides);
                Visit(MufflingPlanes);
                Visit(PartsGroupAreas);
                Visit(AutoDrawGroupPoints);
                Visit(Others);
            }
        };
#pragma endregion

#pragma region Routes
        // Unknown, but related to muffling regions somehow.
        struct SOULS_API Route : Entry {
            int32_t Unk08 = 0;
            int32_t Unk0C = 0;

            virtual uint32_t Type() const = 0;
            void Read(BinaryReader& Reader);
            void Write(BinaryWriter& Writer, int32_t ID);
        };

        template<uint32_t Id>
        struct RouteOf : Route {
            static constexpr uint32_t TypeId = Id;
            uint32_t Type() const override { return Id; }
        };

        struct RouteParam : Msb::Detail::TypedLists<RouteParam, Route> {
            // Unknown; has something to do with muffling portals.
            struct MufflingPortalLink : RouteOf<3> {
                MufflingPortalLink() { Name = "X-X"; }
            };
            // Unknown; has something to do with muffling boxes.
            struct MufflingBoxLink : RouteOf<4> {
                MufflingBoxLink() { Name = "X-X"; }
            };

            int32_t Version = 35;
            std::vector<MufflingPortalLink> MufflingPortalLinks;
            std::vector<MufflingBoxLink> MufflingBoxLinks;

            static constexpr const char* ParamName = "ROUTE_PARAM_ST";

            template<typename F>
            void ForEachList(F&& Visit) {
                Visit(MufflingPortalLinks);
                Visit(MufflingBoxLinks);
            }
            template<typename F>
            void ForEachList(F&& Visit) const {
                Visit(MufflingPortalLinks);
                Visit(MufflingBoxLinks);
            }
        };

        // Used for unused params that should never have any entries in them.
        struct EmptyParam {
            // Unknown; probably some kind of version number.
            int32_t Version = 0;
        };
#pragma endregion

#pragma region Parts
        // Unknown.
        struct UnkStruct1 {
            std::array<uint32_t, 48> CollisionMask{};
            uint8_t Condition1 = 0;
            uint8_t Condition2 = 0;
            template<typename V>
            void Fields(V& F) {
                F(CollisionMask);
                F(Condition1);
                F(Condition2);
                F.template Const<int16_t>(0);
                F.Pad(0xC0);
            }
        };

        // Unknown.
        struct UnkStruct2 {
            int32_t Condition = 0;
            std::array<int32_t, 8> DispGroups{};
            int16_t Unk24 = 0, Unk26 = 0;
            template<typename V>
            void Fields(V& F) {
                F(Condition);
                F(DispGroups);
                F(Unk24);
                F(Unk26);
                F.Pad(0x20);
            }
        };

        // Gparam value IDs for various part types.
        struct GparamConfig {
            int32_t LightSetID = 0, FogParamID = 0, LightScatteringID = 0, EnvMapID = 0;
            template<typename V>
            void Fields(V& F) {
                F(LightSetID);
                F(FogParamID);
                F(LightScatteringID);
                F(EnvMapID);
                F.Pad(0x10);
            }
        };

        // Unknown; sceneGParam struct according to Pav.
        struct SceneGparamConfig {
            std::array<int8_t, 4> EventIDs{};
            float Unk40 = 0;
            template<typename V>
            void Fields(V& F) {
                F.Pad(0x3C);
                F(EventIDs);
                F(Unk40);
                F.Pad(12);
            }
        };

        // Unknown.
        struct UnkStruct7 {
            int32_t Unk00 = 0, Unk04 = 0;
            // ID in GrassTypeParam determining properties of dynamic grass on a map piece.
            int32_t GrassTypeParamID = 0;
            int32_t Unk0C = 0, Unk10 = 0, Unk14 = 0;
            template<typename V>
            void Fields(V& F) {
                F(Unk00);
                F(Unk04);
                F(GrassTypeParamID);
                F(Unk0C);
                F(Unk10);
                F(Unk14);
                F.template Const<int32_t>(-1);
                F.template Const<int32_t>(0);
            }
        };

        // Which optional blocks of data a part type has.
        enum PartBlocks : unsigned {
            NoBlocks   = 0,
            HasUnk1    = 1,
            HasUnk2    = 2,
            HasGparam  = 4,
            HasScene   = 8,
            HasUnk7    = 16,
        };

        // Common data for all types of part.
        struct SOULS_API Part : Entry {
            // The model used by this part; requires an entry in ModelParam.
            Msb::Ref<ModelList> ModelName;
            // A path to a .sib file, presumably some kind of editor placeholder.
            std::string SibPath;
            Vector3 Position;
            Vector3 Rotation;
            // Scale of the part; only works for map pieces and objects.
            Vector3 Scale{1.f, 1.f, 1.f};
            // Identifies the part in event scripts.
            int32_t EntityID = -1;
            uint8_t UnkE04 = 0, UnkE05 = 0, UnkE06 = 0, LanternID = 0, LodParamID = 0, UnkE09 = 0;
            bool IsPointLightShadowSrc = false;
            uint8_t UnkE0B             = 0;
            bool IsShadowSrc           = false;
            uint8_t IsStaticShadowSrc = 0, IsCascade3ShadowSrc = 0, UnkE0F = 0, UnkE10 = 0;
            bool IsShadowDest = false, IsShadowOnly = false, DrawByReflectCam = false, DrawOnlyReflectCam = false;
            uint8_t EnableOnAboveShadow = 0;
            bool DisablePointLightEffect = false;
            uint8_t UnkE17 = 0;
            int32_t UnkE18 = 0;
            // Allows multiple parts to be identified by the same entity ID.
            std::array<int32_t, 8> EntityGroupIDs{-1, -1, -1, -1, -1, -1, -1, -1};
            int32_t UnkE3C = 0;
            int32_t UnkE40 = 0;

            virtual uint32_t Type() const = 0;
            virtual unsigned Blocks() const = 0;
            void Read(BinaryReader& Reader);
            void Write(BinaryWriter& Writer, int32_t ID);
            virtual void ReadTypeData(BinaryReader& Reader)  = 0;
            virtual void WriteTypeData(BinaryWriter& Writer) = 0;
            // Reads or writes one of the optional blocks (see PartBlocks).
            virtual void ReadBlock(unsigned Block, BinaryReader&) {}
            virtual void WriteBlock(unsigned Block, BinaryWriter&) {}
            virtual void GetNames(const Entries& Lists);
            virtual void GetIndices(const Entries& Lists);
        };

        // Implements a part type's data functions from its Fields function. D::Block (a PartBlocks mask) says which of
        // the optional blocks it has; the blocks are members of D (Unk1, Unk2, Gparam, SceneGparam, Unk7).
        template<typename D, uint32_t Id, typename B = Part>
        struct PartImpl : B {
            static constexpr uint32_t TypeId = Id;
            uint32_t Type() const override { return Id; }
            unsigned Blocks() const override { return D::Block; }
            void ReadTypeData(BinaryReader& Reader) override {
                Msb::Detail::ReadVisitor V{Reader};
                static_cast<D*>(this)->Fields(V);
            }
            void WriteTypeData(BinaryWriter& Writer) override {
                Msb::Detail::WriteVisitor V{Writer};
                static_cast<D*>(this)->Fields(V);
            }
            template<typename V>
            void VisitBlock(unsigned BlockBit, V& Visitor) {
                D& Self = *static_cast<D*>(this);
                if constexpr (D::Block & HasUnk1) {
                    if (BlockBit == HasUnk1) Self.Unk1.Fields(Visitor);
                }
                if constexpr (D::Block & HasUnk2) {
                    if (BlockBit == HasUnk2) Self.Unk2.Fields(Visitor);
                }
                if constexpr (D::Block & HasGparam) {
                    if (BlockBit == HasGparam) Self.Gparam.Fields(Visitor);
                }
                if constexpr (D::Block & HasScene) {
                    if (BlockBit == HasScene) Self.SceneGparam.Fields(Visitor);
                }
                if constexpr (D::Block & HasUnk7) {
                    if (BlockBit == HasUnk7) Self.Unk7.Fields(Visitor);
                }
            }
            void ReadBlock(unsigned Block, BinaryReader& Reader) override {
                Msb::Detail::ReadVisitor V{Reader};
                VisitBlock(Block, V);
            }
            void WriteBlock(unsigned Block, BinaryWriter& Writer) override {
                Msb::Detail::WriteVisitor V{Writer};
                VisitBlock(Block, V);
            }
            void GetNames(const Entries& Lists) override {
                B::GetNames(Lists);
                Msb::Detail::NameVisitor<Entries> V{Lists};
                static_cast<D*>(this)->Fields(V);
            }
            void GetIndices(const Entries& Lists) override {
                B::GetIndices(Lists);
                Msb::Detail::IndexVisitor<Entries> V{Lists};
                static_cast<D*>(this)->Fields(V);
            }
        };

        // Common base data for objects and dummy objects.
        struct ObjectBase : Part {
            GparamConfig Gparam;
            // Reference to a map piece or collision; believed to determine when the object is loaded.
            Msb::Ref<PartList> ObjPartName1;
            uint8_t BreakTerm                = 0;
            bool NetSyncType                 = false;
            uint8_t UnkT0E                   = 0;
            bool SetMainObjStructureBooleans = false;
            int16_t AnimID                   = 0;
            int16_t UnkT18 = 0, UnkT1A = 0;
            // References to collisions; believed to be involved with loading when grappling to the object.
            Msb::Ref<PartList> ObjPartName2, ObjPartName3;

            ObjectBase() { Name = "oXXXXXX_XXXX"; }
            template<typename V>
            void ObjectFields(V& F) {
                F.Pad(8);
                F(ObjPartName1);
                F(BreakTerm);
                F(NetSyncType);
                F(UnkT0E);
                F(SetMainObjStructureBooleans);
                F(AnimID);
                F.template Const<int16_t>(-1);
                F.template Const<int32_t>(-1);
                F(UnkT18);
                F(UnkT1A);
                F.template Const<int32_t>(-1);
                F(ObjPartName2);
                F(ObjPartName3);
            }
        };

        // Common base data for enemies and dummy enemies.
        struct EnemyBase : Part {
            GparamConfig Gparam;
            // An ID in NPCThinkParam that determines the enemy's AI characteristics.
            int32_t ThinkParamID = -1;
            // An ID in NPCParam that determines a variety of enemy properties.
            int32_t NPCParamID = -1;
            // Previously talk ID, now always 0 or 1 except for the Memorial Mob in Senpou.
            int32_t UnkT10 = -1;
            int16_t PlatoonID = 0;
            // An ID in CharaInitParam that determines a human's inventory and stats.
            int32_t CharaInitID = -1;
            // Should reference the collision the enemy starts on.
            Msb::Ref<PartList> CollisionPartName;
            int16_t UnkT20 = 0, UnkT22 = 0;
            int32_t UnkT24 = 0;
            int32_t BackupEventAnimID = -1;
            int32_t EventFlagID = -1;
            int32_t EventFlagCompareState = 0;
            int32_t UnkT48 = 0, UnkT4C = 0, UnkT50 = 0, UnkT78 = 0;
            float UnkT84 = 0;

            EnemyBase() { Name = "cXXXX_XXXX"; }
            template<typename V>
            void EnemyFields(V& F) {
                F.Pad(8);
                F(ThinkParamID);
                F(NPCParamID);
                F(UnkT10);
                F.template Const<int16_t>(0);
                F(PlatoonID);
                F(CharaInitID);
                F(CollisionPartName);
                F(UnkT20);
                F(UnkT22);
                F(UnkT24);
                F.Pattern(0x10, 0xFF);
                F(BackupEventAnimID);
                F.template Const<int32_t>(-1);
                F(EventFlagID);
                F(EventFlagCompareState);
                F(UnkT48);
                F(UnkT4C);
                F(UnkT50);
                F.template Const<int32_t>(1);
                F.template Const<int32_t>(-1);
                F.template Const<int32_t>(1);
                F.Pad(0x18);
                F(UnkT78);
                F.Pad(8);
                F(UnkT84);
                for (int I = 0; I < 5; ++I) {
                    F.template Const<int32_t>(-1);
                    F.template Const<int16_t>(-1);
                    F.template Const<int16_t>(0xA);
                }
                F.Pad(0x10);
            }
        };

        // Instances of actual things in the map.
        struct PartsParam : Msb::Detail::TypedLists<PartsParam, Part> {
            // Fixed visual geometry.
            struct MapPiece : PartImpl<MapPiece, 0> {
                static constexpr unsigned Block = HasUnk1 | HasGparam | HasUnk7;
                UnkStruct1 Unk1;
                GparamConfig Gparam;
                UnkStruct7 Unk7;
                MapPiece() { Name = "mXXXXXX_XXXX"; }
                template<typename V>
                void Fields(V& F) {
                    F.Pad(8);
                }
            };

            // A dynamic or interactible element in the map.
            struct Object : PartImpl<Object, 1, ObjectBase> {
                static constexpr unsigned Block = HasUnk1 | HasGparam;
                UnkStruct1 Unk1;
                template<typename V>
                void Fields(V& F) {
                    ObjectFields(F);
                }
            };

            // Any non-player character.
            struct Enemy : PartImpl<Enemy, 2, EnemyBase> {
                static constexpr unsigned Block = HasUnk1 | HasGparam;
                UnkStruct1 Unk1;
                template<typename V>
                void Fields(V& F) {
                    EnemyFields(F);
                }
            };

            // A spawn point for the player, or something.
            struct Player : PartImpl<Player, 4> {
                static constexpr unsigned Block = NoBlocks;
                Player() { Name = "c0000_XXXX"; }
                template<typename V>
                void Fields(V& F) {
                    F.Pad(0x10);
                }
            };

            // Invisible but physical geometry.
            struct Collision : PartImpl<Collision, 5> {
                static constexpr unsigned Block = HasUnk1 | HasUnk2 | HasGparam | HasScene;
                UnkStruct1 Unk1;
                UnkStruct2 Unk2;
                GparamConfig Gparam;
                SceneGparamConfig SceneGparam;
                uint8_t HitFilterID = 0;
                // Adds reverb to sounds while on this collision to simulate echoes.
                uint8_t SoundSpaceType = 0;
                float ReflectPlaneHeight = 0;
                // Determines the text to display for map popups and save files.
                int16_t MapNameID = 0;
                bool DisableStart = false;
                uint8_t UnkT17    = 0;
                // If not -1, the bonfire with this ID will be disabled when enemies are on this collision.
                int32_t DisableBonfireEntityID = -1;
                uint8_t UnkT24 = 0, UnkT25 = 0, UnkT26 = 0;
                // Should alter visibility while on this collision, but does not seem to do much.
                uint8_t MapVisibility = 0;
                // Used to determine invasion eligibility.
                int32_t PlayRegionID = 0;
                // Alters camera properties while on this collision.
                int16_t LockCamParamID = 0;
                int32_t UnkT3C = 0, UnkT40 = 0;
                float UnkT44 = 0, UnkT48 = 0;
                int32_t UnkT4C = 0;
                float UnkT50 = 0, UnkT54 = 0;

                Collision() { Name = "hXXXXXX"; }
                template<typename V>
                void Fields(V& F) {
                    F(HitFilterID);
                    F(SoundSpaceType);
                    F.template Const<int16_t>(0);
                    F(ReflectPlaneHeight);
                    F.template Const<int32_t>(-1);
                    F.template Const<int32_t>(-1);
                    F.template Const<int32_t>(-1);
                    F(MapNameID);
                    F(DisableStart);
                    F(UnkT17);
                    F(DisableBonfireEntityID);
                    F.template Const<int32_t>(-1);
                    F.template Const<int32_t>(-1);
                    F(UnkT24);
                    F(UnkT25);
                    F(UnkT26);
                    F(MapVisibility);
                    F(PlayRegionID);
                    F(LockCamParamID);
                    F.template Const<int16_t>(-1);
                    F.template Const<int32_t>(-1);
                    F.template Const<int32_t>(-1);
                    F.template Const<int32_t>(-1);
                    F(UnkT3C);
                    F(UnkT40);
                    F(UnkT44);
                    F(UnkT48);
                    F(UnkT4C);
                    F(UnkT50);
                    F(UnkT54);
                    F.Pad(8);
                }
            };

            // An object that either isn't used, or is used for a cutscene.
            struct DummyObject : PartImpl<DummyObject, 9, ObjectBase> {
                static constexpr unsigned Block = HasGparam;
                template<typename V>
                void Fields(V& F) {
                    ObjectFields(F);
                }
            };

            // An enemy that either isn't used, or is used for a cutscene.
            struct DummyEnemy : PartImpl<DummyEnemy, 10, EnemyBase> {
                static constexpr unsigned Block = HasGparam;
                template<typename V>
                void Fields(V& F) {
                    EnemyFields(F);
                }
            };

            // References an actual collision and causes another map to be loaded while on it.
            struct ConnectCollision : PartImpl<ConnectCollision, 11> {
                static constexpr unsigned Block = HasUnk2;
                UnkStruct2 Unk2;
                // The collision part to attach to.
                Msb::Ref<CollisionList> CollisionName;
                // The map to load when on this collision.
                std::array<uint8_t, 4> MapID{};
                ConnectCollision() { Name = "hXXXXXX_XXXX"; }
                template<typename V>
                void Fields(V& F) {
                    F(CollisionName);
                    F(MapID);
                    F.Pad(8);
                }
            };

            int32_t Version = 35;
            std::vector<MapPiece> MapPieces;
            std::vector<Object> Objects;
            std::vector<Enemy> Enemies;
            std::vector<Player> Players;
            std::vector<Collision> Collisions;
            std::vector<DummyObject> DummyObjects;
            std::vector<DummyEnemy> DummyEnemies;
            std::vector<ConnectCollision> ConnectCollisions;

            static constexpr const char* ParamName = "PARTS_PARAM_ST";

            template<typename F>
            void ForEachList(F&& Visit) {
                Visit(MapPieces);
                Visit(Objects);
                Visit(Enemies);
                Visit(Players);
                Visit(Collisions);
                Visit(DummyObjects);
                Visit(DummyEnemies);
                Visit(ConnectCollisions);
            }
            template<typename F>
            void ForEachList(F&& Visit) const {
                Visit(MapPieces);
                Visit(Objects);
                Visit(Enemies);
                Visit(Players);
                Visit(Collisions);
                Visit(DummyObjects);
                Visit(DummyEnemies);
                Visit(ConnectCollisions);
            }
        };
#pragma endregion

        // Model files that are available for parts to use.
        ModelParam Models;
        // Dynamic or interactive systems such as item pickups, levers, enemy spawners, etc.
        EventParam Events;
        // Points or areas of space that trigger some sort of behavior.
        PointParam Regions;
        // Unknown, but related to muffling regions somehow.
        RouteParam Routes;
        // Instances of actual things in the map.
        PartsParam Parts;
        // Unknown and unused.
        EmptyParam Layers{0x23};
        // Sets bone positions for fixed objects; not used in Sekiro.
        EmptyParam PartsPoses{0};
        // Bone names for the parts pose param; not used in Sekiro.
        EmptyParam BoneNames{0};

    protected:
        bool IsImpl(BinaryReader& Reader) override;
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
