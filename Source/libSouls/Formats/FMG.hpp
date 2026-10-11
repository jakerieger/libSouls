/// @file
/// @author Jake Rieger
/// @date 10/7/2026

#pragma once

#include <libSouls/SoulsFile.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of an exported class; only touched by this DLL's own code

    /// @brief FMG: the string container used throughout the series for item names, descriptions, menu text, dialogue
    ///        and so on.
    ///
    /// Each string has a numeric ID; text is stored as UTF-16 in the file and exposed here as UTF-8.
    class SOULS_API FMG : public SoulsFile<FMG> {
    public:
        /// @brief Which game generation's layout the file uses.
        enum class FMGVersion : uint8_t {
            DemonsSouls = 0,
            DarkSouls1  = 1,  ///< and Dark Souls II
            DarkSouls3  = 2,  ///< Bloodborne, Dark Souls III, Sekiro and Elden Ring
        };

        /// @brief A string identified by an ID.
        ///
        /// Text is absent (null) for IDs that exist but have no string.
        struct SOULS_API Entry {
            /// @brief The ID of this entry.
            int32_t ID = 0;
            /// @brief The text of this entry.
            std::optional<std::string> Text;

            Entry() = default;
            /// @brief An entry with an ID and text (nullopt for none).
            Entry(int32_t ID, std::optional<std::string> Text) : ID(ID), Text(std::move(Text)) {}

            /// @brief "ID: Text".
            std::string ToString() const { return std::to_string(ID) + ": " + Text.value_or("<null>"); }
        };

        /// @brief The strings contained in this FMG.
        std::vector<Entry> Entries;
        /// @brief Indicates file format; 0 - DeS, 1 - DS1/DS2, 2 - DS3/BB.
        FMGVersion Version = FMGVersion::DarkSouls1;
        /// @brief Whether multi-byte values are big-endian.
        bool BigEndian = false;

        /// @brief An empty FMG configured for DS1/DS2.
        FMG() = default;
        /// @brief An empty FMG configured for the given version.
        explicit FMG(FMGVersion Version) : Version(Version), BigEndian(Version == FMGVersion::DemonsSouls) {}

        /// @brief The entry with this ID, or nullptr.
        Entry* Find(int32_t ID);
        /// @brief The entry with this ID, or nullptr.
        const Entry* Find(int32_t ID) const;

        /// @brief The string with this ID, or nullopt if there's no such entry or it has no text.
        std::optional<std::string> GetText(int32_t ID) const;
        /// @brief Sets the string with this ID, adding the entry if it doesn't exist.
        void SetText(int32_t ID, std::optional<std::string> Text);

    protected:
        /// @brief FMGs have no magic, so this is a structural check of the header; it can't be certain.
        bool IsImpl(BinaryReader& Reader) override;
        void ReadImpl(BinaryReader& Reader) override;
        /// @brief Sorts Entries by ID first (the file format requires it).
        void WriteImpl(BinaryWriter& Writer) override;
    };

#pragma warning(pop)

}  // namespace Souls
