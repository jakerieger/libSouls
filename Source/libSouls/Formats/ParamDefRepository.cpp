//
// Created by Jake Rieger on 10/7/2026.
//

#include "ParamDefRepository.hpp"

#include <pugixml.hpp>

#include <algorithm>
#include <cctype>

namespace Souls {
    namespace fs = std::filesystem;

    namespace {
        bool HasXmlExtension(const fs::path& Path) {
            std::string Extension = Path.extension().string();
            std::transform(Extension.begin(), Extension.end(), Extension.begin(), [](unsigned char C) { return std::tolower(C); });
            return Extension == ".xml";
        }

        // Whether the file is an XML document whose root element is <PARAMDEF>. Community folders also hold other XML
        // (field metadata, for instance) that isn't ours to load.
        enum class XmlKind { ParamDef, Other, Unreadable };

        XmlKind Classify(const fs::path& File, std::string& Why) {
            pugi::xml_document Document;
            const pugi::xml_parse_result Result = Document.load_file(File.c_str(), pugi::parse_minimal);
            if (!Result) {
                Why = Result.description();
                return XmlKind::Unreadable;
            }
            return std::string_view(Document.document_element().name()) == "PARAMDEF" ? XmlKind::ParamDef : XmlKind::Other;
        }
    }  // namespace

    ParamDefRepository::ParamDefRepository()                                           = default;
    ParamDefRepository::~ParamDefRepository()                                          = default;
    ParamDefRepository::ParamDefRepository(ParamDefRepository&&) noexcept              = default;
    ParamDefRepository& ParamDefRepository::operator=(ParamDefRepository&&) noexcept   = default;

    bool ParamDefRepository::Insert(PARAMDEF Def, const fs::path& Folder) {
        for (std::unique_ptr<Entry>& Existing : Entries) {
            if (Existing->Def.ParamType == Def.ParamType && Existing->Def.DataVersion == Def.DataVersion) {
                Existing = std::make_unique<Entry>(Entry{std::move(Def), Folder});
                return true;
            }
        }
        Entries.push_back(std::make_unique<Entry>(Entry{std::move(Def), Folder}));
        return false;
    }

    void ParamDefRepository::LoadFile(const fs::path& File, const fs::path& Folder, LoadResult& Result) {
        std::string Why;
        switch (Classify(File, Why)) {
            case XmlKind::Other: ++Result.Skipped; return;
            case XmlKind::Unreadable: Result.Errors.push_back({File, Why}); return;
            case XmlKind::ParamDef: break;
        }

        try {
            PARAMDEF Def = PARAMDEF::FromXml(File);
            ++Result.Loaded;
            if (Insert(std::move(Def), Folder)) ++Result.Replaced;
        } catch (const std::exception& E) {
            Result.Errors.push_back({File, E.what()});
        }
    }

    ParamDefRepository::LoadResult ParamDefRepository::AddFolder(const fs::path& Folder, bool Recursive) {
        if (!fs::is_directory(Folder)) {
            throw BinaryException("Paramdef folder does not exist: " + Folder.string());
        }

        // Remember it for Reload (once, however many times it's added).
        const auto Known = std::find_if(Folders.begin(), Folders.end(), [&](const WatchedFolder& F) { return F.Path == Folder; });
        if (Known == Folders.end()) {
            Folders.push_back({Folder, Recursive});
        } else {
            Known->Recursive = Recursive;
        }

        std::vector<fs::path> Files;
        auto Collect = [&](auto Iterator) {
            for (const auto& Item : Iterator) {
                if (Item.is_regular_file() && HasXmlExtension(Item.path())) Files.push_back(Item.path());
            }
        };
        if (Recursive) {
            Collect(fs::recursive_directory_iterator(Folder, fs::directory_options::skip_permission_denied));
        } else {
            Collect(fs::directory_iterator(Folder, fs::directory_options::skip_permission_denied));
        }
        std::sort(Files.begin(), Files.end());  // a fixed order, so which of two duplicate defs wins is predictable

        LoadResult Result;
        for (const fs::path& File : Files) LoadFile(File, Folder, Result);
        return Result;
    }

    ParamDefRepository::LoadResult ParamDefRepository::AddFile(const fs::path& File) {
        if (!fs::is_regular_file(File)) {
            throw BinaryException("Paramdef file does not exist: " + File.string());
        }
        LoadResult Result;
        LoadFile(File, {}, Result);
        return Result;
    }

    void ParamDefRepository::Add(PARAMDEF Def) {
        Insert(std::move(Def), {});
    }

    void ParamDefRepository::Clear() {
        Entries.clear();
        Folders.clear();
    }

    ParamDefRepository::LoadResult ParamDefRepository::Reload() {
        Entries.erase(std::remove_if(Entries.begin(), Entries.end(), [](const std::unique_ptr<Entry>& E) { return !E->Folder.empty(); }),
                      Entries.end());

        LoadResult Total;
        const std::vector<WatchedFolder> ToLoad = Folders;
        for (const WatchedFolder& Folder : ToLoad) {
            if (!fs::is_directory(Folder.Path)) {
                Total.Errors.push_back({Folder.Path, "folder no longer exists"});
                continue;
            }
            LoadResult Result = AddFolder(Folder.Path, Folder.Recursive);
            Total.Loaded += Result.Loaded;
            Total.Replaced += Result.Replaced;
            Total.Skipped += Result.Skipped;
            Total.Errors.insert(Total.Errors.end(), Result.Errors.begin(), Result.Errors.end());
        }
        return Total;
    }

    const PARAMDEF* ParamDefRepository::Find(const PARAM& Param) const {
        for (const std::unique_ptr<Entry>& E : Entries) {
            if (Param.Matches(E->Def)) return &E->Def;
        }
        return nullptr;
    }

    std::vector<const PARAMDEF*> ParamDefRepository::FindCandidates(std::string_view ParamType) const {
        std::vector<const PARAMDEF*> Found;
        for (const std::unique_ptr<Entry>& E : Entries) {
            if (E->Def.ParamType == ParamType) Found.push_back(&E->Def);
        }
        std::stable_sort(Found.begin(), Found.end(), [](const PARAMDEF* A, const PARAMDEF* B) { return A->DataVersion > B->DataVersion; });
        return Found;
    }

    const PARAMDEF* ParamDefRepository::FindByType(std::string_view ParamType) const {
        const auto Found = FindCandidates(ParamType);
        return Found.empty() ? nullptr : Found.front();
    }

    std::vector<const PARAMDEF*> ParamDefRepository::All() const {
        std::vector<const PARAMDEF*> Result;
        Result.reserve(Entries.size());
        for (const std::unique_ptr<Entry>& E : Entries) Result.push_back(&E->Def);
        return Result;
    }
}  // namespace Souls
