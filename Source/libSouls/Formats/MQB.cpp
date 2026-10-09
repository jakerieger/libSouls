//
// Created by Jake Rieger on 10/8/2026.
//

#include "MQB.hpp"

#include <libSouls/TextEncoding.hpp>

#include <map>

namespace Souls {
    namespace {
        using Text::UTF16ToUTF8;
        using Text::UTF8ToUTF16;
        using CustomData = MQB::CustomData;
        using DataType   = MQB::CustomData::DataType;

        std::string ReadName(BinaryReader& Reader) {
            return UTF16ToUTF8(Reader.ReadFixStrW(0x40));
        }

        void WriteName(BinaryWriter& Writer, const std::string& Name) {
            Writer.WriteFixStrW(UTF8ToUTF16(Name), 0x40, 0x00);
        }

        std::vector<int64_t> GetVarints(BinaryReader& Reader, int64_t Offset, size_t Count) {
            std::vector<int64_t> Result;
            Reader.StepIn(Offset);
            for (size_t I = 0; I < Count; ++I) {
                Result.push_back(Reader.ReadVarint());
            }
            Reader.StepOut();
            return Result;
        }

        // State shared by everything written, since some data is written far from where it's referenced.
        struct WriteContext {
            std::vector<const CustomData*> AllCustomData;
            std::vector<int64_t> ValueOffsets;
            std::map<const MQB::Disposition*, int64_t> DisposOffsets;
        };

#pragma region CustomData
        CustomData::Point ReadPoint(BinaryReader& Reader, DataType ValueType, int32_t PointType) {
            CustomData::Point P;
            switch (ValueType) {
                case DataType::Byte: P.Value = Reader.ReadByte(); break;
                case DataType::Float: P.Value = Reader.ReadFloat(); break;
                default: throw BinaryException("Unsupported sequence value type");
            }
            if (ValueType == DataType::Byte) {
                Reader.Assert<int16_t>(0);
                Reader.Assert<uint8_t>(0);
            }
            Reader.Assert<int32_t>(0);
            P.Unk08 = Reader.ReadInt32();
            Reader.Assert<int32_t>(0);
            // Probably also variable type, but in the few instances of point type 2 with value type 3 they're all 0.
            if (PointType == 2) {
                P.Unk10 = Reader.ReadFloat();
                P.Unk14 = Reader.ReadFloat();
            }
            return P;
        }

        void WritePoint(BinaryWriter& Writer, const CustomData::Point& P, DataType ValueType, int32_t PointType) {
            switch (ValueType) {
                case DataType::Byte: Writer.WriteByte(std::get<uint8_t>(P.Value)); break;
                case DataType::Float: Writer.WriteFloat(std::get<float>(P.Value)); break;
                default: throw BinaryException("Unsupported sequence value type");
            }
            if (ValueType == DataType::Byte) {
                Writer.WriteInt16(0);
                Writer.WriteByte(0);
            }
            Writer.WriteInt32(0);
            Writer.WriteInt32(P.Unk08);
            Writer.WriteInt32(0);
            if (PointType == 2) {
                Writer.WriteFloat(P.Unk10);
                Writer.WriteFloat(P.Unk14);
            }
        }

        CustomData::Sequence ReadSequence(BinaryReader& Reader, int64_t ParentValueOffset) {
            CustomData::Sequence S;
            Reader.Assert<int32_t>(0x1C);  // sequence size
            const int32_t PointCount = Reader.ReadInt32();
            S.ValueType              = static_cast<DataType>(Reader.ReadUInt32());
            S.PointType              = Reader.Assert<int32_t>(1, 2);
            Reader.Assert<int32_t>(S.PointType == 1 ? 0x10 : 0x18);  // point size
            const int32_t PointsOffset = Reader.ReadInt32();
            const int32_t ValueOffset  = Reader.ReadInt32();
            if (S.ValueType == DataType::Byte) {
                if (ValueOffset < ParentValueOffset || ValueOffset > ParentValueOffset + 2) {
                    throw BinaryException("Unexpected sequence value offset for a byte sequence");
                }
                S.ValueIndex = static_cast<int32_t>(ValueOffset - ParentValueOffset);
            } else if (S.ValueType == DataType::Float) {
                if (ValueOffset != ParentValueOffset) {
                    throw BinaryException("Unexpected sequence value offset for a float sequence");
                }
                S.ValueIndex = 0;
            } else {
                throw BinaryException("Unsupported sequence value type");
            }
            Reader.StepIn(PointsOffset);
            for (int32_t I = 0; I < PointCount; ++I) {
                S.Points.push_back(ReadPoint(Reader, S.ValueType, S.PointType));
            }
            Reader.StepOut();
            return S;
        }

