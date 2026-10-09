//
// Created by Jake Rieger on 10/8/2026.
//

#include "DRB.hpp"

#include <algorithm>
#include <queue>

namespace Souls {
    namespace {
        constexpr int32_t AnikSize = 0x20;
        constexpr int32_t AnioSize = 0x10;
        constexpr int32_t ScdkSize = 0x20;
        constexpr int32_t ScdoSize = 0x10;
        constexpr int32_t DlgoSize = 0x20;

        using Strings   = DRB::StringTable;
        using Offsets   = DRB::StringOffsets;
        using OffsetQueue = std::queue<int32_t>;

        int32_t FourCC(const char* Name) {
            return static_cast<uint8_t>(Name[0]) | (static_cast<uint8_t>(Name[1]) << 8) | (static_cast<uint8_t>(Name[2]) << 16) |
                   (static_cast<uint8_t>(Name[3]) << 24);
        }

        // Colors are stored as one 32-bit value laid out RGBA from the top byte down.
        Color ReadColor(BinaryReader& Reader) {
            const uint32_t Value = Reader.ReadUInt32();
            return Color::FromArgb(static_cast<uint8_t>(Value), static_cast<uint8_t>(Value >> 24), static_cast<uint8_t>(Value >> 16),
                                   static_cast<uint8_t>(Value >> 8));
        }

        void WriteColor(BinaryWriter& Writer, const Color& C) {
            Writer.WriteUInt32(C.A | (static_cast<uint32_t>(C.R) << 24) | (static_cast<uint32_t>(C.G) << 16) | (static_cast<uint32_t>(C.B) << 8));
        }

        const std::string& StringAt(const Strings& Table, int32_t Offset) {
            const auto Found = Table.find(Offset);
            if (Found == Table.end()) {
                throw BinaryException("DRB string offset " + std::to_string(Offset) + " is not in the string table");
            }
            return Found->second;
        }

        int32_t OffsetOf(const Offsets& Table, const std::string& Text) {
            const auto Found = Table.find(Text);
            if (Found == Table.end()) {
                throw BinaryException("DRB string missing from string table: " + Text);
            }
            return Found->second;
        }

        int32_t Pop(OffsetQueue& Queue) {
            if (Queue.empty()) {
                throw BinaryException("DRB offset queue ran out");
            }
            const int32_t Value = Queue.front();
            Queue.pop();
            return Value;
        }

        void ReadNullBlock(BinaryReader& Reader, const char* Name) {
            Reader.Assert<int32_t>(FourCC(Name));
            Reader.Assert<int32_t>(0);
            Reader.Assert<int32_t>(0);
            Reader.Assert<int32_t>(0);
        }

        void WriteNullBlock(BinaryWriter& Writer, const char* Name) {
            Writer.WriteInt32(FourCC(Name));
            Writer.WriteInt32(0);
            Writer.WriteInt32(0);
            Writer.WriteInt32(0);
        }

        int64_t ReadBlobBlock(BinaryReader& Reader, const char* Name) {
            Reader.Assert<int32_t>(FourCC(Name));
            const int32_t Size = Reader.ReadInt32();
            Reader.Assert<int32_t>(1);
            Reader.Assert<int32_t>(0);
            const int64_t Start = Reader.Position();
            Reader.Skip(Size);
            return Start;
        }

        int64_t WriteBlobBlock(BinaryWriter& Writer, const char* Name) {
            Writer.WriteInt32(FourCC(Name));
            Writer.Reserve<int32_t>(std::string("BlobBlockSize") + Name);
            Writer.WriteInt32(1);
            Writer.WriteInt32(0);
            return Writer.Position();
        }

        void FinishBlobBlock(BinaryWriter& Writer, const char* Name, int64_t Start) {
            Writer.Align(0x10);
            Writer.Fill<int32_t>(std::string("BlobBlockSize") + Name, static_cast<int32_t>(Writer.Position() - Start));
        }

        std::vector<uint8_t> ReadBlobBytes(BinaryReader& Reader, const char* Name) {
            Reader.Assert<int32_t>(FourCC(Name));
            const int32_t Size = Reader.ReadInt32();
            Reader.Assert<int32_t>(1);
            Reader.Assert<int32_t>(0);
            return Reader.ReadBytes(static_cast<size_t>(Size));
        }

        void WriteBlobBytes(BinaryWriter& Writer, const char* Name, const std::vector<uint8_t>& Bytes) {
            Writer.WriteInt32(FourCC(Name));
            Writer.Reserve<int32_t>("BlobSize");
            Writer.WriteInt32(1);
            Writer.WriteInt32(0);
            const int64_t Start = Writer.Position();
            Writer.WriteArray(Bytes);
            Writer.Align(0x10);
            Writer.Fill<int32_t>("BlobSize", static_cast<int32_t>(Writer.Position() - Start));
        }

        int64_t ReadBlockHeader(BinaryReader& Reader, const char* Name, int32_t& Count, int32_t& Size) {
            Reader.Assert<int32_t>(FourCC(Name));
            Size  = Reader.ReadInt32();
            Count = Reader.ReadInt32();
            Reader.Assert<int32_t>(0);
            return Reader.Position();
        }

        int64_t WriteBlockHeader(BinaryWriter& Writer, const char* Name) {
            Writer.WriteInt32(FourCC(Name));
            Writer.Reserve<int32_t>(std::string("BlockSize") + Name);
            Writer.Reserve<int32_t>(std::string("BlockCount") + Name);
            Writer.WriteInt32(0);
            return Writer.Position();
        }

        void FinishBlockHeader(BinaryWriter& Writer, const char* Name, int64_t Start, int32_t Count) {
            Writer.Align(0x10);
            Writer.Fill<int32_t>(std::string("BlockSize") + Name, static_cast<int32_t>(Writer.Position() - Start));
            Writer.Fill<int32_t>(std::string("BlockCount") + Name, Count);
        }

        const char* ShapeNames[] = {"Dialog", "GouraudFrame", "GouraudRect", "GouraudSprite", "Mask", "MonoFrame",
                                    "MonoRect", "Null", "ScrollText", "Sprite", "Text"};
        const char* ControlNames[] = {"DmeCtrlScrollText", "FrpgMenuDlgObjContentsHelpItem", "Static"};
    }  // namespace

#pragma region Controls
    const char* DRB::Control::TypeName() const {
        return ControlNames[static_cast<int>(Type())];
    }

