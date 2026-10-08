//
// Created by Jake Rieger on 10/7/2026.
//

#include "DCX.hpp"
#include <libSouls/Oodle26.hpp>
#include <libSouls/Util.hpp>

#include <zstd.h>

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>

using namespace std::string_view_literals;

namespace Souls::DCX {
    namespace {
        // Reads Size bytes at an absolute position without moving the reader. Empty if out of range.
        std::string GetASCII(BinaryReader& Reader, int64_t Position, size_t Size) {
            if (Position < 0 || Position + static_cast<int64_t>(Size) > Reader.Length()) {
                return {};
            }
            Reader.StepIn(Position);
            std::string Result = Reader.ReadString(Size);
            Reader.StepOut();
            return Result;
        }

        std::vector<uint8_t> GetBytes(BinaryReader& Reader, int64_t Position, int32_t Size) {
            if (Size < 0) {
                throw BinaryException("Negative chunk size in DCX");
            }
            Reader.StepIn(Position);
            std::vector<uint8_t> Result = Reader.ReadBytes(static_cast<size_t>(Size));
            Reader.StepOut();
            return Result;
        }

        // The fixed "DCP" block shared by the DCX_* variants: format tag, level byte, then constants.
        void WriteDCP(BinaryWriter& Writer, std::string_view Format, uint8_t Level) {
            Writer.WriteMagic("DCP\0"sv);
            Writer.WriteMagic(Format);
            Writer.WriteInt32(0x20);
            Writer.WriteByte(Level);
            Writer.Pad(3);
        }

        // Inflates the chunk table shared by DCP_EDGE and DCX_EDGE.
        std::vector<uint8_t> ReadEdgeChunks(BinaryReader& Reader, int32_t ChunkCount, int64_t DataStart,
                                            int32_t UncompressedSize) {
            if (ChunkCount < 0) {
                throw BinaryException("Negative chunk count in EDGE DCX");
            }
            std::vector<uint8_t> Output;
            Output.reserve(static_cast<size_t>(std::max(UncompressedSize, 0)));
            for (int32_t I = 0; I < ChunkCount; ++I) {
                Reader.Assert<int32_t>(0);
                const int32_t Offset = Reader.ReadInt32();
                const int32_t Size   = Reader.ReadInt32();
                const bool Compressed = Reader.Assert<int32_t>(0, 1) == 1;

                const std::vector<uint8_t> Chunk = GetBytes(Reader, DataStart + Offset, Size);
                if (Compressed) {
                    const std::vector<uint8_t> Inflated = Util::InflateRaw(Chunk);
                    Output.insert(Output.end(), Inflated.begin(), Inflated.end());
                } else {
                    Output.insert(Output.end(), Chunk.begin(), Chunk.end());
                }
            }
            if (Output.size() != static_cast<size_t>(UncompressedSize)) {
                throw BinaryException("EDGE DCX decompressed to " + std::to_string(Output.size()) +
                                      " bytes, expected " + std::to_string(UncompressedSize));
            }
            return Output;
        }

#pragma region Decompression
        std::vector<uint8_t> DecompressDCPDFLT(BinaryReader& Br, Type& OutType) {
            Br.AssertMagic("DCP\0"sv);
            Br.AssertMagic("DFLT"sv);
            Br.Assert<int32_t>(0x20);
            Br.Assert<uint8_t>(9);
            Br.AssertPattern(3, 0);
            Br.Assert<int32_t>(0);
            Br.Assert<int32_t>(0);
            Br.Assert<int32_t>(0);
            Br.Assert<int32_t>(0x00010100);

            Br.AssertMagic("DCS\0"sv);
            Br.ReadInt32();  // uncompressed size
            const int32_t CompressedSize = Br.ReadInt32();

            std::vector<uint8_t> Decompressed = Util::ReadZlib(Br, CompressedSize);

            Br.AssertMagic("DCA\0"sv);
            Br.Assert<int32_t>(8);

            OutType = Type::DCP_DFLT;
            return Decompressed;
        }

