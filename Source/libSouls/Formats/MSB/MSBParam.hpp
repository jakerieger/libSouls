//
// Created by Jake Rieger on 10/8/2026.
//

#pragma once

#include "MSBCommon.hpp"

#include <type_traits>
#include <utility>
#include <vector>

namespace Souls::Msb::Detail {

    // Mixin for a param that holds one list per entry type. Derived provides
    //
    //     template<typename F> void ForEachList(F&& Visit);  // calls Visit(list) for each list, in write order
    //
    // (const and non-const versions), and every entry type provides `static constexpr uint32_t TypeId`.
    template<typename Derived, typename EntryT>
    class TypedLists {
    public:
        // Where each entry read from the file ended up: (list number, position in that list), in file order.
        using FileOrder = std::vector<std::pair<size_t, size_t>>;

        // Every entry, in the order they are written: all of the first list, then the second and so on.
        std::vector<EntryT*> GetEntries() {
            std::vector<EntryT*> Result;
            Self().ForEachList([&](auto& List) {
                for (auto& Item : List) {
                    Result.push_back(&Item);
                }
            });
            return Result;
        }

        std::vector<const EntryT*> GetEntries() const {
            std::vector<const EntryT*> Result;
            Self().ForEachList([&](const auto& List) {
                for (const auto& Item : List) {
                    Result.push_back(&Item);
                }
            });
            return Result;
        }

        // Adds an entry to the list for its type and returns a reference to it there.
        template<typename T>
        T& Add(T Item) {
            T* Result = nullptr;
            Self().ForEachList([&](auto& List) {
                using Element = typename std::decay_t<decltype(List)>::value_type;
                if constexpr (std::is_same_v<Element, T>) {
                    List.push_back(std::move(Item));
                    Result = &List.back();
                }
            });
            static_assert(sizeof(T) > 0);
            if (!Result) {
                throw BinaryException("This entry type can't be added to this param");
            }
            return *Result;
        }

        // Reads an entry of the given type with `Item.Read(Reader)` and appends it to its list; records its place.
        // Returns false if no list holds that type.
        template<typename Reader_>
        bool ReadEntryOfType(uint32_t Type, Reader_& Reader, FileOrder& Order) {
            bool Done      = false;
            size_t ListNum = 0;
            Self().ForEachList([&](auto& List) {
                using Element = typename std::decay_t<decltype(List)>::value_type;
                if (!Done && Element::TypeId == Type) {
                    Element Item;
                    Item.Read(Reader);
                    List.push_back(std::move(Item));
                    Order.emplace_back(ListNum, List.size() - 1);
                    Done = true;
                }
                ++ListNum;
            });
            return Done;
        }

        // Pointers to the entries read, in file order. Valid until the lists are modified.
        std::vector<EntryT*> EntriesInFileOrder(const FileOrder& Order) {
            std::vector<EntryT*> Result;
            Result.reserve(Order.size());
            for (const auto& [ListNum, Position] : Order) {
                size_t Current = 0;
                Self().ForEachList([&](auto& List) {
                    if (Current++ == ListNum) {
                        Result.push_back(&List[Position]);
                    }
                });
            }
            return Result;
        }

    private:
        Derived& Self() { return static_cast<Derived&>(*this); }
        const Derived& Self() const { return static_cast<const Derived&>(*this); }
    };

}  // namespace Souls::Msb::Detail