    void DRB::ScrollTextDummy::ReadData(BinaryReader& Reader) {
        Unk00 = Reader.ReadInt32();
    }
    void DRB::ScrollTextDummy::WriteData(BinaryWriter& Writer) const {
        Writer.WriteInt32(Unk00);
    }

    void DRB::HelpItem::ReadData(BinaryReader& Reader) {
        Unk00  = Reader.ReadInt32();
        Unk04  = Reader.ReadInt32();
        Unk08  = Reader.ReadInt32();
        Unk0C  = Reader.ReadInt32();
        Unk10  = Reader.ReadInt32();
        Unk14  = Reader.ReadInt32();
        TextID = Reader.ReadInt32();
    }
    void DRB::HelpItem::WriteData(BinaryWriter& Writer) const {
        Writer.WriteInt32(Unk00);
        Writer.WriteInt32(Unk04);
        Writer.WriteInt32(Unk08);
        Writer.WriteInt32(Unk0C);
        Writer.WriteInt32(Unk10);
        Writer.WriteInt32(Unk14);
        Writer.WriteInt32(TextID);
    }

    void DRB::Static::ReadData(BinaryReader& Reader) {
        Unk00 = Reader.ReadInt32();
    }
    void DRB::Static::WriteData(BinaryWriter& Writer) const {
        Writer.WriteInt32(Unk00);
    }
#pragma endregion

#pragma region Shapes
    const char* DRB::Shape::TypeName() const {
        return ShapeNames[static_cast<int>(Type())];
    }

    void DRB::Shape::ReadData(BinaryReader& Reader, DRBVersion Version, const StringTable& Table) {
        LeftEdge   = Reader.ReadInt16();
        TopEdge    = Reader.ReadInt16();
        RightEdge  = Reader.ReadInt16();
        BottomEdge = Reader.ReadInt16();
        if (Version == DRBVersion::DarkSoulsRemastered && Type() != ShapeType::Null) {
            ScalingOriginX = Reader.ReadInt16();
            ScalingOriginY = Reader.ReadInt16();
            ScalingMode    = Reader.ReadInt16();
            Reader.Assert<int16_t>(0);
        } else if (Version == DRBVersion::DarkSoulsRemasteredOriginOnly && Type() != ShapeType::Null) {
            ScalingOriginX = Reader.ReadInt16();
            ScalingOriginY = Reader.ReadInt16();
            ScalingMode    = 0;
        } else {
            ScalingOriginX = -1;
            ScalingOriginY = -1;
            ScalingMode    = 0;
        }
        ReadSpecific(Reader, Table);
    }

    void DRB::Shape::WriteData(BinaryWriter& Writer, DRBVersion Version, const StringOffsets& Table) const {
        Writer.WriteInt16(LeftEdge);
        Writer.WriteInt16(TopEdge);
        Writer.WriteInt16(RightEdge);
        Writer.WriteInt16(BottomEdge);
        if (Version == DRBVersion::DarkSoulsRemastered && Type() != ShapeType::Null) {
            Writer.WriteInt16(ScalingOriginX);
            Writer.WriteInt16(ScalingOriginY);
            Writer.WriteInt16(ScalingMode);
            Writer.WriteInt16(0);
        } else if (Version == DRBVersion::DarkSoulsRemasteredOriginOnly && Type() != ShapeType::Null) {
            Writer.WriteInt16(ScalingOriginX);
            Writer.WriteInt16(ScalingOriginY);
        }
        WriteSpecific(Writer, Table);
    }

    void DRB::SpriteBase::ReadSpecific(BinaryReader& Reader, const StringTable&) {
        TexLeftEdge   = Reader.ReadInt16();
        TexTopEdge    = Reader.ReadInt16();
        TexRightEdge  = Reader.ReadInt16();
        TexBottomEdge = Reader.ReadInt16();
        TextureIndex  = Reader.ReadInt16();
        Orientation   = static_cast<SpriteOrientation>(Reader.ReadByte());
        BlendMode     = static_cast<BlendingMode>(Reader.ReadByte());
    }

    void DRB::SpriteBase::WriteSpecific(BinaryWriter& Writer, const StringOffsets&) const {
        Writer.WriteInt16(TexLeftEdge);
        Writer.WriteInt16(TexTopEdge);
        Writer.WriteInt16(TexRightEdge);
        Writer.WriteInt16(TexBottomEdge);
        Writer.WriteInt16(TextureIndex);
        Writer.WriteByte(static_cast<uint8_t>(Orientation));
        Writer.WriteByte(static_cast<uint8_t>(BlendMode));
    }

    void DRB::TextBase::CollectStrings(std::vector<std::string>& Out) const {
        if (TextType == TxtType::Literal) {
            Out.push_back(TextLiteral);
        }
    }

    void DRB::TextBase::ReadSpecific(BinaryReader& Reader, const StringTable& Table) {
        BlendMode          = static_cast<BlendingMode>(Reader.ReadByte());
        const bool HasUnk  = Reader.ReadBool();
        LineSpacing        = Reader.ReadInt16();
        PaletteColor       = Reader.ReadInt32();
        CustomColor        = ReadColor(Reader);
        FontSize           = Reader.ReadInt16();
        Alignment          = static_cast<AlignFlags>(Reader.ReadByte());
        TextType           = static_cast<TxtType>(Reader.ReadByte());
        Reader.Assert<int32_t>(0x1C);  // local offset to the variable data below
        TextLiteral.clear();
        if (TextType == TxtType::Literal) {
            TextLiteral = StringAt(Table, Reader.ReadInt32());
            CharLength  = -1;
            TextID      = -1;
        } else if (TextType == TxtType::FMG) {
            CharLength = Reader.ReadInt32();
            TextID     = Reader.ReadInt32();
        } else if (TextType == TxtType::Dynamic) {
            CharLength = Reader.ReadInt32();
            TextID     = -1;
        } else {
            throw BinaryException("Unknown DRB text type");
        }
        ReadSubtype(Reader);
        Unknown.reset();
        if (HasUnk) {
            UnknownA A;
            A.Unk00                = Reader.ReadInt32();
            const int16_t HasSub   = Reader.Assert<int16_t>(0, 1);
            if (HasSub == 1) {
                UnknownB B;
                B.Unk00 = Reader.ReadInt32();
                B.Unk04 = Reader.ReadInt16();
                B.Unk06 = Reader.ReadInt16();
                B.Unk08 = Reader.ReadInt16();
                A.SubUnknown = B;
            }
            Unknown = A;
        }
    }

