//
// Created by Jake Rieger on 10/8/2026.
//

#include "NVA.hpp"

#include <array>
#include <cmath>
#include <functional>

namespace Souls {
    namespace {
        // The unknown sections that can hold map nodes in the file.
        using MapNode = NVA::MapNode;

        // Reads a section header and its entries, then moves past the section.
        template<typename T, typename... Versions>
        void ReadSection(BinaryReader& Reader, int32_t Index, NVA::Section<T>& Section,
                         const std::function<void(int32_t Count, int32_t Version)>& ReadEntries, Versions... Allowed) {
            Reader.Assert<int32_t>(Index);
            Section.Version      = Reader.Assert<int32_t>(static_cast<int32_t>(Allowed)...);
            const int32_t Length = Reader.ReadInt32();
            const int32_t Count  = Reader.ReadInt32();
            const int64_t Start  = Reader.Position();
            ReadEntries(Count, Section.Version);
            Reader.Seek(Start + Length);
        }

        template<typename T>
        void WriteSection(BinaryWriter& Writer, int32_t Index, int32_t Version, size_t Count, const std::function<void()>& WriteEntries) {
            Writer.WriteInt32(Index);
            Writer.WriteInt32(Version);
            Writer.Reserve<int32_t>("SectionLength");
            Writer.WriteInt32(static_cast<int32_t>(Count));
            const int64_t Start = Writer.Position();
            WriteEntries();
            if (Writer.Position() % 0x10 != 0) {
                Writer.Pad(static_cast<size_t>(0x10 - Writer.Position() % 0x10), 0xFF);
            }
            Writer.Fill<int32_t>("SectionLength", static_cast<int32_t>(Writer.Position() - Start));
        }

        float DistanceFromShort(uint16_t S) {
            return S == 0xFFFF ? -1.f : static_cast<float>(S) * 0.01f;
        }

        uint16_t DistanceToShort(float Distance) {
            return static_cast<uint16_t>(Distance == -1.f ? 0xFFFF : std::nearbyint(Distance * 100.f));
        }
    }  // namespace

    bool NVA::IsImpl(BinaryReader& Reader) {
        if (Reader.Length() < 4) {
            return false;
        }
        return Reader.GetASCII(0, 4) == "NVMA";
    }

