// Copyright (C) 2026 Jake Rieger
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, version 3. This program is distributed in the hope that it will
// be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU General
// Public License along with this program. If not, see https://www.gnu.org/licenses/.

#pragma once

#include "PARAM.hpp"
#include "PARAMTDF.hpp"

#include <libSouls/Souls.hpp>

#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of exported classes; only touched by this DLL's own code

    /// @brief A file that couldn't be loaded, and why.
    struct SOULS_API MetadataLoadError {
        /// @brief The file.
        std::filesystem::path Path;
        /// @brief Why it couldn't be loaded.
        std::string Message;
    };

    /// @brief What a load did.
    ///
    /// One bad file doesn't stop the rest from loading.
    struct SOULS_API MetadataLoadResult {
        size_t Files   = 0;  ///< files loaded
        size_t Entries = 0;  ///< names or enumerators added
        /// @brief The files that couldn't be loaded.
        std::vector<MetadataLoadError> Errors;
    };

    /// @brief Names for param rows, which the game's own files leave out: ID-to-name tables kept by the community, such
    ///        as the `Names` folders of Paramdex.
    ///
    /// Each is a text file named after the param (`EquipParamWeapon.txt`) with one "<ID> <name>" line per row.
    ///
    /// @code
    /// ParamNameRepository Names;
    /// Names.AddFolder("C:/Tools/Paramdex/ER/Names");
    /// Names.Apply(Weapons, "EquipParamWeapon");
    /// @endcode
    ///
    /// Param names are matched ignoring case.
    class SOULS_API ParamNameRepository {
    public:
        /// @brief Loads every .txt file in the folder (not its subfolders); the file name without extension is the
        ///        param's name.
        ///
        /// Names from files loaded later replace earlier ones for the same row. Throws BinaryException only if the
        /// folder doesn't exist.
        MetadataLoadResult AddFolder(const std::filesystem::path& Folder);
        /// @brief Loads one names file; the file name without extension is the param's name.
        MetadataLoadResult AddFile(const std::filesystem::path& File);

        /// @brief Sets the name of one row.
        void Add(std::string_view ParamName, int32_t ID, std::string Name);

        /// @brief All the names of a param, by row ID, or nullptr if there are none.
        const std::map<int32_t, std::string>* Find(std::string_view ParamName) const;
        /// @brief One row's name.
        std::optional<std::string> Find(std::string_view ParamName, int32_t ID) const;

        /// @brief Gives the rows of the param their names.
        ///
        /// Rows that already have a (non-empty) name keep it unless Overwrite is set. Returns how many rows were named.
        size_t Apply(PARAM& Param, std::string_view ParamName, bool Overwrite = false) const;

        /// @brief The params that have names, as written when they were added.
        std::vector<std::string> ParamNames() const;
        /// @brief How many params have names.
        size_t Size() const { return _Tables.size(); }
        /// @brief Whether no names are loaded.
        bool Empty() const { return _Tables.empty(); }
        /// @brief Forgets every name.
        void Clear() { _Tables.clear(); }

    private:
        struct Table {
            std::string Name;
            std::map<int32_t, std::string> Names;
        };
        std::map<std::string, Table> _Tables;  ///< by lowercase param name
    };

    /// @brief Names for the values of enumerated field types (the `Tdfs` folders of Paramdex).
    ///
    /// A paramdef field whose internal type is an enum (see PARAMDEF::Field::InternalType) takes its values from one of
    /// these.
    class SOULS_API ParamEnumRepository {
    public:
        /// @brief Loads every .tdf file in the folder (not its subfolders).
        MetadataLoadResult AddFolder(const std::filesystem::path& Folder);
        /// @brief Loads one .tdf file.
        MetadataLoadResult AddFile(const std::filesystem::path& File);
        /// @brief Adds an enum, replacing one with the same name.
        void Add(PARAMTDF Enum);

        /// @brief The enum with this name (ignoring case), or nullptr.
        const PARAMTDF* Find(std::string_view Name) const;
        /// @brief Every enum, ordered by name (ignoring case).
        std::vector<const PARAMTDF*> All() const;
        /// @brief How many enums are loaded.
        size_t Size() const { return _Enums.size(); }
        /// @brief Whether no enums are loaded.
        bool Empty() const { return _Enums.empty(); }
        /// @brief Forgets every enum.
        void Clear() { _Enums.clear(); }

    private:
        std::map<std::string, PARAMTDF> _Enums;  ///< by lowercase name
    };

#pragma warning(pop)

}  // namespace Souls