        std::vector<uint8_t> DecompressDCPEDGE(BinaryReader& Br, Type& OutType) {
            Br.AssertMagic("DCP\0"sv);
            Br.AssertMagic("EDGE"sv);
            Br.Assert<int32_t>(0x20);
            Br.Assert<uint8_t>(9);
            Br.AssertPattern(3, 0);
            Br.Assert<int32_t>(0x10000);
            Br.Assert<int32_t>(0);
            Br.Assert<int32_t>(0);
            Br.Assert<int32_t>(0x00100100);

            Br.AssertMagic("DCS\0"sv);
            const int32_t UncompressedSize = Br.ReadInt32();
            const int32_t CompressedSize   = Br.ReadInt32();
            Br.Assert<int32_t>(0);
            const int64_t DataStart = Br.Position();
            Br.Skip(CompressedSize);

            Br.AssertMagic("DCA\0"sv);
            Br.ReadInt32();  // DCA size
            Br.AssertMagic("EgdT"sv);
            Br.Assert<int32_t>(0x00010000);
            Br.Assert<int32_t>(0x20);
            Br.Assert<int32_t>(0x10);
            Br.Assert<int32_t>(0x10000);
            const int32_t EgdtSize   = Br.ReadInt32();
            const int32_t ChunkCount = Br.ReadInt32();
            Br.Assert<int32_t>(0x100000);

            if (EgdtSize != 0x20 + ChunkCount * 0x10) {
                throw BinaryException("Unexpected EgdT size in EDGE DCX.");
            }

            OutType = Type::DCP_EDGE;
            return ReadEdgeChunks(Br, ChunkCount, DataStart, UncompressedSize);
        }

        std::vector<uint8_t> DecompressDCXEDGE(BinaryReader& Br, Type& OutType) {
            Br.AssertMagic("DCX\0"sv);
            Br.Assert<int32_t>(0x10000);
            Br.Assert<int32_t>(0x18);
            Br.Assert<int32_t>(0x24);
            Br.Assert<int32_t>(0x24);
            const int32_t Unk14 = Br.ReadInt32();

            Br.AssertMagic("DCS\0"sv);
            const int32_t UncompressedSize = Br.ReadInt32();
            Br.ReadInt32();  // compressed size

            Br.AssertMagic("DCP\0"sv);
            Br.AssertMagic("EDGE"sv);
            Br.Assert<int32_t>(0x20);
            Br.Assert<uint8_t>(9);
            Br.AssertPattern(3, 0);
            Br.Assert<int32_t>(0x10000);
            Br.Assert<int32_t>(0);
            Br.Assert<int32_t>(0);
            Br.Assert<int32_t>(0x00100100);

            const int64_t DcaStart = Br.Position();
            Br.AssertMagic("DCA\0"sv);
            const int32_t DcaSize = Br.ReadInt32();
            Br.AssertMagic("EgdT"sv);
            Br.Assert<int32_t>(0x00010100);
            Br.Assert<int32_t>(0x24);
            Br.Assert<int32_t>(0x10);
            Br.Assert<int32_t>(0x10000);
            Br.Assert<int32_t>(UncompressedSize % 0x10000, 0x10000);  // uncompressed size of the last chunk
            const int32_t EgdtSize   = Br.ReadInt32();
            const int32_t ChunkCount = Br.ReadInt32();
            Br.Assert<int32_t>(0x100000);

            if (Unk14 != 0x50 + ChunkCount * 0x10) {
                throw BinaryException("Unexpected unk1 value in EDGE DCX.");
            }
            if (EgdtSize != 0x24 + ChunkCount * 0x10) {
                throw BinaryException("Unexpected EgdT size in EDGE DCX.");
            }

            OutType = Type::DCX_EDGE;
            return ReadEdgeChunks(Br, ChunkCount, DcaStart + DcaSize, UncompressedSize);
        }