        CustomData ReadCustomData(BinaryReader& Reader) {
            CustomData D;
            D.Name = ReadName(Reader);
            D.Type = static_cast<DataType>(Reader.ReadUInt32());
            Reader.Assert<int32_t>(D.Type == DataType::Color ? 3 : 0);
            int64_t ValueOffset = Reader.Position();
            int32_t Length      = 0;  // of the extra data after the header, for strings, custom data and colors
            switch (D.Type) {
                case DataType::Bool: D.Value = Reader.ReadBool(); break;
                case DataType::SByte: D.Value = Reader.ReadSByte(); break;
                case DataType::Byte: D.Value = Reader.ReadByte(); break;
                case DataType::Short: D.Value = Reader.ReadInt16(); break;
                case DataType::Int: D.Value = Reader.ReadInt32(); break;
                case DataType::UInt: D.Value = Reader.ReadUInt32(); break;
                case DataType::Float: D.Value = Reader.ReadFloat(); break;
                case DataType::String:
                case DataType::Custom:
                case DataType::Color: Length = Reader.ReadInt32(); break;
                default: throw BinaryException("Unimplemented custom data type: " + std::to_string(static_cast<uint32_t>(D.Type)));
            }
            if (D.Type == DataType::Bool || D.Type == DataType::SByte || D.Type == DataType::Byte) {
                Reader.Assert<uint8_t>(0);
                Reader.Assert<int16_t>(0);
            } else if (D.Type == DataType::Short) {
                Reader.Assert<int16_t>(0);
            }
            Reader.Assert<int32_t>(0);
            const int32_t SequencesOffset = Reader.ReadInt32();
            const int32_t SequenceCount   = Reader.ReadInt32();
            Reader.Assert<int32_t>(0);
            Reader.Assert<int32_t>(0);
            if (D.Type == DataType::String) {
                if (Length == 0 || Length % 0x10 != 0) {
                    throw BinaryException("Unexpected custom data string length: " + std::to_string(Length));
                }
                D.Value = UTF16ToUTF8(Reader.ReadFixStrW(static_cast<size_t>(Length)));
            } else if (D.Type == DataType::Custom) {
                if (Length % 4 != 0) {
                    throw BinaryException("Unexpected custom data custom length: " + std::to_string(Length));
                }
                D.Value = Reader.ReadBytes(static_cast<size_t>(Length));
            } else if (D.Type == DataType::Color) {
                if (Length != 4) {
                    throw BinaryException("Unexpected custom data color length: " + std::to_string(Length));
                }
                ValueOffset     = Reader.Position();
                const uint8_t R = Reader.ReadByte(), G = Reader.ReadByte(), B = Reader.ReadByte();
                D.Value         = Color::FromArgb(255, R, G, B);
                Reader.Assert<uint8_t>(0);
            }
            if (SequenceCount > 0) {
                Reader.StepIn(SequencesOffset);
                for (int32_t I = 0; I < SequenceCount; ++I) {
                    D.Sequences.push_back(ReadSequence(Reader, ValueOffset));
                }
                Reader.StepOut();
            }
            return D;
        }