    void DRB::TextBase::WriteSpecific(BinaryWriter& Writer, const StringOffsets& Table) const {
        Writer.WriteByte(static_cast<uint8_t>(BlendMode));
        Writer.WriteBool(Unknown.has_value());
        Writer.WriteInt16(LineSpacing);
        Writer.WriteInt32(PaletteColor);
        WriteColor(Writer, CustomColor);
        Writer.WriteInt16(FontSize);
        Writer.WriteByte(static_cast<uint8_t>(Alignment));
        Writer.WriteByte(static_cast<uint8_t>(TextType));
        Writer.WriteInt32(0x1C);
        if (TextType == TxtType::Literal) {
            Writer.WriteInt32(OffsetOf(Table, TextLiteral));
        } else if (TextType == TxtType::FMG) {
            Writer.WriteInt32(CharLength);
            Writer.WriteInt32(TextID);
        } else {
            Writer.WriteInt32(CharLength);
        }
        WriteSubtype(Writer);
        if (Unknown) {
            Writer.WriteInt32(Unknown->Unk00);
            Writer.WriteInt16(Unknown->SubUnknown ? 1 : 0);
            if (Unknown->SubUnknown) {
                Writer.WriteInt32(Unknown->SubUnknown->Unk00);
                Writer.WriteInt16(Unknown->SubUnknown->Unk04);
                Writer.WriteInt16(Unknown->SubUnknown->Unk06);
                Writer.WriteInt16(Unknown->SubUnknown->Unk08);
            }
        }
    }

    void DRB::Dialog::ReadSpecific(BinaryReader& Reader, const StringTable&) {
        DlgIndex     = Reader.ReadInt16();
        Unk02        = Reader.ReadByte();
        Unk03        = Reader.ReadByte();
        PaletteColor = Reader.ReadInt32();
        CustomColor  = ReadColor(Reader);
        const bool HasUnk = Reader.ReadBool();
        Reader.Assert<uint8_t>(0);
        Reader.Assert<uint8_t>(0);
        Reader.Assert<uint8_t>(0);
        Unknown.reset();
        if (HasUnk) {
            UnknownA A;
            A.Unk00 = Reader.ReadInt16();
            A.Unk02 = Reader.ReadInt16();
            A.Unk04 = Reader.ReadInt32();
            Unknown = A;
        }
    }

    void DRB::Dialog::WriteSpecific(BinaryWriter& Writer, const StringOffsets&) const {
        Writer.WriteInt16(DlgIndex);
        Writer.WriteByte(Unk02);
        Writer.WriteByte(Unk03);
        Writer.WriteInt32(PaletteColor);
        WriteColor(Writer, CustomColor);
        Writer.WriteByte(Unknown ? 1 : 0);
        Writer.WriteByte(0);
        Writer.WriteByte(0);
        Writer.WriteByte(0);
        if (Unknown) {
            Writer.WriteInt16(Unknown->Unk00);
            Writer.WriteInt16(Unknown->Unk02);
            Writer.WriteInt32(Unknown->Unk04);
        }
    }

    void DRB::GouraudFrame::ReadSpecific(BinaryReader& Reader, const StringTable&) {
        BlendMode = static_cast<BlendingMode>(Reader.ReadByte());
        Reader.Assert<int16_t>(0);
        Thickness        = Reader.ReadByte();
        TopLeftColor     = ReadColor(Reader);
        TopRightColor    = ReadColor(Reader);
        BottomRightColor = ReadColor(Reader);
        BottomLeftColor  = ReadColor(Reader);
    }
    void DRB::GouraudFrame::WriteSpecific(BinaryWriter& Writer, const StringOffsets&) const {
        Writer.WriteByte(static_cast<uint8_t>(BlendMode));
        Writer.WriteInt16(0);
        Writer.WriteByte(Thickness);
        WriteColor(Writer, TopLeftColor);
        WriteColor(Writer, TopRightColor);
        WriteColor(Writer, BottomRightColor);
        WriteColor(Writer, BottomLeftColor);
    }

    void DRB::GouraudRect::ReadSpecific(BinaryReader& Reader, const StringTable&) {
        BlendMode = static_cast<BlendingMode>(Reader.ReadByte());
        Reader.Assert<int16_t>(0);
        Reader.Assert<uint8_t>(0);
        TopLeftColor     = ReadColor(Reader);
        TopRightColor    = ReadColor(Reader);
        BottomRightColor = ReadColor(Reader);
        BottomLeftColor  = ReadColor(Reader);
    }
    void DRB::GouraudRect::WriteSpecific(BinaryWriter& Writer, const StringOffsets&) const {
        Writer.WriteByte(static_cast<uint8_t>(BlendMode));
        Writer.WriteInt16(0);
        Writer.WriteByte(0);
        WriteColor(Writer, TopLeftColor);
        WriteColor(Writer, TopRightColor);
        WriteColor(Writer, BottomRightColor);
        WriteColor(Writer, BottomLeftColor);
    }

    void DRB::GouraudSprite::ReadSpecific(BinaryReader& Reader, const StringTable& Table) {
        SpriteBase::ReadSpecific(Reader, Table);
        TopLeftColor     = ReadColor(Reader);
        TopRightColor    = ReadColor(Reader);
        BottomRightColor = ReadColor(Reader);
        BottomLeftColor  = ReadColor(Reader);
    }
    void DRB::GouraudSprite::WriteSpecific(BinaryWriter& Writer, const StringOffsets& Table) const {
        SpriteBase::WriteSpecific(Writer, Table);
        WriteColor(Writer, TopLeftColor);
        WriteColor(Writer, TopRightColor);
        WriteColor(Writer, BottomRightColor);
        WriteColor(Writer, BottomLeftColor);
    }

    void DRB::Mask::ReadSpecific(BinaryReader& Reader, const StringTable& Table) {
        DlgoName = StringAt(Table, Reader.ReadInt32());
        Unk04    = Reader.ReadByte();
        Unk05    = Reader.ReadInt32();
        Unk09    = Reader.ReadInt32();
        Unk0D    = Reader.ReadInt16();
    }
    void DRB::Mask::WriteSpecific(BinaryWriter& Writer, const StringOffsets& Table) const {
        Writer.WriteInt32(OffsetOf(Table, DlgoName));
        Writer.WriteByte(Unk04);
        Writer.WriteInt32(Unk05);
        Writer.WriteInt32(Unk09);
        Writer.WriteInt16(Unk0D);
    }