        std::vector<uint8_t> DecompressDCXDFLT(BinaryReader& Br, Type& OutType) {
            Br.AssertMagic("DCX\0"sv);
            const int32_t Unk04 = Br.Assert<int32_t>(0x10000, 0x11000);
            Br.Assert<int32_t>(0x18);
            Br.Assert<int32_t>(0x24);
            const int32_t Unk10 = Br.Assert<int32_t>(0x24, 0x44);
            Br.Assert<int32_t>(Unk10 == 0x24 ? 0x2C : 0x4C);

            Br.AssertMagic("DCS\0"sv);
            Br.ReadInt32();  // uncompressed size
            const int32_t CompressedSize = Br.ReadInt32();

            Br.AssertMagic("DCP\0"sv);
            Br.AssertMagic("DFLT"sv);
            Br.Assert<int32_t>(0x20);
            const uint8_t Level = Br.Assert<uint8_t>(8, 9);
            Br.AssertPattern(3, 0);
            Br.Assert<int32_t>(0);
            const uint8_t Unk38 = Br.Assert<uint8_t>(0, 15);
            Br.AssertPattern(3, 0);
            Br.Assert<int32_t>(0);
            Br.Assert<int32_t>(0x00010100);

            Br.AssertMagic("DCA\0"sv);
            Br.ReadInt32();  // compressed header length

            if (Unk04 == 0x10000 && Unk10 == 0x24 && Level == 9 && Unk38 == 0) {
                OutType = Type::DCX_DFLT_10000_24_9;
            } else if (Unk04 == 0x10000 && Unk10 == 0x44 && Level == 9 && Unk38 == 0) {
                OutType = Type::DCX_DFLT_10000_44_9;
            } else if (Unk04 == 0x11000 && Unk10 == 0x44 && Level == 8 && Unk38 == 0) {
                OutType = Type::DCX_DFLT_11000_44_8;
            } else if (Unk04 == 0x11000 && Unk10 == 0x44 && Level == 9 && Unk38 == 0) {
                OutType = Type::DCX_DFLT_11000_44_9;
            } else if (Unk04 == 0x11000 && Unk10 == 0x44 && Level == 9 && Unk38 == 15) {
                OutType = Type::DCX_DFLT_11000_44_9_15;
            } else {
                throw BinaryException("Unimplemented DCX DFLT permutation.");
            }

            return Util::ReadZlib(Br, CompressedSize);
        }

        // Header shared by DCX_KRAK and DCX_ZSTD; returns {uncompressed size, compressed size}.
        std::pair<uint32_t, uint32_t> ReadDCXHeaderModern(BinaryReader& Br, std::string_view Format, uint8_t& Level,
                                                          uint8_t MinLevel, uint8_t MaxLevel) {
            Br.AssertMagic("DCX\0"sv);
            Br.Assert<int32_t>(0x11000);
            Br.Assert<int32_t>(0x18);
            Br.Assert<int32_t>(0x24);
            Br.Assert<int32_t>(0x44);
            Br.Assert<int32_t>(0x4C);

            Br.AssertMagic("DCS\0"sv);
            const uint32_t UncompressedSize = Br.ReadUInt32();
            const uint32_t CompressedSize   = Br.ReadUInt32();

            Br.AssertMagic("DCP\0"sv);
            Br.AssertMagic(Format);
            Br.Assert<int32_t>(0x20);
            Level = Br.ReadByte();
            if (Level < MinLevel || Level > MaxLevel) {
                throw BinaryException("Unexpected compression level " + std::to_string(Level) + " in DCX " +
                                      std::string(Format));
            }
            Br.AssertPattern(3, 0);
            Br.Assert<int32_t>(0);
            Br.Assert<int32_t>(0);
            Br.Assert<int32_t>(0);
            Br.Assert<int32_t>(0x10100);

            Br.AssertMagic("DCA\0"sv);
            Br.Assert<int32_t>(8);
            return {UncompressedSize, CompressedSize};
        }

        std::vector<uint8_t> DecompressDCXKRAK(BinaryReader& Br, Type& OutType) {
            uint8_t Level = 0;
            const auto [UncompressedSize, CompressedSize] = ReadDCXHeaderModern(Br, "KRAK"sv, Level, 6, 9);
            if (Level == 6) {
                OutType = Type::DCX_KRAK_6;
            } else if (Level == 9) {
                OutType = Type::DCX_KRAK_9;
            } else {
                throw BinaryException("Unimplemented DCX KRAK permutation.");
            }

            std::vector<uint8_t> Compressed = Br.ReadBytes(CompressedSize);
            std::vector<uint8_t> Result     = Oodle26::Decompress(Compressed, UncompressedSize);
            if (Result.empty() && UncompressedSize != 0) {
                throw BinaryException("Oodle decompression failed (is the Oodle DLL available?)");
            }
            return Result;
        }

