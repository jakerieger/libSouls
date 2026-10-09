//
// Created by Jake Rieger on 10/8/2026.
//

#include "GPARAM.hpp"

#include <algorithm>

namespace Souls {
    namespace {
        using GPGame    = GPARAM::GPGame;
        using ParamType = GPARAM::ParamType;

        struct Offsets {
            int32_t GroupHeaders = 0, ParamHeaderOffsets = 0, ParamHeaders = 0, Values = 0, ValueIDs = 0, Unk2 = 0, Unk3 = 0,
                    Unk3ValueIDs = 0, CommentOffsetsOffsets = 0, CommentOffsets = 0, Comments = 0;
        };

        std::string Idx(const char* Name, int64_t A) {
            return std::string(Name) + std::to_string(A);
        }

        std::string Idx(const char* Name, int64_t A, int64_t B) {
            return std::string(Name) + std::to_string(A) + ":" + std::to_string(B);
        }

        void WriteNames(BinaryWriter& Writer, GPGame Game, const std::string& Name1, const std::string& Name2) {
            if (Game == GPGame::DarkSouls2) {
                Writer.WriteShiftJIS(Name1, true);
            } else {
                Writer.WriteUTF16Text(Name1, true);
                Writer.WriteUTF16Text(Name2, true);
            }
            Writer.Align(4);
        }

        GPARAM::Param ReadParam(BinaryReader& Reader, GPGame Game, const Offsets& Offs) {
            GPARAM::Param P;
            const int32_t ParamHeaderOffset = Reader.ReadInt32();
            Reader.StepIn(Offs.ParamHeaders + ParamHeaderOffset);
            const int32_t ValuesOffset   = Reader.ReadInt32();
            const int32_t ValueIDsOffset = Reader.ReadInt32();
            P.Type                       = static_cast<ParamType>(Reader.ReadByte());
            const uint8_t ValueCount     = Reader.ReadByte();
            Reader.Assert<uint8_t>(0);
            Reader.Assert<uint8_t>(0);
            if (P.Type == ParamType::Byte && ValueCount > 1) {
                throw BinaryException("Unsupported GPARAM byte param with more than one value");
            }
            if (Game == GPGame::DarkSouls2) {
                P.Name1 = Reader.ReadShiftJIS();
            } else {
                P.Name1 = Reader.ReadUTF16Text();
                P.Name2 = Reader.ReadUTF16Text();
            }

            Reader.StepIn(Offs.Values + ValuesOffset);
            for (int I = 0; I < ValueCount; ++I) {
                switch (P.Type) {
                    case ParamType::Byte: P.Values.emplace_back(Reader.ReadByte()); break;
                    case ParamType::Short: P.Values.emplace_back(Reader.ReadInt16()); break;
                    case ParamType::IntA:
                    case ParamType::IntB: P.Values.emplace_back(Reader.ReadInt32()); break;
                    case ParamType::BoolA:
                    case ParamType::BoolB: P.Values.emplace_back(Reader.ReadBool()); break;
                    case ParamType::Float: P.Values.emplace_back(Reader.ReadFloat()); break;
                    case ParamType::Float2:
                        P.Values.emplace_back(Reader.ReadVector2());
                        Reader.Assert<int32_t>(0);
                        Reader.Assert<int32_t>(0);
                        break;
                    case ParamType::Float3:
                        P.Values.emplace_back(Reader.ReadVector3());
                        Reader.Assert<int32_t>(0);
                        break;
                    case ParamType::Float4: P.Values.emplace_back(Reader.ReadVector4()); break;
                    case ParamType::Byte4: {
                        std::array<uint8_t, 4> Bytes;
                        Reader.ReadInto(std::span<uint8_t>(Bytes));
                        P.Values.emplace_back(Bytes);
                        break;
                    }
                    default: throw BinaryException("Unknown GPARAM param type");
                }
            }
            Reader.StepOut();

            Reader.StepIn(Offs.ValueIDs + ValueIDsOffset);
            if (Game == GPGame::Sekiro) {
                P.UnkFloats = std::vector<float>();
            }
            for (int I = 0; I < ValueCount; ++I) {
                P.ValueIDs.push_back(Reader.ReadInt32());
                if (Game == GPGame::Sekiro) {
                    P.UnkFloats->push_back(Reader.ReadFloat());
                }
            }
            Reader.StepOut();
            Reader.StepOut();
            return P;
        }

