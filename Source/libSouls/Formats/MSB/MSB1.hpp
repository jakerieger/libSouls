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

    // MSB1: the map layout file of Dark Souls (and its remaster). Extension: .msb
    //
    // An MSB has four params: models (the model files parts can use), events (dynamic and interactive systems such
    // as item pickups and enemy spawners), regions (points and trigger volumes) and parts (the actual things in the
    // map). Entries refer to each other by name.
    class SOULS_API MSB1 : public SoulsFile<MSB1> {
    public:
        using ModelList     = Msb::ModelList;
        using EventList     = Msb::EventList;
        using RegionList    = Msb::RegionList;
        using PartList      = Msb::PartList;
        using CollisionList = Msb::CollisionList;

        // A generic entry in an MSB param.
        struct Entry {
            std::string Name;
            virtual ~Entry() = default;
        };

        // All entries of the file, in file order, for resolving references between them.
        struct Entries {
            std::vector<Entry*> Models, Events, Regions, Parts, Collisions;

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
                } else {
                    return Collisions;
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
            struct MapPiece : ModelOf<0> {
                MapPiece() { Name = "mXXXXBX"; }
            };
            struct Object : ModelOf<1> {
                Object() { Name = "oXXXX"; }
            };
            struct Enemy : ModelOf<2> {
                Enemy() { Name = "cXXXX"; }
            };
            struct Player : ModelOf<4> {
                Player() { Name = "c0000"; }
            };
            struct Collision : ModelOf<5> {
                Collision() { Name = "hXXXXBX"; }
            };
            struct Navmesh : ModelOf<6> {
                Navmesh() { Name = "nXXXXBX"; }
            };

            // Models for fixed terrain and scenery.
            std::vector<MapPiece> MapPieces;
            // Models for dynamic props.
            std::vector<Object> Objects;
            // Models for non-player entities.
            std::vector<Enemy> Enemies;
            // Models for player spawn points, I think.
            std::vector<Player> Players;
            // Models for physics collision.
            std::vector<Collision> Collisions;
            // Models for AI navigation.
            std::vector<Navmesh> Navmeshes;

            static constexpr const char* ParamName = "MODEL_PARAM_ST";

            template<typename F>
            void ForEachList(F&& Visit) {
                Visit(MapPieces);
                Visit(Objects);
                Visit(Enemies);
                Visit(Players);
                Visit(Collisions);
                Visit(Navmeshes);
            }
            template<typename F>
            void ForEachList(F&& Visit) const {
                Visit(MapPieces);
                Visit(Objects);
                Visit(Enemies);
                Visit(Players);
                Visit(Collisions);
                Visit(Navmeshes);
            }
        };
#pragma endregion

#pragma region Regions
        // A point or volume used by scripts or events.
        struct SOULS_API Region : Entry {
            // Describes the physical shape of the region. Composite shapes aren't supported in Dark Souls 1.
            Msb::Shape Shape = Msb::Shapes::Point{};
            // Location of the region.
            Vector3 Position;
            // Rotation of the region, in degrees.
            Vector3 Rotation;
            // Identifies the region in external files.
            int32_t EntityID = -1;

            static constexpr uint32_t TypeId = 0;
            uint32_t Type() const { return 0; }

            Region() { Name = "Region"; }
            void Read(BinaryReader& Reader);
            void Write(BinaryWriter& Writer, int32_t ID);
        };

        // A collection of points and trigger volumes used by scripts and events.
        struct PointParam : Msb::Detail::TypedLists<PointParam, Region> {
            // All regions in the map.
            std::vector<Region> Regions;

            static constexpr const char* ParamName = "POINT_PARAM_ST";

            template<typename F>
            void ForEachList(F&& Visit) {
                Visit(Regions);
            }
            template<typename F>
            void ForEachList(F&& Visit) const {
                Visit(Regions);
            }
        };
#pragma endregion

#pragma region Events
        // Common data for all dynamic events.
        struct SOULS_API Event : Entry {
            // Unknown, should be unique.
            int32_t EventID = -1;
            // Part and region referenced by the event.
            Msb::Ref<PartList> PartName;
            Msb::Ref<RegionList> RegionName;
            // Identifies the event in external files.
            int32_t EntityID = -1;

            virtual uint32_t Type() const = 0;
            void Read(BinaryReader& Reader);
            void Write(BinaryWriter& Writer, int32_t ID);
            virtual void ReadTypeData(BinaryReader& Reader)                = 0;
            virtual void WriteTypeData(BinaryWriter& Writer)               = 0;
            virtual void GetNames(const Entries& Lists);
            virtual void GetIndices(const Entries& Lists);
        };

        // Implements the type data functions of an event from its Fields function.
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
            void GetNames(const Entries& Lists) override {
                Event::GetNames(Lists);
                Msb::Detail::NameVisitor<Entries> V{Lists};
                static_cast<D*>(this)->Fields(V);
            }
            void GetIndices(const Entries& Lists) override {
                Event::GetIndices(Lists);
                Msb::Detail::IndexVisitor<Entries> V{Lists};
                static_cast<D*>(this)->Fields(V);
            }
        };

        // Contains abstract entities that control various dynamic elements in the map.
        struct EventParam : Msb::Detail::TypedLists<EventParam, Event> {
            // A fixed point light.
            struct Light : EventImpl<Light, 0> {
                int32_t PointLightID = 0;
                Light() { Name = "Event: Light"; }
                template<typename V>
                void Fields(V& F) {
                    F(PointLightID);
                }
            };

            // An area-based music or sound effect.
            struct Sound : EventImpl<Sound, 1> {
                // Category of sound.
                int32_t SoundType = 0;
                // ID of the sound file in the FSBs.
                int32_t SoundID = 0;
                Sound() { Name = "Event: Sound"; }
                template<typename V>
                void Fields(V& F) {
                    F(SoundType);
                    F(SoundID);
                }
            };

            // A fixed particle effect.
            struct SFX : EventImpl<SFX, 2> {
                // ID of the effect in the ffxbnds.
                int32_t EffectID = 0;
                SFX() { Name = "Event: SFX"; }
                template<typename V>
                void Fields(V& F) {
                    F(EffectID);
                }
            };

            // Wind that affects particle effects.
            struct Wind : EventImpl<Wind, 3> {
                Vector3 WindVecMin;
                float UnkT0C = 0;
                Vector3 WindVecMax;
                float UnkT1C = 0;
                float WindSwingCycle0 = 0, WindSwingCycle1 = 0, WindSwingCycle2 = 0, WindSwingCycle3 = 0;
                float WindSwingPow0 = 0, WindSwingPow1 = 0, WindSwingPow2 = 0, WindSwingPow3 = 0;
                Wind() { Name = "Event: Wind"; }
                template<typename V>
                void Fields(V& F) {
                    F(WindVecMin);
                    F(UnkT0C);
                    F(WindVecMax);
                    F(UnkT1C);
                    F(WindSwingCycle0);
                    F(WindSwingCycle1);
                    F(WindSwingCycle2);
                    F(WindSwingCycle3);
                    F(WindSwingPow0);
                    F(WindSwingPow1);
                    F(WindSwingPow2);
                    F(WindSwingPow3);
                }
            };

            // A pick-uppable item.
            struct Treasure : EventImpl<Treasure, 4> {
                // The part that the treasure is attached to, such as an item corpse.
                Msb::Ref<PartList> TreasurePartName;
                // Item lots to be granted when the treasure is picked up; only the first appears to be functional.
                std::array<int32_t, 5> ItemLots{-1, -1, -1, -1, -1};
                // Changes the text of the pickup prompt.
                bool InChest = false;
                // Whether the treasure should be hidden by default.
                bool StartDisabled = false;
                Treasure() { Name = "Event: Treasure"; }
                template<typename V>
                void Fields(V& F) {
                    F.template Const<int32_t>(0);
                    F(TreasurePartName);
                    for (int32_t& Lot : ItemLots) {
                        F(Lot);
                        F.template Const<int32_t>(-1);
                    }
                    F(InChest);
                    F(StartDisabled);
                    F.template Const<int16_t>(0);
                }
            };

            // A repeating enemy spawner.
            struct Generator : EventImpl<Generator, 5> {
                uint8_t MaxNum   = 0;
                int8_t GenType   = 0;
                int16_t LimitNum = 0, MinGenNum = 0, MaxGenNum = 0;
                float MinInterval = 0, MaxInterval = 0;
                uint8_t InitialSpawnCount = 0;
                // Points that enemies may be spawned at.
                std::array<Msb::Ref<RegionList>, 4> SpawnPointNames;
                // Enemies to be respawned.
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
                    F.Pad(0x1F);
                    F(SpawnPointNames);
                    F(SpawnPartNames);
                    F.Pad(0x40);
                }
            };

            // A fixed orange soapstone message.
            struct Message : EventImpl<Message, 6> {
                // FMG text ID to display.
                int16_t MessageID = 0;
                int16_t UnkT02    = 0;
                // Whether the message requires Seek Guidance to see.
                bool Hidden = false;
                Message() { Name = "Event: Message"; }
                template<typename V>
                void Fields(V& F) {
                    F(MessageID);
                    F(UnkT02);
                    F(Hidden);
                    F.template Const<uint8_t>(0);
                    F.template Const<int16_t>(0);
                }
            };

            // Represents an interaction with an object.
            struct ObjAct : EventImpl<ObjAct, 7> {
                enum class StateType : uint8_t {
                    Default = 0,
                    Door    = 1,
                    Loop    = 2,
                };

                // Unknown how this differs from the Event EntityID.
                int32_t ObjActEntityID = -1;
                // The object that the ObjAct controls.
                Msb::Ref<PartList> ObjActPartName;
                // ID in ObjActParam that configures the ObjAct.
                int16_t ObjActParamID = -1;
                StateType ObjActState = StateType::Default;
                // Unknown, probably enables or disables the ObjAct.
                int32_t EventFlagID = -1;
                ObjAct() { Name = "Event: ObjAct"; }
                template<typename V>
                void Fields(V& F) {
                    F(ObjActEntityID);
                    F(ObjActPartName);
                    F(ObjActParamID);
                    F(ObjActState);
                    F.template Const<uint8_t>(0);
                    F(EventFlagID);
                }
            };

            // Unknown what this accomplishes beyond just having the region.
            struct SpawnPoint : EventImpl<SpawnPoint, 8> {
                // Point for the SpawnPoint to spawn at.
                Msb::Ref<RegionList> SpawnPointName;
                SpawnPoint() { Name = "Event: SpawnPoint"; }
                template<typename V>
                void Fields(V& F) {
                    F(SpawnPointName);
                    F.Pad(12);
                }
            };

            // The origin of the map, already accounted for in MSB positions.
            struct MapOffset : EventImpl<MapOffset, 9> {
                // Position of the map.
                Vector3 Position;
                // Rotation of the map.
                float Degree = 0;
                MapOffset() { Name = "Event: MapOffset"; }
                template<typename V>
                void Fields(V& F) {
                    F(Position);
                    F(Degree);
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

            // Unknown.
            struct Environment : EventImpl<Environment, 11> {
                int32_t UnkT00 = 0;
                float UnkT04 = 0, UnkT08 = 0, UnkT0C = 0, UnkT10 = 0, UnkT14 = 0;
                Environment() { Name = "Event: Environment"; }
                template<typename V>
                void Fields(V& F) {
                    F(UnkT00);
                    F(UnkT04);
                    F(UnkT08);
                    F(UnkT0C);
                    F(UnkT10);
                    F(UnkT14);
                    F.Pad(8);
                }
            };

            // A fake multiplayer session where you enter an NPC's world.
            struct PseudoMultiplayer : EventImpl<PseudoMultiplayer, 12> {
                // The NPC whose world you are entering.
                int32_t HostEntityID = -1;
                // Set when inside the event's region, unset when outside it.
                int32_t EventFlagID = -1;
                // ID of a goods item that is used to trigger the event.
                int32_t ActivateGoodsID = 0;
                PseudoMultiplayer() { Name = "Event: PseudoMultiplayer"; }
                template<typename V>
                void Fields(V& F) {
                    F(HostEntityID);
                    F(EventFlagID);
                    F(ActivateGoodsID);
                    F.template Const<int32_t>(0);
                }
            };

            std::vector<Light> Lights;                        // fixed point light sources
            std::vector<Sound> Sounds;                        // background music and area-based sounds
            std::vector<SFX> SFXs;                            // particle effects
            std::vector<Wind> Winds;                          // wind that affects SFX; should only be one per map, if any
            std::vector<Treasure> Treasures;                  // item pickups in the open or in chests
            std::vector<Generator> Generators;                // repeated enemy spawners
            std::vector<Message> Messages;                    // static soapstone messages
            std::vector<ObjAct> ObjActs;                      // controllers for object interactions
            std::vector<SpawnPoint> SpawnPoints;              // unknown exactly what this is for
            std::vector<MapOffset> MapOffsets;                // the origin of the map
            std::vector<Navmesh> Navmeshes;                   // unknown, interacts with navmeshes somehow
            std::vector<Environment> Environments;            // unknown
            std::vector<PseudoMultiplayer> PseudoMultiplayers;  // controls the player being summoned to an NPC's world

            static constexpr const char* ParamName = "EVENT_PARAM_ST";

            template<typename F>
            void ForEachList(F&& Visit) {
                Visit(Lights);
                Visit(Sounds);
                Visit(SFXs);
                Visit(Winds);
                Visit(Treasures);
                Visit(Generators);
                Visit(Messages);
                Visit(ObjActs);
                Visit(SpawnPoints);
                Visit(MapOffsets);
                Visit(Navmeshes);
                Visit(Environments);
                Visit(PseudoMultiplayers);
            }
            template<typename F>
            void ForEachList(F&& Visit) const {
                Visit(Lights);
                Visit(Sounds);
                Visit(SFXs);
                Visit(Winds);
                Visit(Treasures);
                Visit(Generators);
                Visit(Messages);
                Visit(ObjActs);
                Visit(SpawnPoints);
                Visit(MapOffsets);
                Visit(Navmeshes);
                Visit(Environments);
                Visit(PseudoMultiplayers);
            }
        };
#pragma endregion

#pragma region Parts
        // Common information for all concrete entities.
        struct SOULS_API Part : Entry {
            // The model of the part, corresponding to an entry in the ModelParam.
            Msb::Ref<ModelList> ModelName;
            // A path to a .sib file, presumed to be some kind of editor placeholder.
            std::string SibPath;
            // Location of the part.
            Vector3 Position;
            // Rotation of the part, in degrees.
            Vector3 Rotation;
            // Scale of the part, only meaningful for map pieces and objects.
            Vector3 Scale{1.f, 1.f, 1.f};
            // Control when the part is visible.
            std::array<uint32_t, 4> DrawGroups{0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
            std::array<uint32_t, 4> DispGroups{0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
            // Identifies the part in external files.
            int32_t EntityID = -1;
            // Unknown.
            uint8_t LightID = 0, FogID = 0, ScatterID = 0, LensFlareID = 0, ShadowID = 0, DofID = 0, ToneMapID = 0,
                    ToneCorrectID = 0, LanternID = 0, LodParamID = 0, IsShadowSrc = 0, IsShadowDest = 0, IsShadowOnly = 0,
                    DrawByReflectCam = 0, DrawOnlyReflectCam = 0, UseDepthBiasFloat = 0, DisablePointLightEffect = 0;

            virtual uint32_t Type() const = 0;
            void Read(BinaryReader& Reader);
            void Write(BinaryWriter& Writer, int32_t ID);
            virtual void ReadTypeData(BinaryReader& Reader)  = 0;
            virtual void WriteTypeData(BinaryWriter& Writer) = 0;
            virtual void GetNames(const Entries& Lists);
            virtual void GetIndices(const Entries& Lists);
        };

        // Implements the type data functions of a part from its Fields function.
        template<typename D, uint32_t Id, typename B = Part>
        struct PartImpl : B {
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
            // Collision that controls loading of the object.
            Msb::Ref<PartList> CollisionName;
            int8_t BreakTerm   = 0;
            int8_t NetSyncType = 0;
            int16_t InitAnimID = 0;
            int16_t UnkT0E     = 0;
            int32_t UnkT10     = 0;

            ObjectBase() { Name = "oXXXX_XXXX"; }
            template<typename V>
            void ObjectFields(V& F) {
                F.template Const<int32_t>(0);
                F(CollisionName);
                F(BreakTerm);
                F(NetSyncType);
                F.template Const<int16_t>(0);
                F(InitAnimID);
                F(UnkT0E);
                F(UnkT10);
                F.template Const<int32_t>(0);
            }
        };

        // Common base data for enemies and dummy enemies.
        struct EnemyBase : Part {
            // ID in NPCThinkParam determining AI properties.
            int32_t ThinkParamID = -1;
            // ID in NPCParam determining character properties.
            int32_t NPCParamID = -1;
            // ID of a talk ESD used by the character.
            int32_t TalkID = -1;
            uint8_t PointMoveType = 0;
            uint16_t PlatoonID    = 0;
            // ID in CharaInitParam determining equipment and stats for humans.
            int32_t CharaInitID = -1;
            // Collision that controls loading of the enemy.
            Msb::Ref<PartList> CollisionName;
            // Regions for the enemy to patrol.
            std::array<Msb::Ref<RegionList, int16_t>, 8> MovePointNames;
            int32_t InitAnimID   = 0;
            int32_t DamageAnimID = 0;

            EnemyBase() { Name = "cXXXX_XXXX"; }
            template<typename V>
            void EnemyFields(V& F) {
                F.template Const<int32_t>(0);
                F.template Const<int32_t>(0);
                F(ThinkParamID);
                F(NPCParamID);
                F(TalkID);
                F(PointMoveType);
                F.template Const<uint8_t>(0);
                F(PlatoonID);
                F(CharaInitID);
                F(CollisionName);
                F.template Const<int32_t>(0);
                F.template Const<int32_t>(0);
                F(MovePointNames);
                F(InitAnimID);
                F(DamageAnimID);
            }
        };

        // All instances of concrete things in the map.
        struct PartsParam : Msb::Detail::TypedLists<PartsParam, Part> {
            // A visible but not physical model making up the map.
            struct MapPiece : PartImpl<MapPiece, 0> {
                MapPiece() { Name = "mXXXXBX"; }
                template<typename V>
                void Fields(V& F) {
                    F.Pad(8);
                }
            };

            // A dynamic or interactible part of the map.
            struct Object : PartImpl<Object, 1, ObjectBase> {
                template<typename V>
                void Fields(V& F) {
                    ObjectFields(F);
                }
            };

            // Any living entity besides the player character.
            struct Enemy : PartImpl<Enemy, 2, EnemyBase> {
                template<typename V>
                void Fields(V& F) {
                    EnemyFields(F);
                }
            };

            // Unknown exactly what these do.
            struct Player : PartImpl<Player, 4> {
                Player() { Name = "c0000_XXXX"; }
                template<typename V>
                void Fields(V& F) {
                    F.Pad(16);
                }
            };

            // Invisible but physical geometry.
            struct Collision : PartImpl<Collision, 5> {
                uint8_t HitFilterID = 0;
                // Causes sounds to be modulated when standing on the collision.
                uint8_t SoundSpaceType      = 0;
                int16_t EnvLightMapSpotIndex = 0;
                float ReflectPlaneHeight     = 0;
                std::array<uint32_t, 4> NvmGroups{0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
                std::array<int32_t, 3> VagrantEntityIDs{-1, -1, -1};
                // Controls displays of the map name on screen or the loading menu.
                int16_t MapNameID  = -1;
                bool DisableStart  = false;
                uint8_t UnkT27     = 0;
                // If set, disables a bonfire when any enemy is on the collision.
                int32_t DisableBonfireEntityID = -1;
                // An ID used for multiplayer eligibility.
                int32_t PlayRegionID = 0;
                // IDs in LockCamParam determining camera properties.
                int16_t LockCamParamID1 = -1;
                int16_t LockCamParamID2 = -1;

                Collision() { Name = "hXXXXBX"; }
                template<typename V>
                void Fields(V& F) {
                    F(HitFilterID);
                    F(SoundSpaceType);
                    F(EnvLightMapSpotIndex);
                    F(ReflectPlaneHeight);
                    F(NvmGroups);
                    F(VagrantEntityIDs);
                    F(MapNameID);
                    F(DisableStart);
                    F(UnkT27);
                    F(DisableBonfireEntityID);
                    F.template Const<int32_t>(-1);
                    F.template Const<int32_t>(-1);
                    F.template Const<int32_t>(-1);
                    F(PlayRegionID);
                    F(LockCamParamID1);
                    F(LockCamParamID2);
                    F.Pad(16);
                }
            };

            // An AI navigation mesh.
            struct Navmesh : PartImpl<Navmesh, 8> {
                std::array<uint32_t, 4> NvmGroups{0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
                Navmesh() { Name = "nXXXXBX"; }
                template<typename V>
                void Fields(V& F) {
                    F(NvmGroups);
                    F.Pad(16);
                }
            };

            // A normally invisible object, either unused or for a cutscene.
            struct DummyObject : PartImpl<DummyObject, 9, ObjectBase> {
                template<typename V>
                void Fields(V& F) {
                    ObjectFields(F);
                }
            };

            // A normally invisible enemy, either unused or for a cutscene.
            struct DummyEnemy : PartImpl<DummyEnemy, 10, EnemyBase> {
                template<typename V>
                void Fields(V& F) {
                    EnemyFields(F);
                }
            };

            // Attaches to an actual collision and causes another map to be loaded when standing on it.
            struct ConnectCollision : PartImpl<ConnectCollision, 11> {
                // The collision which will load another map.
                Msb::Ref<CollisionList> CollisionName;
                // Four bytes specifying the map ID to load.
                std::array<uint8_t, 4> MapID{10, 2, 0, 0};

                ConnectCollision() { Name = "hXXXXBX_XXXX"; }
                template<typename V>
                void Fields(V& F) {
                    F(CollisionName);
                    F(MapID);
                    F.Pad(8);
                }
            };

            std::vector<MapPiece> MapPieces;              // all of the fixed visual geometry of the map
            std::vector<Object> Objects;                  // dynamic props and interactive things
            std::vector<Enemy> Enemies;                   // all non-player characters
            std::vector<Player> Players;                  // something to do with player spawn points
            std::vector<Collision> Collisions;            // invisible physical geometry of the map
            std::vector<Navmesh> Navmeshes;               // AI navigation meshes
            std::vector<DummyObject> DummyObjects;        // objects that don't appear normally; unused or for cutscenes
            std::vector<DummyEnemy> DummyEnemies;         // enemies that don't appear normally; unused or for cutscenes
            std::vector<ConnectCollision> ConnectCollisions;  // dummy parts that make a collision load another map

            static constexpr const char* ParamName = "PARTS_PARAM_ST";

            template<typename F>
            void ForEachList(F&& Visit) {
                Visit(MapPieces);
                Visit(Objects);
                Visit(Enemies);
                Visit(Players);
                Visit(Collisions);
                Visit(Navmeshes);
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
                Visit(Navmeshes);
                Visit(DummyObjects);
                Visit(DummyEnemies);
                Visit(ConnectCollisions);
            }
        };
#pragma endregion

        // True for PS3/X360, false for PC.
        bool BigEndian = false;
        // Model files that are available for parts to use.
        ModelParam Models;
        // Dynamic or interactive systems such as item pickups, levers, enemy spawners, etc.
        EventParam Events;
        // Points or areas of space that trigger some sort of behavior.
        PointParam Regions;
        // Instances of actual things in the map.
        PartsParam Parts;

    protected:
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