        void WriteCustomData(BinaryWriter& Writer, const CustomData& D, WriteContext& Context) {
            WriteName(Writer, D.Name);
            Writer.WriteUInt32(static_cast<uint32_t>(D.Type));
            Writer.WriteInt32(D.Type == DataType::Color ? 3 : 0);
            int32_t Length = -1;
            if (D.Type == DataType::String) {
                const std::u16string Text = UTF8ToUTF16(std::get<std::string>(D.Value));
                Length                    = static_cast<int32_t>((Text.size() + 1) * 2);
                if (Length % 0x10 != 0) {
                    Length += 0x10 - Length % 0x10;
                }
            } else if (D.Type == DataType::Custom) {
                Length = static_cast<int32_t>(std::get<std::vector<uint8_t>>(D.Value).size());
                if (Length % 4 != 0) {
                    throw BinaryException("Unexpected custom data custom length: " + std::to_string(Length));
                }
            } else if (D.Type == DataType::Color) {
                Length = 4;
            }
            int64_t ValueOffset = Writer.Position();
            switch (D.Type) {
                case DataType::Bool: Writer.WriteBool(std::get<bool>(D.Value)); break;
                case DataType::SByte: Writer.WriteSByte(std::get<int8_t>(D.Value)); break;
                case DataType::Byte: Writer.WriteByte(std::get<uint8_t>(D.Value)); break;
                case DataType::Short: Writer.WriteInt16(std::get<int16_t>(D.Value)); break;
                case DataType::Int: Writer.WriteInt32(std::get<int32_t>(D.Value)); break;
                case DataType::UInt: Writer.WriteUInt32(std::get<uint32_t>(D.Value)); break;
                case DataType::Float: Writer.WriteFloat(std::get<float>(D.Value)); break;
                case DataType::String:
                case DataType::Custom:
                case DataType::Color: Writer.WriteInt32(Length); break;
                default: throw BinaryException("Unimplemented custom data type: " + std::to_string(static_cast<uint32_t>(D.Type)));
            }
            if (D.Type == DataType::Bool || D.Type == DataType::SByte || D.Type == DataType::Byte) {
                Writer.WriteByte(0);
                Writer.WriteInt16(0);
            } else if (D.Type == DataType::Short) {
                Writer.WriteInt16(0);
            }
            // This is probably wrong for the 64-bit format.
            Writer.WriteInt32(0);
            Writer.Reserve<int32_t>("SequencesOffset[" + std::to_string(Context.AllCustomData.size()) + "]");
            Writer.WriteInt32(static_cast<int32_t>(D.Sequences.size()));
            Writer.WriteInt32(0);
            Writer.WriteInt32(0);
            if (D.Type == DataType::String) {
                Writer.WriteFixStrW(UTF8ToUTF16(std::get<std::string>(D.Value)), static_cast<size_t>(Length), 0x00);
            } else if (D.Type == DataType::Custom) {
                Writer.WriteBytes(std::get<std::vector<uint8_t>>(D.Value));
            } else if (D.Type == DataType::Color) {
                const Color C = std::get<Color>(D.Value);
                ValueOffset   = Writer.Position();
                Writer.WriteByte(C.R);
                Writer.WriteByte(C.G);
                Writer.WriteByte(C.B);
                Writer.WriteByte(0);
            }
            Context.AllCustomData.push_back(&D);
            Context.ValueOffsets.push_back(ValueOffset);
        }

        void WriteSequences(BinaryWriter& Writer, const CustomData& D, size_t Index, int64_t ValueOffset) {
            const std::string Label = "SequencesOffset[" + std::to_string(Index) + "]";
            if (D.Sequences.empty()) {
                Writer.Fill<int32_t>(Label, 0);
                return;
            }
            Writer.Fill<int32_t>(Label, static_cast<int32_t>(Writer.Position()));
            for (size_t I = 0; I < D.Sequences.size(); ++I) {
                const CustomData::Sequence& S = D.Sequences[I];
                Writer.WriteInt32(0x1C);
                Writer.WriteInt32(static_cast<int32_t>(S.Points.size()));
                Writer.WriteUInt32(static_cast<uint32_t>(S.ValueType));
                Writer.WriteInt32(S.PointType);
                Writer.WriteInt32(S.PointType == 1 ? 0x10 : 0x18);
                Writer.Reserve<int32_t>("PointsOffset[" + std::to_string(Index) + ":" + std::to_string(I) + "]");
                if (S.ValueType == DataType::Byte) {
                    Writer.WriteInt32(static_cast<int32_t>(ValueOffset) + S.ValueIndex);
                } else if (S.ValueType == DataType::Float) {
                    Writer.WriteInt32(static_cast<int32_t>(ValueOffset));
                }
            }
        }