        void WriteValue(BinaryWriter& Writer, ParamType Type, const GPARAM::ParamValue& Value) {
            switch (Type) {
                case ParamType::Byte: Writer.WriteInt32(std::get<uint8_t>(Value)); break;
                case ParamType::Short: Writer.WriteInt16(std::get<int16_t>(Value)); break;
                case ParamType::IntA:
                case ParamType::IntB: Writer.WriteInt32(std::get<int32_t>(Value)); break;
                case ParamType::BoolA:
                case ParamType::BoolB: Writer.WriteBool(std::get<bool>(Value)); break;
                case ParamType::Float: Writer.WriteFloat(std::get<float>(Value)); break;
                case ParamType::Float2:
                    Writer.WriteVector2(std::get<Vector2>(Value));
                    Writer.WriteInt32(0);
                    Writer.WriteInt32(0);
                    break;
                case ParamType::Float3:
                    Writer.WriteVector3(std::get<Vector3>(Value));
                    Writer.WriteInt32(0);
                    break;
                case ParamType::Float4: Writer.WriteVector4(std::get<Vector4>(Value)); break;
                case ParamType::Byte4: Writer.WriteBytes(std::get<std::array<uint8_t, 4>>(Value)); break;
                default: throw BinaryException("Unknown GPARAM param type");
            }
        }
    }  // namespace

    GPARAM::Param* GPARAM::Group::FindParam(const std::string& Name) {
        const auto It = std::find_if(Params.begin(), Params.end(), [&](const Param& P) { return P.Name1 == Name; });
        return It == Params.end() ? nullptr : &*It;
    }

    GPARAM::Group* GPARAM::FindGroup(const std::string& Name1) {
        const auto It = std::find_if(Groups.begin(), Groups.end(), [&](const Group& G) { return G.Name1 == Name1; });
        return It == Groups.end() ? nullptr : &*It;
    }

    bool GPARAM::IsImpl(BinaryReader& Reader) {
        if (Reader.Length() < 4) {
            return false;
        }
        const std::string Magic = Reader.GetASCII(0, 4);
        return Magic == "filt" || Magic == std::string("f\0i\0", 4);
    }

