// Copyright (C) 2026 Jake Rieger
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, version 3. This program is distributed in the hope that it will
// be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU General
// Public License along with this program. If not, see https://www.gnu.org/licenses/.

#include "CApiCommon.hpp"

#include <libSouls/Formats/EMEVD.hpp>

using namespace Souls;
using namespace Souls::CApi;

struct SoulsEmevd {
    EMEVD Value;
};

namespace {
    SoulsEmevd* Wrap(EMEVD Value) {
        auto Result   = std::make_unique<SoulsEmevd>();
        Result->Value = std::move(Value);
        return Result.release();
    }

    EMEVD& File(const SoulsEmevd* Handle) {
        return const_cast<SoulsEmevd&>(Require(Handle, "emevd is NULL")).Value;
    }

    EMEVD::Event& EventAt(const SoulsEmevd* Handle, size_t Index) {
        auto& Events = File(Handle).Events;
        CheckIndex(Index, Events.size(), "event index out of range");
        return Events[Index];
    }

    EMEVD::Instruction& InstructionAt(const SoulsEmevd* Handle, size_t Event, size_t Index) {
        auto& Instructions = EventAt(Handle, Event).Instructions;
        CheckIndex(Index, Instructions.size(), "instruction index out of range");
        return Instructions[Index];
    }

    EMEVD::Parameter& ParameterAt(const SoulsEmevd* Handle, size_t Event, size_t Index) {
        auto& Parameters = EventAt(Handle, Event).Parameters;
        CheckIndex(Index, Parameters.size(), "parameter index out of range");
        return Parameters[Index];
    }

    bool IsBigEndian(const SoulsEmevd* Handle) {
        return File(Handle).Format == EMEVD::Game::DarkSouls1BE;
    }

    std::vector<EMEVD::ArgType> ArgTypes(const uint8_t* Types, size_t Count) {
        if (!Types && Count) ThrowInvalid("arg_types is NULL");
        std::vector<EMEVD::ArgType> Result;
        for (size_t I = 0; I < Count; ++I) {
            if (Types[I] > SOULS_EMEVD_ARG_SINGLE) ThrowInvalid("invalid argument type");
            Result.push_back(static_cast<EMEVD::ArgType>(Types[I]));
        }
        return Result;
    }
}  // namespace

SOULS_C_API SoulsEmevd* souls_emevd_new(SoulsEmevdGame game) {
    return GuardValue<SoulsEmevd*>(nullptr, [&] {
        if (game < SOULS_EMEVD_DARK_SOULS_1 || game > SOULS_EMEVD_SEKIRO) ThrowInvalid("invalid game");
        return Wrap(EMEVD(static_cast<EMEVD::Game>(game)));
    });
}

SOULS_C_API SoulsEmevd* souls_emevd_read_file(const char* path) {
    return GuardValue<SoulsEmevd*>(nullptr, [&] { return Wrap(EMEVD::Read(PathFrom(path))); });
}

SOULS_C_API SoulsEmevd* souls_emevd_read_memory(const uint8_t* data, size_t size) {
    return GuardValue<SoulsEmevd*>(nullptr, [&] { return Wrap(EMEVD::Read(Bytes(data, size))); });
}

SOULS_C_API void souls_emevd_free(SoulsEmevd* emevd) {
    delete emevd;
}

SOULS_C_API SoulsStatus souls_emevd_write_file(SoulsEmevd* emevd, const char* path) {
    return Guard([&] { File(emevd).Write(PathFrom(path)); });
}

SOULS_C_API SoulsBuffer souls_emevd_write_memory(SoulsEmevd* emevd) {
    return GuardValue(SoulsBuffer {nullptr, 0}, [&] { return MakeBuffer(File(emevd).Write()); });
}

SOULS_C_API SoulsEmevdGame souls_emevd_game(const SoulsEmevd* emevd) {
    return GuardValue(SOULS_EMEVD_DARK_SOULS_1, [&] { return static_cast<SoulsEmevdGame>(File(emevd).Format); });
}

#pragma region Events
SOULS_C_API size_t souls_emevd_event_count(const SoulsEmevd* emevd) {
    return GuardValue<size_t>(0, [&] { return File(emevd).Events.size(); });
}

