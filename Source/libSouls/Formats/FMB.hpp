//
// Created by Jake Rieger on 10/8/2026.
//

#pragma once

#include <libSouls/SoulsFile.hpp>

#include <array>
#include <string>
#include <variant>
#include <vector>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of an exported class; only touched by this DLL's own code

    // FMB: "expression" files introduced in Elden Ring. Extension: .expb
    class SOULS_API FMB : public SoulsFile<FMB> {
    public:
        // An entry: a type number plus, depending on the type, nothing, a string, a double, or two doubles.
        struct Entry {
            using Data = std::variant<std::monostate, std::string, double, std::array<double, 2>>;

            int32_t Type = 0;
            Data Value;
        };

        // Unknown.
        int32_t Unk20 = 0;
        std::vector<Entry> Entries;

    protected:
        bool IsImpl(BinaryReader& Reader) override;
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