    void GPARAM::ReadImpl(BinaryReader& Reader) {
        Reader.Order = Endian::Little;
        const std::string Magic = Reader.ReadString(4);
        if (Magic == std::string("f\0i\0", 4)) {
            Reader.AssertMagic(std::string_view("l\0t\0", 4));
        } else if (Magic != "filt") {
            throw BinaryException("Not a GPARAM: bad magic");
        }
        Game  = static_cast<GPGame>(Reader.ReadUInt32());
        Reader.Assert<uint8_t>(0);
        Unk0D = Reader.ReadBool();
        Reader.Assert<int16_t>(0);
        const int32_t GroupCount = Reader.ReadInt32();
        Unk14                    = Reader.ReadInt32();
        Reader.Assert<int32_t>(0x40, 0x50, 0x54);  // header size or group header headers offset
        Offsets Offs;
        Offs.GroupHeaders       = Reader.ReadInt32();
        Offs.ParamHeaderOffsets = Reader.ReadInt32();
        Offs.ParamHeaders       = Reader.ReadInt32();
        Offs.Values             = Reader.ReadInt32();
        Offs.ValueIDs           = Reader.ReadInt32();
        Offs.Unk2               = Reader.ReadInt32();
        const int32_t Unk3Count = Reader.ReadInt32();
        Offs.Unk3               = Reader.ReadInt32();
        Offs.Unk3ValueIDs       = Reader.ReadInt32();
        Reader.Assert<int32_t>(0);
        if (Game == GPGame::DarkSouls3 || Game == GPGame::Sekiro) {
            Offs.CommentOffsetsOffsets = Reader.ReadInt32();
            Offs.CommentOffsets        = Reader.ReadInt32();
            Offs.Comments              = Reader.ReadInt32();
        }
        if (Game == GPGame::Sekiro) {
            Unk50 = Reader.ReadFloat();
        }

        Groups.clear();
        Groups.reserve(static_cast<size_t>(GroupCount));
        for (int32_t I = 0; I < GroupCount; ++I) {
            Group G;
            const int32_t GroupHeaderOffset = Reader.ReadInt32();
            Reader.StepIn(Offs.GroupHeaders + GroupHeaderOffset);
            const int32_t ParamCount              = Reader.ReadInt32();
            const int32_t ParamHeaderOffsetsOffset = Reader.ReadInt32();
            if (Game == GPGame::DarkSouls2) {
                G.Name1 = Reader.ReadShiftJIS();
            } else {
                G.Name1 = Reader.ReadUTF16Text();
                G.Name2 = Reader.ReadUTF16Text();
            }
            Reader.StepIn(Offs.ParamHeaderOffsets + ParamHeaderOffsetsOffset);
            for (int32_t P = 0; P < ParamCount; ++P) {
                G.Params.push_back(ReadParam(Reader, Game, Offs));
            }
            Reader.StepOut();
            Reader.StepOut();
            Groups.push_back(std::move(G));
        }

        Reader.StepIn(Offs.Unk2);
        UnkBlock2 = Reader.ReadBytes(static_cast<size_t>(Offs.Unk3 - Offs.Unk2));
        Reader.StepOut();

        Reader.Seek(Offs.Unk3);
        Unk3s.clear();
        for (int32_t I = 0; I < Unk3Count; ++I) {
            Unk3 U;
            U.GroupIndex             = Reader.ReadInt32();
            const int32_t Count      = Reader.ReadInt32();
            const uint32_t IDsOffset = Reader.ReadUInt32();
            if (Game == GPGame::Sekiro) {
                U.Unk0C = Reader.ReadInt32();
            }
            Reader.StepIn(static_cast<int64_t>(Offs.Unk3ValueIDs) + IDsOffset);
            U.ValueIDs = Reader.ReadArray<int32_t>(static_cast<size_t>(Count));
            Reader.StepOut();
            Unk3s.push_back(std::move(U));
        }

        if (Game == GPGame::DarkSouls3 || Game == GPGame::Sekiro) {
            Reader.StepIn(Offs.CommentOffsetsOffsets);
            const std::vector<int32_t> CommentOffsetsOffsets = Reader.ReadArray<int32_t>(static_cast<size_t>(GroupCount));
            Reader.StepOut();
            const int32_t CommentOffsetsLength = Offs.Comments - Offs.CommentOffsets;
            for (int32_t I = 0; I < GroupCount; ++I) {
                int32_t CommentCount;
                if (I == GroupCount - 1) {
                    CommentCount = (CommentOffsetsLength - CommentOffsetsOffsets[static_cast<size_t>(I)]) / 4;
                } else {
                    CommentCount = (CommentOffsetsOffsets[static_cast<size_t>(I) + 1] - CommentOffsetsOffsets[static_cast<size_t>(I)]) / 4;
                }
                Reader.Seek(Offs.CommentOffsets + CommentOffsetsOffsets[static_cast<size_t>(I)]);
                for (int32_t J = 0; J < CommentCount; ++J) {
                    const int32_t CommentOffset = Reader.ReadInt32();
                    Groups[static_cast<size_t>(I)].Comments.push_back(Reader.GetUTF16Text(Offs.Comments + CommentOffset));
                }
            }
        }
    }

