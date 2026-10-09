//
// Created by Jake Rieger on 10/8/2026.
//

#pragma once

#include <memory>
#include <utility>

namespace Souls {

    // An optional, heap-allocated T with value semantics: copying a ValuePtr copies the object it owns. For recursive
    // data (trees) and optional members of types that are still incomplete where the member is declared.
    template<typename T>
    class ValuePtr {
    public:
        ValuePtr() = default;
        ValuePtr(const T& Value) : Pointer(std::make_unique<T>(Value)) {}
        ValuePtr(T&& Value) : Pointer(std::make_unique<T>(std::move(Value))) {}
        ValuePtr(const ValuePtr& Other) : Pointer(Other.Pointer ? std::make_unique<T>(*Other.Pointer) : nullptr) {}
        ValuePtr(ValuePtr&&) noexcept = default;

        ValuePtr& operator=(const ValuePtr& Other) {
            if (this != &Other) {
                Pointer = Other.Pointer ? std::make_unique<T>(*Other.Pointer) : nullptr;
            }
            return *this;
        }
        ValuePtr& operator=(ValuePtr&&) noexcept = default;

        T* operator->() { return Pointer.get(); }
        const T* operator->() const { return Pointer.get(); }
        T& operator*() { return *Pointer; }
        const T& operator*() const { return *Pointer; }
        T* Get() { return Pointer.get(); }
        const T* Get() const { return Pointer.get(); }
        explicit operator bool() const { return static_cast<bool>(Pointer); }

        template<typename... Args>
        T& Emplace(Args&&... Arguments) {
            Pointer = std::make_unique<T>(std::forward<Args>(Arguments)...);
            return *Pointer;
        }
        void Reset() { Pointer.reset(); }

    private:
        std::unique_ptr<T> Pointer;
    };

}  // namespace Souls
