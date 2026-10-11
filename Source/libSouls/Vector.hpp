// Copyright (C) 2026 Jake Rieger
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, version 3. This program is distributed in the hope that it will
// be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU General
// Public License along with this program. If not, see https://www.gnu.org/licenses/.

#pragma once

namespace Souls {
    /// @brief A 2D vector of floats (texture coordinates, 2D positions).
    struct Vector2 {
        float X = 0.f;  ///< The X component.
        float Y = 0.f;  ///< The Y component.
        /// @brief Component-wise equality.
        bool operator==(const Vector2&) const = default;
    };

    /// @brief A 3D vector of floats (positions, rotations in degrees or radians, scales).
    struct Vector3 {
        float X = 0.f;  ///< The X component.
        float Y = 0.f;  ///< The Y component.
        float Z = 0.f;  ///< The Z component.
        /// @brief Component-wise equality.
        bool operator==(const Vector3&) const = default;
    };

    /// @brief A 4D vector of floats (homogeneous positions, quaternions, colors).
    struct Vector4 {
        float X = 0.f;  ///< The X component.
        float Y = 0.f;  ///< The Y component.
        float Z = 0.f;  ///< The Z component.
        float W = 0.f;  ///< The W component.
        /// @brief Component-wise equality.
        bool operator==(const Vector4&) const = default;
    };
}  // namespace Souls
