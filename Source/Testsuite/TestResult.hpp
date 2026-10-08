//
// Created by Jake Rieger on 10/7/2026.
//

#pragma once

// Return value of a test function that couldn't run (missing game files, missing Oodle DLL). The runner turns it
// into exit code 77, which CTest treats as "skipped" via SKIP_RETURN_CODE.
inline constexpr int TestSkipped = -1;
inline constexpr int TestSkippedExitCode = 77;
