//
// Created by Jake Rieger on 10/7/2026.
//

#include "BHD5.hpp"

#include <libSouls/Crypto.hpp>

#include <string_view>

using namespace std::string_view_literals;

namespace Souls {
    namespace {
        std::string Key(const char* Field, size_t Bucket, size_t File) {
            return Field + std::to_string(Bucket) + ":" + std::to_string(File);
        }

        std::vector<BHD5::Range> ReadRanges(BinaryReader& Reader) {
            const int32_t Count = Reader.ReadInt32();
            if (Count < 0 || static_cast<int64_t>(Count) * 16 > Reader.Remaining()) {
                throw BinaryException("Invalid range count " + std::to_string(Count) + " in BHD5");
            }
            std::vector<BHD5::Range> Ranges;
            Ranges.reserve(static_cast<size_t>(Count));
            for (int32_t I = 0; I < Count; ++I) {
                BHD5::Range Range;
                Range.StartOffset = Reader.ReadInt64();
                Range.EndOffset   = Reader.ReadInt64();
                Ranges.push_back(Range);
            }
            return Ranges;
        }

        void WriteRanges(BinaryWriter& Writer, const std::vector<BHD5::Range>& Ranges) {
            Writer.WriteInt32(static_cast<int32_t>(Ranges.size()));
            for (const BHD5::Range& Range : Ranges) {
                Writer.WriteInt64(Range.StartOffset);
                Writer.WriteInt64(Range.EndOffset);
            }
        }

        BHD5::FileHeader ReadFileHeader(BinaryReader& Reader, BHD5::Game Game) {
            BHD5::FileHeader Header;
            int64_t ShaHashOffset = 0;
            int64_t AesKeyOffset  = 0;

            if (Game >= BHD5::Game::EldenRing) {
                Header.FileNameHash     = Reader.ReadUInt64();
                Header.PaddedFileSize   = Reader.ReadInt32();
                Header.UnpaddedFileSize = Reader.ReadInt32();
                Header.FileOffset       = Reader.ReadInt64();
                ShaHashOffset           = Reader.ReadInt64();
                AesKeyOffset            = Reader.ReadInt64();
            } else {
                Header.FileNameHash   = Reader.ReadUInt32();
                Header.PaddedFileSize = Reader.ReadInt32();
                Header.FileOffset     = Reader.ReadInt64();

                if (Game >= BHD5::Game::DarkSouls2) {
                    ShaHashOffset = Reader.ReadInt64();
                    AesKeyOffset  = Reader.ReadInt64();
                }
                if (Game >= BHD5::Game::DarkSouls3) {
                    Header.UnpaddedFileSize = Reader.ReadInt64();
                }
            }

            if (ShaHashOffset != 0) {
                Reader.StepIn(ShaHashOffset);
                BHD5::SHAHash Sha;
                Sha.Hash   = Reader.ReadBytes(32);
                Sha.Ranges = ReadRanges(Reader);
                Header.SHA = std::move(Sha);
                Reader.StepOut();
            }

            if (AesKeyOffset != 0) {
                Reader.StepIn(AesKeyOffset);
                BHD5::AESKey Aes;
                Aes.Key    = Reader.ReadBytes(16);
                Aes.Ranges = ReadRanges(Reader);
                Header.AES = std::move(Aes);
                Reader.StepOut();
            }

            return Header;
        }

        void WriteFileHeader(BinaryWriter& Writer, BHD5::Game Game, const BHD5::FileHeader& Header, size_t BucketIndex, size_t FileIndex) {
            if (Game >= BHD5::Game::EldenRing) {
                Writer.WriteUInt64(Header.FileNameHash);
                Writer.WriteInt32(Header.PaddedFileSize);
                Writer.WriteInt32(static_cast<int32_t>(Header.UnpaddedFileSize));
                Writer.WriteInt64(Header.FileOffset);
                Writer.Reserve<int64_t>(Key("SHAHashOffset", BucketIndex, FileIndex));
                Writer.Reserve<int64_t>(Key("AESKeyOffset", BucketIndex, FileIndex));
            } else {
                Writer.WriteUInt32(static_cast<uint32_t>(Header.FileNameHash));
                Writer.WriteInt32(Header.PaddedFileSize);
                Writer.WriteInt64(Header.FileOffset);

                if (Game >= BHD5::Game::DarkSouls2) {
                    Writer.Reserve<int64_t>(Key("SHAHashOffset", BucketIndex, FileIndex));
                    Writer.Reserve<int64_t>(Key("AESKeyOffset", BucketIndex, FileIndex));
                }
                if (Game >= BHD5::Game::DarkSouls3) {
                    Writer.WriteInt64(Header.UnpaddedFileSize);
                }
            }
        }