SOULS_C_API int64_t souls_emevd_event_id(const SoulsEmevd* emevd, size_t event) {
    return GuardValue<int64_t>(-1, [&] { return EventAt(emevd, event).ID; });
}

SOULS_C_API SoulsStatus souls_emevd_set_event_id(SoulsEmevd* emevd, size_t event, int64_t id) {
    return Guard([&] { EventAt(emevd, event).ID = id; });
}

SOULS_C_API int64_t souls_emevd_find_event(const SoulsEmevd* emevd, int64_t id) {
    return GuardValue<int64_t>(-1, [&]() -> int64_t {
        const auto& Events = File(emevd).Events;
        for (size_t I = 0; I < Events.size(); ++I) {
            if (Events[I].ID == id) return static_cast<int64_t>(I);
        }
        return -1;
    });
}

SOULS_C_API uint32_t souls_emevd_event_rest_behavior(const SoulsEmevd* emevd, size_t event) {
    return GuardValue<uint32_t>(0, [&] { return static_cast<uint32_t>(EventAt(emevd, event).RestBehavior); });
}

SOULS_C_API SoulsStatus souls_emevd_set_event_rest_behavior(SoulsEmevd* emevd, size_t event, uint32_t behavior) {
    return Guard([&] {
        if (behavior > 2) ThrowInvalid("invalid rest behavior");
        EventAt(emevd, event).RestBehavior = static_cast<EMEVD::Event::RestBehaviorType>(behavior);
    });
}

SOULS_C_API const char* souls_emevd_event_name(const SoulsEmevd* emevd, size_t event) {
    return GuardValue<const char*>(nullptr, [&]() -> const char* {
        const auto& Name = EventAt(emevd, event).Name;
        return Name ? Name->c_str() : nullptr;
    });
}

SOULS_C_API SoulsStatus souls_emevd_set_event_name(SoulsEmevd* emevd, size_t event, const char* name_or_null) {
    return Guard([&] {
        auto& Name = EventAt(emevd, event).Name;
        if (name_or_null) Name = std::string(name_or_null);
        else Name.reset();
    });
}

SOULS_C_API int64_t souls_emevd_add_event(SoulsEmevd* emevd, int64_t id) {
    return GuardValue<int64_t>(-1, [&]() -> int64_t {
        auto& Events = File(emevd).Events;
        Events.emplace_back(id);
        return static_cast<int64_t>(Events.size() - 1);
    });
}

SOULS_C_API SoulsStatus souls_emevd_remove_event(SoulsEmevd* emevd, size_t event) {
    return Guard([&] {
        auto& Events = File(emevd).Events;
        CheckIndex(event, Events.size(), "event index out of range");
        Events.erase(Events.begin() + static_cast<std::ptrdiff_t>(event));
    });
}
#pragma endregion

#pragma region Instructions
SOULS_C_API size_t souls_emevd_instruction_count(const SoulsEmevd* emevd, size_t event) {
    return GuardValue<size_t>(0, [&] { return EventAt(emevd, event).Instructions.size(); });
}

SOULS_C_API int32_t souls_emevd_instruction_bank(const SoulsEmevd* emevd, size_t event, size_t instruction) {
    return GuardValue<int32_t>(-1, [&] { return InstructionAt(emevd, event, instruction).Bank; });
}

SOULS_C_API int32_t souls_emevd_instruction_id(const SoulsEmevd* emevd, size_t event, size_t instruction) {
    return GuardValue<int32_t>(-1, [&] { return InstructionAt(emevd, event, instruction).ID; });
}

SOULS_C_API const uint8_t*
souls_emevd_instruction_args(const SoulsEmevd* emevd, size_t event, size_t instruction, size_t* out_size) {
    return GuardValue<const uint8_t*>(nullptr, [&] {
        const auto& Args = InstructionAt(emevd, event, instruction).ArgData;
        if (out_size) *out_size = Args.size();
        return Args.data();
    });
}

SOULS_C_API SoulsStatus souls_emevd_set_instruction_args(
  SoulsEmevd* emevd, size_t event, size_t instruction, const uint8_t* data, size_t size) {
    return Guard([&] {
        const auto Span = Bytes(data, size);
        InstructionAt(emevd, event, instruction).ArgData.assign(Span.begin(), Span.end());
    });
}

