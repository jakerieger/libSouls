/// @file
/// @author Jake Rieger
/// @date 10/7/2026

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
