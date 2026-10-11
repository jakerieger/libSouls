// Copyright (C) 2026 Jake Rieger
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, version 3. This program is distributed in the hope that it will
// be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU General
// Public License along with this program. If not, see https://www.gnu.org/licenses/.

#include "CApiCommon.hpp"

#include <libSouls/Binders/BXFReader.hpp>
#include <libSouls/Crypto.hpp>
#include <libSouls/Formats/BHD5.hpp>
#include <libSouls/Util.hpp>

#include <cstring>
#include <unordered_map>

using namespace Souls;
using namespace Souls::CApi;

struct SoulsBxfReader {
    std::unique_ptr<BXFReader> Reader;
};

struct SoulsBhd5 {
    explicit SoulsBhd5(BHD5 Value) : Header(std::move(Value)) {
        for (size_t B = 0; B < Header.Buckets.size(); ++B) {
            for (size_t F = 0; F < Header.Buckets[B].size(); ++F) {
                ByHash.emplace(Header.Buckets[B][F].FileNameHash, Files.size());
                Files.emplace_back(B, F);
            }
        }
    }
    const BHD5::FileHeader& File(size_t Index) const {
        CheckIndex(Index, Files.size(), "file index out of range");
        return Header.Buckets[Files[Index].first][Files[Index].second];
    }
    BHD5 Header;
    std::vector<std::pair<size_t, size_t>> Files;
    std::unordered_map<uint64_t, size_t> ByHash;
};

struct SoulsBdt {
    std::unique_ptr<BinaryReader> Reader;
};

namespace {
    BHD5::Game ToGame(SoulsBhd5Game Game) {
        switch (Game) {
            case SOULS_BHD5_DARK_SOULS_1:
                return BHD5::Game::DarkSouls1;
            case SOULS_BHD5_DARK_SOULS_2:
                return BHD5::Game::DarkSouls2;
            case SOULS_BHD5_DARK_SOULS_3:
                return BHD5::Game::DarkSouls3;
            case SOULS_BHD5_ELDEN_RING:
                return BHD5::Game::EldenRing;
        }
        ThrowInvalid("invalid game");
    }

    bool StartsWithBhd5(const std::vector<uint8_t>& Data) {
        return Data.size() >= 4 && std::memcmp(Data.data(), "BHD5", 4) == 0;
    }

    template<typename T>
    SoulsBinder* WrapBxf(T Value) {
        auto Result = std::make_unique<SoulsBinder>();
        if constexpr (std::is_same_v<T, BXF3>) {
            Result->Bxf3 = std::make_unique<BXF3>(std::move(Value));
        } else {
            Result->Bxf4 = std::make_unique<BXF4>(std::move(Value));
        }
        return Result.release();
    }

    BXFReader& ReaderOf(const SoulsBxfReader* Handle) {
        return *const_cast<SoulsBxfReader&>(Require(Handle, "reader is NULL")).Reader;
    }

    const BXFReader::FileInfo& InfoAt(const SoulsBxfReader* Handle, size_t Index) {
        BXFReader& R = ReaderOf(Handle);
        CheckIndex(Index, R.FileCount(), "file index out of range");
        return R.File(Index);
    }
}  // namespace

#pragma region BXF
SOULS_C_API SoulsBinder* souls_bxf_read_files(SoulsBinderKind kind, const char* header_path, const char* data_path) {
    return GuardValue<SoulsBinder*>(nullptr, [&]() -> SoulsBinder* {
        const auto H = PathFrom(header_path);
        const auto D = PathFrom(data_path);
        if (kind == SOULS_BINDER_BXF3) return WrapBxf(BXF3::Read(H, D));
        if (kind == SOULS_BINDER_BXF4) return WrapBxf(BXF4::Read(H, D));
        ThrowInvalid("kind must be SOULS_BINDER_BXF3 or SOULS_BINDER_BXF4");
    });
}

