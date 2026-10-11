// Copyright (C) 2026 Jake Rieger
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, version 3. This program is distributed in the hope that it will
// be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU General
// Public License along with this program. If not, see https://www.gnu.org/licenses/.

#pragma once

#include <bit>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace Souls {
    /// @brief A byte order.
    enum class Endian {
        Little,  ///< Least significant byte first (PC and most console formats).
        Big,     ///< Most significant byte first (PS3 and Xbox 360 formats).
    };

    /// @brief The byte order of the machine the library runs on.
    inline constexpr Endian HostEndian = std::endian::native == std::endian::little ? Endian::Little : Endian::Big;

    /// @brief Anything that can be read/written as a raw fixed-size value: integers, floats, bool, enums.
    template<typename T>
    concept Scalar = std::is_arithmetic_v<T> || std::is_enum_v<T>;

    namespace Detail {
        template<size_t Size>
        struct UnsignedOfSize;
        template<>
        struct UnsignedOfSize<1> {
            using Type = uint8_t;
        };
        template<>
        struct UnsignedOfSize<2> {
            using Type = uint16_t;
        };
        template<>
        struct UnsignedOfSize<4> {
            using Type = uint32_t;
        };
        template<>
        struct UnsignedOfSize<8> {
            using Type = uint64_t;
        };
    }  // namespace Detail

    /// @brief Reverses the bytes of a value.
    template<Scalar T>
    constexpr T ByteSwap(T Value) {
        if constexpr (sizeof(T) == 1) {
            return Value;
        } else {
            using U    = typename Detail::UnsignedOfSize<sizeof(T)>::Type;
            const U In = std::bit_cast<U>(Value);
            U Out      = 0;
            for (size_t I = 0; I < sizeof(T); ++I) {
                Out = static_cast<U>((Out << 8) | ((In >> (8 * I)) & 0xFF));
            }
            return std::bit_cast<T>(Out);
        }
    }

    /// @brief Converts between the given byte order and the host's.
    ///
    /// Symmetric, so it works for both reading and writing.
    template<Scalar T>
    constexpr T ToHost(T Value, Endian Order) {
        return Order == HostEndian ? Value : ByteSwap(Value);
    }

    /// @brief Thrown when data isn't valid for the format being read, or can't be written as asked.
    class BinaryException : public std::runtime_error {
    public:
        /// @brief An exception with the given message.
        explicit BinaryException(const std::string& Message) : std::runtime_error(Message) {}
    };
}  // namespace Souls
