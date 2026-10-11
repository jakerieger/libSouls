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

    /// @brief MSB3: the map layout file of Dark Souls III.
    ///
    /// Extension: .msb
    ///
    /// An MSB3 has params of models (the model files parts can use), events, regions, routes, layers, parts and parts
    /// poses. Entries refer to each other by name.
    class SOULS_API MSB3 : public SoulsFile<MSB3> {
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
        /// @brief Tag for references to bone names.
        using BoneNameList = Msb::BoneNameList;
        struct PatrolInfoList {};  ///< just the patrol info events (a sublist of the events)

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
            /// @brief Just the patrol info events, in file order.
            std::vector<Entry*> PatrolInfos;
            /// @brief Every bone name, in file order.
            std::vector<Entry*> BoneNames;

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
                } else if constexpr (std::is_same_v<Tag, PatrolInfoList>) {
                    return PatrolInfos;
                } else {
                    return BoneNames;
                }
            }
        };

#pragma region Models
        /// @brief A model available for use by parts in this map.
        struct SOULS_API Model : Entry {
            /// @brief Unknown network path to a .sib file.
            std::string SibPath;
            /// @brief How many parts use the model; recalculated when writing.
            int32_t InstanceCount = 0;

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

        /// @brief A section containing all the models available to parts in this map.
        struct ModelParam : Msb::Detail::TypedLists<ModelParam, Model> {
            /// @brief A fixed part of the level geometry.
            struct MapPiece : ModelImpl<MapPiece, 0, true> {
                /// @brief Unknown.
                uint8_t UnkT00 = 0;
                /// @brief Unknown.
                uint8_t UnkT01 = 0;
                /// @brief Unknown.
                bool UnkT02 = true;
                /// @brief Unknown.
                bool UnkT03 = true;
                MapPiece() { Name = "mXXXXXX"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
                template<typename V>
                void Fields(V& F) {
                    F(UnkT00);
                    F(UnkT01);
                    F(UnkT02);
                    F(UnkT03);
                    F.Pad(12);
                }
            };

            /// @brief A dynamic or interactible entity.
            struct Object : ModelImpl<Object, 1, true> {
                /// @brief Unknown.
                uint8_t UnkT00 = 0;
                /// @brief Unknown.
                uint8_t UnkT01 = 0;
                /// @brief Unknown.
                bool UnkT02 = true;
                /// @brief Unknown.
                bool UnkT03 = true;
                Object() { Name = "oXXXXXX"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
                template<typename V>
                void Fields(V& F) {
                    F(UnkT00);
                    F(UnkT01);
                    F(UnkT02);
                    F(UnkT03);
                    F.Pad(12);
                }
            };

            /// @brief Any character in the map that is not the player.
            struct Enemy : ModelImpl<Enemy, 2, false> {
                Enemy() { Name = "cXXXX"; }
            };

            /// @brief The player character.
            struct Player : ModelImpl<Player, 4, false> {
                Player() { Name = "c0000"; }
            };

            /// @brief The invisible physical surface of the map.
            struct Collision : ModelImpl<Collision, 5, false> {
                Collision() { Name = "hXXXXXX"; }
            };

            /// @brief Unknown.
            struct Other : ModelImpl<Other, 0xFFFFFFFF, false> {
                Other() { Name = "lXXXXXX"; }
            };

            /// @brief Map piece models in this section.
            std::vector<MapPiece> MapPieces;
            /// @brief Object models in this section.
            std::vector<Object> Objects;
            /// @brief Enemy models in this section.
            std::vector<Enemy> Enemies;
            /// @brief Player models in this section.
            std::vector<Player> Players;
            /// @brief Collision models in this section.
            std::vector<Collision> Collisions;
            /// @brief Other models in this section.
            std::vector<Other> Others;

            /// @brief The version number this param always has in this game's files.
            static constexpr int32_t ParamVersion = 3;
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
                Visit(Others);
            }
            /// @brief Calls Visit with each per-type list, in the order the entries are written.
            template<typename F>
            void ForEachList(F&& Visit) const {
                Visit(MapPieces);
                Visit(Objects);
                Visit(Enemies);
                Visit(Players);
                Visit(Collisions);
                Visit(Others);
            }
        };
#pragma endregion

#pragma region Events
        /// @brief An interactive or dynamic feature of the map.
        struct SOULS_API Event : Entry {
            /// @brief Unknown.
            int32_t EventID = -1;
            /// @brief The part and region the event is attached to.
            Msb::Ref<PartList> PartName;
            /// @brief The name of a region the event is attached to.
            Msb::Ref<RegionList> PointName;
            /// @brief Used to identify the event in event scripts.
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

        /// @brief Implements an event type's type number and type data from its Fields function.
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

        /// @brief Events controlling various interactive or dynamic features in the map.
        struct EventParam : Msb::Detail::TypedLists<EventParam, Event> {
            /// @brief A pickuppable item.
            struct Treasure : EventImpl<Treasure, 4> {
                /// @brief The part the treasure is attached to.
                Msb::Ref<PartList> TreasurePartName;
                /// @brief First item lot given by this treasure, and second item lot (rarely used).
                int32_t ItemLot1 = -1;
                /// @brief Second item lot given by this treasure; rarely used.
                int32_t ItemLot2 = -1;
                /// @brief Unknown.
                int32_t UnkT18 = -1;
                /// @brief If not -1, uses an entry from ActionButtonParam for the pickup prompt.
                int32_t ActionButtonParamID = -1;
                /// @brief Animation to play when taking this treasure.
                int32_t PickupAnimID = 60070;
                /// @brief Changes the text of the pickup prompt and causes the treasure to be uninteractible by
                ///        default.
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
                    F(ItemLot1);
                    F(ItemLot2);
                    F(UnkT18);
                    for (int I = 0; I < 7; ++I) {
                        F.template Const<int32_t>(-1);
                    }
                    F(ActionButtonParamID);
                    F(PickupAnimID);
                    F(InChest);
                    F(StartDisabled);
                    F.Pad(2);
                    F.Pad(12);
                }
            };

            /// @brief A continuous enemy spawner.
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
                /// @brief Regions that enemies can be spawned at.
                std::array<Msb::Ref<RegionList>, 8> SpawnPointNames;
                /// @brief Enemies spawned by this generator.
                std::array<Msb::Ref<PartList>, 32> SpawnPartNames;
                /// @brief Unknown.
                uint8_t InitialSpawnCount = 0;
                /// @brief Unknown.
                float UnkT14 = 0;
                /// @brief Unknown.
                float UnkT18 = 0;
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
                    F.Pad(20);
                    F(SpawnPointNames);
                    F.Pad(16);
                    F(SpawnPartNames);
                    F.Pad(32);
                }
            };

            /// @brief Controls usable objects like levers.
            struct ObjAct : EventImpl<ObjAct, 7> {
                /// @brief How the object behaves when used.
                enum class ObjActState : uint8_t {
                    OneState      = 0,
                    DoorState     = 1,
                    OneLoopState  = 2,
                    OneLoopState2 = 3,
                    DoorState2    = 4,
                };
                /// @brief Unknown.
                int32_t ObjActEntityID = -1;
                /// @brief The object which is being interacted with.
                Msb::Ref<PartList> ObjActPartName;
                /// @brief ID in ObjActParam that configures this ObjAct.
                int32_t ObjActParamID = 0;
                /// @brief Unknown.
                ObjActState ObjActStateType = ObjActState::OneState;
                /// @brief Unknown.
                int32_t EventFlagID = 0;
                ObjAct() { Name = "Event: ObjAct"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
                template<typename V>
                void Fields(V& F) {
                    F(ObjActEntityID);
                    F(ObjActPartName);
                    F(ObjActParamID);
                    F(ObjActStateType);
                    F.Pad(3);
                    F(EventFlagID);
                    F.Pad(12);
                }
            };

            /// @brief Moves all of the map pieces when cutscenes are played.
            struct MapOffset : EventImpl<MapOffset, 9> {
                /// @brief Position of the map offset.
                Vector3 Position;
                /// @brief Rotation of the map offset.
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

            /// @brief A fake multiplayer interaction where the player goes to an NPC's world.
            struct PseudoMultiplayer : EventImpl<PseudoMultiplayer, 12> {
                /// @brief The NPC whose world you are entering.
                int32_t HostEntityID = -1;
                /// @brief Set when inside the event's region, unset when outside it.
                int32_t EventFlagID = -1;
                /// @brief ID of a goods item that is used to trigger the event.
                int32_t ActivateGoodsID = -1;
                /// @brief Unknown; possibly a sound ID, a map event ID, and flags.
                int32_t UnkT0C = -1;
                /// @brief Unknown; possibly a map event ID.
                int32_t UnkT10 = -1;
                /// @brief Unknown; possibly flags.
                int32_t UnkT14 = 0;
                /// @brief Unknown.
                int32_t UnkT18 = 0;
                PseudoMultiplayer() { Name = "Event: PseudoMultiplayer"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
                template<typename V>
                void Fields(V& F) {
                    F(HostEntityID);
                    F(EventFlagID);
                    F(ActivateGoodsID);
                    F(UnkT0C);
                    F(UnkT10);
                    F(UnkT14);
                    F(UnkT18);
                    F.Pad(4);
                }
            };

            /// @brief A simple list of points defining a path for enemies to take.
            struct PatrolInfo : EventImpl<PatrolInfo, 14> {
                /// @brief Unknown; probably some kind of route type.
                int32_t UnkT00 = 0;
                /// @brief List of points in the route.
                std::array<Msb::Ref<RegionList, int16_t>, 32> WalkPointNames;
                PatrolInfo() { Name = "Event: PatrolInfo"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
                template<typename V>
                void Fields(V& F) {
                    F(UnkT00);
                    F.Pad(12);
                    F(WalkPointNames);
                }
            };

            /// @brief Unknown.
            struct PlatoonInfo : EventImpl<PlatoonInfo, 15> {
                /// @brief Unknown.
                int32_t PlatoonIDScriptActivate = 0;
                /// @brief Unknown.
                int32_t State = 0;
                /// @brief Unknown.
                std::array<Msb::Ref<PartList>, 32> GroupPartsNames;
                PlatoonInfo() { Name = "Event: PlatoonInfo"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
                template<typename V>
                void Fields(V& F) {
                    F(PlatoonIDScriptActivate);
                    F(State);
                    F.Pad(8);
                    F(GroupPartsNames);
                }
            };

            /// @brief Unknown.
            ///
            /// Only appears once in one unused MSB.
            struct Other : EventImpl<Other, 0xFFFFFFFF> {
                /// @brief Unknown; possibly a sound type.
                int32_t UnkT00 = 0;
                /// @brief Unknown; possibly a sound ID.
                int32_t UnkT04 = 0;
                Other() { Name = "Event: Other"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
                template<typename V>
                void Fields(V& F) {
                    F(UnkT00);
                    F(UnkT04);
                    F.Pattern(0x40, 0xFF);
                }
            };

            /// @brief Treasures in the MSB.
            std::vector<Treasure> Treasures;
            /// @brief Generators in the MSB.
            std::vector<Generator> Generators;
            /// @brief Object actions in the MSB.
            std::vector<ObjAct> ObjActs;
            /// @brief Map offsets in the MSB.
            std::vector<MapOffset> MapOffsets;
            /// @brief Pseudo multiplayer events in the MSB.
            std::vector<PseudoMultiplayer> PseudoMultiplayers;
            /// @brief The PatrolInfo entries.
            std::vector<PatrolInfo> PatrolInfos;
            /// @brief The PlatoonInfo entries.
            std::vector<PlatoonInfo> PlatoonInfos;
            /// @brief Other events in the MSB.
            std::vector<Other> Others;

            /// @brief The version number this param always has in this game's files.
            static constexpr int32_t ParamVersion = 3;
            /// @brief The param's name in the file.
            static constexpr const char* ParamName = "EVENT_PARAM_ST";

            /// @brief Calls Visit with each per-type list, in the order the entries are written.
            template<typename F>
            void ForEachList(F&& Visit) {
                Visit(Treasures);
                Visit(Generators);
                Visit(ObjActs);
                Visit(MapOffsets);
                Visit(PseudoMultiplayers);
                Visit(PatrolInfos);
                Visit(PlatoonInfos);
                Visit(Others);
            }
            /// @brief Calls Visit with each per-type list, in the order the entries are written.
            template<typename F>
            void ForEachList(F&& Visit) const {
                Visit(Treasures);
                Visit(Generators);
                Visit(ObjActs);
                Visit(MapOffsets);
                Visit(PseudoMultiplayers);
                Visit(PatrolInfos);
                Visit(PlatoonInfos);
                Visit(Others);
            }
        };
#pragma endregion

#pragma region Regions
        /// @brief Whether a region type has type-specific data after its common data.
        enum class TypeDataPresence {
            Never,
            Sometimes,
            Always,
            AlwaysNull,  ///< the data is always there but is not pointed to by an offset
        };

        /// @brief A point or volumetric area used for a variety of purposes.
        struct SOULS_API Region : Entry {
            /// @brief Unknown.
            int32_t Unk2C = 0;
            /// @brief The shape of this region.
            ///
            /// Composite shapes aren't supported in Dark Souls III.
            Msb::Shape Shape = Msb::Shapes::Point {};
            /// @brief Controls whether the event is present in different ceremonies.
            ///
            /// Maybe only used for messages?
            uint32_t MapStudioLayer = 0;
            /// @brief Center of the region.
            Vector3 Position;
            /// @brief Rotation of the region, in degrees.
            Vector3 Rotation;
            /// @brief Unknown.
            std::vector<int16_t> UnkA;
            /// @brief Unknown.
            std::vector<int16_t> UnkB;
            /// @brief Region is inactive unless this part is drawn; empty for always active.
            Msb::Ref<PartList> ActivationPartName;
            /// @brief An ID used to identify this region in event scripts.
            int32_t EntityID = -1;

            /// @brief The type number of this entry as stored in the file.
            virtual uint32_t Type() const = 0;
            /// @brief Whether entries of this type have type-specific data.
            virtual TypeDataPresence ShouldHaveTypeData() const = 0;
            /// @brief Whether this region writes type-specific data (only optional for some types).
            virtual bool DoesHaveTypeData() const { return ShouldHaveTypeData() != TypeDataPresence::Never; }
            /// @brief Reads the entry from its position in the file.
            void Read(BinaryReader& Reader);
            /// @brief Writes the entry; ID is its position among the entries of the same type.
            void Write(BinaryWriter& Writer, int32_t ID);
            /// @brief Reads the type-specific data.
            virtual void ReadTypeData(BinaryReader&) {}
            /// @brief Writes the type-specific data.
            virtual void WriteTypeData(BinaryWriter&) {}
            /// @brief Prepares the region to be read: types whose data is optional forget it until the read finds some.
            virtual void ResetForRead() {}
            /// @brief Turns the indices read from the file into the names of the entries they refer to.
            virtual void GetNames(const Entries& Lists);
            /// @brief Turns the names of referenced entries into the indices written to the file.
            virtual void GetIndices(const Entries& Lists);
        };

        /// @brief Implements a region type's type number and type data from its Fields function.
        template<typename D, uint32_t Id, TypeDataPresence Presence>
        struct RegionImpl : Region {
            /// @brief The type number entries of this type have in the file.
            static constexpr uint32_t TypeId = Id;
            /// @brief The type number of this entry as stored in the file.
            uint32_t Type() const override { return Id; }
            /// @brief Whether entries of this type have type-specific data.
            TypeDataPresence ShouldHaveTypeData() const override { return Presence; }
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
                Region::GetNames(Lists);
                Msb::Detail::NameVisitor<Entries> V {Lists};
                static_cast<D*>(this)->Fields(V);
            }
            /// @brief Turns the names of referenced entries into the indices written to the file.
            void GetIndices(const Entries& Lists) override {
                Region::GetIndices(Lists);
                Msb::Detail::IndexVisitor<Entries> V {Lists};
                static_cast<D*>(this)->Fields(V);
            }
        };

        /// @brief A section containing points and volumes for various purposes.
        struct PointParam : Msb::Detail::TypedLists<PointParam, Region> {
            /// @brief A point where other players invade your world.
            struct InvasionPoint : RegionImpl<InvasionPoint, 1, TypeDataPresence::Always> {
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
            struct EnvironmentMapPoint : RegionImpl<EnvironmentMapPoint, 2, TypeDataPresence::Sometimes> {
                /// @brief Whether or not the type data will be written to the file.
                bool SaveTypeData = true;
                /// @brief Unknown; observed values 0x80 and 0x100.
                int32_t UnkT00 = 0x80;
                EnvironmentMapPoint() { Name = "Region: EnvironmentMapPoint"; }
                /// @brief Whether this region writes type-specific data (only optional for some types).
                bool DoesHaveTypeData() const override { return SaveTypeData; }
                /// @brief Prepares the region to be read: types whose data is optional forget it until the read finds
                /// some.
                void ResetForRead() override { SaveTypeData = false; }
                /// @brief Reads the type-specific data.
                void ReadTypeData(BinaryReader& Reader) override {
                    SaveTypeData = true;
                    RegionImpl::ReadTypeData(Reader);
                }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
                template<typename V>
                void Fields(V& F) {
                    F(UnkT00);
                    F.template Const<int32_t>(0);
                }
            };

            /// @brief A region that plays a sound while you are in it.
            struct Sound : RegionImpl<Sound, 4, TypeDataPresence::Always> {
                /// @brief Types of sound that may be in a Sound region.
                enum class SndType : uint32_t {
                    Environment = 0,  ///< ambient sounds like wind, creaking, etc.
                    BGM         = 6,  ///< boss fight music
                    Voice       = 7,  ///< character voices
                };
                /// @brief Type of sound in this region; determines mixing behavior like muffling.
                SndType SoundType = SndType::Environment;
                /// @brief ID of the sound to play in this region, or 0 for child regions.
                int32_t SoundID = 0;
                /// @brief Names of other Sound regions which extend this one.
                std::array<Msb::Ref<RegionList>, 16> ChildRegionNames;
                Sound() { Name = "Region: Sound"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
                template<typename V>
                void Fields(V& F) {
                    F(SoundType);
                    F(SoundID);
                    F(ChildRegionNames);
                }
            };

            /// @brief A region that plays a special effect.
            struct SFX : RegionImpl<SFX, 5, TypeDataPresence::Always> {
                /// @brief The ID of the .fxr file to play in this region.
                int32_t EffectID = -1;
                /// @brief Unknown.
                int32_t UnkT04 = -1;
                /// @brief If true, the effect is off by default until enabled by event scripts.
                bool StartDisabled = false;
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
                    F.AsInt32(StartDisabled);
                }
            };

            /// @brief Unknown exactly what this does.
            struct WindSFX : RegionImpl<WindSFX, 6, TypeDataPresence::Always> {
                /// @brief ID of an .fxr file.
                int32_t EffectID = -1;
                /// @brief Name of a corresponding WindArea region.
                Msb::Ref<RegionList> WindAreaName;
                WindSFX() { Name = "Region: WindSFX"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
                template<typename V>
                void Fields(V& F) {
                    F(EffectID);
                    F.template Const<int32_t>(-1);
                    F.template Const<int32_t>(-1);
                    F.template Const<int32_t>(-1);
                    F.template Const<int32_t>(-1);
                    F(WindAreaName);
                    F.template Const<float>(-1);
                }
            };

            /// @brief A region where players enter the map.
            struct SpawnPoint : RegionImpl<SpawnPoint, 8, TypeDataPresence::Always> {
                /// @brief Unknown; seems kind of like a region index, but also kind of doesn't.
                int32_t UnkT00 = -1;
                SpawnPoint() { Name = "Region: SpawnPoint"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
                template<typename V>
                void Fields(V& F) {
                    F(UnkT00);
                    F.Pad(12);
                }
            };

            /// @brief An orange developer message.
            struct Message : RegionImpl<Message, 9, TypeDataPresence::Always> {
                /// @brief ID of the message's text in the FMGs.
                int16_t MessageID = -1;
                /// @brief Unknown.
                ///
                /// Always 0 or 2.
                int16_t UnkT02 = 0;
                /// @brief Whether the message requires Seek Guidance to appear.
                bool Hidden = false;
                Message() { Name = "Region: Message"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
                template<typename V>
                void Fields(V& F) {
                    F(MessageID);
                    F(UnkT02);
                    F.AsInt32(Hidden);
                }
            };

            /// @brief A point in a patrol route.
            struct PatrolRoute : RegionImpl<PatrolRoute, 11, TypeDataPresence::Never> {
                PatrolRoute() { Name = "Region: PatrolRoute"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
                template<typename V>
                void Fields(V&) {}
            };
            /// @brief Unknown.
            struct MovementPoint : RegionImpl<MovementPoint, 12, TypeDataPresence::Never> {
                MovementPoint() { Name = "Region: MovementPoint"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
                template<typename V>
                void Fields(V&) {}
            };
            /// @brief Seems to be used for moving enemies around.
            struct WarpPoint : RegionImpl<WarpPoint, 13, TypeDataPresence::Never> {
                WarpPoint() { Name = "Region: WarpPoint"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
                template<typename V>
                void Fields(V&) {}
            };
            /// @brief Triggers an enemy when entered.
            struct ActivationArea : RegionImpl<ActivationArea, 14, TypeDataPresence::Never> {
                ActivationArea() { Name = "Region: ActivationArea"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
                template<typename V>
                void Fields(V&) {}
            };
            /// @brief Any kind of region for use with event scripts.
            struct Event : RegionImpl<Event, 15, TypeDataPresence::Never> {
                Event() { Name = "Region: Event"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
                template<typename V>
                void Fields(V&) {}
            };
            /// @brief Unknown; only used 3 times in Catacombs.
            struct Logic : RegionImpl<Logic, 0, TypeDataPresence::Never> {
                Logic() { Name = "Region: Logic"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
                template<typename V>
                void Fields(V&) {}
            };

            /// @brief Unknown.
            struct EnvironmentMapEffectBox : RegionImpl<EnvironmentMapEffectBox, 17, TypeDataPresence::Always> {
                /// @brief Unknown.
                float UnkT00 = 0;
                /// @brief Unknown.
                float Compare = 0;
                /// @brief Unknown.
                bool UnkT08 = false;
                /// @brief Unknown.
                uint8_t UnkT09 = 0;
                /// @brief Unknown.
                int16_t UnkT0A = 0;
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
                    F.Pad(32);
                }
            };

            /// @brief Unknown; each WindSFX has a reference to a WindArea.
            struct WindArea : RegionImpl<WindArea, 18, TypeDataPresence::Never> {
                WindArea() { Name = "Region: WindArea"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
                template<typename V>
                void Fields(V&) {}
            };

            /// @brief Muffles environmental sound while inside it.
            struct MufflingBox : RegionImpl<MufflingBox, 20, TypeDataPresence::AlwaysNull> {
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

            /// @brief A region leading into a MufflingBox.
            struct MufflingPortal : RegionImpl<MufflingPortal, 21, TypeDataPresence::AlwaysNull> {
                /// @brief Unknown.
                int32_t UnkT00 = 0;
                MufflingPortal() { Name = "Region: MufflingPortal"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
                template<typename V>
                void Fields(V& F) {
                    F(UnkT00);
                    F.Pad(12);
                }
            };

            /// @brief Most likely an unused region.
            struct Other : RegionImpl<Other, 0xFFFFFFFF, TypeDataPresence::Never> {
                Other() { Name = "Region: Other"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
                template<typename V>
                void Fields(V&) {}
            };

            /// @brief InvasionPoints in the MSB.
            std::vector<InvasionPoint> InvasionPoints;
            /// @brief EnvironmentMapPoints in the MSB.
            std::vector<EnvironmentMapPoint> EnvironmentMapPoints;
            /// @brief Sound regions in the MSB.
            std::vector<Sound> Sounds;
            /// @brief The SFX entries.
            std::vector<SFX> SFXs;
            /// @brief The WindSFX entries.
            std::vector<WindSFX> WindSFXs;
            /// @brief SpawnPoints in the MSB.
            std::vector<SpawnPoint> SpawnPoints;
            /// @brief Messages in the MSB.
            std::vector<Message> Messages;
            /// @brief PatrolRoute points in the MSB.
            std::vector<PatrolRoute> PatrolRoutes;
            /// @brief MovementPoints in the MSB.
            std::vector<MovementPoint> MovementPoints;
            /// @brief WarpPoints in the MSB.
            std::vector<WarpPoint> WarpPoints;
            /// @brief ActivationAreas in the MSB.
            std::vector<ActivationArea> ActivationAreas;
            /// @brief Event regions in the MSB.
            std::vector<Event> Events;
            /// @brief The Logic entries.
            std::vector<Logic> Logics;
            /// @brief EnvironmentMapEffectBoxes in the MSB.
            std::vector<EnvironmentMapEffectBox> EnvironmentMapEffectBoxes;
            /// @brief WindAreas in the MSB.
            std::vector<WindArea> WindAreas;
            /// @brief MufflingBoxes in the MSB.
            std::vector<MufflingBox> MufflingBoxes;
            /// @brief MufflingPortals in the MSB.
            std::vector<MufflingPortal> MufflingPortals;
            /// @brief Most likely a dumping ground for unused regions.
            std::vector<Other> Others;

            /// @brief The version number this param always has in this game's files.
            static constexpr int32_t ParamVersion = 3;
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
                Visit(Messages);
                Visit(PatrolRoutes);
                Visit(MovementPoints);
                Visit(WarpPoints);
                Visit(ActivationAreas);
                Visit(Events);
                Visit(Logics);
                Visit(EnvironmentMapEffectBoxes);
                Visit(WindAreas);
                Visit(MufflingBoxes);
                Visit(MufflingPortals);
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
                Visit(Messages);
                Visit(PatrolRoutes);
                Visit(MovementPoints);
                Visit(WarpPoints);
                Visit(ActivationAreas);
                Visit(Events);
                Visit(Logics);
                Visit(EnvironmentMapEffectBoxes);
                Visit(WindAreas);
                Visit(MufflingBoxes);
                Visit(MufflingPortals);
                Visit(Others);
            }
        };
#pragma endregion

#pragma region Routes and layers
        /// @brief Unknown.
        struct SOULS_API Route : Entry {
            /// @brief Unknown.
            int32_t Unk08 = 0;
            /// @brief Unknown.
            int32_t Unk0C = 0;
            /// @brief The type number entries of this type have in the file.
            static constexpr uint32_t TypeId = 0;
            /// @brief The type number of this entry as stored in the file.
            uint32_t Type() const { return 0; }
            Route() { Name = "XX-XX"; }
            /// @brief Reads the entry from its position in the file.
            void Read(BinaryReader& Reader);
            /// @brief Writes the entry; ID is its position among the entries of the same type.
            void Write(BinaryWriter& Writer, int32_t ID);
        };

        /// @brief A section containing routes.
        ///
        /// Purpose unknown.
        struct RouteParam : Msb::Detail::TypedLists<RouteParam, Route> {
            /// @brief The routes in this section.
            std::vector<Route> Routes;
            /// @brief The version number this param always has in this game's files.
            static constexpr int32_t ParamVersion = 3;
            /// @brief The param's name in the file.
            static constexpr const char* ParamName = "ROUTE_PARAM_ST";
            /// @brief Calls Visit with each per-type list, in the order the entries are written.
            template<typename F>
            void ForEachList(F&& Visit) {
                Visit(Routes);
            }
            /// @brief Calls Visit with each per-type list, in the order the entries are written.
            template<typename F>
            void ForEachList(F&& Visit) const {
                Visit(Routes);
            }
        };

        /// @brief Unknown; seems to have been related to ceremonies but probably unused in release.
        struct SOULS_API Layer : Entry {
            /// @brief Unknown; usually just counts up from 0.
            int32_t Unk08 = 0;
            /// @brief Unknown; usually just counts up from 0.
            int32_t Unk0C = 0;
            /// @brief Unknown; seems to always be 0.
            int32_t Unk10 = 0;
            /// @brief The type number entries of this type have in the file.
            static constexpr uint32_t TypeId = 0;
            /// @brief The type number of this entry as stored in the file.
            uint32_t Type() const { return 0; }
            Layer() { Name = "Layer"; }
            /// @brief Reads the entry from its position in the file.
            void Read(BinaryReader& Reader);
            /// @brief Writes the entry; ID is its position among the entries of the same type.
            void Write(BinaryWriter& Writer, int32_t ID);
        };

        /// @brief A section containing layers, which probably don't actually do anything.
        struct LayerParam : Msb::Detail::TypedLists<LayerParam, Layer> {
            /// @brief The layers in this section.
            std::vector<Layer> Layers;
            /// @brief The version number this param always has in this game's files.
            static constexpr int32_t ParamVersion = 3;
            /// @brief The param's name in the file.
            static constexpr const char* ParamName = "LAYER_PARAM_ST";
            /// @brief Calls Visit with each per-type list, in the order the entries are written.
            template<typename F>
            void ForEachList(F&& Visit) {
                Visit(Layers);
            }
            /// @brief Calls Visit with each per-type list, in the order the entries are written.
            template<typename F>
            void ForEachList(F&& Visit) const {
                Visit(Layers);
            }
        };
#pragma endregion

#pragma region Parts
        /// @brief Gparam value IDs for various part types.
        struct GparamConfig {
            /// @brief ID of the value set from LightSet ParamEditor to use.
            int32_t LightSetID = 0;
            /// @brief ID of the value set from FogParamEditor to use.
            int32_t FogParamID = 0;
            /// @brief ID of the value set from LightScattering ParamEditor to use.
            int32_t LightScatteringID = 0;
            /// @brief ID of the value set from Env Map Editor to use.
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

        /// @brief Unknown.
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

        /// @brief Any instance of some "thing" in a map.
        struct SOULS_API Part : Entry {
            /// @brief Unknown network path to a .sib file.
            std::string SibPath;
            /// @brief The name of this part's model.
            Msb::Ref<ModelList> ModelName;
            /// @brief The center of the part.
            Vector3 Position;
            /// @brief The rotation of the part.
            Vector3 Rotation;
            /// @brief The scale of the part, which only really works right for map pieces.
            Vector3 Scale {1.f, 1.f, 1.f};
            /// @brief A bitmask that determines which ceremonies the part appears in.
            uint32_t MapStudioLayer = 0;
            /// @brief Unknown.
            std::array<uint32_t, 8> DrawGroups {
              0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
            /// @brief Unknown.
            std::array<uint32_t, 8> DispGroups {
              0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
            /// @brief Unknown.
            std::array<uint32_t, 8> BackreadGroups {
              0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
            /// @brief Used to identify the part in event scripts.
            int32_t EntityID = -1;
            /// @brief Used to identify multiple parts with the same ID in event scripts.
            std::array<int32_t, 8> EntityGroups {-1, -1, -1, -1, -1, -1, -1, -1};
            /// @brief Unknown.
            int8_t UnkE04 = 0;
            /// @brief Unknown.
            int8_t UnkE05 = 0;
            /// @brief Unknown.
            int8_t LanternID = 0;
            /// @brief Unknown.
            int8_t LodParamID = 0;
            /// @brief Unknown.
            int8_t UnkE0E = 0;
            /// @brief Unknown.
            bool PointLightShadowSource = false;
            /// @brief Unknown.
            bool ShadowSource = false;
            /// @brief Unknown.
            bool ShadowDest = false;
            /// @brief Unknown.
            bool IsShadowOnly = false;
            /// @brief Unknown.
            bool DrawByReflectCam = false;
            /// @brief Unknown.
            bool DrawOnlyReflectCam = false;
            /// @brief Unknown.
            bool UseDepthBiasFloat = false;
            /// @brief Unknown.
            bool DisablePointLightEffect = false;
            /// @brief Unknown.
            int32_t UnkE18 = 0;

            /// @brief The type number of this entry as stored in the file.
            virtual uint32_t Type() const = 0;
            /// @brief Whether this part type has a gparam config block.
            virtual bool HasGparamConfig() const = 0;
            /// @brief Whether this part type has a scene gparam config block.
            virtual bool HasSceneGparamConfig() const = 0;
            /// @brief Reads the entry from its position in the file.
            void Read(BinaryReader& Reader);
            /// @brief Writes the entry; ID is its position among the entries of the same type.
            void Write(BinaryWriter& Writer, int32_t ID);
            /// @brief Reads the type-specific data.
            virtual void ReadTypeData(BinaryReader& Reader) = 0;
            /// @brief Writes the type-specific data.
            virtual void WriteTypeData(BinaryWriter& Writer) = 0;
            /// @brief Reads the gparam config block.
            virtual void ReadGparamConfig(BinaryReader&) {}
            /// @brief Writes the gparam config block.
            virtual void WriteGparamConfig(BinaryWriter&) {}
            /// @brief Reads the scene gparam config block.
            virtual void ReadSceneGparamConfig(BinaryReader&) {}
            /// @brief Writes the scene gparam config block.
            virtual void WriteSceneGparamConfig(BinaryWriter&) {}
            /// @brief Turns the indices read from the file into the names of the entries they refer to.
            virtual void GetNames(const Entries& Lists);
            /// @brief Turns the names of referenced entries into the indices written to the file.
            virtual void GetIndices(const Entries& Lists);
        };

        /// @brief Implements the type data functions of a part from its Fields function.
        ///
        /// A part type that has gparam configs provides Gparam / SceneGparam members and sets the flags.
        template<typename D, uint32_t Id, bool WithGparam, bool WithSceneGparam, typename B = Part>
        struct PartImpl : B {
            /// @brief The type number entries of this type have in the file.
            static constexpr uint32_t TypeId = Id;
            /// @brief The type number of this entry as stored in the file.
            uint32_t Type() const override { return Id; }
            /// @brief Whether this part type has a gparam config block.
            bool HasGparamConfig() const override { return WithGparam; }
            /// @brief Whether this part type has a scene gparam config block.
            bool HasSceneGparamConfig() const override { return WithSceneGparam; }
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
            /// @brief Reads the gparam config block.
            void ReadGparamConfig(BinaryReader& Reader) override {
                if constexpr (WithGparam) {
                    Msb::Detail::ReadVisitor V {Reader};
                    static_cast<D*>(this)->Gparam.Fields(V);
                }
            }
            /// @brief Writes the gparam config block.
            void WriteGparamConfig(BinaryWriter& Writer) override {
                if constexpr (WithGparam) {
                    Msb::Detail::WriteVisitor V {Writer};
                    static_cast<D*>(this)->Gparam.Fields(V);
                }
            }
            /// @brief Reads the scene gparam config block.
            void ReadSceneGparamConfig(BinaryReader& Reader) override {
                if constexpr (WithSceneGparam) {
                    Msb::Detail::ReadVisitor V {Reader};
                    static_cast<D*>(this)->SceneGparam.Fields(V);
                }
            }
            /// @brief Writes the scene gparam config block.
            void WriteSceneGparamConfig(BinaryWriter& Writer) override {
                if constexpr (WithSceneGparam) {
                    Msb::Detail::WriteVisitor V {Writer};
                    static_cast<D*>(this)->SceneGparam.Fields(V);
                }
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
            /// @brief Gparam IDs for this object.
            GparamConfig Gparam;
            /// @brief Unknown.
            Msb::Ref<PartList> CollisionName;
            /// @brief Unknown.
            uint8_t BreakTerm = 0;
            /// @brief Unknown.
            bool NetSyncType = false;
            /// @brief Unknown.
            bool CollisionFilter = false;
            /// @brief Unknown.
            bool SetMainObjStructureBooleans = false;
            /// @brief Automatically playing animations; only the first is actually used, according to Pav.
            std::array<int16_t, 4> AnimIDs {-1, -1, -1, -1};
            /// @brief Value added to the base ModelSfxParam ID; only the first is actually used, according to Pav.
            std::array<int16_t, 4> ModelSfxParamRelativeIDs {-1, -1, -1, -1};

            ObjectBase() { Name = "oXXXXXX_XXXX"; }
            /// @brief Visits the fields that objects and dummy objects share.
            template<typename V>
            void ObjectFields(V& F) {
                F.Pad(8);
                F(CollisionName);
                F(BreakTerm);
                F(NetSyncType);
                F(CollisionFilter);
                F(SetMainObjStructureBooleans);
                F(AnimIDs);
                F(ModelSfxParamRelativeIDs);
            }
        };

        /// @brief Common base data for enemies and dummy enemies.
        struct EnemyBase : Part {
            /// @brief Gparam IDs for this enemy.
            GparamConfig Gparam;
            /// @brief Unknown.
            Msb::Ref<PartList> CollisionName;
            /// @brief Controls enemy AI, stats, speech and equipment.
            int32_t ThinkParamID = 0;
            /// @brief Controls enemy stats.
            int32_t NPCParamID = 0;
            /// @brief Controls enemy speech.
            int32_t TalkID = 0;
            /// @brief Controls enemy equipment.
            int32_t CharaInitID = 0;
            /// @brief Unknown.
            uint8_t PointMoveType = 0;
            /// @brief Unknown.
            int16_t PlatoonID = 0;
            /// @brief Walk route followed by this enemy.
            Msb::Ref<PatrolInfoList, int16_t> WalkRouteName;
            /// @brief Unknown.
            int32_t BackupEventAnimID = 0;
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
                F(TalkID);
                F(PointMoveType);
                F.template Const<uint8_t>(0);
                F(PlatoonID);
                F(CharaInitID);
                F(CollisionName);
                F(WalkRouteName);
                F.template Const<int16_t>(0);
                F.template Const<int32_t>(0);
                F.template Const<int32_t>(-1);
                F.template Const<int32_t>(-1);
                F.template Const<int32_t>(-1);
                F.template Const<int32_t>(-1);
                F(BackupEventAnimID);
                F.template Const<int32_t>(-1);
                F.Pad(56);
                F(UnkT78);
                F.Pad(8);
                F(UnkT84);
                for (int I = 0; I < 5; ++I) {
                    F.template Const<int32_t>(-1);
                    F.template Const<int16_t>(-1);
                    F.template Const<int16_t>(0xA);
                }
                F.Pad(16);
            }
        };

        /// @brief Instances of various "things" in this MSB.
        struct PartsParam : Msb::Detail::TypedLists<PartsParam, Part> {
            /// @brief A static model making up the map.
            struct MapPiece : PartImpl<MapPiece, 0, true, false> {
                /// @brief Gparam IDs for this map piece.
                GparamConfig Gparam;
                MapPiece() { Name = "mXXXXXX_XXXX"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
                template<typename V>
                void Fields(V& F) {
                    F.Pad(8);
                }
            };

            /// @brief Any dynamic object such as elevators, crates, ladders, etc.
            struct Object : PartImpl<Object, 1, true, false, ObjectBase> {
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
                template<typename V>
                void Fields(V& F) {
                    ObjectFields(F);
                }
            };

            /// @brief Any non-player character, not necessarily hostile.
            struct Enemy : PartImpl<Enemy, 2, true, false, EnemyBase> {
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
                template<typename V>
                void Fields(V& F) {
                    EnemyFields(F);
                }
            };

            /// @brief A player spawn point.
            struct Player : PartImpl<Player, 4, false, false> {
                Player() { Name = "c0000_XXXX"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
                template<typename V>
                void Fields(V& F) {
                    F.Pad(16);
                }
            };

            /// @brief An invisible collision mesh, also used for death planes.
            struct Collision : PartImpl<Collision, 5, true, true> {
                /// @brief Amount of reverb to apply to sounds.
                enum class SoundSpace : uint8_t {
                    NoReverb          = 0,
                    SmallReverbA      = 1,
                    SmallReverbB      = 2,
                    MiddleReverbA     = 3,
                    MiddleReverbB     = 4,
                    LargeReverbA      = 5,
                    LargeReverbB      = 6,
                    ExtraLargeReverbA = 7,
                    ExtraLargeReverbB = 8,
                };
                /// @brief Unknown.
                enum class MapVisiblity : uint8_t {
                    Good      = 0,
                    Dark      = 1,
                    PitchDark = 2,
                };
                /// @brief Gparam IDs for this collision.
                GparamConfig Gparam;
                /// @brief Unknown.
                SceneGparamConfig SceneGparam;
                /// @brief Unknown.
                uint8_t HitFilterID = 0;
                /// @brief Modifies sounds while the player is touching this collision.
                SoundSpace SoundSpaceType = SoundSpace::NoReverb;
                /// @brief Unknown.
                int16_t EnvLightMapSpotIndex = 0;
                /// @brief Unknown.
                float ReflectPlaneHeight = 0;
                /// @brief Unknown.
                int16_t MapNameID = -1;
                /// @brief Unknown.
                bool DisableStart = false;
                /// @brief Disables a bonfire with this entity ID when an enemy is touching this collision.
                int32_t DisableBonfireEntityID = -1;
                /// @brief Unknown.
                int32_t PlayRegionID = -1;
                /// @brief Unknown.
                int16_t LockCamID1 = 0;
                /// @brief Unknown.
                int16_t LockCamID2 = 0;
                /// @brief Unknown.
                ///
                /// Always refers to another collision part.
                Msb::Ref<PartList> UnkHitName;
                /// @brief ID in MapMimicryEstablishmentParam.
                int32_t ChameleonParamID = 0;
                /// @brief Unknown.
                uint8_t UnkT34 = 0;
                /// @brief Unknown.
                uint8_t UnkT35 = 0;
                /// @brief Unknown.
                uint8_t UnkT36 = 0;
                /// @brief Unknown.
                MapVisiblity MapVisType = MapVisiblity::Good;

                Collision() { Name = "hXXXXXX"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
                template<typename V>
                void Fields(V& F) {
                    F(HitFilterID);
                    F(SoundSpaceType);
                    F(EnvLightMapSpotIndex);
                    F(ReflectPlaneHeight);
                    F.Pad(16);
                    F.template Const<int32_t>(-1);
                    F.template Const<int32_t>(-1);
                    F.template Const<int32_t>(-1);
                    F(MapNameID);
                    F(DisableStart);
                    F.template Const<uint8_t>(0);
                    F(DisableBonfireEntityID);
                    F(ChameleonParamID);
                    F(UnkHitName);
                    F(UnkT34);
                    F(UnkT35);
                    F(UnkT36);
                    F(MapVisType);
                    F(PlayRegionID);
                    F(LockCamID1);
                    F(LockCamID2);
                    F.Pad(16);
                }
            };

            /// @brief An object that is either unused, or used for a cutscene.
            struct DummyObject : PartImpl<DummyObject, 9, true, false, ObjectBase> {
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
                template<typename V>
                void Fields(V& F) {
                    ObjectFields(F);
                }
            };

            /// @brief An enemy that is either unused, or used for a cutscene.
            struct DummyEnemy : PartImpl<DummyEnemy, 10, true, false, EnemyBase> {
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves
                /// references.
                template<typename V>
                void Fields(V& F) {
                    EnemyFields(F);
                }
            };

            /// @brief Determines which collision parts load other maps.
            struct ConnectCollision : PartImpl<ConnectCollision, 11, false, false> {
                /// @brief The name of the associated collision part.
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

            /// @brief Map pieces in the MSB.
            std::vector<MapPiece> MapPieces;
            /// @brief Objects in the MSB.
            std::vector<Object> Objects;
            /// @brief Enemies in the MSB.
            std::vector<Enemy> Enemies;
            /// @brief Players in the MSB.
            std::vector<Player> Players;
            /// @brief Collisions in the MSB.
            std::vector<Collision> Collisions;
            /// @brief Dummy objects in the MSB.
            std::vector<DummyObject> DummyObjects;
            /// @brief Dummy enemies in the MSB.
            std::vector<DummyEnemy> DummyEnemies;
            /// @brief Connect collisions in the MSB.
            std::vector<ConnectCollision> ConnectCollisions;

            /// @brief The version number this param always has in this game's files.
            static constexpr int32_t ParamVersion = 3;
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

#pragma region Parts poses
        /// @brief A set of bone transforms to pose an individual part in the map.
        struct PartsPose {
            /// @brief A transform for one bone in a model.
            struct Bone {
                /// @brief The name of the bone to transform.
                std::string Name  = "Master";
                int32_t NameIndex = 0;  ///< only meaningful while reading and writing
                /// @brief Translation of the bone.
                Vector3 Translation;
                /// @brief Rotation of the bone.
                Vector3 Rotation;
                /// @brief Scale of the bone.
                Vector3 Scale {1.f, 1.f, 1.f};
            };

            /// @brief The name of the part to pose.
            Msb::Ref<PartList, int16_t> PartName;
            /// @brief Transforms for each bone.
            std::vector<Bone> Bones;
        };
#pragma endregion

        /// @brief Models in this MSB.
        ModelParam Models;
        /// @brief Events in this MSB.
        EventParam Events;
        /// @brief Regions in this MSB.
        PointParam Regions;
        /// @brief Routes in this MSB.
        std::vector<Route> Routes;
        /// @brief Layers in this MSB.
        std::vector<Layer> Layers;
        /// @brief Parts in this MSB.
        PartsParam Parts;
        /// @brief Fixed poses for parts in this MSB.
        std::vector<PartsPose> PartsPoses;

    protected:
        bool IsImpl(BinaryReader& Reader) override;
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