    void NVA::ReadImpl(BinaryReader& Reader) {
        Reader.Order = Endian::Little;
        Reader.AssertMagic("NVMA");
        Version = static_cast<NVAVersion>(Reader.ReadUInt32());
        Reader.ReadUInt32();  // file size
        Reader.Assert<int32_t>(Version == NVAVersion::OldBloodborne ? 8 : 9);  // section count

        // Navmeshes refer to map nodes by index into a later section, so keep the indices until that is read.
        std::vector<std::pair<int16_t, int16_t>> NodeRanges;
        Navmeshes.Items.clear();
        ReadSection<Navmesh>(Reader, 0, Navmeshes, [&](int32_t Count, int32_t SectionVersion) {
            for (int32_t I = 0; I < Count; ++I) {
                Navmesh N;
                N.Position = Reader.ReadVector3();
                Reader.Assert<float>(1);
                N.Rotation = Reader.ReadVector3();
                Reader.Assert<int32_t>(0);
                N.Scale = Reader.ReadVector3();
                Reader.Assert<int32_t>(0);
                N.NameID                  = Reader.ReadInt32();
                N.ModelID                 = Reader.ReadInt32();
                N.Unk38                   = Reader.ReadInt32();
                Reader.Assert<int32_t>(0);
                N.VertexCount             = Reader.ReadInt32();
                const int32_t NameRefCount = Reader.ReadInt32();
                const int16_t NodesIndex  = Reader.ReadInt16();
                const int16_t NodeCount   = Reader.ReadInt16();
                N.Unk4C                   = Reader.Assert<int32_t>(0, 1) == 1;
                if (SectionVersion < 4) {
                    if (NameRefCount > 16) {
                        throw BinaryException("Name reference count should not exceed 16 in DS3/BB.");
                    }
                    N.NameReferenceIDs = Reader.ReadArray<int32_t>(static_cast<size_t>(NameRefCount));
                    for (int32_t R = 0; R < 16 - NameRefCount; ++R) {
                        Reader.Assert<int32_t>(-1);
                    }
                } else {
                    const int32_t NameRefOffset = Reader.ReadInt32();
                    Reader.Assert<int32_t>(0);
                    Reader.Assert<int32_t>(0);
                    Reader.Assert<int32_t>(0);
                    Reader.StepIn(NameRefOffset);
                    N.NameReferenceIDs = Reader.ReadArray<int32_t>(static_cast<size_t>(NameRefCount));
                    Reader.StepOut();
                }
                NodeRanges.emplace_back(NodesIndex, NodeCount);
                Navmeshes.Items.push_back(std::move(N));
            }
        }, 2, 3, 4);

        Entries1.Items.clear();
        ReadSection<Entry1>(Reader, 1, Entries1, [&](int32_t Count, int32_t) {
            for (int32_t I = 0; I < Count; ++I) {
                Entry1 E;
                E.Unk00 = Reader.ReadInt32();
                Reader.Assert<int32_t>(0);
                Entries1.Items.push_back(E);
            }
        }, 1);

        Entries2.Items.clear();
        ReadSection<Entry2>(Reader, 2, Entries2, [&](int32_t Count, int32_t) {
            for (int32_t I = 0; I < Count; ++I) {
                Entry2 E;
                E.Unk00                       = Reader.ReadInt32();
                const int32_t ReferenceCount  = Reader.ReadInt32();
                E.Unk08                       = Reader.ReadInt32();
                Reader.Assert<int32_t>(0);
                if (ReferenceCount > 64) {
                    throw BinaryException("Entry2 reference count should not exceed 64.");
                }
                for (int32_t R = 0; R < ReferenceCount; ++R) {
                    Reference Ref;
                    Ref.UnkIndex = Reader.ReadInt32();
                    Ref.NameID   = Reader.ReadInt32();
                    E.References.push_back(Ref);
                }
                for (int32_t R = 0; R < 64 - ReferenceCount; ++R) {
                    Reader.Assert<int64_t>(0);
                }
                Entries2.Items.push_back(std::move(E));
            }
        }, 1);

        // Section 3 is empty in all known NVAs.
        NVA::Section<int> Section3;
        ReadSection<int>(Reader, 3, Section3, [&](int32_t Count, int32_t) {
            if (Count != 0) {
                throw BinaryException("Section3 is empty in all known NVAs.");
            }
        }, 1);

        std::vector<std::pair<int32_t, int32_t>> ConnectorRanges;  // points index/count, conditions index/count
        Connectors.Items.clear();
        std::vector<std::array<int32_t, 4>> ConnectorIndices;
        ReadSection<Connector>(Reader, 4, Connectors, [&](int32_t Count, int32_t) {
            for (int32_t I = 0; I < Count; ++I) {
                Connector C;
                C.MainNameID                  = Reader.ReadInt32();
                C.TargetNameID                = Reader.ReadInt32();
                const int32_t PointCount      = Reader.ReadInt32();
                const int32_t ConditionCount  = Reader.ReadInt32();
                const int32_t PointsIndex     = Reader.ReadInt32();
                Reader.Assert<int32_t>(0);
                const int32_t ConditionsIndex = Reader.ReadInt32();
                Reader.Assert<int32_t>(0);
                ConnectorIndices.push_back({PointsIndex, PointCount, ConditionsIndex, ConditionCount});
                Connectors.Items.push_back(std::move(C));
            }
        }, 1);

        NVA::Section<ConnectorPoint> Points;
        ReadSection<ConnectorPoint>(Reader, 5, Points, [&](int32_t Count, int32_t) {
            for (int32_t I = 0; I < Count; ++I) {
                ConnectorPoint P;
                P.Unk00 = Reader.ReadInt32();
                P.Unk04 = Reader.ReadInt32();
                P.Unk08 = Reader.ReadInt32();
                P.Unk0C = Reader.ReadInt32();
                Points.Items.push_back(P);
            }
        }, 1);

        NVA::Section<ConnectorCondition> Conditions;
        ReadSection<ConnectorCondition>(Reader, 6, Conditions, [&](int32_t Count, int32_t) {
            for (int32_t I = 0; I < Count; ++I) {
                ConnectorCondition C;
                C.Condition1 = Reader.ReadInt32();
                C.Condition2 = Reader.ReadInt32();
                Conditions.Items.push_back(C);
            }
        }, 1);

        Entries7.Items.clear();
        ReadSection<Entry7>(Reader, 7, Entries7, [&](int32_t Count, int32_t) {
            for (int32_t I = 0; I < Count; ++I) {
                Entry7 E;
                E.Position = Reader.ReadVector3();
                Reader.Assert<float>(1);
                E.NameID = Reader.ReadInt32();
                E.Unk14  = Reader.ReadInt32();
                E.Unk18 = Reader.ReadInt32();
                Reader.Assert<int32_t>(0);
                Entries7.Items.push_back(E);
            }
        }, 1);

        NVA::Section<MapNode> MapNodes;
        MapNodes.Version = 1;
        if (Version != NVAVersion::OldBloodborne) {
            ReadSection<MapNode>(Reader, 8, MapNodes, [&](int32_t Count, int32_t SectionVersion) {
                for (int32_t I = 0; I < Count; ++I) {
                    MapNode N;
                    N.Position      = Reader.ReadVector3();
                    N.Section0Index = Reader.ReadInt16();
                    N.MainID        = Reader.ReadInt16();
                    if (SectionVersion < 2) {
                        for (const uint16_t S : Reader.ReadArray<uint16_t>(16)) {
                            N.SiblingDistances.push_back(DistanceFromShort(S));
                        }
                    } else {
                        const int32_t SubIDCount   = Reader.ReadInt32();
                        N.Unk14                    = Reader.ReadInt32();
                        const int32_t SubIDsOffset = Reader.ReadInt32();
                        Reader.Assert<int32_t>(0);
                        Reader.StepIn(SubIDsOffset);
                        for (const uint16_t S : Reader.ReadArray<uint16_t>(static_cast<size_t>(SubIDCount))) {
                            N.SiblingDistances.push_back(DistanceFromShort(S));
                        }
                        Reader.StepOut();
                    }
                    MapNodes.Items.push_back(std::move(N));
                }
            }, 1, 2);
        }

        // Hand each navmesh its map nodes, and each connector its points and conditions.
        for (size_t I = 0; I < Navmeshes.Items.size(); ++I) {
            Navmesh& N               = Navmeshes.Items[I];
            const auto [Index, Count] = NodeRanges[I];
            for (int32_t K = 0; K < Count; ++K) {
                const size_t At = static_cast<size_t>(Index) + static_cast<size_t>(K);
                if (At >= MapNodes.Items.size()) {
                    throw BinaryException("Navmesh map node index out of range");
                }
                N.MapNodes.push_back(MapNodes.Items[At]);
            }
            for (MapNode& M : N.MapNodes) {
                if (M.SiblingDistances.size() > N.MapNodes.size()) {
                    M.SiblingDistances.resize(N.MapNodes.size());
                }
            }
        }
        for (size_t I = 0; I < Connectors.Items.size(); ++I) {
            Connector& C = Connectors.Items[I];
            const auto& Idx = ConnectorIndices[I];
            for (int32_t K = 0; K < Idx[1]; ++K) {
                const size_t At = static_cast<size_t>(Idx[0] + K);
                if (At >= Points.Items.size()) throw BinaryException("Connector point index out of range");
                C.Points.push_back(Points.Items[At]);
            }
            for (int32_t K = 0; K < Idx[3]; ++K) {
                const size_t At = static_cast<size_t>(Idx[2] + K);
                if (At >= Conditions.Items.size()) throw BinaryException("Connector condition index out of range");
                C.Conditions.push_back(Conditions.Items[At]);
            }
        }
    }

