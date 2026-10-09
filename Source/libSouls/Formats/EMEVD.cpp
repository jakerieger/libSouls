//
// Created by Jake Rieger on 10/8/2026.
//

#include "EMEVD.hpp"

#include "EMELD.hpp"

#include <algorithm>
#include <map>
#include <unordered_map>

namespace Souls {
    namespace {
        struct Offsets {
            int64_t Events = 0, Instructions = 0, Layers = 0, Parameters = 0, LinkedFiles = 0, Arguments = 0, Strings = 0;
        };

        using Game = EMEVD::Game;

        uint32_t ReadLayer(BinaryReader& Reader) {
            Reader.Assert<int32_t>(2);
            const uint32_t Layer = Reader.ReadUInt32();
            Reader.AssertVarint(0);
            Reader.AssertVarint(-1);
            Reader.AssertVarint(1);
            return Layer;
        }

        void WriteLayer(BinaryWriter& Writer, uint32_t Layer) {
            Writer.WriteInt32(2);
            Writer.WriteUInt32(Layer);
            Writer.WriteVarint(0);
            Writer.WriteVarint(-1);
            Writer.WriteVarint(1);
        }

        EMEVD::Instruction ReadInstruction(BinaryReader& Reader, Game Format, const Offsets& Offs) {
            EMEVD::Instruction I;
            I.Bank                   = Reader.ReadInt32();
            I.ID                     = Reader.ReadInt32();
            const int64_t ArgsLength = Reader.ReadVarint();
            const int64_t ArgsOffset = Reader.ReadVarint();
            int64_t LayerOffset;
            if (Format < Game::DarkSouls3) {
                LayerOffset = Reader.ReadInt32();
                Reader.Assert<int32_t>(0);
            } else {
                LayerOffset = Reader.ReadInt64();
            }

            if (ArgsLength > 0) {
                Reader.StepIn(Offs.Arguments + ArgsOffset);
                I.ArgData = Reader.ReadBytes(static_cast<size_t>(ArgsLength));
                Reader.StepOut();
            }
            if (LayerOffset != -1) {
                Reader.StepIn(Offs.Layers + LayerOffset);
                I.Layer = ReadLayer(Reader);
                Reader.StepOut();
            }
            return I;
        }

        EMEVD::Event ReadEvent(BinaryReader& Reader, Game Format, const Offsets& Offs) {
            EMEVD::Event E;
            E.ID                          = Reader.ReadVarint();
            const int64_t InstrCount      = Reader.ReadVarint();
            const int64_t InstrOffset     = Reader.ReadVarint();
            const int64_t ParameterCount  = Reader.ReadVarint();
            const int64_t ParameterOffset = Reader.ReadVarint();
            E.RestBehavior                = static_cast<EMEVD::Event::RestBehaviorType>(Reader.ReadUInt32());
            Reader.Assert<int32_t>(0);

            if (InstrCount > 0) {
                Reader.StepIn(Offs.Instructions + InstrOffset);
                for (int64_t I = 0; I < InstrCount; ++I) {
                    E.Instructions.push_back(ReadInstruction(Reader, Format, Offs));
                }
                Reader.StepOut();
            }
            if (ParameterCount > 0) {
                Reader.StepIn(Offs.Parameters + ParameterOffset);
                for (int64_t I = 0; I < ParameterCount; ++I) {
                    EMEVD::Parameter P;
                    P.InstructionIndex = Reader.ReadVarint();
                    P.TargetStartByte  = Reader.ReadVarint();
                    P.SourceStartByte  = Reader.ReadVarint();
                    P.ByteCount        = Reader.ReadInt32();
                    P.UnkID            = Reader.ReadInt32();
                    E.Parameters.push_back(P);
                }
                Reader.StepOut();
            }
            return E;
        }

        std::string Key(size_t Event, const char* What) {
            return "Event" + std::to_string(Event) + What;
        }

        std::string Key(size_t Event, size_t Instr, const char* What) {
            return "Event" + std::to_string(Event) + "Instr" + std::to_string(Instr) + What;
        }
    }  // namespace

    void EMEVD::Instruction::PackArgs(const std::vector<ArgValue>& Args, bool BigEndian) {
        BinaryWriter Writer(BigEndian ? Endian::Big : Endian::Little);
        for (const ArgValue& Arg : Args) {
            std::visit(
                [&](auto Value) {
                    using T = decltype(Value);
                    if constexpr (sizeof(T) > 1) {
                        Writer.Align(sizeof(T));
                    }
                    Writer.Write<T>(Value);
                },
                Arg);
        }
        Writer.Align(4);
        ArgData = Writer.ToBytes();
    }

