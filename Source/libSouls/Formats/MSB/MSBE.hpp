/// @file
/// @author Jake Rieger
/// @date 10/11/2026

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

    /// @brief MSBE: the map layout file of Elden Ring.
    ///
    /// Extension: .msb
    ///
    /// An MSBE has params of models (the model files parts can use), events, regions, routes, layers (unused) and
    /// parts. Entries refer to each other by name.
    class SOULS_API MSBE : public SoulsFile<MSBE> {
    public:
        /// @brief Tag for references to models.
        using ModelList     = Msb::ModelList;
        /// @brief Tag for references to events.
        using EventList     = Msb::EventList;
        /// @brief Tag for references to regions.
        using RegionList    = Msb::RegionList;
        /// @brief Tag for references to parts.
        using PartList      = Msb::PartList;
        /// @brief Tag for references to collision parts only.
        using CollisionList = Msb::CollisionList;
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
                } else {
                    return PatrolInfos;
                }
            }
        };

        /// @brief Implements the type-specific data functions of an entry from D's Fields function.
        template<typename D, typename B, bool WithData>
        struct TypeDataOf : B {
            /// @brief Reads the type-specific data.
            void ReadTypeData(BinaryReader& Reader) override {
                if constexpr (WithData) {
                    Msb::Detail::ReadVisitor V{Reader};
                    static_cast<D*>(this)->Fields(V);
                }
            }
            /// @brief Writes the type-specific data.
            void WriteTypeData(BinaryWriter& Writer) override {
                if constexpr (WithData) {
                    Msb::Detail::WriteVisitor V{Writer};
                    static_cast<D*>(this)->Fields(V);
                }
            }
            /// @brief Turns the indices read from the file into the names of the entries they refer to.
            void GetNames(const Entries& Lists) override {
                B::GetNames(Lists);
                if constexpr (WithData) {
                    Msb::Detail::NameVisitor<Entries> V{Lists};
                    static_cast<D*>(this)->Fields(V);
                }
            }
            /// @brief Turns the names of referenced entries into the indices written to the file.
            void GetIndices(const Entries& Lists) override {
                B::GetIndices(Lists);
                if constexpr (WithData) {
                    Msb::Detail::IndexVisitor<Entries> V{Lists};
                    static_cast<D*>(this)->Fields(V);
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
            int32_t Unk1C         = 0;

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
            /// @brief A model for fixed terrain or scenery.
            struct MapPiece : ModelOf<0> {
                MapPiece() { Name = "mXXXXXX"; }
            };
            /// @brief A model for a non-player entity.
            struct Enemy : ModelOf<2> {
                Enemy() { Name = "cXXXX"; }
            };
            /// @brief A model for a player spawn point?
            struct Player : ModelOf<4> {
                Player() { Name = "c0000"; }
            };
            /// @brief A model for collision physics.
            struct Collision : ModelOf<5> {
                Collision() { Name = "hXXXXXX"; }
            };
            /// @brief A model for an asset (Elden Ring's successor to objects).
            struct Asset : ModelOf<10> {
                Asset() { Name = "AEGxxx_xxx"; }
            };

            /// @brief Unknown; probably some kind of version number.
            int32_t Version = 73;
            /// @brief Models for fixed terrain and scenery.
            std::vector<MapPiece> MapPieces;
            /// @brief Models for non-player entities.
            std::vector<Enemy> Enemies;
            /// @brief Models for player spawn points, I think.
            std::vector<Player> Players;
            /// @brief Models for physics collision.
            std::vector<Collision> Collisions;
            /// @brief Models for assets.
            std::vector<Asset> Assets;

            /// @brief The param's name in the file.
            static constexpr const char* ParamName = "MODEL_PARAM_ST";

            /// @brief Calls Visit with each per-type list, in the order the entries are written.
            template<typename F>
            void ForEachList(F&& Visit) {
                Visit(MapPieces);
                Visit(Enemies);
                Visit(Players);
                Visit(Collisions);
                Visit(Assets);
            }
            /// @brief Calls Visit with each per-type list, in the order the entries are written.
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
        /// @brief A dynamic or interactive system.
        struct SOULS_API Event : Entry {
            /// @brief Unknown.
            int32_t EventID = -1;
            /// @brief Unknown.
            Msb::Ref<PartList> PartName;
            /// @brief Unknown.
            Msb::Ref<RegionList> RegionName;
            /// @brief Identifies the event in event scripts.
            uint32_t EntityID = 0;
            /// @brief Unknown.
            int32_t Unk14     = 0;
            /// @brief Unknown.
            uint8_t UnkE0C    = 0;
            /// @brief Unknown.
            int32_t MapID = 0;
            /// @brief Unknown.
            int32_t UnkS04 = 0;
            /// @brief Unknown.
            int32_t UnkS08 = 0;
            /// @brief Unknown.
            int32_t UnkS0C = 0;
            /// @brief The ID the entry had in the file, written back unchanged (some events' IDs follow no pattern); -1
            ///        for entries made from scratch, which are numbered by their position.
            int32_t FileID = -1;

            /// @brief The type number of this entry as stored in the file.
            virtual uint32_t Type() const    = 0;
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
        struct EventImpl : TypeDataOf<D, Event, WithData> {
            /// @brief The type number entries of this type have in the file.
            static constexpr uint32_t TypeId = Id;
            /// @brief The type number of this entry as stored in the file.
            uint32_t Type() const override { return Id; }
            /// @brief Whether entries of this type have type-specific data.
            bool HasTypeData() const override { return WithData; }
        };

        /// @brief Dynamic or interactive systems such as item pickups, levers, enemy spawners, etc.
        struct EventParam : Msb::Detail::TypedLists<EventParam, Event> {
            /// @brief An item pickup in the open or inside a container.
            struct Treasure : EventImpl<Treasure, 4> {
                /// @brief The part that the treasure is attached to.
                Msb::Ref<PartList> TreasurePartName;
                /// @brief The item lot to be given (ItemLotParam_map).
                int32_t ItemLotID = -1;
                /// @brief If not -1, uses an entry from ActionButtonParam for the pickup prompt.
                int32_t ActionButtonID = -1;
                /// @brief Animation to play when taking this treasure.
                int32_t PickupAnimID = -1;
                /// @brief Changes the text of the pickup prompt.
                uint8_t InChest = 0;
                /// @brief Whether the treasure should be hidden by default.
                uint8_t StartDisabled = 0;
                Treasure() { Name = "Event: Treasure"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
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
                uint8_t MaxNum   = 0;
                /// @brief Unknown.
                int8_t GenType   = 0;
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
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
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
                uint32_t ObjActEntityID = 0;
                /// @brief The part to be interacted with.
                Msb::Ref<PartList> ObjActPartName;
                /// @brief A row in ObjActParam.
                int32_t ObjActID     = -1;
                /// @brief Unknown.
                uint8_t StateType    = 0;
                /// @brief Unknown.
                uint32_t EventFlagID = 0;
                ObjAct() { Name = "Event: ObjAct"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
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

            /// @brief Unknown.
            struct Navmesh : EventImpl<Navmesh, 10> {
                /// @brief Unknown.
                Msb::Ref<RegionList> NavmeshRegionName;
                Navmesh() { Name = "Event: Navmesh"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
                template<typename V>
                void Fields(V& F) {
                    F(NavmeshRegionName);
                    F.Pad(12);
                }
            };

            /// @brief A fake multiplayer interaction where the player goes to an NPC's world.
            struct PseudoMultiplayer : EventImpl<PseudoMultiplayer, 12> {
                /// @brief The NPC whose world you're entering.
                uint32_t HostEntityID = 0;
                /// @brief Set when inside the event's region, unset when outside it.
                uint32_t EventFlagID = 0;
                /// @brief ID of a goods item that is used to trigger the event.
                int32_t ActivateGoodsID = -1;
                /// @brief Unknown; possibly a sound ID.
                int32_t UnkT0C = -1;
                /// @brief Unknown; possibly a map event ID.
                int32_t UnkT10 = -1;
                /// @brief Unknown; possibly flags.
                int32_t UnkT14 = 0;
                /// @brief Unknown.
                int32_t UnkT18 = 0;
                /// @brief Unknown.
                int32_t UnkT1C = 0;
                PseudoMultiplayer() { Name = "Event: PseudoMultiplayer"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
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

            /// @brief Unknown.
            struct PlatoonInfo : EventImpl<PlatoonInfo, 15> {
                /// @brief Unknown.
                int32_t PlatoonIDScriptActivate = 0;
                /// @brief Unknown.
                int32_t State                   = 0;
                /// @brief Unknown.
                std::array<Msb::Ref<PartList>, 32> GroupPartNames;
                PlatoonInfo() { Name = "Event: PlatoonInfo"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
                template<typename V>
                void Fields(V& F) {
                    F(PlatoonIDScriptActivate);
                    F(State);
                    F.Pad(8);
                    F(GroupPartNames);
                }
            };

            /// @brief A patrol route that enemies follow.
            struct PatrolInfo : EventImpl<PatrolInfo, 20> {
                /// @brief Determines patrol behavior: 0 returns to the first region on loop, 1 goes through the list
                ///        backwards on loop, etc.
                ///
                /// (PATROL_TYPE)
                uint8_t PatrolType = 0;
                /// @brief Unknown.
                std::array<Msb::Ref<RegionList, int16_t>, 64> WalkRegionNames;
                PatrolInfo() { Name = "Event: PatrolInfo"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
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

            /// @brief Unknown.
            struct Mount : EventImpl<Mount, 21> {
                /// @brief Unknown.
                Msb::Ref<PartList> RiderPartName;
                /// @brief Unknown.
                Msb::Ref<PartList> MountPartName;
                Mount() { Name = "Event: Mount"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
                template<typename V>
                void Fields(V& F) {
                    F(RiderPartName);
                    F(MountPartName);
                }
            };

            /// @brief Unknown.
            struct SignPool : EventImpl<SignPool, 23> {
                /// @brief Unknown.
                Msb::Ref<PartList> SignPartName;
                /// @brief A row in SignPuddleParam.
                int32_t SignPuddleParamID = 0;
                SignPool() { Name = "Event: SignPool"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
                template<typename V>
                void Fields(V& F) {
                    F(SignPartName);
                    F(SignPuddleParamID);
                    F.Pad(8);
                }
            };

            /// @brief A stake of Marika: where the player can choose to respawn.
            struct RetryPoint : EventImpl<RetryPoint, 24> {
                /// @brief The asset that represents the stake.
                Msb::Ref<PartList> RetryPartName;
                /// @brief Flag that must be set for the stake to be available.
                uint32_t TriggerEventFlagID = 0;
                /// @brief The distance at which the retry point is active (only used if no retry region is set).
                float RetryDistance = 0;
                /// @brief The region in which the retry point is active.
                Msb::Ref<RegionList, int16_t> RetryRegionName;
                RetryPoint() { Name = "Event: RetryPoint"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
                template<typename V>
                void Fields(V& F) {
                    F(RetryPartName);
                    F(TriggerEventFlagID);
                    F(RetryDistance);
                    F(RetryRegionName);
                    F.template Const<int16_t>(0);
                }
            };

            /// @brief Unknown.
            struct AreaTeam : EventImpl<AreaTeam, 25> {
                /// @brief Presumably the entity ID of the team's leader.
                int32_t EntityIDLeader = 0;
                /// @brief Unknown.
                int32_t EUnk04 = 0;
                /// @brief Unknown.
                int32_t EUnk08 = 0;
                /// @brief Unknown.
                int32_t EUnk0C = 0;
                /// @brief Presumably the region of the team's leader.
                int32_t RegionIDLeader = 0;
                /// @brief Presumably the region of the team's first guest.
                int32_t RegionIDGuest1 = 0;
                /// @brief Presumably the region of the team's second guest.
                int32_t RegionIDGuest2 = 0;
                /// @brief Unknown.
                int32_t EUnk1C = 0;
                /// @brief Unknown.
                int32_t EUnk20 = 0;
                /// @brief Unknown.
                int32_t EUnk24 = 0;
                /// @brief Unknown.
                int32_t EUnk28 = 0;
                AreaTeam() { Name = "Event: AreaTeam"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
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

            /// @brief Unknown.
            struct Other : EventImpl<Other, 0xFFFFFFFF, false> {
                Other() { Name = "Event: Other"; }
            };

            /// @brief Unknown; probably some kind of version number.
            int32_t Version = 73;
            /// @brief Item pickups out in the open or inside containers.
            std::vector<Treasure> Treasures;
            /// @brief Enemy spawners.
            std::vector<Generator> Generators;
            /// @brief Interactive objects like levers and doors.
            std::vector<ObjAct> ObjActs;
            /// @brief Unknown.
            std::vector<Navmesh> Navmeshes;
            /// @brief Unknown.
            std::vector<PseudoMultiplayer> PseudoMultiplayers;
            /// @brief The PlatoonInfo entries.
            std::vector<PlatoonInfo> PlatoonInfos;
            /// @brief The PatrolInfo entries.
            std::vector<PatrolInfo> PatrolInfos;
            /// @brief Unknown.
            std::vector<Mount> Mounts;
            /// @brief Unknown.
            std::vector<SignPool> SignPools;
            /// @brief Unknown.
            std::vector<RetryPoint> RetryPoints;
            /// @brief Unknown.
            std::vector<AreaTeam> AreaTeams;
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
            /// @brief Calls Visit with each per-type list, in the order the entries are written.
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
        /// @brief A point or volume that triggers some sort of interaction.
        struct SOULS_API Region : Entry {
            /// @brief The shape of the region.
            Msb::Shape Shape = Msb::Shapes::Point{};
            /// @brief The location of the region.
            Vector3 Position;
            /// @brief The rotation of the region, in degrees.
            Vector3 Rotation;
            /// @brief Presumed ID for regions; unique per map, incremented per region.
            int32_t RegionID = 0;
            /// @brief Unknown.
            int32_t Unk40    = 0;
            /// @brief Controls whether the region is active in different ceremonies.
            uint32_t MapStudioLayer = 0xFFFFFFFF;
            /// @brief Unknown.
            std::vector<int16_t> UnkA;
            /// @brief Unknown.
            std::vector<int16_t> UnkB;
            /// @brief If specified, the region is only active when the part is loaded.
            Msb::Ref<PartList> ActivationPartName;
            /// @brief Identifies the region in event scripts.
            uint32_t EntityID = 0;
            /// @brief Unknown.
            uint8_t UnkE08    = 0;
            /// @brief Unknown.
            int32_t MapID = 0;
            /// @brief Unknown.
            int32_t UnkS04 = 0;
            /// @brief Unknown.
            int32_t UnkS0C = 0;

            /// @brief The type number of this entry as stored in the file.
            virtual uint32_t Type() const    = 0;
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
        template<typename D, uint32_t Id, bool WithData = true>
        struct RegionImpl : TypeDataOf<D, Region, WithData> {
            /// @brief The type number entries of this type have in the file.
            static constexpr uint32_t TypeId = Id;
            /// @brief The type number of this entry as stored in the file.
            uint32_t Type() const override { return Id; }
            /// @brief Whether entries of this type have type-specific data.
            bool HasTypeData() const override { return WithData; }
        };

        /// @brief Points and volumes used to trigger various effects.
        struct PointParam : Msb::Detail::TypedLists<PointParam, Region> {
            /// @brief A point where a player can invade your world.
            struct InvasionPoint : RegionImpl<InvasionPoint, 1> {
                /// @brief Not sure what this does.
                int32_t Priority = 0;
                InvasionPoint() { Name = "Region: InvasionPoint"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
                template<typename V>
                void Fields(V& F) {
                    F(Priority);
                }
            };

            /// @brief Unknown.
            struct EnvironmentMapPoint : RegionImpl<EnvironmentMapPoint, 2> {
                /// @brief Unknown.
                float UnkT00   = 0;
                /// @brief Unknown.
                int32_t UnkT04 = 0;
                /// @brief Unknown.
                bool UnkT0D = false;
                /// @brief Unknown.
                bool UnkT0E = false;
                /// @brief Unknown.
                bool UnkT0F = false;
                /// @brief Unknown.
                float UnkT10 = 0;
                /// @brief Unknown.
                float UnkT14 = 0;
                /// @brief Unknown.
                std::array<uint8_t, 4> UnkMapID{};
                /// @brief Unknown.
                int32_t UnkT20 = 0;
                /// @brief Unknown.
                int32_t UnkT24 = 0;
                /// @brief Unknown.
                int32_t UnkT28 = 0;
                /// @brief Unknown.
                uint8_t UnkT2C = 0;
                /// @brief Unknown.
                uint8_t UnkT2D = 0;
                EnvironmentMapPoint() { Name = "Region: EnvironmentMapPoint"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
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

            /// @brief An area where a sound plays.
            struct Sound : RegionImpl<Sound, 4> {
                /// @brief The category of the sound.
                int32_t SoundType = 0;
                /// @brief The ID of the sound.
                int32_t SoundID = 0;
                /// @brief References to other regions used to build a composite shape.
                std::array<Msb::Ref<RegionList>, 16> ChildRegionNames;
                /// @brief Unknown.
                bool UnkT49 = false;
                Sound() { Name = "Region: Sound"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
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

            /// @brief A point where a particle effect can play.
            struct SFX : RegionImpl<SFX, 5> {
                /// @brief The ID of the particle effect FFX.
                int32_t EffectID = 0;
                /// @brief If not 0, the effect is off until enabled by event scripts.
                int32_t StartDisabled = 0;
                SFX() { Name = "Region: SFX"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
                template<typename V>
                void Fields(V& F) {
                    F(EffectID);
                    F(StartDisabled);
                }
            };

            /// @brief Unknown.
            struct WindSFX : RegionImpl<WindSFX, 6> {
                /// @brief ID of the effect FFX.
                int32_t EffectID = 0;
                /// @brief Reference to a WindArea region.
                Msb::Ref<RegionList> WindAreaName;
                /// @brief Unknown.
                float UnkT08 = 0;
                WindSFX() { Name = "Region: WindSFX"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
                template<typename V>
                void Fields(V& F) {
                    F(EffectID);
                    F(WindAreaName);
                    F(UnkT08);
                }
            };

            /// @brief A point where the player can spawn into the map.
            struct SpawnPoint : RegionImpl<SpawnPoint, 8> {
                SpawnPoint() { Name = "Region: SpawnPoint"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
                template<typename V>
                void Fields(V& F) {
                    F.template Const<int32_t>(-1);
                    F.Pad(12);
                }
            };

            /// @brief A developer message.
            struct Message : RegionImpl<Message, 9> {
                /// @brief ID of the message's text in the FMGs.
                int16_t MessageID = -1;
                /// @brief Unknown; always 0 or 2.
                int16_t UnkT02 = 0;
                /// @brief Whether the message requires Seek Guidance to appear.
                bool Hidden            = false;
                /// @brief Unknown.
                int32_t UnkT08         = 0;
                /// @brief Unknown.
                int32_t MessageSfxID   = 0;
                /// @brief Event flag required to be on for the message to appear.
                uint32_t EnableEventFlagID = 0;
                /// @brief ID of a character to render along with the message.
                int32_t CharacterModelName = 0;
                /// @brief NpcParam ID to use when rendering a character with the message.
                int32_t NPCParamID = 0;
                /// @brief Animation ID to use when rendering a character with the message.
                int32_t AnimationID      = 0;
                /// @brief Unknown.
                int32_t CharaInitParamID = 0;
                Message() { Name = "Region: Message"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
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

            /// @brief Unknown.
            struct EnvironmentMapEffectBox : RegionImpl<EnvironmentMapEffectBox, 17> {
                /// @brief Distance from the camera required before enabling the envmap; 0 is always enabled.
                float EnableDist = 0;
                /// @brief Distance it takes for an envmap to fully transition into view.
                float TransitionDist = 0;
                /// @brief Unknown.
                uint8_t UnkT08 = 0;
                /// @brief Unknown.
                uint8_t UnkT09 = 0;
                /// @brief Unknown.
                int16_t UnkT0A = 0;
                /// @brief Strength of specular light in the region.
                float SpecularLightMult = 0;
                /// @brief Strength of direct light emitting from the EnvironmentMapPoint.
                float PointLightMult = 0;
                /// @brief Unknown.
                int16_t UnkT2C       = 0;
                /// @brief Affects lighting with other fields when true; possibly normalizes light when false.
                bool IsModifyLight = false;
                /// @brief Unknown.
                bool UnkT2F        = false;
                /// @brief Unknown.
                int16_t UnkT30     = 0;
                /// @brief Unknown.
                bool UnkT32 = false;
                /// @brief Unknown.
                bool UnkT33 = false;
                /// @brief Unknown.
                int16_t UnkT34 = 0;
                /// @brief Unknown.
                int16_t UnkT36 = 0;
                EnvironmentMapEffectBox() { Name = "Region: EnvironmentMapEffectBox"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
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

            /// @brief Unknown.
            struct WindArea : RegionImpl<WindArea, 18, false> {
                WindArea() { Name = "Region: WindArea"; }
            };

            /// @brief Used to align different maps.
            struct Connection : RegionImpl<Connection, 21> {
                /// @brief The map this connection targets.
                std::array<int8_t, 4> TargetMapID{};
                Connection() { Name = "Region: Connection"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
                template<typename V>
                void Fields(V& F) {
                    F(TargetMapID);
                    F.Pad(12);
                }
            };

            /// @brief Unknown.
            struct PatrolRoute22 : RegionImpl<PatrolRoute22, 22> {
                PatrolRoute22() { Name = "Region: PatrolRoute22"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
                template<typename V>
                void Fields(V& F) {
                    F.template Const<int32_t>(-1);
                    F.template Const<int32_t>(0);
                }
            };

            /// @brief Unknown.
            struct BuddySummonPoint : RegionImpl<BuddySummonPoint, 26> {
                BuddySummonPoint() { Name = "Region: BuddySummonPoint"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
                template<typename V>
                void Fields(V& F) {
                    F.Pad(16);
                }
            };

            /// @brief Unknown.
            struct DisableTumbleweed : RegionImpl<DisableTumbleweed, 27> {
                DisableTumbleweed() { Name = "Region: DisableTumbleweed"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
                template<typename V>
                void Fields(V& F) {
                    F.template Const<int32_t>(-1);
                    F.Pad(8);
                }
            };

            /// @brief An area where sound is muffled.
            struct MufflingBox : RegionImpl<MufflingBox, 28> {
                /// @brief Unknown.
                int32_t UnkT00 = 0;
                /// @brief Unknown.
                float UnkT24 = 0;
                /// @brief Unknown.
                float UnkT34 = 0;
                /// @brief Unknown.
                float UnkT3C = 0;
                /// @brief Unknown.
                float UnkT40 = 0;
                /// @brief Unknown.
                float UnkT44 = 0;
                MufflingBox() { Name = "Region: MufflingBox"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
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

            /// @brief An entrance to a muffling box.
            struct MufflingPortal : RegionImpl<MufflingPortal, 29> {
                /// @brief Unknown.
                int32_t UnkT00 = 0;
                MufflingPortal() { Name = "Region: MufflingPortal"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
                template<typename V>
                void Fields(V& F) {
                    F(UnkT00);
                    F.Pad(20);
                    F.template Const<int32_t>(32);
                    F.Pad(24);
                    F.template Const<int32_t>(-1);
                }
            };

            /// @brief Unknown.
            struct SoundRegion : RegionImpl<SoundRegion, 30> {
                /// @brief Unknown.
                uint8_t UnkT00 = 0;
                /// @brief Unknown.
                uint8_t UnkT01 = 0;
                /// @brief Unknown.
                uint8_t UnkT02 = 0;
                /// @brief Unknown.
                uint8_t UnkT03 = 0;
                /// @brief Unknown.
                int32_t UnkT04 = 0;
                /// @brief Unknown.
                int16_t UnkT08 = 0;
                /// @brief Unknown.
                int16_t UnkT0A = 0;
                /// @brief Unknown.
                uint8_t UnkT0C = 0;
                SoundRegion() { Name = "Region: SoundRegion"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
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

            /// @brief Unknown.
            struct MufflingPlane : RegionImpl<MufflingPlane, 31, false> {
                MufflingPlane() { Name = "Region: MufflingPlane"; }
            };

            /// @brief A point along an NPC patrol path.
            struct PatrolRoute : RegionImpl<PatrolRoute, 32> {
                /// @brief Unknown.
                int32_t UnkT00 = 0;
                PatrolRoute() { Name = "Region: PatrolRoute"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
                template<typename V>
                void Fields(V& F) {
                    F(UnkT00);
                }
            };

            /// @brief Unknown.
            struct MapPoint : RegionImpl<MapPoint, 33> {
                /// @brief A row in WorldMapPointParam.
                int32_t WorldMapPointParamID = 0;
                /// @brief Unknown.
                int32_t UnkT04               = 0;
                /// @brief Unknown.
                float UnkT08 = 0;
                /// @brief Unknown.
                float UnkT0C = 0;
                /// @brief Unknown.
                int32_t UnkT10 = 0;
                /// @brief Unknown.
                float UnkT14 = 0;
                /// @brief Unknown.
                float UnkT18 = 0;
                MapPoint() { Name = "Region: MapPoint"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
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

            /// @brief Unknown.
            struct WeatherOverride : RegionImpl<WeatherOverride, 35> {
                /// @brief A row in WeatherLotParam.
                int32_t WeatherLotParamID = 0;
                /// @brief Unknown.
                uint8_t UnkT08 = 0;
                /// @brief Unknown.
                uint8_t UnkT09 = 0;
                /// @brief Unknown.
                uint8_t UnkT0A = 0;
                /// @brief Unknown.
                uint8_t UnkT0B = 0;
                WeatherOverride() { Name = "Region: WeatherOverride"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
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

            /// @brief Unknown.
            struct AutoDrawGroupPoint : RegionImpl<AutoDrawGroupPoint, 36> {
                /// @brief Unknown.
                int32_t UnkT00 = 0;
                AutoDrawGroupPoint() { Name = "Region: AutoDrawGroupPoint"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
                template<typename V>
                void Fields(V& F) {
                    F(UnkT00);
                    F.Pad(28);
                }
            };

            /// @brief Unknown.
            struct GroupDefeatReward : RegionImpl<GroupDefeatReward, 37> {
                /// @brief Unknown.
                int32_t UnkT00 = 0;
                /// @brief Unknown.
                int32_t UnkT04 = 0;
                /// @brief Unknown.
                int32_t UnkT08 = 0;
                /// @brief Enemies to defeat to receive the reward.
                std::array<Msb::Ref<PartList>, 8> PartNames;
                /// @brief Unknown.
                int32_t UnkT34 = 0;
                /// @brief Unknown.
                int32_t UnkT38 = 0;
                /// @brief A row in HPEstusFlaskRecoveryParam / MPEstusFlaskRecoveryParam.
                int32_t EstusFlaskRecoveryID = 0;
                GroupDefeatReward() { Name = "Region: GroupDefeatReward"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
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

            /// @brief Unknown.
            struct MapPointDiscoveryOverride : RegionImpl<MapPointDiscoveryOverride, 38, false> {
                MapPointDiscoveryOverride() { Name = "Region: MapPointDiscoveryOverride"; }
            };
            /// @brief Unknown.
            struct MapPointParticipationOverride : RegionImpl<MapPointParticipationOverride, 39, false> {
                MapPointParticipationOverride() { Name = "Region: MapPointParticipationOverride"; }
            };

            /// @brief Unknown.
            struct Hitset : RegionImpl<Hitset, 40> {
                /// @brief Unknown.
                int32_t UnkT00 = 0;
                Hitset() { Name = "Region: Hitset"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
                template<typename V>
                void Fields(V& F) {
                    F(UnkT00);
                }
            };

            /// @brief A region that disables fast travel.
            struct FastTravelRestriction : RegionImpl<FastTravelRestriction, 41> {
                /// @brief Fast travel is disabled while this flag is off.
                uint32_t EventFlagID = 0;
                FastTravelRestriction() { Name = "Region: FastTravelRestriction"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
                template<typename V>
                void Fields(V& F) {
                    F(EventFlagID);
                    F.template Const<int32_t>(0);
                }
            };

            /// @brief Unknown.
            struct WeatherCreateAssetPoint : RegionImpl<WeatherCreateAssetPoint, 42> {
                WeatherCreateAssetPoint() { Name = "Region: WeatherCreateAssetPoint"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
                template<typename V>
                void Fields(V& F) {
                    F.template Const<int32_t>(0);
                }
            };

            /// @brief Unknown.
            struct PlayArea : RegionImpl<PlayArea, 43> {
                /// @brief A row in PlayRegionParam.
                int32_t PlayRegionID = 0;
                /// @brief Unknown.
                int32_t UnkT04       = 0;
                PlayArea() { Name = "Region: PlayArea"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
                template<typename V>
                void Fields(V& F) {
                    F(PlayRegionID);
                    F(UnkT04);
                }
            };

            /// @brief Unknown.
            struct EnvironmentMapOutput : RegionImpl<EnvironmentMapOutput, 44, false> {
                EnvironmentMapOutput() { Name = "Region: EnvironmentMapOutput"; }
            };

            /// @brief A point where Torrent can jump high.
            struct MountJump : RegionImpl<MountJump, 46> {
                /// @brief How far the player moves upwards when activating it.
                float JumpHeight = 0;
                /// @brief Unknown.
                int32_t UnkT04   = 0;
                MountJump() { Name = "Region: MountJump"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
                template<typename V>
                void Fields(V& F) {
                    F(JumpHeight);
                    F(UnkT04);
                }
            };

            /// @brief Unknown.
            struct Dummy : RegionImpl<Dummy, 48> {
                /// @brief Unknown.
                int32_t UnkT00 = 0;
                Dummy() { Name = "Region: Dummy"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
                template<typename V>
                void Fields(V& F) {
                    F(UnkT00);
                    F.template Const<int32_t>(0);
                }
            };

            /// @brief Unknown.
            struct FallPreventionRemoval : RegionImpl<FallPreventionRemoval, 49> {
                FallPreventionRemoval() { Name = "Region: FallPreventionRemoval"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
                template<typename V>
                void Fields(V& F) {
                    F.Pad(8);
                }
            };

            /// @brief Unknown.
            struct NavmeshCutting : RegionImpl<NavmeshCutting, 50> {
                NavmeshCutting() { Name = "Region: NavmeshCutting"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
                template<typename V>
                void Fields(V& F) {
                    F.Pad(8);
                }
            };

            /// @brief Changes the name shown for the area.
            struct MapNameOverride : RegionImpl<MapNameOverride, 51> {
                /// @brief FMG ID for the area title card; negative values apply when entering, positive ones on map
                ///        load.
                int32_t TextID = 0;
                MapNameOverride() { Name = "Region: MapNameOverride"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
                template<typename V>
                void Fields(V& F) {
                    F(TextID);
                    F.template Const<int32_t>(0);
                }
            };

            /// @brief Unknown.
            struct MountJumpFall : RegionImpl<MountJumpFall, 52> {
                MountJumpFall() { Name = "Region: MountJumpFall"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
                template<typename V>
                void Fields(V& F) {
                    F.template Const<int32_t>(-1);
                    F.template Const<int32_t>(0);
                }
            };

            /// @brief Affects where Torrent can be summoned.
            struct HorseRideOverride : RegionImpl<HorseRideOverride, 53> {
                /// @brief 1 forbids riding Torrent, 2 permits it.
                uint32_t OverrideType = 1;
                HorseRideOverride() { Name = "Region: HorseRideOverride"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
                template<typename V>
                void Fields(V& F) {
                    F(OverrideType);
                    F.template Const<int32_t>(0);
                }
            };

            /// @brief Unknown.
            struct LockedMountJump : RegionImpl<LockedMountJump, 54> {
                /// @brief How far the player moves upwards when activating it.
                float JumpHeight = 0;
                /// @brief Unknown.
                int32_t UnkT04   = 0;
                /// @brief Probably an event flag that enables it.
                int32_t UnkT08 = 0;
                LockedMountJump() { Name = "Region: LockedMountJump"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
                template<typename V>
                void Fields(V& F) {
                    F(JumpHeight);
                    F(UnkT04);
                    F(UnkT08);
                    F.template Const<int32_t>(-1);
                }
            };

            /// @brief Unknown.
            struct LockedMountJumpFall : RegionImpl<LockedMountJumpFall, 55> {
                /// @brief Probably an event flag that enables it.
                int32_t UnkT08 = 0;
                LockedMountJumpFall() { Name = "Region: LockedMountJumpFall"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
                template<typename V>
                void Fields(V& F) {
                    F.template Const<int32_t>(-1);
                    F.template Const<int32_t>(0);
                    F(UnkT08);
                }
            };

            /// @brief Most likely an unused region.
            struct Other : RegionImpl<Other, 0xFFFFFFFF, false> {
                Other() { Name = "Region: Other"; }
            };

            /// @brief Unknown; probably some kind of version number.
            int32_t Version = 73;
            /// @brief Previously points where players will appear when invading; not sure if they do anything in Sekiro.
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
            /// @brief Points that have developer messages.
            std::vector<Message> Messages;
            /// @brief Unknown.
            std::vector<EnvironmentMapEffectBox> EnvironmentMapEffectBoxes;
            /// @brief Unknown.
            std::vector<WindArea> WindAreas;
            /// @brief Unknown.
            std::vector<Connection> Connections;
            /// @brief Unknown.
            std::vector<PatrolRoute22> PatrolRoute22s;
            /// @brief Unknown.
            std::vector<BuddySummonPoint> BuddySummonPoints;
            /// @brief Unknown.
            std::vector<DisableTumbleweed> DisableTumbleweeds;
            /// @brief Areas where sound is muffled.
            std::vector<MufflingBox> MufflingBoxes;
            /// @brief Entrances to muffling boxes.
            std::vector<MufflingPortal> MufflingPortals;
            /// @brief Unknown.
            std::vector<SoundRegion> SoundRegions;
            /// @brief Unknown.
            std::vector<MufflingPlane> MufflingPlanes;
            /// @brief Points that describe an NPC patrol path.
            std::vector<PatrolRoute> PatrolRoutes;
            /// @brief Unknown.
            std::vector<MapPoint> MapPoints;
            /// @brief Unknown.
            std::vector<WeatherOverride> WeatherOverrides;
            /// @brief Unknown.
            std::vector<AutoDrawGroupPoint> AutoDrawGroupPoints;
            /// @brief Unknown.
            std::vector<GroupDefeatReward> GroupDefeatRewards;
            /// @brief Unknown.
            std::vector<MapPointDiscoveryOverride> MapPointDiscoveryOverrides;
            /// @brief Unknown.
            std::vector<MapPointParticipationOverride> MapPointParticipationOverrides;
            /// @brief Unknown.
            std::vector<Hitset> Hitsets;
            /// @brief The FastTravelRestriction entries.
            std::vector<FastTravelRestriction> FastTravelRestrictions;
            /// @brief Unknown.
            std::vector<WeatherCreateAssetPoint> WeatherCreateAssetPoints;
            /// @brief Unknown.
            std::vector<PlayArea> PlayAreas;
            /// @brief Unknown.
            std::vector<EnvironmentMapOutput> EnvironmentMapOutputs;
            /// @brief Unknown.
            std::vector<MountJump> MountJumps;
            /// @brief Unknown.
            std::vector<Dummy> Dummies;
            /// @brief Unknown.
            std::vector<FallPreventionRemoval> FallPreventionRemovals;
            /// @brief Unknown.
            std::vector<NavmeshCutting> NavmeshCuttings;
            /// @brief Unknown.
            std::vector<MapNameOverride> MapNameOverrides;
            /// @brief Unknown.
            std::vector<MountJumpFall> MountJumpFalls;
            /// @brief Unknown.
            std::vector<HorseRideOverride> HorseRideOverrides;
            /// @brief Unknown.
            std::vector<LockedMountJump> LockedMountJumps;
            /// @brief Unknown.
            std::vector<LockedMountJumpFall> LockedMountJumpFalls;
            /// @brief Most likely a dumping ground for unused regions.
            std::vector<Other> Others;

            /// @brief The param's name in the file.
            static constexpr const char* ParamName = "POINT_PARAM_ST";

            /// @brief Calls Visit with each per-type list, for both ForEachList overloads.
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
            /// @brief Calls Visit with each per-type list, in the order the entries are written.
            template<typename F>
            void ForEachList(F&& Visit) {
                VisitLists(*this, Visit);
            }
            /// @brief Calls Visit with each per-type list, in the order the entries are written.
            template<typename F>
            void ForEachList(F&& Visit) const {
                VisitLists(*this, Visit);
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
            /// @brief The ID the entry had in the file, written back unchanged; -1 for entries made from scratch.
            int32_t FileID = -1;

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
            /// @brief Unknown.
            struct Other : RouteOf<0xFFFFFFFF> {
                Other() { Name = "X-X"; }
            };

            /// @brief Unknown; probably some kind of version number.
            int32_t Version = 73;
            /// @brief Unknown.
            std::vector<MufflingPortalLink> MufflingPortalLinks;
            /// @brief Unknown.
            std::vector<MufflingBoxLink> MufflingBoxLinks;
            /// @brief Unknown.
            std::vector<Other> Others;

            /// @brief The param's name in the file.
            static constexpr const char* ParamName = "ROUTE_PARAM_ST";

            /// @brief Calls Visit with each per-type list, in the order the entries are written.
            template<typename F>
            void ForEachList(F&& Visit) {
                Visit(MufflingPortalLinks);
                Visit(MufflingBoxLinks);
                Visit(Others);
            }
            /// @brief Calls Visit with each per-type list, in the order the entries are written.
            template<typename F>
            void ForEachList(F&& Visit) const {
                Visit(MufflingPortalLinks);
                Visit(MufflingBoxLinks);
                Visit(Others);
            }
        };

        /// @brief Used for unused params that should never have any entries in them.
        struct EmptyParam {
            /// @brief Unknown; probably some kind of version number.
            int32_t Version = 0;
        };
#pragma endregion

#pragma region Parts
        /// @brief Unknown; holds the part's display and draw groups.
        struct UnkStruct1 {
            /// @brief Unknown.
            std::array<uint32_t, 8> DisplayGroups{};
            /// @brief Unknown.
            std::array<uint32_t, 8> DrawGroups{};
            /// @brief Unknown.
            std::array<uint32_t, 32> CollisionMask{};
            /// @brief Unknown.
            uint8_t Condition1 = 0;
            /// @brief Unknown.
            uint8_t Condition2 = 0;
            /// @brief Unknown.
            uint8_t UnkC2 = 0;
            /// @brief Unknown.
            uint8_t UnkC3 = 0;
            /// @brief Unknown.
            int16_t UnkC4 = 0;
            /// @brief Unknown.
            int16_t UnkC6 = 0;
            /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
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

        /// @brief Unknown.
        struct UnkStruct2 {
            /// @brief Unknown.
            int32_t Condition = 0;
            /// @brief Unknown.
            std::array<uint32_t, 8> DispGroups{};
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

        /// @brief Overrides of the gparams used while on a collision.
        struct SceneGparamConfig {
            /// @brief Unknown.
            float TransitionTime = 0;
            /// @brief The hundredths place of a gparam ID to use, and the ones to override it with, in order.
            int8_t GparamSubIDBase = -1;
            /// @brief Value of the hundredths place of a Gparam to override Base with.
            int8_t GparamSubIDOverride1 = -1;
            /// @brief Value of the hundredths place of a Gparam to override Base and Override 1 with.
            int8_t GparamSubIDOverride2 = -1;
            /// @brief Value of the hundredths place of a Gparam to override Base and Override 1 and Override 2 with.
            int8_t GparamSubIDOverride3 = -1;
            /// @brief Unknown.
            int8_t Unk1C = -1;
            /// @brief Unknown.
            int8_t Unk1D = -1;
            /// @brief Unknown.
            int8_t Unk20 = -1;
            /// @brief Unknown.
            int8_t Unk21 = -1;
            /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
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

        /// @brief Grass types used on a part (rows in GrassTypeParam).
        struct GrassConfig {
            /// @brief Rows in GrassTypeParam.
            std::array<int32_t, 6> GrassParamIDs{};
            /// @brief Unknown.
            int32_t Unk18 = 0;
            /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
            template<typename V>
            void Fields(V& F) {
                F(GrassParamIDs);
                F(Unk18);
                F.template Const<int32_t>(0);
            }
        };

        /// @brief Unknown.
        struct UnkStruct8 {
            /// @brief Unknown.
            int32_t Unk00 = 0;
            /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
            template<typename V>
            void Fields(V& F) {
                F(Unk00);
                F.Pad(28);
            }
        };

        /// @brief Unknown.
        struct UnkStruct9 {
            /// @brief Unknown.
            int32_t Unk00 = 0;
            /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
            template<typename V>
            void Fields(V& F) {
                F(Unk00);
                F.Pad(28);
            }
        };

        /// @brief Unknown; related to which map tile loads the part.
        struct TileLoadConfig {
            /// @brief Unknown.
            std::array<uint8_t, 4> MapID{};
            /// @brief Unknown.
            int32_t Unk04 = 0;
            /// @brief Unknown.
            int32_t Unk0C = 0;
            /// @brief Unknown.
            int32_t Unk10 = 0;
            /// @brief Unknown culling behaviour for extreme height differences; 0 to 10000.
            int32_t CullingHeightBehavior = 0;
            /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
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

        /// @brief Unknown.
        struct UnkStruct11 {
            /// @brief Unknown.
            int32_t Unk00 = 0;
            /// @brief Unknown.
            int32_t Unk04 = 0;
            /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
            template<typename V>
            void Fields(V& F) {
                F(Unk00);
                F(Unk04);
                F.Pad(24);
            }
        };

        /// @brief Which optional blocks of data a part type has; the block of each bit is the member of the same name.
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

        /// @brief Common data for all types of part.
        struct SOULS_API Part : Entry {
            /// @brief The model used by this part; requires an entry in ModelParam.
            Msb::Ref<ModelList> ModelName;
            /// @brief Involved with serialization.
            int32_t InstanceID = 0;
            /// @brief A path to a .sib file, presumably some kind of editor placeholder.
            std::string SibPath;
            /// @brief Location of the part.
            Vector3 Position;
            /// @brief Rotation of the part.
            Vector3 Rotation;
            /// @brief Scale of the part; only works for map pieces and assets.
            Vector3 Scale{1.f, 1.f, 1.f};
            /// @brief 0 never disables the part; 1 disables it in all released versions of the game; 2 and 3 are
            ///        unknown.
            uint32_t GameEditionDisable = 0;
            /// @brief Very speculative.
            uint32_t MapStudioLayer     = 0;
            /// @brief Identifies the part in event scripts.
            uint32_t EntityID = 0;
            /// @brief If set, PartsDrawParamID is used; otherwise the asset's own param is.
            uint8_t IsUsePartsDrawParamID = 0;
            /// @brief A row in PartsDrawParam.
            int16_t PartsDrawParamID     = 0;
            /// @brief Unknown.
            int8_t IsPointLightShadowSrc = 0;
            /// @brief Unknown.
            uint8_t UnkE0B               = 0;
            /// @brief Unknown.
            bool IsShadowSrc             = false;
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
            uint8_t EnableOnAboveShadow  = 0;
            /// @brief Unknown.
            bool DisablePointLightEffect = false;
            /// @brief Unknown.
            uint8_t UnkE17               = 0;
            /// @brief Unknown.
            int32_t UnkE18               = 0;
            /// @brief Allows multiple parts to be identified by the same entity ID.
            std::array<uint32_t, 8> EntityGroupIDs{};
            /// @brief Unknown.
            int16_t UnkE3C = 0;
            /// @brief Unknown.
            int16_t UnkE3E = 0;
            /// @brief Display and draw groups and collision masks; every part type has this block.
            UnkStruct1 Unk1;

            /// @brief The type number of this entry as stored in the file.
            virtual uint32_t Type() const   = 0;
            /// @brief Which of the optional data blocks this part type has (a mask of PartBlocks).
            virtual unsigned Blocks() const = 0;
            /// @brief Reads the entry from its position in the file.
            void Read(BinaryReader& Reader);
            /// @brief Writes the entry; ID is its position among the entries of the same type.
            void Write(BinaryWriter& Writer, int32_t ID);
            /// @brief Reads the type-specific data.
            virtual void ReadTypeData(BinaryReader& Reader)  = 0;
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
        /// D::Block (a PartBlocks mask) says which of the optional blocks it has; each block is the member of D with
        /// the bit's name (Unk1, Gparam, TileLoad...).
        template<typename D, uint32_t Id, typename B = Part>
        struct PartImpl : TypeDataOf<D, B, true> {
            /// @brief The type number entries of this type have in the file.
            static constexpr uint32_t TypeId = Id;
            /// @brief The type number of this entry as stored in the file.
            uint32_t Type() const override { return Id; }
            /// @brief Which of the optional data blocks this part type has (a mask of PartBlocks).
            unsigned Blocks() const override { return D::Block; }
            /// @brief Visits the fields of one of the optional data blocks.
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
            /// @brief Writes one of the optional data blocks.
            void WriteBlock(unsigned Block, BinaryWriter& Writer) override {
                Msb::Detail::WriteVisitor V{Writer};
                VisitBlock(Block, V);
            }
        };

        /// @brief Common base data for enemies and dummy enemies.
        struct EnemyBase : Part {
            /// @brief Unknown.
            GparamConfig Gparam;
            /// @brief Unknown.
            UnkStruct8 Unk8;
            /// @brief Unknown.
            TileLoadConfig TileLoad;
            /// @brief A row in NpcThinkParam that determines the enemy's AI characteristics.
            int32_t ThinkParamID = -1;
            /// @brief A row in NpcParam that determines a variety of enemy properties.
            int32_t NPCParamID = -1;
            /// @brief The enemy's talk script (ESD).
            int32_t TalkID    = -1;
            /// @brief Unknown.
            bool UnkT15       = false;
            /// @brief Unknown.
            int16_t PlatoonID = -1;
            /// @brief A row in CharaInitParam that determines a human's inventory and stats.
            int32_t CharaInitID = -1;
            /// @brief Should reference the collision the enemy starts on.
            Msb::Ref<PartList> CollisionPartName;
            /// @brief The patrol route (a PatrolInfo event) the enemy follows.
            Msb::Ref<PatrolInfoList, int16_t> WalkRouteName;
            /// @brief Unknown.
            int32_t UnkT24 = -1;
            /// @brief Unknown.
            int32_t UnkT28 = 0;
            /// @brief A row in ChrActivateConditionParam that affects when the enemy appears.
            int32_t ChrActivateCondParamID = 0;
            /// @brief Unknown.
            int32_t UnkT34                 = 0;
            /// @brief Unknown.
            int32_t BackupEventAnimID      = -1;
            /// @brief Unknown.
            int32_t UnkT3C                 = 0;
            /// @brief Rows in SpEffectSetParam applied to the enemy.
            std::array<int32_t, 4> SpEffectSetParamIDs{};
            /// @brief Unknown.
            float UnkT84 = 0;

            EnemyBase() { Name = "cXXXX_XXXX"; }
            /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
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

        /// @brief Instances of actual things in the map.
        struct PartsParam : Msb::Detail::TypedLists<PartsParam, Part> {
            /// @brief Fixed visual geometry; not used much in Elden Ring.
            struct MapPiece : PartImpl<MapPiece, 0> {
                /// @brief Which optional data blocks this part type has (a mask of PartBlocks).
                static constexpr unsigned Block = HasUnk1 | HasGparam | HasGrass | HasUnk8 | HasUnk9 | HasTileLoad | HasUnk11;
                /// @brief Unknown.
                GparamConfig Gparam;
                /// @brief Unknown.
                GrassConfig Grass;
                /// @brief Unknown.
                UnkStruct8 Unk8;
                /// @brief Unknown.
                UnkStruct9 Unk9;
                /// @brief Unknown.
                TileLoadConfig TileLoad;
                /// @brief Unknown.
                UnkStruct11 Unk11;
                MapPiece() { Name = "mXXXXXX_XXXX"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
                template<typename V>
                void Fields(V& F) {
                    F.Pad(8);
                }
            };

            /// @brief Any non-player character.
            struct Enemy : PartImpl<Enemy, 2, EnemyBase> {
                /// @brief Which optional data blocks this part type has (a mask of PartBlocks).
                static constexpr unsigned Block = HasUnk1 | HasGparam | HasUnk8 | HasTileLoad;
            };

            /// @brief A spawn point for the player, or something.
            struct Player : PartImpl<Player, 4> {
                /// @brief Which optional data blocks this part type has (a mask of PartBlocks).
                static constexpr unsigned Block = HasUnk1 | HasUnk8 | HasTileLoad;
                /// @brief Unknown.
                UnkStruct8 Unk8;
                /// @brief Unknown.
                TileLoadConfig TileLoad;
                /// @brief Unknown.
                int32_t Unk00 = 0;
                Player() { Name = "c0000_XXXX"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
                template<typename V>
                void Fields(V& F) {
                    F(Unk00);
                    F.Pad(12);
                }
            };

            /// @brief Invisible but physical geometry.
            struct Collision : PartImpl<Collision, 5> {
                /// @brief Which optional data blocks this part type has (a mask of PartBlocks).
                static constexpr unsigned Block = HasUnk1 | HasUnk2 | HasGparam | HasSceneGparam | HasUnk8 | HasTileLoad | HasUnk11;
                /// @brief Unknown.
                UnkStruct2 Unk2;
                /// @brief Unknown.
                GparamConfig Gparam;
                /// @brief Unknown.
                SceneGparamConfig SceneGparam;
                /// @brief Unknown.
                UnkStruct8 Unk8;
                /// @brief Unknown.
                TileLoadConfig TileLoad;
                /// @brief Unknown.
                UnkStruct11 Unk11;
                /// @brief Collision behavior: 8 standard, 9 camera only, 11 enemy only, 13 fall death camera, 15 kill,
                ///        ...
                uint8_t HitFilterID = 8;
                /// @brief Unknown.
                uint8_t UnkT01 = 0;
                /// @brief Unknown.
                uint8_t UnkT02 = 0;
                /// @brief Unknown.
                bool UnkT03    = false;
                /// @brief Unknown.
                float UnkT04   = 0;
                /// @brief Unknown.
                int32_t UnkT06 = 0;
                /// @brief Unknown.
                float UnkT14   = 0;
                /// @brief Location text to display when stepping onto this collision.
                int32_t LocationTextID = 0;
                /// @brief Unknown.
                int32_t UnkT1C         = 0;
                /// @brief Used to determine invasion eligibility (PlayRegionParam).
                int32_t PlayRegionID = 0;
                /// @brief Unknown.
                int16_t UnkT24 = 0;
                /// @brief Unknown.
                int16_t UnkT26 = 0;
                /// @brief Unknown.
                int32_t UnkT30 = 0;
                /// @brief Unknown.
                uint8_t UnkT34 = 0;
                /// @brief Unknown.
                uint8_t UnkT35 = 0;
                /// @brief Disables summoning and riding Torrent.
                bool DisableTorrent = false;
                /// @brief Unknown.
                int16_t UnkT3C = 0;
                /// @brief Unknown.
                int16_t UnkT3E = 0;
                /// @brief Unknown.
                float UnkT40 = 0;
                /// @brief Disables fast travel unless this flag is set.
                uint32_t EnableFastTravelEventFlagID = 0;
                /// @brief Unknown.
                int16_t UnkT4C = 0;
                /// @brief Unknown.
                int16_t UnkT4E = 0;

                Collision() { Name = "hXXXXXX"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
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

            /// @brief An asset that either isn't used, or is used for a cutscene.
            struct DummyAsset : PartImpl<DummyAsset, 9> {
                /// @brief Which optional data blocks this part type has (a mask of PartBlocks).
                static constexpr unsigned Block = HasUnk1 | HasGparam | HasUnk8 | HasTileLoad;
                /// @brief Unknown.
                GparamConfig Gparam;
                /// @brief Unknown.
                UnkStruct8 Unk8;
                /// @brief Unknown.
                TileLoadConfig TileLoad;
                /// @brief Unknown.
                int32_t UnkT18 = 0;
                DummyAsset() { Name = "AEGxxx_xxx_xxxx"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
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

            /// @brief An enemy that either isn't used, or is used for a cutscene.
            struct DummyEnemy : PartImpl<DummyEnemy, 10, EnemyBase> {
                /// @brief Which optional data blocks this part type has (a mask of PartBlocks).
                static constexpr unsigned Block = HasUnk1 | HasGparam | HasUnk8 | HasTileLoad;
            };

            /// @brief References an actual collision and causes another map to be loaded while on it.
            struct ConnectCollision : PartImpl<ConnectCollision, 11> {
                /// @brief Which optional data blocks this part type has (a mask of PartBlocks).
                static constexpr unsigned Block = HasUnk1 | HasUnk2 | HasUnk8 | HasTileLoad | HasUnk11;
                /// @brief Unknown.
                UnkStruct2 Unk2;
                /// @brief Unknown.
                UnkStruct8 Unk8;
                /// @brief Unknown.
                TileLoadConfig TileLoad;
                /// @brief Unknown.
                UnkStruct11 Unk11;
                /// @brief The collision part to attach to.
                Msb::Ref<CollisionList> CollisionName;
                /// @brief The map to load when on this collision.
                std::array<uint8_t, 4> MapID{};
                /// @brief Unknown.
                uint8_t UnkT08 = 0;
                /// @brief Unknown.
                bool UnkT09    = false;
                /// @brief Unknown.
                uint8_t UnkT0A = 0;
                /// @brief Unknown.
                bool UnkT0B    = false;
                ConnectCollision() { Name = "hXXXXXX_XXXX"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
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

            /// @brief A placed asset: Elden Ring's dynamic props and interactive things.
            struct Asset : PartImpl<Asset, 13> {
                /// @brief Which optional data blocks this part type has (a mask of PartBlocks).
                static constexpr unsigned Block = HasUnk1 | HasUnk2 | HasGparam | HasGrass | HasUnk8 | HasUnk9 | HasTileLoad | HasUnk11;

                /// @brief Unknown.
                struct AssetUnkStruct1 {
                    /// @brief Unknown.
                    int16_t Unk00 = 0;
                    /// @brief Unknown.
                    bool Unk04    = false;
                    /// @brief Disables summoning and riding Torrent, but only when the asset isn't on a collision that
                    ///        does.
                    bool DisableTorrentAssetOnly = false;
                    /// @brief Unknown.
                    int32_t Unk1C                = 0;
                    /// @brief Unknown.
                    int16_t Unk24 = 0;
                    /// @brief Unknown.
                    int16_t Unk26 = 0;
                    /// @brief Unknown.
                    int32_t Unk28 = 0;
                    /// @brief Unknown.
                    int32_t Unk2C = 0;
                    /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
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

                /// @brief Unknown.
                struct AssetUnkStruct2 {
                    /// @brief Unknown.
                    int32_t Unk00 = 0;
                    /// @brief Unknown.
                    int32_t Unk04 = 0;
                    /// @brief Unknown.
                    float Unk14   = 0;
                    /// @brief Unknown.
                    uint8_t Unk1C = 0;
                    /// @brief Unknown.
                    uint8_t Unk1D = 0;
                    /// @brief Unknown.
                    uint8_t Unk1E = 0;
                    /// @brief Unknown.
                    uint8_t Unk1F = 0;
                    /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
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

                /// @brief Unknown.
                struct AssetUnkStruct3 {
                    /// @brief Unknown.
                    int32_t Unk00 = 0;
                    /// @brief Unknown.
                    float Unk04   = 0;
                    /// @brief Unknown.
                    uint8_t Unk09 = 0;
                    /// @brief Unknown.
                    uint8_t Unk0A = 0;
                    /// @brief Unknown.
                    uint8_t Unk0B = 0;
                    /// @brief Unknown.
                    int16_t Unk0C = 0;
                    /// @brief Unknown.
                    int16_t Unk0E = 0;
                    /// @brief Unknown.
                    float Unk10   = 0;
                    /// @brief Disables the asset when this map is loaded.
                    std::array<int8_t, 4> DisableWhenMapLoadedMapID{};
                    /// @brief Unknown.
                    int32_t Unk18 = 0;
                    /// @brief Unknown.
                    int32_t Unk1C = 0;
                    /// @brief Unknown.
                    int32_t Unk20 = 0;
                    /// @brief Unknown.
                    uint8_t Unk24 = 0;
                    /// @brief Unknown.
                    bool Unk25    = false;
                    /// @brief Unknown.
                    int32_t Unk28 = 0;
                    /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
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

                /// @brief Unknown.
                struct AssetUnkStruct4 {
                    /// @brief Unknown.
                    bool Unk00    = false;
                    /// @brief Unknown.
                    uint8_t Unk01 = 0;
                    /// @brief Unknown.
                    uint8_t Unk02 = 0;
                    /// @brief Unknown.
                    bool Unk03    = false;
                    /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
                    template<typename V>
                    void Fields(V& F) {
                        F(Unk00);
                        F(Unk01);
                        F(Unk02);
                        F(Unk03);
                        F.Pad(60);
                    }
                };

                /// @brief Unknown.
                UnkStruct2 Unk2;
                /// @brief Unknown.
                GparamConfig Gparam;
                /// @brief Unknown.
                GrassConfig Grass;
                /// @brief Unknown.
                UnkStruct8 Unk8;
                /// @brief Unknown.
                UnkStruct9 Unk9;
                /// @brief Unknown.
                TileLoadConfig TileLoad;
                /// @brief Unknown.
                UnkStruct11 Unk11;
                /// @brief Unknown.
                int16_t UnkT00 = 0;
                /// @brief Unknown.
                int16_t UnkT02 = 0;
                /// @brief Unknown.
                uint8_t UnkT10 = 0;
                /// @brief Unknown.
                bool UnkT11    = false;
                /// @brief Unknown.
                uint8_t UnkT12 = 0;
                /// @brief Added to the model ID to pick the row in AssetModelSfxParam.
                int16_t AssetSfxParamRelativeID = 0;
                /// @brief Unknown.
                int16_t UnkT1E                  = 0;
                /// @brief Unknown.
                int32_t UnkT24 = 0;
                /// @brief Unknown.
                int32_t UnkT28 = 0;
                /// @brief Unknown.
                int32_t UnkT30 = 0;
                /// @brief Unknown.
                int32_t UnkT34 = 0;
                /// @brief Unknown.
                std::array<Msb::Ref<PartList>, 6> UnkPartNames;
                /// @brief Unknown.
                bool UnkT50    = false;
                /// @brief Unknown.
                uint8_t UnkT51 = 0;
                /// @brief Unknown.
                uint8_t UnkT53 = 0;
                /// @brief Unknown.
                Msb::Ref<PartList> UnkT54PartName;
                /// @brief Unknown.
                int32_t UnkModelMaskAndAnimID = 0;
                /// @brief Unknown.
                int32_t UnkT5C = 0;
                /// @brief Unknown.
                int32_t UnkT60 = 0;
                /// @brief Unknown.
                int32_t UnkT64 = 0;
                /// @brief Unknown.
                AssetUnkStruct1 AssetUnk1;
                /// @brief Unknown.
                AssetUnkStruct2 AssetUnk2;
                /// @brief Unknown.
                AssetUnkStruct3 AssetUnk3;
                /// @brief Unknown.
                AssetUnkStruct4 AssetUnk4;

                Asset() { Name = "AEGxxx_xxx_xxxx"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
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

            /// @brief Unknown; probably some kind of version number.
            int32_t Version = 73;
            /// @brief All of the fixed visual geometry of the map.
            std::vector<MapPiece> MapPieces;
            /// @brief All non-player characters.
            std::vector<Enemy> Enemies;
            /// @brief These have something to do with player spawn points.
            std::vector<Player> Players;
            /// @brief Invisible physical geometry of the map.
            std::vector<Collision> Collisions;
            /// @brief Objects that don't appear normally; either unused, or used for cutscenes.
            std::vector<DummyAsset> DummyAssets;
            /// @brief Enemies that don't appear normally; either unused, or used for cutscenes.
            std::vector<DummyEnemy> DummyEnemies;
            /// @brief Dummy parts that reference an actual collision and cause it to load another map.
            std::vector<ConnectCollision> ConnectCollisions;
            /// @brief Dynamic props and interactive things.
            std::vector<Asset> Assets;

            /// @brief The param's name in the file.
            static constexpr const char* ParamName = "PARTS_PARAM_ST";

            /// @brief Calls Visit with each per-type list, in the order the entries are written.
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
            /// @brief Calls Visit with each per-type list, in the order the entries are written.
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

        /// @brief Model files that are available for parts to use.
        ModelParam Models;
        /// @brief Dynamic or interactive systems such as item pickups, levers, enemy spawners, etc.
        EventParam Events;
        /// @brief Points or areas of space that trigger some sort of behavior.
        PointParam Regions;
        /// @brief Unknown, but related to muffling regions somehow.
        RouteParam Routes;
        /// @brief Unknown and unused.
        EmptyParam Layers{73};
        /// @brief Instances of actual things in the map.
        PartsParam Parts;

    protected:
        bool IsImpl(BinaryReader& Reader) override;
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