    void DRB::MonoFrame::ReadSpecific(BinaryReader& Reader, const StringTable&) {
        BlendMode = static_cast<BlendingMode>(Reader.ReadByte());
        Reader.Assert<int16_t>(0);
        Thickness    = Reader.ReadByte();
        PaletteColor = Reader.ReadInt32();
        CustomColor  = ReadColor(Reader);
    }
    void DRB::MonoFrame::WriteSpecific(BinaryWriter& Writer, const StringOffsets&) const {
        Writer.WriteByte(static_cast<uint8_t>(BlendMode));
        Writer.WriteInt16(0);
        Writer.WriteByte(Thickness);
        Writer.WriteInt32(PaletteColor);
        WriteColor(Writer, CustomColor);
    }

    void DRB::MonoRect::ReadSpecific(BinaryReader& Reader, const StringTable&) {
        BlendMode = static_cast<BlendingMode>(Reader.ReadByte());
        Reader.Assert<int16_t>(0);
        Reader.Assert<uint8_t>(0);
        PaletteColor = Reader.ReadInt32();
        CustomColor  = ReadColor(Reader);
    }
    void DRB::MonoRect::WriteSpecific(BinaryWriter& Writer, const StringOffsets&) const {
        Writer.WriteByte(static_cast<uint8_t>(BlendMode));
        Writer.WriteInt16(0);
        Writer.WriteByte(0);
        Writer.WriteInt32(PaletteColor);
        WriteColor(Writer, CustomColor);
    }

    void DRB::ScrollText::ReadSubtype(BinaryReader& Reader) {
        UnkX00      = Reader.ReadInt16();
        UnkX02      = Reader.ReadInt16();
        UnkX04      = Reader.ReadInt16();
        UnkX06      = Reader.ReadInt16();
        ScrollSpeed = Reader.ReadInt16();
        UnkX0A      = Reader.ReadInt16();
    }
    void DRB::ScrollText::WriteSubtype(BinaryWriter& Writer) const {
        Writer.WriteInt16(UnkX00);
        Writer.WriteInt16(UnkX02);
        Writer.WriteInt16(UnkX04);
        Writer.WriteInt16(UnkX06);
        Writer.WriteInt16(ScrollSpeed);
        Writer.WriteInt16(UnkX0A);
    }

    void DRB::Sprite::ReadSpecific(BinaryReader& Reader, const StringTable& Table) {
        SpriteBase::ReadSpecific(Reader, Table);
        PaletteColor = Reader.ReadInt32();
        CustomColor  = ReadColor(Reader);
    }
    void DRB::Sprite::WriteSpecific(BinaryWriter& Writer, const StringOffsets& Table) const {
        SpriteBase::WriteSpecific(Writer, Table);
        Writer.WriteInt32(PaletteColor);
        WriteColor(Writer, CustomColor);
    }
#pragma endregion

    namespace {
        std::shared_ptr<DRB::Shape> MakeShape(const std::string& Type) {
            if (Type == "Dialog") return std::make_shared<DRB::Dialog>();
            if (Type == "GouraudFrame") return std::make_shared<DRB::GouraudFrame>();
            if (Type == "GouraudRect") return std::make_shared<DRB::GouraudRect>();
            if (Type == "GouraudSprite") return std::make_shared<DRB::GouraudSprite>();
            if (Type == "Mask") return std::make_shared<DRB::Mask>();
            if (Type == "MonoFrame") return std::make_shared<DRB::MonoFrame>();
            if (Type == "MonoRect") return std::make_shared<DRB::MonoRect>();
            if (Type == "Null") return std::make_shared<DRB::Null>();
            if (Type == "ScrollText") return std::make_shared<DRB::ScrollText>();
            if (Type == "Sprite") return std::make_shared<DRB::Sprite>();
            if (Type == "Text") return std::make_shared<DRB::Text>();
            throw BinaryException("Unknown shape type: " + Type);
        }

        std::shared_ptr<DRB::Control> MakeControl(const std::string& Type) {
            if (Type == "DmeCtrlScrollText") return std::make_shared<DRB::ScrollTextDummy>();
            if (Type == "FrpgMenuDlgObjContentsHelpItem") return std::make_shared<DRB::HelpItem>();
            if (Type == "Static") return std::make_shared<DRB::Static>();
            throw BinaryException("Unknown control type: " + Type);
        }

        template<typename T>
        T Take(std::map<int32_t, T>& Map, int32_t Offset) {
            const auto Found = Map.find(Offset);
            if (Found == Map.end()) {
                throw BinaryException("DRB reference to missing entry at offset " + std::to_string(Offset));
            }
            T Result = std::move(Found->second);
            Map.erase(Found);
            return Result;
        }
    }  // namespace

    bool DRB::IsImpl(BinaryReader& Reader) {
        if (Reader.Length() < 4) {
            return false;
        }
        const std::string Magic = Reader.GetASCII(0, 4);
        return Magic == std::string("DRB\0", 4) || Magic == std::string("\0BRD", 4);
    }