        void WriteHashAndKey(BinaryWriter& Writer, BHD5::Game Game, const BHD5::FileHeader& Header, size_t BucketIndex, size_t FileIndex) {
            if (Game < BHD5::Game::DarkSouls2) {
                return;
            }

            auto WriteSha = [&] {
                if (!Header.SHA) {
                    Writer.Fill<int64_t>(Key("SHAHashOffset", BucketIndex, FileIndex), 0);
                    return;
                }
                if (Header.SHA->Hash.size() != 32) {
                    throw BinaryException("SHA hash must be 32 bytes long.");
                }
                Writer.Fill<int64_t>(Key("SHAHashOffset", BucketIndex, FileIndex), Writer.Position());
                Writer.WriteBytes(Header.SHA->Hash);
                WriteRanges(Writer, Header.SHA->Ranges);
            };

            auto WriteAes = [&] {
                if (!Header.AES) {
                    Writer.Fill<int64_t>(Key("AESKeyOffset", BucketIndex, FileIndex), 0);
                    return;
                }
                if (Header.AES->Key.size() != 16) {
                    throw BinaryException("AES key must be 16 bytes long.");
                }
                Writer.Fill<int64_t>(Key("AESKeyOffset", BucketIndex, FileIndex), Writer.Position());
                Writer.WriteBytes(Header.AES->Key);
                WriteRanges(Writer, Header.AES->Ranges);
            };

            // The header slots are always SHA then AES, but Elden Ring's files lay the AES key data out first.
            if (Game >= BHD5::Game::EldenRing) {
                WriteAes();
                WriteSha();
            } else {
                WriteSha();
                WriteAes();
            }
        }
    }  // namespace

    void BHD5::AESKey::Decrypt(std::span<uint8_t> Bytes) const {
        for (const Range& Range : Ranges) {
            if (Range.StartOffset == -1 || Range.EndOffset == -1 || Range.StartOffset == Range.EndOffset) {
                continue;
            }
            if (Range.StartOffset < 0 || Range.EndOffset < Range.StartOffset ||
                static_cast<uint64_t>(Range.EndOffset) > Bytes.size()) {
                throw BinaryException("Encrypted range lies outside the file's data");
            }
            Crypto::DecryptAesEcb(Key,
                                  Bytes.subspan(static_cast<size_t>(Range.StartOffset),
                                                static_cast<size_t>(Range.EndOffset - Range.StartOffset)));
        }
    }

    std::vector<uint8_t> BHD5::FileHeader::ReadFile(BinaryReader& BdtReader) const {
        if (PaddedFileSize < 0) {
            throw BinaryException("Invalid file size in BHD5");
        }
        BdtReader.StepIn(FileOffset);
        std::vector<uint8_t> Bytes = BdtReader.ReadBytes(static_cast<size_t>(PaddedFileSize));
        BdtReader.StepOut();

        if (AES) {
            AES->Decrypt(Bytes);
        }
        return Bytes;
    }

