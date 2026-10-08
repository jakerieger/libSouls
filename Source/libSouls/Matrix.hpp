//
// Created by Jake Rieger on 10/8/2026.
//

#pragma once

#include "Vector.hpp"

#include <cmath>

namespace Souls {
    // A 4x4 matrix using the same conventions as System.Numerics (and so as SoulsFormats): row-major, with vectors
    // treated as rows, so transforms are applied left to right and translation lives in the fourth row.
    struct Matrix4x4 {
        float M11 = 1, M12 = 0, M13 = 0, M14 = 0;
        float M21 = 0, M22 = 1, M23 = 0, M24 = 0;
        float M31 = 0, M32 = 0, M33 = 1, M34 = 0;
        float M41 = 0, M42 = 0, M43 = 0, M44 = 1;

        bool operator==(const Matrix4x4&) const = default;

        static Matrix4x4 Identity() { return {}; }

        static Matrix4x4 CreateScale(const Vector3& Scale) {
            Matrix4x4 Result;
            Result.M11 = Scale.X;
            Result.M22 = Scale.Y;
            Result.M33 = Scale.Z;
            return Result;
        }

        static Matrix4x4 CreateTranslation(const Vector3& Position) {
            Matrix4x4 Result;
            Result.M41 = Position.X;
            Result.M42 = Position.Y;
            Result.M43 = Position.Z;
            return Result;
        }

        static Matrix4x4 CreateRotationX(float Radians) {
            const float C = std::cos(Radians), S = std::sin(Radians);
            Matrix4x4 Result;
            Result.M22 = C;
            Result.M23 = S;
            Result.M32 = -S;
            Result.M33 = C;
            return Result;
        }

        static Matrix4x4 CreateRotationY(float Radians) {
            const float C = std::cos(Radians), S = std::sin(Radians);
            Matrix4x4 Result;
            Result.M11 = C;
            Result.M13 = -S;
            Result.M31 = S;
            Result.M33 = C;
            return Result;
        }

        static Matrix4x4 CreateRotationZ(float Radians) {
            const float C = std::cos(Radians), S = std::sin(Radians);
            Matrix4x4 Result;
            Result.M11 = C;
            Result.M12 = S;
            Result.M21 = -S;
            Result.M22 = C;
            return Result;
        }

        // Applies this transform first, then Other.
        Matrix4x4 operator*(const Matrix4x4& Other) const {
            Matrix4x4 R;
            R.M11 = M11 * Other.M11 + M12 * Other.M21 + M13 * Other.M31 + M14 * Other.M41;
            R.M12 = M11 * Other.M12 + M12 * Other.M22 + M13 * Other.M32 + M14 * Other.M42;
            R.M13 = M11 * Other.M13 + M12 * Other.M23 + M13 * Other.M33 + M14 * Other.M43;
            R.M14 = M11 * Other.M14 + M12 * Other.M24 + M13 * Other.M34 + M14 * Other.M44;
            R.M21 = M21 * Other.M11 + M22 * Other.M21 + M23 * Other.M31 + M24 * Other.M41;
            R.M22 = M21 * Other.M12 + M22 * Other.M22 + M23 * Other.M32 + M24 * Other.M42;
            R.M23 = M21 * Other.M13 + M22 * Other.M23 + M23 * Other.M33 + M24 * Other.M43;
            R.M24 = M21 * Other.M14 + M22 * Other.M24 + M23 * Other.M34 + M24 * Other.M44;
            R.M31 = M31 * Other.M11 + M32 * Other.M21 + M33 * Other.M31 + M34 * Other.M41;
            R.M32 = M31 * Other.M12 + M32 * Other.M22 + M33 * Other.M32 + M34 * Other.M42;
            R.M33 = M31 * Other.M13 + M32 * Other.M23 + M33 * Other.M33 + M34 * Other.M43;
            R.M34 = M31 * Other.M14 + M32 * Other.M24 + M33 * Other.M34 + M34 * Other.M44;
            R.M41 = M41 * Other.M11 + M42 * Other.M21 + M43 * Other.M31 + M44 * Other.M41;
            R.M42 = M41 * Other.M12 + M42 * Other.M22 + M43 * Other.M32 + M44 * Other.M42;
            R.M43 = M41 * Other.M13 + M42 * Other.M23 + M43 * Other.M33 + M44 * Other.M43;
            R.M44 = M41 * Other.M14 + M42 * Other.M24 + M43 * Other.M34 + M44 * Other.M44;
            return R;
        }

        // Transforms a point (w = 1).
        Vector3 Transform(const Vector3& V) const {
            return {V.X * M11 + V.Y * M21 + V.Z * M31 + M41,
                    V.X * M12 + V.Y * M22 + V.Z * M32 + M42,
                    V.X * M13 + V.Y * M23 + V.Z * M33 + M43};
        }
    };
}  // namespace Souls