SOULS_C_API SoulsBinder* souls_bxf_read_memory(
  SoulsBinderKind kind, const uint8_t* header, size_t header_size, const uint8_t* data, size_t data_size) {
    return GuardValue<SoulsBinder*>(nullptr, [&]() -> SoulsBinder* {
        const auto H = Bytes(header, header_size);
        const auto D = Bytes(data, data_size);
        if (kind == SOULS_BINDER_BXF3) return WrapBxf(BXF3::Read(H, D));
        if (kind == SOULS_BINDER_BXF4) return WrapBxf(BXF4::Read(H, D));
        ThrowInvalid("kind must be SOULS_BINDER_BXF3 or SOULS_BINDER_BXF4");
    });
}

SOULS_C_API SoulsStatus souls_bxf_write_files(SoulsBinder* binder, const char* header_path, const char* data_path) {
    return Guard([&] {
        SoulsBinder& B = Require(binder, "binder is NULL");
        if (!B.IsBxf()) throw Unsupported("The binder is not a split binder (BXF3 or BXF4).");
        const auto H = PathFrom(header_path);
        const auto D = PathFrom(data_path);
        if (B.Bxf3) B.Bxf3->Write(H, D);
        else B.Bxf4->Write(H, D);
    });
}

SOULS_C_API SoulsStatus souls_bxf_write_memory(SoulsBinder* binder, SoulsBuffer* out_header, SoulsBuffer* out_data) {
    return Guard([&] {
        if (!out_header || !out_data) ThrowInvalid("out_header and out_data must not be NULL");
        SoulsBinder& B = Require(binder, "binder is NULL");
        if (!B.IsBxf()) throw Unsupported("The binder is not a split binder (BXF3 or BXF4).");
        const BXFBytes Result = B.Bxf3 ? B.Bxf3->Write() : B.Bxf4->Write();
        *out_header           = MakeBuffer(Result.Header);
        *out_data             = MakeBuffer(Result.Data);
    });
}
#pragma endregion

#pragma region BXFReader
SOULS_C_API SoulsBxfReader* souls_bxf_reader_open(const char* header_path, const char* data_path) {
    return GuardValue<SoulsBxfReader*>(nullptr, [&] {
        auto Result    = std::make_unique<SoulsBxfReader>();
        Result->Reader = std::make_unique<BXFReader>(PathFrom(header_path), PathFrom(data_path));
        return Result.release();
    });
}

SOULS_C_API void souls_bxf_reader_free(SoulsBxfReader* reader) {
    delete reader;
}

SOULS_C_API SoulsBinderKind souls_bxf_reader_kind(const SoulsBxfReader* reader) {
    return GuardValue(SOULS_BINDER_BXF3, [&] {
        return ReaderOf(reader).Kind == BXFReader::Generation::BXF4 ? SOULS_BINDER_BXF4 : SOULS_BINDER_BXF3;
    });
}

SOULS_C_API size_t souls_bxf_reader_file_count(const SoulsBxfReader* reader) {
    return GuardValue<size_t>(0, [&] { return ReaderOf(reader).FileCount(); });
}

SOULS_C_API int32_t souls_bxf_reader_file_id(const SoulsBxfReader* reader, size_t index) {
    return GuardValue<int32_t>(-1, [&] { return InfoAt(reader, index).ID; });
}

SOULS_C_API const char* souls_bxf_reader_file_name(const SoulsBxfReader* reader, size_t index) {
    return GuardValue<const char*>(nullptr, [&]() -> const char* {
        const auto& Info = InfoAt(reader, index);
        return Info.Name ? Info.Name->c_str() : nullptr;
    });
}

SOULS_C_API uint8_t souls_bxf_reader_file_flags(const SoulsBxfReader* reader, size_t index) {
    return GuardValue<uint8_t>(0, [&] { return static_cast<uint8_t>(InfoAt(reader, index).Flags); });
}