    void DRB::ReadVersion(BinaryReader& Reader, DRBVersion V) {
        BigEndian    = Reader.GetASCII(0, 4) == std::string("\0BRD", 4);
        Reader.Order = BigEndian ? Endian::Big : Endian::Little;
        Version      = V;
        ReadNullBlock(Reader, "DRB");

        // Strings.
        Strings StringTable;
        {
            int32_t Count, Size;
            const int64_t Start = ReadBlockHeader(Reader, "STR", Count, Size);
            for (int32_t I = 0; I < Count; ++I) {
                const int32_t Offset = static_cast<int32_t>(Reader.Position() - Start);
                StringTable[Offset]  = Reader.ReadUTF16Text();
            }
            Reader.Seek(Start + Size);
        }

        Textures.clear();
        {
            int32_t Count, Size;
            const int64_t Start = ReadBlockHeader(Reader, "TEXI", Count, Size);
            for (int32_t I = 0; I < Count; ++I) {
                Texture T;
                const int32_t NameOffset = Reader.ReadInt32();
                const int32_t PathOffset = Reader.ReadInt32();
                Reader.Assert<int32_t>(0);
                Reader.Assert<int32_t>(0);
                T.Name = StringAt(StringTable, NameOffset);
                T.Path = StringAt(StringTable, PathOffset);
                Textures.push_back(std::move(T));
            }
            Reader.Seek(Start + Size);
        }

        const int64_t ShprStart = ReadBlobBlock(Reader, "SHPR");
        const int64_t CtprStart = ReadBlobBlock(Reader, "CTPR");
        AnipBytes               = ReadBlobBytes(Reader, "ANIP");
        IntpBytes               = ReadBlobBytes(Reader, "INTP");
        const int64_t ScdpStart = ReadBlobBlock(Reader, "SCDP");

        std::map<int32_t, std::shared_ptr<Shape>> Shapes;
        {
            int32_t Count, Size;
            const int64_t Start = ReadBlockHeader(Reader, "SHAP", Count, Size);
            for (int32_t I = 0; I < Count; ++I) {
                const int32_t Offset     = static_cast<int32_t>(Reader.Position() - Start);
                const int32_t TypeOffset = Reader.ReadInt32();
                const int32_t ShprOffset = Reader.ReadInt32();
                auto Result              = MakeShape(StringAt(StringTable, TypeOffset));
                Reader.StepIn(ShprStart + ShprOffset);
                Result->ReadData(Reader, V, StringTable);
                Reader.StepOut();
                Shapes[Offset] = std::move(Result);
            }
            Reader.Seek(Start + Size);
        }

        std::map<int32_t, std::shared_ptr<Control>> Controls;
        {
            int32_t Count, Size;
            const int64_t Start = ReadBlockHeader(Reader, "CTRL", Count, Size);
            for (int32_t I = 0; I < Count; ++I) {
                const int32_t Offset     = static_cast<int32_t>(Reader.Position() - Start);
                const int32_t TypeOffset = Reader.ReadInt32();
                const int32_t CtprOffset = Reader.ReadInt32();
                auto Result              = MakeControl(StringAt(StringTable, TypeOffset));
                Reader.StepIn(CtprStart + CtprOffset);
                Result->ReadData(Reader);
                Reader.StepOut();
                Controls[Offset] = std::move(Result);
            }
            Reader.Seek(Start + Size);
        }

        std::map<int32_t, Anik> Aniks;
        {
            int32_t Count, Size;
            const int64_t Start = ReadBlockHeader(Reader, "ANIK", Count, Size);
            for (int32_t I = 0; I < Count; ++I) {
                const int32_t Offset = static_cast<int32_t>(Reader.Position() - Start);
                Anik A;
                const int32_t NameOffset = Reader.ReadInt32();
                A.Unk04                  = Reader.ReadInt32();
                A.Unk08                  = Reader.ReadByte();
                A.Unk09                  = Reader.ReadByte();
                A.Unk0A                  = Reader.ReadInt16();
                A.IntpOffset             = Reader.ReadInt32();
                A.AnipOffset             = Reader.ReadInt32();
                A.Unk14                  = Reader.ReadInt32();
                A.Unk18                  = Reader.ReadInt32();
                A.Unk1C                  = Reader.ReadInt32();
                A.Name                   = StringAt(StringTable, NameOffset);
                Aniks[Offset]            = std::move(A);
            }
            Reader.Seek(Start + Size);
        }

        std::map<int32_t, Anio> Anios;
        {
            int32_t Count, Size;
            const int64_t Start = ReadBlockHeader(Reader, "ANIO", Count, Size);
            for (int32_t I = 0; I < Count; ++I) {
                const int32_t Offset = static_cast<int32_t>(Reader.Position() - Start);
                Anio A;
                A.Unk00                  = Reader.ReadInt32();
                const int32_t AnikCount  = Reader.ReadInt32();
                const int32_t AnikOffset = Reader.ReadInt32();
                A.Unk0C                  = Reader.ReadInt32();
                for (int32_t J = 0; J < AnikCount; ++J) {
                    A.Aniks.push_back(Take(Aniks, AnikOffset + AnikSize * J));
                }
                Anios[Offset] = std::move(A);
            }
            Reader.Seek(Start + Size);
        }

        Anims.clear();
        {
            int32_t Count, Size;
            const int64_t Start = ReadBlockHeader(Reader, "ANIM", Count, Size);
            for (int32_t I = 0; I < Count; ++I) {
                Anim A;
                const int32_t NameOffset = Reader.ReadInt32();
                const int32_t AnioCount  = Reader.ReadInt32();
                const int32_t AnioOffset = Reader.ReadInt32();
                A.Unk0C                  = Reader.ReadInt32();
                A.Unk10                  = Reader.ReadInt32();
                A.Unk14                  = Reader.ReadInt32();
                A.Unk18                  = Reader.ReadInt32();
                A.Unk1C                  = Reader.ReadInt32();
                A.Unk20                  = Reader.ReadInt32();
                A.Unk24                  = Reader.ReadInt32();
                A.Unk28                  = Reader.ReadInt32();
                A.Unk2C                  = Reader.ReadInt32();
                A.Name                   = StringAt(StringTable, NameOffset);
                for (int32_t J = 0; J < AnioCount; ++J) {
                    A.Anios.push_back(Take(Anios, AnioOffset + AnioSize * J));
                }
                Anims.push_back(std::move(A));
            }
            Reader.Seek(Start + Size);
        }

        std::map<int32_t, Scdk> Scdks;
        {
            int32_t Count, Size;
            const int64_t Start = ReadBlockHeader(Reader, "SCDK", Count, Size);
            for (int32_t I = 0; I < Count; ++I) {
                const int32_t Offset = static_cast<int32_t>(Reader.Position() - Start);
                Scdk K;
                const int32_t NameOffset = Reader.ReadInt32();
                K.Unk04                  = Reader.ReadInt32();
                K.Unk08                  = Reader.ReadInt32();
                K.Unk0C                  = Reader.ReadInt32();
                const int32_t ScdpOffset = Reader.ReadInt32();
                K.Unk14                  = Reader.ReadInt32();
                K.Unk18                  = Reader.ReadInt32();
                K.Unk1C                  = Reader.ReadInt32();
                K.Name                   = StringAt(StringTable, NameOffset);
                Reader.StepIn(ScdpStart + ScdpOffset);
                K.AnimIndex = Reader.ReadInt32();
                K.Scdp04    = Reader.ReadInt32();
                Reader.StepOut();
                Scdks[Offset] = std::move(K);
            }
            Reader.Seek(Start + Size);
        }

        std::map<int32_t, Scdo> Scdos;
        {
            int32_t Count, Size;
            const int64_t Start = ReadBlockHeader(Reader, "SCDO", Count, Size);
            for (int32_t I = 0; I < Count; ++I) {
                const int32_t Offset = static_cast<int32_t>(Reader.Position() - Start);
                Scdo O;
                const int32_t NameOffset = Reader.ReadInt32();
                const int32_t ScdkCount  = Reader.ReadInt32();
                const int32_t ScdkOffset = Reader.ReadInt32();
                O.Unk0C                  = Reader.ReadInt32();
                O.Name                   = StringAt(StringTable, NameOffset);
                for (int32_t J = 0; J < ScdkCount; ++J) {
                    O.Scdks.push_back(Take(Scdks, ScdkOffset + ScdkSize * J));
                }
                Scdos[Offset] = std::move(O);
            }
            Reader.Seek(Start + Size);
        }

        Scdls.clear();
        {
            int32_t Count, Size;
            const int64_t Start = ReadBlockHeader(Reader, "SCDL", Count, Size);
            for (int32_t I = 0; I < Count; ++I) {
                Scdl L;
                const int32_t NameOffset = Reader.ReadInt32();
                const int32_t ScdoCount  = Reader.ReadInt32();
                const int32_t ScdoOffset = Reader.ReadInt32();
                L.Unk0C                  = Reader.ReadInt32();
                L.Name                   = StringAt(StringTable, NameOffset);
                for (int32_t J = 0; J < ScdoCount; ++J) {
                    L.Scdos.push_back(Take(Scdos, ScdoOffset + ScdoSize * J));
                }
                Scdls.push_back(std::move(L));
            }
            Reader.Seek(Start + Size);
        }

        // Reads the part of a Dlgo shared with Dlg.
        const auto ReadDlgo = [&](Dlgo& D) {
            const int32_t NameOffset = Reader.ReadInt32();
            const int32_t ShapOffset = Reader.ReadInt32();
            const int32_t CtrlOffset = Reader.ReadInt32();
            D.Unk0C                  = Reader.ReadInt32();
            D.Unk10                  = Reader.ReadInt32();
            D.Unk14                  = Reader.ReadInt32();
            D.Unk18                  = Reader.ReadInt32();
            D.Unk1C                  = Reader.ReadInt32();
            D.Name                   = StringAt(StringTable, NameOffset);
            D.Shape_                 = Take(Shapes, ShapOffset);
            D.Control_               = Take(Controls, CtrlOffset);
        };

        std::map<int32_t, Dlgo> Dlgos;
        {
            int32_t Count, Size;
            const int64_t Start = ReadBlockHeader(Reader, "DLGO", Count, Size);
            for (int32_t I = 0; I < Count; ++I) {
                const int32_t Offset = static_cast<int32_t>(Reader.Position() - Start);
                Dlgo D;
                ReadDlgo(D);
                Dlgos[Offset] = std::move(D);
            }
            Reader.Seek(Start + Size);
        }

        Dlgs.clear();
        {
            int32_t Count, Size;
            const int64_t Start = ReadBlockHeader(Reader, "DLG", Count, Size);
            for (int32_t I = 0; I < Count; ++I) {
                Dlg D;
                ReadDlgo(D);
                const int32_t DlgoCount  = Reader.ReadInt32();
                const int32_t DlgoOffset = Reader.ReadInt32();
                D.LeftEdge               = Reader.ReadInt16();
                D.TopEdge                = Reader.ReadInt16();
                D.RightEdge              = Reader.ReadInt16();
                D.BottomEdge             = Reader.ReadInt16();
                Reader.ReadInto(std::span<int16_t>(D.Unk30));
                D.Unk3A = Reader.ReadInt16();
                D.Unk3C = Reader.ReadInt32();
                for (int32_t J = 0; J < DlgoCount; ++J) {
                    D.Dlgos.push_back(Take(Dlgos, DlgoOffset + DlgoSize * J));
                }
                Dlgs.push_back(std::move(D));
            }
            Reader.Seek(Start + Size);
        }
        ReadNullBlock(Reader, "END");
    }

