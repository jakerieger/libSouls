//
// Created by Jake Rieger on 10/8/2026.
//

#pragma once

#include <libSouls/SoulsFile.hpp>

#include <vector>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of an exported class; only touched by this DLL's own code

    // EDGE: a Sekiro file that defines grapple points and hangable edges for a model. Extension: .edge
    class SOULS_API EDGE : public SoulsFile<EDGE> {
    public:
        // Which type of edge an edge is.
        enum class EdgeType : uint8_t {
            Grapple = 1,  // a grapplable point
            Hang    = 2,  // a hangable ledge
            Hug     = 3,  // a huggable wall
        };

        // A grapple point, hangable ledge, or huggable wall.
        struct Edge {
            // The starting point of the edge.
            Vector3 V1;
            // The ending point of the edge.
            Vector3 V2;
            // Only for wires, the point you are actually pulled towards.
            Vector3 V3;
            // Only for wires, unknown, always 1.
            float Unk2C = 0;
            int32_t Unk30 = 0;
            EdgeType Type = EdgeType::Grapple;
            // For wires, a relative ID in WireVariationParam; for walls, unknown.
            uint8_t VariationID = 0;
            uint8_t Unk36       = 0;
        };

        int32_t ID = 0;
        std::vector<Edge> Edges;

    protected:
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
