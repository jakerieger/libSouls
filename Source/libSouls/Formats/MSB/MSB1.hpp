// Copyright (C) 2026 Jake Rieger
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, version 3. This program is distributed in the hope that it will
// be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU General
// Public License along with this program. If not, see https://www.gnu.org/licenses/.

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

    /// @brief MSB1: the map layout file of Dark Souls (and its remaster).
    ///
    /// Extension: .msb
    ///
    /// An MSB has four params: models (the model files parts can use), events (dynamic and interactive systems such
    /// as item pickups and enemy spawners), regions (points and trigger volumes) and parts (the actual things in the
    /// map). Entries refer to each other by name.
    class SOULS_API MSB1 : public SoulsFile<MSB1> {
    public:
        /// @brief Tag for references to models.
        using ModelList = Msb::ModelList;
        /// @brief Tag for references to events.
        using EventList = Msb::EventList;
        /// @brief Tag for references to regions.
        using RegionList = Msb::RegionList;
        /// @brief Tag for references to parts.
        using PartList = Msb::PartList;
        /// @brief Tag for references to collision parts only.
        using CollisionList = Msb::CollisionList;

        /// @brief A generic entry in an MSB param.
        struct Entry {
            /// @brief The name of this entry.
            std::string Name;
            virtual ~Entry() = default;
        };

        /// @brief All entries of the file, in file order, for resolving references between them.
        struct Entries {
            /// @brief Every model, in file order.
            std::vector<Entry*> Models;
            /// @brief Every event, in file order.
            std::vector<Entry*> Events;
            /// @brief Every region, in file order.
            std::vector<Entry*> Regions;
            /// @brief Every part, in file order.
            std::vector<Entry*> Parts;
            /// @brief Just the collision parts, in file order.
            std::vector<Entry*> Collisions;

            /// @brief The list of entries a reference tag refers to.
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
        /// @brief A model file available for parts to reference.
        struct SOULS_API Model : Entry {
            /// @brief A path to a .sib file, presumed to be some kind of editor placeholder.
            std::string SibPath;
            /// @brief How many parts use the model; recalculated when writing.
            int32_t InstanceCount = 0;

            /// @brief The type number of this entry as stored in the file.
            virtual uint32_t Type() const = 0;
            /// @brief Reads the entry from its position in the file.
            void Read(BinaryReader& Reader);
            /// @brief Writes the entry; ID is its position among the entries of the same type.
            void Write(BinaryWriter& Writer, int32_t ID);
        };

        /// @brief Implements a model type's type number (and type data, if it has any).
        template<uint32_t Id>
        struct ModelOf : Model {
            /// @brief The type number entries of this type have in the file.
            static constexpr uint32_t TypeId = Id;
            /// @brief The type number of this entry as stored in the file.
            uint32_t Type() const override { return Id; }
        };

        /// @brief Model files that are available for parts to use.
        struct ModelParam : Msb::Detail::TypedLists<ModelParam, Model> {
            /// @brief A model for a static piece of visual map geometry.
            struct MapPiece : ModelOf<0> {
                MapPiece() { Name = "mXXXXBX"; }
            };
            /// @brief A model for a dynamic or interactible part.
            struct Object : ModelOf<1> {
                Object() { Name = "oXXXX"; }
            };
            /// @brief A model for any non-player character.
            struct Enemy : ModelOf<2> {
                Enemy() { Name = "cXXXX"; }
            };
            /// @brief A model for a player spawn point.
            struct Player : ModelOf<4> {
                Player() { Name = "c0000"; }
            };
            /// @brief A model for a static piece of physical map geometry.
            struct Collision : ModelOf<5> {
                Collision() { Name = "hXXXXBX"; }
            };
            /// @brief Unknown.
            struct Navmesh : ModelOf<6> {
                Navmesh() { Name = "nXXXXBX"; }
            };

            /// @brief Models for fixed terrain and scenery.
            std::vector<MapPiece> MapPieces;
            /// @brief Models for dynamic props.
            std::vector<Object> Objects;
            /// @brief Models for non-player entities.
            std::vector<Enemy> Enemies;
            /// @brief Models for player spawn points, I think.
            std::vector<Player> Players;
            /// @brief Models for physics collision.
            std::vector<Collision> Collisions;
            /// @brief Models for AI navigation.
            std::vector<Navmesh> Navmeshes;

            /// @brief The param's name in the file.
            static constexpr const char* ParamName = "MODEL_PARAM_ST";

            /// @brief Calls Visit with each per-type list, in the order the entries are written.
            template<typename F>
            void ForEachList(F&& Visit) {
                Visit(MapPieces);
                Visit(Objects);
                Visit(Enemies);
                Visit(Players);
                Visit(Collisions);
                Visit(Navmeshes);
            }
            /// @brief Calls Visit with each per-type list, in the order the entries are written.
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
        /// @brief A point or volume used by scripts or events.
        struct SOULS_API Region : Entry {
            /// @brief Describes the physical shape of the region.
            ///
            /// Composite shapes aren't supported in Dark Souls 1.
            Msb::Shape Shape = Msb::Shapes::Point {};
            /// @brief Location of the region.
            Vector3 Position;
            /// @brief Rotation of the region, in degrees.
            Vector3 Rotation;
            /// @brief Identifies the region in external files.
            int32_t EntityID = -1;

            /// @brief The type number entries of this type have in the file.
            static constexpr uint32_t TypeId = 0;
            /// @brief The type number of this entry as stored in the file.
            uint32_t Type() const { return 0; }

            Region() { Name = "Region"; }
            /// @brief Reads the entry from its position in the file.
            void Read(BinaryReader& Reader);
            /// @brief Writes the entry; ID is its position among the entries of the same type.
            void Write(BinaryWriter& Writer, int32_t ID);
        };

        /// @brief A collection of points and trigger volumes used by scripts and events.
        struct PointParam : Msb::Detail::TypedLists<PointParam, Region> {
            /// @brief All regions in the map.
            std::vector<Region> Regions;

            /// @brief The param's name in the file.
            static constexpr const char* ParamName = "POINT_PARAM_ST";

            /// @brief Calls Visit with each per-type list, in the order the entries are written.
            template<typename F>
            void ForEachList(F&& Visit) {
                Visit(Regions);
            }
            /// @brief Calls Visit with each per-type list, in the order the entries are written.
            template<typename F>
            void ForEachList(F&& Visit) const {
                Visit(Regions);
            }
        };
#pragma endregion

#pragma region Events
        /// @brief Common data for all dynamic events.
        struct SOULS_API Event : Entry {
            /// @brief Unknown, should be unique.
            int32_t EventID = -1;
            /// @brief Part and region referenced by the event.
            Msb::Ref<PartList> PartName;
            /// @brief Region referenced by the event.
            Msb::Ref<RegionList> RegionName;
            /// @brief Identifies the event in external files.
            int32_t EntityID = -1;

            /// @brief The type number of this entry as stored in the file.
            virtual uint32_t Type() const = 0;
            /// @brief Reads the entry from its position in the file.
            void Read(BinaryReader& Reader);
            /// @brief Writes the entry; ID is its position among the entries of the same type.
            void Write(BinaryWriter& Writer, int32_t ID);
            /// @brief Reads the type-specific data.
            virtual void ReadTypeData(BinaryReader& Reader) = 0;
            /// @brief Writes the type-specific data.
            virtual void WriteTypeData(BinaryWriter& Writer) = 0;
            /// @brief Turns the indices read from the file into the names of the entries they refer to.
            virtual void GetNames(const Entries& Lists);
            /// @brief Turns the names of referenced entries into the indices written to the file.
            virtual void GetIndices(const Entries& Lists);
        };

        /// @brief Implements the type data functions of an event from its Fields function.
        template<typename D, uint32_t Id>
        struct EventImpl : Event {
            /// @brief The type number entries of this type have in the file.
            static constexpr uint32_t TypeId = Id;
            /// @brief The type number of this entry as stored in the file.
            uint32_t Type() const override { return Id; }
            /// @brief Reads the type-specific data.
            void ReadTypeData(BinaryReader& Reader) override {
                Msb::Detail::ReadVisitor V {Reader};
                static_cast<D*>(this)->Fields(V);
            }
            /// @brief Writes the type-specific data.
            void WriteTypeData(BinaryWriter& Writer) override {
                Msb::Detail::WriteVisitor V {Writer};
                static_cast<D*>(this)->Fields(V);
            }
            /// @brief Turns the indices read from the file into the names of the entries they refer to.
            void GetNames(const Entries& Lists) override {
                Event::GetNames(Lists);
                Msb::Detail::NameVisitor<Entries> V {Lists};
                static_cast<D*>(this)->Fields(V);
            }
            /// @brief Turns the names of referenced entries into the indices written to the file.
            void GetIndices(const Entries& Lists) override {
                Event::GetIndices(Lists);
                Msb::Detail::IndexVisitor<Entries> V {Lists};
                static_cast<D*>(this)->Fields(V);
            }
        };

        /// @brief Contains abstract entities that control various dynamic elements in the map.
        struct EventParam : Msb::Detail::TypedLists<EventParam, Event> {
            /// @brief A fixed point light.
            struct Light : EventImpl<Light, 0> {
                /// @brief Unknown.
                int32_t PointLightID = 0;
                Light() { Name = "Event: Light"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
                template<typename V>
                void Fields(V& F) {
                    F(PointLightID);
                }
            };

            /// @brief An area-based music or sound effect.
            struct Sound : EventImpl<Sound, 1> {
                /// @brief Category of sound.
                int32_t SoundType = 0;
                /// @brief ID of the sound file in the FSBs.
                int32_t SoundID = 0;
                Sound() { Name = "Event: Sound"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
                template<typename V>
                void Fields(V& F) {
                    F(SoundType);
                    F(SoundID);
                }
            };

            /// @brief A fixed particle effect.
            struct SFX : EventImpl<SFX, 2> {
                /// @brief ID of the effect in the ffxbnds.
                int32_t EffectID = 0;
                SFX() { Name = "Event: SFX"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
                template<typename V>
                void Fields(V& F) {
                    F(EffectID);
                }
            };

            /// @brief Wind that affects particle effects.
            struct Wind : EventImpl<Wind, 3> {
                /// @brief Unknown.
                Vector3 WindVecMin;
                /// @brief Unknown.
                float UnkT0C = 0;
                /// @brief Unknown.
                Vector3 WindVecMax;
                /// @brief Unknown.
                float UnkT1C = 0;
                /// @brief Unknown.
                float WindSwingCycle0 = 0;
                /// @brief Unknown.
                float WindSwingCycle1 = 0;
                /// @brief Unknown.
                float WindSwingCycle2 = 0;
                /// @brief Unknown.
                float WindSwingCycle3 = 0;
                /// @brief Unknown.
                float WindSwingPow0 = 0;
                /// @brief Unknown.
                float WindSwingPow1 = 0;
                /// @brief Unknown.
                float WindSwingPow2 = 0;
                /// @brief Unknown.
                float WindSwingPow3 = 0;
                Wind() { Name = "Event: Wind"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
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

            /// @brief A pick-uppable item.
            struct Treasure : EventImpl<Treasure, 4> {
                /// @brief The part that the treasure is attached to, such as an item corpse.
                Msb::Ref<PartList> TreasurePartName;
                /// @brief Item lots to be granted when the treasure is picked up; only the first appears to be
                ///        functional.
                std::array<int32_t, 5> ItemLots {-1, -1, -1, -1, -1};
                /// @brief Changes the text of the pickup prompt.
                bool InChest = false;
                /// @brief Whether the treasure should be hidden by default.
                bool StartDisabled = false;
                Treasure() { Name = "Event: Treasure"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
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

            /// @brief A repeating enemy spawner.
            struct Generator : EventImpl<Generator, 5> {
                /// @brief Unknown.
                uint8_t MaxNum = 0;
                /// @brief Unknown.
                int8_t GenType = 0;
                /// @brief Unknown.
                int16_t LimitNum = 0;
                /// @brief Unknown.
                int16_t MinGenNum = 0;
                /// @brief Unknown.
                int16_t MaxGenNum = 0;
                /// @brief Unknown.
                float MinInterval = 0;
                /// @brief Unknown.
                float MaxInterval = 0;
                /// @brief Unknown.
                uint8_t InitialSpawnCount = 0;
                /// @brief Points that enemies may be spawned at.
                std::array<Msb::Ref<RegionList>, 4> SpawnPointNames;
                /// @brief Enemies to be respawned.
                std::array<Msb::Ref<PartList>, 32> SpawnPartNames;
                Generator() { Name = "Event: Generator"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
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

            /// @brief A fixed orange soapstone message.
            struct Message : EventImpl<Message, 6> {
                /// @brief FMG text ID to display.
                int16_t MessageID = 0;
                /// @brief Unknown.
                int16_t UnkT02 = 0;
                /// @brief Whether the message requires Seek Guidance to see.
                bool Hidden = false;
                Message() { Name = "Event: Message"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
                template<typename V>
                void Fields(V& F) {
                    F(MessageID);
                    F(UnkT02);
                    F(Hidden);
                    F.template Const<uint8_t>(0);
                    F.template Const<int16_t>(0);
                }
            };

            /// @brief Represents an interaction with an object.
            struct ObjAct : EventImpl<ObjAct, 7> {
                /// @brief How the object behaves when used.
                enum class StateType : uint8_t {
                    Default = 0,
                    Door    = 1,
                    Loop    = 2,
                };

                /// @brief Unknown how this differs from the Event EntityID.
                int32_t ObjActEntityID = -1;
                /// @brief The object that the ObjAct controls.
                Msb::Ref<PartList> ObjActPartName;
                /// @brief ID in ObjActParam that configures the ObjAct.
                int16_t ObjActParamID = -1;
                /// @brief Unknown.
                StateType ObjActState = StateType::Default;
                /// @brief Unknown, probably enables or disables the ObjAct.
                int32_t EventFlagID = -1;
                ObjAct() { Name = "Event: ObjAct"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
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

            /// @brief Unknown what this accomplishes beyond just having the region.
            struct SpawnPoint : EventImpl<SpawnPoint, 8> {
                /// @brief Point for the SpawnPoint to spawn at.
                Msb::Ref<RegionList> SpawnPointName;
                SpawnPoint() { Name = "Event: SpawnPoint"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
                template<typename V>
                void Fields(V& F) {
                    F(SpawnPointName);
                    F.Pad(12);
                }
            };

            /// @brief The origin of the map, already accounted for in MSB positions.
            struct MapOffset : EventImpl<MapOffset, 9> {
                /// @brief Position of the map.
                Vector3 Position;
                /// @brief Rotation of the map.
                float Degree = 0;
                MapOffset() { Name = "Event: MapOffset"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
                template<typename V>
                void Fields(V& F) {
                    F(Position);
                    F(Degree);
                }
            };

            /// @brief Unknown.
            struct Navmesh : EventImpl<Navmesh, 10> {
                /// @brief Unknown.
                Msb::Ref<RegionList> NavmeshRegionName;
                Navmesh() { Name = "Event: Navmesh"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
                template<typename V>
                void Fields(V& F) {
                    F(NavmeshRegionName);
                    F.Pad(12);
                }
            };

            /// @brief Unknown.
            struct Environment : EventImpl<Environment, 11> {
                /// @brief Unknown.
                int32_t UnkT00 = 0;
                /// @brief Unknown.
                float UnkT04 = 0;
                /// @brief Unknown.
                float UnkT08 = 0;
                /// @brief Unknown.
                float UnkT0C = 0;
                /// @brief Unknown.
                float UnkT10 = 0;
                /// @brief Unknown.
                float UnkT14 = 0;
                Environment() { Name = "Event: Environment"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
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

            /// @brief A fake multiplayer session where you enter an NPC's world.
            struct PseudoMultiplayer : EventImpl<PseudoMultiplayer, 12> {
                /// @brief The NPC whose world you are entering.
                int32_t HostEntityID = -1;
                /// @brief Set when inside the event's region, unset when outside it.
                int32_t EventFlagID = -1;
                /// @brief ID of a goods item that is used to trigger the event.
                int32_t ActivateGoodsID = 0;
                PseudoMultiplayer() { Name = "Event: PseudoMultiplayer"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
                template<typename V>
                void Fields(V& F) {
                    F(HostEntityID);
                    F(EventFlagID);
                    F(ActivateGoodsID);
                    F.template Const<int32_t>(0);
                }
            };

            std::vector<Light> Lights;              ///< fixed point light sources
            std::vector<Sound> Sounds;              ///< background music and area-based sounds
            std::vector<SFX> SFXs;                  ///< particle effects
            std::vector<Wind> Winds;                ///< wind that affects SFX; should only be one per map, if any
            std::vector<Treasure> Treasures;        ///< item pickups in the open or in chests
            std::vector<Generator> Generators;      ///< repeated enemy spawners
            std::vector<Message> Messages;          ///< static soapstone messages
            std::vector<ObjAct> ObjActs;            ///< controllers for object interactions
            std::vector<SpawnPoint> SpawnPoints;    ///< unknown exactly what this is for
            std::vector<MapOffset> MapOffsets;      ///< the origin of the map
            std::vector<Navmesh> Navmeshes;         ///< unknown, interacts with navmeshes somehow
            std::vector<Environment> Environments;  ///< unknown
            std::vector<PseudoMultiplayer>
              PseudoMultiplayers;  ///< controls the player being summoned to an NPC's world

            /// @brief The param's name in the file.
            static constexpr const char* ParamName = "EVENT_PARAM_ST";

            /// @brief Calls Visit with each per-type list, in the order the entries are written.
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
            /// @brief Calls Visit with each per-type list, in the order the entries are written.
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
        /// @brief Common information for all concrete entities.
        struct SOULS_API Part : Entry {
            /// @brief The model of the part, corresponding to an entry in the ModelParam.
            Msb::Ref<ModelList> ModelName;
            /// @brief A path to a .sib file, presumed to be some kind of editor placeholder.
            std::string SibPath;
            /// @brief Location of the part.
            Vector3 Position;
            /// @brief Rotation of the part, in degrees.
            Vector3 Rotation;
            /// @brief Scale of the part, only meaningful for map pieces and objects.
            Vector3 Scale {1.f, 1.f, 1.f};
            /// @brief Control when the part is visible.
            std::array<uint32_t, 4> DrawGroups {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
            /// @brief Controls when the part is visible.
            std::array<uint32_t, 4> DispGroups {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
            /// @brief Identifies the part in external files.
            int32_t EntityID = -1;
            /// @brief Unknown.
            uint8_t LightID = 0;
            /// @brief Unknown.
            uint8_t FogID = 0;
            /// @brief Unknown.
            uint8_t ScatterID = 0;
            /// @brief Unknown.
            uint8_t LensFlareID = 0;
            /// @brief Unknown.
            uint8_t ShadowID = 0;
            /// @brief Unknown.
            uint8_t DofID = 0;
            /// @brief Unknown.
            uint8_t ToneMapID = 0;
            /// @brief Unknown.
            uint8_t ToneCorrectID = 0;
            /// @brief Unknown.
            uint8_t LanternID = 0;
            /// @brief Unknown.
            uint8_t LodParamID = 0;
            /// @brief Unknown.
            uint8_t IsShadowSrc = 0;
            /// @brief Unknown.
            uint8_t IsShadowDest = 0;
            /// @brief Unknown.
            uint8_t IsShadowOnly = 0;
            /// @brief Unknown.
            uint8_t DrawByReflectCam = 0;
            /// @brief Unknown.
            uint8_t DrawOnlyReflectCam = 0;
            /// @brief Unknown.
            uint8_t UseDepthBiasFloat = 0;
            /// @brief Unknown.
            uint8_t DisablePointLightEffect = 0;

            /// @brief The type number of this entry as stored in the file.
            virtual uint32_t Type() const = 0;
            /// @brief Reads the entry from its position in the file.
            void Read(BinaryReader& Reader);
            /// @brief Writes the entry; ID is its position among the entries of the same type.
            void Write(BinaryWriter& Writer, int32_t ID);
            /// @brief Reads the type-specific data.
            virtual void ReadTypeData(BinaryReader& Reader) = 0;
            /// @brief Writes the type-specific data.
            virtual void WriteTypeData(BinaryWriter& Writer) = 0;
            /// @brief Turns the indices read from the file into the names of the entries they refer to.
            virtual void GetNames(const Entries& Lists);
            /// @brief Turns the names of referenced entries into the indices written to the file.
            virtual void GetIndices(const Entries& Lists);
        };

        /// @brief Implements the type data functions of a part from its Fields function.
        template<typename D, uint32_t Id, typename B = Part>
        struct PartImpl : B {
            /// @brief The type number entries of this type have in the file.
            static constexpr uint32_t TypeId = Id;
            /// @brief The type number of this entry as stored in the file.
            uint32_t Type() const override { return Id; }
            /// @brief Reads the type-specific data.
            void ReadTypeData(BinaryReader& Reader) override {
                Msb::Detail::ReadVisitor V {Reader};
                static_cast<D*>(this)->Fields(V);
            }
            /// @brief Writes the type-specific data.
            void WriteTypeData(BinaryWriter& Writer) override {
                Msb::Detail::WriteVisitor V {Writer};
                static_cast<D*>(this)->Fields(V);
            }
            /// @brief Turns the indices read from the file into the names of the entries they refer to.
            void GetNames(const Entries& Lists) override {
                B::GetNames(Lists);
                Msb::Detail::NameVisitor<Entries> V {Lists};
                static_cast<D*>(this)->Fields(V);
            }
            /// @brief Turns the names of referenced entries into the indices written to the file.
            void GetIndices(const Entries& Lists) override {
                B::GetIndices(Lists);
                Msb::Detail::IndexVisitor<Entries> V {Lists};
                static_cast<D*>(this)->Fields(V);
            }
        };

        /// @brief Common base data for objects and dummy objects.
        struct ObjectBase : Part {
            /// @brief Collision that controls loading of the object.
            Msb::Ref<PartList> CollisionName;
            /// @brief Unknown.
            int8_t BreakTerm = 0;
            /// @brief Unknown.
            int8_t NetSyncType = 0;
            /// @brief Unknown.
            int16_t InitAnimID = 0;
            /// @brief Unknown.
            int16_t UnkT0E = 0;
            /// @brief Unknown.
            int32_t UnkT10 = 0;

            ObjectBase() { Name = "oXXXX_XXXX"; }
            /// @brief Visits the fields that objects and dummy objects share.
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

        /// @brief Common base data for enemies and dummy enemies.
        struct EnemyBase : Part {
            /// @brief ID in NPCThinkParam determining AI properties.
            int32_t ThinkParamID = -1;
            /// @brief ID in NPCParam determining character properties.
            int32_t NPCParamID = -1;
            /// @brief ID of a talk ESD used by the character.
            int32_t TalkID = -1;
            /// @brief Unknown.
            uint8_t PointMoveType = 0;
            /// @brief Unknown.
            uint16_t PlatoonID = 0;
            /// @brief ID in CharaInitParam determining equipment and stats for humans.
            int32_t CharaInitID = -1;
            /// @brief Collision that controls loading of the enemy.
            Msb::Ref<PartList> CollisionName;
            /// @brief Regions for the enemy to patrol.
            std::array<Msb::Ref<RegionList, int16_t>, 8> MovePointNames;
            /// @brief Unknown.
            int32_t InitAnimID = 0;
            /// @brief Unknown.
            int32_t DamageAnimID = 0;

            EnemyBase() { Name = "cXXXX_XXXX"; }
            /// @brief Visits the fields that enemies and dummy enemies share.
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

        /// @brief All instances of concrete things in the map.
        struct PartsParam : Msb::Detail::TypedLists<PartsParam, Part> {
            /// @brief A visible but not physical model making up the map.
            struct MapPiece : PartImpl<MapPiece, 0> {
                MapPiece() { Name = "mXXXXBX"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
                template<typename V>
                void Fields(V& F) {
                    F.Pad(8);
                }
            };

            /// @brief A dynamic or interactible part of the map.
            struct Object : PartImpl<Object, 1, ObjectBase> {
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
                template<typename V>
                void Fields(V& F) {
                    ObjectFields(F);
                }
            };

            /// @brief Any living entity besides the player character.
            struct Enemy : PartImpl<Enemy, 2, EnemyBase> {
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
                template<typename V>
                void Fields(V& F) {
                    EnemyFields(F);
                }
            };

            /// @brief Unknown exactly what these do.
            struct Player : PartImpl<Player, 4> {
                Player() { Name = "c0000_XXXX"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
                template<typename V>
                void Fields(V& F) {
                    F.Pad(16);
                }
            };

            /// @brief Invisible but physical geometry.
            struct Collision : PartImpl<Collision, 5> {
                /// @brief Unknown.
                uint8_t HitFilterID = 0;
                /// @brief Causes sounds to be modulated when standing on the collision.
                uint8_t SoundSpaceType = 0;
                /// @brief Unknown.
                int16_t EnvLightMapSpotIndex = 0;
                /// @brief Unknown.
                float ReflectPlaneHeight = 0;
                /// @brief Unknown.
                std::array<uint32_t, 4> NvmGroups {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
                /// @brief Unknown.
                std::array<int32_t, 3> VagrantEntityIDs {-1, -1, -1};
                /// @brief Controls displays of the map name on screen or the loading menu.
                int16_t MapNameID = -1;
                /// @brief Unknown.
                bool DisableStart = false;
                /// @brief Unknown.
                uint8_t UnkT27 = 0;
                /// @brief If set, disables a bonfire when any enemy is on the collision.
                int32_t DisableBonfireEntityID = -1;
                /// @brief An ID used for multiplayer eligibility.
                int32_t PlayRegionID = 0;
                /// @brief IDs in LockCamParam determining camera properties.
                int16_t LockCamParamID1 = -1;
                /// @brief ID in LockCamParam determining camera properties.
                int16_t LockCamParamID2 = -1;

                Collision() { Name = "hXXXXBX"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
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

            /// @brief An AI navigation mesh.
            struct Navmesh : PartImpl<Navmesh, 8> {
                /// @brief Unknown.
                std::array<uint32_t, 4> NvmGroups {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
                Navmesh() { Name = "nXXXXBX"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
                template<typename V>
                void Fields(V& F) {
                    F(NvmGroups);
                    F.Pad(16);
                }
            };

            /// @brief A normally invisible object, either unused or for a cutscene.
            struct DummyObject : PartImpl<DummyObject, 9, ObjectBase> {
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
                template<typename V>
                void Fields(V& F) {
                    ObjectFields(F);
                }
            };

            /// @brief A normally invisible enemy, either unused or for a cutscene.
            struct DummyEnemy : PartImpl<DummyEnemy, 10, EnemyBase> {
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
                template<typename V>
                void Fields(V& F) {
                    EnemyFields(F);
                }
            };

            /// @brief Attaches to an actual collision and causes another map to be loaded when standing on it.
            struct ConnectCollision : PartImpl<ConnectCollision, 11> {
                /// @brief The collision which will load another map.
                Msb::Ref<CollisionList> CollisionName;
                /// @brief Four bytes specifying the map ID to load.
                std::array<uint8_t, 4> MapID {10, 2, 0, 0};

                ConnectCollision() { Name = "hXXXXBX_XXXX"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
                template<typename V>
                void Fields(V& F) {
                    F(CollisionName);
                    F(MapID);
                    F.Pad(8);
                }
            };

            std::vector<MapPiece> MapPieces;        ///< all of the fixed visual geometry of the map
            std::vector<Object> Objects;            ///< dynamic props and interactive things
            std::vector<Enemy> Enemies;             ///< all non-player characters
            std::vector<Player> Players;            ///< something to do with player spawn points
            std::vector<Collision> Collisions;      ///< invisible physical geometry of the map
            std::vector<Navmesh> Navmeshes;         ///< AI navigation meshes
            std::vector<DummyObject> DummyObjects;  ///< objects that don't appear normally; unused or for cutscenes
            std::vector<DummyEnemy> DummyEnemies;   ///< enemies that don't appear normally; unused or for cutscenes
            std::vector<ConnectCollision> ConnectCollisions;  ///< dummy parts that make a collision load another map

            /// @brief The param's name in the file.
            static constexpr const char* ParamName = "PARTS_PARAM_ST";

            /// @brief Calls Visit with each per-type list, in the order the entries are written.
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
            /// @brief Calls Visit with each per-type list, in the order the entries are written.
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

        /// @brief True for PS3/X360, false for PC.
        bool BigEndian = false;
        /// @brief Model files that are available for parts to use.
        ModelParam Models;
        /// @brief Dynamic or interactive systems such as item pickups, levers, enemy spawners, etc.
        EventParam Events;
        /// @brief Points or areas of space that trigger some sort of behavior.
        PointParam Regions;
        /// @brief Instances of actual things in the map.
        PartsParam Parts;

    protected:
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
