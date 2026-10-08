//
// Created by Jake Rieger on 10/7/2026.
//

#include "Oodle26.hpp"

#include <Windows.h>
#include <cstdio>
#include <mutex>
#include <vector>

namespace Souls {
    namespace {
        template<typename T>
        T LoadProc(HMODULE Module, const char* Name) {
            auto Proc = reinterpret_cast<T>(::GetProcAddress(Module, Name));
            if (!Proc) std::fprintf(stderr, "Failed to find %s\n", Name);
            return Proc;
        }
    }  // namespace

    struct Oodle26::State {
        std::mutex Lock;
        std::filesystem::path Path = L"oo2core_6_win64.dll";
        Api Instance {};
        bool Attempted = false;
    };

    Oodle26::State& Oodle26::GetState() {
        static State Instance;
        return Instance;
    }

    bool Oodle26::SetDLLPath(const std::filesystem::path& Path) {
        State& S = GetState();
        std::lock_guard Guard(S.Lock);
        if (S.Instance.Loaded) return false;

        S.Path      = Path;
        S.Attempted = false;
        return true;
    }

    bool Oodle26::IsAvailable() {
        return GetApi() != nullptr;
    }

    const Oodle26::Api* Oodle26::GetApi() {
        State& S = GetState();
        std::lock_guard Guard(S.Lock);

        if (!S.Attempted) {
            S.Attempted = true;

            // Deliberately never unloaded: Oodle hands back pointers into its own memory (e.g. the default compress
            // options), so unloading it would leave those pointers dangling.
            HMODULE Module = ::LoadLibraryW(S.Path.c_str());
            if (!Module) {
                std::fprintf(stderr, "Failed to load Oodle DLL \"%s\" (error %lu)\n", S.Path.string().c_str(), ::GetLastError());
                return nullptr;
            }

            Api& Result = S.Instance;
            Result.Compress = LoadProc<OodleLZ_Compress_T>(Module, "OodleLZ_Compress");
            Result.CompressOptions_GetDefault =
              LoadProc<OodleLZ_CompressOptions_GetDefault_T>(Module, "OodleLZ_CompressOptions_GetDefault");
            Result.Decompress = LoadProc<OodleLZ_Decompress_T>(Module, "OodleLZ_Decompress");
            Result.GetCompressedBufferSizeNeeded =
              LoadProc<OodleLZ_GetCompressedBufferSizeNeeded_T>(Module, "OodleLZ_GetCompressedBufferSizeNeeded");
            Result.GetDecodeBufferSize = LoadProc<OodleLZ_GetDecodeBufferSize_T>(Module, "OodleLZ_GetDecodeBufferSize");

            Result.Loaded = Result.Compress && Result.CompressOptions_GetDefault && Result.Decompress &&
                            Result.GetCompressedBufferSizeNeeded && Result.GetDecodeBufferSize;
        }

        return S.Instance.Loaded ? &S.Instance : nullptr;
    }

    std::vector<uint8_t> Oodle26::Compress(std::vector<uint8_t>& Source,
                                           const OodleLZCompressor Compressor,
                                           const OodleLZCompressionLevel Level) {
        const CompressOptions* Defaults = OodleLZ_CompressOptions_GetDefault(Compressor, Level);
        if (!Defaults) return {};

        CompressOptions Options = *Defaults;
        Options.SeekChunkReset  = 1;
        Options.SeekChunkLen    = 0x40000;

        const int64_t CompressBufferSizeNeeded =
          OodleLZ_GetCompressedBufferSizeNeeded(static_cast<int64_t>(Source.size()));
        if (CompressBufferSizeNeeded <= 0) return {};

        std::vector<uint8_t> CompressBuffer(static_cast<size_t>(CompressBufferSizeNeeded));
        const int64_t CompressedLen = OodleLZ_Compress(Compressor,
                                                       Source.data(),
                                                       static_cast<int64_t>(Source.size()),
                                                       CompressBuffer.data(),
                                                       Level,
                                                       &Options);
        if (CompressedLen <= 0) return {};  // Oodle returns 0 on failure

        CompressBuffer.resize(static_cast<size_t>(CompressedLen));
        return CompressBuffer;
    }

    std::vector<uint8_t> Oodle26::Decompress(std::vector<uint8_t>& Source, const int64_t UncompressedSize) {
        const int64_t DecodeBufferSize = OodleLZ_GetDecodeBufferSize(UncompressedSize, true);
        if (DecodeBufferSize <= 0) return {};

        std::vector<uint8_t> RawBuffer(static_cast<size_t>(DecodeBufferSize));
        const int64_t RawLen =
          OodleLZ_Decompress(Source.data(), static_cast<int64_t>(Source.size()), RawBuffer.data(), UncompressedSize);
        if (RawLen <= 0) return {};  // Oodle returns 0 on failure

        RawBuffer.resize(static_cast<size_t>(RawLen));
        return RawBuffer;
    }

    int64_t Oodle26::OodleLZ_Compress(OodleLZCompressor Compressor,
                                      void* RawBuffer,
                                      int64_t RawLen,
                                      void* CompressedBuffer,
                                      OodleLZCompressionLevel Level,
                                      void* Options,
                                      void* DictionaryBase,
                                      void* Lrm,
                                      void* ScratchMemory,
                                      int64_t ScratchLen) {
        const Api* Oodle = GetApi();
        if (!Oodle) return 0;

        return Oodle->Compress(Compressor,
                               RawBuffer,
                               RawLen,
                               CompressedBuffer,
                               Level,
                               Options,
                               DictionaryBase,
                               Lrm,
                               ScratchMemory,
                               ScratchLen);
    }

    int64_t Oodle26::OodleLZ_GetCompressedBufferSizeNeeded(const int64_t RawLen) {
        const Api* Oodle = GetApi();
        if (!Oodle) return 0;

        return Oodle->GetCompressedBufferSizeNeeded(RawLen);
    }

    int64_t Oodle26::OodleLZ_GetDecodeBufferSize(int64_t RawLen, bool CorruptionPossible) {
        const Api* Oodle = GetApi();
        if (!Oodle) return 0;

        return Oodle->GetDecodeBufferSize(RawLen, CorruptionPossible ? 1 : 0);
    }

    const Oodle26::CompressOptions* Oodle26::OodleLZ_CompressOptions_GetDefault(OodleLZCompressor Compressor,
                                                                                OodleLZCompressionLevel Level) {
        const Api* Oodle = GetApi();
        if (!Oodle) return nullptr;

        return Oodle->CompressOptions_GetDefault(Compressor, Level);
    }

    int64_t Oodle26::OodleLZ_Decompress(void* CompressedBuffer,
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
                                        OodleLZDecodeThreadPhase ThreadPhase) {
        const Api* Oodle = GetApi();
        if (!Oodle) return 0;

        return Oodle->Decompress(CompressedBuffer,
                                 CompressedLen,
                                 RawBuffer,
                                 RawLen,
                                 FuzzSafe,
                                 CheckCRC,
                                 Verbosity,
                                 DecompressedBufferBase,
                                 DecompressBufferLen,
                                 Callback,
                                 CallbackUserData,
                                 DecoderMemory,
                                 DecoderMemoryLen,
                                 ThreadPhase);
    }
}  // namespace Souls