    void DRB::ReadImpl(BinaryReader& Reader) {
        Reader.Seek(0);
        const std::vector<uint8_t> Bytes = Reader.ReadBytes(static_cast<size_t>(Reader.Length()));
        const DCX::Type Kept             = Compression;

        std::optional<DRB> Fallback;
        std::string FirstError;
        for (const DRBVersion V : {DRBVersion::DarkSoulsRemastered, DRBVersion::DarkSoulsRemasteredOriginOnly, DRBVersion::DarkSouls}) {
            try {
                DRB Candidate;
                BinaryReader Source{std::span<const uint8_t>(Bytes)};
                Candidate.ReadVersion(Source, V);
                if (Candidate.Write() == Bytes) {
                    *this             = std::move(Candidate);
                    this->Compression = Kept;
                    return;
                }
                if (!Fallback) {
                    Fallback = std::move(Candidate);
                }
            } catch (const std::exception& E) {
                if (FirstError.empty()) {
                    FirstError = E.what();
                }
            }
        }
        if (!Fallback) {
            throw BinaryException("Failed to read DRB: " + FirstError);
        }
        *this             = std::move(*Fallback);
        this->Compression = Kept;
    }

    DRB DRB::Read(std::span<const uint8_t> Data, DRBVersion V) {
        BinaryReader Source(Data);
        DCX::DecompressedReader Decompressed(Source);
        DRB File;
        File.Compression = Decompressed.Compression();
        File.ReadVersion(Decompressed.Reader(), V);
        return File;
    }

    DRB DRB::Read(const std::filesystem::path& Path, DRBVersion V) {
        BinaryReader Source(Path);
        DCX::DecompressedReader Decompressed(Source);
        DRB File;
        File.Compression = Decompressed.Compression();
        File.ReadVersion(Decompressed.Reader(), V);
        return File;
    }

