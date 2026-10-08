//
// Created by Jake Rieger on 10/7/2026.
//

#pragma once

namespace Souls {
    struct Vector2 {
        float X = 0.f, Y = 0.f;
        bool operator==(const Vector2&) const = default;
    };

    struct Vector3 {
        float X = 0.f, Y = 0.f, Z = 0.f;
        bool operator==(const Vector3&) const = default;
    };

    struct Vector4 {
        float X = 0.f, Y = 0.f, Z = 0.f, W = 0.f;
        bool operator==(const Vector4&) const = default;
    };
}  // namespace Souls
