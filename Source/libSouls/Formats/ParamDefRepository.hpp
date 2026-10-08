//
// Created by Jake Rieger on 10/7/2026.
//

#pragma once

#include "PARAM.hpp"
#include "PARAMDEF.hpp"

#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of an exported class; only touched by this DLL's own code

    // A collection of PARAMDEFs for one game, loaded from folders of XML files (the form the modding community
    // shares them in). Ask it which def describes a given PARAM.
    //
    // Make one repository per game: different games reuse param type names with different layouts, so defs from
    // more than one game shouldn't share a repository.
    //
    //     ParamDefRepository Defs;
    //     Defs.AddFolder("C:/Tools/Paramdex/ER/Defs");
    //     if (const PARAMDEF* Def = Defs.Find(Param)) {
    //         ParamLayout Layout(*Def, Param.BigEndian);
    //     }
    //
    // Pointers it returns stay valid until the repository is cleared, reloaded, or the def they point to is
    // replaced. Not thread-safe.
    class SOULS_API ParamDefRepository {
    public:
        // A file that couldn't be loaded, and why.
        struct LoadError {
            std::filesystem::path Path;
            std::string Message;
        };

        // What a load did. One bad file doesn't stop the rest from loading.
        struct LoadResult {
            size_t Loaded   = 0;  // defs added (including any that replaced an earlier def)
            size_t Replaced = 0;  // of those, how many replaced a def with the same type and data version
            size_t Skipped  = 0;  // XML files that aren't PARAMDEFs (metadata and the like)
            std::vector<LoadError> Errors;
        };

        ParamDefRepository();
        ~ParamDefRepository();
        ParamDefRepository(const ParamDefRepository&)            = delete;
        ParamDefRepository& operator=(const ParamDefRepository&) = delete;
        ParamDefRepository(ParamDefRepository&&) noexcept;
        ParamDefRepository& operator=(ParamDefRepository&&) noexcept;

#pragma region Loading
        // Loads every .xml file in the folder (and its subfolders, if Recursive), in path order. A def with the same
        // param type and data version as one already present replaces it, so folders added later override earlier
        // ones. The folder is remembered for Reload(). Throws BinaryException only if the folder doesn't exist.
        LoadResult AddFolder(const std::filesystem::path& Folder, bool Recursive = true);

        // Loads a single XML file; replacement works as for folders. Not remembered for Reload().
        LoadResult AddFile(const std::filesystem::path& File);

        // Adds a def you already have. Replaces one with the same param type and data version.
        void Add(PARAMDEF Def);

        // Forgets everything, including the folders to reload.
        void Clear();

        // Drops the defs loaded from folders and loads those folders again, picking up files that were added,
        // changed or removed. Defs added with Add() and files added with AddFile() are kept.
        LoadResult Reload();
#pragma endregion

#pragma region Lookup
        // The def that describes the param: same param type and data version, and a row size that agrees (see
        // PARAM::Matches). Nullptr if there isn't one.
        const PARAMDEF* Find(const PARAM& Param) const;

        // The def with this param type and the highest data version, or nullptr.
        const PARAMDEF* FindByType(std::string_view ParamType) const;

        // Every def with this param type, newest data version first. Useful for explaining why Find() came up
        // empty (a def exists, but for another data version or row size).
        std::vector<const PARAMDEF*> FindCandidates(std::string_view ParamType) const;

        size_t Size() const { return Entries.size(); }
        bool Empty() const { return Entries.empty(); }
        // Every def, in the order loaded.
        std::vector<const PARAMDEF*> All() const;
#pragma endregion

    private:
        struct Entry {
            PARAMDEF Def;
            // The folder the def was loaded from; empty for defs added directly.
            std::filesystem::path Folder;
        };

        struct WatchedFolder {
            std::filesystem::path Path;
            bool Recursive;
        };

        // Adds Def, replacing a match. Returns true if it replaced one.
        bool Insert(PARAMDEF Def, const std::filesystem::path& Folder);
        void LoadFile(const std::filesystem::path& File, const std::filesystem::path& Folder, LoadResult& Result);

        // Entries are individually allocated so pointers to their defs stay put as the list changes.
        std::vector<std::unique_ptr<Entry>> Entries;
        std::vector<WatchedFolder> Folders;
    };

#pragma warning(pop)

}  // namespace Souls
