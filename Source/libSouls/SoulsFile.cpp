//
// Created by Jake Rieger on 10/7/2026.
//

#include "SoulsFile.hpp"

namespace Souls {
    bool ISoulsFile::Validate(std::exception_ptr& Error) {
        Error = nullptr;
        return true;
    }

    bool ISoulsFile::ValidateIndex(int64_t Count, int64_t Index, const std::string& Message, std::exception_ptr& Error) {
        if (Index < 0 || Index >= Count) {
            Error = std::make_exception_ptr(std::out_of_range(Message));
            return false;
        }
        Error = nullptr;
        return true;
    }

    bool ISoulsFile::IsImpl(BinaryReader&) {
        throw BinaryException("Is is not implemented for this format.");
    }

    void ISoulsFile::ReadImpl(BinaryReader&) {
        throw BinaryException("Read is not implemented for this format.");
    }

    void ISoulsFile::WriteImpl(BinaryWriter&) {
        throw BinaryException("Write is not implemented for this format.");
    }

    void ISoulsFile::Write(BinaryWriter& Writer, DCX::Type Compression) {
        if (Compression == DCX::Type::None) {
            WriteImpl(Writer);
            return;
        }
        // Formats are written uncompressed first, then wrapped.
        BinaryWriter Uncompressed(Endian::Little);
        WriteImpl(Uncompressed);
        const std::vector<uint8_t> Bytes = Uncompressed.ToBytes();
        DCX::Compress(Bytes, Writer, Compression);
    }

    std::vector<uint8_t> ISoulsFile::Write() {
        return Write(Compression);
    }

    std::vector<uint8_t> ISoulsFile::Write(DCX::Type Compression) {
        std::exception_ptr Error;
        if (!Validate(Error)) {
            std::rethrow_exception(Error);
        }

        BinaryWriter Writer(Endian::Little);
        Write(Writer, Compression);
        return Writer.ToBytes();
    }

    void ISoulsFile::Write(const std::filesystem::path& Path) {
        Write(Path, Compression);
    }

    void ISoulsFile::Write(const std::filesystem::path& Path, DCX::Type Compression) {
        std::exception_ptr Error;
        if (!Validate(Error)) {
            std::rethrow_exception(Error);
        }

        if (Path.has_parent_path()) {
            std::filesystem::create_directories(Path.parent_path());
        }
        BinaryWriter Writer(Path, Endian::Little);
        Write(Writer, Compression);
        Writer.Finish();
    }
}  // namespace Souls
