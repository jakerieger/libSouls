//
// Created by Jake Rieger on 10/7/2026.
//

#pragma once

#include "Souls.hpp"

#include <cstdint>
#include <filesystem>
#include <vector>

namespace Souls {
    class SOULS_API Oodle26 {
    public:
        enum class OodleLZCompressor : int32_t {
            Invalid   = -1,
            LZH       = 0,
            LZHLW     = 1,
            LZNIB     = 2,
            None      = 3,
            LZB16     = 4,
            LZBLW     = 5,
            LZA       = 6,
            LZNA      = 7,
            Kraken    = 8,
            Mermaid   = 9,
            BitKnit   = 10,
            Selkie    = 11,
            Hydra     = 12,
            Leviathan = 13,
            Count     = 14,
            Force32   = 0x40000000,
        };

        enum class OodleLZCompressionLevel : int32_t {
            HyperFast4 = -4,
            HyperFast3 = -3,
            HyperFast2 = -2,
            HyperFast1 = -1,
            None       = 0,
            SuperFast  = 1,
            VeryFast   = 2,
            Fast       = 3,
            Normal     = 4,
            Optimal1   = 5,
            Optimal2   = 6,
            Optimal3   = 7,
            Optimal4   = 8,
            Optimal5   = 9,
            HyperFast  = HyperFast1,
            Optimal    = Optimal2,
            Max        = Optimal5,
            Min        = HyperFast4,
            Force32    = 0x40000000,
            Invalid    = Force32,
        };

        // Sets where the Oodle DLL (oo2core_6_win64.dll) is loaded from. The default is the bare file name, which
        // uses the normal Windows DLL search (e.g. next to the executable). Call this before the first Compress or
        // Decompress; returns false, changing nothing, if the DLL is already loaded. A failed earlier load attempt
        // doesn't count, so you can correct the path and try again.
        static bool SetDLLPath(const std::filesystem::path& Path);

        // Whether the DLL can be loaded from the current path (loads it if needed).
        static bool IsAvailable();

        static std::vector<uint8_t>
        Compress(std::vector<uint8_t>& Source, OodleLZCompressor Compressor, OodleLZCompressionLevel Level);
        static std::vector<uint8_t> Decompress(std::vector<uint8_t>& Source, int64_t UncompressedSize);

    private:
        enum class OodleLZCheckCRC : int32_t {
            No      = 0,
            Yes     = 1,
            Force32 = 0x40000000,
        };

        enum class OodleLZFuzzSafe : int32_t {
            No  = 0,
            Yes = 1,
        };

        enum class OodleLZVerbosity : int32_t {
            None    = 0,
            Minimal = 1,
            Some    = 2,
            Lots    = 3,
            Force32 = 0x40000000,
        };

        enum class OodleLZDecodeThreadPhase : int32_t {
            ThreadPhase1   = 1,
            ThreadPhase2   = 2,
            ThreadPhaseAll = 3,
            Unthreaded     = ThreadPhaseAll,
        };

        enum class OodleLZProfile : int32_t {
            Main    = 0,
            Reduced = 1,
            Force32 = 0x40000000,
        };

        // Oodle's OO_BOOL is a 4-byte int, not a C++ bool, so the "bool" fields are int32_t here.
        struct CompressOptions {
            uint32_t Verbosity;
            int32_t MinMatchLen;
            int32_t SeekChunkReset;
            int32_t SeekChunkLen;
            OodleLZProfile Profile;
            int32_t DictionarySize;
            int32_t SpaceSpeedTradeoffBytes;
            int32_t MaxHuffmansPerChunk;
            int32_t SendQuantumCRCs;
            int32_t MaxLocalDictionarySize;
            int32_t MakeLongRangeMatcher;
            int32_t MatchTableSizeLog2;
        };

#pragma region DLL Functions
        using OodleLZ_Compress_T = int64_t(__stdcall*)(OodleLZCompressor Compressor,
                                                       const void* RawBuffer,
                                                       int64_t RawLen,
                                                       void* CompressedBuffer,
                                                       OodleLZCompressionLevel Level,
                                                       const void* Options,
                                                       const void* DictionaryBase,
                                                       const void* Lrm,
                                                       void* ScratchMemory,
                                                       int64_t ScratchLen);

