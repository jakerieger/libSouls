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

    /// @brief MSBS: the map layout file of Sekiro.
    ///
    /// Extension: .msb
    ///
    /// An MSBS has params of models (the model files parts can use), events, regions, routes, layers (unused), parts,
    /// and two more unused ones. Entries refer to each other by name.
    class SOULS_API MSBS : public SoulsFile<MSBS> {
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
        struct EnemyList {};               ///< just the enemy parts (a sublist of the parts)
        struct AutoDrawGroupPointList {};  ///< just the auto draw group point regions

        /// @brief A generic entry with a name.
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
            /// @brief Just the enemy parts, in file order.
            std::vector<Entry*> Enemies;
            /// @brief Just the auto draw group point regions, in file order.
            std::vector<Entry*> AutoDrawGroupPoints;

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
        /// @brief A model file available for parts to reference.
        struct SOULS_API Model : Entry {
            /// @brief A path to a .sib file, presumed to be some kind of editor placeholder.
            std::string SibPath;
            /// @brief How many parts use the model; recalculated when writing.
            int32_t InstanceCount = 0;
            /// @brief Unknown.
            int32_t Unk1C = 0;

            /// @brief The type number of this entry as stored in the file.
            virtual uint32_t Type() const = 0;
            /// @brief Whether entries of this type have type-specific data.
            virtual bool HasTypeData() const = 0;
            /// @brief Reads the type-specific data.
            virtual void ReadTypeData(BinaryReader&) {}
            /// @brief Writes the type-specific data.
            virtual void WriteTypeData(BinaryWriter&) {}
            /// @brief Reads the entry from its position in the file.
            void Read(BinaryReader& Reader);
            /// @brief Writes the entry; ID is its position among the entries of the same type.
            void Write(BinaryWriter& Writer, int32_t ID);
        };

        /// @brief Implements a model type's type number (and type data, if it has any).
        template<typename D, uint32_t Id, bool WithData>
        struct ModelImpl : Model {
            /// @brief The type number entries of this type have in the file.
            static constexpr uint32_t TypeId = Id;
            /// @brief The type number of this entry as stored in the file.
            uint32_t Type() const override { return Id; }
            /// @brief Whether entries of this type have type-specific data.
            bool HasTypeData() const override { return WithData; }
            /// @brief Reads the type-specific data.
            void ReadTypeData(BinaryReader& Reader) override {
                if constexpr (WithData) {
                    Msb::Detail::ReadVisitor V {Reader};
                    static_cast<D*>(this)->Fields(V);
                }
            }
            /// @brief Writes the type-specific data.
            void WriteTypeData(BinaryWriter& Writer) override {
                if constexpr (WithData) {
                    Msb::Detail::WriteVisitor V {Writer};
                    static_cast<D*>(this)->Fields(V);
                }
            }
        };

        /// @brief Model files that are available for parts to use.
        struct ModelParam : Msb::Detail::TypedLists<ModelParam, Model> {
            /// @brief A model for fixed terrain or scenery.
            struct MapPiece : ModelImpl<MapPiece, 0, true> {
                /// @brief Unknown.
                bool UnkT00 = false;
                /// @brief Unknown.
                bool UnkT01 = false;
                /// @brief Unknown.
                bool UnkT02 = false;
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
                /// @brief Unknown.
                float UnkT18 = 0;
                MapPiece() { Name = "mXXXXXX"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
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
            /// @brief A model for a dynamic prop.
            struct Object : ModelImpl<Object, 1, false> {
                Object() { Name = "oXXXXXX"; }
            };
            /// @brief A model for a non-player entity.
            struct Enemy : ModelImpl<Enemy, 2, false> {
                Enemy() { Name = "cXXXX"; }
            };
            /// @brief A model for a player spawn point?
            struct Player : ModelImpl<Player, 4, false> {
                Player() { Name = "c0000"; }
            };
            /// @brief A model for collision physics.
            struct Collision : ModelImpl<Collision, 5, false> {
                Collision() { Name = "hXXXXXX"; }
            };

            /// @brief Unknown; probably some kind of version number.
            int32_t Version = 35;
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
            }
            /// @brief Calls Visit with each per-type list, in the order the entries are written.
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
        /// @brief A dynamic or interactive system.
        struct SOULS_API Event : Entry {
            /// @brief Unknown.
            int32_t EventID = -1;
            /// @brief Unknown.
            Msb::Ref<PartList> PartName;
            /// @brief Unknown.
            Msb::Ref<RegionList> RegionName;
            /// @brief Identifies the event in event scripts.
            int32_t EntityID = -1;

            /// @brief The type number of this entry as stored in the file.
            virtual uint32_t Type() const = 0;
            /// @brief Whether entries of this type have type-specific data.
            virtual bool HasTypeData() const = 0;
            /// @brief Reads the entry from its position in the file.
            void Read(BinaryReader& Reader);
            /// @brief Writes the entry; ID is its position among the entries of the same type.
            void Write(BinaryWriter& Writer, int32_t ID);
            /// @brief Reads the type-specific data.
            virtual void ReadTypeData(BinaryReader&) {}
            /// @brief Writes the type-specific data.
            virtual void WriteTypeData(BinaryWriter&) {}
            /// @brief Turns the indices read from the file into the names of the entries they refer to.
            virtual void GetNames(const Entries& Lists);
            /// @brief Turns the names of referenced entries into the indices written to the file.
            virtual void GetIndices(const Entries& Lists);
        };

        /// @brief Implements an event type's type number and type data from its Fields function.
        template<typename D, uint32_t Id, bool WithData = true>
        struct EventImpl : Event {
            /// @brief The type number entries of this type have in the file.
            static constexpr uint32_t TypeId = Id;
            /// @brief The type number of this entry as stored in the file.
            uint32_t Type() const override { return Id; }
            /// @brief Whether entries of this type have type-specific data.
            bool HasTypeData() const override { return WithData; }
            /// @brief Reads the type-specific data.
            void ReadTypeData(BinaryReader& Reader) override {
                if constexpr (WithData) {
                    Msb::Detail::ReadVisitor V {Reader};
                    static_cast<D*>(this)->Fields(V);
                }
            }
            /// @brief Writes the type-specific data.
            void WriteTypeData(BinaryWriter& Writer) override {
                if constexpr (WithData) {
                    Msb::Detail::WriteVisitor V {Writer};
                    static_cast<D*>(this)->Fields(V);
                }
            }
            /// @brief Turns the indices read from the file into the names of the entries they refer to.
            void GetNames(const Entries& Lists) override {
                Event::GetNames(Lists);
                if constexpr (WithData) {
                    Msb::Detail::NameVisitor<Entries> V {Lists};
                    static_cast<D*>(this)->Fields(V);
                }
            }
            /// @brief Turns the names of referenced entries into the indices written to the file.
            void GetIndices(const Entries& Lists) override {
                Event::GetIndices(Lists);
                if constexpr (WithData) {
                    Msb::Detail::IndexVisitor<Entries> V {Lists};
                    static_cast<D*>(this)->Fields(V);
                }
            }
        };

        /// @brief Dynamic or interactive systems such as item pickups, levers, enemy spawners, etc.
        struct EventParam : Msb::Detail::TypedLists<EventParam, Event> {
            /// @brief An item pickup in the open or inside a container.
            struct Treasure : EventImpl<Treasure, 4> {
                /// @brief The part that the treasure is attached to.
                Msb::Ref<PartList> TreasurePartName;
                /// @brief The item lot to be given.
                int32_t ItemLotID = -1;
                /// @brief If not -1, uses an entry from ActionButtonParam for the pickup prompt.
                int32_t ActionButtonID = -1;
                /// @brief Animation to play when taking this treasure.
                int32_t PickupAnimID = -1;
                /// @brief Changes the text of the pickup prompt.
                bool InChest = false;
                /// @brief Whether the treasure should be hidden by default.
                bool StartDisabled = false;
                Treasure() { Name = "Event: Treasure"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
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

            /// @brief An enemy spawner.
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
                /// @brief Unknown.
                float UnkT14 = 0;
                /// @brief Unknown.
                float UnkT18 = 0;
                /// @brief Regions where parts will spawn from.
                std::array<Msb::Ref<RegionList>, 8> SpawnRegionNames;
                /// @brief Parts that will be respawned.
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

            /// @brief An interactive object.
            struct ObjAct : EventImpl<ObjAct, 7> {
                /// @brief Unknown why objacts need an extra entity ID.
                int32_t ObjActEntityID = -1;
                /// @brief The part to be interacted with.
                Msb::Ref<PartList> ObjActPartName;
                /// @brief A row in ObjActParam.
                int32_t ObjActID = -1;
                /// @brief Unknown.
                uint8_t StateType = 0;
                /// @brief Unknown.
                int32_t EventFlagID = -1;
                ObjAct() { Name = "Event: ObjAct"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
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

            /// @brief Shifts the entire map; already accounted for in MSB coordinates.
            struct MapOffset : EventImpl<MapOffset, 9> {
                /// @brief How much to shift by.
                Vector3 Position;
                /// @brief Unknown, but looks like rotation.
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
            struct PatrolInfo : EventImpl<PatrolInfo, 14> {
                /// @brief Unknown.
                struct WREntry {
                    /// @brief Unknown.
                    Msb::Ref<RegionList, int16_t> RegionName;
                    /// @brief Unknown.
                    int32_t Unk04 = 0;
                    /// @brief Unknown.
                    int32_t Unk08 = 0;
                    /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                    /// references.
                    template<typename V>
                    void Fields(V& F) {
                        F(RegionName);
                        F.template Const<int16_t>(0);
                        F(Unk04);
                        F(Unk08);
                    }
                };
                /// @brief Unknown.
                int32_t UnkT00 = 0;
                /// @brief Unknown.
                std::array<Msb::Ref<RegionList, int16_t>, 32> WalkRegionNames;
                /// @brief Unknown.
                std::array<WREntry, 5> WREntries;
                PatrolInfo() { Name = "Event: PatrolInfo"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
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

            /// @brief Unknown.
            struct PlatoonInfo : EventImpl<PlatoonInfo, 15> {
                /// @brief Unknown.
                int32_t PlatoonIDScriptActive = 0;
                /// @brief Unknown.
                int32_t State = 0;
                /// @brief Unknown.
                std::array<Msb::Ref<PartList>, 32> GroupPartNames;
                PlatoonInfo() { Name = "Event: PlatoonInfo"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
                template<typename V>
                void Fields(V& F) {
                    F(PlatoonIDScriptActive);
                    F(State);
                    F.Pad(8);
                    F(GroupPartNames);
                }
            };

            /// @brief A resource item placed in the map; uses the base event's region for positioning.
            struct ResourceItemInfo : EventImpl<ResourceItemInfo, 17> {
                /// @brief ID of a row in ResourceItemLotParam that determines the resource(s) to give.
                int32_t ResourceItemLotParamID = 0;
                ResourceItemInfo() { Name = "Event: ResourceItemInfo"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
                template<typename V>
                void Fields(V& F) {
                    F(ResourceItemLotParamID);
                    F.Pad(0x1C);
                }
            };

            /// @brief Sets the grass lod parameters for the map.
            struct GrassLodParam : EventImpl<GrassLodParam, 18> {
                /// @brief ID of a row in GrassLodRangeParam.
                int32_t GrassLodRangeParamID = 0;
                GrassLodParam() { Name = "Event: GrassLodParam"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
                template<typename V>
                void Fields(V& F) {
                    F(GrassLodRangeParamID);
                    F.Pad(0x1C);
                }
            };

            /// @brief Unknown.
            struct SkitInfo : EventImpl<SkitInfo, 20> {
                /// @brief Unknown.
                int32_t UnkT00 = 0;
                /// @brief Unknown.
                uint8_t UnkT04 = 0;
                /// @brief Unknown.
                uint8_t UnkT05 = 0;
                /// @brief Unknown.
                uint8_t UnkT06 = 0;
                /// @brief Unknown.
                uint8_t UnkT07 = 0;
                SkitInfo() { Name = "Event: SkitInfo"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
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

            /// @brief Unknown.
            struct PlacementGroup : EventImpl<PlacementGroup, 21> {
                /// @brief Unknown.
                std::array<Msb::Ref<PartList>, 32> Event21PartNames;
                PlacementGroup() { Name = "Event: PlacementGroup"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
                template<typename V>
                void Fields(V& F) {
                    F(Event21PartNames);
                }
            };

            /// @brief Unknown.
            struct PartsGroup : EventImpl<PartsGroup, 22, false> {
                PartsGroup() { Name = "Event: PartsGroup"; }
            };

            /// @brief Unknown.
            struct Talk : EventImpl<Talk, 23> {
                /// @brief Unknown.
                int32_t UnkT00 = 0;
                /// @brief Unknown.
                std::array<Msb::Ref<EnemyList>, 8> EnemyNames;
                /// @brief IDs of talk ESDs.
                std::array<int32_t, 8> TalkIDs {};
                /// @brief Unknown.
                int16_t UnkT44 = 0;
                /// @brief Unknown.
                int16_t UnkT46 = 0;
                /// @brief Unknown.
                int32_t UnkT48 = 0;
                Talk() { Name = "Event: Talk"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
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

            /// @brief Specifies a collision to which an autodrawgroup filming point belongs, whatever that means.
            struct AutoDrawGroupCollision : EventImpl<AutoDrawGroupCollision, 24> {
                /// @brief Name of the filming point for the autodrawgroup capture, probably.
                Msb::Ref<AutoDrawGroupPointList> AutoDrawGroupPointName;
                /// @brief The collision that the filming point belongs to, presumably.
                Msb::Ref<CollisionList> OwningCollisionName;
                AutoDrawGroupCollision() { Name = "Event: AutoDrawGroupCollision"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
                template<typename V>
                void Fields(V& F) {
                    F(AutoDrawGroupPointName);
                    F(OwningCollisionName);
                    F.Pad(0x18);
                }
            };

            /// @brief Unknown.
            struct Other : EventImpl<Other, 0xFFFFFFFF, false> {
                Other() { Name = "Event: Other"; }
            };

            /// @brief Unknown; probably some kind of version number.
            int32_t Version = 35;
            /// @brief Item pickups out in the open or inside containers.
            std::vector<Treasure> Treasures;
            /// @brief Enemy spawners.
            std::vector<Generator> Generators;
            /// @brief Interactive objects like levers and doors.
            std::vector<ObjAct> ObjActs;
            /// @brief Indicates a shift of the entire map; already accounted for in MSB positions, but must be applied
            /// to other formats such as BTL. Should only be one per map, if any.
            std::vector<MapOffset> MapOffsets;
            /// @brief The PatrolInfo entries.
            std::vector<PatrolInfo> PatrolInfos;
            /// @brief The PlatoonInfo entries.
            std::vector<PlatoonInfo> PlatoonInfos;
            /// @brief The ResourceItemInfo entries.
            std::vector<ResourceItemInfo> ResourceItemInfos;
            /// @brief Sets the grass lod parameters for the map. Should only be one per map, if any.
            std::vector<GrassLodParam> GrassLodParams;
            /// @brief The SkitInfo entries.
            std::vector<SkitInfo> SkitInfos;
            /// @brief Unknown.
            std::vector<PlacementGroup> PlacementGroups;
            /// @brief Unknown.
            std::vector<PartsGroup> PartsGroups;
            /// @brief Unknown.
            std::vector<Talk> Talks;
            /// @brief Unknown.
            std::vector<AutoDrawGroupCollision> AutoDrawGroupCollisions;
            /// @brief Unknown.
            std::vector<Other> Others;

            /// @brief The param's name in the file.
            static constexpr const char* ParamName = "EVENT_PARAM_ST";

            /// @brief Calls Visit with each per-type list, in the order the entries are written.
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
            /// @brief Calls Visit with each per-type list, in the order the entries are written.
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
        /// @brief A point or volume that triggers some sort of interaction.
        struct SOULS_API Region : Entry {
            /// @brief The shape of the region.
            Msb::Shape Shape = Msb::Shapes::Point {};
            /// @brief The location of the region.
            Vector3 Position;
            /// @brief The rotation of the region, in degrees.
            Vector3 Rotation;
            /// @brief Unknown.
            int32_t Unk2C = 0;
            /// @brief Controls whether the region is active in different ceremonies.
            uint32_t MapStudioLayer = 0xFFFFFFFF;
            /// @brief Unknown.
            std::vector<int16_t> UnkA;
            /// @brief Unknown.
            std::vector<int16_t> UnkB;
            /// @brief If specified, the region is only active when the part is loaded.
            Msb::Ref<PartList> ActivationPartName;
            /// @brief Identifies the region in event scripts.
            int32_t EntityID = -1;

            /// @brief The type number of this entry as stored in the file.
            virtual uint32_t Type() const = 0;
            /// @brief Whether entries of this type have type-specific data.
            virtual bool HasTypeData() const = 0;
            /// @brief Reads the entry from its position in the file.
            void Read(BinaryReader& Reader);
            /// @brief Writes the entry; ID is its position among the entries of the same type.
            void Write(BinaryWriter& Writer, int32_t ID);
            /// @brief Reads the type-specific data.
            virtual void ReadTypeData(BinaryReader&) {}
            /// @brief Writes the type-specific data.
            virtual void WriteTypeData(BinaryWriter&) {}
            /// @brief Turns the indices read from the file into the names of the entries they refer to.
            virtual void GetNames(const Entries& Lists);
            /// @brief Turns the names of referenced entries into the indices written to the file.
            virtual void GetIndices(const Entries& Lists);
        };

        /// @brief Implements a region type's type number and type data from its Fields function.
        template<typename D, uint32_t Id, bool WithData>
        struct RegionImpl : Region {
            /// @brief The type number entries of this type have in the file.
            static constexpr uint32_t TypeId = Id;
            /// @brief The type number of this entry as stored in the file.
            uint32_t Type() const override { return Id; }
            /// @brief Whether entries of this type have type-specific data.
            bool HasTypeData() const override { return WithData; }
            /// @brief Reads the type-specific data.
            void ReadTypeData(BinaryReader& Reader) override {
                if constexpr (WithData) {
                    Msb::Detail::ReadVisitor V {Reader};
                    static_cast<D*>(this)->Fields(V);
                }
            }
            /// @brief Writes the type-specific data.
            void WriteTypeData(BinaryWriter& Writer) override {
                if constexpr (WithData) {
                    Msb::Detail::WriteVisitor V {Writer};
                    static_cast<D*>(this)->Fields(V);
                }
            }
            /// @brief Turns the indices read from the file into the names of the entries they refer to.
            void GetNames(const Entries& Lists) override {
                Region::GetNames(Lists);
                if constexpr (WithData) {
                    Msb::Detail::NameVisitor<Entries> V {Lists};
                    static_cast<D*>(this)->Fields(V);
                }
            }
            /// @brief Turns the names of referenced entries into the indices written to the file.
            void GetIndices(const Entries& Lists) override {
                Region::GetIndices(Lists);
                if constexpr (WithData) {
                    Msb::Detail::IndexVisitor<Entries> V {Lists};
                    static_cast<D*>(this)->Fields(V);
                }
            }
        };

        /// @brief Points and volumes used to trigger various effects.
        struct PointParam : Msb::Detail::TypedLists<PointParam, Region> {
            /// @brief Previously points where players will appear when invading; may not do anything in Sekiro.
            struct InvasionPoint : RegionImpl<InvasionPoint, 1, true> {
                /// @brief Not sure what this does.
                int32_t Priority = 0;
                InvasionPoint() { Name = "Region: InvasionPoint"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
                template<typename V>
                void Fields(V& F) {
                    F(Priority);
                }
            };

            /// @brief Unknown.
            struct EnvironmentMapPoint : RegionImpl<EnvironmentMapPoint, 2, true> {
                /// @brief Unknown.
                float UnkT00 = 0;
                /// @brief Unknown.
                int32_t UnkT04 = 0;
                /// @brief Unknown.
                int32_t UnkT0C = 0;
                /// @brief Unknown.
                float UnkT10 = 0;
                /// @brief Unknown.
                float UnkT14 = 0;
                /// @brief Unknown.
                int32_t UnkT18 = 0;
                /// @brief Unknown.
                int32_t UnkT1C = 0;
                /// @brief Unknown.
                int32_t UnkT20 = 0;
                /// @brief Unknown.
                int32_t UnkT24 = 0;
                /// @brief Unknown.
                int32_t UnkT28 = 0;
                EnvironmentMapPoint() { Name = "Region: EnvironmentMapPoint"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
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

            /// @brief An area where a sound plays.
            struct Sound : RegionImpl<Sound, 4, true> {
                /// @brief The category of the sound.
                int32_t SoundType = 0;
                /// @brief The ID of the sound.
                int32_t SoundID = 0;
                /// @brief References to other regions used to build a composite shape.
                std::array<Msb::Ref<RegionList>, 16> ChildRegionNames;
                /// @brief Unknown.
                int32_t UnkT48 = 0;
                Sound() { Name = "Region: Sound"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
                template<typename V>
                void Fields(V& F) {
                    F(SoundType);
                    F(SoundID);
                    F(ChildRegionNames);
                    F(UnkT48);
                }
            };

            /// @brief A point where a particle effect can play.
            struct SFX : RegionImpl<SFX, 5, true> {
                /// @brief The ID of the particle effect FFX.
                int32_t EffectID = 0;
                /// @brief Unknown.
                int32_t UnkT04 = 0;
                /// @brief Whether the effect is off until activated.
                int32_t StartDisabled = 0;
                SFX() { Name = "Region: SFX"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
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

            /// @brief Unknown.
            struct WindSFX : RegionImpl<WindSFX, 6, true> {
                /// @brief ID of the effect FFX.
                int32_t EffectID = 0;
                /// @brief Reference to a WindArea region.
                Msb::Ref<RegionList> WindAreaName;
                /// @brief Unknown.
                float UnkT18 = 0;
                WindSFX() { Name = "Region: WindSFX"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
                template<typename V>
                void Fields(V& F) {
                    F(EffectID);
                    F.Pattern(0x10, 0xFF);
                    F(WindAreaName);
                    F(UnkT18);
                    F.template Const<int32_t>(0);
                }
            };

            /// @brief A point where the player can spawn into the map.
            struct SpawnPoint : RegionImpl<SpawnPoint, 8, true> {
                SpawnPoint() { Name = "Region: SpawnPoint"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
                template<typename V>
                void Fields(V& F) {
                    F.template Const<int32_t>(-1);
                    F.Pad(12);
                }
            };

            /// @brief A point along an NPC patrol path.
            struct PatrolRoute : RegionImpl<PatrolRoute, 11, false> {
                PatrolRoute() { Name = "Region: PatrolRoute"; }
            };
            /// @brief A point the player can be warped to.
            struct WarpPoint : RegionImpl<WarpPoint, 13, false> {
                WarpPoint() { Name = "Region: WarpPoint"; }
            };
            /// @brief An area that triggers enemies when entered.
            struct ActivationArea : RegionImpl<ActivationArea, 14, false> {
                ActivationArea() { Name = "Region: ActivationArea"; }
            };
            /// @brief A generic area used by event scripts.
            struct Event : RegionImpl<Event, 15, false> {
                Event() { Name = "Region: Event"; }
            };
            /// @brief Unknown.
            struct Logic : RegionImpl<Logic, 0, false> {
                Logic() { Name = "Region: Logic"; }
            };

            /// @brief Unknown.
            struct EnvironmentMapEffectBox : RegionImpl<EnvironmentMapEffectBox, 17, true> {
                /// @brief Unknown.
                float UnkT00 = 0;
                /// @brief Unknown.
                float Compare = 0;
                /// @brief Unknown.
                uint8_t UnkT08 = 0;
                /// @brief Unknown.
                uint8_t UnkT09 = 0;
                /// @brief Unknown.
                int16_t UnkT0A = 0;
                /// @brief Unknown.
                int32_t UnkT24 = 0;
                /// @brief Unknown.
                float UnkT28 = 0;
                /// @brief Unknown.
                float UnkT2C = 0;
                EnvironmentMapEffectBox() { Name = "Region: EnvironmentMapEffectBox"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
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

            /// @brief Unknown.
            struct WindArea : RegionImpl<WindArea, 18, false> {
                WindArea() { Name = "Region: WindArea"; }
            };

            /// @brief An area where sound is muffled.
            struct MufflingBox : RegionImpl<MufflingBox, 20, true> {
                /// @brief Unknown.
                int32_t UnkT00 = 0;
                MufflingBox() { Name = "Region: MufflingBox"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
                template<typename V>
                void Fields(V& F) {
                    F(UnkT00);
                }
            };

            /// @brief An entrance to a muffling box.
            struct MufflingPortal : RegionImpl<MufflingPortal, 21, true> {
                /// @brief Unknown.
                int32_t UnkT00 = 0;
                MufflingPortal() { Name = "Region: MufflingPortal"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
                template<typename V>
                void Fields(V& F) {
                    F(UnkT00);
                    F.template Const<int32_t>(0);
                }
            };

            /// @brief Unknown.
            struct SoundSpaceOverride : RegionImpl<SoundSpaceOverride, 23, true> {
                /// @brief Unknown, probably soundspace types.
                uint8_t UnkT00 = 0;
                /// @brief Unknown, probably a soundspace type.
                uint8_t UnkT01 = 0;
                SoundSpaceOverride() { Name = "Region: SoundSpaceOverride"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
                template<typename V>
                void Fields(V& F) {
                    F(UnkT00);
                    F(UnkT01);
                    F.Pad(0x1E);
                }
            };

            /// @brief Unknown.
            struct MufflingPlane : RegionImpl<MufflingPlane, 24, false> {
                MufflingPlane() { Name = "Region: MufflingPlane"; }
            };

            /// @brief Unknown.
            struct PartsGroupArea : RegionImpl<PartsGroupArea, 25, true> {
                /// @brief Unknown.
                int64_t UnkT00 = 0;
                PartsGroupArea() { Name = "Region: PartsGroupArea"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
                template<typename V>
                void Fields(V& F) {
                    F(UnkT00);
                }
            };

            /// @brief Unknown.
            struct AutoDrawGroupPoint : RegionImpl<AutoDrawGroupPoint, 26, true> {
                /// @brief Unknown.
                int64_t UnkT00 = 0;
                AutoDrawGroupPoint() { Name = "Region: AutoDrawGroupPoint"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
                template<typename V>
                void Fields(V& F) {
                    F(UnkT00);
                    F.Pad(0x18);
                }
            };

            /// @brief Most likely an unused region.
            struct Other : RegionImpl<Other, 0xFFFFFFFF, false> {
                Other() { Name = "Region: Other"; }
            };

            /// @brief Unknown; probably some kind of version number.
            int32_t Version = 35;
            /// @brief Previously points where players will appear when invading; not sure if they do anything in
            /// Sekiro.
            std::vector<InvasionPoint> InvasionPoints;
            /// @brief Unknown.
            std::vector<EnvironmentMapPoint> EnvironmentMapPoints;
            /// @brief Areas where a sound will play.
            std::vector<Sound> Sounds;
            /// @brief The SFX entries.
            std::vector<SFX> SFXs;
            /// @brief The WindSFX entries.
            std::vector<WindSFX> WindSFXs;
            /// @brief Points where the player can spawn into a map.
            std::vector<SpawnPoint> SpawnPoints;
            /// @brief Points that describe an NPC patrol path.
            std::vector<PatrolRoute> PatrolRoutes;
            /// @brief Regions for warping the player.
            std::vector<WarpPoint> WarpPoints;
            /// @brief Regions that trigger enemies when entered.
            std::vector<ActivationArea> ActivationAreas;
            /// @brief Generic regions for use with event scripts.
            std::vector<Event> Events;
            /// @brief The Logic entries.
            std::vector<Logic> Logics;
            /// @brief Unknown.
            std::vector<EnvironmentMapEffectBox> EnvironmentMapEffectBoxes;
            /// @brief Unknown.
            std::vector<WindArea> WindAreas;
            /// @brief Areas where sound is muffled.
            std::vector<MufflingBox> MufflingBoxes;
            /// @brief Entrances to muffling boxes.
            std::vector<MufflingPortal> MufflingPortals;
            /// @brief Unknown.
            std::vector<SoundSpaceOverride> SoundSpaceOverrides;
            /// @brief Unknown.
            std::vector<MufflingPlane> MufflingPlanes;
            /// @brief Unknown.
            std::vector<PartsGroupArea> PartsGroupAreas;
            /// @brief Unknown.
            std::vector<AutoDrawGroupPoint> AutoDrawGroupPoints;
            /// @brief Most likely a dumping ground for unused regions.
            std::vector<Other> Others;

            /// @brief The param's name in the file.
            static constexpr const char* ParamName = "POINT_PARAM_ST";

            /// @brief Calls Visit with each per-type list, in the order the entries are written.
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
            /// @brief Calls Visit with each per-type list, in the order the entries are written.
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
        /// @brief Unknown, but related to muffling regions somehow.
        struct SOULS_API Route : Entry {
            /// @brief Unknown.
            int32_t Unk08 = 0;
            /// @brief Unknown.
            int32_t Unk0C = 0;

            /// @brief The type number of this entry as stored in the file.
            virtual uint32_t Type() const = 0;
            /// @brief Reads the entry from its position in the file.
            void Read(BinaryReader& Reader);
            /// @brief Writes the entry; ID is its position among the entries of the same type.
            void Write(BinaryWriter& Writer, int32_t ID);
        };

        /// @brief Implements a route type's type number.
        template<uint32_t Id>
        struct RouteOf : Route {
            /// @brief The type number entries of this type have in the file.
            static constexpr uint32_t TypeId = Id;
            /// @brief The type number of this entry as stored in the file.
            uint32_t Type() const override { return Id; }
        };

        /// @brief Unknown, but related to muffling regions somehow.
        struct RouteParam : Msb::Detail::TypedLists<RouteParam, Route> {
            /// @brief Unknown; has something to do with muffling portals.
            struct MufflingPortalLink : RouteOf<3> {
                MufflingPortalLink() { Name = "X-X"; }
            };
            /// @brief Unknown; has something to do with muffling boxes.
            struct MufflingBoxLink : RouteOf<4> {
                MufflingBoxLink() { Name = "X-X"; }
            };

            /// @brief Unknown; probably some kind of version number.
            int32_t Version = 35;
            /// @brief Unknown.
            std::vector<MufflingPortalLink> MufflingPortalLinks;
            /// @brief Unknown.
            std::vector<MufflingBoxLink> MufflingBoxLinks;

            /// @brief The param's name in the file.
            static constexpr const char* ParamName = "ROUTE_PARAM_ST";

            /// @brief Calls Visit with each per-type list, in the order the entries are written.
            template<typename F>
            void ForEachList(F&& Visit) {
                Visit(MufflingPortalLinks);
                Visit(MufflingBoxLinks);
            }
            /// @brief Calls Visit with each per-type list, in the order the entries are written.
            template<typename F>
            void ForEachList(F&& Visit) const {
                Visit(MufflingPortalLinks);
                Visit(MufflingBoxLinks);
            }
        };

        /// @brief Used for unused params that should never have any entries in them.
        struct EmptyParam {
            /// @brief Unknown; probably some kind of version number.
            int32_t Version = 0;
        };
#pragma endregion

#pragma region Parts
        /// @brief Unknown.
        struct UnkStruct1 {
            /// @brief Unknown.
            std::array<uint32_t, 48> CollisionMask {};
            /// @brief Unknown.
            uint8_t Condition1 = 0;
            /// @brief Unknown.
            uint8_t Condition2 = 0;
            /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
            template<typename V>
            void Fields(V& F) {
                F(CollisionMask);
                F(Condition1);
                F(Condition2);
                F.template Const<int16_t>(0);
                F.Pad(0xC0);
            }
        };

        /// @brief Unknown.
        struct UnkStruct2 {
            /// @brief Unknown.
            int32_t Condition = 0;
            /// @brief Unknown.
            std::array<int32_t, 8> DispGroups {};
            /// @brief Unknown.
            int16_t Unk24 = 0;
            /// @brief Unknown.
            int16_t Unk26 = 0;
            /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
            template<typename V>
            void Fields(V& F) {
                F(Condition);
                F(DispGroups);
                F(Unk24);
                F(Unk26);
                F.Pad(0x20);
            }
        };

        /// @brief Gparam value IDs for various part types.
        struct GparamConfig {
            /// @brief ID of the value set from LightSet ParamEditor to use.
            int32_t LightSetID = 0;
            /// @brief ID of the value set from FogParamEditor to use.
            int32_t FogParamID = 0;
            /// @brief ID of the value set from LightScattering : ParamEditor to use.
            int32_t LightScatteringID = 0;
            /// @brief ID of the value set from Env Map:Editor to use.
            int32_t EnvMapID = 0;
            /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
            template<typename V>
            void Fields(V& F) {
                F(LightSetID);
                F(FogParamID);
                F(LightScatteringID);
                F(EnvMapID);
                F.Pad(0x10);
            }
        };

        /// @brief Unknown; sceneGParam struct according to Pav.
        struct SceneGparamConfig {
            /// @brief Unknown.
            std::array<int8_t, 4> EventIDs {};
            /// @brief Unknown.
            float Unk40 = 0;
            /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
            template<typename V>
            void Fields(V& F) {
                F.Pad(0x3C);
                F(EventIDs);
                F(Unk40);
                F.Pad(12);
            }
        };

        /// @brief Unknown.
        struct UnkStruct7 {
            /// @brief Unknown.
            int32_t Unk00 = 0;
            /// @brief Unknown.
            int32_t Unk04 = 0;
            /// @brief ID in GrassTypeParam determining properties of dynamic grass on a map piece.
            int32_t GrassTypeParamID = 0;
            /// @brief Unknown.
            int32_t Unk0C = 0;
            /// @brief Unknown.
            int32_t Unk10 = 0;
            /// @brief Unknown.
            int32_t Unk14 = 0;
            /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
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

        /// @brief Which optional blocks of data a part type has.
        enum PartBlocks : unsigned {
            NoBlocks  = 0,
            HasUnk1   = 1,
            HasUnk2   = 2,
            HasGparam = 4,
            HasScene  = 8,
            HasUnk7   = 16,
        };

        /// @brief Common data for all types of part.
        struct SOULS_API Part : Entry {
            /// @brief The model used by this part; requires an entry in ModelParam.
            Msb::Ref<ModelList> ModelName;
            /// @brief A path to a .sib file, presumably some kind of editor placeholder.
            std::string SibPath;
            /// @brief Location of the part.
            Vector3 Position;
            /// @brief Rotation of the part.
            Vector3 Rotation;
            /// @brief Scale of the part; only works for map pieces and objects.
            Vector3 Scale {1.f, 1.f, 1.f};
            /// @brief Identifies the part in event scripts.
            int32_t EntityID = -1;
            /// @brief Unknown.
            uint8_t UnkE04 = 0;
            /// @brief Unknown.
            uint8_t UnkE05 = 0;
            /// @brief Unknown.
            uint8_t UnkE06 = 0;
            /// @brief Unknown.
            uint8_t LanternID = 0;
            /// @brief Unknown.
            uint8_t LodParamID = 0;
            /// @brief Unknown.
            uint8_t UnkE09 = 0;
            /// @brief Unknown.
            bool IsPointLightShadowSrc = false;
            /// @brief Unknown.
            uint8_t UnkE0B = 0;
            /// @brief Unknown.
            bool IsShadowSrc = false;
            /// @brief Unknown.
            uint8_t IsStaticShadowSrc = 0;
            /// @brief Unknown.
            uint8_t IsCascade3ShadowSrc = 0;
            /// @brief Unknown.
            uint8_t UnkE0F = 0;
            /// @brief Unknown.
            uint8_t UnkE10 = 0;
            /// @brief Unknown.
            bool IsShadowDest = false;
            /// @brief Unknown.
            bool IsShadowOnly = false;
            /// @brief Unknown.
            bool DrawByReflectCam = false;
            /// @brief Unknown.
            bool DrawOnlyReflectCam = false;
            /// @brief Unknown.
            uint8_t EnableOnAboveShadow = 0;
            /// @brief Unknown.
            bool DisablePointLightEffect = false;
            /// @brief Unknown.
            uint8_t UnkE17 = 0;
            /// @brief Unknown.
            int32_t UnkE18 = 0;
            /// @brief Allows multiple parts to be identified by the same entity ID.
            std::array<int32_t, 8> EntityGroupIDs {-1, -1, -1, -1, -1, -1, -1, -1};
            /// @brief Unknown.
            int32_t UnkE3C = 0;
            /// @brief Unknown.
            int32_t UnkE40 = 0;

            /// @brief The type number of this entry as stored in the file.
            virtual uint32_t Type() const = 0;
            /// @brief Which of the optional data blocks this part type has (a mask of PartBlocks).
            virtual unsigned Blocks() const = 0;
            /// @brief Reads the entry from its position in the file.
            void Read(BinaryReader& Reader);
            /// @brief Writes the entry; ID is its position among the entries of the same type.
            void Write(BinaryWriter& Writer, int32_t ID);
            /// @brief Reads the type-specific data.
            virtual void ReadTypeData(BinaryReader& Reader) = 0;
            /// @brief Writes the type-specific data.
            virtual void WriteTypeData(BinaryWriter& Writer) = 0;
            /// @brief Reads or writes one of the optional blocks (see PartBlocks).
            virtual void ReadBlock(unsigned Block, BinaryReader&) {}
            /// @brief Writes one of the optional data blocks.
            virtual void WriteBlock(unsigned Block, BinaryWriter&) {}
            /// @brief Turns the indices read from the file into the names of the entries they refer to.
            virtual void GetNames(const Entries& Lists);
            /// @brief Turns the names of referenced entries into the indices written to the file.
            virtual void GetIndices(const Entries& Lists);
        };

        /// @brief Implements a part type's data functions from its Fields function.
        ///
        /// D::Block (a PartBlocks mask) says which of the optional blocks it has; the blocks are members of D (Unk1,
        /// Unk2, Gparam, SceneGparam, Unk7).
        template<typename D, uint32_t Id, typename B = Part>
        struct PartImpl : B {
            /// @brief The type number entries of this type have in the file.
            static constexpr uint32_t TypeId = Id;
            /// @brief The type number of this entry as stored in the file.
            uint32_t Type() const override { return Id; }
            /// @brief Which of the optional data blocks this part type has (a mask of PartBlocks).
            unsigned Blocks() const override { return D::Block; }
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
            /// @brief Visits the fields of one of the optional data blocks.
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
                Msb::Detail::ReadVisitor V {Reader};
                VisitBlock(Block, V);
            }
            /// @brief Writes one of the optional data blocks.
            void WriteBlock(unsigned Block, BinaryWriter& Writer) override {
                Msb::Detail::WriteVisitor V {Writer};
                VisitBlock(Block, V);
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
            /// @brief Unknown.
            GparamConfig Gparam;
            /// @brief Reference to a map piece or collision; believed to determine when the object is loaded.
            Msb::Ref<PartList> ObjPartName1;
            /// @brief Unknown.
            uint8_t BreakTerm = 0;
            /// @brief Unknown.
            bool NetSyncType = false;
            /// @brief Unknown.
            uint8_t UnkT0E = 0;
            /// @brief Unknown.
            bool SetMainObjStructureBooleans = false;
            /// @brief Unknown.
            int16_t AnimID = 0;
            /// @brief Unknown.
            int16_t UnkT18 = 0;
            /// @brief Unknown.
            int16_t UnkT1A = 0;
            /// @brief References to collisions; believed to be involved with loading when grappling to the object.
            Msb::Ref<PartList> ObjPartName2;
            /// @brief Reference to a collision; believed to be involved with loading when grappling to the object.
            Msb::Ref<PartList> ObjPartName3;

            ObjectBase() { Name = "oXXXXXX_XXXX"; }
            /// @brief Visits the fields that objects and dummy objects share.
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

        /// @brief Common base data for enemies and dummy enemies.
        struct EnemyBase : Part {
            /// @brief Unknown.
            GparamConfig Gparam;
            /// @brief An ID in NPCThinkParam that determines the enemy's AI characteristics.
            int32_t ThinkParamID = -1;
            /// @brief An ID in NPCParam that determines a variety of enemy properties.
            int32_t NPCParamID = -1;
            /// @brief Previously talk ID, now always 0 or 1 except for the Memorial Mob in Senpou.
            int32_t UnkT10 = -1;
            /// @brief Unknown.
            int16_t PlatoonID = 0;
            /// @brief An ID in CharaInitParam that determines a human's inventory and stats.
            int32_t CharaInitID = -1;
            /// @brief Should reference the collision the enemy starts on.
            Msb::Ref<PartList> CollisionPartName;
            /// @brief Unknown.
            int16_t UnkT20 = 0;
            /// @brief Unknown.
            int16_t UnkT22 = 0;
            /// @brief Unknown.
            int32_t UnkT24 = 0;
            /// @brief Unknown.
            int32_t BackupEventAnimID = -1;
            /// @brief Unknown.
            int32_t EventFlagID = -1;
            /// @brief Unknown.
            int32_t EventFlagCompareState = 0;
            /// @brief Unknown.
            int32_t UnkT48 = 0;
            /// @brief Unknown.
            int32_t UnkT4C = 0;
            /// @brief Unknown.
            int32_t UnkT50 = 0;
            /// @brief Unknown.
            int32_t UnkT78 = 0;
            /// @brief Unknown.
            float UnkT84 = 0;

            EnemyBase() { Name = "cXXXX_XXXX"; }
            /// @brief Visits the fields that enemies and dummy enemies share.
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

        /// @brief Instances of actual things in the map.
        struct PartsParam : Msb::Detail::TypedLists<PartsParam, Part> {
            /// @brief Fixed visual geometry.
            struct MapPiece : PartImpl<MapPiece, 0> {
                /// @brief Which optional data blocks this part type has (a mask of PartBlocks).
                static constexpr unsigned Block = HasUnk1 | HasGparam | HasUnk7;
                /// @brief Unknown.
                UnkStruct1 Unk1;
                /// @brief Unknown.
                GparamConfig Gparam;
                /// @brief Unknown.
                UnkStruct7 Unk7;
                MapPiece() { Name = "mXXXXXX_XXXX"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
                template<typename V>
                void Fields(V& F) {
                    F.Pad(8);
                }
            };

            /// @brief A dynamic or interactible element in the map.
            struct Object : PartImpl<Object, 1, ObjectBase> {
                /// @brief Which optional data blocks this part type has (a mask of PartBlocks).
                static constexpr unsigned Block = HasUnk1 | HasGparam;
                /// @brief Unknown.
                UnkStruct1 Unk1;
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
                template<typename V>
                void Fields(V& F) {
                    ObjectFields(F);
                }
            };

            /// @brief Any non-player character.
            struct Enemy : PartImpl<Enemy, 2, EnemyBase> {
                /// @brief Which optional data blocks this part type has (a mask of PartBlocks).
                static constexpr unsigned Block = HasUnk1 | HasGparam;
                /// @brief Unknown.
                UnkStruct1 Unk1;
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
                template<typename V>
                void Fields(V& F) {
                    EnemyFields(F);
                }
            };

            /// @brief A spawn point for the player, or something.
            struct Player : PartImpl<Player, 4> {
                /// @brief Which optional data blocks this part type has (a mask of PartBlocks).
                static constexpr unsigned Block = NoBlocks;
                Player() { Name = "c0000_XXXX"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
                template<typename V>
                void Fields(V& F) {
                    F.Pad(0x10);
                }
            };

            /// @brief Invisible but physical geometry.
            struct Collision : PartImpl<Collision, 5> {
                /// @brief Which optional data blocks this part type has (a mask of PartBlocks).
                static constexpr unsigned Block = HasUnk1 | HasUnk2 | HasGparam | HasScene;
                /// @brief Unknown.
                UnkStruct1 Unk1;
                /// @brief Unknown.
                UnkStruct2 Unk2;
                /// @brief Unknown.
                GparamConfig Gparam;
                /// @brief Unknown.
                SceneGparamConfig SceneGparam;
                /// @brief Unknown.
                uint8_t HitFilterID = 0;
                /// @brief Adds reverb to sounds while on this collision to simulate echoes.
                uint8_t SoundSpaceType = 0;
                /// @brief Unknown.
                float ReflectPlaneHeight = 0;
                /// @brief Determines the text to display for map popups and save files.
                int16_t MapNameID = 0;
                /// @brief Unknown.
                bool DisableStart = false;
                /// @brief Unknown.
                uint8_t UnkT17 = 0;
                /// @brief If not -1, the bonfire with this ID will be disabled when enemies are on this collision.
                int32_t DisableBonfireEntityID = -1;
                /// @brief Unknown.
                uint8_t UnkT24 = 0;
                /// @brief Unknown.
                uint8_t UnkT25 = 0;
                /// @brief Unknown.
                uint8_t UnkT26 = 0;
                /// @brief Should alter visibility while on this collision, but does not seem to do much.
                uint8_t MapVisibility = 0;
                /// @brief Used to determine invasion eligibility.
                int32_t PlayRegionID = 0;
                /// @brief Alters camera properties while on this collision.
                int16_t LockCamParamID = 0;
                /// @brief Unknown.
                int32_t UnkT3C = 0;
                /// @brief Unknown.
                int32_t UnkT40 = 0;
                /// @brief Unknown.
                float UnkT44 = 0;
                /// @brief Unknown.
                float UnkT48 = 0;
                /// @brief Unknown.
                int32_t UnkT4C = 0;
                /// @brief Unknown.
                float UnkT50 = 0;
                /// @brief Unknown.
                float UnkT54 = 0;

                Collision() { Name = "hXXXXXX"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
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

            /// @brief An object that either isn't used, or is used for a cutscene.
            struct DummyObject : PartImpl<DummyObject, 9, ObjectBase> {
                /// @brief Which optional data blocks this part type has (a mask of PartBlocks).
                static constexpr unsigned Block = HasGparam;
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
                template<typename V>
                void Fields(V& F) {
                    ObjectFields(F);
                }
            };

            /// @brief An enemy that either isn't used, or is used for a cutscene.
            struct DummyEnemy : PartImpl<DummyEnemy, 10, EnemyBase> {
                /// @brief Which optional data blocks this part type has (a mask of PartBlocks).
                static constexpr unsigned Block = HasGparam;
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
                template<typename V>
                void Fields(V& F) {
                    EnemyFields(F);
                }
            };

            /// @brief References an actual collision and causes another map to be loaded while on it.
            struct ConnectCollision : PartImpl<ConnectCollision, 11> {
                /// @brief Which optional data blocks this part type has (a mask of PartBlocks).
                static constexpr unsigned Block = HasUnk2;
                /// @brief Unknown.
                UnkStruct2 Unk2;
                /// @brief The collision part to attach to.
                Msb::Ref<CollisionList> CollisionName;
                /// @brief The map to load when on this collision.
                std::array<uint8_t, 4> MapID {};
                ConnectCollision() { Name = "hXXXXXX_XXXX"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
                template<typename V>
                void Fields(V& F) {
                    F(CollisionName);
                    F(MapID);
                    F.Pad(8);
                }
            };

            /// @brief Unknown; probably some kind of version number.
            int32_t Version = 35;
            /// @brief All of the fixed visual geometry of the map.
            std::vector<MapPiece> MapPieces;
            /// @brief Dynamic props and interactive things.
            std::vector<Object> Objects;
            /// @brief All non-player characters.
            std::vector<Enemy> Enemies;
            /// @brief These have something to do with player spawn points.
            std::vector<Player> Players;
            /// @brief Invisible physical geometry of the map.
            std::vector<Collision> Collisions;
            /// @brief Objects that don't appear normally; either unused, or used for cutscenes.
            std::vector<DummyObject> DummyObjects;
            /// @brief Enemies that don't appear normally; either unused, or used for cutscenes.
            std::vector<DummyEnemy> DummyEnemies;
            /// @brief Dummy parts that reference an actual collision and cause it to load another map.
            std::vector<ConnectCollision> ConnectCollisions;

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
                Visit(DummyObjects);
                Visit(DummyEnemies);
                Visit(ConnectCollisions);
            }
        };
#pragma endregion

        /// @brief Model files that are available for parts to use.
        ModelParam Models;
        /// @brief Dynamic or interactive systems such as item pickups, levers, enemy spawners, etc.
        EventParam Events;
        /// @brief Points or areas of space that trigger some sort of behavior.
        PointParam Regions;
        /// @brief Unknown, but related to muffling regions somehow.
        RouteParam Routes;
        /// @brief Instances of actual things in the map.
        PartsParam Parts;
        /// @brief Unknown and unused.
        EmptyParam Layers {0x23};
        /// @brief Sets bone positions for fixed objects; not used in Sekiro.
        EmptyParam PartsPoses {0};
        /// @brief Bone names for the parts pose param; not used in Sekiro.
        EmptyParam BoneNames {0};

    protected:
        bool IsImpl(BinaryReader& Reader) override;
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
