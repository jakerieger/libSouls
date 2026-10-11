// Copyright (C) 2026 Jake Rieger
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, version 3. This program is distributed in the hope that it will
// be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU General
// Public License along with this program. If not, see https://www.gnu.org/licenses/.

#pragma once

#include <libSouls/SoulsFile.hpp>

#include <vector>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of an exported class; only touched by this DLL's own code

    /// @brief NVA: a file that defines the placement and properties of navmeshes in Bloodborne, Dark Souls III, Sekiro
    ///        and Elden Ring.
    ///
    /// Extension: .nva
    class SOULS_API NVA : public SoulsFile<NVA> {
    public:
        /// @brief Version of the overall format.
        enum class NVAVersion : uint32_t {
            OldBloodborne = 3,  ///< used for a single BB test map, m29_03_10_00; has no map node section
            DarkSouls3    = 4,  ///< and Bloodborne
            Sekiro        = 5,
            EldenRing     = 6,
        };

        /// @brief NVA is split up into lists of different types, each with its own version number.
        template<typename T>
        struct Section {
            /// @brief A version number indicating the format of the section.
            ///
            /// Do not change this unless you know what you are doing.
            int32_t Version = 1;
            /// @brief The section's entries.
            std::vector<T> Items;
        };

        /// @brief Adjacent nodes in an inter-navmesh graph.
        struct MapNode {
            /// @brief Unknown.
            Vector3 Position;
            /// @brief Index to a navmesh.
            int16_t Section0Index = 0;
            /// @brief Unknown.
            int16_t MainID = 0;
            /// @brief Unknown.
            ///
            /// -1 marks unused slots.
            std::vector<float> SiblingDistances;
            /// @brief Only present in Sekiro and later.
            int32_t Unk14 = 0;
        };

        /// @brief An instance of a navmesh.
        struct Navmesh {
            /// @brief Position of the mesh.
            Vector3 Position;
            /// @brief Rotation of the mesh, in radians.
            Vector3 Rotation;
            /// @brief Scale of the mesh.
            Vector3 Scale {1.f, 1.f, 1.f};
            /// @brief Unknown.
            int32_t NameID = 0;
            /// @brief Unknown.
            int32_t ModelID = 0;
            /// @brief Unknown.
            int32_t Unk38 = 0;
            /// @brief Should equal number of vertices in the model file.
            int32_t VertexCount = 0;
            /// @brief Unknown.
            std::vector<int32_t> NameReferenceIDs;
            /// @brief Adjacent nodes in an inter-navmesh graph.
            std::vector<MapNode> MapNodes;
            /// @brief Unknown.
            bool Unk4C = false;
        };

        /// @brief Unknown.
        struct Entry1 {
            /// @brief Always 0 in Dark Souls III and Sekiro, sometimes 1 in Bloodborne.
            int32_t Unk00 = 0;
        };

        /// @brief Unknown.
        struct Reference {
            /// @brief Unknown.
            int32_t UnkIndex = 0;
            /// @brief Unknown.
            int32_t NameID = 0;
        };

        /// @brief Unknown.
        struct Entry2 {
            /// @brief Seems to just be the index of this entry.
            int32_t Unk00 = 0;
            /// @brief References in this entry; maximum of 64.
            std::vector<Reference> References;
            /// @brief Unknown.
            int32_t Unk08 = -1;
        };

        /// @brief A point used to connect two navmeshes.
        struct ConnectorPoint {
            /// @brief Unknown.
            int32_t Unk00 = 0;
            /// @brief Unknown.
            int32_t Unk04 = 0;
            /// @brief Unknown.
            int32_t Unk08 = 0;
            /// @brief Unknown.
            int32_t Unk0C = 0;
        };

        /// @brief An unknown condition used by a connector.
        struct ConnectorCondition {
            /// @brief Unknown.
            int32_t Condition1 = 0;
            /// @brief Unknown.
            int32_t Condition2 = 0;
        };

        /// @brief A connection between two navmeshes.
        struct Connector {
            /// @brief Unknown.
            int32_t MainNameID = 0;
            /// @brief The navmesh to be attached.
            int32_t TargetNameID = 0;
            /// @brief Points used by this connection.
            std::vector<ConnectorPoint> Points;
            /// @brief Conditions used by this connection.
            std::vector<ConnectorCondition> Conditions;
        };

        /// @brief Unknown; believed to have something to do with connecting maps.
        struct Entry7 {
            /// @brief Unknown.
            Vector3 Position;
            /// @brief Unknown.
            int32_t NameID = 0;
            /// @brief Zero in most files, but not all.
            int32_t Unk14 = 0;
            /// @brief Unknown.
            int32_t Unk18 = 0;
        };

        /// @brief The format version of this file.
        NVAVersion Version = NVAVersion::DarkSouls3;
        /// @brief Navmesh instances in the map.
        ///
        /// Version: 2 for Dark Souls III and the BB test map, 3 for BB, 4 for Sekiro.
        Section<Navmesh> Navmeshes {2, {}};
        /// @brief Unknown.
        Section<Entry1> Entries1;
        /// @brief Unknown.
        Section<Entry2> Entries2;
        /// @brief Connections between different navmeshes.
        Section<Connector> Connectors;
        /// @brief Unknown.
        Section<Entry7> Entries7;

    protected:
        bool IsImpl(BinaryReader& Reader) override;
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
