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

    // MSB3: the map layout file of Dark Souls III. Extension: .msb
    //
    // An MSB3 has params of models (the model files parts can use), events, regions, routes, layers, parts and parts
    // poses. Entries refer to each other by name.
    class SOULS_API MSB3 : public SoulsFile<MSB3> {
    public:
        using ModelList      = Msb::ModelList;
        using EventList      = Msb::EventList;
        using RegionList     = Msb::RegionList;
        using PartList       = Msb::PartList;
        using CollisionList  = Msb::CollisionList;
        using BoneNameList   = Msb::BoneNameList;
        struct PatrolInfoList {};  // just the patrol info events (a sublist of the events)

        // A generic entry with a name.
        struct Entry {
            std::string Name;
            virtual ~Entry() = default;
        };

        // All entries of the file, in file order, for resolving references between them.
        struct Entries {
            std::vector<Entry*> Models, Events, Regions, Parts, Collisions, PatrolInfos, BoneNames;

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
        // A model available for use by parts in this map.
        struct SOULS_API Model : Entry {
            // Unknown network path to a .sib file.
            std::string SibPath;
            // How many parts use the model; recalculated when writing.
            int32_t InstanceCount = 0;

            virtual uint32_t Type() const        = 0;
            virtual bool HasTypeData() const     = 0;
            virtual void ReadTypeData(BinaryReader&)  {}
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

        // A section containing all the models available to parts in this map.
        struct ModelParam : Msb::Detail::TypedLists<ModelParam, Model> {
            // A fixed part of the level geometry.
            struct MapPiece : ModelImpl<MapPiece, 0, true> {
                uint8_t UnkT00 = 0;
                uint8_t UnkT01 = 0;
                bool UnkT02    = true;
                bool UnkT03    = true;
                MapPiece() { Name = "mXXXXXX"; }
                template<typename V>
                void Fields(V& F) {
                    F(UnkT00);
                    F(UnkT01);
                    F(UnkT02);
                    F(UnkT03);
                    F.Pad(12);
                }
            };

            // A dynamic or interactible entity.
            struct Object : ModelImpl<Object, 1, true> {
                uint8_t UnkT00 = 0;
                uint8_t UnkT01 = 0;
                bool UnkT02    = true;
                bool UnkT03    = true;
                Object() { Name = "oXXXXXX"; }
                template<typename V>
                void Fields(V& F) {
                    F(UnkT00);
                    F(UnkT01);
                    F(UnkT02);
                    F(UnkT03);
                    F.Pad(12);
                }
            };

            // Any character in the map that is not the player.
            struct Enemy : ModelImpl<Enemy, 2, false> {
                Enemy() { Name = "cXXXX"; }
            };

            // The player character.
            struct Player : ModelImpl<Player, 4, false> {
                Player() { Name = "c0000"; }
            };

            // The invisible physical surface of the map.
            struct Collision : ModelImpl<Collision, 5, false> {
                Collision() { Name = "hXXXXXX"; }
            };

            // Unknown.
            struct Other : ModelImpl<Other, 0xFFFFFFFF, false> {
                Other() { Name = "lXXXXXX"; }
            };

            std::vector<MapPiece> MapPieces;
            std::vector<Object> Objects;
            std::vector<Enemy> Enemies;
            std::vector<Player> Players;
            std::vector<Collision> Collisions;
            std::vector<Other> Others;

            static constexpr int32_t ParamVersion    = 3;
            static constexpr const char* ParamName   = "MODEL_PARAM_ST";

            template<typename F>
            void ForEachList(F&& Visit) {
                Visit(MapPieces);
                Visit(Objects);
                Visit(Enemies);
                Visit(Players);
                Visit(Collisions);
                Visit(Others);
            }
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
        // An interactive or dynamic feature of the map.
        struct SOULS_API Event : Entry {
            // Unknown.
            int32_t EventID = -1;
            // The part and region the event is attached to.
            Msb::Ref<PartList> PartName;
            Msb::Ref<RegionList> PointName;
            // Used to identify the event in event scripts.
            int32_t EntityID = -1;

            virtual uint32_t Type() const = 0;
            void Read(BinaryReader& Reader);
            void Write(BinaryWriter& Writer, int32_t ID);
            virtual void ReadTypeData(BinaryReader& Reader)  = 0;
            virtual void WriteTypeData(BinaryWriter& Writer) = 0;
            virtual void GetNames(const Entries& Lists);
            virtual void GetIndices(const Entries& Lists);
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

        // Events controlling various interactive or dynamic features in the map.
        struct EventParam : Msb::Detail::TypedLists<EventParam, Event> {
            // A pickuppable item.
            struct Treasure : EventImpl<Treasure, 4> {
                // The part the treasure is attached to.
                Msb::Ref<PartList> TreasurePartName;
                // First item lot given by this treasure, and second item lot (rarely used).
                int32_t ItemLot1 = -1;
                int32_t ItemLot2 = -1;
                int32_t UnkT18   = -1;
                // If not -1, uses an entry from ActionButtonParam for the pickup prompt.
                int32_t ActionButtonParamID = -1;
                // Animation to play when taking this treasure.
                int32_t PickupAnimID = 60070;
                // Changes the text of the pickup prompt and causes the treasure to be uninteractible by default.
                bool InChest = false;
                // Whether the treasure should be hidden by default.
                bool StartDisabled = false;
                Treasure() { Name = "Event: Treasure"; }
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

            // A continuous enemy spawner.
            struct Generator : EventImpl<Generator, 5> {
                uint8_t MaxNum   = 0;
                int8_t GenType   = 0;
                int16_t LimitNum = 0, MinGenNum = 0, MaxGenNum = 0;
                float MinInterval = 0, MaxInterval = 0;
                // Regions that enemies can be spawned at.
                std::array<Msb::Ref<RegionList>, 8> SpawnPointNames;
                // Enemies spawned by this generator.
                std::array<Msb::Ref<PartList>, 32> SpawnPartNames;
                uint8_t InitialSpawnCount = 0;
                float UnkT14 = 0, UnkT18 = 0;
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
                    F.Pad(20);
                    F(SpawnPointNames);
                    F.Pad(16);
                    F(SpawnPartNames);
                    F.Pad(32);
                }
            };

            // Controls usable objects like levers.
            struct ObjAct : EventImpl<ObjAct, 7> {
                enum class ObjActState : uint8_t {
                    OneState      = 0,
                    DoorState     = 1,
                    OneLoopState  = 2,
                    OneLoopState2 = 3,
                    DoorState2    = 4,
                };
                int32_t ObjActEntityID = -1;
                // The object which is being interacted with.
                Msb::Ref<PartList> ObjActPartName;
                // ID in ObjActParam that configures this ObjAct.
                int32_t ObjActParamID = 0;
                ObjActState ObjActStateType = ObjActState::OneState;
                int32_t EventFlagID         = 0;
                ObjAct() { Name = "Event: ObjAct"; }
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

            // Moves all of the map pieces when cutscenes are played.
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

            // A fake multiplayer interaction where the player goes to an NPC's world.
            struct PseudoMultiplayer : EventImpl<PseudoMultiplayer, 12> {
                // The NPC whose world you are entering.
                int32_t HostEntityID = -1;
                // Set when inside the event's region, unset when outside it.
                int32_t EventFlagID = -1;
                // ID of a goods item that is used to trigger the event.
                int32_t ActivateGoodsID = -1;
                // Unknown; possibly a sound ID, a map event ID, and flags.
                int32_t UnkT0C = -1, UnkT10 = -1, UnkT14 = 0, UnkT18 = 0;
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
                    F.Pad(4);
                }
            };

            // A simple list of points defining a path for enemies to take.
            struct PatrolInfo : EventImpl<PatrolInfo, 14> {
                // Unknown; probably some kind of route type.
                int32_t UnkT00 = 0;
                // List of points in the route.
                std::array<Msb::Ref<RegionList, int16_t>, 32> WalkPointNames;
                PatrolInfo() { Name = "Event: PatrolInfo"; }
                template<typename V>
                void Fields(V& F) {
                    F(UnkT00);
                    F.Pad(12);
                    F(WalkPointNames);
                }
            };

            // Unknown.
            struct PlatoonInfo : EventImpl<PlatoonInfo, 15> {
                int32_t PlatoonIDScriptActivate = 0;
                int32_t State                   = 0;
                std::array<Msb::Ref<PartList>, 32> GroupPartsNames;
                PlatoonInfo() { Name = "Event: PlatoonInfo"; }
                template<typename V>
                void Fields(V& F) {
                    F(PlatoonIDScriptActivate);
                    F(State);
                    F.Pad(8);
                    F(GroupPartsNames);
                }
            };

            // Unknown. Only appears once in one unused MSB.
            struct Other : EventImpl<Other, 0xFFFFFFFF> {
                int32_t UnkT00 = 0, UnkT04 = 0;
                Other() { Name = "Event: Other"; }
                template<typename V>
                void Fields(V& F) {
                    F(UnkT00);
                    F(UnkT04);
                    F.Pattern(0x40, 0xFF);
                }
            };

            std::vector<Treasure> Treasures;
            std::vector<Generator> Generators;
            std::vector<ObjAct> ObjActs;
            std::vector<MapOffset> MapOffsets;
            std::vector<PseudoMultiplayer> PseudoMultiplayers;
            std::vector<PatrolInfo> PatrolInfos;
            std::vector<PlatoonInfo> PlatoonInfos;
            std::vector<Other> Others;

            static constexpr int32_t ParamVersion  = 3;
            static constexpr const char* ParamName = "EVENT_PARAM_ST";

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
        // Whether a region type has type-specific data after its common data.
        enum class TypeDataPresence {
            Never,
            Sometimes,
            Always,
            AlwaysNull,  // the data is always there but is not pointed to by an offset
        };

        // A point or volumetric area used for a variety of purposes.
        struct SOULS_API Region : Entry {
            int32_t Unk2C = 0;
            // The shape of this region. Composite shapes aren't supported in Dark Souls III.
            Msb::Shape Shape = Msb::Shapes::Point{};
            // Controls whether the event is present in different ceremonies. Maybe only used for messages?
            uint32_t MapStudioLayer = 0;
            // Center of the region.
            Vector3 Position;
            // Rotation of the region, in degrees.
            Vector3 Rotation;
            // Unknown.
            std::vector<int16_t> UnkA, UnkB;
            // Region is inactive unless this part is drawn; empty for always active.
            Msb::Ref<PartList> ActivationPartName;
            // An ID used to identify this region in event scripts.
            int32_t EntityID = -1;

            virtual uint32_t Type() const                        = 0;
            virtual TypeDataPresence ShouldHaveTypeData() const  = 0;
            virtual bool DoesHaveTypeData() const { return ShouldHaveTypeData() != TypeDataPresence::Never; }
            void Read(BinaryReader& Reader);
            void Write(BinaryWriter& Writer, int32_t ID);
            virtual void ReadTypeData(BinaryReader&)  {}
            virtual void WriteTypeData(BinaryWriter&) {}
            virtual void ResetForRead() {}
            virtual void GetNames(const Entries& Lists);
            virtual void GetIndices(const Entries& Lists);
        };

        template<typename D, uint32_t Id, TypeDataPresence Presence>
        struct RegionImpl : Region {
            static constexpr uint32_t TypeId = Id;
            uint32_t Type() const override { return Id; }
            TypeDataPresence ShouldHaveTypeData() const override { return Presence; }
            void ReadTypeData(BinaryReader& Reader) override {
                Msb::Detail::ReadVisitor V{Reader};
                static_cast<D*>(this)->Fields(V);
            }
            void WriteTypeData(BinaryWriter& Writer) override {
                Msb::Detail::WriteVisitor V{Writer};
                static_cast<D*>(this)->Fields(V);
            }
            void GetNames(const Entries& Lists) override {
                Region::GetNames(Lists);
                Msb::Detail::NameVisitor<Entries> V{Lists};
                static_cast<D*>(this)->Fields(V);
            }
            void GetIndices(const Entries& Lists) override {
                Region::GetIndices(Lists);
                Msb::Detail::IndexVisitor<Entries> V{Lists};
                static_cast<D*>(this)->Fields(V);
            }
        };

        // A section containing points and volumes for various purposes.
        struct PointParam : Msb::Detail::TypedLists<PointParam, Region> {
            // A point where other players invade your world.
            struct InvasionPoint : RegionImpl<InvasionPoint, 1, TypeDataPresence::Always> {
                // Not sure what this does.
                int32_t Priority = 0;
                InvasionPoint() { Name = "Region: InvasionPoint"; }
                template<typename V>
                void Fields(V& F) {
                    F(Priority);
                }
            };

            // Unknown.
            struct EnvironmentMapPoint : RegionImpl<EnvironmentMapPoint, 2, TypeDataPresence::Sometimes> {
                // Whether or not the type data will be written to the file.
                bool SaveTypeData = true;
                // Unknown; observed values 0x80 and 0x100.
                int32_t UnkT00 = 0x80;
                EnvironmentMapPoint() { Name = "Region: EnvironmentMapPoint"; }
                bool DoesHaveTypeData() const override { return SaveTypeData; }
                void ResetForRead() override { SaveTypeData = false; }
                void ReadTypeData(BinaryReader& Reader) override {
                    SaveTypeData = true;
                    RegionImpl::ReadTypeData(Reader);
                }
                template<typename V>
                void Fields(V& F) {
                    F(UnkT00);
                    F.template Const<int32_t>(0);
                }
            };

            // A region that plays a sound while you are in it.
            struct Sound : RegionImpl<Sound, 4, TypeDataPresence::Always> {
                // Types of sound that may be in a Sound region.
                enum class SndType : uint32_t {
                    Environment = 0,  // ambient sounds like wind, creaking, etc.
                    BGM         = 6,  // boss fight music
                    Voice       = 7,  // character voices
                };
                // Type of sound in this region; determines mixing behavior like muffling.
                SndType SoundType = SndType::Environment;
                // ID of the sound to play in this region, or 0 for child regions.
                int32_t SoundID = 0;
                // Names of other Sound regions which extend this one.
                std::array<Msb::Ref<RegionList>, 16> ChildRegionNames;
                Sound() { Name = "Region: Sound"; }
                template<typename V>
                void Fields(V& F) {
                    F(SoundType);
                    F(SoundID);
                    F(ChildRegionNames);
                }
            };

            // A region that plays a special effect.
            struct SFX : RegionImpl<SFX, 5, TypeDataPresence::Always> {
                // The ID of the .fxr file to play in this region.
                int32_t EffectID = -1;
                int32_t UnkT04   = -1;
                // If true, the effect is off by default until enabled by event scripts.
                bool StartDisabled = false;
                SFX() { Name = "Region: SFX"; }
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

            // Unknown exactly what this does.
            struct WindSFX : RegionImpl<WindSFX, 6, TypeDataPresence::Always> {
                // ID of an .fxr file.
                int32_t EffectID = -1;
                // Name of a corresponding WindArea region.
                Msb::Ref<RegionList> WindAreaName;
                WindSFX() { Name = "Region: WindSFX"; }
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

            // A region where players enter the map.
            struct SpawnPoint : RegionImpl<SpawnPoint, 8, TypeDataPresence::Always> {
                int32_t UnkT00 = -1;
                SpawnPoint() { Name = "Region: SpawnPoint"; }
                template<typename V>
                void Fields(V& F) {
                    F(UnkT00);
                    F.Pad(12);
                }
            };

            // An orange developer message.
            struct Message : RegionImpl<Message, 9, TypeDataPresence::Always> {
                // ID of the message's text in the FMGs.
                int16_t MessageID = -1;
                // Unknown. Always 0 or 2.
                int16_t UnkT02 = 0;
                // Whether the message requires Seek Guidance to appear.
                bool Hidden = false;
                Message() { Name = "Region: Message"; }
                template<typename V>
                void Fields(V& F) {
                    F(MessageID);
                    F(UnkT02);
                    F.AsInt32(Hidden);
                }
            };

            // Regions with no type-specific data.
            struct PatrolRoute : RegionImpl<PatrolRoute, 11, TypeDataPresence::Never> {  // a point in a patrol route
                PatrolRoute() { Name = "Region: PatrolRoute"; }
                template<typename V>
                void Fields(V&) {}
            };
            struct MovementPoint : RegionImpl<MovementPoint, 12, TypeDataPresence::Never> {  // unknown
                MovementPoint() { Name = "Region: MovementPoint"; }
                template<typename V>
                void Fields(V&) {}
            };
            struct WarpPoint : RegionImpl<WarpPoint, 13, TypeDataPresence::Never> {  // seems to be used for moving enemies around
                WarpPoint() { Name = "Region: WarpPoint"; }
                template<typename V>
                void Fields(V&) {}
            };
            struct ActivationArea : RegionImpl<ActivationArea, 14, TypeDataPresence::Never> {  // triggers an enemy when entered
                ActivationArea() { Name = "Region: ActivationArea"; }
                template<typename V>
                void Fields(V&) {}
            };
            struct Event : RegionImpl<Event, 15, TypeDataPresence::Never> {  // any kind of region for use with event scripts
                Event() { Name = "Region: Event"; }
                template<typename V>
                void Fields(V&) {}
            };
            struct Logic : RegionImpl<Logic, 0, TypeDataPresence::Never> {  // unknown; only used 3 times in Catacombs
                Logic() { Name = "Region: Logic"; }
                template<typename V>
                void Fields(V&) {}
            };

            // Unknown.
            struct EnvironmentMapEffectBox : RegionImpl<EnvironmentMapEffectBox, 17, TypeDataPresence::Always> {
                float UnkT00   = 0;
                float Compare  = 0;
                bool UnkT08    = false;
                uint8_t UnkT09 = 0;
                int16_t UnkT0A = 0;
                EnvironmentMapEffectBox() { Name = "Region: EnvironmentMapEffectBox"; }
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

            // Unknown; each WindSFX has a reference to a WindArea.
            struct WindArea : RegionImpl<WindArea, 18, TypeDataPresence::Never> {
                WindArea() { Name = "Region: WindArea"; }
                template<typename V>
                void Fields(V&) {}
            };

            // Muffles environmental sound while inside it.
            struct MufflingBox : RegionImpl<MufflingBox, 20, TypeDataPresence::AlwaysNull> {
                int32_t UnkT00 = 0;
                MufflingBox() { Name = "Region: MufflingBox"; }
                template<typename V>
                void Fields(V& F) {
                    F(UnkT00);
                }
            };

            // A region leading into a MufflingBox.
            struct MufflingPortal : RegionImpl<MufflingPortal, 21, TypeDataPresence::AlwaysNull> {
                int32_t UnkT00 = 0;
                MufflingPortal() { Name = "Region: MufflingPortal"; }
                template<typename V>
                void Fields(V& F) {
                    F(UnkT00);
                    F.Pad(12);
                }
            };

            // Most likely an unused region.
            struct Other : RegionImpl<Other, 0xFFFFFFFF, TypeDataPresence::Never> {
                Other() { Name = "Region: Other"; }
                template<typename V>
                void Fields(V&) {}
            };

            std::vector<InvasionPoint> InvasionPoints;
            std::vector<EnvironmentMapPoint> EnvironmentMapPoints;
            std::vector<Sound> Sounds;
            std::vector<SFX> SFXs;
            std::vector<WindSFX> WindSFXs;
            std::vector<SpawnPoint> SpawnPoints;
            std::vector<Message> Messages;
            std::vector<PatrolRoute> PatrolRoutes;
            std::vector<MovementPoint> MovementPoints;
            std::vector<WarpPoint> WarpPoints;
            std::vector<ActivationArea> ActivationAreas;
            std::vector<Event> Events;
            std::vector<Logic> Logics;
            std::vector<EnvironmentMapEffectBox> EnvironmentMapEffectBoxes;
            std::vector<WindArea> WindAreas;
            std::vector<MufflingBox> MufflingBoxes;
            std::vector<MufflingPortal> MufflingPortals;
            std::vector<Other> Others;

            static constexpr int32_t ParamVersion  = 3;
            static constexpr const char* ParamName = "POINT_PARAM_ST";

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
        // Unknown.
        struct SOULS_API Route : Entry {
            int32_t Unk08 = 0;
            int32_t Unk0C = 0;
            static constexpr uint32_t TypeId = 0;
            uint32_t Type() const { return 0; }
            Route() { Name = "XX-XX"; }
            void Read(BinaryReader& Reader);
            void Write(BinaryWriter& Writer, int32_t ID);
        };

        // A section containing routes. Purpose unknown.
        struct RouteParam : Msb::Detail::TypedLists<RouteParam, Route> {
            std::vector<Route> Routes;
            static constexpr int32_t ParamVersion  = 3;
            static constexpr const char* ParamName = "ROUTE_PARAM_ST";
            template<typename F>
            void ForEachList(F&& Visit) {
                Visit(Routes);
            }
            template<typename F>
            void ForEachList(F&& Visit) const {
                Visit(Routes);
            }
        };

        // Unknown; seems to have been related to ceremonies but probably unused in release.
        struct SOULS_API Layer : Entry {
            // Unknown; usually just counts up from 0.
            int32_t Unk08 = 0;
            int32_t Unk0C = 0;
            // Unknown; seems to always be 0.
            int32_t Unk10 = 0;
            static constexpr uint32_t TypeId = 0;
            uint32_t Type() const { return 0; }
            Layer() { Name = "Layer"; }
            void Read(BinaryReader& Reader);
            void Write(BinaryWriter& Writer, int32_t ID);
        };

        // A section containing layers, which probably don't actually do anything.
        struct LayerParam : Msb::Detail::TypedLists<LayerParam, Layer> {
            std::vector<Layer> Layers;
            static constexpr int32_t ParamVersion  = 3;
            static constexpr const char* ParamName = "LAYER_PARAM_ST";
            template<typename F>
            void ForEachList(F&& Visit) {
                Visit(Layers);
            }
            template<typename F>
            void ForEachList(F&& Visit) const {
                Visit(Layers);
            }
        };
#pragma endregion

#pragma region Parts
        // Gparam value IDs for various part types.
        struct GparamConfig {
            // ID of the value set from LightSet ParamEditor to use.
            int32_t LightSetID = 0;
            // ID of the value set from FogParamEditor to use.
            int32_t FogParamID = 0;
            // ID of the value set from LightScattering ParamEditor to use.
            int32_t LightScatteringID = 0;
            // ID of the value set from Env Map Editor to use.
            int32_t EnvMapID = 0;
            template<typename V>
            void Fields(V& F) {
                F(LightSetID);
                F(FogParamID);
                F(LightScatteringID);
                F(EnvMapID);
                F.Pad(0x10);
            }
        };

        // Unknown.
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

        // Any instance of some "thing" in a map.
        struct SOULS_API Part : Entry {
            // Unknown network path to a .sib file.
            std::string SibPath;
            // The name of this part's model.
            Msb::Ref<ModelList> ModelName;
            Vector3 Position;
            Vector3 Rotation;
            // The scale of the part, which only really works right for map pieces.
            Vector3 Scale{1.f, 1.f, 1.f};
            // A bitmask that determines which ceremonies the part appears in.
            uint32_t MapStudioLayer = 0;
            std::array<uint32_t, 8> DrawGroups{0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
            std::array<uint32_t, 8> DispGroups{0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
            std::array<uint32_t, 8> BackreadGroups{0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
            // Used to identify the part in event scripts.
            int32_t EntityID = -1;
            // Used to identify multiple parts with the same ID in event scripts.
            std::array<int32_t, 8> EntityGroups{-1, -1, -1, -1, -1, -1, -1, -1};
            int8_t UnkE04 = 0, UnkE05 = 0, LanternID = 0, LodParamID = 0, UnkE0E = 0;
            bool PointLightShadowSource = false, ShadowSource = false, ShadowDest = false, IsShadowOnly = false,
                 DrawByReflectCam = false, DrawOnlyReflectCam = false, UseDepthBiasFloat = false, DisablePointLightEffect = false;
            int32_t UnkE18 = 0;

            virtual uint32_t Type() const   = 0;
            virtual bool HasGparamConfig() const      = 0;
            virtual bool HasSceneGparamConfig() const = 0;
            void Read(BinaryReader& Reader);
            void Write(BinaryWriter& Writer, int32_t ID);
            virtual void ReadTypeData(BinaryReader& Reader)           = 0;
            virtual void WriteTypeData(BinaryWriter& Writer)          = 0;
            virtual void ReadGparamConfig(BinaryReader&)              {}
            virtual void WriteGparamConfig(BinaryWriter&)             {}
            virtual void ReadSceneGparamConfig(BinaryReader&)         {}
            virtual void WriteSceneGparamConfig(BinaryWriter&)        {}
            virtual void GetNames(const Entries& Lists);
            virtual void GetIndices(const Entries& Lists);
        };

        // Implements the type data functions of a part from its Fields function. A part type that has gparam configs
        // provides Gparam / SceneGparam members and sets the flags.
        template<typename D, uint32_t Id, bool WithGparam, bool WithSceneGparam, typename B = Part>
        struct PartImpl : B {
            static constexpr uint32_t TypeId = Id;
            uint32_t Type() const override { return Id; }
            bool HasGparamConfig() const override { return WithGparam; }
            bool HasSceneGparamConfig() const override { return WithSceneGparam; }
            void ReadTypeData(BinaryReader& Reader) override {
                Msb::Detail::ReadVisitor V{Reader};
                static_cast<D*>(this)->Fields(V);
            }
            void WriteTypeData(BinaryWriter& Writer) override {
                Msb::Detail::WriteVisitor V{Writer};
                static_cast<D*>(this)->Fields(V);
            }
            void ReadGparamConfig(BinaryReader& Reader) override {
                if constexpr (WithGparam) {
                    Msb::Detail::ReadVisitor V{Reader};
                    static_cast<D*>(this)->Gparam.Fields(V);
                }
            }
            void WriteGparamConfig(BinaryWriter& Writer) override {
                if constexpr (WithGparam) {
                    Msb::Detail::WriteVisitor V{Writer};
                    static_cast<D*>(this)->Gparam.Fields(V);
                }
            }
            void ReadSceneGparamConfig(BinaryReader& Reader) override {
                if constexpr (WithSceneGparam) {
                    Msb::Detail::ReadVisitor V{Reader};
                    static_cast<D*>(this)->SceneGparam.Fields(V);
                }
            }
            void WriteSceneGparamConfig(BinaryWriter& Writer) override {
                if constexpr (WithSceneGparam) {
                    Msb::Detail::WriteVisitor V{Writer};
                    static_cast<D*>(this)->SceneGparam.Fields(V);
                }
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
            // Gparam IDs for this object.
            GparamConfig Gparam;
            // Unknown.
            Msb::Ref<PartList> CollisionName;
            uint8_t BreakTerm                = 0;
            bool NetSyncType                 = false;
            bool CollisionFilter             = false;
            bool SetMainObjStructureBooleans = false;
            // Automatically playing animations; only the first is actually used, according to Pav.
            std::array<int16_t, 4> AnimIDs{-1, -1, -1, -1};
            // Value added to the base ModelSfxParam ID; only the first is actually used, according to Pav.
            std::array<int16_t, 4> ModelSfxParamRelativeIDs{-1, -1, -1, -1};

            ObjectBase() { Name = "oXXXXXX_XXXX"; }
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

        // Common base data for enemies and dummy enemies.
        struct EnemyBase : Part {
            // Gparam IDs for this enemy.
            GparamConfig Gparam;
            Msb::Ref<PartList> CollisionName;
            // Controls enemy AI, stats, speech and equipment.
            int32_t ThinkParamID = 0;
            int32_t NPCParamID   = 0;
            int32_t TalkID       = 0;
            int32_t CharaInitID  = 0;
            uint8_t PointMoveType = 0;
            int16_t PlatoonID     = 0;
            // Walk route followed by this enemy.
            Msb::Ref<PatrolInfoList, int16_t> WalkRouteName;
            int32_t BackupEventAnimID = 0;
            int32_t UnkT78            = 0;
            float UnkT84              = 0;

            EnemyBase() { Name = "cXXXX_XXXX"; }
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

        // Instances of various "things" in this MSB.
        struct PartsParam : Msb::Detail::TypedLists<PartsParam, Part> {
            // A static model making up the map.
            struct MapPiece : PartImpl<MapPiece, 0, true, false> {
                GparamConfig Gparam;
                MapPiece() { Name = "mXXXXXX_XXXX"; }
                template<typename V>
                void Fields(V& F) {
                    F.Pad(8);
                }
            };

            // Any dynamic object such as elevators, crates, ladders, etc.
            struct Object : PartImpl<Object, 1, true, false, ObjectBase> {
                template<typename V>
                void Fields(V& F) {
                    ObjectFields(F);
                }
            };

            // Any non-player character, not necessarily hostile.
            struct Enemy : PartImpl<Enemy, 2, true, false, EnemyBase> {
                template<typename V>
                void Fields(V& F) {
                    EnemyFields(F);
                }
            };

            // A player spawn point.
            struct Player : PartImpl<Player, 4, false, false> {
                Player() { Name = "c0000_XXXX"; }
                template<typename V>
                void Fields(V& F) {
                    F.Pad(16);
                }
            };

            // An invisible collision mesh, also used for death planes.
            struct Collision : PartImpl<Collision, 5, true, true> {
                // Amount of reverb to apply to sounds.
                enum class SoundSpace : uint8_t {
                    NoReverb = 0, SmallReverbA = 1, SmallReverbB = 2, MiddleReverbA = 3, MiddleReverbB = 4, LargeReverbA = 5,
                    LargeReverbB = 6, ExtraLargeReverbA = 7, ExtraLargeReverbB = 8,
                };
                // Unknown.
                enum class MapVisiblity : uint8_t {
                    Good      = 0,
                    Dark      = 1,
                    PitchDark = 2,
                };
                GparamConfig Gparam;
                SceneGparamConfig SceneGparam;
                uint8_t HitFilterID = 0;
                // Modifies sounds while the player is touching this collision.
                SoundSpace SoundSpaceType = SoundSpace::NoReverb;
                int16_t EnvLightMapSpotIndex = 0;
                float ReflectPlaneHeight     = 0;
                int16_t MapNameID            = -1;
                bool DisableStart            = false;
                // Disables a bonfire with this entity ID when an enemy is touching this collision.
                int32_t DisableBonfireEntityID = -1;
                int32_t PlayRegionID           = -1;
                int16_t LockCamID1 = 0, LockCamID2 = 0;
                // Unknown. Always refers to another collision part.
                Msb::Ref<PartList> UnkHitName;
                // ID in MapMimicryEstablishmentParam.
                int32_t ChameleonParamID = 0;
                uint8_t UnkT34 = 0, UnkT35 = 0, UnkT36 = 0;
                MapVisiblity MapVisType = MapVisiblity::Good;

                Collision() { Name = "hXXXXXX"; }
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

            // An object that is either unused, or used for a cutscene.
            struct DummyObject : PartImpl<DummyObject, 9, true, false, ObjectBase> {
                template<typename V>
                void Fields(V& F) {
                    ObjectFields(F);
                }
            };

            // An enemy that is either unused, or used for a cutscene.
            struct DummyEnemy : PartImpl<DummyEnemy, 10, true, false, EnemyBase> {
                template<typename V>
                void Fields(V& F) {
                    EnemyFields(F);
                }
            };

            // Determines which collision parts load other maps.
            struct ConnectCollision : PartImpl<ConnectCollision, 11, false, false> {
                // The name of the associated collision part.
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

            std::vector<MapPiece> MapPieces;
            std::vector<Object> Objects;
            std::vector<Enemy> Enemies;
            std::vector<Player> Players;
            std::vector<Collision> Collisions;
            std::vector<DummyObject> DummyObjects;
            std::vector<DummyEnemy> DummyEnemies;
            std::vector<ConnectCollision> ConnectCollisions;

            static constexpr int32_t ParamVersion  = 3;
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

#pragma region Parts poses
        // A set of bone transforms to pose an individual part in the map.
        struct PartsPose {
            // A transform for one bone in a model.
            struct Bone {
                std::string Name = "Master";
                int32_t NameIndex = 0;  // only meaningful while reading and writing
                Vector3 Translation;
                Vector3 Rotation;
                Vector3 Scale{1.f, 1.f, 1.f};
            };

            // The name of the part to pose.
            Msb::Ref<PartList, int16_t> PartName;
            std::vector<Bone> Bones;
        };
#pragma endregion

        // Models in this MSB.
        ModelParam Models;
        // Events in this MSB.
        EventParam Events;
        // Regions in this MSB.
        PointParam Regions;
        // Routes in this MSB.
        std::vector<Route> Routes;
        // Layers in this MSB.
        std::vector<Layer> Layers;
        // Parts in this MSB.
        PartsParam Parts;
        // Fixed poses for parts in this MSB.
        std::vector<PartsPose> PartsPoses;

    protected:
        bool IsImpl(BinaryReader& Reader) override;
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