SOULS_C_API SoulsStatus
souls_emevd_set_instruction(SoulsEmevd* emevd, size_t event, size_t instruction, int32_t bank, int32_t id) {
    return Guard([&] {
        auto& I = InstructionAt(emevd, event, instruction);
        I.Bank  = bank;
        I.ID    = id;
    });
}

SOULS_C_API int
souls_emevd_instruction_layer(const SoulsEmevd* emevd, size_t event, size_t instruction, uint32_t* out_layer) {
    return GuardValue(0, [&] {
        const auto& Layer = InstructionAt(emevd, event, instruction).Layer;
        if (Layer && out_layer) *out_layer = *Layer;
        return Layer ? 1 : 0;
    });
}

SOULS_C_API SoulsStatus
souls_emevd_set_instruction_layer(SoulsEmevd* emevd, size_t event, size_t instruction, int has_layer, uint32_t layer) {
    return Guard([&] {
        auto& Value = InstructionAt(emevd, event, instruction).Layer;
        if (has_layer) Value = layer;
        else Value.reset();
    });
}

SOULS_C_API int64_t souls_emevd_add_instruction(
  SoulsEmevd* emevd, size_t event, int32_t bank, int32_t id, const uint8_t* args, size_t args_size) {
    return GuardValue<int64_t>(-1, [&]() -> int64_t {
        const auto Span    = Bytes(args, args_size);
        auto& Instructions = EventAt(emevd, event).Instructions;
        Instructions.emplace_back(bank, id, std::vector<uint8_t>(Span.begin(), Span.end()));
        return static_cast<int64_t>(Instructions.size() - 1);
    });
}

SOULS_C_API SoulsStatus souls_emevd_remove_instruction(SoulsEmevd* emevd, size_t event, size_t instruction) {
    return Guard([&] {
        auto& Instructions = EventAt(emevd, event).Instructions;
        CheckIndex(instruction, Instructions.size(), "instruction index out of range");
        Instructions.erase(Instructions.begin() + static_cast<std::ptrdiff_t>(instruction));
    });
}

SOULS_C_API SoulsStatus souls_emevd_unpack_args(const SoulsEmevd* emevd,
                                                size_t event,
                                                size_t instruction,
                                                const uint8_t* arg_types,
                                                size_t arg_count,
                                                double* out_values) {
    return Guard([&] {
        if (!out_values && arg_count) ThrowInvalid("out_values is NULL");
        const auto Types  = ArgTypes(arg_types, arg_count);
        const auto Values = InstructionAt(emevd, event, instruction).UnpackArgs(Types, IsBigEndian(emevd));
        for (size_t I = 0; I < Values.size(); ++I) {
            out_values[I] = std::visit([](auto V) { return static_cast<double>(V); }, Values[I]);
        }
    });
}

SOULS_C_API SoulsStatus souls_emevd_pack_args(SoulsEmevd* emevd,
                                              size_t event,
                                              size_t instruction,
                                              const uint8_t* arg_types,
                                              size_t arg_count,
                                              const double* values) {
    return Guard([&] {
        if (!values && arg_count) ThrowInvalid("values is NULL");
        const auto Types = ArgTypes(arg_types, arg_count);
        std::vector<EMEVD::ArgValue> Packed;
        for (size_t I = 0; I < arg_count; ++I) {
            const double V = values[I];
            switch (Types[I]) {
                case EMEVD::ArgType::Byte:
                    Packed.emplace_back(static_cast<uint8_t>(V));
                    break;
                case EMEVD::ArgType::UInt16:
                    Packed.emplace_back(static_cast<uint16_t>(V));
                    break;
                case EMEVD::ArgType::UInt32:
                    Packed.emplace_back(static_cast<uint32_t>(V));
                    break;
                case EMEVD::ArgType::SByte:
                    Packed.emplace_back(static_cast<int8_t>(V));
                    break;
                case EMEVD::ArgType::Int16:
                    Packed.emplace_back(static_cast<int16_t>(V));
                    break;
                case EMEVD::ArgType::Int32:
                    Packed.emplace_back(static_cast<int32_t>(V));
                    break;
                case EMEVD::ArgType::Single:
                    Packed.emplace_back(static_cast<float>(V));
                    break;
            }
        }
        InstructionAt(emevd, event, instruction).PackArgs(Packed, IsBigEndian(emevd));
    });
}
#pragma endregion