SOULS_C_API int64_t souls_bxf_reader_file_stored_size(const SoulsBxfReader* reader, size_t index) {
    return GuardValue<int64_t>(-1, [&] { return InfoAt(reader, index).CompressedSize; });
}

SOULS_C_API int64_t souls_bxf_reader_file_unpacked_size(const SoulsBxfReader* reader, size_t index) {
    return GuardValue<int64_t>(-1, [&] { return InfoAt(reader, index).UncompressedSize; });
}

SOULS_C_API int64_t souls_bxf_reader_find(const SoulsBxfReader* reader, const char* name, int ignore_case) {
    return GuardValue<int64_t>(-1, [&]() -> int64_t {
        if (!name) ThrowInvalid("name is NULL");
        const auto Found = ReaderOf(reader).IndexOf(name, ignore_case != 0);
        return Found ? static_cast<int64_t>(*Found) : -1;
    });
}

SOULS_C_API int64_t souls_bxf_reader_find_file_name(const SoulsBxfReader* reader, const char* file_name) {
    return GuardValue<int64_t>(-1, [&]() -> int64_t {
        if (!file_name) ThrowInvalid("file_name is NULL");
        const auto Found = ReaderOf(reader).IndexOfFileName(file_name);
        return Found ? static_cast<int64_t>(*Found) : -1;
    });
}

SOULS_C_API SoulsBuffer souls_bxf_reader_read(SoulsBxfReader* reader, size_t index) {
    return GuardValue(SoulsBuffer {nullptr, 0}, [&] {
        BXFReader& R = ReaderOf(reader);
        CheckIndex(index, R.FileCount(), "file index out of range");
        return MakeBuffer(R.ReadBytes(index));
    });
}
#pragma endregion

#pragma region BHD5
SOULS_C_API uint32_t souls_path_hash32(const char* path) {
    return GuardValue<uint32_t>(0, [&] {
        if (!path) ThrowInvalid("path is NULL");
        return Util::FromPathHash(path);
    });
}

SOULS_C_API uint64_t souls_path_hash64(const char* path) {
    return GuardValue<uint64_t>(0, [&] {
        if (!path) ThrowInvalid("path is NULL");
        return Util::FromPathHash64(path);
    });
}

SOULS_C_API SoulsBhd5* souls_bhd5_open(SoulsBhd5Game game,
                                       const char* header_path,
                                       const char* exe_path_or_null,
                                       const char* key_pem_or_null) {
    return GuardValue<SoulsBhd5*>(nullptr, [&] {
        const BHD5::Game Format         = ToGame(game);
        const std::vector<uint8_t> File = ReadAllBytes(PathFrom(header_path));
        if (StartsWithBhd5(File)) { return new SoulsBhd5(BHD5::Read(File, Format)); }

        std::vector<std::string> Keys;
        if (key_pem_or_null) Keys.emplace_back(key_pem_or_null);
        if (exe_path_or_null) {
            for (std::string& Key : Crypto::FindRsaPublicKeys(ReadAllBytes(PathFrom(exe_path_or_null)))) {
                Keys.push_back(std::move(Key));
            }
        }
        if (Keys.empty()) {
            throw BinaryException(
              "The header is encrypted and no RSA key was given; pass the game's exe or a PEM key.");
        }
        for (const std::string& Key : Keys) {
            try {
                std::vector<uint8_t> Decrypted = Crypto::DecryptRsaBlocks(Key, File);
                if (StartsWithBhd5(Decrypted)) { return new SoulsBhd5(BHD5::Read(Decrypted, Format)); }
            } catch (const BinaryException&) {
                // The wrong key can produce blocks the crypto library rejects; try the next.
            }
        }
        throw BinaryException("None of the RSA keys decrypts this header.");
    });
}

SOULS_C_API SoulsBhd5* souls_bhd5_read_memory(SoulsBhd5Game game, const uint8_t* data, size_t size) {
    return GuardValue<SoulsBhd5*>(nullptr, [&] { return new SoulsBhd5(BHD5::Read(Bytes(data, size), ToGame(game))); });
}