    void NVA::WriteImpl(BinaryWriter& Writer) {
        // Flatten the per-navmesh and per-connector lists into the shared sections.
        std::vector<ConnectorPoint> Points;
        std::vector<ConnectorCondition> Conditions;
        std::vector<std::array<int32_t, 2>> ConnectorStarts;
        for (const Connector& C : Connectors.Items) {
            ConnectorStarts.push_back({static_cast<int32_t>(Points.size()), static_cast<int32_t>(Conditions.size())});
            Points.insert(Points.end(), C.Points.begin(), C.Points.end());
            Conditions.insert(Conditions.end(), C.Conditions.begin(), C.Conditions.end());
        }
        const int32_t MapNodeVersion = Version == NVAVersion::Sekiro || Version == NVAVersion::EldenRing ? 2 : 1;
        std::vector<const MapNode*> MapNodes;
        std::vector<int16_t> NodeStarts;
        for (const Navmesh& N : Navmeshes.Items) {
            NodeStarts.push_back(static_cast<int16_t>(MapNodes.size()));
            for (const MapNode& M : N.MapNodes) {
                MapNodes.push_back(&M);
            }
        }

        Writer.Order = Endian::Little;
        Writer.WriteMagic("NVMA");
        Writer.WriteUInt32(static_cast<uint32_t>(Version));
        Writer.Reserve<uint32_t>("FileSize");
        Writer.WriteInt32(Version == NVAVersion::OldBloodborne ? 8 : 9);

        WriteSection<Navmesh>(Writer, 0, Navmeshes.Version, Navmeshes.Items.size(), [&] {
            for (size_t I = 0; I < Navmeshes.Items.size(); ++I) {
                const Navmesh& N = Navmeshes.Items[I];
                Writer.WriteVector3(N.Position);
                Writer.WriteFloat(1);
                Writer.WriteVector3(N.Rotation);
                Writer.WriteInt32(0);
                Writer.WriteVector3(N.Scale);
                Writer.WriteInt32(0);
                Writer.WriteInt32(N.NameID);
                Writer.WriteInt32(N.ModelID);
                Writer.WriteInt32(N.Unk38);
                Writer.WriteInt32(0);
                Writer.WriteInt32(N.VertexCount);
                Writer.WriteInt32(static_cast<int32_t>(N.NameReferenceIDs.size()));
                Writer.WriteInt16(NodeStarts[I]);
                Writer.WriteInt16(static_cast<int16_t>(N.MapNodes.size()));
                Writer.WriteInt32(N.Unk4C ? 1 : 0);
                if (Navmeshes.Version < 4) {
                    if (N.NameReferenceIDs.size() > 16) {
                        throw BinaryException("Name reference count should not exceed 16 in DS3/BB.");
                    }
                    Writer.WriteArray(N.NameReferenceIDs);
                    for (size_t R = 0; R < 16 - N.NameReferenceIDs.size(); ++R) {
                        Writer.WriteInt32(-1);
                    }
                } else {
                    Writer.Reserve<int32_t>("NameRefOffset" + std::to_string(I));
                    Writer.WriteInt32(0);
                    Writer.WriteInt32(0);
                    Writer.WriteInt32(0);
                }
            }
            if (Navmeshes.Version >= 4) {
                for (size_t I = 0; I < Navmeshes.Items.size(); ++I) {
                    Writer.Fill<int32_t>("NameRefOffset" + std::to_string(I), static_cast<int32_t>(Writer.Position()));
                    Writer.WriteArray(Navmeshes.Items[I].NameReferenceIDs);
                }
            }
        });

        WriteSection<Entry1>(Writer, 1, Entries1.Version, Entries1.Items.size(), [&] {
            for (const Entry1& E : Entries1.Items) {
                Writer.WriteInt32(E.Unk00);
                Writer.WriteInt32(0);
            }
        });

        WriteSection<Entry2>(Writer, 2, Entries2.Version, Entries2.Items.size(), [&] {
            for (const Entry2& E : Entries2.Items) {
                Writer.WriteInt32(E.Unk00);
                Writer.WriteInt32(static_cast<int32_t>(E.References.size()));
                Writer.WriteInt32(E.Unk08);
                Writer.WriteInt32(0);
                if (E.References.size() > 64) {
                    throw BinaryException("Entry2 reference count should not exceed 64.");
                }
                for (const Reference& R : E.References) {
                    Writer.WriteInt32(R.UnkIndex);
                    Writer.WriteInt32(R.NameID);
                }
                for (size_t R = 0; R < 64 - E.References.size(); ++R) {
                    Writer.WriteInt64(0);
                }
            }
        });

        WriteSection<int>(Writer, 3, 1, 0, [] {});

        WriteSection<Connector>(Writer, 4, Connectors.Version, Connectors.Items.size(), [&] {
            for (size_t I = 0; I < Connectors.Items.size(); ++I) {
                const Connector& C = Connectors.Items[I];
                Writer.WriteInt32(C.MainNameID);
                Writer.WriteInt32(C.TargetNameID);
                Writer.WriteInt32(static_cast<int32_t>(C.Points.size()));
                Writer.WriteInt32(static_cast<int32_t>(C.Conditions.size()));
                Writer.WriteInt32(ConnectorStarts[I][0]);
                Writer.WriteInt32(0);
                Writer.WriteInt32(ConnectorStarts[I][1]);
                Writer.WriteInt32(0);
            }
        });

        WriteSection<ConnectorPoint>(Writer, 5, 1, Points.size(), [&] {
            for (const ConnectorPoint& P : Points) {
                Writer.WriteInt32(P.Unk00);
                Writer.WriteInt32(P.Unk04);
                Writer.WriteInt32(P.Unk08);
                Writer.WriteInt32(P.Unk0C);
            }
        });

        WriteSection<ConnectorCondition>(Writer, 6, 1, Conditions.size(), [&] {
            for (const ConnectorCondition& C : Conditions) {
                Writer.WriteInt32(C.Condition1);
                Writer.WriteInt32(C.Condition2);
            }
        });

        WriteSection<Entry7>(Writer, 7, Entries7.Version, Entries7.Items.size(), [&] {
            for (const Entry7& E : Entries7.Items) {
                Writer.WriteVector3(E.Position);
                Writer.WriteFloat(1);
                Writer.WriteInt32(E.NameID);
                Writer.WriteInt32(E.Unk14);
                Writer.WriteInt32(E.Unk18);
                Writer.WriteInt32(0);
            }
        });

        if (Version != NVAVersion::OldBloodborne) {
            WriteSection<MapNode>(Writer, 8, MapNodeVersion, MapNodes.size(), [&] {
                for (size_t I = 0; I < MapNodes.size(); ++I) {
                    const MapNode& M = *MapNodes[I];
                    Writer.WriteVector3(M.Position);
                    Writer.WriteInt16(M.Section0Index);
                    Writer.WriteInt16(M.MainID);
                    if (MapNodeVersion < 2) {
                        if (M.SiblingDistances.size() > 16) {
                            throw BinaryException("MapNode distance count must not exceed 16 in DS3/BB.");
                        }
                        for (const float Distance : M.SiblingDistances) {
                            Writer.WriteUInt16(DistanceToShort(Distance));
                        }
                        for (size_t R = 0; R < 16 - M.SiblingDistances.size(); ++R) {
                            Writer.WriteUInt16(0xFFFF);
                        }
                    } else {
                        Writer.WriteInt32(static_cast<int32_t>(M.SiblingDistances.size()));
                        Writer.WriteInt32(M.Unk14);
                        Writer.Reserve<int32_t>("SubIDsOffset" + std::to_string(I));
                        Writer.WriteInt32(0);
                    }
                }
                if (MapNodeVersion >= 2) {
                    for (size_t I = 0; I < MapNodes.size(); ++I) {
                        Writer.Fill<int32_t>("SubIDsOffset" + std::to_string(I), static_cast<int32_t>(Writer.Position()));
                        for (const float Distance : MapNodes[I]->SiblingDistances) {
                            Writer.WriteUInt16(DistanceToShort(Distance));
                        }
                    }
                }
            });
        }

        Writer.Fill<uint32_t>("FileSize", static_cast<uint32_t>(Writer.Position()));
    }
}  // namespace Souls