        std::vector<uint8_t> DecompressDCXZSTD(BinaryReader& Br, Type& OutType) {
            uint8_t Level = 0;
            const auto [UncompressedSize, CompressedSize] = ReadDCXHeaderModern(Br, "ZSTD"sv, Level, 21, 21);
            OutType                                       = Type::DCX_ZSTD;

            const std::vector<uint8_t> Compressed = Br.ReadBytes(CompressedSize);
            std::vector<uint8_t> Result(UncompressedSize);
            const size_t Size = ZSTD_decompress(Result.data(), Result.size(), Compressed.data(), Compressed.size());
            if (ZSTD_isError(Size)) {
                throw BinaryException(std::string("Zstd decompression failed: ") + ZSTD_getErrorName(Size));
            }
            Result.resize(Size);
            return Result;
        }
#pragma endregion

#pragma region Compression
        void CompressDCPDFLT(std::span<const uint8_t> Data, BinaryWriter& Bw) {
            Bw.WriteMagic("DCP\0"sv);
            Bw.WriteMagic("DFLT"sv);
            Bw.WriteInt32(0x20);
            Bw.WriteByte(9);
            Bw.Pad(3);
            Bw.WriteInt32(0);
            Bw.WriteInt32(0);
            Bw.WriteInt32(0);
            Bw.WriteInt32(0x00010100);

            Bw.WriteMagic("DCS\0"sv);
            Bw.WriteInt32(static_cast<int32_t>(Data.size()));
            Bw.Reserve<int32_t>("CompressedSize");

            const int CompressedSize = Util::WriteZlib(Bw, 0xDA, Data, 9);
            Bw.Fill<int32_t>("CompressedSize", CompressedSize);

            Bw.WriteMagic("DCA\0"sv);
            Bw.WriteInt32(8);
        }

