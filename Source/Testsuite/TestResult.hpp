// Copyright (C) 2026 Jake Rieger
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, version 3. This program is distributed in the hope that it will
// be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU General
// Public License along with this program. If not, see https://www.gnu.org/licenses/.

#pragma once

// Return value of a test function that couldn't run (missing game files, missing Oodle DLL). The runner turns it
// into exit code 77, which CTest treats as "skipped" via SKIP_RETURN_CODE.
inline constexpr int TestSkipped         = -1;
inline constexpr int TestSkippedExitCode = 77;