#pragma region Parameters and strings
SOULS_C_API size_t souls_emevd_parameter_count(const SoulsEmevd* emevd, size_t event) {
    return GuardValue<size_t>(0, [&] { return EventAt(emevd, event).Parameters.size(); });
}

SOULS_C_API SoulsStatus souls_emevd_parameter(const SoulsEmevd* emevd,
                                              size_t event,
                                              size_t parameter,
                                              int64_t* instruction_index,
                                              int64_t* target_start_byte,
                                              int64_t* source_start_byte,
                                              int32_t* byte_count,
                                              int32_t* unk_id) {
    return Guard([&] {
        const EMEVD::Parameter& P = ParameterAt(emevd, event, parameter);
        if (instruction_index) *instruction_index = P.InstructionIndex;
        if (target_start_byte) *target_start_byte = P.TargetStartByte;
        if (source_start_byte) *source_start_byte = P.SourceStartByte;
        if (byte_count) *byte_count = P.ByteCount;
        if (unk_id) *unk_id = P.UnkID;
    });
}

SOULS_C_API int64_t souls_emevd_add_parameter(SoulsEmevd* emevd,
                                              size_t event,
                                              int64_t instruction_index,
                                              int64_t target_start_byte,
                                              int64_t source_start_byte,
                                              int32_t byte_count,
                                              int32_t unk_id) {
    return GuardValue<int64_t>(-1, [&]() -> int64_t {
        auto& Parameters = EventAt(emevd, event).Parameters;
        EMEVD::Parameter P;
        P.InstructionIndex = instruction_index;
        P.TargetStartByte  = target_start_byte;
        P.SourceStartByte  = source_start_byte;
        P.ByteCount        = byte_count;
        P.UnkID            = unk_id;
        Parameters.push_back(P);
        return static_cast<int64_t>(Parameters.size() - 1);
    });
}

SOULS_C_API SoulsStatus souls_emevd_remove_parameter(SoulsEmevd* emevd, size_t event, size_t parameter) {
    return Guard([&] {
        auto& Parameters = EventAt(emevd, event).Parameters;
        CheckIndex(parameter, Parameters.size(), "parameter index out of range");
        Parameters.erase(Parameters.begin() + static_cast<std::ptrdiff_t>(parameter));
    });
}

SOULS_C_API const uint8_t* souls_emevd_string_data(const SoulsEmevd* emevd, size_t* out_size) {
    return GuardValue<const uint8_t*>(nullptr, [&] {
        const auto& Data = File(emevd).StringData;
        if (out_size) *out_size = Data.size();
        return Data.data();
    });
}

SOULS_C_API SoulsStatus souls_emevd_set_string_data(SoulsEmevd* emevd, const uint8_t* data, size_t size) {
    return Guard([&] {
        const auto Span = Bytes(data, size);
        File(emevd).StringData.assign(Span.begin(), Span.end());
    });
}

SOULS_C_API size_t souls_emevd_linked_file_count(const SoulsEmevd* emevd) {
    return GuardValue<size_t>(0, [&] { return File(emevd).LinkedFileOffsets.size(); });
}

SOULS_C_API int64_t souls_emevd_linked_file_offset(const SoulsEmevd* emevd, size_t index) {
    return GuardValue<int64_t>(-1, [&] {
        const auto& Offsets = File(emevd).LinkedFileOffsets;
        CheckIndex(index, Offsets.size(), "linked file index out of range");
        return Offsets[index];
    });
}

SOULS_C_API SoulsStatus souls_emevd_set_linked_file_offsets(SoulsEmevd* emevd, const int64_t* offsets, size_t count) {
    return Guard([&] {
        if (!offsets && count) ThrowInvalid("offsets is NULL");
        File(emevd).LinkedFileOffsets.assign(offsets, offsets + count);
    });
}
#pragma endregion
