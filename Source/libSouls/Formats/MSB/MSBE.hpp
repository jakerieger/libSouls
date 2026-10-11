//
// Created by Jake Rieger on 10/11/2026.
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

    // MSBE: the map layout file of Elden Ring. Extension: .msb
    //
    // An MSBE has params of models (the model files parts can use), events, regions, routes, layers (unused) and
    // parts. Entries refer to each other by name.
    class SOULS_API MSBE : public SoulsFile<MSBE> {
    public:
        using ModelList     = Msb::ModelList;
        using EventList     = Msb::EventList;
        using RegionList    = Msb::RegionList;
        using PartList      = Msb::PartList;
        using CollisionList = Msb::CollisionList;
        struct PatrolInfoList {};  // just the patrol info events (a sublist of the events)

        // A generic entry with a name.
        struct Entry {
            std::string Name;
            virtual ~Entry() = default;
        };

        // All entries of the file, in file order, for resolving references between them.
        struct Entries {
            std::vector<Entry*> Models, Events, Regions, Parts, Collisions, PatrolInfos;

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
                    return PatrolInfos;
                }
            }
        };

        // Implements the type-specific data functions of an entry from D's Fields function.
        template<typename D, typename B, bool WithData>
        struct TypeDataOf : B {
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
                B::GetNames(Lists);
                if constexpr (WithData) {
                    Msb::Detail::NameVisitor<Entries> V{Lists};
                    static_cast<D*>(this)->Fields(V);
                }
            }
            void GetIndices(const Entries& Lists) override {
                B::GetIndices(Lists);
                if constexpr (WithData) {
                    Msb::Detail::IndexVisitor<Entries> V{Lists};
                    static_cast<D*>(this)->Fields(V);
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

            virtual uint32_t Type() const = 0;
            void Read(BinaryReader& Reader);
            void Write(BinaryWriter& Writer, int32_t ID);
        };

        template<uint32_t Id>
        struct ModelOf : Model {
            static constexpr uint32_t TypeId = Id;
            uint32_t Type() const override { return Id; }
        };

        // Model files that are available for parts to use.
        struct ModelParam : Msb::Detail::TypedLists<ModelParam, Model> {
            // A model for fixed terrain or scenery.
            struct MapPiece : ModelOf<0> {
                MapPiece() { Name = "mXXXXXX"; }
            };
            // A model for a non-player entity.
            struct Enemy : ModelOf<2> {
                Enemy() { Name = "cXXXX"; }
            };
            // A model for a player spawn point?
            struct Player : ModelOf<4> {
                Player() { Name = "c0000"; }
            };
            // A model for collision physics.
            struct Collision : ModelOf<5> {
                Collision() { Name = "hXXXXXX"; }
            };
            // A model for an asset (Elden Ring's successor to objects).
            struct Asset : ModelOf<10> {
                Asset() { Name = "AEGxxx_xxx"; }
            };

            // Unknown; probably some kind of version number.
            int32_t Version = 73;
            std::vector<MapPiece> MapPieces;
            std::vector<Enemy> Enemies;
            std::vector<Player> Players;
            std::vector<Collision> Collisions;
            std::vector<Asset> Assets;

            static constexpr const char* ParamName = "MODEL_PARAM_ST";

            template<typename F>
            void ForEachList(F&& Visit) {
                Visit(MapPieces);
                Visit(Enemies);
                Visit(Players);
                Visit(Collisions);
                Visit(Assets);
            }
            template<typename F>
            void ForEachList(F&& Visit) const {
                Visit(MapPieces);
                Visit(Enemies);
                Visit(Players);
                Visit(Collisions);
                Visit(Assets);
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
            uint32_t EntityID = 0;
            int32_t Unk14     = 0;
            uint8_t UnkE0C    = 0;
            int32_t MapID = 0, UnkS04 = 0, UnkS08 = 0, UnkS0C = 0;
            // The ID the entry had in the file, written back unchanged (some events' IDs follow no pattern); -1 for
            // entries made from scratch, which are numbered by their position.
            int32_t FileID = -1;

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
        struct EventImpl : TypeDataOf<D, Event, WithData> {
            static constexpr uint32_t TypeId = Id;
            uint32_t Type() const override { return Id; }
            bool HasTypeData() const override { return WithData; }
        };

        // Dynamic or interactive systems such as item pickups, levers, enemy spawners, etc.
        struct EventParam : Msb::Detail::TypedLists<EventParam, Event> {
            // An item pickup in the open or inside a container.
            struct Treasure : EventImpl<Treasure, 4> {
                // The part that the treasure is attached to.
                Msb::Ref<PartList> TreasurePartName;
                // The item lot to be given (ItemLotParam_map).
                int32_t ItemLotID = -1;
                // If not -1, uses an entry from ActionButtonParam for the pickup prompt.
                int32_t ActionButtonID = -1;
                // Animation to play when taking this treasure.
                int32_t PickupAnimID = -1;
                // Changes the text of the pickup prompt.
                uint8_t InChest = 0;
                // Whether the treasure should be hidden by default.
                uint8_t StartDisabled = 0;
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
                uint32_t ObjActEntityID = 0;
                // The part to be interacted with.
                Msb::Ref<PartList> ObjActPartName;
                // A row in ObjActParam.
                int32_t ObjActID     = -1;
                uint8_t StateType    = 0;
                uint32_t EventFlagID = 0;
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

            // Unknown.
            struct Navmesh : EventImpl<Navmesh, 10> {
                Msb::Ref<RegionList> NavmeshRegionName;
                Navmesh() { Name = "Event: Navmesh"; }
                template<typename V>
                void Fields(V& F) {
                    F(NavmeshRegionName);
                    F.Pad(12);
                }
            };

            // A fake multiplayer interaction where the player goes to an NPC's world.
            struct PseudoMultiplayer : EventImpl<PseudoMultiplayer, 12> {
                // The NPC whose world you're entering.
                uint32_t HostEntityID = 0;
                // Set when inside the event's region, unset when outside it.
                uint32_t EventFlagID = 0;
                // ID of a goods item that is used to trigger the event.
                int32_t ActivateGoodsID = -1;
                int32_t UnkT0C = -1, UnkT10 = -1, UnkT14 = 0, UnkT18 = 0, UnkT1C = 0;
                PseudoMultiplayer() { Name = "Event: PseudoMultiplayer"; }
                template<typename V>
                void Fields(V& F) {
                    F(HostEntityID);
                    F(EventFlagID);
                    F(ActivateGoodsID);
                    F(UnkT0C);
                    F(UnkT10);
                    F(UnkT14);
                    F(UnkT18);
                    F(UnkT1C);
                    F.template Const<int32_t>(-1);
                }
            };

            // Unknown.
            struct PlatoonInfo : EventImpl<PlatoonInfo, 15> {
                int32_t PlatoonIDScriptActivate = 0;
                int32_t State                   = 0;
                std::array<Msb::Ref<PartList>, 32> GroupPartNames;
                PlatoonInfo() { Name = "Event: PlatoonInfo"; }
                template<typename V>
                void Fields(V& F) {
                    F(PlatoonIDScriptActivate);
                    F(State);
                    F.Pad(8);
                    F(GroupPartNames);
                }
            };

            // A patrol route that enemies follow.
            struct PatrolInfo : EventImpl<PatrolInfo, 20> {
                // Determines patrol behavior: 0 returns to the first region on loop, 1 goes through the list
                // backwards on loop, etc. (PATROL_TYPE)
                uint8_t PatrolType = 0;
                std::array<Msb::Ref<RegionList, int16_t>, 64> WalkRegionNames;
                PatrolInfo() { Name = "Event: PatrolInfo"; }
                template<typename V>
                void Fields(V& F) {
                    F(PatrolType);
                    F.template Const<uint8_t>(0);
                    F.template Const<uint8_t>(0);
                    F.template Const<uint8_t>(1);
                    F.template Const<int32_t>(-1);
                    F.Pad(8);
                    F(WalkRegionNames);
                }
            };

            // Unknown.
            struct Mount : EventImpl<Mount, 21> {
                Msb::Ref<PartList> RiderPartName;
                Msb::Ref<PartList> MountPartName;
                Mount() { Name = "Event: Mount"; }
                template<typename V>
                void Fields(V& F) {
                    F(RiderPartName);
                    F(MountPartName);
                }
            };

            // Unknown.
            struct SignPool : EventImpl<SignPool, 23> {
                Msb::Ref<PartList> SignPartName;
                // A row in SignPuddleParam.
                int32_t SignPuddleParamID = 0;
                SignPool() { Name = "Event: SignPool"; }
                template<typename V>
                void Fields(V& F) {
                    F(SignPartName);
                    F(SignPuddleParamID);
                    F.Pad(8);
                }
            };

            // A stake of Marika: where the player can choose to respawn.
            struct RetryPoint : EventImpl<RetryPoint, 24> {
                // The asset that represents the stake.
                Msb::Ref<PartList> RetryPartName;
                // Flag that must be set for the stake to be available.
                uint32_t TriggerEventFlagID = 0;
                // The distance at which the retry point is active (only used if no retry region is set).
                float RetryDistance = 0;
                // The region in which the retry point is active.
                Msb::Ref<RegionList, int16_t> RetryRegionName;
                RetryPoint() { Name = "Event: RetryPoint"; }
                template<typename V>
                void Fields(V& F) {
                    F(RetryPartName);
                    F(TriggerEventFlagID);
                    F(RetryDistance);
                    F(RetryRegionName);
                    F.template Const<int16_t>(0);
                }
            };

            // Unknown.
            struct AreaTeam : EventImpl<AreaTeam, 25> {
                int32_t EntityIDLeader = 0, EUnk04 = 0, EUnk08 = 0, EUnk0C = 0;
                int32_t RegionIDLeader = 0, RegionIDGuest1 = 0, RegionIDGuest2 = 0;
                int32_t EUnk1C = 0, EUnk20 = 0, EUnk24 = 0, EUnk28 = 0;
                AreaTeam() { Name = "Event: AreaTeam"; }
                template<typename V>
                void Fields(V& F) {
                    F(EntityIDLeader);
                    F(EUnk04);
                    F(EUnk08);
                    F(EUnk0C);
                    F(RegionIDLeader);
                    F(RegionIDGuest1);
                    F(RegionIDGuest2);
                    F(EUnk1C);
                    F(EUnk20);
                    F(EUnk24);
                    F(EUnk28);
                }
            };

            // Unknown.
            struct Other : EventImpl<Other, 0xFFFFFFFF, false> {
                Other() { Name = "Event: Other"; }
            };

            int32_t Version = 73;
            std::vector<Treasure> Treasures;
            std::vector<Generator> Generators;
            std::vector<ObjAct> ObjActs;
            std::vector<Navmesh> Navmeshes;
            std::vector<PseudoMultiplayer> PseudoMultiplayers;
            std::vector<PlatoonInfo> PlatoonInfos;
            std::vector<PatrolInfo> PatrolInfos;
            std::vector<Mount> Mounts;
            std::vector<SignPool> SignPools;
            std::vector<RetryPoint> RetryPoints;
            std::vector<AreaTeam> AreaTeams;
            std::vector<Other> Others;

            static constexpr const char* ParamName = "EVENT_PARAM_ST";

            template<typename F>
            void ForEachList(F&& Visit) {
                Visit(Treasures);
                Visit(Generators);
                Visit(ObjActs);
                Visit(Navmeshes);
                Visit(PseudoMultiplayers);
                Visit(PlatoonInfos);
                Visit(PatrolInfos);
                Visit(Mounts);
                Visit(SignPools);
                Visit(RetryPoints);
                Visit(AreaTeams);
                Visit(Others);
            }
            template<typename F>
            void ForEachList(F&& Visit) const {
                Visit(Treasures);
                Visit(Generators);
                Visit(ObjActs);
                Visit(Navmeshes);
                Visit(PseudoMultiplayers);
                Visit(PlatoonInfos);
                Visit(PatrolInfos);
                Visit(Mounts);
                Visit(SignPools);
                Visit(RetryPoints);
                Visit(AreaTeams);
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
            // Presumed ID for regions; unique per map, incremented per region.
            int32_t RegionID = 0;
            int32_t Unk40    = 0;
            // Controls whether the region is active in different ceremonies.
            uint32_t MapStudioLayer = 0xFFFFFFFF;
            std::vector<int16_t> UnkA, UnkB;
            // If specified, the region is only active when the part is loaded.
            Msb::Ref<PartList> ActivationPartName;
            // Identifies the region in event scripts.
            uint32_t EntityID = 0;
            uint8_t UnkE08    = 0;
            int32_t MapID = 0, UnkS04 = 0, UnkS0C = 0;

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
        struct RegionImpl : TypeDataOf<D, Region, WithData> {
            static constexpr uint32_t TypeId = Id;
            uint32_t Type() const override { return Id; }
            bool HasTypeData() const override { return WithData; }
        };

        // Points and volumes used to trigger various effects.
        struct PointParam : Msb::Detail::TypedLists<PointParam, Region> {
            // A point where a player can invade your world.
            struct InvasionPoint : RegionImpl<InvasionPoint, 1> {
                int32_t Priority = 0;
                InvasionPoint() { Name = "Region: InvasionPoint"; }
                template<typename V>
                void Fields(V& F) {
                    F(Priority);
                }
            };

            // Unknown.
            struct EnvironmentMapPoint : RegionImpl<EnvironmentMapPoint, 2> {
                float UnkT00   = 0;
                int32_t UnkT04 = 0;
                bool UnkT0D = false, UnkT0E = false, UnkT0F = false;
                float UnkT10 = 0, UnkT14 = 0;
                std::array<uint8_t, 4> UnkMapID{};
                int32_t UnkT20 = 0, UnkT24 = 0, UnkT28 = 0;
                uint8_t UnkT2C = 0, UnkT2D = 0;
                EnvironmentMapPoint() { Name = "Region: EnvironmentMapPoint"; }
                template<typename V>
                void Fields(V& F) {
                    F(UnkT00);
                    F(UnkT04);
                    F.template Const<int32_t>(-1);
                    F.template Const<uint8_t>(0);
                    F(UnkT0D);
                    F(UnkT0E);
                    F(UnkT0F);
                    F(UnkT10);
                    F(UnkT14);
                    F(UnkMapID);
                    F.template Const<int32_t>(0);
                    F(UnkT20);
                    F(UnkT24);
                    F(UnkT28);
                    F(UnkT2C);
                    F(UnkT2D);
                    F.template Const<int16_t>(0);
                }
            };

            // An area where a sound plays.
            struct Sound : RegionImpl<Sound, 4> {
                // The category of the sound.
                int32_t SoundType = 0;
                // The ID of the sound.
                int32_t SoundID = 0;
                // References to other regions used to build a composite shape.
                std::array<Msb::Ref<RegionList>, 16> ChildRegionNames;
                bool UnkT49 = false;
                Sound() { Name = "Region: Sound"; }
                template<typename V>
                void Fields(V& F) {
                    F(SoundType);
                    F(SoundID);
                    F(ChildRegionNames);
                    F.template Const<uint8_t>(0);
                    F(UnkT49);
                    F.template Const<int16_t>(0);
                }
            };

            // A point where a particle effect can play.
            struct SFX : RegionImpl<SFX, 5> {
                // The ID of the particle effect FFX.
                int32_t EffectID = 0;
                // If not 0, the effect is off until enabled by event scripts.
                int32_t StartDisabled = 0;
                SFX() { Name = "Region: SFX"; }
                template<typename V>
                void Fields(V& F) {
                    F(EffectID);
                    F(StartDisabled);
                }
            };

            // Unknown.
            struct WindSFX : RegionImpl<WindSFX, 6> {
                // ID of the effect FFX.
                int32_t EffectID = 0;
                // Reference to a WindArea region.
                Msb::Ref<RegionList> WindAreaName;
                float UnkT08 = 0;
                WindSFX() { Name = "Region: WindSFX"; }
                template<typename V>
                void Fields(V& F) {
                    F(EffectID);
                    F(WindAreaName);
                    F(UnkT08);
                }
            };

            // A point where the player can spawn into the map.
            struct SpawnPoint : RegionImpl<SpawnPoint, 8> {
                SpawnPoint() { Name = "Region: SpawnPoint"; }
                template<typename V>
                void Fields(V& F) {
                    F.template Const<int32_t>(-1);
                    F.Pad(12);
                }
            };

            // A developer message.
            struct Message : RegionImpl<Message, 9> {
                // ID of the message's text in the FMGs.
                int16_t MessageID = -1;
                // Unknown; always 0 or 2.
                int16_t UnkT02 = 0;
                // Whether the message requires Seek Guidance to appear.
                bool Hidden            = false;
                int32_t UnkT08         = 0;
                int32_t MessageSfxID   = 0;
                // Event flag required to be on for the message to appear.
                uint32_t EnableEventFlagID = 0;
                // ID of a character to render along with the message.
                int32_t CharacterModelName = 0;
                // NpcParam ID to use when rendering a character with the message.
                int32_t NPCParamID = 0;
                // Animation ID to use when rendering a character with the message.
                int32_t AnimationID      = 0;
                int32_t CharaInitParamID = 0;
                Message() { Name = "Region: Message"; }
                template<typename V>
                void Fields(V& F) {
                    F(MessageID);
                    F(UnkT02);
                    F.AsInt32(Hidden);
                    F(UnkT08);
                    F(MessageSfxID);
                    F(EnableEventFlagID);
                    F(CharacterModelName);
                    F(NPCParamID);
                    F(AnimationID);
                    F(CharaInitParamID);
                }
            };

            // Unknown.
            struct EnvironmentMapEffectBox : RegionImpl<EnvironmentMapEffectBox, 17> {
                // Distance from the camera required before enabling the envmap; 0 is always enabled.
                float EnableDist = 0;
                // Distance it takes for an envmap to fully transition into view.
                float TransitionDist = 0;
                uint8_t UnkT08 = 0, UnkT09 = 0;
                int16_t UnkT0A = 0;
                // Strength of specular light in the region.
                float SpecularLightMult = 0;
                // Strength of direct light emitting from the EnvironmentMapPoint.
                float PointLightMult = 0;
                int16_t UnkT2C       = 0;
                // Affects lighting with other fields when true; possibly normalizes light when false.
                bool IsModifyLight = false;
                bool UnkT2F        = false;
                int16_t UnkT30     = 0;
                bool UnkT32 = false, UnkT33 = false;
                int16_t UnkT34 = 0, UnkT36 = 0;
                EnvironmentMapEffectBox() { Name = "Region: EnvironmentMapEffectBox"; }
                template<typename V>
                void Fields(V& F) {
                    F(EnableDist);
                    F(TransitionDist);
                    F(UnkT08);
                    F(UnkT09);
                    F(UnkT0A);
                    F.Pad(0x18);
                    F(SpecularLightMult);
                    F(PointLightMult);
                    F(UnkT2C);
                    F(IsModifyLight);
                    F(UnkT2F);
                    F(UnkT30);
                    F(UnkT32);
                    F(UnkT33);
                    F(UnkT34);
                    F(UnkT36);
                    F.template Const<int32_t>(0);
                }
            };

            // Regions with no type-specific data.
            struct WindArea : RegionImpl<WindArea, 18, false> {  // unknown
                WindArea() { Name = "Region: WindArea"; }
            };

            // Used to align different maps.
            struct Connection : RegionImpl<Connection, 21> {
                // The map this connection targets.
                std::array<int8_t, 4> TargetMapID{};
                Connection() { Name = "Region: Connection"; }
                template<typename V>
                void Fields(V& F) {
                    F(TargetMapID);
                    F.Pad(12);
                }
            };

            // Unknown.
            struct PatrolRoute22 : RegionImpl<PatrolRoute22, 22> {
                PatrolRoute22() { Name = "Region: PatrolRoute22"; }
                template<typename V>
                void Fields(V& F) {
                    F.template Const<int32_t>(-1);
                    F.template Const<int32_t>(0);
                }
            };

            // Unknown.
            struct BuddySummonPoint : RegionImpl<BuddySummonPoint, 26> {
                BuddySummonPoint() { Name = "Region: BuddySummonPoint"; }
                template<typename V>
                void Fields(V& F) {
                    F.Pad(16);
                }
            };

            // Unknown.
            struct DisableTumbleweed : RegionImpl<DisableTumbleweed, 27> {
                DisableTumbleweed() { Name = "Region: DisableTumbleweed"; }
                template<typename V>
                void Fields(V& F) {
                    F.template Const<int32_t>(-1);
                    F.Pad(8);
                }
            };

            // An area where sound is muffled.
            struct MufflingBox : RegionImpl<MufflingBox, 28> {
                int32_t UnkT00 = 0;
                float UnkT24 = 0, UnkT34 = 0, UnkT3C = 0, UnkT40 = 0, UnkT44 = 0;
                MufflingBox() { Name = "Region: MufflingBox"; }
                template<typename V>
                void Fields(V& F) {
                    F(UnkT00);
                    F.Pad(20);
                    F.template Const<int32_t>(32);
                    F.Pad(8);
                    F(UnkT24);
                    F.Pad(12);
                    F(UnkT34);
                    F.Pad(4);
                    F(UnkT3C);
                    F(UnkT40);
                    F(UnkT44);
                }
            };

            // An entrance to a muffling box.
            struct MufflingPortal : RegionImpl<MufflingPortal, 29> {
                int32_t UnkT00 = 0;
                MufflingPortal() { Name = "Region: MufflingPortal"; }
                template<typename V>
                void Fields(V& F) {
                    F(UnkT00);
                    F.Pad(20);
                    F.template Const<int32_t>(32);
                    F.Pad(24);
                    F.template Const<int32_t>(-1);
                }
            };

            // Unknown.
            struct SoundRegion : RegionImpl<SoundRegion, 30> {
                uint8_t UnkT00 = 0, UnkT01 = 0, UnkT02 = 0, UnkT03 = 0;
                int32_t UnkT04 = 0;
                int16_t UnkT08 = 0, UnkT0A = 0;
                uint8_t UnkT0C = 0;
                SoundRegion() { Name = "Region: SoundRegion"; }
                template<typename V>
                void Fields(V& F) {
                    F(UnkT00);
                    F(UnkT01);
                    F(UnkT02);
                    F(UnkT03);
                    F(UnkT04);
                    F(UnkT08);
                    F(UnkT0A);
                    F(UnkT0C);
                    F.Pad(19);
                }
            };

            struct MufflingPlane : RegionImpl<MufflingPlane, 31, false> {  // unknown
                MufflingPlane() { Name = "Region: MufflingPlane"; }
            };

            // A point along an NPC patrol path.
            struct PatrolRoute : RegionImpl<PatrolRoute, 32> {
                int32_t UnkT00 = 0;
                PatrolRoute() { Name = "Region: PatrolRoute"; }
                template<typename V>
                void Fields(V& F) {
                    F(UnkT00);
                }
            };

            // Unknown.
            struct MapPoint : RegionImpl<MapPoint, 33> {
                // A row in WorldMapPointParam.
                int32_t WorldMapPointParamID = 0;
                int32_t UnkT04               = 0;
                float UnkT08 = 0, UnkT0C = 0;
                int32_t UnkT10 = 0;
                float UnkT14 = 0, UnkT18 = 0;
                MapPoint() { Name = "Region: MapPoint"; }
                template<typename V>
                void Fields(V& F) {
                    F(WorldMapPointParamID);
                    F(UnkT04);
                    F(UnkT08);
                    F(UnkT0C);
                    F(UnkT10);
                    F(UnkT14);
                    F(UnkT18);
                    F.template Const<int32_t>(0);
                }
            };

            // Unknown.
            struct WeatherOverride : RegionImpl<WeatherOverride, 35> {
                // A row in WeatherLotParam.
                int32_t WeatherLotParamID = 0;
                uint8_t UnkT08 = 0, UnkT09 = 0, UnkT0A = 0, UnkT0B = 0;
                WeatherOverride() { Name = "Region: WeatherOverride"; }
                template<typename V>
                void Fields(V& F) {
                    F(WeatherLotParamID);
                    F.template Const<int32_t>(-1);
                    F(UnkT08);
                    F(UnkT09);
                    F(UnkT0A);
                    F(UnkT0B);
                    F.Pad(20);
                }
            };

            // Unknown.
            struct AutoDrawGroupPoint : RegionImpl<AutoDrawGroupPoint, 36> {
                int32_t UnkT00 = 0;
                AutoDrawGroupPoint() { Name = "Region: AutoDrawGroupPoint"; }
                template<typename V>
                void Fields(V& F) {
                    F(UnkT00);
                    F.Pad(28);
                }
            };

            // Unknown.
            struct GroupDefeatReward : RegionImpl<GroupDefeatReward, 37> {
                int32_t UnkT00 = 0, UnkT04 = 0, UnkT08 = 0;
                // Enemies to defeat to receive the reward.
                std::array<Msb::Ref<PartList>, 8> PartNames;
                int32_t UnkT34 = 0, UnkT38 = 0;
                // A row in HPEstusFlaskRecoveryParam / MPEstusFlaskRecoveryParam.
                int32_t EstusFlaskRecoveryID = 0;
                GroupDefeatReward() { Name = "Region: GroupDefeatReward"; }
                template<typename V>
                void Fields(V& F) {
                    F(UnkT00);
                    F(UnkT04);
                    F(UnkT08);
                    F.template Const<int32_t>(-1);
                    F.template Const<int32_t>(-1);
                    F(PartNames);
                    F(UnkT34);
                    F(UnkT38);
                    F.Pattern(24, 0xFF);
                    F(EstusFlaskRecoveryID);
                    F.Pad(8);
                }
            };

            struct MapPointDiscoveryOverride : RegionImpl<MapPointDiscoveryOverride, 38, false> {  // unknown
                MapPointDiscoveryOverride() { Name = "Region: MapPointDiscoveryOverride"; }
            };
            struct MapPointParticipationOverride : RegionImpl<MapPointParticipationOverride, 39, false> {  // unknown
                MapPointParticipationOverride() { Name = "Region: MapPointParticipationOverride"; }
            };

            // Unknown.
            struct Hitset : RegionImpl<Hitset, 40> {
                int32_t UnkT00 = 0;
                Hitset() { Name = "Region: Hitset"; }
                template<typename V>
                void Fields(V& F) {
                    F(UnkT00);
                }
            };

            // A region that disables fast travel.
            struct FastTravelRestriction : RegionImpl<FastTravelRestriction, 41> {
                // Fast travel is disabled while this flag is off.
                uint32_t EventFlagID = 0;
                FastTravelRestriction() { Name = "Region: FastTravelRestriction"; }
                template<typename V>
                void Fields(V& F) {
                    F(EventFlagID);
                    F.template Const<int32_t>(0);
                }
            };

            // Unknown.
            struct WeatherCreateAssetPoint : RegionImpl<WeatherCreateAssetPoint, 42> {
                WeatherCreateAssetPoint() { Name = "Region: WeatherCreateAssetPoint"; }
                template<typename V>
                void Fields(V& F) {
                    F.template Const<int32_t>(0);
                }
            };

            // Unknown.
            struct PlayArea : RegionImpl<PlayArea, 43> {
                // A row in PlayRegionParam.
                int32_t PlayRegionID = 0;
                int32_t UnkT04       = 0;
                PlayArea() { Name = "Region: PlayArea"; }
                template<typename V>
                void Fields(V& F) {
                    F(PlayRegionID);
                    F(UnkT04);
                }
            };

            struct EnvironmentMapOutput : RegionImpl<EnvironmentMapOutput, 44, false> {  // unknown
                EnvironmentMapOutput() { Name = "Region: EnvironmentMapOutput"; }
            };

            // A point where Torrent can jump high.
            struct MountJump : RegionImpl<MountJump, 46> {
                // How far the player moves upwards when activating it.
                float JumpHeight = 0;
                int32_t UnkT04   = 0;
                MountJump() { Name = "Region: MountJump"; }
                template<typename V>
                void Fields(V& F) {
                    F(JumpHeight);
                    F(UnkT04);
                }
            };

            // Unknown.
            struct Dummy : RegionImpl<Dummy, 48> {
                int32_t UnkT00 = 0;
                Dummy() { Name = "Region: Dummy"; }
                template<typename V>
                void Fields(V& F) {
                    F(UnkT00);
                    F.template Const<int32_t>(0);
                }
            };

            // Unknown.
            struct FallPreventionRemoval : RegionImpl<FallPreventionRemoval, 49> {
                FallPreventionRemoval() { Name = "Region: FallPreventionRemoval"; }
                template<typename V>
                void Fields(V& F) {
                    F.Pad(8);
                }
            };

            // Unknown.
            struct NavmeshCutting : RegionImpl<NavmeshCutting, 50> {
                NavmeshCutting() { Name = "Region: NavmeshCutting"; }
                template<typename V>
                void Fields(V& F) {
                    F.Pad(8);
                }
            };

            // Changes the name shown for the area.
            struct MapNameOverride : RegionImpl<MapNameOverride, 51> {
                // FMG ID for the area title card; negative values apply when entering, positive ones on map load.
                int32_t TextID = 0;
                MapNameOverride() { Name = "Region: MapNameOverride"; }
                template<typename V>
                void Fields(V& F) {
                    F(TextID);
                    F.template Const<int32_t>(0);
                }
            };

            // Unknown.
            struct MountJumpFall : RegionImpl<MountJumpFall, 52> {
                MountJumpFall() { Name = "Region: MountJumpFall"; }
                template<typename V>
                void Fields(V& F) {
                    F.template Const<int32_t>(-1);
                    F.template Const<int32_t>(0);
                }
            };

            // Affects where Torrent can be summoned.
            struct HorseRideOverride : RegionImpl<HorseRideOverride, 53> {
                // 1 forbids riding Torrent, 2 permits it.
                uint32_t OverrideType = 1;
                HorseRideOverride() { Name = "Region: HorseRideOverride"; }
                template<typename V>
                void Fields(V& F) {
                    F(OverrideType);
                    F.template Const<int32_t>(0);
                }
            };

            // Unknown.
            struct LockedMountJump : RegionImpl<LockedMountJump, 54> {
                // How far the player moves upwards when activating it.
                float JumpHeight = 0;
                int32_t UnkT04   = 0;
                // Probably an event flag that enables it.
                int32_t UnkT08 = 0;
                LockedMountJump() { Name = "Region: LockedMountJump"; }
                template<typename V>
                void Fields(V& F) {
                    F(JumpHeight);
                    F(UnkT04);
                    F(UnkT08);
                    F.template Const<int32_t>(-1);
                }
            };

            // Unknown.
            struct LockedMountJumpFall : RegionImpl<LockedMountJumpFall, 55> {
                // Probably an event flag that enables it.
                int32_t UnkT08 = 0;
                LockedMountJumpFall() { Name = "Region: LockedMountJumpFall"; }
                template<typename V>
                void Fields(V& F) {
                    F.template Const<int32_t>(-1);
                    F.template Const<int32_t>(0);
                    F(UnkT08);
                }
            };

            // Most likely an unused region.
            struct Other : RegionImpl<Other, 0xFFFFFFFF, false> {
                Other() { Name = "Region: Other"; }
            };

            int32_t Version = 73;
            std::vector<InvasionPoint> InvasionPoints;
            std::vector<EnvironmentMapPoint> EnvironmentMapPoints;
            std::vector<Sound> Sounds;
            std::vector<SFX> SFXs;
            std::vector<WindSFX> WindSFXs;
            std::vector<SpawnPoint> SpawnPoints;
            std::vector<Message> Messages;
            std::vector<EnvironmentMapEffectBox> EnvironmentMapEffectBoxes;
            std::vector<WindArea> WindAreas;
            std::vector<Connection> Connections;
            std::vector<PatrolRoute22> PatrolRoute22s;
            std::vector<BuddySummonPoint> BuddySummonPoints;
            std::vector<DisableTumbleweed> DisableTumbleweeds;
            std::vector<MufflingBox> MufflingBoxes;
            std::vector<MufflingPortal> MufflingPortals;
            std::vector<SoundRegion> SoundRegions;
            std::vector<MufflingPlane> MufflingPlanes;
            std::vector<PatrolRoute> PatrolRoutes;
            std::vector<MapPoint> MapPoints;
            std::vector<WeatherOverride> WeatherOverrides;
            std::vector<AutoDrawGroupPoint> AutoDrawGroupPoints;
            std::vector<GroupDefeatReward> GroupDefeatRewards;
            std::vector<MapPointDiscoveryOverride> MapPointDiscoveryOverrides;
            std::vector<MapPointParticipationOverride> MapPointParticipationOverrides;
            std::vector<Hitset> Hitsets;
            std::vector<FastTravelRestriction> FastTravelRestrictions;
            std::vector<WeatherCreateAssetPoint> WeatherCreateAssetPoints;
            std::vector<PlayArea> PlayAreas;
            std::vector<EnvironmentMapOutput> EnvironmentMapOutputs;
            std::vector<MountJump> MountJumps;
            std::vector<Dummy> Dummies;
            std::vector<FallPreventionRemoval> FallPreventionRemovals;
            std::vector<NavmeshCutting> NavmeshCuttings;
            std::vector<MapNameOverride> MapNameOverrides;
            std::vector<MountJumpFall> MountJumpFalls;
            std::vector<HorseRideOverride> HorseRideOverrides;
            std::vector<LockedMountJump> LockedMountJumps;
            std::vector<LockedMountJumpFall> LockedMountJumpFalls;
            std::vector<Other> Others;

            static constexpr const char* ParamName = "POINT_PARAM_ST";

            template<typename Self, typename F>
            static void VisitLists(Self& P, F&& Visit) {
                Visit(P.InvasionPoints);
                Visit(P.EnvironmentMapPoints);
                Visit(P.Sounds);
                Visit(P.SFXs);
                Visit(P.WindSFXs);
                Visit(P.SpawnPoints);
                Visit(P.Messages);
                Visit(P.EnvironmentMapEffectBoxes);
                Visit(P.WindAreas);
                Visit(P.Connections);
                Visit(P.PatrolRoute22s);
                Visit(P.BuddySummonPoints);
                Visit(P.DisableTumbleweeds);
                Visit(P.MufflingBoxes);
                Visit(P.MufflingPortals);
                Visit(P.SoundRegions);
                Visit(P.MufflingPlanes);
                Visit(P.PatrolRoutes);
                Visit(P.MapPoints);
                Visit(P.WeatherOverrides);
                Visit(P.AutoDrawGroupPoints);
                Visit(P.GroupDefeatRewards);
                Visit(P.MapPointDiscoveryOverrides);
                Visit(P.MapPointParticipationOverrides);
                Visit(P.Hitsets);
                Visit(P.FastTravelRestrictions);
                Visit(P.WeatherCreateAssetPoints);
                Visit(P.PlayAreas);
                Visit(P.EnvironmentMapOutputs);
                Visit(P.MountJumps);
                Visit(P.Dummies);
                Visit(P.FallPreventionRemovals);
                Visit(P.NavmeshCuttings);
                Visit(P.MapNameOverrides);
                Visit(P.MountJumpFalls);
                Visit(P.HorseRideOverrides);
                Visit(P.LockedMountJumps);
                Visit(P.LockedMountJumpFalls);
                Visit(P.Others);
            }
            template<typename F>
            void ForEachList(F&& Visit) {
                VisitLists(*this, Visit);
            }
            template<typename F>
            void ForEachList(F&& Visit) const {
                VisitLists(*this, Visit);
            }
        };
#pragma endregion

#pragma region Routes
        // Unknown, but related to muffling regions somehow.
        struct SOULS_API Route : Entry {
            int32_t Unk08 = 0;
            int32_t Unk0C = 0;
            // The ID the entry had in the file, written back unchanged; -1 for entries made from scratch.
            int32_t FileID = -1;

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
            // Unknown.
            struct Other : RouteOf<0xFFFFFFFF> {
                Other() { Name = "X-X"; }
            };

            int32_t Version = 73;
            std::vector<MufflingPortalLink> MufflingPortalLinks;
            std::vector<MufflingBoxLink> MufflingBoxLinks;
            std::vector<Other> Others;

            static constexpr const char* ParamName = "ROUTE_PARAM_ST";

            template<typename F>
            void ForEachList(F&& Visit) {
                Visit(MufflingPortalLinks);
                Visit(MufflingBoxLinks);
                Visit(Others);
            }
            template<typename F>
            void ForEachList(F&& Visit) const {
                Visit(MufflingPortalLinks);
                Visit(MufflingBoxLinks);
                Visit(Others);
            }
        };

        // Used for unused params that should never have any entries in them.
        struct EmptyParam {
            // Unknown; probably some kind of version number.
            int32_t Version = 0;
        };
#pragma endregion

#pragma region Parts
        // Unknown; holds the part's display and draw groups.
        struct UnkStruct1 {
            std::array<uint32_t, 8> DisplayGroups{};
            std::array<uint32_t, 8> DrawGroups{};
            std::array<uint32_t, 32> CollisionMask{};
            uint8_t Condition1 = 0, Condition2 = 0, UnkC2 = 0, UnkC3 = 0;
            int16_t UnkC4 = 0, UnkC6 = 0;
            template<typename V>
            void Fields(V& F) {
                F(DisplayGroups);
                F(DrawGroups);
                F(CollisionMask);
                F(Condition1);
                F(Condition2);
                F(UnkC2);
                F(UnkC3);
                F(UnkC4);
                F(UnkC6);
                F.Pad(0xC0);
            }
        };

        // Unknown.
        struct UnkStruct2 {
            int32_t Condition = 0;
            std::array<uint32_t, 8> DispGroups{};
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

        // Overrides of the gparams used while on a collision.
        struct SceneGparamConfig {
            float TransitionTime = 0;
            // The hundredths place of a gparam ID to use, and the ones to override it with, in order.
            int8_t GparamSubIDBase = -1, GparamSubIDOverride1 = -1, GparamSubIDOverride2 = -1, GparamSubIDOverride3 = -1;
            int8_t Unk1C = -1, Unk1D = -1, Unk20 = -1, Unk21 = -1;
            template<typename V>
            void Fields(V& F) {
                F.Pad(16);
                F(TransitionTime);
                F.template Const<int32_t>(0);
                F(GparamSubIDBase);
                F(GparamSubIDOverride1);
                F(GparamSubIDOverride2);
                F(GparamSubIDOverride3);
                F(Unk1C);
                F(Unk1D);
                F.Pad(2);
                F(Unk20);
                F(Unk21);
                F.Pad(2);
                F.Pad(44);
            }
        };

        // Grass types used on a part (rows in GrassTypeParam).
        struct GrassConfig {
            std::array<int32_t, 6> GrassParamIDs{};
            int32_t Unk18 = 0;
            template<typename V>
            void Fields(V& F) {
                F(GrassParamIDs);
                F(Unk18);
                F.template Const<int32_t>(0);
            }
        };

        // Unknown.
        struct UnkStruct8 {
            int32_t Unk00 = 0;
            template<typename V>
            void Fields(V& F) {
                F(Unk00);
                F.Pad(28);
            }
        };

        // Unknown.
        struct UnkStruct9 {
            int32_t Unk00 = 0;
            template<typename V>
            void Fields(V& F) {
                F(Unk00);
                F.Pad(28);
            }
        };

        // Unknown; related to which map tile loads the part.
        struct TileLoadConfig {
            std::array<uint8_t, 4> MapID{};
            int32_t Unk04 = 0, Unk0C = 0, Unk10 = 0;
            // Unknown culling behaviour for extreme height differences; 0 to 10000.
            int32_t CullingHeightBehavior = 0;
            template<typename V>
            void Fields(V& F) {
                F(MapID);
                F(Unk04);
                F.template Const<int32_t>(0);
                F(Unk0C);
                F(Unk10);
                F(CullingHeightBehavior);
                F.Pad(8);
            }
        };

        // Unknown.
        struct UnkStruct11 {
            int32_t Unk00 = 0, Unk04 = 0;
            template<typename V>
            void Fields(V& F) {
                F(Unk00);
                F(Unk04);
                F.Pad(24);
            }
        };

        // Which optional blocks of data a part type has; the block of each bit is the member of the same name.
        enum PartBlocks : unsigned {
            NoBlocks       = 0,
            HasUnk1        = 1 << 0,
            HasUnk2        = 1 << 1,
            HasGparam      = 1 << 2,
            HasSceneGparam = 1 << 3,
            HasGrass       = 1 << 4,
            HasUnk8        = 1 << 5,
            HasUnk9        = 1 << 6,
            HasTileLoad    = 1 << 7,
            HasUnk11       = 1 << 8,
        };

        // Common data for all types of part.
        struct SOULS_API Part : Entry {
            // The model used by this part; requires an entry in ModelParam.
            Msb::Ref<ModelList> ModelName;
            // Involved with serialization.
            int32_t InstanceID = 0;
            // A path to a .sib file, presumably some kind of editor placeholder.
            std::string SibPath;
            Vector3 Position;
            Vector3 Rotation;
            // Scale of the part; only works for map pieces and assets.
            Vector3 Scale{1.f, 1.f, 1.f};
            // 0 never disables the part; 1 disables it in all released versions of the game; 2 and 3 are unknown.
            uint32_t GameEditionDisable = 0;
            uint32_t MapStudioLayer     = 0;
            // Identifies the part in event scripts.
            uint32_t EntityID = 0;
            // If set, PartsDrawParamID is used; otherwise the asset's own param is.
            uint8_t IsUsePartsDrawParamID = 0;
            // A row in PartsDrawParam.
            int16_t PartsDrawParamID     = 0;
            int8_t IsPointLightShadowSrc = 0;
            uint8_t UnkE0B               = 0;
            bool IsShadowSrc             = false;
            uint8_t IsStaticShadowSrc = 0, IsCascade3ShadowSrc = 0, UnkE0F = 0, UnkE10 = 0;
            bool IsShadowDest = false, IsShadowOnly = false, DrawByReflectCam = false, DrawOnlyReflectCam = false;
            uint8_t EnableOnAboveShadow  = 0;
            bool DisablePointLightEffect = false;
            uint8_t UnkE17               = 0;
            int32_t UnkE18               = 0;
            // Allows multiple parts to be identified by the same entity ID.
            std::array<uint32_t, 8> EntityGroupIDs{};
            int16_t UnkE3C = 0, UnkE3E = 0;
            // Display and draw groups and collision masks; every part type has this block.
            UnkStruct1 Unk1;

            virtual uint32_t Type() const   = 0;
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
        // the optional blocks it has; each block is the member of D with the bit's name (Unk1, Gparam, TileLoad...).
        template<typename D, uint32_t Id, typename B = Part>
        struct PartImpl : TypeDataOf<D, B, true> {
            static constexpr uint32_t TypeId = Id;
            uint32_t Type() const override { return Id; }
            unsigned Blocks() const override { return D::Block; }
            template<typename V>
            void VisitBlock(unsigned Bit, V& Visitor) {
                D& Self = *static_cast<D*>(this);
                if constexpr (D::Block & HasUnk1) if (Bit == HasUnk1) Self.Unk1.Fields(Visitor);
                if constexpr (D::Block & HasUnk2) if (Bit == HasUnk2) Self.Unk2.Fields(Visitor);
                if constexpr (D::Block & HasGparam) if (Bit == HasGparam) Self.Gparam.Fields(Visitor);
                if constexpr (D::Block & HasSceneGparam) if (Bit == HasSceneGparam) Self.SceneGparam.Fields(Visitor);
                if constexpr (D::Block & HasGrass) if (Bit == HasGrass) Self.Grass.Fields(Visitor);
                if constexpr (D::Block & HasUnk8) if (Bit == HasUnk8) Self.Unk8.Fields(Visitor);
                if constexpr (D::Block & HasUnk9) if (Bit == HasUnk9) Self.Unk9.Fields(Visitor);
                if constexpr (D::Block & HasTileLoad) if (Bit == HasTileLoad) Self.TileLoad.Fields(Visitor);
                if constexpr (D::Block & HasUnk11) if (Bit == HasUnk11) Self.Unk11.Fields(Visitor);
            }
            void ReadBlock(unsigned Block, BinaryReader& Reader) override {
                Msb::Detail::ReadVisitor V{Reader};
                VisitBlock(Block, V);
            }
            void WriteBlock(unsigned Block, BinaryWriter& Writer) override {
                Msb::Detail::WriteVisitor V{Writer};
                VisitBlock(Block, V);
            }
        };

        // Common base data for enemies and dummy enemies.
        struct EnemyBase : Part {
            GparamConfig Gparam;
            UnkStruct8 Unk8;
            TileLoadConfig TileLoad;
            // A row in NpcThinkParam that determines the enemy's AI characteristics.
            int32_t ThinkParamID = -1;
            // A row in NpcParam that determines a variety of enemy properties.
            int32_t NPCParamID = -1;
            // The enemy's talk script (ESD).
            int32_t TalkID    = -1;
            bool UnkT15       = false;
            int16_t PlatoonID = -1;
            // A row in CharaInitParam that determines a human's inventory and stats.
            int32_t CharaInitID = -1;
            // Should reference the collision the enemy starts on.
            Msb::Ref<PartList> CollisionPartName;
            // The patrol route (a PatrolInfo event) the enemy follows.
            Msb::Ref<PatrolInfoList, int16_t> WalkRouteName;
            int32_t UnkT24 = -1, UnkT28 = 0;
            // A row in ChrActivateConditionParam that affects when the enemy appears.
            int32_t ChrActivateCondParamID = 0;
            int32_t UnkT34                 = 0;
            int32_t BackupEventAnimID      = -1;
            int32_t UnkT3C                 = 0;
            // Rows in SpEffectSetParam applied to the enemy.
            std::array<int32_t, 4> SpEffectSetParamIDs{};
            float UnkT84 = 0;

            EnemyBase() { Name = "cXXXX_XXXX"; }
            template<typename V>
            void Fields(V& F) {
                F.Pad(8);
                F(ThinkParamID);
                F(NPCParamID);
                F(TalkID);
                F.template Const<uint8_t>(0);
                F(UnkT15);
                F(PlatoonID);
                F(CharaInitID);
                F(CollisionPartName);
                F(WalkRouteName);
                F.template Const<int16_t>(0);
                F(UnkT24);
                F(UnkT28);
                F(ChrActivateCondParamID);
                F.template Const<int32_t>(0);
                F(UnkT34);
                F(BackupEventAnimID);
                F(UnkT3C);
                F(SpEffectSetParamIDs);
                F.Pad(40);
                F.template Const<uint64_t>(0x80);
                F.template Const<int32_t>(0);
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
            // Fixed visual geometry; not used much in Elden Ring.
            struct MapPiece : PartImpl<MapPiece, 0> {
                static constexpr unsigned Block = HasUnk1 | HasGparam | HasGrass | HasUnk8 | HasUnk9 | HasTileLoad | HasUnk11;
                GparamConfig Gparam;
                GrassConfig Grass;
                UnkStruct8 Unk8;
                UnkStruct9 Unk9;
                TileLoadConfig TileLoad;
                UnkStruct11 Unk11;
                MapPiece() { Name = "mXXXXXX_XXXX"; }
                template<typename V>
                void Fields(V& F) {
                    F.Pad(8);
                }
            };

            // Any non-player character.
            struct Enemy : PartImpl<Enemy, 2, EnemyBase> {
                static constexpr unsigned Block = HasUnk1 | HasGparam | HasUnk8 | HasTileLoad;
            };

            // A spawn point for the player, or something.
            struct Player : PartImpl<Player, 4> {
                static constexpr unsigned Block = HasUnk1 | HasUnk8 | HasTileLoad;
                UnkStruct8 Unk8;
                TileLoadConfig TileLoad;
                int32_t Unk00 = 0;
                Player() { Name = "c0000_XXXX"; }
                template<typename V>
                void Fields(V& F) {
                    F(Unk00);
                    F.Pad(12);
                }
            };

            // Invisible but physical geometry.
            struct Collision : PartImpl<Collision, 5> {
                static constexpr unsigned Block = HasUnk1 | HasUnk2 | HasGparam | HasSceneGparam | HasUnk8 | HasTileLoad | HasUnk11;
                UnkStruct2 Unk2;
                GparamConfig Gparam;
                SceneGparamConfig SceneGparam;
                UnkStruct8 Unk8;
                TileLoadConfig TileLoad;
                UnkStruct11 Unk11;
                // Collision behavior: 8 standard, 9 camera only, 11 enemy only, 13 fall death camera, 15 kill, ...
                uint8_t HitFilterID = 8;
                uint8_t UnkT01 = 0, UnkT02 = 0;
                bool UnkT03    = false;
                float UnkT04   = 0;
                int32_t UnkT06 = 0;
                float UnkT14   = 0;
                // Location text to display when stepping onto this collision.
                int32_t LocationTextID = 0;
                int32_t UnkT1C         = 0;
                // Used to determine invasion eligibility (PlayRegionParam).
                int32_t PlayRegionID = 0;
                int16_t UnkT24 = 0, UnkT26 = 0;
                int32_t UnkT30 = 0;
                uint8_t UnkT34 = 0, UnkT35 = 0;
                // Disables summoning and riding Torrent.
                bool DisableTorrent = false;
                int16_t UnkT3C = 0, UnkT3E = 0;
                float UnkT40 = 0;
                // Disables fast travel unless this flag is set.
                uint32_t EnableFastTravelEventFlagID = 0;
                int16_t UnkT4C = 0, UnkT4E = 0;

                Collision() { Name = "hXXXXXX"; }
                template<typename V>
                void Fields(V& F) {
                    F(HitFilterID);
                    F(UnkT01);
                    F(UnkT02);
                    F(UnkT03);
                    F(UnkT04);
                    F(UnkT06);
                    F.Pad(8);
                    F(UnkT14);
                    F(LocationTextID);
                    F(UnkT1C);
                    F(PlayRegionID);
                    F(UnkT24);
                    F(UnkT26);
                    F.template Const<int32_t>(0);
                    F.template Const<int32_t>(-1);
                    F(UnkT30);
                    F(UnkT34);
                    F(UnkT35);
                    F(DisableTorrent);
                    F.template Const<uint8_t>(0);
                    F.template Const<int32_t>(-1);
                    F(UnkT3C);
                    F(UnkT3E);
                    F(UnkT40);
                    F.template Const<int32_t>(0);
                    F(EnableFastTravelEventFlagID);
                    F(UnkT4C);
                    F(UnkT4E);
                }
            };

            // An asset that either isn't used, or is used for a cutscene.
            struct DummyAsset : PartImpl<DummyAsset, 9> {
                static constexpr unsigned Block = HasUnk1 | HasGparam | HasUnk8 | HasTileLoad;
                GparamConfig Gparam;
                UnkStruct8 Unk8;
                TileLoadConfig TileLoad;
                int32_t UnkT18 = 0;
                DummyAsset() { Name = "AEGxxx_xxx_xxxx"; }
                template<typename V>
                void Fields(V& F) {
                    F.Pad(8);
                    F.template Const<int32_t>(-1);
                    F.template Const<int32_t>(0);
                    F.template Const<int32_t>(-1);
                    F.template Const<int32_t>(-1);
                    F(UnkT18);
                    F.template Const<int32_t>(-1);
                }
            };

            // An enemy that either isn't used, or is used for a cutscene.
            struct DummyEnemy : PartImpl<DummyEnemy, 10, EnemyBase> {
                static constexpr unsigned Block = HasUnk1 | HasGparam | HasUnk8 | HasTileLoad;
            };

            // References an actual collision and causes another map to be loaded while on it.
            struct ConnectCollision : PartImpl<ConnectCollision, 11> {
                static constexpr unsigned Block = HasUnk1 | HasUnk2 | HasUnk8 | HasTileLoad | HasUnk11;
                UnkStruct2 Unk2;
                UnkStruct8 Unk8;
                TileLoadConfig TileLoad;
                UnkStruct11 Unk11;
                // The collision part to attach to.
                Msb::Ref<CollisionList> CollisionName;
                // The map to load when on this collision.
                std::array<uint8_t, 4> MapID{};
                uint8_t UnkT08 = 0;
                bool UnkT09    = false;
                uint8_t UnkT0A = 0;
                bool UnkT0B    = false;
                ConnectCollision() { Name = "hXXXXXX_XXXX"; }
                template<typename V>
                void Fields(V& F) {
                    F(CollisionName);
                    F(MapID);
                    F(UnkT08);
                    F(UnkT09);
                    F(UnkT0A);
                    F(UnkT0B);
                    F.template Const<int32_t>(0);
                }
            };

            // A placed asset: Elden Ring's dynamic props and interactive things.
            struct Asset : PartImpl<Asset, 13> {
                static constexpr unsigned Block = HasUnk1 | HasUnk2 | HasGparam | HasGrass | HasUnk8 | HasUnk9 | HasTileLoad | HasUnk11;

                // Unknown.
                struct AssetUnkStruct1 {
                    int16_t Unk00 = 0;
                    bool Unk04    = false;
                    // Disables summoning and riding Torrent, but only when the asset isn't on a collision that does.
                    bool DisableTorrentAssetOnly = false;
                    int32_t Unk1C                = 0;
                    int16_t Unk24 = 0, Unk26 = 0;
                    int32_t Unk28 = 0, Unk2C = 0;
                    template<typename V>
                    void Fields(V& F) {
                        F(Unk00);
                        F.template Const<int16_t>(-1);
                        F(Unk04);
                        F(DisableTorrentAssetOnly);
                        F.template Const<int16_t>(-1);
                        F.Pad(8);
                        F.Pattern(12, 0xFF);
                        F(Unk1C);
                        F.template Const<int32_t>(0);
                        F(Unk24);
                        F(Unk26);
                        F(Unk28);
                        F(Unk2C);
                        F.Pad(16);
                    }
                };

                // Unknown.
                struct AssetUnkStruct2 {
                    int32_t Unk00 = 0, Unk04 = 0;
                    float Unk14   = 0;
                    uint8_t Unk1C = 0, Unk1D = 0, Unk1E = 0, Unk1F = 0;
                    template<typename V>
                    void Fields(V& F) {
                        F(Unk00);
                        F(Unk04);
                        F.template Const<int32_t>(-1);
                        F.Pad(8);
                        F(Unk14);
                        F.template Const<int32_t>(0);
                        F(Unk1C);
                        F(Unk1D);
                        F(Unk1E);
                        F(Unk1F);
                        F.Pad(32);
                    }
                };

                // Unknown.
                struct AssetUnkStruct3 {
                    int32_t Unk00 = 0;
                    float Unk04   = 0;
                    uint8_t Unk09 = 0, Unk0A = 0, Unk0B = 0;
                    int16_t Unk0C = 0, Unk0E = 0;
                    float Unk10   = 0;
                    // Disables the asset when this map is loaded.
                    std::array<int8_t, 4> DisableWhenMapLoadedMapID{};
                    int32_t Unk18 = 0, Unk1C = 0, Unk20 = 0;
                    uint8_t Unk24 = 0;
                    bool Unk25    = false;
                    int32_t Unk28 = 0;
                    template<typename V>
                    void Fields(V& F) {
                        F(Unk00);
                        F(Unk04);
                        F.template Const<int8_t>(-1);
                        F(Unk09);
                        F(Unk0A);
                        F(Unk0B);
                        F(Unk0C);
                        F(Unk0E);
                        F(Unk10);
                        F(DisableWhenMapLoadedMapID);
                        F(Unk18);
                        F(Unk1C);
                        F(Unk20);
                        F(Unk24);
                        F(Unk25);
                        F.Pad(2);
                        F(Unk28);
                        F.Pad(20);
                    }
                };

                // Unknown.
                struct AssetUnkStruct4 {
                    bool Unk00    = false;
                    uint8_t Unk01 = 0, Unk02 = 0;
                    bool Unk03    = false;
                    template<typename V>
                    void Fields(V& F) {
                        F(Unk00);
                        F(Unk01);
                        F(Unk02);
                        F(Unk03);
                        F.Pad(60);
                    }
                };

                UnkStruct2 Unk2;
                GparamConfig Gparam;
                GrassConfig Grass;
                UnkStruct8 Unk8;
                UnkStruct9 Unk9;
                TileLoadConfig TileLoad;
                UnkStruct11 Unk11;
                int16_t UnkT00 = 0, UnkT02 = 0;
                uint8_t UnkT10 = 0;
                bool UnkT11    = false;
                uint8_t UnkT12 = 0;
                // Added to the model ID to pick the row in AssetModelSfxParam.
                int16_t AssetSfxParamRelativeID = 0;
                int16_t UnkT1E                  = 0;
                int32_t UnkT24 = 0, UnkT28 = 0, UnkT30 = 0, UnkT34 = 0;
                std::array<Msb::Ref<PartList>, 6> UnkPartNames;
                bool UnkT50    = false;
                uint8_t UnkT51 = 0, UnkT53 = 0;
                Msb::Ref<PartList> UnkT54PartName;
                int32_t UnkModelMaskAndAnimID = 0;
                int32_t UnkT5C = 0, UnkT60 = 0, UnkT64 = 0;
                AssetUnkStruct1 AssetUnk1;
                AssetUnkStruct2 AssetUnk2;
                AssetUnkStruct3 AssetUnk3;
                AssetUnkStruct4 AssetUnk4;

                Asset() { Name = "AEGxxx_xxx_xxxx"; }
                template<typename V>
                void Fields(V& F) {
                    F(UnkT00);
                    F(UnkT02);
                    F.Pad(12);
                    F(UnkT10);
                    F(UnkT11);
                    F(UnkT12);
                    F.template Const<uint8_t>(0);
                    F.Pad(8);
                    F(AssetSfxParamRelativeID);
                    F(UnkT1E);
                    F.template Const<int32_t>(-1);
                    F(UnkT24);
                    F(UnkT28);
                    F.template Const<int32_t>(0);
                    F(UnkT30);
                    F(UnkT34);
                    F(UnkPartNames);
                    F(UnkT50);
                    F(UnkT51);
                    F.template Const<uint8_t>(0);
                    F(UnkT53);
                    F(UnkT54PartName);
                    F(UnkModelMaskAndAnimID);
                    F(UnkT5C);
                    F(UnkT60);
                    F(UnkT64);
                    // Offsets of the four structs below, which are always in the same place.
                    F.template Const<int64_t>(0x88);
                    F.template Const<int64_t>(0xC8);
                    F.template Const<int64_t>(0x108);
                    F.template Const<int64_t>(0x148);
                    AssetUnk1.Fields(F);
                    AssetUnk2.Fields(F);
                    AssetUnk3.Fields(F);
                    AssetUnk4.Fields(F);
                }
            };

            int32_t Version = 73;
            std::vector<MapPiece> MapPieces;
            std::vector<Enemy> Enemies;
            std::vector<Player> Players;
            std::vector<Collision> Collisions;
            std::vector<DummyAsset> DummyAssets;
            std::vector<DummyEnemy> DummyEnemies;
            std::vector<ConnectCollision> ConnectCollisions;
            std::vector<Asset> Assets;

            static constexpr const char* ParamName = "PARTS_PARAM_ST";

            template<typename F>
            void ForEachList(F&& Visit) {
                Visit(MapPieces);
                Visit(Enemies);
                Visit(Players);
                Visit(Collisions);
                Visit(DummyAssets);
                Visit(DummyEnemies);
                Visit(ConnectCollisions);
                Visit(Assets);
            }
            template<typename F>
            void ForEachList(F&& Visit) const {
                Visit(MapPieces);
                Visit(Enemies);
                Visit(Players);
                Visit(Collisions);
                Visit(DummyAssets);
                Visit(DummyEnemies);
                Visit(ConnectCollisions);
                Visit(Assets);
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
        // Unknown and unused.
        EmptyParam Layers{73};
        // Instances of actual things in the map.
        PartsParam Parts;

    protected:
        bool IsImpl(BinaryReader& Reader) override;
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
