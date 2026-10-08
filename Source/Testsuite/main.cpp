//
// Created by Jake Rieger on 10/7/2026.
//

#include "TestResult.hpp"

#include <libSouls/Oodle26.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <initializer_list>

int RunBinaryTests();
int RunBinderTests();
int RunBinderFileTests(size_t MaxFiles);
int RunRegulationTests();
int RunOodleTests();
int RunSoulsFileTests();
int RunRealFileTests(size_t MaxFiles);

namespace {
    int Run(const char* Name, int Argc, char** Argv) {
        if (std::strcmp(Name, "binary") == 0) return RunBinaryTests();
        if (std::strcmp(Name, "binder") == 0) return RunBinderTests();
        if (std::strcmp(Name, "binderfiles") == 0) {
            const size_t MaxFiles = Argc > 2 ? static_cast<size_t>(std::strtoull(Argv[2], nullptr, 10)) : 40;
            return RunBinderFileTests(MaxFiles > 0 ? MaxFiles : 40);
        }
        if (std::strcmp(Name, "regulation") == 0) return RunRegulationTests();
        if (std::strcmp(Name, "oodle") == 0) return RunOodleTests();
        if (std::strcmp(Name, "soulsfile") == 0) return RunSoulsFileTests();
        if (std::strcmp(Name, "realfiles") == 0) {
            const size_t MaxFiles = Argc > 2 ? static_cast<size_t>(std::strtoull(Argv[2], nullptr, 10)) : 150;
            return RunRealFileTests(MaxFiles > 0 ? MaxFiles : 150);
        }
        std::fprintf(
          stderr,
          "Unknown test suite \"%s\" (binary, binder, oodle, soulsfile, regulation, binderfiles [max files], realfiles [max files])\n",
          Name);
        return 1;
    }
}  // namespace

// Usage: Testsuite <suite> [args]. With no suite, runs everything. Exit codes: 0 pass, 77 skipped (CTest
// SKIP_RETURN_CODE), anything else fails.
int main(int Argc, char** Argv) {
    try {
        // The tests use the Oodle DLL that ships with the game install; without it the Oodle-dependent suites skip.
        Souls::Oodle26::SetDLLPath("C:/Program Files (x86)/Steam/steamapps/common/ELDEN RING/Game/oo2core_6_win64.dll");

        if (Argc > 1) {
            const int Result = Run(Argv[1], Argc, Argv);
            return Result == TestSkipped ? TestSkippedExitCode : (Result != 0 ? 1 : 0);
        }

        int Failed = 0;
        for (const char* Name : {"binary", "binder", "oodle", "soulsfile", "regulation", "binderfiles", "realfiles"}) {
            const int Result = Run(Name, Argc, Argv);
            if (Result != 0 && Result != TestSkipped) { ++Failed; }
        }
        return Failed == 0 ? 0 : 1;
    } catch (const std::exception& E) {
        std::fflush(stdout);
        std::fprintf(stderr, "Unhandled exception: %s\n", E.what());
        return 2;
    }
}
