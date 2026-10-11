//
// Created by Jake Rieger on 10/11/2026.
//

#include "ParamMetadata.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <fstream>
#include <sstream>

namespace Souls {
    namespace {
        std::string Lower(std::string_view Text) {
            std::string Result(Text);
            std::transform(Result.begin(), Result.end(), Result.begin(), [](unsigned char C) { return static_cast<char>(std::tolower(C)); });
            return Result;
        }

        std::string ReadText(const std::filesystem::path& Path) {
            std::ifstream File(Path, std::ios::binary);
            if (!File) {
                throw BinaryException("Failed to open file for reading: " + Path.string());
            }
            std::ostringstream Buffer;
            Buffer << File.rdbuf();
            std::string Text = Buffer.str();
            if (Text.size() >= 3 && static_cast<uint8_t>(Text[0]) == 0xEF && static_cast<uint8_t>(Text[1]) == 0xBB && static_cast<uint8_t>(Text[2]) == 0xBF) {
                Text.erase(0, 3);
            }
            return Text;
        }

        std::string_view Trim(std::string_view Text) {
            while (!Text.empty() && std::isspace(static_cast<unsigned char>(Text.front()))) Text.remove_prefix(1);
            while (!Text.empty() && std::isspace(static_cast<unsigned char>(Text.back()))) Text.remove_suffix(1);
            return Text;
        }

        std::vector<std::filesystem::path> FilesWithExtension(const std::filesystem::path& Folder, std::string_view Extension) {
            if (!std::filesystem::is_directory(Folder)) {
                throw BinaryException("The folder doesn't exist: " + Folder.string());
            }
            std::vector<std::filesystem::path> Files;
            for (const auto& Entry : std::filesystem::directory_iterator(Folder)) {
                if (Entry.is_regular_file() && Lower(Entry.path().extension().string()) == Extension) {
                    Files.push_back(Entry.path());
                }
            }
            std::sort(Files.begin(), Files.end());
            return Files;
        }
    }  // namespace

#pragma region ParamNameRepository
    void ParamNameRepository::Add(std::string_view ParamName, int32_t ID, std::string Name) {
        Table& T = Tables[Lower(ParamName)];
        if (T.Name.empty()) {
            T.Name = std::string(ParamName);
        }
        T.Names[ID] = std::move(Name);
    }

    MetadataLoadResult ParamNameRepository::AddFile(const std::filesystem::path& File) {
        MetadataLoadResult Result;
        try {
            const std::string Text = ReadText(File);
            const std::string ParamName = File.stem().string();
            size_t Added = 0;
            std::istringstream Lines(Text);
            std::string Line;
            while (std::getline(Lines, Line)) {
                const std::string_view Trimmed = Trim(Line);
                if (Trimmed.empty()) continue;
                // "<ID> <name>"; a line with just an ID names the row with nothing.
                const size_t Space = Trimmed.find_first_of(" \t");
                const std::string_view IdText = Space == std::string_view::npos ? Trimmed : Trimmed.substr(0, Space);
                int32_t ID = 0;
                const auto [End, Error] = std::from_chars(IdText.data(), IdText.data() + IdText.size(), ID);
                if (Error != std::errc() || End != IdText.data() + IdText.size()) {
                    continue;  // not a row line (a comment or a header)
                }
                const std::string_view Name = Space == std::string_view::npos ? std::string_view() : Trim(Trimmed.substr(Space));
                Add(ParamName, ID, std::string(Name));
                ++Added;
            }
            Result.Files   = 1;
            Result.Entries = Added;
        } catch (const std::exception& E) {
            Result.Errors.push_back({File, E.what()});
        }
        return Result;
    }

    MetadataLoadResult ParamNameRepository::AddFolder(const std::filesystem::path& Folder) {
        MetadataLoadResult Total;
        for (const auto& File : FilesWithExtension(Folder, ".txt")) {
            MetadataLoadResult One = AddFile(File);
            Total.Files += One.Files;
            Total.Entries += One.Entries;
            Total.Errors.insert(Total.Errors.end(), One.Errors.begin(), One.Errors.end());
        }
        return Total;
    }

    const std::map<int32_t, std::string>* ParamNameRepository::Find(std::string_view ParamName) const {
        const auto Found = Tables.find(Lower(ParamName));
        return Found == Tables.end() ? nullptr : &Found->second.Names;
    }

    std::optional<std::string> ParamNameRepository::Find(std::string_view ParamName, int32_t ID) const {
        const auto* Names = Find(ParamName);
        if (!Names) return std::nullopt;
        const auto Found = Names->find(ID);
        if (Found == Names->end()) return std::nullopt;
        return Found->second;
    }

    size_t ParamNameRepository::Apply(PARAM& Param, std::string_view ParamName, bool Overwrite) const {
        const auto* Names = Find(ParamName);
        if (!Names) return 0;
        size_t Named = 0;
        for (PARAM::Row& Row : Param.Rows) {
            if (Row.Name && !Row.Name->empty() && !Overwrite) continue;
            const auto Found = Names->find(Row.ID);
            if (Found != Names->end()) {
                Row.Name = Found->second;
                ++Named;
            }
        }
        return Named;
    }

    std::vector<std::string> ParamNameRepository::ParamNames() const {
        std::vector<std::string> Result;
        for (const auto& [Key, T] : Tables) Result.push_back(T.Name);
        return Result;
    }
#pragma endregion

#pragma region ParamEnumRepository
    void ParamEnumRepository::Add(PARAMTDF Enum) {
        const std::string Key = Lower(Enum.Name);
        Enums.insert_or_assign(Key, std::move(Enum));
    }

    MetadataLoadResult ParamEnumRepository::AddFile(const std::filesystem::path& File) {
        MetadataLoadResult Result;
        try {
            PARAMTDF Parsed(ReadText(File));
            Result.Entries = Parsed.Entries.size();
            Add(std::move(Parsed));
            Result.Files = 1;
        } catch (const std::exception& E) {
            Result.Errors.push_back({File, E.what()});
        }
        return Result;
    }

    MetadataLoadResult ParamEnumRepository::AddFolder(const std::filesystem::path& Folder) {
        MetadataLoadResult Total;
        for (const auto& File : FilesWithExtension(Folder, ".tdf")) {
            MetadataLoadResult One = AddFile(File);
            Total.Files += One.Files;
            Total.Entries += One.Entries;
            Total.Errors.insert(Total.Errors.end(), One.Errors.begin(), One.Errors.end());
        }
        return Total;
    }

    const PARAMTDF* ParamEnumRepository::Find(std::string_view Name) const {
        const auto Found = Enums.find(Lower(Name));
        return Found == Enums.end() ? nullptr : &Found->second;
    }

    std::vector<const PARAMTDF*> ParamEnumRepository::All() const {
        std::vector<const PARAMTDF*> Result;
        for (const auto& [Key, E] : Enums) Result.push_back(&E);
        return Result;
    }
#pragma endregion
}  // namespace Souls