        using OodleLZ_CompressOptions_GetDefault_T = const CompressOptions*(__cdecl*)(OodleLZCompressor Compressor,
                                                                                      OodleLZCompressionLevel Level);

        using OodleLZ_Decompress_T = int64_t(__stdcall*)(void* CompressedBuffer,
                                                         int64_t CompressedLen,
                                                         void* RawBuffer,
                                                         int64_t RawLen,
                                                         OodleLZFuzzSafe FuzzSafe,
                                                         OodleLZCheckCRC CheckCRC,
                                                         OodleLZVerbosity Verbosity,
                                                         intptr_t DecompressedBufferBase,
                                                         int64_t DecompressBufferLen,
                                                         intptr_t Callback,
                                                         intptr_t CallbackUserData,
                                                         intptr_t DecoderMemory,
                                                         int64_t DecoderMemoryLen,
                                                         OodleLZDecodeThreadPhase ThreadPhase);

        using OodleLZ_GetCompressedBufferSizeNeeded_T = int64_t(__stdcall*)(int64_t RawLen);

        using OodleLZ_GetDecodeBufferSize_T = int64_t(__stdcall*)(int64_t RawLen, int32_t CorruptionPossible);
#pragma endregion

        // Function pointers resolved once from the DLL, which then stays loaded for the life of the process.
        struct Api {
            OodleLZ_Compress_T Compress                                           = nullptr;
            OodleLZ_CompressOptions_GetDefault_T CompressOptions_GetDefault       = nullptr;
            OodleLZ_Decompress_T Decompress                                       = nullptr;
            OodleLZ_GetCompressedBufferSizeNeeded_T GetCompressedBufferSizeNeeded = nullptr;
            OodleLZ_GetDecodeBufferSize_T GetDecodeBufferSize                     = nullptr;
            bool Loaded                                                           = false;
        };

        // Loader state (DLL path, load result, lock); defined in Oodle26.cpp.
        struct State;
        static State& GetState();

        // Returns nullptr if the DLL or any of its functions couldn't be loaded.
        static const Api* GetApi();

        static int64_t OodleLZ_Compress(OodleLZCompressor Compressor,
                                        void* RawBuffer,
                                        int64_t RawLen,
                                        void* CompressedBuffer,
                                        OodleLZCompressionLevel Level,
                                        void* Options        = nullptr,
                                        void* DictionaryBase = nullptr,
                                        void* Lrm            = nullptr,
                                        void* ScratchMemory  = nullptr,
                                        int64_t ScratchLen   = 0);

        static int64_t OodleLZ_GetCompressedBufferSizeNeeded(int64_t RawLen);

        static int64_t OodleLZ_GetDecodeBufferSize(int64_t RawLen, bool CorruptionPossible);

        static const CompressOptions*
        OodleLZ_CompressOptions_GetDefault(OodleLZCompressor Compressor  = OodleLZCompressor::Invalid,
                                           OodleLZCompressionLevel Level = OodleLZCompressionLevel::Normal);

        static int64_t OodleLZ_Decompress(void* CompressedBuffer,
                                          int64_t CompressedLen,
                                          void* RawBuffer,
                                          int64_t RawLen,
                                          OodleLZFuzzSafe FuzzSafe             = OodleLZFuzzSafe::Yes,
                                          OodleLZCheckCRC CheckCRC             = OodleLZCheckCRC::No,
                                          OodleLZVerbosity Verbosity           = OodleLZVerbosity::None,
                                          intptr_t DecompressedBufferBase      = 0,
                                          int64_t DecompressBufferLen          = 0,
                                          intptr_t Callback                    = 0,
                                          intptr_t CallbackUserData            = 0,
                                          intptr_t DecoderMemory               = 0,
                                          int64_t DecoderMemoryLen             = 0,
                                          OodleLZDecodeThreadPhase ThreadPhase = OodleLZDecodeThreadPhase::Unthreaded);
    };
}  // namespace Souls