    BHD5 BHD5::Read(BinaryReader& Reader, Game Format) {
        BHD5 Result(Format);

        Reader.AssertMagic("BHD5"sv);
        Result.BigEndian = Reader.Assert<int8_t>(0, -1) == 0;
        Reader.Order     = Result.BigEndian ? Endian::Big : Endian::Little;
        Result.Unk05     = Reader.ReadBool();
        Reader.AssertPattern(2, 0);
        Reader.Assert<int32_t>(1);
        Reader.ReadInt32();  // file size
        const int32_t BucketCount   = Reader.ReadInt32();
        const int32_t BucketsOffset = Reader.ReadInt32();

        if (Format >= Game::DarkSouls2) {
            const int32_t SaltLength = Reader.ReadInt32();
            if (SaltLength < 0) {
                throw BinaryException("Invalid salt length in BHD5");
            }
            Result.Salt = Reader.ReadString(static_cast<size_t>(SaltLength));
        }

        // Each bucket is 8 bytes, so this also rejects absurd counts from corrupt files.
        if (BucketCount < 0 || static_cast<int64_t>(BucketCount) * 8 > Reader.Length()) {
            throw BinaryException("Invalid BHD5 bucket count " + std::to_string(BucketCount));
        }

        Reader.Seek(BucketsOffset);
        Result.Buckets.reserve(static_cast<size_t>(BucketCount));
        for (int32_t I = 0; I < BucketCount; ++I) {
            const int32_t HeaderCount   = Reader.ReadInt32();
            const int32_t HeadersOffset = Reader.ReadInt32();
            if (HeaderCount < 0 || static_cast<int64_t>(HeaderCount) * 0x10 > Reader.Length()) {
                throw BinaryException("Invalid BHD5 file count " + std::to_string(HeaderCount));
            }

            Bucket Files;
            Files.reserve(static_cast<size_t>(HeaderCount));
            Reader.StepIn(HeadersOffset);
            for (int32_t J = 0; J < HeaderCount; ++J) {
                Files.push_back(ReadFileHeader(Reader, Format));
            }
            Reader.StepOut();
            Result.Buckets.push_back(std::move(Files));
        }
        return Result;
    }

    BHD5 BHD5::Read(std::span<const uint8_t> Data, Game Format) {
        BinaryReader Reader(Data);
        return Read(Reader, Format);
    }

    BHD5 BHD5::Read(const std::filesystem::path& Path, Game Format) {
        BinaryReader Reader(Path);
        return Read(Reader, Format);
    }

    void BHD5::Write(BinaryWriter& Writer) {
        Writer.Order = BigEndian ? Endian::Big : Endian::Little;
        Writer.WriteMagic("BHD5"sv);
        Writer.WriteSByte(BigEndian ? 0 : -1);
        Writer.WriteBool(Unk05);
        Writer.Pad(2);
        Writer.WriteInt32(1);
        Writer.Reserve<int32_t>("FileSize");
        Writer.WriteInt32(static_cast<int32_t>(Buckets.size()));
        Writer.Reserve<int32_t>("BucketsOffset");

        if (Format >= Game::DarkSouls2) {
            Writer.WriteInt32(static_cast<int32_t>(Salt.size()));
            Writer.WriteString(Salt, false);
        }

        Writer.Fill<int32_t>("BucketsOffset", static_cast<int32_t>(Writer.Position()));
        for (size_t I = 0; I < Buckets.size(); ++I) {
            Writer.WriteInt32(static_cast<int32_t>(Buckets[I].size()));
            Writer.Reserve<int32_t>("FileHeadersOffset" + std::to_string(I));
        }

        for (size_t I = 0; I < Buckets.size(); ++I) {
            Writer.Fill<int32_t>("FileHeadersOffset" + std::to_string(I), static_cast<int32_t>(Writer.Position()));
            for (size_t J = 0; J < Buckets[I].size(); ++J) {
                WriteFileHeader(Writer, Format, Buckets[I][J], I, J);
            }
        }

        for (size_t I = 0; I < Buckets.size(); ++I) {
            for (size_t J = 0; J < Buckets[I].size(); ++J) {
                WriteHashAndKey(Writer, Format, Buckets[I][J], I, J);
            }
        }

        Writer.Fill<int32_t>("FileSize", static_cast<int32_t>(Writer.Position()));
    }

    std::vector<uint8_t> BHD5::Write() {
        BinaryWriter Writer(Endian::Little);
        Write(Writer);
        return Writer.ToBytes();
    }

    void BHD5::Write(const std::filesystem::path& Path) {
        if (Path.has_parent_path()) {
            std::filesystem::create_directories(Path.parent_path());
        }
        BinaryWriter Writer(Path, Endian::Little);
        Write(Writer);
        Writer.Finish();
    }
}  // namespace Souls
