//
// Created by Jake Rieger on 10/7/2026.
//

#include "TestResult.hpp"

#include <libSouls/Oodle26.hpp>
#include <cstdio>

// Round-trips data through the Oodle DLL. Skipped if the DLL can't be loaded.
int RunOodleTests() {
    using namespace Souls;

    std::vector<uint8_t> Data(1024, 'A');
    for (size_t I = 0; I < Data.size(); I += 7) {
        Data[I] = static_cast<uint8_t>(I);
    }

    auto Compressed =
      Oodle26::Compress(Data, Oodle26::OodleLZCompressor::Kraken, Oodle26::OodleLZCompressionLevel::Normal);
    if (Compressed.empty()) {
        std::printf("Oodle tests skipped (Oodle DLL unavailable)\n");
        return TestSkipped;
    }

    int Failures = 0;
    if (Compressed.size() >= Data.size()) {
        std::printf("FAIL Oodle did not compress the data (%zu -> %zu)\n", Data.size(), Compressed.size());
        ++Failures;
    }
    if (Oodle26::Decompress(Compressed, static_cast<int64_t>(Data.size())) != Data) {
        std::printf("FAIL Oodle round trip mismatch\n");
        ++Failures;
    }
    std::printf(Failures == 0 ? "Oodle tests passed\n" : "Oodle tests: %d failure(s)\n", Failures);
    return Failures;
}