    std::vector<EMEVD::ArgValue> EMEVD::Instruction::UnpackArgs(const std::vector<ArgType>& ArgStruct, bool BigEndian) const {
        BinaryReader Reader(std::span<const uint8_t>(ArgData), BigEndian ? Endian::Big : Endian::Little);
        std::vector<ArgValue> Result;
        for (const ArgType Arg : ArgStruct) {
            switch (Arg) {
                case ArgType::Byte: Result.emplace_back(Reader.ReadByte()); break;
                case ArgType::UInt16: Reader.Align(2); Result.emplace_back(Reader.ReadUInt16()); break;
                case ArgType::UInt32: Reader.Align(4); Result.emplace_back(Reader.ReadUInt32()); break;
                case ArgType::SByte: Result.emplace_back(Reader.ReadSByte()); break;
                case ArgType::Int16: Reader.Align(2); Result.emplace_back(Reader.ReadInt16()); break;
                case ArgType::Int32: Reader.Align(4); Result.emplace_back(Reader.ReadInt32()); break;
                case ArgType::Single: Reader.Align(4); Result.emplace_back(Reader.ReadFloat()); break;
                default: throw BinaryException("Unimplemented argument type");
            }
        }
        return Result;
    }

    void EMEVD::ImportEMELD(const EMELD& Eld, bool Overwrite) {
        std::unordered_map<int64_t, std::string> Names;
        for (const EMELD::Event& E : Eld.Events) {
            Names[E.ID] = E.Name;
        }
        for (Event& E : Events) {
            const auto Found = Names.find(E.ID);
            if ((Overwrite || !E.Name) && Found != Names.end()) {
                E.Name = Found->second;
            }
        }
    }

    EMELD EMEVD::ExportEMELD() const {
        EMELD Eld(Format);
        for (const Event& E : Events) {
            if (E.Name) {
                Eld.Events.push_back({E.ID, *E.Name});
            }
        }
        return Eld;
    }

    bool EMEVD::IsImpl(BinaryReader& Reader) {
        if (Reader.Length() < 4) {
            return false;
        }
        const std::string Magic = Reader.GetASCII(0, 4);
        return Magic == std::string("EVD\0", 4);
    }

    void EMEVD::ReadImpl(BinaryReader& Reader) {
        Reader.AssertMagic(std::string_view("EVD\0", 4));
        const bool BigEndian = Reader.ReadBool();
        const bool Is64Bit   = Reader.Assert<int8_t>(0, -1) == -1;
        const bool Unk06     = Reader.ReadBool();
        const bool Unk07     = Reader.Assert<int8_t>(0, -1) == -1;
        Reader.Order         = BigEndian ? Endian::Big : Endian::Little;
        Reader.VarintLong    = Is64Bit;
        const int32_t Version = Reader.Assert<int32_t>(0xCC, 0xCD);
        Reader.ReadInt32();  // file size

        if (!BigEndian && !Is64Bit && !Unk06 && !Unk07 && Version == 0xCC) {
            Format = Game::DarkSouls1;
        } else if (BigEndian && !Is64Bit && !Unk06 && !Unk07 && Version == 0xCC) {
            Format = Game::DarkSouls1BE;
        } else if (!BigEndian && Is64Bit && !Unk06 && !Unk07 && Version == 0xCC) {
            Format = Game::Bloodborne;
        } else if (!BigEndian && Is64Bit && Unk06 && !Unk07 && Version == 0xCD) {
            Format = Game::DarkSouls3;
        } else if (!BigEndian && Is64Bit && Unk06 && Unk07 && Version == 0xCD) {
            Format = Game::Sekiro;
        } else {
            throw BinaryException("Unknown EMEVD format");
        }

        Offsets Offs;
        const int64_t EventCount = Reader.ReadVarint();
        Offs.Events              = Reader.ReadVarint();
        Reader.ReadVarint();  // instruction count
        Offs.Instructions = Reader.ReadVarint();
        Reader.AssertVarint(0);  // unknown struct count
        Reader.ReadVarint();     // unknown struct offset
        Reader.ReadVarint();     // layer count
        Offs.Layers = Reader.ReadVarint();
        Reader.ReadVarint();  // parameter count
        Offs.Parameters           = Reader.ReadVarint();
        const int64_t LinkedCount = Reader.ReadVarint();
        Offs.LinkedFiles          = Reader.ReadVarint();
        Reader.ReadVarint();  // argument data length
        Offs.Arguments             = Reader.ReadVarint();
        const int64_t StringsLength = Reader.ReadVarint();
        Offs.Strings               = Reader.ReadVarint();
        if (!Is64Bit) {
            Reader.Assert<int32_t>(0);
        }

        Reader.Seek(Offs.Events);
        Events.clear();
        Events.reserve(static_cast<size_t>(EventCount));
        for (int64_t I = 0; I < EventCount; ++I) {
            Events.push_back(ReadEvent(Reader, Format, Offs));
        }

        Reader.Seek(Offs.LinkedFiles);
        LinkedFileOffsets.clear();
        for (int64_t I = 0; I < LinkedCount; ++I) {
            LinkedFileOffsets.push_back(Reader.ReadVarint());
        }

        Reader.Seek(Offs.Strings);
        StringData = Reader.ReadBytes(static_cast<size_t>(StringsLength));
    }

