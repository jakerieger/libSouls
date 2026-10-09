//
// Created by Jake Rieger on 10/8/2026.
//

#pragma once

#include "MSBCommon.hpp"

#include <optional>
#include <string>
#include <vector>

// Reading and writing the param containers of the 64-bit MSB formats (Dark Souls II onward): a version number, the
// offsets of the entries, the name of the param as UTF-16, and the offset of the next param.
namespace Souls::Msb::Detail {

    inline void RequireNonZero(int64_t Offset, const char* What) {
        if (Offset == 0) {
            throw BinaryException(std::string(What) + " must not be 0.");
        }
    }

    struct ParamHeader {
        int32_t Version = 0;
        std::vector<int64_t> EntryOffsets;
        int64_t NextParamOffset = 0;
    };

    // Reads a param's header. Throws if ExpectedVersion is given and the version differs, or the name isn't Type.
    inline ParamHeader ReadParamHeader(BinaryReader& Reader, std::optional<int32_t> ExpectedVersion, const char* Type) {
        ParamHeader Header;
        Header.Version = Reader.ReadInt32();
        if (ExpectedVersion && Header.Version != *ExpectedVersion) {
            throw BinaryException("Unexpected MSB param version " + std::to_string(Header.Version));
        }
        const int32_t OffsetCount = Reader.ReadInt32();
        const int64_t NameOffset  = Reader.ReadInt64();
        if (OffsetCount < 1) {
            throw BinaryException("Invalid MSB param entry count");
        }
        Header.EntryOffsets    = Reader.ReadArray<int64_t>(static_cast<size_t>(OffsetCount - 1));
        Header.NextParamOffset = Reader.ReadInt64();
        const std::string Name = Reader.GetUTF16Text(NameOffset);
        if (Name != Type) {
            throw BinaryException(std::string("Expected param \"") + Type + "\", got param \"" + Name + "\"");
        }
        return Header;
    }

    inline void WriteParamHeader(BinaryWriter& Writer, int32_t Version, const char* Type, size_t Count) {
        Writer.WriteInt32(Version);
        Writer.WriteInt32(static_cast<int32_t>(Count) + 1);
        Writer.Reserve<int64_t>("ParamNameOffset");
        for (size_t I = 0; I < Count; ++I) {
            Writer.Reserve<int64_t>("EntryOffset" + std::to_string(I));
        }
        Writer.Reserve<int64_t>("NextParamOffset");
        Writer.Fill<int64_t>("ParamNameOffset", Writer.Position());
        Writer.WriteUTF16Text(Type, true);
        Writer.Align(8);
    }

    // Reads a param whose entries have a type number TypeOffset bytes into each entry (or -1 if single-typed), into
    // the Param's per-type lists. If VersionOut is given, the param's version is stored there.
    template<typename ParamT>
    typename ParamT::FileOrder ReadTypedParam(BinaryReader& Reader, ParamT& Param, int TypeOffset, std::optional<int32_t> ExpectedVersion,
                                              int32_t* VersionOut = nullptr) {
        const ParamHeader Header = ReadParamHeader(Reader, ExpectedVersion, ParamT::ParamName);
        if (VersionOut) {
            *VersionOut = Header.Version;
        }
        typename ParamT::FileOrder Order;
        for (const int64_t Offset : Header.EntryOffsets) {
            Reader.Seek(Offset);
            const uint32_t Type = TypeOffset < 0 ? 0 : Reader.ReadAt<uint32_t>(Reader.Position() + TypeOffset);
            if (!Param.ReadEntryOfType(Type, Reader, Order)) {
                throw BinaryException("Unsupported MSB entry type: " + std::to_string(Type));
            }
        }
        Reader.Seek(Header.NextParamOffset);
        return Order;
    }

    // Writes a param's header and entries; each entry's Type() decides when the per-type ID counter restarts.
    template<typename EntryT>
    void WriteTypedParam(BinaryWriter& Writer, int32_t Version, const char* Type, const std::vector<EntryT*>& Entries) {
        WriteParamHeader(Writer, Version, Type, Entries.size());
        int32_t Id = 0;
        std::optional<uint32_t> CurrentType;
        for (size_t I = 0; I < Entries.size(); ++I) {
            if (!CurrentType || *CurrentType != Entries[I]->Type()) {
                CurrentType = Entries[I]->Type();
                Id          = 0;
            }
            Writer.Fill<int64_t>("EntryOffset" + std::to_string(I), Writer.Position());
            Entries[I]->Write(Writer, Id);
            ++Id;
        }
    }

    template<typename Base, typename Derived>
    std::vector<Base*> Upcast(const std::vector<Derived*>& Items) {
        return std::vector<Base*>(Items.begin(), Items.end());
    }

}  // namespace Souls::Msb::Detail
