//
// Created by Jake Rieger on 10/7/2026.
//

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

    // Non-template half of SoulsFile: the compression setting and everything about writing. Use SoulsFile<T> as the
    // actual base class of a format; this exists so code can hold or write any format through one type.
    class SOULS_API ISoulsFile {
    public:
        virtual ~ISoulsFile()                      = default;
        ISoulsFile(const ISoulsFile&)              = default;
        ISoulsFile(ISoulsFile&&)                   = default;
        ISoulsFile& operator=(const ISoulsFile&)   = default;
        ISoulsFile& operator=(ISoulsFile&&)        = default;

        // Compression used by Write(). Set to whatever was detected when the file was read.
        DCX::Type Compression = DCX::Type::None;

        // Returns false and sets Error if the file isn't in a state that can be written. The write functions call
        // this first and throw the error.
        virtual bool Validate(std::exception_ptr& Error);

        std::vector<uint8_t> Write();
        std::vector<uint8_t> Write(DCX::Type Compression);
        void Write(const std::filesystem::path& Path);
        void Write(const std::filesystem::path& Path, DCX::Type Compression);

    protected:
        ISoulsFile() = default;

        // Format hooks. A format overrides the ones it supports; the defaults throw BinaryException. IsImpl should
        // only peek (check magic/version) and must be safe to call on arbitrary, even too-short, data.
        virtual bool IsImpl(BinaryReader& Reader);
        virtual void ReadImpl(BinaryReader& Reader);
        virtual void WriteImpl(BinaryWriter& Writer);

        // Helpers for Validate overrides.
        template<typename Pointer>
        static bool ValidateNull(const Pointer& Value, const std::string& Message, std::exception_ptr& Error) {
            if (!Value) {
                Error = std::make_exception_ptr(std::invalid_argument(Message));
                return false;
            }
            Error = nullptr;
            return true;
        }

        static bool ValidateIndex(int64_t Count, int64_t Index, const std::string& Message, std::exception_ptr& Error);

    private:
        void Write(BinaryWriter& Writer, DCX::Type Compression);
    };

    // Base class for a format that may be wrapped in DCX. Derive with CRTP and override the hooks:
    //
    //     class FMG : public SoulsFile<FMG> {
    //     protected:
    //         bool IsImpl(BinaryReader& Reader) override;
    //         void ReadImpl(BinaryReader& Reader) override;
    //         void WriteImpl(BinaryWriter& Writer) override;
    //     };
    //
    // T must be default-constructible and movable. Reading and detection transparently decompress DCX input.
    template<typename T>
    class SoulsFile : public ISoulsFile {
    public:
#pragma region Is
        // Whether the data (after any DCX decompression) is this format.
        static bool Is(std::span<const uint8_t> Data) {
            if (Data.empty()) {
                return false;
            }
            BinaryReader Source(Data);
            return IsReader(Source);
        }

        static bool Is(const std::filesystem::path& Path) {
            BinaryReader Source(Path);
            if (Source.Length() == 0) {
                return false;
            }
            return IsReader(Source);
        }
#pragma endregion

#pragma region Read
        static T Read(std::span<const uint8_t> Data) {
            BinaryReader Source(Data);
            return ReadReader(Source);
        }

        static T Read(const std::filesystem::path& Path) {
            BinaryReader Source(Path);
            return ReadReader(Source);
        }
#pragma endregion

#pragma region IsRead
        // Reads the file only if it's this format; nullopt otherwise.
        static std::optional<T> IsRead(std::span<const uint8_t> Data) {
            BinaryReader Source(Data);
            return IsReadReader(Source);
        }

        static std::optional<T> IsRead(const std::filesystem::path& Path) {
            BinaryReader Source(Path);
            return IsReadReader(Source);
        }
#pragma endregion

    protected:
        SoulsFile() = default;

    private:
        // Hooks are protected in ISoulsFile, so go through SoulsFile<T> (never T, which may redeclare them).
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
            if (!AsBase(File).IsImpl(Reader)) {
                return std::nullopt;
            }
            Reader.Seek(0);
            File.Compression = Decompressed.Compression();
            AsBase(File).ReadImpl(Reader);
            return File;
        }
    };

}  // namespace Souls