    void EMEVD::WriteImpl(BinaryWriter& Writer) {
        const bool BigEndian = Format == Game::DarkSouls1BE;
        const bool Is64Bit   = Format >= Game::Bloodborne;
        const bool Unk06     = Format >= Game::DarkSouls3;
        const bool Unk07     = Format >= Game::Sekiro;
        const int32_t Version = Format < Game::DarkSouls3 ? 0xCC : 0xCD;

        std::vector<uint32_t> Layers;
        int64_t InstructionTotal = 0, ParameterTotal = 0;
        for (const Event& E : Events) {
            InstructionTotal += static_cast<int64_t>(E.Instructions.size());
            ParameterTotal += static_cast<int64_t>(E.Parameters.size());
            for (const Instruction& I : E.Instructions) {
                if (I.Layer && std::find(Layers.begin(), Layers.end(), *I.Layer) == Layers.end()) {
                    Layers.push_back(*I.Layer);
                }
            }
        }

        Writer.WriteMagic(std::string_view("EVD\0", 4));
        Writer.WriteBool(BigEndian);
        Writer.WriteSByte(static_cast<int8_t>(Is64Bit ? -1 : 0));
        Writer.WriteBool(Unk06);
        Writer.WriteSByte(static_cast<int8_t>(Unk07 ? -1 : 0));
        Writer.Order      = BigEndian ? Endian::Big : Endian::Little;
        Writer.VarintLong = Is64Bit;
        Writer.WriteInt32(Version);
        Writer.Reserve<int32_t>("FileSize");

        Offsets Offs;
        Writer.WriteVarint(static_cast<int64_t>(Events.size()));
        Writer.ReserveVarint("EventsOffset");
        Writer.WriteVarint(InstructionTotal);
        Writer.ReserveVarint("InstructionsOffset");
        Writer.WriteVarint(0);
        Writer.ReserveVarint("Offset3");
        Writer.WriteVarint(static_cast<int64_t>(Layers.size()));
        Writer.ReserveVarint("LayersOffset");
        Writer.WriteVarint(ParameterTotal);
        Writer.ReserveVarint("ParametersOffset");
        Writer.WriteVarint(static_cast<int64_t>(LinkedFileOffsets.size()));
        Writer.ReserveVarint("LinkedFilesOffset");
        Writer.ReserveVarint("ArgumentsLength");
        Writer.ReserveVarint("ArgumentsOffset");
        Writer.WriteVarint(static_cast<int64_t>(StringData.size()));
        Writer.ReserveVarint("StringsOffset");
        if (!Is64Bit) {
            Writer.WriteInt32(0);
        }

        Offs.Events = Writer.Position();
        Writer.FillVarint("EventsOffset", Writer.Position());
        for (size_t I = 0; I < Events.size(); ++I) {
            const Event& E = Events[I];
            Writer.WriteVarint(E.ID);
            Writer.WriteVarint(static_cast<int64_t>(E.Instructions.size()));
            Writer.ReserveVarint(Key(I, "InstrsOffset"));
            Writer.WriteVarint(static_cast<int64_t>(E.Parameters.size()));
            if (Format < Game::Bloodborne) {
                Writer.Reserve<int32_t>(Key(I, "ParamsOffset"));
            } else if (Format < Game::DarkSouls3) {
                Writer.Reserve<int32_t>(Key(I, "ParamsOffset"));
                Writer.WriteInt32(0);
            } else {
                Writer.Reserve<int64_t>(Key(I, "ParamsOffset"));
            }
            Writer.WriteUInt32(static_cast<uint32_t>(E.RestBehavior));
            Writer.WriteInt32(0);
        }

        Offs.Instructions = Writer.Position();
        Writer.FillVarint("InstructionsOffset", Writer.Position());
        for (size_t I = 0; I < Events.size(); ++I) {
            const Event& E = Events[I];
            Writer.FillVarint(Key(I, "InstrsOffset"), E.Instructions.empty() ? -1 : Writer.Position() - Offs.Instructions);
            for (size_t J = 0; J < E.Instructions.size(); ++J) {
                const Instruction& In = E.Instructions[J];
                Writer.WriteInt32(In.Bank);
                Writer.WriteInt32(In.ID);
                Writer.WriteVarint(static_cast<int64_t>(In.ArgData.size()));
                if (Format < Game::Bloodborne) {
                    Writer.Reserve<int32_t>(Key(I, J, "ArgsOffset"));
                } else if (Format < Game::Sekiro) {
                    Writer.Reserve<int32_t>(Key(I, J, "ArgsOffset"));
                    Writer.WriteInt32(0);
                } else {
                    Writer.Reserve<int64_t>(Key(I, J, "ArgsOffset"));
                }
                if (Format < Game::DarkSouls3) {
                    Writer.Reserve<int32_t>(Key(I, J, "LayerOffset"));
                    Writer.WriteInt32(0);
                } else {
                    Writer.Reserve<int64_t>(Key(I, J, "LayerOffset"));
                }
            }
        }

        Writer.FillVarint("Offset3", Writer.Position());
        Offs.Layers = Writer.Position();
        Writer.FillVarint("LayersOffset", Writer.Position());
        std::map<uint32_t, int64_t> LayerOffsets;
        for (const uint32_t Layer : Layers) {
            LayerOffsets[Layer] = Writer.Position() - Offs.Layers;
            WriteLayer(Writer, Layer);
        }
        for (size_t I = 0; I < Events.size(); ++I) {
            for (size_t J = 0; J < Events[I].Instructions.size(); ++J) {
                const Instruction& In = Events[I].Instructions[J];
                const int64_t LayerOffset = In.Layer ? LayerOffsets.at(*In.Layer) : -1;
                if (Format < Game::DarkSouls3) {
                    Writer.Fill<int32_t>(Key(I, J, "LayerOffset"), static_cast<int32_t>(LayerOffset));
                } else {
                    Writer.Fill<int64_t>(Key(I, J, "LayerOffset"), LayerOffset);
                }
            }
        }

        Offs.Arguments = Writer.Position();
        Writer.FillVarint("ArgumentsOffset", Writer.Position());
        for (size_t I = 0; I < Events.size(); ++I) {
            for (size_t J = 0; J < Events[I].Instructions.size(); ++J) {
                const Instruction& In = Events[I].Instructions[J];
                const int64_t ArgsOffset = In.ArgData.empty() ? -1 : Writer.Position() - Offs.Arguments;
                if (Format < Game::Sekiro) {
                    Writer.Fill<int32_t>(Key(I, J, "ArgsOffset"), static_cast<int32_t>(ArgsOffset));
                } else {
                    Writer.Fill<int64_t>(Key(I, J, "ArgsOffset"), ArgsOffset);
                }
                Writer.WriteBytes(In.ArgData);
                Writer.Align(4);
            }
        }
        if ((Writer.Position() - Offs.Arguments) % 0x10 > 0) {
            Writer.Pad(static_cast<size_t>(0x10 - (Writer.Position() - Offs.Arguments) % 0x10));
        }
        Writer.FillVarint("ArgumentsLength", Writer.Position() - Offs.Arguments);

        Offs.Parameters = Writer.Position();
        Writer.FillVarint("ParametersOffset", Writer.Position());
        for (size_t I = 0; I < Events.size(); ++I) {
            const Event& E = Events[I];
            const int64_t ParamsOffset = E.Parameters.empty() ? -1 : Writer.Position() - Offs.Parameters;
            if (Format < Game::DarkSouls3) {
                Writer.Fill<int32_t>(Key(I, "ParamsOffset"), static_cast<int32_t>(ParamsOffset));
            } else {
                Writer.Fill<int64_t>(Key(I, "ParamsOffset"), ParamsOffset);
            }
            for (const Parameter& P : E.Parameters) {
                Writer.WriteVarint(P.InstructionIndex);
                Writer.WriteVarint(P.TargetStartByte);
                Writer.WriteVarint(P.SourceStartByte);
                Writer.WriteInt32(P.ByteCount);
                Writer.WriteInt32(P.UnkID);
            }
        }

        Offs.LinkedFiles = Writer.Position();
        Writer.FillVarint("LinkedFilesOffset", Writer.Position());
        for (const int64_t Offset : LinkedFileOffsets) {
            Writer.WriteVarint(static_cast<int32_t>(Offset));
        }

        Offs.Strings = Writer.Position();
        Writer.FillVarint("StringsOffset", Writer.Position());
        Writer.WriteBytes(StringData);
        Writer.Fill<int32_t>("FileSize", static_cast<int32_t>(Writer.Position()));
    }
}  // namespace Souls
