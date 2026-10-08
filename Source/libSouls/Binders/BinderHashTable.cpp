//
// Created by Jake Rieger on 10/7/2026.
//

#include "BinderHashTable.hpp"

#include <libSouls/Util.hpp>

#include <algorithm>

namespace Souls::BinderHashTable {
    namespace {
        struct PathHash {
            int32_t Index;
            uint32_t Hash;
        };

        struct HashGroup {
            int32_t Index;
            int32_t Length;
        };
    }  // namespace

    void Assert(BinaryReader& Reader) {
        Reader.ReadInt64();  // hashes offset
        Reader.ReadInt32();  // bucket count
        Reader.Assert<uint8_t>(0x10);
        Reader.Assert<uint8_t>(8);
        Reader.Assert<uint8_t>(8);
        Reader.Assert<uint8_t>(0);
    }

    void Write(BinaryWriter& Writer, const std::vector<BinderFileHeader>& Files) {
        uint32_t GroupCount = 0;
        for (uint32_t P = static_cast<uint32_t>(Files.size() / 7); P <= 100000; ++P) {
            if (Util::IsPrime(P)) {
                GroupCount = P;
                break;
            }
        }
        if (GroupCount == 0) {
            throw BinaryException("Could not determine hash group count.");
        }

        std::vector<std::vector<PathHash>> HashLists(GroupCount);
        for (size_t I = 0; I < Files.size(); ++I) {
            if (!Files[I].Name) {
                throw BinaryException("Binder file " + std::to_string(I) + " has no name to hash");
            }
            const PathHash Entry{static_cast<int32_t>(I), Util::FromPathHash(*Files[I].Name)};
            HashLists[Entry.Hash % GroupCount].push_back(Entry);
        }

        // Entries within a group are ordered by hash. Stable, so equal hashes keep file order.
        for (auto& List : HashLists) {
            std::stable_sort(List.begin(), List.end(), [](const PathHash& A, const PathHash& B) { return A.Hash < B.Hash; });
        }

        std::vector<HashGroup> Groups;
        std::vector<PathHash> Hashes;
        int32_t Count = 0;
        for (const auto& List : HashLists) {
            const int32_t Start = Count;
            for (const PathHash& Entry : List) {
                Hashes.push_back(Entry);
                ++Count;
            }
            Groups.push_back({Start, Count - Start});
        }

        Writer.Reserve<int64_t>("HashesOffset");
        Writer.WriteUInt32(GroupCount);

        Writer.WriteByte(0x10);
        Writer.WriteByte(8);
        Writer.WriteByte(8);
        Writer.WriteByte(0);

        for (const HashGroup& Group : Groups) {
            Writer.WriteInt32(Group.Length);
            Writer.WriteInt32(Group.Index);
        }

        Writer.Fill<int64_t>("HashesOffset", Writer.Position());
        for (const PathHash& Entry : Hashes) {
            Writer.WriteUInt32(Entry.Hash);
            Writer.WriteInt32(Entry.Index);
        }
    }
}  // namespace Souls::BinderHashTable