        void WriteSequencePoints(BinaryWriter& Writer, const CustomData& D, size_t Index) {
            for (size_t I = 0; I < D.Sequences.size(); ++I) {
                const CustomData::Sequence& S = D.Sequences[I];
                Writer.Fill<int32_t>("PointsOffset[" + std::to_string(Index) + ":" + std::to_string(I) + "]", static_cast<int32_t>(Writer.Position()));
                for (const CustomData::Point& P : S.Points) {
                    WritePoint(Writer, P, S.ValueType, S.PointType);
                }
            }
        }
#pragma endregion

#pragma region Dispositions and timelines
        MQB::Transform ReadTransform(BinaryReader& Reader) {
            MQB::Transform T;
            T.Frame       = Reader.ReadFloat();
            T.Translation = Reader.ReadVector3();
            T.Unk10       = Reader.ReadVector3();
            T.Unk1C       = Reader.ReadVector3();
            T.Rotation    = Reader.ReadVector3();
            T.Unk34       = Reader.ReadVector3();
            T.Unk40       = Reader.ReadVector3();
            T.Scale       = Reader.ReadVector3();
            T.Unk58       = Reader.ReadVector3();
            T.Unk64       = Reader.ReadVector3();
            return T;
        }

        void WriteTransform(BinaryWriter& Writer, const MQB::Transform& T) {
            Writer.WriteFloat(T.Frame);
            Writer.WriteVector3(T.Translation);
            Writer.WriteVector3(T.Unk10);
            Writer.WriteVector3(T.Unk1C);
            Writer.WriteVector3(T.Rotation);
            Writer.WriteVector3(T.Unk34);
            Writer.WriteVector3(T.Unk40);
            Writer.WriteVector3(T.Scale);
            Writer.WriteVector3(T.Unk58);
            Writer.WriteVector3(T.Unk64);
        }

        MQB::Disposition ReadDisposition(BinaryReader& Reader) {
            MQB::Disposition D;
            D.ID                      = Reader.ReadInt32();
            D.ResourceIndex           = Reader.ReadInt32();
            D.Unk08                   = Reader.ReadInt32();
            D.StartFrame              = Reader.ReadInt32();
            D.Duration                = Reader.ReadInt32();
            D.Unk14                   = Reader.ReadInt32();
            D.Unk18                   = Reader.ReadInt32();
            D.Unk1C                   = Reader.ReadInt32();
            D.Unk20                   = Reader.Assert<int32_t>(0, 1);
            const int32_t CustomCount = Reader.ReadInt32();
            D.Unk28                   = Reader.ReadInt32();
            Reader.Assert<int32_t>(0);
            for (int32_t I = 0; I < CustomCount; ++I) {
                D.CustomData.push_back(ReadCustomData(Reader));
            }
            Reader.Assert<int32_t>(0);
            const int32_t TransformCount = Reader.ReadInt32();
            for (int I = 0; I < 6; ++I) {
                Reader.Assert<int32_t>(0);
            }
            for (int32_t I = 0; I < TransformCount; ++I) {
                D.Transforms.push_back(ReadTransform(Reader));
            }
            return D;
        }

        void WriteDisposition(BinaryWriter& Writer, const MQB::Disposition& D, WriteContext& Context) {
            Writer.WriteInt32(D.ID);
            Writer.WriteInt32(D.ResourceIndex);
            Writer.WriteInt32(D.Unk08);
            Writer.WriteInt32(D.StartFrame);
            Writer.WriteInt32(D.Duration);
            Writer.WriteInt32(D.Unk14);
            Writer.WriteInt32(D.Unk18);
            Writer.WriteInt32(D.Unk1C);
            Writer.WriteInt32(D.Unk20);
            Writer.WriteInt32(static_cast<int32_t>(D.CustomData.size()));
            Writer.WriteInt32(D.Unk28);
            Writer.WriteInt32(0);
            for (const CustomData& C : D.CustomData) {
                WriteCustomData(Writer, C, Context);
            }
            Writer.WriteInt32(0);
            Writer.WriteInt32(static_cast<int32_t>(D.Transforms.size()));
            for (int I = 0; I < 6; ++I) {
                Writer.WriteInt32(0);
            }
            for (const MQB::Transform& T : D.Transforms) {
                WriteTransform(Writer, T);
            }
        }