    void DRB::WriteImpl(BinaryWriter& Writer) {
        Writer.Order = BigEndian ? Endian::Big : Endian::Little;
        WriteNullBlock(Writer, "DRB");

        // Gather the strings in the order they're first needed.
        Offsets StringOffsets;
        {
            const int64_t Start = WriteBlockHeader(Writer, "STR");
            const auto Add      = [&](const std::string& Text) {
                if (!StringOffsets.contains(Text)) {
                    StringOffsets[Text] = static_cast<int32_t>(Writer.Position() - Start);
                    Writer.WriteUTF16Text(Text, true);
                }
            };
            const auto AddDlgo = [&](const Dlgo& D) {
                Add(D.Name);
                Add(D.Shape_->TypeName());
                std::vector<std::string> Extra;
                D.Shape_->CollectStrings(Extra);
                for (const std::string& S : Extra) {
                    Add(S);
                }
                Add(D.Control_->TypeName());
            };
            for (const Dlg& D : Dlgs) {
                AddDlgo(D);
                for (const Dlgo& Child : D.Dlgos) {
                    AddDlgo(Child);
                }
            }
            for (const Texture& T : Textures) {
                Add(T.Name);
                Add(T.Path);
            }
            for (const Anim& A : Anims) {
                Add(A.Name);
                for (const Anio& O : A.Anios) {
                    for (const Anik& K : O.Aniks) {
                        Add(K.Name);
                    }
                }
            }
            for (const Scdl& L : Scdls) {
                Add(L.Name);
                for (const Scdo& O : L.Scdos) {
                    Add(O.Name);
                    for (const Scdk& K : O.Scdks) {
                        Add(K.Name);
                    }
                }
            }
            FinishBlockHeader(Writer, "STR", Start, static_cast<int32_t>(StringOffsets.size()));
        }

        {
            const int64_t Start = WriteBlockHeader(Writer, "TEXI");
            for (const Texture& T : Textures) {
                Writer.WriteInt32(OffsetOf(StringOffsets, T.Name));
                Writer.WriteInt32(OffsetOf(StringOffsets, T.Path));
                Writer.WriteInt32(0);
                Writer.WriteInt32(0);
            }
            FinishBlockHeader(Writer, "TEXI", Start, static_cast<int32_t>(Textures.size()));
        }

        OffsetQueue ShprOffsets;
        {
            const int64_t Start = WriteBlobBlock(Writer, "SHPR");
            const auto WriteShape = [&](const Shape& S) {
                ShprOffsets.push(static_cast<int32_t>(Writer.Position() - Start));
                S.WriteData(Writer, Version, StringOffsets);
            };
            for (const Dlg& D : Dlgs) {
                WriteShape(*D.Shape_);
                for (const Dlgo& Child : D.Dlgos) {
                    WriteShape(*Child.Shape_);
                }
            }
            FinishBlobBlock(Writer, "SHPR", Start);
        }

        OffsetQueue CtprOffsets;
        {
            const int64_t Start = WriteBlobBlock(Writer, "CTPR");
            const auto WriteControl = [&](const Control& C) {
                CtprOffsets.push(static_cast<int32_t>(Writer.Position() - Start));
                C.WriteData(Writer);
            };
            for (const Dlg& D : Dlgs) {
                WriteControl(*D.Control_);
                for (const Dlgo& Child : D.Dlgos) {
                    WriteControl(*Child.Control_);
                }
            }
            FinishBlobBlock(Writer, "CTPR", Start);
        }

        WriteBlobBytes(Writer, "ANIP", AnipBytes);
        WriteBlobBytes(Writer, "INTP", IntpBytes);

        OffsetQueue ScdpOffsets;
        {
            const int64_t Start = WriteBlobBlock(Writer, "SCDP");
            for (const Scdl& L : Scdls) {
                for (const Scdo& O : L.Scdos) {
                    for (const Scdk& K : O.Scdks) {
                        ScdpOffsets.push(static_cast<int32_t>(Writer.Position() - Start));
                        Writer.WriteInt32(K.AnimIndex);
                        Writer.WriteInt32(K.Scdp04);
                    }
                }
            }
            FinishBlobBlock(Writer, "SCDP", Start);
        }

        OffsetQueue DlgShapOffsets, DlgoShapOffsets;
        {
            const int64_t Start = WriteBlockHeader(Writer, "SHAP");
            const auto WriteShape = [&](const Shape& S) {
                const auto Offset = static_cast<int32_t>(Writer.Position() - Start);
                Writer.WriteInt32(OffsetOf(StringOffsets, S.TypeName()));
                Writer.WriteInt32(Pop(ShprOffsets));
                return Offset;
            };
            int32_t Count = 0;
            for (const Dlg& D : Dlgs) {
                DlgShapOffsets.push(WriteShape(*D.Shape_));
                ++Count;
                for (const Dlgo& Child : D.Dlgos) {
                    DlgoShapOffsets.push(WriteShape(*Child.Shape_));
                    ++Count;
                }
            }
            FinishBlockHeader(Writer, "SHAP", Start, Count);
        }

        OffsetQueue DlgCtrlOffsets, DlgoCtrlOffsets;
        {
            const int64_t Start = WriteBlockHeader(Writer, "CTRL");
            const auto WriteControl = [&](const Control& C) {
                const auto Offset = static_cast<int32_t>(Writer.Position() - Start);
                Writer.WriteInt32(OffsetOf(StringOffsets, C.TypeName()));
                Writer.WriteInt32(Pop(CtprOffsets));
                return Offset;
            };
            int32_t Count = 0;
            for (const Dlg& D : Dlgs) {
                DlgCtrlOffsets.push(WriteControl(*D.Control_));
                ++Count;
                for (const Dlgo& Child : D.Dlgos) {
                    DlgoCtrlOffsets.push(WriteControl(*Child.Control_));
                    ++Count;
                }
            }
            FinishBlockHeader(Writer, "CTRL", Start, Count);
        }

        OffsetQueue AnikOffsets;
        {
            const int64_t Start = WriteBlockHeader(Writer, "ANIK");
            int32_t Count       = 0;
            for (const Anim& A : Anims) {
                for (const Anio& O : A.Anios) {
                    AnikOffsets.push(static_cast<int32_t>(Writer.Position() - Start));
                    Count += static_cast<int32_t>(O.Aniks.size());
                    for (const Anik& K : O.Aniks) {
                        Writer.WriteInt32(OffsetOf(StringOffsets, K.Name));
                        Writer.WriteInt32(K.Unk04);
                        Writer.WriteByte(K.Unk08);
                        Writer.WriteByte(K.Unk09);
                        Writer.WriteInt16(K.Unk0A);
                        Writer.WriteInt32(K.IntpOffset);
                        Writer.WriteInt32(K.AnipOffset);
                        Writer.WriteInt32(K.Unk14);
                        Writer.WriteInt32(K.Unk18);
                        Writer.WriteInt32(K.Unk1C);
                    }
                }
            }
            FinishBlockHeader(Writer, "ANIK", Start, Count);
        }

        OffsetQueue AnioOffsets;
        {
            const int64_t Start = WriteBlockHeader(Writer, "ANIO");
            int32_t Count       = 0;
            for (const Anim& A : Anims) {
                AnioOffsets.push(static_cast<int32_t>(Writer.Position() - Start));
                Count += static_cast<int32_t>(A.Anios.size());
                for (const Anio& O : A.Anios) {
                    Writer.WriteInt32(O.Unk00);
                    Writer.WriteInt32(static_cast<int32_t>(O.Aniks.size()));
                    Writer.WriteInt32(Pop(AnikOffsets));
                    Writer.WriteInt32(O.Unk0C);
                }
            }
            FinishBlockHeader(Writer, "ANIO", Start, Count);
        }

        {
            const int64_t Start = WriteBlockHeader(Writer, "ANIM");
            for (const Anim& A : Anims) {
                Writer.WriteInt32(OffsetOf(StringOffsets, A.Name));
                Writer.WriteInt32(static_cast<int32_t>(A.Anios.size()));
                Writer.WriteInt32(Pop(AnioOffsets));
                Writer.WriteInt32(A.Unk0C);
                Writer.WriteInt32(A.Unk10);
                Writer.WriteInt32(A.Unk14);
                Writer.WriteInt32(A.Unk18);
                Writer.WriteInt32(A.Unk1C);
                Writer.WriteInt32(A.Unk20);
                Writer.WriteInt32(A.Unk24);
                Writer.WriteInt32(A.Unk28);
                Writer.WriteInt32(A.Unk2C);
            }
            FinishBlockHeader(Writer, "ANIM", Start, static_cast<int32_t>(Anims.size()));
        }

        OffsetQueue ScdkOffsets;
        {
            const int64_t Start = WriteBlockHeader(Writer, "SCDK");
            int32_t Count       = 0;
            for (const Scdl& L : Scdls) {
                for (const Scdo& O : L.Scdos) {
                    ScdkOffsets.push(static_cast<int32_t>(Writer.Position() - Start));
                    Count += static_cast<int32_t>(O.Scdks.size());
                    for (const Scdk& K : O.Scdks) {
                        Writer.WriteInt32(OffsetOf(StringOffsets, K.Name));
                        Writer.WriteInt32(K.Unk04);
                        Writer.WriteInt32(K.Unk08);
                        Writer.WriteInt32(K.Unk0C);
                        Writer.WriteInt32(Pop(ScdpOffsets));
                        Writer.WriteInt32(K.Unk14);
                        Writer.WriteInt32(K.Unk18);
                        Writer.WriteInt32(K.Unk1C);
                    }
                }
            }
            FinishBlockHeader(Writer, "SCDK", Start, Count);
        }

        OffsetQueue ScdoOffsets;
        {
            const int64_t Start = WriteBlockHeader(Writer, "SCDO");
            int32_t Count       = 0;
            for (const Scdl& L : Scdls) {
                ScdoOffsets.push(static_cast<int32_t>(Writer.Position() - Start));
                Count += static_cast<int32_t>(L.Scdos.size());
                for (const Scdo& O : L.Scdos) {
                    Writer.WriteInt32(OffsetOf(StringOffsets, O.Name));
                    Writer.WriteInt32(static_cast<int32_t>(O.Scdks.size()));
                    Writer.WriteInt32(Pop(ScdkOffsets));
                    Writer.WriteInt32(O.Unk0C);
                }
            }
            FinishBlockHeader(Writer, "SCDO", Start, Count);
        }

        {
            const int64_t Start = WriteBlockHeader(Writer, "SCDL");
            for (const Scdl& L : Scdls) {
                Writer.WriteInt32(OffsetOf(StringOffsets, L.Name));
                Writer.WriteInt32(static_cast<int32_t>(L.Scdos.size()));
                Writer.WriteInt32(Pop(ScdoOffsets));
                Writer.WriteInt32(L.Unk0C);
            }
            FinishBlockHeader(Writer, "SCDL", Start, static_cast<int32_t>(Scdls.size()));
        }

        const auto WriteDlgo = [&](const Dlgo& D, OffsetQueue& Shaps, OffsetQueue& Ctrls) {
            Writer.WriteInt32(OffsetOf(StringOffsets, D.Name));
            Writer.WriteInt32(Pop(Shaps));
            Writer.WriteInt32(Pop(Ctrls));
            Writer.WriteInt32(D.Unk0C);
            Writer.WriteInt32(D.Unk10);
            Writer.WriteInt32(D.Unk14);
            Writer.WriteInt32(D.Unk18);
            Writer.WriteInt32(D.Unk1C);
        };

        OffsetQueue DlgoOffsets;
        {
            const int64_t Start = WriteBlockHeader(Writer, "DLGO");
            int32_t Count       = 0;
            for (const Dlg& D : Dlgs) {
                DlgoOffsets.push(static_cast<int32_t>(Writer.Position() - Start));
                Count += static_cast<int32_t>(D.Dlgos.size());
                for (const Dlgo& Child : D.Dlgos) {
                    WriteDlgo(Child, DlgoShapOffsets, DlgoCtrlOffsets);
                }
            }
            FinishBlockHeader(Writer, "DLGO", Start, Count);
        }

        {
            const int64_t Start = WriteBlockHeader(Writer, "DLG");
            for (const Dlg& D : Dlgs) {
                WriteDlgo(D, DlgShapOffsets, DlgCtrlOffsets);
                Writer.WriteInt32(static_cast<int32_t>(D.Dlgos.size()));
                Writer.WriteInt32(Pop(DlgoOffsets));
                Writer.WriteInt16(D.LeftEdge);
                Writer.WriteInt16(D.TopEdge);
                Writer.WriteInt16(D.RightEdge);
                Writer.WriteInt16(D.BottomEdge);
                Writer.WriteArray(std::span<const int16_t>(D.Unk30));
                Writer.WriteInt16(D.Unk3A);
                Writer.WriteInt32(D.Unk3C);
            }
            FinishBlockHeader(Writer, "DLG", Start, static_cast<int32_t>(Dlgs.size()));
        }
        WriteNullBlock(Writer, "END");
    }
}  // namespace Souls
