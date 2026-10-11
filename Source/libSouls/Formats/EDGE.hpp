/// @file
/// @author Jake Rieger
/// @date 10/8/2026

#pragma once

#include <libSouls/SoulsFile.hpp>

#include <vector>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of an exported class; only touched by this DLL's own code

    /// @brief EDGE: a Sekiro file that defines grapple points and hangable edges for a model.
    ///
    /// Extension: .edge
    class SOULS_API EDGE : public SoulsFile<EDGE> {
    public:
        /// @brief Which type of edge an edge is.
        enum class EdgeType : uint8_t {
            Grapple = 1,  ///< a grapplable point
            Hang    = 2,  ///< a hangable ledge
            Hug     = 3,  ///< a huggable wall
        };

        /// @brief A grapple point, hangable ledge, or huggable wall.
        struct Edge {
            /// @brief The starting point of the edge.
            Vector3 V1;
            /// @brief The ending point of the edge.
            Vector3 V2;
            /// @brief Only for wires, the point you are actually pulled towards.
            Vector3 V3;
            /// @brief Only for wires, unknown, always 1.
            float Unk2C = 0;
            /// @brief Unknown.
            int32_t Unk30 = 0;
            /// @brief What type of edge this is.
            EdgeType Type = EdgeType::Grapple;
            /// @brief For wires, a relative ID in WireVariationParam; for walls, unknown.
            uint8_t VariationID = 0;
            /// @brief Unknown.
            uint8_t Unk36       = 0;
        };

        /// @brief Unknown.
        int32_t ID = 0;
        /// @brief Edges defined in this file.
        std::vector<Edge> Edges;

    protected:
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
