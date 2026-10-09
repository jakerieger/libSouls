//
// Created by Jake Rieger on 10/8/2026.
//

#pragma once

#include <libSouls/SoulsFile.hpp>

#include <vector>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of an exported class; only touched by this DLL's own code

    // NVA: a file that defines the placement and properties of navmeshes in Bloodborne, Dark Souls III, Sekiro and
    // Elden Ring. Extension: .nva
    class SOULS_API NVA : public SoulsFile<NVA> {
    public:
        // Version of the overall format.
        enum class NVAVersion : uint32_t {
            OldBloodborne = 3,  // used for a single BB test map, m29_03_10_00; has no map node section
            DarkSouls3    = 4,  // and Bloodborne
            Sekiro        = 5,
            EldenRing     = 6,
        };

        // NVA is split up into lists of different types, each with its own version number.
        template<typename T>
        struct Section {
            // A version number indicating the format of the section. Do not change this unless you know what you are doing.
            int32_t Version = 1;
            std::vector<T> Items;
        };

        // Adjacent nodes in an inter-navmesh graph.
        struct MapNode {
            Vector3 Position;
            // Index to a navmesh.
            int16_t Section0Index = 0;
            int16_t MainID        = 0;
            // Unknown. -1 marks unused slots.
            std::vector<float> SiblingDistances;
            // Only present in Sekiro and later.
            int32_t Unk14 = 0;
        };

        // An instance of a navmesh.
        struct Navmesh {
            Vector3 Position;
            // Rotation of the mesh, in radians.
            Vector3 Rotation;
            Vector3 Scale{1.f, 1.f, 1.f};
            int32_t NameID      = 0;
            int32_t ModelID     = 0;
            int32_t Unk38       = 0;
            // Should equal number of vertices in the model file.
            int32_t VertexCount = 0;
            std::vector<int32_t> NameReferenceIDs;
            std::vector<MapNode> MapNodes;
            bool Unk4C = false;
        };

        struct Entry1 {
            // Always 0 in Dark Souls III and Sekiro, sometimes 1 in Bloodborne.
            int32_t Unk00 = 0;
        };

        struct Reference {
            int32_t UnkIndex = 0;
            int32_t NameID   = 0;
        };

        struct Entry2 {
            // Seems to just be the index of this entry.
            int32_t Unk00 = 0;
            // References in this entry; maximum of 64.
            std::vector<Reference> References;
            int32_t Unk08 = -1;
        };

        struct ConnectorPoint {
            int32_t Unk00 = 0, Unk04 = 0, Unk08 = 0, Unk0C = 0;
        };

        struct ConnectorCondition {
            int32_t Condition1 = 0, Condition2 = 0;
        };

        // A connection between two navmeshes.
        struct Connector {
            int32_t MainNameID = 0;
            // The navmesh to be attached.
            int32_t TargetNameID = 0;
            std::vector<ConnectorPoint> Points;
            std::vector<ConnectorCondition> Conditions;
        };

        // Unknown; believed to have something to do with connecting maps.
        struct Entry7 {
            Vector3 Position;
            int32_t NameID = 0;
            // Zero in most files, but not all.
            int32_t Unk14 = 0;
            int32_t Unk18 = 0;
        };

        // The format version of this file.
        NVAVersion Version = NVAVersion::DarkSouls3;
        // Navmesh instances in the map. Version: 2 for Dark Souls III and the BB test map, 3 for BB, 4 for Sekiro.
        Section<Navmesh> Navmeshes{2, {}};
        Section<Entry1> Entries1;
        Section<Entry2> Entries2;
        // Connections between different navmeshes.
        Section<Connector> Connectors;
        Section<Entry7> Entries7;

    protected:
        bool IsImpl(BinaryReader& Reader) override;
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