        void CompressDCXEDGE(std::span<const uint8_t> Data, BinaryWriter& Bw) {
            constexpr size_t ChunkSize = 0x10000;
            const int32_t ChunkCount   = static_cast<int32_t>((Data.size() + ChunkSize - 1) / ChunkSize);
            // Uncompressed size of the final chunk: a full chunk when the data is an exact multiple.
            const int32_t TrailingSize = Data.empty() ? 0 : static_cast<int32_t>(Data.size() - (ChunkCount - 1) * ChunkSize);

            Bw.WriteMagic("DCX\0"sv);
            Bw.WriteInt32(0x10000);
            Bw.WriteInt32(0x18);
            Bw.WriteInt32(0x24);
            Bw.WriteInt32(0x24);
            Bw.WriteInt32(0x50 + ChunkCount * 0x10);

            Bw.WriteMagic("DCS\0"sv);
            Bw.WriteInt32(static_cast<int32_t>(Data.size()));
            Bw.Reserve<int32_t>("CompressedSize");

            Bw.WriteMagic("DCP\0"sv);
            Bw.WriteMagic("EDGE"sv);
            Bw.WriteInt32(0x20);
            Bw.WriteByte(9);
            Bw.Pad(3);
            Bw.WriteInt32(0x10000);
            Bw.WriteInt32(0);
            Bw.WriteInt32(0);
            Bw.WriteInt32(0x00100100);

            const int64_t DcaStart = Bw.Position();
            Bw.WriteMagic("DCA\0"sv);
            Bw.Reserve<int32_t>("DCASize");
            const int64_t EgdtStart = Bw.Position();
            Bw.WriteMagic("EgdT"sv);
            Bw.WriteInt32(0x00010100);
            Bw.WriteInt32(0x24);
            Bw.WriteInt32(0x10);
            Bw.WriteInt32(0x10000);
            Bw.WriteInt32(TrailingSize);
            Bw.Reserve<int32_t>("EGDTSize");
            Bw.WriteInt32(ChunkCount);
            Bw.WriteInt32(0x100000);

            for (int32_t I = 0; I < ChunkCount; ++I) {
                Bw.WriteInt32(0);
                Bw.Reserve<int32_t>("ChunkOffset" + std::to_string(I));
                Bw.Reserve<int32_t>("ChunkSize" + std::to_string(I));
                Bw.Reserve<int32_t>("ChunkCompressed" + std::to_string(I));
            }

            Bw.Fill<int32_t>("DCASize", static_cast<int32_t>(Bw.Position() - DcaStart));
            Bw.Fill<int32_t>("EGDTSize", static_cast<int32_t>(Bw.Position() - EgdtStart));
            const int64_t DataStart = Bw.Position();

            int32_t CompressedSize = 0;
            for (int32_t I = 0; I < ChunkCount; ++I) {
                const size_t Start = static_cast<size_t>(I) * ChunkSize;
                const auto Raw     = Data.subspan(Start, std::min(ChunkSize, Data.size() - Start));

                std::vector<uint8_t> Deflated = Util::DeflateRaw(Raw);
                const bool Compressed         = Deflated.size() < Raw.size();
                // Incompressible chunks are stored as-is.
                const std::span<const uint8_t> Chunk = Compressed ? std::span<const uint8_t>(Deflated) : Raw;

                const std::string Suffix = std::to_string(I);
                Bw.Fill<int32_t>("ChunkCompressed" + Suffix, Compressed ? 1 : 0);
                Bw.Fill<int32_t>("ChunkOffset" + Suffix, static_cast<int32_t>(Bw.Position() - DataStart));
                Bw.Fill<int32_t>("ChunkSize" + Suffix, static_cast<int32_t>(Chunk.size()));
                Bw.WriteBytes(Chunk);
                Bw.Align(0x10);
                CompressedSize += static_cast<int32_t>(Chunk.size());
            }

            Bw.Fill<int32_t>("CompressedSize", CompressedSize);
        }

        void CompressDCXDFLT(std::span<const uint8_t> Data, BinaryWriter& Bw, Type Compression) {
            const int32_t Unk04 =
              (Compression == Type::DCX_DFLT_10000_24_9 || Compression == Type::DCX_DFLT_10000_44_9) ? 0x10000 : 0x11000;
            const int32_t Unk10 = Compression == Type::DCX_DFLT_10000_24_9 ? 0x24 : 0x44;
            const int32_t Unk14 = Compression == Type::DCX_DFLT_10000_24_9 ? 0x2C : 0x4C;
            const uint8_t Level = Compression == Type::DCX_DFLT_11000_44_8 ? 8 : 9;
            const uint8_t Unk38 = Compression == Type::DCX_DFLT_11000_44_9_15 ? 15 : 0;

            Bw.WriteMagic("DCX\0"sv);
            Bw.WriteInt32(Unk04);
            Bw.WriteInt32(0x18);
            Bw.WriteInt32(0x24);
            Bw.WriteInt32(Unk10);
            Bw.WriteInt32(Unk14);

            Bw.WriteMagic("DCS\0"sv);
            Bw.WriteInt32(static_cast<int32_t>(Data.size()));
            Bw.Reserve<int32_t>("CompressedSize");

            WriteDCP(Bw, "DFLT"sv, Level);
            Bw.WriteInt32(0);
            Bw.WriteByte(Unk38);
            Bw.Pad(3);
            Bw.WriteInt32(0);
            Bw.WriteInt32(0x00010100);

            Bw.WriteMagic("DCA\0"sv);
            Bw.WriteInt32(8);

            const int64_t CompressedStart = Bw.Position();
            Util::WriteZlib(Bw, 0xDA, Data, Level);
            Bw.Fill<int32_t>("CompressedSize", static_cast<int32_t>(Bw.Position() - CompressedStart));
        }

