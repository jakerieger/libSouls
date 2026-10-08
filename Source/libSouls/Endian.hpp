//
// Created by Jake Rieger on 10/7/2026.
//

#pragma once

#include <bit>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace Souls {
    enum class Endian {
        Little,
        Big,
    };

    inline constexpr Endian HostEndian = std::endian::native == std::endian::little ? Endian::Little : Endian::Big;

    // Anything that can be read/written as a raw fixed-size value: integers, floats, bool, enums.
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

    // Converts between the given byte order and the host's. Symmetric, so it works for both reading and writing.
    template<Scalar T>
    constexpr T ToHost(T Value, Endian Order) {
        return Order == HostEndian ? Value : ByteSwap(Value);
    }

    class BinaryException : public std::runtime_error {
    public:
        explicit BinaryException(const std::string& Message) : std::runtime_error(Message) {}
    };
}  // namespace Souls