        MQB::Timeline ReadTimeline(BinaryReader& Reader, MQB::MQBVersion Version, std::map<int64_t, MQB::Disposition>& DisposByOffset) {
            MQB::Timeline T;
            const int64_t DisposOffsetsOffset = Reader.ReadVarint();
            const int32_t DisposCount         = Reader.ReadInt32();
            if (Version == MQB::MQBVersion::DarkSouls2Scholar) {
                Reader.Assert<int32_t>(0);
            }
            const int64_t CustomDataOffset = Reader.ReadVarint();
            const int32_t CustomDataCount  = Reader.ReadInt32();
            T.Unk10                        = Reader.ReadInt32();
            for (const int64_t Offset : GetVarints(Reader, DisposOffsetsOffset, static_cast<size_t>(DisposCount))) {
                const auto Found = DisposByOffset.find(Offset);
                if (Found == DisposByOffset.end()) {
                    throw BinaryException("MQB timeline refers to a missing disposition");
                }
                T.Dispositions.push_back(std::move(Found->second));
                DisposByOffset.erase(Found);
            }
            Reader.StepIn(CustomDataOffset);
            for (int32_t I = 0; I < CustomDataCount; ++I) {
                T.CustomData.push_back(ReadCustomData(Reader));
            }
            Reader.StepOut();
            return T;
        }

        MQB::Cut ReadCut(BinaryReader& Reader, MQB::MQBVersion Version) {
            MQB::Cut C;
            C.Name                    = ReadName(Reader);
            const int32_t DisposCount = Reader.ReadInt32();
            C.Unk44                   = Reader.ReadInt32();
            C.Duration                = Reader.ReadInt32();
            Reader.Assert<int32_t>(0);
            const int32_t TimelineCount = Reader.ReadInt32();
            if (Version == MQB::MQBVersion::DarkSouls2Scholar) {
                Reader.Assert<int32_t>(0);
            }
            const int64_t TimelinesOffset = Reader.ReadVarint();
            if (Version != MQB::MQBVersion::DarkSouls2Scholar) {
                Reader.Assert<int64_t>(0);
            }
            std::map<int64_t, MQB::Disposition> DisposByOffset;
            for (int32_t I = 0; I < DisposCount; ++I) {
                const int64_t Offset    = Reader.Position();
                DisposByOffset[Offset] = ReadDisposition(Reader);
            }
            Reader.StepIn(TimelinesOffset);
            for (int32_t I = 0; I < TimelineCount; ++I) {
                C.Timelines.push_back(ReadTimeline(Reader, Version, DisposByOffset));
            }
            Reader.StepOut();
            return C;
        }
#pragma endregion
    }  // namespace

    bool MQB::IsImpl(BinaryReader& Reader) {
        return Reader.Length() >= 4 && Reader.GetASCII(0, 4) == "MQB ";
    }