        // Header and payload shared by DCX_KRAK and DCX_ZSTD.
        void WriteDCXModern(BinaryWriter& Bw, size_t UncompressedSize, std::span<const uint8_t> Compressed,
                            std::string_view Format, uint8_t Level) {
            Bw.WriteMagic("DCX\0"sv);
            Bw.WriteInt32(0x11000);
            Bw.WriteInt32(0x18);
            Bw.WriteInt32(0x24);
            Bw.WriteInt32(0x44);
            Bw.WriteInt32(0x4C);

            Bw.WriteMagic("DCS\0"sv);
            Bw.WriteUInt32(static_cast<uint32_t>(UncompressedSize));
            Bw.WriteUInt32(static_cast<uint32_t>(Compressed.size()));

            WriteDCP(Bw, Format, Level);
            Bw.WriteInt32(0);
            Bw.WriteInt32(0);
            Bw.WriteInt32(0);
            Bw.WriteInt32(0x10100);

            Bw.WriteMagic("DCA\0"sv);
            Bw.WriteInt32(8);

            Bw.WriteBytes(Compressed);
            Bw.Align(0x10);
        }

        void CompressDCXKRAK(std::span<const uint8_t> Data, BinaryWriter& Bw, Type Compression) {
            const uint8_t Level = Compression == Type::DCX_KRAK_6 ? 6 : 9;
            std::vector<uint8_t> Source(Data.begin(), Data.end());
            const std::vector<uint8_t> Compressed = Oodle26::Compress(
              Source, Oodle26::OodleLZCompressor::Kraken, static_cast<Oodle26::OodleLZCompressionLevel>(Level));
            if (Compressed.empty() && !Data.empty()) {
                throw BinaryException("Oodle compression failed (is the Oodle DLL available?)");
            }
            WriteDCXModern(Bw, Data.size(), Compressed, "KRAK"sv, Level);
        }

        void CompressDCXZSTD(std::span<const uint8_t> Data, BinaryWriter& Bw) {
            const std::unique_ptr<ZSTD_CCtx, decltype(&ZSTD_freeCCtx)> Context(ZSTD_createCCtx(), &ZSTD_freeCCtx);
            if (!Context) {
                throw BinaryException("Failed to create Zstd context");
            }
            // Parameters match the files the games ship: level 21, no content size in the frame, 64 KiB window.
            ZSTD_CCtx_setParameter(Context.get(), ZSTD_c_compressionLevel, 21);
            ZSTD_CCtx_setParameter(Context.get(), ZSTD_c_contentSizeFlag, 0);
            ZSTD_CCtx_setParameter(Context.get(), ZSTD_c_windowLog, 16);

            std::vector<uint8_t> Compressed(ZSTD_compressBound(Data.size()));
            const size_t Size =
              ZSTD_compress2(Context.get(), Compressed.data(), Compressed.size(), Data.data(), Data.size());
            if (ZSTD_isError(Size)) {
                throw BinaryException(std::string("Zstd compression failed: ") + ZSTD_getErrorName(Size));
            }
            Compressed.resize(Size);
            WriteDCXModern(Bw, Data.size(), Compressed, "ZSTD"sv, 21);
        }
#pragma endregion
    }  // namespace

#pragma region Detection
    bool Is(BinaryReader& Reader) {
        if (Reader.Length() < 4) {
            return false;
        }
        const std::string Magic = GetASCII(Reader, 0, 4);
        return Magic == "DCP\0"sv || Magic == "DCX\0"sv;
    }

    bool Is(std::span<const uint8_t> Data) {
        BinaryReader Reader(Data, Endian::Big);
        return Is(Reader);
    }

