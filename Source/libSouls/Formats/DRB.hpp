//
// Created by Jake Rieger on 10/8/2026.
//

#pragma once

#include <libSouls/Color.hpp>
#include <libSouls/SoulsFile.hpp>

#include <array>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace Souls {

#pragma warning(push)
#pragma warning(disable : 4251)  // STL members of exported classes; only touched by this DLL's own code

    // DRB: a layout format for the user interface of Dark Souls and Dark Souls Remastered. Extension: .drb
    //
    // The file doesn't say which game it is from, and the shape layouts differ between the two, so the version is
    // detected by trying each and keeping the one that writes back to the same bytes. Pass the version explicitly to
    // skip the guessing.
    class SOULS_API DRB : public SoulsFile<DRB> {
    public:
        enum class DRBVersion {
            DarkSouls,
            DarkSoulsRemastered,
            // Some Remastered files (nowloading.drb) store only the scaling origin, without the mode field.
            DarkSoulsRemasteredOriginOnly,
        };

        using StringTable   = std::map<int32_t, std::string>;
        using StringOffsets = std::map<std::string, int32_t>;

#pragma region Textures
        struct Texture {
            std::string Name;
            std::string Path;
        };
#pragma endregion

#pragma region Controls
        enum class ControlType {
            DmeCtrlScrollText,
            FrpgMenuDlgObjContentsHelpItem,
            Static,
        };

        // Behavior attached to an element.
        struct SOULS_API Control {
            virtual ~Control()                = default;
            virtual ControlType Type() const  = 0;
            virtual void ReadData(BinaryReader& Reader)        = 0;
            virtual void WriteData(BinaryWriter& Writer) const = 0;
            const char* TypeName() const;
        };

        struct SOULS_API ScrollTextDummy : Control {
            int32_t Unk00 = 0;
            ControlType Type() const override { return ControlType::DmeCtrlScrollText; }
            void ReadData(BinaryReader& Reader) override;
            void WriteData(BinaryWriter& Writer) const override;
        };

        struct SOULS_API HelpItem : Control {
            int32_t Unk00 = 0, Unk04 = 0, Unk08 = 0, Unk0C = 0, Unk10 = 0, Unk14 = 0;
            int32_t TextID = -1;
            ControlType Type() const override { return ControlType::FrpgMenuDlgObjContentsHelpItem; }
            void ReadData(BinaryReader& Reader) override;
            void WriteData(BinaryWriter& Writer) const override;
        };

        struct SOULS_API Static : Control {
            int32_t Unk00 = 0;
            ControlType Type() const override { return ControlType::Static; }
            void ReadData(BinaryReader& Reader) override;
            void WriteData(BinaryWriter& Writer) const override;
        };
#pragma endregion

#pragma region Shapes
        enum class ShapeType {
            Dialog,
            GouraudFrame,
            GouraudRect,
            GouraudSprite,
            Mask,
            MonoFrame,
            MonoRect,
            Null,
            ScrollText,
            Sprite,
            Text,
        };

        enum class BlendingMode : uint8_t {
            Opaque   = 0,
            Alpha    = 1,
            Add      = 2,
            Subtract = 3,
        };

        // Flags; combine with |.
        enum class SpriteOrientation : uint8_t {
            None          = 0,
            RotateCW      = 0x10,
            Rotate180     = 0x20,
            FlipVertical  = 0x40,
            FlipHorizontal = 0x80,
        };

        // Flags; combine with |.
        enum class AlignFlags : uint8_t {
            TopLeft          = 0,
            Right            = 1,
            CenterHorizontal = 2,
            Bottom           = 4,
            CenterVertical   = 8,
        };

        enum class TxtType : uint8_t {
            Literal = 0,
            FMG     = 1,
            Dynamic = 2,
        };

        // The visual part of an element.
        struct SOULS_API Shape {
            int16_t LeftEdge = 0, TopEdge = 0, RightEdge = 0, BottomEdge = 0;
            // Only stored by Dark Souls Remastered. The mode is missing from some files.
            int16_t ScalingOriginX = -1, ScalingOriginY = -1, ScalingMode = 0;

            virtual ~Shape()                = default;
            virtual ShapeType Type() const  = 0;
            const char* TypeName() const;
            void ReadData(BinaryReader& Reader, DRBVersion Version, const StringTable& Strings);
            void WriteData(BinaryWriter& Writer, DRBVersion Version, const StringOffsets& Strings) const;
            // Strings the shape needs in the string table.
            virtual void CollectStrings(std::vector<std::string>&) const {}

        protected:
            virtual void ReadSpecific(BinaryReader& Reader, const StringTable& Strings)           = 0;
            virtual void WriteSpecific(BinaryWriter& Writer, const StringOffsets& Strings) const = 0;
        };

        struct SOULS_API SpriteBase : Shape {
            int16_t TexLeftEdge = 0, TexTopEdge = 0, TexRightEdge = 0, TexBottomEdge = 0;
            int16_t TextureIndex = 0;
            SpriteOrientation Orientation = SpriteOrientation::None;
            BlendingMode BlendMode        = BlendingMode::Alpha;

        protected:
            void ReadSpecific(BinaryReader& Reader, const StringTable& Strings) override;
            void WriteSpecific(BinaryWriter& Writer, const StringOffsets& Strings) const override;
        };

        struct SOULS_API TextBase : Shape {
            struct UnknownB {
                int32_t Unk00 = 0;
                int16_t Unk04 = 0xFF;
                int16_t Unk06 = 0;
                int16_t Unk08 = 0;
            };
            struct UnknownA {
                int32_t Unk00 = 0;
                std::optional<UnknownB> SubUnknown;
            };

            BlendingMode BlendMode = BlendingMode::Alpha;
            int16_t LineSpacing    = 0;
            int32_t PaletteColor   = 0;
            Color CustomColor;
            int16_t FontSize        = 0;
            AlignFlags Alignment    = AlignFlags::TopLeft;
            TxtType TextType        = TxtType::FMG;
            int32_t CharLength      = 0;
            std::string TextLiteral;  // only for TxtType::Literal
            int32_t TextID = -1;
            std::optional<UnknownA> Unknown = UnknownA{};

            void CollectStrings(std::vector<std::string>& Out) const override;

        protected:
            void ReadSpecific(BinaryReader& Reader, const StringTable& Strings) override;
            void WriteSpecific(BinaryWriter& Writer, const StringOffsets& Strings) const override;
            virtual void ReadSubtype(BinaryReader&) {}
            virtual void WriteSubtype(BinaryWriter&) const {}
        };

        // A reference to another dialog (by its position in DRB::Dlgs, -1 for none).
        struct SOULS_API Dialog : Shape {
            struct UnknownA {
                int16_t Unk00 = 0;
                int16_t Unk02 = 0;
                int32_t Unk04 = 0;
            };

            int16_t DlgIndex = -1;
            uint8_t Unk02    = 1;
            uint8_t Unk03    = 1;
            int32_t PaletteColor = 0;
            Color CustomColor    = Color::White();
            std::optional<UnknownA> Unknown;
            ShapeType Type() const override { return ShapeType::Dialog; }

        protected:
            void ReadSpecific(BinaryReader& Reader, const StringTable& Strings) override;
            void WriteSpecific(BinaryWriter& Writer, const StringOffsets& Strings) const override;
        };

        struct SOULS_API GouraudFrame : Shape {
            BlendingMode BlendMode = BlendingMode::Alpha;
            uint8_t Thickness      = 1;
            Color TopLeftColor = Color::Black(), TopRightColor = Color::Black(), BottomRightColor = Color::Black(),
                  BottomLeftColor = Color::Black();
            ShapeType Type() const override { return ShapeType::GouraudFrame; }

        protected:
            void ReadSpecific(BinaryReader& Reader, const StringTable& Strings) override;
            void WriteSpecific(BinaryWriter& Writer, const StringOffsets& Strings) const override;
        };

        struct SOULS_API GouraudRect : Shape {
            BlendingMode BlendMode = BlendingMode::Alpha;
            Color TopLeftColor = Color::Black(), TopRightColor = Color::Black(), BottomRightColor = Color::Black(),
                  BottomLeftColor = Color::Black();
            ShapeType Type() const override { return ShapeType::GouraudRect; }

        protected:
            void ReadSpecific(BinaryReader& Reader, const StringTable& Strings) override;
            void WriteSpecific(BinaryWriter& Writer, const StringOffsets& Strings) const override;
        };

        struct SOULS_API GouraudSprite : SpriteBase {
            Color TopLeftColor = Color::White(), TopRightColor = Color::White(), BottomRightColor = Color::White(),
                  BottomLeftColor = Color::White();
            ShapeType Type() const override { return ShapeType::GouraudSprite; }

        protected:
            void ReadSpecific(BinaryReader& Reader, const StringTable& Strings) override;
            void WriteSpecific(BinaryWriter& Writer, const StringOffsets& Strings) const override;
        };

        struct SOULS_API Mask : Shape {
            // The name of the element this mask applies to.
            std::string DlgoName;
            uint8_t Unk04  = 1;
            int32_t Unk05  = 0;
            int32_t Unk09  = 0;
            int16_t Unk0D  = 0;
            ShapeType Type() const override { return ShapeType::Mask; }
            void CollectStrings(std::vector<std::string>& Out) const override { Out.push_back(DlgoName); }

        protected:
            void ReadSpecific(BinaryReader& Reader, const StringTable& Strings) override;
            void WriteSpecific(BinaryWriter& Writer, const StringOffsets& Strings) const override;
        };

        struct SOULS_API MonoFrame : Shape {
            BlendingMode BlendMode = BlendingMode::Alpha;
            uint8_t Thickness      = 1;
            int32_t PaletteColor   = 0;
            Color CustomColor      = Color::Black();
            ShapeType Type() const override { return ShapeType::MonoFrame; }

        protected:
            void ReadSpecific(BinaryReader& Reader, const StringTable& Strings) override;
            void WriteSpecific(BinaryWriter& Writer, const StringOffsets& Strings) const override;
        };

        struct SOULS_API MonoRect : Shape {
            BlendingMode BlendMode = BlendingMode::Alpha;
            int32_t PaletteColor   = 0;
            Color CustomColor      = Color::Black();
            ShapeType Type() const override { return ShapeType::MonoRect; }

        protected:
            void ReadSpecific(BinaryReader& Reader, const StringTable& Strings) override;
            void WriteSpecific(BinaryWriter& Writer, const StringOffsets& Strings) const override;
        };

        struct SOULS_API Null : Shape {
            ShapeType Type() const override { return ShapeType::Null; }

        protected:
            void ReadSpecific(BinaryReader&, const StringTable&) override {}
            void WriteSpecific(BinaryWriter&, const StringOffsets&) const override {}
        };

        struct SOULS_API ScrollText : TextBase {
            int16_t UnkX00 = 0, UnkX02 = 0, UnkX04 = 0, UnkX06 = 0;
            int16_t ScrollSpeed = 15;
            int16_t UnkX0A      = 0;
            ShapeType Type() const override { return ShapeType::ScrollText; }

        protected:
            void ReadSubtype(BinaryReader& Reader) override;
            void WriteSubtype(BinaryWriter& Writer) const override;
        };

        struct SOULS_API Sprite : SpriteBase {
            int32_t PaletteColor = 0;
            Color CustomColor    = Color::White();
            ShapeType Type() const override { return ShapeType::Sprite; }

        protected:
            void ReadSpecific(BinaryReader& Reader, const StringTable& Strings) override;
            void WriteSpecific(BinaryWriter& Writer, const StringOffsets& Strings) const override;
        };

        struct SOULS_API Text : TextBase {
            ShapeType Type() const override { return ShapeType::Text; }
        };
#pragma endregion

#pragma region Animations
        struct Anik {
            std::string Name;
            int32_t Unk04 = 0;
            uint8_t Unk08 = 0;
            uint8_t Unk09 = 0;
            int16_t Unk0A = 0;
            int32_t IntpOffset = 0;
            int32_t AnipOffset = 0;
            int32_t Unk14 = 0, Unk18 = 0, Unk1C = 0;
        };

        struct Anio {
            int32_t Unk00 = 0;
            std::vector<Anik> Aniks;
            int32_t Unk0C = 0;
        };

        struct Anim {
            std::string Name;
            std::vector<Anio> Anios;
            int32_t Unk0C = 0;
            int32_t Unk10 = 4, Unk14 = 4, Unk18 = 4, Unk1C = 1;
            int32_t Unk20 = 0, Unk24 = 0, Unk28 = 0, Unk2C = 0;
        };

        struct Scdk {
            std::string Name;
            int32_t Unk04 = 0;
            int32_t Unk08 = 1;
            int32_t Unk0C = 0;
            int32_t Unk14 = 0, Unk18 = 0, Unk1C = 0;
            int32_t AnimIndex = 0;
            int32_t Scdp04    = 0;
        };

        struct Scdo {
            std::string Name;
            std::vector<Scdk> Scdks;
            int32_t Unk0C = 0;
        };

        struct Scdl {
            std::string Name;
            std::vector<Scdo> Scdos;
            int32_t Unk0C = 0;
        };
#pragma endregion

#pragma region Dialogs
        // An element of a dialog.
        struct Dlgo {
            std::string Name;
            std::shared_ptr<Shape> Shape_ = std::make_shared<Null>();
            std::shared_ptr<Control> Control_ = std::make_shared<Static>();
            int32_t Unk0C = 0, Unk10 = 0, Unk14 = 0, Unk18 = 0, Unk1C = 0;
        };

        // A dialog: an element that contains other elements.
        struct Dlg : Dlgo {
            std::vector<Dlgo> Dlgos;
            int16_t LeftEdge = 0, TopEdge = 0, RightEdge = 0, BottomEdge = 0;
            std::array<int16_t, 5> Unk30{-1, -1, -1, -1, -1};
            int16_t Unk3A = 0;
            int32_t Unk3C = 0;

            Dlgo* Find(const std::string& Name) {
                for (Dlgo& D : Dlgos) {
                    if (D.Name == Name) {
                        return &D;
                    }
                }
                return nullptr;
            }
        };
#pragma endregion

        DRBVersion Version = DRBVersion::DarkSoulsRemastered;
        bool BigEndian     = false;
        std::vector<Texture> Textures;
        std::vector<uint8_t> AnipBytes;
        std::vector<uint8_t> IntpBytes;
        std::vector<Anim> Anims;
        std::vector<Scdl> Scdls;
        std::vector<Dlg> Dlgs;

        Dlg* Find(const std::string& Name) {
            for (Dlg& D : Dlgs) {
                if (D.Name == Name) {
                    return &D;
                }
            }
            return nullptr;
        }

        // Reads a file of a known game, skipping the version detection.
        static DRB Read(std::span<const uint8_t> Data, DRBVersion Version);
        static DRB Read(const std::filesystem::path& Path, DRBVersion Version);
        using SoulsFile<DRB>::Read;

    protected:
        bool IsImpl(BinaryReader& Reader) override;
        void ReadImpl(BinaryReader& Reader) override;
        void WriteImpl(BinaryWriter& Writer) override;

    private:
        void ReadVersion(BinaryReader& Reader, DRBVersion Version);
    };

#pragma warning(pop)

}  // namespace Souls
