/// @file
/// @author Jake Rieger
/// @date 10/8/2026

#pragma once

#include <libSouls/SoulsFile.hpp>

#include <array>
#include <string>
#include <variant>
#include <vector>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of an exported class; only touched by this DLL's own code

    /// @brief FMB: "expression" files introduced in Elden Ring.
    ///
    /// Extension: .expb
    class SOULS_API FMB : public SoulsFile<FMB> {
    public:
        /// @brief An entry: a type number plus, depending on the type, nothing, a string, a double, or two doubles.
        struct Entry {
            /// @brief What an entry can hold.
            using Data = std::variant<std::monostate, std::string, double, std::array<double, 2>>;

            /// @brief Type of the entry.
            int32_t Type = 0;
            /// @brief The entry's data, which depends on its type.
            Data Value;
        };

        /// @brief Unknown.
        int32_t Unk20 = 0;
        /// @brief Entries in this FMB.
        std::vector<Entry> Entries;

    protected:
        bool IsImpl(BinaryReader& Reader) override;
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