    void MQB::ReadImpl(BinaryReader& Reader) {
        Reader.AssertMagic("MQB ");
        BigEndian    = Reader.Assert<int8_t>(0, -1) == -1;
        Reader.Order = BigEndian ? Endian::Big : Endian::Little;
        Reader.Assert<uint8_t>(0);
        const int8_t LongFormat = Reader.Assert<int8_t>(0, -1);
        Reader.Assert<uint8_t>(0);
        Version                 = static_cast<MQBVersion>(Reader.ReadUInt32());
        const int32_t HeaderSize = Reader.ReadInt32();
        if ((Version != MQBVersion::DarkSouls2Scholar && LongFormat == -1) || (Version == MQBVersion::DarkSouls2Scholar && LongFormat == 0)) {
            throw BinaryException("Unexpected long format for this MQB version.");
        }
        if ((Version == MQBVersion::DarkSouls2 && HeaderSize != 0x14) || (Version == MQBVersion::DarkSouls2Scholar && HeaderSize != 0x28) ||
            (Version == MQBVersion::Bloodborne && HeaderSize != 0x20) || (Version == MQBVersion::DarkSouls3 && HeaderSize != 0x24)) {
            throw BinaryException("Unexpected header size for this MQB version.");
        }
        Reader.VarintLong                = Version == MQBVersion::DarkSouls2Scholar;
        const int64_t ResourcePathsOffset = Reader.ReadVarint();
        if (Version == MQBVersion::DarkSouls2Scholar) {
            for (int I = 0; I < 4; ++I) {
                Reader.Assert<int32_t>(0);
            }
        } else if (Version >= MQBVersion::Bloodborne) {
            Reader.Assert<int32_t>(1);
            Reader.Assert<int32_t>(0);
            Reader.Assert<int32_t>(0);
            if (Version >= MQBVersion::DarkSouls3) {
                Reader.Assert<int32_t>(0);
            }
        }
        Name                         = ReadName(Reader);
        Framerate                    = Reader.ReadFloat();
        const int32_t ResourceCount  = Reader.ReadInt32();
        const int32_t CutCount       = Reader.ReadInt32();
        for (int I = 0; I < 5; ++I) {
            Reader.Assert<int32_t>(0);
        }

        Resources.clear();
        for (int32_t I = 0; I < ResourceCount; ++I) {
            Resource R;
            R.Name        = ReadName(Reader);
            R.ParentIndex = Reader.ReadInt32();
            Reader.Assert<int32_t>(I);
            R.Unk48                   = Reader.ReadInt32();
            const int32_t CustomCount = Reader.ReadInt32();
            for (int32_t J = 0; J < CustomCount; ++J) {
                R.CustomData.push_back(ReadCustomData(Reader));
            }
            Resources.push_back(std::move(R));
        }
        Cuts.clear();
        for (int32_t I = 0; I < CutCount; ++I) {
            Cuts.push_back(ReadCut(Reader, Version));
        }

        Reader.Seek(ResourcePathsOffset);
        const std::vector<int64_t> PathOffsets = GetVarints(Reader, ResourcePathsOffset, static_cast<size_t>(ResourceCount));
        Reader.Skip(static_cast<int64_t>(ResourceCount) * (Reader.VarintLong ? 8 : 4));
        ResourceDirectory = Reader.ReadUTF16Text();
        for (int32_t I = 0; I < ResourceCount; ++I) {
            if (PathOffsets[static_cast<size_t>(I)] != 0) {
                Resources[static_cast<size_t>(I)].Path = Reader.GetUTF16Text(PathOffsets[static_cast<size_t>(I)]);
            }
        }
    }

