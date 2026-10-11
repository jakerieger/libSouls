//
// Created by Jake Rieger on 10/11/2026.
//

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

    // A file that couldn't be loaded, and why.
    struct SOULS_API MetadataLoadError {
        std::filesystem::path Path;
        std::string Message;
    };

    // What a load did. One bad file doesn't stop the rest from loading.
    struct SOULS_API MetadataLoadResult {
        size_t Files   = 0;  // files loaded
        size_t Entries = 0;  // names or enumerators added
        std::vector<MetadataLoadError> Errors;
    };

    // Names for param rows, which the game's own files leave out: ID-to-name tables kept by the community, such as the
    // `Names` folders of Paramdex. Each is a text file named after the param (`EquipParamWeapon.txt`) with one
    // "<ID> <name>" line per row.
    //
    //     ParamNameRepository Names;
    //     Names.AddFolder("C:/Tools/Paramdex/ER/Names");
    //     Names.Apply(Weapons, "EquipParamWeapon");
    //
    // Param names are matched ignoring case.
    class SOULS_API ParamNameRepository {
    public:
        // Loads every .txt file in the folder (not its subfolders); the file name without extension is the param's
        // name. Names from files loaded later replace earlier ones for the same row. Throws BinaryException only if
        // the folder doesn't exist.
        MetadataLoadResult AddFolder(const std::filesystem::path& Folder);
        MetadataLoadResult AddFile(const std::filesystem::path& File);

        // Sets the name of one row.
        void Add(std::string_view ParamName, int32_t ID, std::string Name);

        // All the names of a param, by row ID, or nullptr if there are none.
        const std::map<int32_t, std::string>* Find(std::string_view ParamName) const;
        // One row's name.
        std::optional<std::string> Find(std::string_view ParamName, int32_t ID) const;

        // Gives the rows of the param their names. Rows that already have a (non-empty) name keep it unless Overwrite is set.
        // Returns how many rows were named.
        size_t Apply(PARAM& Param, std::string_view ParamName, bool Overwrite = false) const;

        // The params that have names, as written when they were added.
        std::vector<std::string> ParamNames() const;
        size_t Size() const { return Tables.size(); }
        bool Empty() const { return Tables.empty(); }
        void Clear() { Tables.clear(); }

    private:
        struct Table {
            std::string Name;
            std::map<int32_t, std::string> Names;
        };
        std::map<std::string, Table> Tables;  // by lowercase param name
    };

    // Names for the values of enumerated field types (the `Tdfs` folders of Paramdex). A paramdef field whose internal
    // type is an enum (see PARAMDEF::Field::InternalType) takes its values from one of these.
    class SOULS_API ParamEnumRepository {
    public:
        // Loads every .tdf file in the folder (not its subfolders).
        MetadataLoadResult AddFolder(const std::filesystem::path& Folder);
        MetadataLoadResult AddFile(const std::filesystem::path& File);
        // Adds an enum, replacing one with the same name.
        void Add(PARAMTDF Enum);

        // The enum with this name (ignoring case), or nullptr.
        const PARAMTDF* Find(std::string_view Name) const;
        std::vector<const PARAMTDF*> All() const;
        size_t Size() const { return Enums.size(); }
        bool Empty() const { return Enums.empty(); }
        void Clear() { Enums.clear(); }

    private:
        std::map<std::string, PARAMTDF> Enums;  // by lowercase name
    };

#pragma warning(pop)

}  // namespace Souls
