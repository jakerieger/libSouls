/// @file
/// @author Jake Rieger
/// @date 10/8/2026

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

    /// @brief MSB2: the map layout file of Dark Souls II, both the original and Scholar of the First Sin.
    ///
    /// Extension: .msb
    ///
    /// The original game's files use 32-bit offsets and the Scholar's use 64-bit offsets. Entries refer to each other
    /// by name.
    class SOULS_API MSB2 : public SoulsFile<MSB2> {
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
        /// @brief Tag for references to bone names.
        using BoneNameList  = Msb::BoneNameList;

        /// @brief The different formats of Dark Souls II MSBs.
        enum class MSBFormat {
            DarkSouls2LE,       ///< 32-bit little-endian format for the original game on PC
            DarkSouls2BE,       ///< 32-bit big-endian format for the original game on consoles
            DarkSouls2Scholar,  ///< 64-bit format for Scholar of the First Sin on all platforms
        };

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
                } else {
                    return BoneNames;
                }
            }
        };

#pragma region Models
        /// @brief A model file available for parts to reference.
        struct SOULS_API Model : Entry {
            /// @brief The type number of this entry as stored in the file.
            virtual uint32_t Type() const    = 0;
            /// @brief Whether entries of this type have type-specific data.
            virtual bool HasTypeData() const = 0;
            /// @brief Reads the type-specific data.
            virtual void ReadTypeData(BinaryReader&) {}
            /// @brief Writes the type-specific data.
            virtual void WriteTypeData(BinaryWriter&) {}
            /// @brief Reads the entry from its position in the file.
            void Read(BinaryReader& Reader);
            /// @brief Writes the entry; Index is its position among the entries of the same type.
            void Write(BinaryWriter& Writer, int32_t Index);
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
        };

        /// @brief Models available for parts in the map to use.
        struct ModelParam : Msb::Detail::TypedLists<ModelParam, Model> {
            /// @brief A model for a static piece of visual map geometry.
            struct MapPiece : ModelImpl<MapPiece, 0, false> {
                MapPiece() { Name = "mXXXX"; }
            };
            /// @brief A model for a dynamic or interactible part.
            struct Object : ModelImpl<Object, 1, true> {
                Object() { Name = "oXX_XXXX"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
                template<typename V>
                void Fields(V& F) {
                    F.ConstVarint(0);
                }
            };
            /// @brief A model for a static piece of physical map geometry.
            struct Collision : ModelImpl<Collision, 3, false> {
                Collision() { Name = "hXX_XXXX"; }
            };
            /// @brief A model for an AI navigation mesh.
            struct Navmesh : ModelImpl<Navmesh, 4, false> {
                Navmesh() { Name = "nXX_XXXX"; }
            };

            /// @brief Models for static visual elements.
            std::vector<MapPiece> MapPieces;
            /// @brief Models for dynamic or interactible elements.
            std::vector<Object> Objects;
            /// @brief Models for invisible but physical surfaces.
            std::vector<Collision> Collisions;
            /// @brief Models for AI navigation.
            std::vector<Navmesh> Navmeshes;

            /// @brief The version number this param always has in this game's files.
            static constexpr int32_t ParamVersion  = 5;
            /// @brief The param's name in the file.
            static constexpr const char* ParamName = "MODEL_PARAM_ST";

            /// @brief Calls Visit with each per-type list, in the order the entries are written.
            template<typename F>
            void ForEachList(F&& Visit) {
                Visit(MapPieces);
                Visit(Objects);
                Visit(Collisions);
                Visit(Navmeshes);
            }
            /// @brief Calls Visit with each per-type list, in the order the entries are written.
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
        /// @brief An abstract entity that controls map properties or behaviors.
        struct SOULS_API Event : Entry {
            /// @brief Uniquely identifies the event in the map.
            int32_t EventID = -1;

            /// @brief The type number of this entry as stored in the file.
            virtual uint32_t Type() const = 0;
            /// @brief Reads the entry from its position in the file.
            void Read(BinaryReader& Reader);
            /// @brief Writes the entry; Index is its position among the entries of the same type.
            void Write(BinaryWriter& Writer, int32_t Index);
            /// @brief Reads the type-specific data.
            virtual void ReadTypeData(BinaryReader& Reader)  = 0;
            /// @brief Writes the type-specific data.
            virtual void WriteTypeData(BinaryWriter& Writer) = 0;
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
                Msb::Detail::ReadVisitor V{Reader};
                static_cast<D*>(this)->Fields(V);
            }
            /// @brief Writes the type-specific data.
            void WriteTypeData(BinaryWriter& Writer) override {
                Msb::Detail::WriteVisitor V{Writer};
                static_cast<D*>(this)->Fields(V);
            }
        };

        /// @brief Abstract entities that control map properties or behaviors.
        struct EventParam : Msb::Detail::TypedLists<EventParam, Event> {
            /// @brief Unknown if this does anything.
            struct Light : EventImpl<Light, 1> {
                /// @brief Unknown.
                uint8_t UnkT00 = 0;
                /// @brief Unknown.
                float UnkT04 = 0;
                /// @brief Unknown.
                float UnkT08 = 0;
                /// @brief Unknown.
                Color ColorT0C;
                /// @brief Unknown.
                Color ColorT10;
                /// @brief Unknown.
                float UnkT1C = 0;
                /// @brief Unknown.
                float UnkT20 = 0;
                /// @brief Unknown.
                Color ColorT24;
                /// @brief Unknown.
                Color ColorT28;
                /// @brief Unknown.
                Color ColorT34;
                /// @brief Unknown.
                Color ColorT38;
                /// @brief Unknown.
                Color ColorT3C;
                /// @brief Unknown.
                float UnkT40   = 0;
                /// @brief Unknown.
                uint8_t UnkT44 = 0;
                Light() { Name = "Event: Light"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
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

            /// @brief Unknown if this does anything.
            struct Shadow : EventImpl<Shadow, 2> {
                /// @brief Unknown.
                float UnkT04 = 0;
                /// @brief Unknown.
                float UnkT08 = 0;
                /// @brief Unknown.
                float UnkT0C = 0;
                /// @brief Unknown.
                float UnkT14 = 0;
                /// @brief Unknown.
                float UnkT18 = 0;
                /// @brief Unknown.
                float UnkT20 = 0;
                /// @brief Unknown.
                Color ColorT24;
                Shadow() { Name = "Event: Shadow"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
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

            /// @brief Unknown if this does anything.
            struct Fog : EventImpl<Fog, 3> {
                /// @brief Unknown.
                uint8_t UnkT00 = 0;
                /// @brief Unknown.
                Color ColorT04;
                /// @brief Unknown.
                float UnkT08 = 0;
                /// @brief Unknown.
                float UnkT0C = 0;
                /// @brief Unknown.
                float UnkT10 = 0;
                /// @brief Unknown.
                uint8_t UnkT14 = 0;
                /// @brief Unknown.
                uint8_t UnkT15 = 0;
                /// @brief Unknown.
                uint8_t UnkT16 = 0;
                Fog() { Name = "Event: Fog"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
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

            /// @brief Sets the background color when no models are in the way.
            ///
            /// Should only be one per map.
            struct BGColor : EventImpl<BGColor, 4> {
                /// @brief The background color.
                Color Color_;
                BGColor() { Name = "Event: BGColor"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
                template<typename V>
                void Fields(V& F) {
                    F.RGBA(Color_);
                    F.Pad(0x24);
                }
            };

            /// @brief Sets the origin of the map; already factored into MSB positions, but affects BTL.
            ///
            /// Should only be one per map.
            struct MapOffset : EventImpl<MapOffset, 5> {
                /// @brief The origin of the map.
                Vector3 Translation;
                MapOffset() { Name = "Event: MapOffset"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
                template<typename V>
                void Fields(V& F) {
                    F(Translation);
                    F.Pad(4);
                }
            };

            /// @brief Unknown exactly what this is for.
            struct Warp : EventImpl<Warp, 6> {
                /// @brief Unknown.
                uint8_t UnkT00 = 0;
                /// @brief Presumably the position to be warped to.
                Vector3 Position;
                Warp() { Name = "Event: Warp"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
                template<typename V>
                void Fields(V& F) {
                    F(UnkT00);
                    F.Pad(3);
                    F(Position);
                }
            };

            /// @brief Unknown if this does anything.
            struct CheapMode : EventImpl<CheapMode, 7> {
                /// @brief Unknown.
                int16_t UnkT00 = 0;
                CheapMode() { Name = "Event: CheapMode"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
                template<typename V>
                void Fields(V& F) {
                    F(UnkT00);
                    F.Pad(0xE);
                }
            };

            /// @brief Unknown if these do anything.
            std::vector<Light> Lights;
            /// @brief Unknown if these do anything.
            std::vector<Shadow> Shadows;
            /// @brief Unknown if these do anything.
            std::vector<Fog> Fogs;
            /// @brief Sets the background color when no models are in the way. Should only be one per map.
            std::vector<BGColor> BGColors;
            /// @brief Sets the origin of the map; already factored into MSB positions, but affects BTL. Should only be one per map.
            std::vector<MapOffset> MapOffsets;
            /// @brief Unknown exactly what this is for.
            std::vector<Warp> Warps;
            /// @brief Unknown if these do anything.
            std::vector<CheapMode> CheapModes;

            /// @brief The version number this param always has in this game's files.
            static constexpr int32_t ParamVersion  = 5;
            /// @brief The param's name in the file.
            static constexpr const char* ParamName = "EVENT_PARAM_ST";

            /// @brief Calls Visit with each per-type list, in the order the entries are written.
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
            /// @brief Calls Visit with each per-type list, in the order the entries are written.
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
        /// @brief A point or volume that triggers some behavior.
        struct SOULS_API Region : Entry {
            /// @brief Unknown.
            int16_t Unk08 = 0;
            /// @brief Describes the space encompassed by the region.
            ///
            /// Composite shapes aren't supported in Dark Souls II.
            Msb::Shape Shape = Msb::Shapes::Point{};
            /// @brief Unknown.
            int16_t Unk0E    = 0;
            /// @brief Location of the region.
            Vector3 Position;
            /// @brief Rotation of the region, in degrees.
            Vector3 Rotation;

            /// @brief The type number of this entry as stored in the file.
            virtual uint32_t Type() const    = 0;
            /// @brief Whether entries of this type have type-specific data.
            virtual bool HasTypeData() const = 0;
            /// @brief Reads the entry from its position in the file.
            void Read(BinaryReader& Reader);
            /// @brief Writes the entry; Index is its position among the entries of the same type.
            void Write(BinaryWriter& Writer, int32_t Index);
            /// @brief Reads the type-specific data.
            virtual void ReadTypeData(BinaryReader&) {}
            /// @brief Writes the type-specific data.
            virtual void WriteTypeData(BinaryWriter&) {}
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
        };

        /// @brief Points or volumes that trigger some behavior.
        struct PointParam : Msb::Detail::TypedLists<PointParam, Region> {
            /// @brief Unknown, possibly walk points for enemies.
            struct Region0 : RegionImpl<Region0, 0, false> {
                Region0() { Name = "Region: Region0"; }
            };

            /// @brief Unknown if this does anything.
            struct Light : RegionImpl<Light, 3, true> {
                /// @brief Unknown.
                int32_t UnkT00 = 0;
                /// @brief Unknown.
                Color ColorT04;
                /// @brief Unknown.
                Color ColorT08;
                /// @brief Unknown.
                float UnkT0C = 0;
                Light() { Name = "Region: Light"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
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

            /// @brief Unknown, presumably the default spawn location for a map.
            struct StartPoint : RegionImpl<StartPoint, 5, false> {
                StartPoint() { Name = "Region: StartPoint"; }
            };

            /// @brief A sound effect that plays in a certain area.
            struct Sound : RegionImpl<Sound, 7, true> {
                /// @brief Unknown; possibly sound type.
                int32_t UnkT00 = 0;
                /// @brief ID of the sound to play.
                int32_t SoundID = 0;
                /// @brief Unknown.
                int32_t UnkT08  = 0;
                Sound() { Name = "Region: Sound"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
                template<typename V>
                void Fields(V& F) {
                    F(UnkT00);
                    F(SoundID);
                    F(UnkT08);
                    F.Pad(0x14);
                }
            };

            /// @brief A special effect that plays at a certain region.
            struct SFX : RegionImpl<SFX, 9, true> {
                /// @brief The effect to play at this region.
                int32_t EffectID = 0;
                /// @brief Unknown.
                int32_t UnkT04   = 0;
                SFX() { Name = "Region: SFX"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
                template<typename V>
                void Fields(V& F) {
                    F(EffectID);
                    F(UnkT04);
                    F.Pad(0x18);
                }
            };

            /// @brief Unknown, presumably sets wind speed/direction.
            struct Wind : RegionImpl<Wind, 13, true> {
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
                /// @brief Unknown.
                float UnkT18 = 0;
                Wind() { Name = "Region: Wind"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
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

            /// @brief Unknown, names mention lightmaps and GI.
            struct EnvLight : RegionImpl<EnvLight, 14, true> {
                /// @brief Unknown.
                int32_t UnkT00 = 0;
                /// @brief Unknown.
                float UnkT04 = 0;
                /// @brief Unknown.
                float UnkT08 = 0;
                EnvLight() { Name = "Region: EnvLight"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
                template<typename V>
                void Fields(V& F) {
                    F(UnkT00);
                    F(UnkT04);
                    F(UnkT08);
                    F.Pad(0x14);
                }
            };

            /// @brief Unknown if this does anything.
            struct Fog : RegionImpl<Fog, 15, true> {
                /// @brief Unknown.
                int32_t UnkT00 = 0;
                /// @brief Unknown.
                int32_t UnkT04 = 0;
                Fog() { Name = "Region: Fog"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
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

            /// @brief Unknown, possibly walk points for enemies.
            std::vector<Region0> Region0s;
            /// @brief Unknown if these do anything.
            std::vector<Light> Lights;
            /// @brief Unknown, presumably the default position for spawning into the map.
            std::vector<StartPoint> StartPoints;
            /// @brief Sound effects that play in certain areas.
            std::vector<Sound> Sounds;
            /// @brief Special effects that play at certain areas.
            std::vector<SFX> SFXs;
            /// @brief Unknown, presumably set wind speed/direction.
            std::vector<Wind> Winds;
            /// @brief Unknown, names mention lightmaps and GI.
            std::vector<EnvLight> EnvLights;
            /// @brief Unknown if these do anything.
            std::vector<Fog> Fogs;

            /// @brief The version number this param always has in this game's files.
            static constexpr int32_t ParamVersion  = 5;
            /// @brief The param's name in the file.
            static constexpr const char* ParamName = "POINT_PARAM_ST";

            /// @brief Calls Visit with each per-type list, in the order the entries are written.
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
            /// @brief Calls Visit with each per-type list, in the order the entries are written.
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
        /// @brief A concrete map element.
        struct SOULS_API Part : Entry {
            /// @brief The name of the part's model, referencing ModelParam.
            Msb::Ref<ModelList, int16_t> ModelName;
            /// @brief Location of the part.
            Vector3 Position;
            /// @brief Rotation of the part, in degrees.
            Vector3 Rotation;
            /// @brief Scale of the part; only supported for map pieces and objects.
            Vector3 Scale{1.f, 1.f, 1.f};
            /// @brief Not confirmed; determines when the part is loaded.
            std::array<uint32_t, 4> DrawGroups{0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
            /// @brief Unknown; possibly nvm groups.
            int32_t Unk44 = 0;
            /// @brief Unknown; possibly nvm groups.
            int32_t Unk48 = 0;
            /// @brief Unknown; possibly nvm groups.
            int32_t Unk4C = 0;
            /// @brief Unknown; possibly nvm groups.
            int32_t Unk50 = 0;
            /// @brief Not confirmed; determines when the part is visible.
            std::array<uint32_t, 4> DispGroups{0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
            /// @brief Unknown.
            int32_t Unk64 = 0;
            /// @brief Unknown.
            uint8_t Unk6C = 0;
            /// @brief Unknown.
            uint8_t Unk6E = 0;

            /// @brief The type number of this entry as stored in the file.
            virtual uint32_t Type() const = 0;
            /// @brief Reads the entry from its position in the file.
            void Read(BinaryReader& Reader);
            /// @brief Writes the entry; Index is its position among the entries of the same type.
            void Write(BinaryWriter& Writer, int32_t Index);
            /// @brief Reads the type-specific data.
            virtual void ReadTypeData(BinaryReader& Reader)  = 0;
            /// @brief Writes the type-specific data.
            virtual void WriteTypeData(BinaryWriter& Writer) = 0;
            /// @brief Turns the indices read from the file into the names of the entries they refer to.
            virtual void GetNames(const Entries& Lists);
            /// @brief Turns the names of referenced entries into the indices written to the file.
            virtual void GetIndices(const Entries& Lists);
        };

        /// @brief Implements a part type's type number and type data from its Fields function.
        template<typename D, uint32_t Id>
        struct PartImpl : Part {
            /// @brief The type number entries of this type have in the file.
            static constexpr uint32_t TypeId = Id;
            /// @brief The type number of this entry as stored in the file.
            uint32_t Type() const override { return Id; }
            /// @brief Reads the type-specific data.
            void ReadTypeData(BinaryReader& Reader) override {
                Msb::Detail::ReadVisitor V{Reader};
                static_cast<D*>(this)->Fields(V);
            }
            /// @brief Writes the type-specific data.
            void WriteTypeData(BinaryWriter& Writer) override {
                Msb::Detail::WriteVisitor V{Writer};
                static_cast<D*>(this)->Fields(V);
            }
            /// @brief Turns the indices read from the file into the names of the entries they refer to.
            void GetNames(const Entries& Lists) override {
                Part::GetNames(Lists);
                Msb::Detail::NameVisitor<Entries> V{Lists};
                static_cast<D*>(this)->Fields(V);
            }
            /// @brief Turns the names of referenced entries into the indices written to the file.
            void GetIndices(const Entries& Lists) override {
                Part::GetIndices(Lists);
                Msb::Detail::IndexVisitor<Entries> V{Lists};
                static_cast<D*>(this)->Fields(V);
            }
        };

        /// @brief Concrete map elements.
        struct PartsParam : Msb::Detail::TypedLists<PartsParam, Part> {
            /// @brief A visible but intangible model.
            struct MapPiece : PartImpl<MapPiece, 0> {
                /// @brief Unknown.
                int16_t UnkT00 = 0;
                /// @brief Unknown.
                uint8_t UnkT02 = 0;
                MapPiece() { Name = "mXXXX_XXXX"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
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

            /// @brief A dynamic or interactible element.
            struct Object : PartImpl<Object, 1> {
                /// @brief Unknown.
                int32_t MapObjectInstanceParamID = 0;
                /// @brief Unknown.
                int16_t UnkT04                   = 0;
                Object() { Name = "oXX_XXXX_XXXX"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
                template<typename V>
                void Fields(V& F) {
                    F(MapObjectInstanceParamID);
                    F(UnkT04);
                    F.template Const<int16_t>(0);
                    F.Pad(8);
                }
            };

            /// @brief An invisible but physical surface that controls map loading and graphics settings, among other
            ///        things.
            struct Collision : PartImpl<Collision, 3> {
                /// @brief Unknown.
                int32_t UnkT00 = 0;
                /// @brief Unknown.
                int32_t UnkT04 = 0;
                /// @brief Unknown.
                int32_t UnkT08 = 0;
                /// @brief Unknown.
                int32_t UnkT0C = 0;
                /// @brief Unknown.
                uint8_t UnkT10 = 0;
                /// @brief Unknown.
                uint8_t UnkT11 = 0;
                /// @brief Unknown.
                uint8_t UnkT12 = 0;
                /// @brief Unknown.
                uint8_t UnkT13 = 0;
                /// @brief Unknown.
                uint8_t UnkT14 = 0;
                /// @brief Unknown.
                uint8_t UnkT15 = 0;
                /// @brief Unknown.
                uint8_t UnkT17 = 0;
                /// @brief Unknown.
                int32_t UnkT18 = 0;
                /// @brief Unknown.
                int32_t UnkT1C = 0;
                /// @brief Unknown.
                int32_t UnkT20 = 0;
                /// @brief Unknown.
                uint8_t UnkT26 = 0;
                /// @brief Unknown.
                uint8_t UnkT27 = 0;
                /// @brief Unknown.
                int32_t UnkT28 = 0;
                /// @brief Unknown.
                uint8_t UnkT2C = 0;
                /// @brief Unknown.
                int16_t UnkT2E = 0;
                /// @brief Unknown.
                int32_t UnkT30 = 0;
                /// @brief Unknown.
                uint8_t UnkT35 = 0;
                /// @brief Unknown.
                int16_t UnkT36 = 0;
                /// @brief Unknown.
                int32_t UnkT3C = 0;
                /// @brief Unknown.
                uint8_t UnkT40 = 0;
                /// @brief Unknown.
                int32_t UnkT44 = 0;
                Collision() { Name = "hXX_XXXX_XXXX"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
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

            /// @brief An AI navigation mesh.
            struct Navmesh : PartImpl<Navmesh, 4> {
                /// @brief Unknown; possibly nvm groups.
                int32_t UnkT00 = 0;
                /// @brief Unknown; possibly nvm groups.
                int32_t UnkT04 = 0;
                /// @brief Unknown; possibly nvm groups.
                int32_t UnkT08 = 0;
                /// @brief Unknown; possibly nvm groups.
                int32_t UnkT0C = 0;
                Navmesh() { Name = "nXX_XXXX_XXXX"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
                template<typename V>
                void Fields(V& F) {
                    F(UnkT00);
                    F(UnkT04);
                    F(UnkT08);
                    F(UnkT0C);
                    F.Pad(0x10);
                }
            };

            /// @brief Causes another map to be loaded when standing on the referenced collision.
            struct ConnectCollision : PartImpl<ConnectCollision, 5> {
                /// @brief Name of the referenced collision part.
                Msb::Ref<CollisionList> CollisionName;
                /// @brief The map to load when on this collision.
                std::array<uint8_t, 4> MapID{};
                /// @brief Unknown.
                int32_t UnkT08 = 0;
                /// @brief Unknown.
                uint8_t UnkT0C = 0;
                ConnectCollision() { Name = "hXX_XXXX_XXXX"; }
                /// @brief Visits the type-specific fields in file order; the visitor reads, writes or resolves references.
                template<typename V>
                void Fields(V& F) {
                    F(CollisionName);
                    F(MapID);
                    F(UnkT08);
                    F(UnkT0C);
                    F.Pad(3);
                }
            };

            /// @brief Visible but intangible models.
            std::vector<MapPiece> MapPieces;
            /// @brief Dynamic or interactible elements.
            std::vector<Object> Objects;
            /// @brief Invisible but physical surfaces.
            std::vector<Collision> Collisions;
            /// @brief AI navigation meshes.
            std::vector<Navmesh> Navmeshes;
            /// @brief Connections to other maps.
            std::vector<ConnectCollision> ConnectCollisions;

            /// @brief The version number this param always has in this game's files.
            static constexpr int32_t ParamVersion  = 5;
            /// @brief The param's name in the file.
            static constexpr const char* ParamName = "PARTS_PARAM_ST";

            /// @brief Calls Visit with each per-type list, in the order the entries are written.
            template<typename F>
            void ForEachList(F&& Visit) {
                Visit(MapPieces);
                Visit(Objects);
                Visit(Collisions);
                Visit(Navmeshes);
                Visit(ConnectCollisions);
            }
            /// @brief Calls Visit with each per-type list, in the order the entries are written.
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

        /// @brief A set of bone transforms to pose a rigged object.
        struct PartPose {
            /// @brief A transform for a single bone in an object.
            struct Bone {
                /// @brief The name of the bone to transform.
                std::string Name = "Master";
                int32_t NameIndex = 0;  ///< only meaningful while reading and writing
                /// @brief Translation of the bone.
                Vector3 Translation;
                /// @brief Rotation of the bone, in radians.
                Vector3 Rotation;
                /// @brief Scale of the bone.
                Vector3 Scale{1.f, 1.f, 1.f};
            };

            /// @brief The name of the part to be posed.
            Msb::Ref<PartList, int16_t> PartName;
            /// @brief Transforms for each bone in the object.
            std::vector<Bone> Bones;
        };

        /// @brief The format to use when writing.
        MSBFormat Format = MSBFormat::DarkSouls2Scholar;
        /// @brief Model files available for parts to use.
        ModelParam Models;
        /// @brief Abstract entities that set map properties or control behaviors.
        EventParam Events;
        /// @brief Points or volumes that trigger certain behaviors.
        PointParam Regions;
        /// @brief Concrete entities in the map.
        PartsParam Parts;
        /// @brief Predetermined poses applied to objects such as corpses.
        std::vector<PartPose> PartPoses;

    protected:
        bool IsImpl(BinaryReader& Reader) override;
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
