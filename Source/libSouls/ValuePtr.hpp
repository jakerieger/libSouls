// Copyright (C) 2026 Jake Rieger
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, version 3. This program is distributed in the hope that it will
// be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU General
// Public License along with this program. If not, see https://www.gnu.org/licenses/.

#pragma once

#include <memory>
#include <utility>

namespace Souls {

    /// @brief An optional, heap-allocated T with value semantics: copying a ValuePtr copies the object it owns.
    ///
    /// For recursive data (trees) and optional members of types that are still incomplete where the member is declared.
    template<typename T>
    class ValuePtr {
    public:
        /// @brief An empty ValuePtr.
        ValuePtr() = default;
        /// @brief A ValuePtr holding a copy of Value.
        ValuePtr(const T& Value) : Pointer(std::make_unique<T>(Value)) {}
        /// @brief A ValuePtr holding Value, moved in.
        ValuePtr(T&& Value) : Pointer(std::make_unique<T>(std::move(Value))) {}
        /// @brief A deep copy: the new ValuePtr owns a copy of Other's object.
        ValuePtr(const ValuePtr& Other) : Pointer(Other.Pointer ? std::make_unique<T>(*Other.Pointer) : nullptr) {}
        /// @brief Takes Other's object, leaving Other empty.
        ValuePtr(ValuePtr&&) noexcept = default;

        /// @brief A deep copy: replaces the object with a copy of Other's.
        ValuePtr& operator=(const ValuePtr& Other) {
            if (this != &Other) { Pointer = Other.Pointer ? std::make_unique<T>(*Other.Pointer) : nullptr; }
            return *this;
        }
        /// @brief Takes Other's object, leaving Other empty.
        ValuePtr& operator=(ValuePtr&&) noexcept = default;

        /// @brief Member access to the object; it must not be empty.
        T* operator->() { return Pointer.get(); }
        /// @brief Member access to the object; it must not be empty.
        const T* operator->() const { return Pointer.get(); }
        /// @brief The object; it must not be empty.
        T& operator*() { return *Pointer; }
        /// @brief The object; it must not be empty.
        const T& operator*() const { return *Pointer; }
        /// @brief The object, or nullptr if empty.
        T* Get() { return Pointer.get(); }
        /// @brief The object, or nullptr if empty.
        const T* Get() const { return Pointer.get(); }
        /// @brief Whether there is an object.
        explicit operator bool() const { return static_cast<bool>(Pointer); }

        /// @brief Replaces the object with one constructed from Arguments and returns it.
        template<typename... Args>
        T& Emplace(Args&&... Arguments) {
            Pointer = std::make_unique<T>(std::forward<Args>(Arguments)...);
            return *Pointer;
        }
        /// @brief Destroys the object, leaving the ValuePtr empty.
        void Reset() { Pointer.reset(); }

    private:
        std::unique_ptr<T> Pointer;
    };

}  // namespace Souls