    void GPARAM::WriteImpl(BinaryWriter& Writer) {
        Writer.Order = Endian::Little;
        if (Game == GPGame::DarkSouls2) {
            Writer.WriteMagic("filt");
        } else {
            Writer.WriteUTF16Text("filt", false);
        }
        Writer.WriteUInt32(static_cast<uint32_t>(Game));
        Writer.WriteByte(0);
        Writer.WriteBool(Unk0D);
        Writer.WriteInt16(0);
        Writer.WriteInt32(static_cast<int32_t>(Groups.size()));
        Writer.WriteInt32(Unk14);
        Writer.Reserve<int32_t>("HeaderSize");
        Writer.Reserve<int32_t>("GroupHeadersOffset");
        Writer.Reserve<int32_t>("ParamHeaderOffsetsOffset");
        Writer.Reserve<int32_t>("ParamHeadersOffset");
        Writer.Reserve<int32_t>("ValuesOffset");
        Writer.Reserve<int32_t>("ValueIDsOffset");
        Writer.Reserve<int32_t>("UnkOffset2");
        Writer.WriteInt32(static_cast<int32_t>(Unk3s.size()));
        Writer.Reserve<int32_t>("UnkOffset3");
        Writer.Reserve<int32_t>("Unk3ValuesOffset");
        Writer.WriteInt32(0);
        if (Game == GPGame::DarkSouls3 || Game == GPGame::Sekiro) {
            Writer.Reserve<int32_t>("CommentOffsetsOffsetsOffset");
            Writer.Reserve<int32_t>("CommentOffsetsOffset");
            Writer.Reserve<int32_t>("CommentsOffset");
        }
        if (Game == GPGame::Sekiro) {
            Writer.WriteFloat(Unk50);
        }
        Writer.Fill<int32_t>("HeaderSize", static_cast<int32_t>(Writer.Position()));

        for (size_t I = 0; I < Groups.size(); ++I) {
            Writer.Reserve<int32_t>(Idx("GroupHeaderOffset", static_cast<int64_t>(I)));
        }

        const int32_t GroupHeadersOffset = static_cast<int32_t>(Writer.Position());
        Writer.Fill<int32_t>("GroupHeadersOffset", GroupHeadersOffset);
        for (size_t I = 0; I < Groups.size(); ++I) {
            const Group& G = Groups[I];
            Writer.Fill<int32_t>(Idx("GroupHeaderOffset", static_cast<int64_t>(I)), static_cast<int32_t>(Writer.Position()) - GroupHeadersOffset);
            Writer.WriteInt32(static_cast<int32_t>(G.Params.size()));
            Writer.Reserve<int32_t>(Idx("ParamHeaderOffsetsOffset", static_cast<int64_t>(I)));
            WriteNames(Writer, Game, G.Name1, G.Name2);
        }

        const int32_t ParamHeaderOffsetsOffset = static_cast<int32_t>(Writer.Position());
        Writer.Fill<int32_t>("ParamHeaderOffsetsOffset", ParamHeaderOffsetsOffset);
        for (size_t I = 0; I < Groups.size(); ++I) {
            Writer.Fill<int32_t>(Idx("ParamHeaderOffsetsOffset", static_cast<int64_t>(I)), static_cast<int32_t>(Writer.Position()) - ParamHeaderOffsetsOffset);
            for (size_t P = 0; P < Groups[I].Params.size(); ++P) {
                Writer.Reserve<int32_t>(Idx("ParamHeaderOffset", static_cast<int64_t>(I), static_cast<int64_t>(P)));
            }
        }

        const int32_t ParamHeadersOffset = static_cast<int32_t>(Writer.Position());
        Writer.Fill<int32_t>("ParamHeadersOffset", ParamHeadersOffset);
        for (size_t I = 0; I < Groups.size(); ++I) {
            for (size_t P = 0; P < Groups[I].Params.size(); ++P) {
                const Param& Pr = Groups[I].Params[P];
                Writer.Fill<int32_t>(Idx("ParamHeaderOffset", static_cast<int64_t>(I), static_cast<int64_t>(P)), static_cast<int32_t>(Writer.Position()) - ParamHeadersOffset);
                Writer.Reserve<int32_t>(Idx("ValuesOffset", static_cast<int64_t>(I), static_cast<int64_t>(P)));
                Writer.Reserve<int32_t>(Idx("ValueIDsOffset", static_cast<int64_t>(I), static_cast<int64_t>(P)));
                Writer.WriteByte(static_cast<uint8_t>(Pr.Type));
                Writer.WriteByte(static_cast<uint8_t>(Pr.Values.size()));
                Writer.WriteByte(0);
                Writer.WriteByte(0);
                WriteNames(Writer, Game, Pr.Name1, Pr.Name2);
            }
        }

        const int32_t ValuesOffset = static_cast<int32_t>(Writer.Position());
        Writer.Fill<int32_t>("ValuesOffset", ValuesOffset);
        for (size_t I = 0; I < Groups.size(); ++I) {
            for (size_t P = 0; P < Groups[I].Params.size(); ++P) {
                const Param& Pr = Groups[I].Params[P];
                Writer.Fill<int32_t>(Idx("ValuesOffset", static_cast<int64_t>(I), static_cast<int64_t>(P)), static_cast<int32_t>(Writer.Position()) - ValuesOffset);
                for (const ParamValue& V : Pr.Values) {
                    WriteValue(Writer, Pr.Type, V);
                }
                Writer.Align(4);
            }
        }

        const int32_t ValueIDsOffset = static_cast<int32_t>(Writer.Position());
        Writer.Fill<int32_t>("ValueIDsOffset", ValueIDsOffset);
        for (size_t I = 0; I < Groups.size(); ++I) {
            for (size_t P = 0; P < Groups[I].Params.size(); ++P) {
                const Param& Pr = Groups[I].Params[P];
                Writer.Fill<int32_t>(Idx("ValueIDsOffset", static_cast<int64_t>(I), static_cast<int64_t>(P)), static_cast<int32_t>(Writer.Position()) - ValueIDsOffset);
                for (size_t V = 0; V < Pr.ValueIDs.size(); ++V) {
                    Writer.WriteInt32(Pr.ValueIDs[V]);
                    if (Game == GPGame::Sekiro) {
                        Writer.WriteFloat(Pr.UnkFloats.value().at(V));
                    }
                }
            }
        }

        Writer.Fill<int32_t>("UnkOffset2", static_cast<int32_t>(Writer.Position()));
        Writer.WriteBytes(UnkBlock2);
        Writer.Fill<int32_t>("UnkOffset3", static_cast<int32_t>(Writer.Position()));
        for (size_t I = 0; I < Unk3s.size(); ++I) {
            Writer.WriteInt32(Unk3s[I].GroupIndex);
            Writer.WriteInt32(static_cast<int32_t>(Unk3s[I].ValueIDs.size()));
            Writer.Reserve<int32_t>(Idx("Unk3ValueIDsOffset", static_cast<int64_t>(I)));
            if (Game == GPGame::Sekiro) {
                Writer.WriteInt32(Unk3s[I].Unk0C);
            }
        }
        const int32_t Unk3ValuesOffset = static_cast<int32_t>(Writer.Position());
        Writer.Fill<int32_t>("Unk3ValuesOffset", Unk3ValuesOffset);
        for (size_t I = 0; I < Unk3s.size(); ++I) {
            const std::string Name = Idx("Unk3ValueIDsOffset", static_cast<int64_t>(I));
            if (Unk3s[I].ValueIDs.empty()) {
                Writer.Fill<int32_t>(Name, 0);
            } else {
                Writer.Fill<int32_t>(Name, static_cast<int32_t>(Writer.Position()) - Unk3ValuesOffset);
                Writer.WriteArray(Unk3s[I].ValueIDs);
            }
        }

        if (Game == GPGame::DarkSouls3 || Game == GPGame::Sekiro) {
            Writer.Fill<int32_t>("CommentOffsetsOffsetsOffset", static_cast<int32_t>(Writer.Position()));
            for (size_t I = 0; I < Groups.size(); ++I) {
                Writer.Reserve<int32_t>(Idx("CommentOffsetsOffset", static_cast<int64_t>(I)));
            }
            const int32_t CommentOffsetsOffset = static_cast<int32_t>(Writer.Position());
            Writer.Fill<int32_t>("CommentOffsetsOffset", CommentOffsetsOffset);
            for (size_t I = 0; I < Groups.size(); ++I) {
                Writer.Fill<int32_t>(Idx("CommentOffsetsOffset", static_cast<int64_t>(I)), static_cast<int32_t>(Writer.Position()) - CommentOffsetsOffset);
                for (size_t C = 0; C < Groups[I].Comments.size(); ++C) {
                    Writer.Reserve<int32_t>(Idx("CommentOffset", static_cast<int64_t>(I), static_cast<int64_t>(C)));
                }
            }
            const int32_t CommentsOffset = static_cast<int32_t>(Writer.Position());
            Writer.Fill<int32_t>("CommentsOffset", CommentsOffset);
            for (size_t I = 0; I < Groups.size(); ++I) {
                for (size_t C = 0; C < Groups[I].Comments.size(); ++C) {
                    Writer.Fill<int32_t>(Idx("CommentOffset", static_cast<int64_t>(I), static_cast<int64_t>(C)), static_cast<int32_t>(Writer.Position()) - CommentsOffset);
                    Writer.WriteUTF16Text(Groups[I].Comments[C], true);
                    Writer.Align(4);
                }
            }
        }
    }
}  // namespace Souls