SOULS_C_API void souls_bhd5_free(SoulsBhd5* bhd5) {
    delete bhd5;
}

SOULS_C_API size_t souls_bhd5_file_count(const SoulsBhd5* bhd5) {
    return GuardValue<size_t>(0, [&] { return Require(bhd5, "bhd5 is NULL").Files.size(); });
}

SOULS_C_API uint64_t souls_bhd5_file_hash(const SoulsBhd5* bhd5, size_t index) {
    return GuardValue<uint64_t>(0, [&] { return Require(bhd5, "bhd5 is NULL").File(index).FileNameHash; });
}

SOULS_C_API int64_t souls_bhd5_file_stored_size(const SoulsBhd5* bhd5, size_t index) {
    return GuardValue<int64_t>(-1, [&] {
        return static_cast<int64_t>(Require(bhd5, "bhd5 is NULL").File(index).PaddedFileSize);
    });
}

SOULS_C_API int64_t souls_bhd5_file_unpacked_size(const SoulsBhd5* bhd5, size_t index) {
    return GuardValue<int64_t>(-1, [&] { return Require(bhd5, "bhd5 is NULL").File(index).UnpaddedFileSize; });
}

SOULS_C_API int64_t souls_bhd5_file_offset(const SoulsBhd5* bhd5, size_t index) {
    return GuardValue<int64_t>(-1, [&] { return Require(bhd5, "bhd5 is NULL").File(index).FileOffset; });
}

SOULS_C_API int souls_bhd5_file_is_encrypted(const SoulsBhd5* bhd5, size_t index) {
    return GuardValue(0, [&] { return Require(bhd5, "bhd5 is NULL").File(index).AES.has_value() ? 1 : 0; });
}

SOULS_C_API int64_t souls_bhd5_find_hash(const SoulsBhd5* bhd5, uint64_t hash) {
    return GuardValue<int64_t>(-1, [&]() -> int64_t {
        const SoulsBhd5& B = Require(bhd5, "bhd5 is NULL");
        const auto Found   = B.ByHash.find(hash);
        return Found == B.ByHash.end() ? -1 : static_cast<int64_t>(Found->second);
    });
}

SOULS_C_API int64_t souls_bhd5_find_path(const SoulsBhd5* bhd5, const char* path) {
    return GuardValue<int64_t>(-1, [&]() -> int64_t {
        if (!path) ThrowInvalid("path is NULL");
        const SoulsBhd5& B = Require(bhd5, "bhd5 is NULL");
        // Elden Ring names files with the 64-bit hash; the older games with the 32-bit one.
        const uint64_t Hash =
          B.Header.Format == BHD5::Game::EldenRing ? Util::FromPathHash64(path) : Util::FromPathHash(path);
        const auto Found = B.ByHash.find(Hash);
        return Found == B.ByHash.end() ? -1 : static_cast<int64_t>(Found->second);
    });
}

SOULS_C_API SoulsBdt* souls_bdt_open(const char* data_path) {
    return GuardValue<SoulsBdt*>(nullptr, [&] {
        auto Result    = std::make_unique<SoulsBdt>();
        Result->Reader = std::make_unique<BinaryReader>(PathFrom(data_path));
        return Result.release();
    });
}

SOULS_C_API void souls_bdt_free(SoulsBdt* bdt) {
    delete bdt;
}

SOULS_C_API SoulsBuffer souls_bhd5_extract(const SoulsBhd5* bhd5, SoulsBdt* bdt, size_t index) {
    return GuardValue(SoulsBuffer {nullptr, 0}, [&] {
        const BHD5::FileHeader& File = Require(bhd5, "bhd5 is NULL").File(index);
        BinaryReader& Reader         = *Require(bdt, "bdt is NULL").Reader;
        return MakeBuffer(File.ReadFile(Reader));
    });
}
#pragma endregion
