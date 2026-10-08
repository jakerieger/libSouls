//
// Created by Jake Rieger on 10/7/2026.
//

#include "BXFReader.hpp"
#include "BXFHeaders.hpp"

#include <algorithm>
#include <cctype>

namespace Souls {
    namespace fs = std::filesystem;

    struct BXFReader::Impl {
        std::vector<BinderFileHeader> Headers;
        std::unique_ptr<BinaryReader> Data;
    };

    namespace {
        std::string Lower(std::string_view Text) {
            std::string Result(Text);
            std::transform(Result.begin(), Result.end(), Result.begin(), [](unsigned char C) { return std::tolower(C); });
            return Result;
        }

        // The part of a stored path after its last folder separator.
        std::string_view LastComponent(std::string_view Path) {
            const size_t Slash = Path.find_last_of("\\/");
            return Slash == std::string_view::npos ? Path : Path.substr(Slash + 1);
        }

        std::vector<uint8_t> ReadWholeFile(const fs::path& Path) {
            BinaryReader Reader(Path);
            return Reader.ReadBytes(static_cast<size_t>(Reader.Length()));
        }
    }  // namespace

    BXFReader::BXFReader(const fs::path& HeaderPath, const fs::path& DataPath) : State(std::make_unique<Impl>()) {
        // Header files are small compared to the data they describe, so this one is read whole.
        const std::vector<uint8_t> Header = ReadWholeFile(HeaderPath);
        State->Data                       = std::make_unique<BinaryReader>(DataPath);
        Initialize(Header);
    }

    BXFReader::BXFReader(std::span<const uint8_t> Header, const fs::path& DataPath) : State(std::make_unique<Impl>()) {
        State->Data = std::make_unique<BinaryReader>(DataPath);
        Initialize(Header);
    }

    BXFReader::BXFReader(std::span<const uint8_t> Header, std::vector<uint8_t> Data) : State(std::make_unique<Impl>()) {
        State->Data = std::make_unique<BinaryReader>(std::move(Data));
        Initialize(Header);
    }

    BXFReader::~BXFReader()                                  = default;
    BXFReader::BXFReader(BXFReader&&) noexcept               = default;
    BXFReader& BXFReader::operator=(BXFReader&&) noexcept    = default;

    void BXFReader::Initialize(std::span<const uint8_t> Header) {
        if (Header.size() < 4) {
            throw BinaryException("BXF header file is too small");
        }

        BinaryReader HeaderReader(Header);
        const bool IsBXF3 = Header[0] == 'B' && Header[1] == 'H' && Header[2] == 'F' && Header[3] == '3';
        const bool IsBXF4 = Header[0] == 'B' && Header[1] == 'H' && Header[2] == 'F' && Header[3] == '4';
        if (!IsBXF3 && !IsBXF4) {
            throw BinaryException("The header file is not a BXF3 or BXF4 header (BHF3/BHF4)");
        }

        if (IsBXF3) {
            BXF3 Settings;
            BXFHeaders::ReadBDF3(*State->Data);
            State->Headers = BXFHeaders::ReadBHF3(Settings, HeaderReader);
            Kind           = Generation::BXF3;
            Version        = Settings.Version;
            Format         = Settings.Format;
            BigEndian      = Settings.BigEndian;
            BitBigEndian   = Settings.BitBigEndian;
        } else {
            BXF4 Settings;
            BXFHeaders::ReadBDF4(*State->Data);
            State->Headers = BXFHeaders::ReadBHF4(Settings, HeaderReader);
            Kind           = Generation::BXF4;
            Version        = Settings.Version;
            Format         = Settings.Format;
            BigEndian      = Settings.BigEndian;
            BitBigEndian   = Settings.BitBigEndian;
            Unk04          = Settings.Unk04;
            Unk05          = Settings.Unk05;
            Unicode        = Settings.Unicode;
            Extended       = Settings.Extended;
        }

        Files.reserve(State->Headers.size());
        for (const BinderFileHeader& Header : State->Headers) {
            FileInfo Info;
            Info.Flags            = Header.Flags;
            Info.ID               = Header.ID;
            Info.Name             = Header.Name;
            Info.CompressedSize   = Header.CompressedSize;
            Info.UncompressedSize = Header.UncompressedSize;
            Info.DataOffset       = Header.DataOffset;
            Files.push_back(std::move(Info));
        }
    }

    const BXFReader::FileInfo& BXFReader::File(size_t Index) const {
        if (Index >= Files.size()) {
            throw BinaryException("File index " + std::to_string(Index) + " is out of range (" + std::to_string(Files.size()) + " files)");
        }
        return Files[Index];
    }

    std::optional<size_t> BXFReader::IndexOf(std::string_view Name, bool IgnoreCase) const {
        const std::string Wanted = IgnoreCase ? Lower(Name) : std::string(Name);
        for (size_t I = 0; I < Files.size(); ++I) {
            if (!Files[I].Name) continue;
            if ((IgnoreCase ? Lower(*Files[I].Name) : *Files[I].Name) == Wanted) return I;
        }
        return std::nullopt;
    }

    std::optional<size_t> BXFReader::IndexOfFileName(std::string_view FileName) const {
        const std::string Wanted = Lower(LastComponent(FileName));
        for (size_t I = 0; I < Files.size(); ++I) {
            if (Files[I].Name && Lower(LastComponent(*Files[I].Name)) == Wanted) return I;
        }
        return std::nullopt;
    }

    BinderFile BXFReader::ReadFile(size_t Index) {
        File(Index);  // range check
        return State->Headers[Index].ReadFileData(*State->Data);
    }

    std::vector<uint8_t> BXFReader::ReadBytes(size_t Index) {
        return ReadFile(Index).Bytes;
    }

    TPF BXFReader::ReadTPF(size_t Index) {
        const std::vector<uint8_t> Bytes = ReadBytes(Index);
        return TPF::Read(Bytes);
    }

    TPF BXFReader::ReadTPF(std::string_view FileName) {
        const auto Index = IndexOfFileName(FileName);
        if (!Index) {
            throw BinaryException("No file named \"" + std::string(FileName) + "\" in this archive");
        }
        return ReadTPF(*Index);
    }
}  // namespace Souls