    void MQB::WriteImpl(BinaryWriter& Writer) {
        Writer.Order      = BigEndian ? Endian::Big : Endian::Little;
        Writer.VarintLong = Version == MQBVersion::DarkSouls2Scholar;
        Writer.WriteMagic("MQB ");
        Writer.WriteSByte(BigEndian ? -1 : 0);
        Writer.WriteByte(0);
        Writer.WriteSByte(Version == MQBVersion::DarkSouls2Scholar ? -1 : 0);
        Writer.WriteByte(0);
        Writer.WriteUInt32(static_cast<uint32_t>(Version));
        switch (Version) {
            case MQBVersion::DarkSouls2: Writer.WriteInt32(0x14); break;
            case MQBVersion::DarkSouls2Scholar: Writer.WriteInt32(0x28); break;
            case MQBVersion::Bloodborne: Writer.WriteInt32(0x20); break;
            case MQBVersion::DarkSouls3: Writer.WriteInt32(0x24); break;
            default: throw BinaryException("Missing header size for this MQB version.");
        }
        Writer.ReserveVarint("ResourcePathsOffset");
        if (Version == MQBVersion::DarkSouls2Scholar) {
            for (int I = 0; I < 4; ++I) {
                Writer.WriteInt32(0);
            }
        } else if (Version >= MQBVersion::Bloodborne) {
            Writer.WriteInt32(1);
            Writer.WriteInt32(0);
            Writer.WriteInt32(0);
            if (Version >= MQBVersion::DarkSouls3) {
                Writer.WriteInt32(0);
            }
        }
        WriteName(Writer, Name);
        Writer.WriteFloat(Framerate);
        Writer.WriteInt32(static_cast<int32_t>(Resources.size()));
        Writer.WriteInt32(static_cast<int32_t>(Cuts.size()));
        for (int I = 0; I < 5; ++I) {
            Writer.WriteInt32(0);
        }

        WriteContext Context;
        for (size_t I = 0; I < Resources.size(); ++I) {
            const Resource& R = Resources[I];
            WriteName(Writer, R.Name);
            Writer.WriteInt32(R.ParentIndex);
            Writer.WriteInt32(static_cast<int32_t>(I));
            Writer.WriteInt32(R.Unk48);
            Writer.WriteInt32(static_cast<int32_t>(R.CustomData.size()));
            for (const CustomData& C : R.CustomData) {
                WriteCustomData(Writer, C, Context);
            }
        }

        for (size_t I = 0; I < Cuts.size(); ++I) {
            const Cut& C = Cuts[I];
            size_t DisposCount = 0;
            for (const Timeline& T : C.Timelines) {
                DisposCount += T.Dispositions.size();
            }
            WriteName(Writer, C.Name);
            Writer.WriteInt32(static_cast<int32_t>(DisposCount));
            Writer.WriteInt32(C.Unk44);
            Writer.WriteInt32(C.Duration);
            Writer.WriteInt32(0);
            Writer.WriteInt32(static_cast<int32_t>(C.Timelines.size()));
            if (Version == MQBVersion::DarkSouls2Scholar) {
                Writer.WriteInt32(0);
            }
            Writer.ReserveVarint("TimelinesOffset" + std::to_string(I));
            if (Version != MQBVersion::DarkSouls2Scholar) {
                Writer.WriteInt64(0);
            }
            for (const Timeline& T : C.Timelines) {
                for (const Disposition& D : T.Dispositions) {
                    Context.DisposOffsets[&D] = Writer.Position();
                    WriteDisposition(Writer, D, Context);
                }
            }
        }

        const auto Tag = [](size_t Cut, size_t Timeline, const char* What) {
            return std::string(What) + "[" + std::to_string(Cut) + ":" + std::to_string(Timeline) + "]";
        };
        for (size_t I = 0; I < Cuts.size(); ++I) {
            Writer.FillVarint("TimelinesOffset" + std::to_string(I), Writer.Position());
            for (size_t J = 0; J < Cuts[I].Timelines.size(); ++J) {
                const Timeline& T = Cuts[I].Timelines[J];
                Writer.ReserveVarint(Tag(I, J, "DisposOffsetsOffset"));
                Writer.WriteInt32(static_cast<int32_t>(T.Dispositions.size()));
                if (Version == MQBVersion::DarkSouls2Scholar) {
                    Writer.WriteInt32(0);
                }
                Writer.ReserveVarint(Tag(I, J, "TimelineCustomDataOffset"));
                Writer.WriteInt32(static_cast<int32_t>(T.CustomData.size()));
                Writer.WriteInt32(T.Unk10);
            }
        }
        for (size_t I = 0; I < Cuts.size(); ++I) {
            for (size_t J = 0; J < Cuts[I].Timelines.size(); ++J) {
                Writer.FillVarint(Tag(I, J, "TimelineCustomDataOffset"), Writer.Position());
                for (const CustomData& C : Cuts[I].Timelines[J].CustomData) {
                    WriteCustomData(Writer, C, Context);
                }
            }
        }
        for (size_t I = 0; I < Cuts.size(); ++I) {
            for (size_t J = 0; J < Cuts[I].Timelines.size(); ++J) {
                Writer.FillVarint(Tag(I, J, "DisposOffsetsOffset"), Writer.Position());
                for (const Disposition& D : Cuts[I].Timelines[J].Dispositions) {
                    Writer.WriteVarint(Context.DisposOffsets.at(&D));
                }
            }
        }

        Writer.FillVarint("ResourcePathsOffset", Writer.Position());
        for (size_t I = 0; I < Resources.size(); ++I) {
            Writer.ReserveVarint("ResourcePathOffset" + std::to_string(I));
        }
        Writer.WriteUTF16Text(ResourceDirectory, true);
        for (size_t I = 0; I < Resources.size(); ++I) {
            if (!Resources[I].Path) {
                Writer.FillVarint("ResourcePathOffset" + std::to_string(I), 0);
            } else {
                Writer.FillVarint("ResourcePathOffset" + std::to_string(I), Writer.Position());
                Writer.WriteUTF16Text(*Resources[I].Path, true);
            }
        }
        // Strange, but it matches the files.
        if (Version >= MQBVersion::Bloodborne) {
            Writer.WriteInt16(0);
            Writer.Align(4);
        }
        for (size_t I = 0; I < Context.AllCustomData.size(); ++I) {
            WriteSequences(Writer, *Context.AllCustomData[I], I, Context.ValueOffsets[I]);
        }
        for (size_t I = 0; I < Context.AllCustomData.size(); ++I) {
            WriteSequencePoints(Writer, *Context.AllCustomData[I], I);
        }
    }
}  // namespace Souls
