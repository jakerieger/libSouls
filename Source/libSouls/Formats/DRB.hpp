// Copyright (C) 2026 Jake Rieger
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, version 3. This program is distributed in the hope that it will
// be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU General
// Public License along with this program. If not, see https://www.gnu.org/licenses/.

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

    /// @brief DRB: a layout format for the user interface of Dark Souls and Dark Souls Remastered.
    ///
    /// Extension: .drb
    ///
    /// The file doesn't say which game it is from, and the shape layouts differ between the two, so the version is
    /// detected by trying each and keeping the one that writes back to the same bytes. Pass the version explicitly to
    /// skip the guessing.
    class SOULS_API DRB : public SoulsFile<DRB> {
    public:
        /// @brief Which game's layout the file uses.
        enum class DRBVersion {
            DarkSouls,
            DarkSoulsRemastered,
            /// @brief Some Remastered files (nowloading.drb) store only the scaling origin, without the mode field.
            DarkSoulsRemasteredOriginOnly,
        };

        /// @brief The file's strings by offset, as read.
        using StringTable = std::map<int32_t, std::string>;
        /// @brief The offsets strings are written at.
        using StringOffsets = std::map<std::string, int32_t>;

#pragma region Textures
        /// @brief A texture available to be referenced by UI elements.
        struct Texture {
            /// @brief A friendly name for the texture.
            std::string Name;
            /// @brief The network path to the texture.
            std::string Path;
        };
#pragma endregion

#pragma region Controls
        /// @brief The kinds of control.
        enum class ControlType {
            DmeCtrlScrollText,
            FrpgMenuDlgObjContentsHelpItem,
            Static,
        };

        /// @brief Behavior attached to an element.
        struct SOULS_API Control {
            virtual ~Control() = default;
            /// @brief The type number of this entry as stored in the file.
            virtual ControlType Type() const = 0;
            /// @brief Reads the type-specific data.
            virtual void ReadData(BinaryReader& Reader) = 0;
            /// @brief Writes the type-specific data.
            virtual void WriteData(BinaryWriter& Writer) const = 0;
            /// @brief The type's name, as SoulsFormats calls it.
            const char* TypeName() const;
        };

        /// @brief Unknown.
        struct SOULS_API ScrollTextDummy : Control {
            /// @brief Unknown; always 0.
            int32_t Unk00 = 0;
            /// @brief The type number of this entry as stored in the file.
            ControlType Type() const override { return ControlType::DmeCtrlScrollText; }
            /// @brief Reads the type-specific data.
            void ReadData(BinaryReader& Reader) override;
            /// @brief Writes the type-specific data.
            void WriteData(BinaryWriter& Writer) const override;
        };

        /// @brief Unknown.
        struct SOULS_API HelpItem : Control {
            /// @brief Unknown.
            int32_t Unk00 = 0;
            /// @brief Unknown.
            int32_t Unk04 = 0;
            /// @brief Unknown.
            int32_t Unk08 = 0;
            /// @brief Unknown.
            int32_t Unk0C = 0;
            /// @brief Unknown.
            int32_t Unk10 = 0;
            /// @brief Unknown.
            int32_t Unk14 = 0;
            /// @brief An FMG ID.
            int32_t TextID = -1;
            /// @brief The type number of this entry as stored in the file.
            ControlType Type() const override { return ControlType::FrpgMenuDlgObjContentsHelpItem; }
            /// @brief Reads the type-specific data.
            void ReadData(BinaryReader& Reader) override;
            /// @brief Writes the type-specific data.
            void WriteData(BinaryWriter& Writer) const override;
        };

        /// @brief Unknown.
        struct SOULS_API Static : Control {
            /// @brief Unknown; always 0.
            int32_t Unk00 = 0;
            /// @brief The type number of this entry as stored in the file.
            ControlType Type() const override { return ControlType::Static; }
            /// @brief Reads the type-specific data.
            void ReadData(BinaryReader& Reader) override;
            /// @brief Writes the type-specific data.
            void WriteData(BinaryWriter& Writer) const override;
        };
#pragma endregion

#pragma region Shapes
        /// @brief The kinds of shape.
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

        /// @brief How a shape's colors are blended with what's behind it.
        enum class BlendingMode : uint8_t {
            Opaque   = 0,
            Alpha    = 1,
            Add      = 2,
            Subtract = 3,
        };

        /// @brief Flags; combine with |.
        enum class SpriteOrientation : uint8_t {
            None           = 0,
            RotateCW       = 0x10,
            Rotate180      = 0x20,
            FlipVertical   = 0x40,
            FlipHorizontal = 0x80,
        };

        /// @brief Flags; combine with |.
        enum class AlignFlags : uint8_t {
            TopLeft          = 0,
            Right            = 1,
            CenterHorizontal = 2,
            Bottom           = 4,
            CenterVertical   = 8,
        };

        /// @brief Where a text shape's text comes from.
        enum class TxtType : uint8_t {
            Literal = 0,
            FMG     = 1,
            Dynamic = 2,
        };

        /// @brief The visual part of an element.
        struct SOULS_API Shape {
            /// @brief Left bound of this element, relative to 1280x720.
            int16_t LeftEdge = 0;
            /// @brief Top bound of this element, relative to 1280x720.
            int16_t TopEdge = 0;
            /// @brief Right bound of this element, relative to 1280x720.
            int16_t RightEdge = 0;
            /// @brief Bottom bound of this element, relative to 1280x720.
            int16_t BottomEdge = 0;
            /// @brief Only stored by Dark Souls Remastered.
            ///
            /// The mode is missing from some files.
            int16_t ScalingOriginX = -1;
            /// @brief For DSR, the Y coordinate which the element scales relative to.
            int16_t ScalingOriginY = -1;
            /// @brief For DSR, the behavior of scaling for this element.
            int16_t ScalingMode = 0;

            virtual ~Shape() = default;
            /// @brief The type number of this entry as stored in the file.
            virtual ShapeType Type() const = 0;
            /// @brief The type's name, as SoulsFormats calls it.
            const char* TypeName() const;
            /// @brief Reads the type-specific data.
            void ReadData(BinaryReader& Reader, DRBVersion Version, const StringTable& Strings);
            /// @brief Writes the type-specific data.
            void WriteData(BinaryWriter& Writer, DRBVersion Version, const StringOffsets& Strings) const;
            /// @brief Strings the shape needs in the string table.
            virtual void CollectStrings(std::vector<std::string>&) const {}

        protected:
            /// @brief Reads the fields specific to this shape type.
            virtual void ReadSpecific(BinaryReader& Reader, const StringTable& Strings) = 0;
            /// @brief Writes the fields specific to this shape type.
            virtual void WriteSpecific(BinaryWriter& Writer, const StringOffsets& Strings) const = 0;
        };

        /// @brief Displays a region of a texture.
        struct SOULS_API SpriteBase : Shape {
            /// @brief Left bound of the texture region displayed by this element.
            int16_t TexLeftEdge = 0;
            /// @brief Top bound of the texture region displayed by this element.
            int16_t TexTopEdge = 0;
            /// @brief Right bound of the texture region displayed by this element.
            int16_t TexRightEdge = 0;
            /// @brief Bottom bound of the texture region displayed by this element.
            int16_t TexBottomEdge = 0;
            /// @brief The texture to display, indexing menu.tpf.
            int16_t TextureIndex = 0;
            /// @brief Flags modifying the orientation of the texture.
            SpriteOrientation Orientation = SpriteOrientation::None;
            /// @brief Color blending mode.
            BlendingMode BlendMode = BlendingMode::Alpha;

        protected:
            /// @brief Reads the fields specific to this shape type.
            void ReadSpecific(BinaryReader& Reader, const StringTable& Strings) override;
            /// @brief Writes the fields specific to this shape type.
            void WriteSpecific(BinaryWriter& Writer, const StringOffsets& Strings) const override;
        };

        /// @brief Either a fixed text or scrolling text element.
        struct SOULS_API TextBase : Shape {
            /// @brief Unknown structure only observed in Shadow Assault: Tenchu.
            struct UnknownB {
                /// @brief Unknown; always 0.
                int32_t Unk00 = 0;
                /// @brief Unknown; always 0xFF.
                int16_t Unk04 = 0xFF;
                /// @brief Unknown; always 0 or 2.
                int16_t Unk06 = 0;
                /// @brief Unknown; always 0 or 2.
                int16_t Unk08 = 0;
            };
            /// @brief Unknown.
            struct UnknownA {
                /// @brief Unknown; always 0.
                int32_t Unk00 = 0;
                /// @brief Unknown optional structure, null if not present.
                std::optional<UnknownB> SubUnknown;
            };

            /// @brief Color blending mode.
            BlendingMode BlendMode = BlendingMode::Alpha;
            /// @brief Distance between each line of text.
            int16_t LineSpacing = 0;
            /// @brief Index of a color in the menu color param, or 0 to use the CustomColor.
            int32_t PaletteColor = 0;
            /// @brief Custom color used when PaletteColor is 0.
            Color CustomColor;
            /// @brief From 0-11, different sizes of the menu font. 12 is the subtitle font.
            int16_t FontSize = 0;
            /// @brief The horizontal and vertical alignment of the text.
            AlignFlags Alignment = AlignFlags::TopLeft;
            /// @brief Whether the element uses a text literal, a static FMG ID, or is assigned at runtime.
            TxtType TextType = TxtType::FMG;
            /// @brief The maximum characters to display.
            int32_t CharLength = 0;
            std::string TextLiteral;  ///< only for TxtType::Literal
            /// @brief If TextType is FMG, the FMG ID to display, otherwise -1.
            int32_t TextID = -1;
            /// @brief An unknown optional structure which is always present in practice.
            std::optional<UnknownA> Unknown = UnknownA {};

            void CollectStrings(std::vector<std::string>& Out) const override;

        protected:
            /// @brief Reads the fields specific to this shape type.
            void ReadSpecific(BinaryReader& Reader, const StringTable& Strings) override;
            /// @brief Writes the fields specific to this shape type.
            void WriteSpecific(BinaryWriter& Writer, const StringOffsets& Strings) const override;
            /// @brief Reads the fields specific to this sprite subtype.
            virtual void ReadSubtype(BinaryReader&) {}
            /// @brief Writes the fields specific to this sprite subtype.
            virtual void WriteSubtype(BinaryWriter&) const {}
        };

        /// @brief A reference to another dialog (by its position in DRB::Dlgs, -1 for none).
        struct SOULS_API Dialog : Shape {
            /// @brief Unknown.
            struct UnknownA {
                /// @brief Unknown; always 0.
                int16_t Unk00 = 0;
                /// @brief Unknown.
                int16_t Unk02 = 0;
                /// @brief Unknown.
                int32_t Unk04 = 0;
            };

            /// @brief The dialog shown, by its position in DRB::Dlgs; -1 for none.
            int16_t DlgIndex = -1;
            /// @brief Unknown; always 0 or 1.
            uint8_t Unk02 = 1;
            /// @brief Unknown; always 0 or 1. Determines whether the optional structure is loaded.
            uint8_t Unk03 = 1;
            /// @brief Index of a color in the menu color param, or 0 to use the CustomColor.
            int32_t PaletteColor = 0;
            /// @brief Custom color used when PaletteColor is 0.
            Color CustomColor = Color::White();
            /// @brief An unknown, optional, and unused structure; only loaded if Unk03 is non-zero.
            std::optional<UnknownA> Unknown;
            /// @brief The type number of this entry as stored in the file.
            ShapeType Type() const override { return ShapeType::Dialog; }

        protected:
            /// @brief Reads the fields specific to this shape type.
            void ReadSpecific(BinaryReader& Reader, const StringTable& Strings) override;
            /// @brief Writes the fields specific to this shape type.
            void WriteSpecific(BinaryWriter& Writer, const StringOffsets& Strings) const override;
        };

        /// @brief A rectangular frame with color interpolated between each corner.
        struct SOULS_API GouraudFrame : Shape {
            /// @brief Color blending mode.
            BlendingMode BlendMode = BlendingMode::Alpha;
            /// @brief Thickness of the border.
            uint8_t Thickness = 1;
            /// @brief The color at the top left corner.
            Color TopLeftColor = Color::Black();
            /// @brief The color at the top right corner.
            Color TopRightColor = Color::Black();
            /// @brief The color at the bottom right corner.
            Color BottomRightColor = Color::Black();
            /// @brief The color at the bottom left corner.
            Color BottomLeftColor = Color::Black();
            /// @brief The type number of this entry as stored in the file.
            ShapeType Type() const override { return ShapeType::GouraudFrame; }

        protected:
            /// @brief Reads the fields specific to this shape type.
            void ReadSpecific(BinaryReader& Reader, const StringTable& Strings) override;
            /// @brief Writes the fields specific to this shape type.
            void WriteSpecific(BinaryWriter& Writer, const StringOffsets& Strings) const override;
        };

        /// @brief A rectangle with color interpolated between each corner.
        struct SOULS_API GouraudRect : Shape {
            /// @brief Color blending mode.
            BlendingMode BlendMode = BlendingMode::Alpha;
            /// @brief The color at the top left corner.
            Color TopLeftColor = Color::Black();
            /// @brief The color at the top right corner.
            Color TopRightColor = Color::Black();
            /// @brief The color at the bottom right corner.
            Color BottomRightColor = Color::Black();
            /// @brief The color at the bottom left corner.
            Color BottomLeftColor = Color::Black();
            /// @brief The type number of this entry as stored in the file.
            ShapeType Type() const override { return ShapeType::GouraudRect; }

        protected:
            /// @brief Reads the fields specific to this shape type.
            void ReadSpecific(BinaryReader& Reader, const StringTable& Strings) override;
            /// @brief Writes the fields specific to this shape type.
            void WriteSpecific(BinaryWriter& Writer, const StringOffsets& Strings) const override;
        };

        /// @brief Displays a texture region with color interpolated between each corner.
        struct SOULS_API GouraudSprite : SpriteBase {
            /// @brief The color at the top left corner.
            Color TopLeftColor = Color::White();
            /// @brief The color at the top right corner.
            Color TopRightColor = Color::White();
            /// @brief The color at the bottom right corner.
            Color BottomRightColor = Color::White();
            /// @brief The color at the bottom left corner.
            Color BottomLeftColor = Color::White();
            /// @brief The type number of this entry as stored in the file.
            ShapeType Type() const override { return ShapeType::GouraudSprite; }

        protected:
            /// @brief Reads the fields specific to this shape type.
            void ReadSpecific(BinaryReader& Reader, const StringTable& Strings) override;
            /// @brief Writes the fields specific to this shape type.
            void WriteSpecific(BinaryWriter& Writer, const StringOffsets& Strings) const override;
        };

        /// @brief Unknown.
        struct SOULS_API Mask : Shape {
            /// @brief The name of the element this mask applies to.
            std::string DlgoName;
            /// @brief Unknown; always 1.
            uint8_t Unk04 = 1;
            /// @brief Unknown; always 0.
            int32_t Unk05 = 0;
            /// @brief Unknown; always 0.
            int32_t Unk09 = 0;
            /// @brief Unknown; always 0.
            int16_t Unk0D = 0;
            /// @brief The type number of this entry as stored in the file.
            ShapeType Type() const override { return ShapeType::Mask; }
            void CollectStrings(std::vector<std::string>& Out) const override { Out.push_back(DlgoName); }

        protected:
            /// @brief Reads the fields specific to this shape type.
            void ReadSpecific(BinaryReader& Reader, const StringTable& Strings) override;
            /// @brief Writes the fields specific to this shape type.
            void WriteSpecific(BinaryWriter& Writer, const StringOffsets& Strings) const override;
        };

        /// @brief A rectangular frame with a solid color.
        struct SOULS_API MonoFrame : Shape {
            /// @brief Color blending mode.
            BlendingMode BlendMode = BlendingMode::Alpha;
            /// @brief Thickness of the border.
            uint8_t Thickness = 1;
            /// @brief Index of a color in the menu color param, or 0 to use the CustomColor.
            int32_t PaletteColor = 0;
            /// @brief Custom color used when PaletteColor is 0.
            Color CustomColor = Color::Black();
            /// @brief The type number of this entry as stored in the file.
            ShapeType Type() const override { return ShapeType::MonoFrame; }

        protected:
            /// @brief Reads the fields specific to this shape type.
            void ReadSpecific(BinaryReader& Reader, const StringTable& Strings) override;
            /// @brief Writes the fields specific to this shape type.
            void WriteSpecific(BinaryWriter& Writer, const StringOffsets& Strings) const override;
        };

        /// @brief A rectangle with a solid color.
        struct SOULS_API MonoRect : Shape {
            /// @brief Color blending mode.
            BlendingMode BlendMode = BlendingMode::Alpha;
            /// @brief Index of a color in the menu color param, or 0 to use the CustomColor.
            int32_t PaletteColor = 0;
            /// @brief Custom color used when PaletteColor is 0.
            Color CustomColor = Color::Black();
            /// @brief The type number of this entry as stored in the file.
            ShapeType Type() const override { return ShapeType::MonoRect; }

        protected:
            /// @brief Reads the fields specific to this shape type.
            void ReadSpecific(BinaryReader& Reader, const StringTable& Strings) override;
            /// @brief Writes the fields specific to this shape type.
            void WriteSpecific(BinaryWriter& Writer, const StringOffsets& Strings) const override;
        };

        /// @brief An invisible element used to mark a position.
        struct SOULS_API Null : Shape {
            /// @brief The type number of this entry as stored in the file.
            ShapeType Type() const override { return ShapeType::Null; }

        protected:
            /// @brief Reads the fields specific to this shape type.
            void ReadSpecific(BinaryReader&, const StringTable&) override {}
            /// @brief Writes the fields specific to this shape type.
            void WriteSpecific(BinaryWriter&, const StringOffsets&) const override {}
        };

        /// @brief A scrolling text field.
        struct SOULS_API ScrollText : TextBase {
            /// @brief Unknown.
            int16_t UnkX00 = 0;
            /// @brief Unknown.
            int16_t UnkX02 = 0;
            /// @brief Unknown.
            int16_t UnkX04 = 0;
            /// @brief Unknown.
            int16_t UnkX06 = 0;
            /// @brief Unknown; always 15 or 100.
            int16_t ScrollSpeed = 15;
            /// @brief Unknown.
            int16_t UnkX0A = 0;
            /// @brief The type number of this entry as stored in the file.
            ShapeType Type() const override { return ShapeType::ScrollText; }

        protected:
            /// @brief Reads the fields specific to this sprite subtype.
            void ReadSubtype(BinaryReader& Reader) override;
            /// @brief Writes the fields specific to this sprite subtype.
            void WriteSubtype(BinaryWriter& Writer) const override;
        };

        /// @brief Displays a texture region with color tinting.
        struct SOULS_API Sprite : SpriteBase {
            /// @brief Index of a color in the menu color param, or 0 to use the CustomColor.
            int32_t PaletteColor = 0;
            /// @brief Custom color used when PaletteColor is 0.
            Color CustomColor = Color::White();
            /// @brief The type number of this entry as stored in the file.
            ShapeType Type() const override { return ShapeType::Sprite; }

        protected:
            /// @brief Reads the fields specific to this shape type.
            void ReadSpecific(BinaryReader& Reader, const StringTable& Strings) override;
            /// @brief Writes the fields specific to this shape type.
            void WriteSpecific(BinaryWriter& Writer, const StringOffsets& Strings) const override;
        };

        /// @brief A fixed text field.
        struct SOULS_API Text : TextBase {
            /// @brief The type number of this entry as stored in the file.
            ShapeType Type() const override { return ShapeType::Text; }
        };
#pragma endregion

#pragma region Animations
        /// @brief Unknown.
        struct Anik {
            /// @brief The name of this Anik.
            std::string Name;
            /// @brief Unknown.
            int32_t Unk04 = 0;
            /// @brief Unknown; always 0-1.
            uint8_t Unk08 = 0;
            /// @brief Unknown; always 1-2.
            uint8_t Unk09 = 0;
            /// @brief Unknown; always 0.
            int16_t Unk0A = 0;
            /// @brief An offset into the INTP block.
            int32_t IntpOffset = 0;
            /// @brief An offset into the ANIP block.
            int32_t AnipOffset = 0;
            /// @brief Unknown; always 0.
            int32_t Unk14 = 0;
            /// @brief Unknown; always 0.
            int32_t Unk18 = 0;
            /// @brief Unknown; always 0.
            int32_t Unk1C = 0;
        };

        /// @brief Unknown.
        struct Anio {
            /// @brief Unknown.
            int32_t Unk00 = 0;
            /// @brief Aniks in this Anio.
            std::vector<Anik> Aniks;
            /// @brief Unknown.
            int32_t Unk0C = 0;
        };

        /// @brief Unknown.
        struct Anim {
            /// @brief The name of this Anim.
            std::string Name;
            /// @brief Anios in this Anim.
            std::vector<Anio> Anios;
            /// @brief Unknown.
            int32_t Unk0C = 0;
            /// @brief Unknown; always 4.
            int32_t Unk10 = 4;
            /// @brief Unknown; always 4.
            int32_t Unk14 = 4;
            /// @brief Unknown; always 4.
            int32_t Unk18 = 4;
            /// @brief Unknown; always 1.
            int32_t Unk1C = 1;
            /// @brief Unknown; always 0.
            int32_t Unk20 = 0;
            /// @brief Unknown; always 0.
            int32_t Unk24 = 0;
            /// @brief Unknown; always 0.
            int32_t Unk28 = 0;
            /// @brief Unknown; always 0.
            int32_t Unk2C = 0;
        };

        /// @brief Unknown.
        struct Scdk {
            /// @brief The name of this Scdk.
            std::string Name;
            /// @brief Unknown.
            int32_t Unk04 = 0;
            /// @brief Unknown; always 1.
            int32_t Unk08 = 1;
            /// @brief Unknown; always 0-1.
            int32_t Unk0C = 0;
            /// @brief Unknown.
            int32_t Unk14 = 0;
            /// @brief Unknown; always 0.
            int32_t Unk18 = 0;
            /// @brief Unknown; always 0.
            int32_t Unk1C = 0;
            /// @brief An index into the Anim list.
            int32_t AnimIndex = 0;
            /// @brief Unknown; always 0-1.
            int32_t Scdp04 = 0;
        };

        /// @brief Unknown.
        struct Scdo {
            /// @brief The name of this Scdo.
            std::string Name;
            /// @brief Scdks in this Scdo.
            std::vector<Scdk> Scdks;
            /// @brief Unknown.
            int32_t Unk0C = 0;
        };

        /// @brief Unknown.
        struct Scdl {
            /// @brief The name of this Scdl.
            std::string Name;
            /// @brief Scdos in this Scdl.
            std::vector<Scdo> Scdos;
            /// @brief Unknown.
            int32_t Unk0C = 0;
        };
#pragma endregion

#pragma region Dialogs
        /// @brief An element of a dialog.
        struct Dlgo {
            /// @brief The name of the element.
            std::string Name;
            /// @brief The visual properties of the element.
            std::shared_ptr<Shape> Shape_ = std::make_shared<Null>();
            /// @brief The behavior of the element.
            std::shared_ptr<Control> Control_ = std::make_shared<Static>();
            /// @brief Unknown; always 0.
            int32_t Unk0C = 0;
            /// @brief Unknown; always 0.
            int32_t Unk10 = 0;
            /// @brief Unknown; always 0.
            int32_t Unk14 = 0;
            /// @brief Unknown; always 0.
            int32_t Unk18 = 0;
            /// @brief Unknown; always 0.
            int32_t Unk1C = 0;
        };

        /// @brief A dialog: an element that contains other elements.
        struct Dlg : Dlgo {
            /// @brief The child elements attached to this group.
            std::vector<Dlgo> Dlgos;
            /// @brief Left edge of the group.
            int16_t LeftEdge = 0;
            /// @brief Top edge of the group.
            int16_t TopEdge = 0;
            /// @brief Right edge of the group.
            int16_t RightEdge = 0;
            /// @brief Bottom edge of the group.
            int16_t BottomEdge = 0;
            /// @brief Unknown.
            std::array<int16_t, 5> Unk30 {-1, -1, -1, -1, -1};
            /// @brief Unknown; always 0.
            int16_t Unk3A = 0;
            /// @brief Unknown; always 0.
            int32_t Unk3C = 0;

            /// @brief The element with this name, or nullptr.
            Dlgo* Find(const std::string& Name) {
                for (Dlgo& D : Dlgos) {
                    if (D.Name == Name) { return &D; }
                }
                return nullptr;
            }
        };
#pragma endregion

        /// @brief The version of the format used for this file.
        DRBVersion Version = DRBVersion::DarkSoulsRemastered;
        /// @brief True for PS3/X360, false otherwise.
        bool BigEndian = false;
        /// @brief Available UI textures for reference; not actually necessary for textures to work.
        std::vector<Texture> Textures;
        /// @brief Unknown data used by Aniks.
        std::vector<uint8_t> AnipBytes;
        /// @brief Unknown data used by Aniks.
        std::vector<uint8_t> IntpBytes;
        /// @brief Unknown.
        std::vector<Anim> Anims;
        /// @brief Unknown.
        std::vector<Scdl> Scdls;
        /// @brief Groups of UI elements.
        std::vector<Dlg> Dlgs;

        /// @brief The dialog with this name, or nullptr.
        Dlg* Find(const std::string& Name) {
            for (Dlg& D : Dlgs) {
                if (D.Name == Name) { return &D; }
            }
            return nullptr;
        }

        /// @brief Reads a file of a known game, skipping the version detection.
        static DRB Read(std::span<const uint8_t> Data, DRBVersion Version);
        /// @brief Reads a file of a known game from disk, skipping the version detection.
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