    bool Is(const std::filesystem::path& Path) {
        BinaryReader Reader(Path, Endian::Big);
        return Is(Reader);
    }
#pragma endregion

#pragma region Decompress
    std::vector<uint8_t> Decompress(BinaryReader& Reader, Type& OutType) {
        Reader.Order = Endian::Big;
        const std::string Magic = GetASCII(Reader, 0, 4);
        if (Magic == "DCP\0"sv) {
            const std::string Format = GetASCII(Reader, 4, 4);
            if (Format == "DFLT") return DecompressDCPDFLT(Reader, OutType);
            if (Format == "EDGE") return DecompressDCPEDGE(Reader, OutType);
            throw BinaryException("Unimplemented DCP compression format \"" + Format + "\".");
        }
        if (Magic == "DCX\0"sv) {
            const std::string Format = GetASCII(Reader, 0x28, 4);
            if (Format == "DFLT") return DecompressDCXDFLT(Reader, OutType);
            if (Format == "EDGE") return DecompressDCXEDGE(Reader, OutType);
            if (Format == "KRAK") return DecompressDCXKRAK(Reader, OutType);
            if (Format == "ZSTD") return DecompressDCXZSTD(Reader, OutType);
            throw BinaryException("Unimplemented DCX compression format \"" + Format + "\".");
        }

        const int64_t Length = Reader.Length();
        if (Length >= 2) {
            const uint8_t B0 = Reader.ReadAt<uint8_t>(0);
            const uint8_t B1 = Reader.ReadAt<uint8_t>(1);
            if (B0 == 0x78 && (B1 == 0x01 || B1 == 0x5E || B1 == 0x9C || B1 == 0xDA)) {
                OutType = Type::Zlib;
                return Util::ReadZlib(Reader, static_cast<int>(Length));
            }
        }
        throw BinaryException("Could not determine compression format.");
    }

    std::vector<uint8_t> Decompress(std::span<const uint8_t> Data, Type& OutType) {
        BinaryReader Reader(Data, Endian::Big);
        return Decompress(Reader, OutType);
    }

    std::vector<uint8_t> Decompress(std::span<const uint8_t> Data) {
        Type Ignored;
        return Decompress(Data, Ignored);
    }

    std::vector<uint8_t> Decompress(const std::filesystem::path& Path, Type& OutType) {
        BinaryReader Reader(Path, Endian::Big);
        return Decompress(Reader, OutType);
    }

    std::vector<uint8_t> Decompress(const std::filesystem::path& Path) {
        Type Ignored;
        return Decompress(Path, Ignored);
    }
#pragma endregion

    DecompressedReader::DecompressedReader(BinaryReader& Source) {
        if (Is(Source)) {
            std::vector<uint8_t> Bytes = Decompress(Source, Detected);
            Owned                      = std::make_unique<BinaryReader>(std::move(Bytes), Endian::Little);
            Active                     = Owned.get();
        } else {
            Active = &Source;
        }
    }

#pragma region Compress
    void Compress(std::span<const uint8_t> Data, BinaryWriter& Writer, Type Compression) {
        Writer.Order = Endian::Big;
        switch (Compression) {
            case Type::Zlib:
                Util::WriteZlib(Writer, 0xDA, Data, 9);
                break;
            case Type::DCP_DFLT:
                CompressDCPDFLT(Data, Writer);
                break;
            case Type::DCX_EDGE:
                CompressDCXEDGE(Data, Writer);
                break;
            case Type::DCX_DFLT_10000_24_9:
            case Type::DCX_DFLT_10000_44_9:
            case Type::DCX_DFLT_11000_44_8:
            case Type::DCX_DFLT_11000_44_9:
            case Type::DCX_DFLT_11000_44_9_15:
                CompressDCXDFLT(Data, Writer, Compression);
                break;
            case Type::DCX_KRAK_6:
            case Type::DCX_KRAK_9:
                CompressDCXKRAK(Data, Writer, Compression);
                break;
            case Type::DCX_ZSTD:
                CompressDCXZSTD(Data, Writer);
                break;
            case Type::Unknown:
                throw BinaryException("You cannot compress a DCX with an unknown type.");
            default:
                throw BinaryException("Compression for the given type is not implemented.");
        }
    }

    std::vector<uint8_t> Compress(std::span<const uint8_t> Data, Type Compression) {
        BinaryWriter Writer(Endian::Big);
        Compress(Data, Writer, Compression);
        return Writer.ToBytes();
    }

    void Compress(std::span<const uint8_t> Data, Type Compression, const std::filesystem::path& Path) {
        BinaryWriter Writer(Path, Endian::Big);
        Compress(Data, Writer, Compression);
        Writer.Finish();
    }
#pragma endregion
}  // namespace Souls::DCX
