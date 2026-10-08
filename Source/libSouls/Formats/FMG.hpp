//
// Created by Jake Rieger on 10/7/2026.
//

#pragma once

#include <libSouls/SoulsFile.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of an exported class; only touched by this DLL's own code

    // FMG: the string container used throughout the series for item names, descriptions, menu text, dialogue and so
    // on. Each string has a numeric ID; text is stored as UTF-16 in the file and exposed here as UTF-8.
    class SOULS_API FMG : public SoulsFile<FMG> {
    public:
        // Which game generation's layout the file uses.
        enum class FMGVersion : uint8_t {
            DemonsSouls = 0,
            DarkSouls1  = 1,  // and Dark Souls II
            DarkSouls3  = 2,  // Bloodborne, Dark Souls III, Sekiro and Elden Ring
        };

        // A string identified by an ID. Text is absent (null) for IDs that exist but have no string.
        struct SOULS_API Entry {
            int32_t ID = 0;
            std::optional<std::string> Text;

            Entry() = default;
            Entry(int32_t ID, std::optional<std::string> Text) : ID(ID), Text(std::move(Text)) {}

            std::string ToString() const { return std::to_string(ID) + ": " + Text.value_or("<null>"); }
        };

        std::vector<Entry> Entries;
        FMGVersion Version = FMGVersion::DarkSouls1;
        // Whether multi-byte values are big-endian.
        bool BigEndian = false;

        // An empty FMG configured for DS1/DS2.
        FMG() = default;
        // An empty FMG configured for the given version.
        explicit FMG(FMGVersion Version) : Version(Version), BigEndian(Version == FMGVersion::DemonsSouls) {}

        // The entry with this ID, or nullptr.
        Entry* Find(int32_t ID);
        const Entry* Find(int32_t ID) const;

        // The string with this ID, or nullopt if there's no such entry or it has no text.
        std::optional<std::string> GetText(int32_t ID) const;
        // Sets the string with this ID, adding the entry if it doesn't exist.
        void SetText(int32_t ID, std::optional<std::string> Text);

    protected:
        // FMGs have no magic, so this is a structural check of the header; it can't be certain.
        bool IsImpl(BinaryReader& Reader) override;
        void ReadImpl(BinaryReader& Reader) override;
        // Sorts Entries by ID first (the file format requires it).
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
