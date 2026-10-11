// Copyright (C) 2026 Jake Rieger
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, version 3. This program is distributed in the hope that it will
// be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU General
// Public License along with this program. If not, see https://www.gnu.org/licenses/.

#pragma once

#include <libSouls/Souls.hpp>
#include <libSouls/BinaryReader.hpp>
#include <libSouls/BinaryWriter.hpp>
#include <libSouls/Formats/DCX.hpp>

#include <concepts>
#include <exception>
#include <filesystem>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace Souls {

    /// @brief Non-template half of SoulsFile: the compression setting and everything about writing.
    ///
    /// Use SoulsFile<T> as the actual base class of a format; this exists so code can hold or write any format through
    /// one type.
    class SOULS_API ISoulsFile {
    public:
        virtual ~ISoulsFile() = default;
        /// @brief Files are copyable and movable values.
        ISoulsFile(const ISoulsFile&) = default;
        /// @brief Files are copyable and movable values.
        ISoulsFile(ISoulsFile&&) = default;
        /// @brief Files are copyable and movable values.
        ISoulsFile& operator=(const ISoulsFile&) = default;
        /// @brief Files are copyable and movable values.
        ISoulsFile& operator=(ISoulsFile&&) = default;

        /// @brief Compression used by Write().
        ///
        /// Set to whatever was detected when the file was read.
        DCX::Type Compression = DCX::Type::None;

        /// @brief Returns false and sets Error if the file isn't in a state that can be written.
        ///
        /// The write functions call this first and throw the error.
        virtual bool Validate(std::exception_ptr& Error);

        /// @brief Writes the file to memory, compressed with Compression.
        /// @throws BinaryException (or what Validate reports) if the file can't be written.
        std::vector<uint8_t> Write();
        /// @brief Writes the file to memory with the given compression (Compression is left unchanged).
        std::vector<uint8_t> Write(DCX::Type Compression);
        /// @brief Writes the file to disk, compressed with Compression, creating the folder if needed.
        void Write(const std::filesystem::path& Path);
        /// @brief Writes the file to disk with the given compression, creating the folder if needed.
        void Write(const std::filesystem::path& Path, DCX::Type Compression);

    protected:
        /// @brief Only formats (through SoulsFile) create these.
        ISoulsFile() = default;

        /// @brief Whether the (decompressed) data is this format.
        ///
        /// A format hook: formats override the hooks they support, and the defaults throw BinaryException. IsImpl
        /// should only peek (check magic and version) and must be safe to call on arbitrary, even too-short, data.
        virtual bool IsImpl(BinaryReader& Reader);
        /// @brief Reads the file from its decompressed data (a format hook; see IsImpl).
        virtual void ReadImpl(BinaryReader& Reader);
        /// @brief Writes the file's uncompressed data (a format hook; see IsImpl).
        virtual void WriteImpl(BinaryWriter& Writer);

        /// @brief A Validate helper: fails with Message if Value is null.
        template<typename Pointer>
        static bool ValidateNull(const Pointer& Value, const std::string& Message, std::exception_ptr& Error) {
            if (!Value) {
                Error = std::make_exception_ptr(std::invalid_argument(Message));
                return false;
            }
            Error = nullptr;
            return true;
        }

        /// @brief A Validate helper: fails with Message unless 0 <= Index < Count.
        static bool ValidateIndex(int64_t Count, int64_t Index, const std::string& Message, std::exception_ptr& Error);

    private:
        void Write(BinaryWriter& Writer, DCX::Type Compression);
    };

    /// @brief Base class for a format that may be wrapped in DCX.
    ///
    /// Derive with CRTP and override the hooks:
    ///
    /// @code
    /// class FMG : public SoulsFile<FMG> {
    /// protected:
    ///     bool IsImpl(BinaryReader& Reader) override;
    ///     void ReadImpl(BinaryReader& Reader) override;
    ///     void WriteImpl(BinaryWriter& Writer) override;
    /// };
    /// @endcode
    ///
    /// T must be default-constructible and movable. Reading and detection transparently decompress DCX input.
    template<typename T>
    class SoulsFile : public ISoulsFile {
    public:
#pragma region Is
        /// @brief Whether the data (after any DCX decompression) is this format.
        static bool Is(std::span<const uint8_t> Data) {
            if (Data.empty()) { return false; }
            BinaryReader Source(Data);
            return IsReader(Source);
        }

        /// @brief Whether the file on disk (after any DCX decompression) is this format.
        static bool Is(const std::filesystem::path& Path) {
            BinaryReader Source(Path);
            if (Source.Length() == 0) { return false; }
            return IsReader(Source);
        }
#pragma endregion

#pragma region Read
        /// @brief Reads the file from memory, decompressing it first if it's DCX.
        /// @throws BinaryException if the data isn't valid for the format.
        static T Read(std::span<const uint8_t> Data) {
            BinaryReader Source(Data);
            return ReadReader(Source);
        }

        /// @brief Reads the file from disk, decompressing it first if it's DCX.
        /// @throws BinaryException if the file can't be read or isn't valid for the format.
        static T Read(const std::filesystem::path& Path) {
            BinaryReader Source(Path);
            return ReadReader(Source);
        }
#pragma endregion

#pragma region IsRead
        /// @brief Reads the file only if it's this format; nullopt otherwise.
        static std::optional<T> IsRead(std::span<const uint8_t> Data) {
            BinaryReader Source(Data);
            return IsReadReader(Source);
        }

        /// @brief Reads the file on disk only if it's this format; nullopt otherwise.
        static std::optional<T> IsRead(const std::filesystem::path& Path) {
            BinaryReader Source(Path);
            return IsReadReader(Source);
        }
#pragma endregion

    protected:
        /// @brief Formats are created through their own constructors (or Read).
        SoulsFile() = default;

    private:
        /// @brief Hooks are protected in ISoulsFile, so go through SoulsFile<T> (never T, which may redeclare them).
        static SoulsFile& AsBase(T& Value) { return static_cast<SoulsFile&>(Value); }

        static bool IsReader(BinaryReader& Source) {
            T Probe;
            DCX::DecompressedReader Decompressed(Source);
            return AsBase(Probe).IsImpl(Decompressed.Reader());
        }

        static T ReadReader(BinaryReader& Source) {
            T File;
            DCX::DecompressedReader Decompressed(Source);
            File.Compression = Decompressed.Compression();
            AsBase(File).ReadImpl(Decompressed.Reader());
            return File;
        }

        static std::optional<T> IsReadReader(BinaryReader& Source) {
            T File;
            DCX::DecompressedReader Decompressed(Source);
            BinaryReader& Reader = Decompressed.Reader();
            if (!AsBase(File).IsImpl(Reader)) { return std::nullopt; }
            Reader.Seek(0);
            File.Compression = Decompressed.Compression();
            AsBase(File).ReadImpl(Reader);
            return File;
        }
    };

}  // namespace Souls
