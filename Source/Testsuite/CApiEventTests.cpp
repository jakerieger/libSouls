//
// Created by Jake Rieger on 10/11/2026.
//

#include "TestResult.hpp"

#include <libSouls/CApi/souls.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace {
    int Failures = 0;

#define CHECK(Cond)                                                     \
    do {                                                                \
        if (!(Cond)) {                                                  \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #Cond); \
            ++Failures;                                                 \
        }                                                               \
    } while (0)

    namespace fs = std::filesystem;
}  // namespace

int TestCApiEmevdSynthetic() {
    Failures = 0;
    for (SoulsEmevdGame Game : {SOULS_EMEVD_DARK_SOULS_1, SOULS_EMEVD_DARK_SOULS_3, SOULS_EMEVD_SEKIRO}) {
        SoulsEmevd* Emevd = souls_emevd_new(Game);
        CHECK(Emevd != nullptr && souls_emevd_game(Emevd) == Game);
        if (!Emevd) continue;

        CHECK(souls_emevd_add_event(Emevd, 0) == 0);
        CHECK(souls_emevd_add_event(Emevd, 11000000) == 1);
        CHECK(souls_emevd_set_event_rest_behavior(Emevd, 1, 1) == SOULS_OK);
        CHECK(souls_emevd_set_event_rest_behavior(Emevd, 1, 9) == SOULS_ERR_INVALID_ARGUMENT);

        // Arguments: a uint32, a float and an int16 (packed with alignment, padded to 4 bytes).
        const uint8_t Types[] = {SOULS_EMEVD_ARG_UINT32, SOULS_EMEVD_ARG_SINGLE, SOULS_EMEVD_ARG_INT16};
        const double Values[] = {7000.0, 2.5, -3.0};
        const int64_t Instruction = souls_emevd_add_instruction(Emevd, 1, 2000, 0, nullptr, 0);
        CHECK(Instruction == 0);
        CHECK(souls_emevd_pack_args(Emevd, 1, 0, Types, 3, Values) == SOULS_OK);
        size_t ArgSize = 0;
        CHECK(souls_emevd_instruction_args(Emevd, 1, 0, &ArgSize) != nullptr && ArgSize == 12);
        CHECK(souls_emevd_add_instruction(Emevd, 1, 2003, 4, nullptr, 0) == 1);
        CHECK(souls_emevd_add_parameter(Emevd, 1, 0, 0, 0, 4, 0) == 0);
        CHECK(souls_emevd_set_event_name(Emevd, 1, "Test event") == SOULS_OK);
        const int64_t Offsets[] = {0, 16};
        CHECK(souls_emevd_set_linked_file_offsets(Emevd, Offsets, 2) == SOULS_OK);
        const uint8_t Strings[] = {'a', 0, 'b', 0};
        CHECK(souls_emevd_set_string_data(Emevd, Strings, sizeof Strings) == SOULS_OK);

        SoulsBuffer Bytes = souls_emevd_write_memory(Emevd);
        CHECK(Bytes.data != nullptr);
        SoulsEmevd* Back = souls_emevd_read_memory(Bytes.data, Bytes.size);
        CHECK(Back != nullptr);
        if (Back) {
            CHECK(souls_emevd_event_count(Back) == 2 && souls_emevd_find_event(Back, 11000000) == 1);
            CHECK(souls_emevd_event_rest_behavior(Back, 1) == 1);
            CHECK(souls_emevd_instruction_count(Back, 1) == 2);
            CHECK(souls_emevd_instruction_bank(Back, 1, 0) == 2000 && souls_emevd_instruction_id(Back, 1, 0) == 0);
            double Unpacked[3] = {};
            CHECK(souls_emevd_unpack_args(Back, 1, 0, Types, 3, Unpacked) == SOULS_OK);
            CHECK(Unpacked[0] == 7000.0 && Unpacked[1] == 2.5 && Unpacked[2] == -3.0);
            CHECK(souls_emevd_parameter_count(Back, 1) == 1);
            int64_t Instruction2 = 9, Target = 9, Source = 9;
            int32_t Count = 0, Unk = 9;
            CHECK(souls_emevd_parameter(Back, 1, 0, &Instruction2, &Target, &Source, &Count, &Unk) == SOULS_OK && Count == 4);
            CHECK(souls_emevd_unpack_args(Back, 1, 0, Types, 3, nullptr) == SOULS_ERR_INVALID_ARGUMENT);
            const uint8_t BadTypes[] = {99};
            CHECK(souls_emevd_unpack_args(Back, 1, 0, BadTypes, 1, Unpacked) == SOULS_ERR_INVALID_ARGUMENT);
            CHECK(souls_emevd_remove_instruction(Back, 1, 0) == SOULS_OK && souls_emevd_instruction_count(Back, 1) == 1);
            CHECK(souls_emevd_remove_event(Back, 0) == SOULS_OK && souls_emevd_event_count(Back) == 1);
            CHECK(souls_emevd_event_id(Back, 5) == -1 && souls_last_error_code() == SOULS_ERR_INVALID_ARGUMENT);
            souls_emevd_free(Back);
        }
        souls_buffer_free(Bytes);
        souls_emevd_free(Emevd);
    }
    CHECK(souls_emevd_new(static_cast<SoulsEmevdGame>(99)) == nullptr);
    return Failures;
}

// A real Dark Souls Remastered event script: it writes back as it was, and instruction arguments pack and unpack.
int TestCApiEmevdReal() {
    Failures            = 0;
    const fs::path Path = "C:/Program Files (x86)/Steam/steamapps/common/DARK SOULS REMASTERED/event/m10_00_00_00.emevd.dcx";
    if (!fs::exists(Path)) {
        std::printf("Real EMEVD C API checks skipped (no Dark Souls Remastered install)\n");
        return TestSkipped;
    }
    const std::string PathText = Path.string();
    SoulsBuffer Original       = souls_read_file(PathText.c_str());
    SoulsEmevd* Emevd          = souls_emevd_read_file(PathText.c_str());
    CHECK(Emevd != nullptr && Original.data != nullptr);
    if (!Emevd) return Failures;

    CHECK(souls_emevd_game(Emevd) == SOULS_EMEVD_DARK_SOULS_1);
    CHECK(souls_emevd_event_count(Emevd) > 0);

    // Unedited, it writes back to the same script (compared uncompressed).
    SoulsBuffer Written = souls_emevd_write_memory(Emevd);
    SoulsBuffer A       = souls_dcx_decompress(Original.data, Original.size, nullptr);
    SoulsBuffer B       = souls_dcx_decompress(Written.data, Written.size, nullptr);
    CHECK(A.size > 0 && A.size == B.size && std::memcmp(A.data, B.data, A.size) == 0);
    souls_buffer_free(A);
    souls_buffer_free(B);
    souls_buffer_free(Written);

    // Reading the first instruction with 4-byte-multiple arguments as uint32s and packing them back gives the same bytes.
    bool Checked = false;
    for (size_t E = 0; E < souls_emevd_event_count(Emevd) && !Checked; ++E) {
        for (size_t I = 0; I < souls_emevd_instruction_count(Emevd, E) && !Checked; ++I) {
            size_t Size         = 0;
            const uint8_t* Args = souls_emevd_instruction_args(Emevd, E, I, &Size);
            if (!Args || Size == 0 || Size % 4 != 0) continue;
            std::vector<uint8_t> Copy(Args, Args + Size);
            const std::vector<uint8_t> Types(Size / 4, SOULS_EMEVD_ARG_UINT32);
            std::vector<double> Values(Size / 4);
            CHECK(souls_emevd_unpack_args(Emevd, E, I, Types.data(), Types.size(), Values.data()) == SOULS_OK);
            CHECK(souls_emevd_pack_args(Emevd, E, I, Types.data(), Types.size(), Values.data()) == SOULS_OK);
            const uint8_t* After = souls_emevd_instruction_args(Emevd, E, I, &Size);
            CHECK(Size == Copy.size() && std::memcmp(After, Copy.data(), Size) == 0);
            Checked = true;
        }
    }
    CHECK(Checked);
    souls_emevd_free(Emevd);
    souls_buffer_free(Original);
    return Failures;
}

int TestCApiTae3() {
    Failures = 0;
    SoulsTae3* Tae = souls_tae3_new();
    CHECK(Tae != nullptr);
    if (!Tae) return Failures;
    CHECK(souls_tae3_set_id(Tae, 1234) == SOULS_OK);
    CHECK(souls_tae3_set_names(Tae, "skeleton.hkt", "c1234.sib") == SOULS_OK);

    const size_t Types = souls_tae3_type_count();
    CHECK(Types > 100);
    CHECK(souls_tae3_type_name(Types) == nullptr && souls_last_error_code() == SOULS_ERR_INVALID_ARGUMENT);

    // One event of every type with a layout, all with default fields.
    const int64_t Anim = souls_tae3_add_animation(Tae, 5);
    CHECK(Anim == 0);
    for (size_t T = 0; T < Types; ++T) {
        const int64_t Event = souls_tae3_add_event(Tae, 0, souls_tae3_type_number(T), static_cast<double>(T), static_cast<double>(T) + 1.0);
        CHECK(Event == static_cast<int64_t>(T));
    }
    CHECK(souls_tae3_add_event(Tae, 0, -12345, 0, 1) == -1);
    CHECK(souls_tae3_event_count(Tae, 0) == Types);

    // Edit a typed event by field name.
    const int64_t Jump = souls_tae3_add_event(Tae, 0, 0, 0.5, 1.5);  // JumpTable
    CHECK(Jump >= 0);
    CHECK(std::strcmp(souls_tae3_event_type_name(Tae, 0, static_cast<size_t>(Jump)), "JumpTable") == 0);
    CHECK(souls_tae3_set_number(Tae, 0, static_cast<size_t>(Jump), "JumpTableID", 77) == SOULS_OK);
    double Value = 0;
    CHECK(souls_tae3_get_number(Tae, 0, static_cast<size_t>(Jump), "JumpTableID", &Value) == SOULS_OK && Value == 77);
    CHECK(souls_tae3_get_number(Tae, 0, static_cast<size_t>(Jump), "NoSuchField", &Value) == SOULS_ERR_UNSUPPORTED);
    CHECK(souls_tae3_get_bytes(Tae, 0, static_cast<size_t>(Jump), "JumpTableID", nullptr, 0, nullptr) == SOULS_ERR_INVALID_ARGUMENT);
    CHECK(souls_tae3_field_count(Tae, 0, static_cast<size_t>(Jump)) == 5);
    CHECK(souls_tae3_field_kind(Tae, 0, static_cast<size_t>(Jump), 0) == SOULS_PROPERTY_INT);

    const int32_t Members[] = {static_cast<int32_t>(Jump)};
    CHECK(souls_tae3_add_group(Tae, 0, 0, Members, 1) == 0);
    const int32_t Bad[] = {9999};
    CHECK(souls_tae3_add_group(Tae, 0, 0, Bad, 1) == -1);

    CHECK(souls_tae3_add_animation(Tae, 6) == 1);
    CHECK(souls_tae3_set_animation_reference(Tae, 1, 1, "a000_003000.hkt") == SOULS_OK);

    SoulsBuffer Bytes = souls_tae3_write_memory(Tae);
    CHECK(Bytes.data != nullptr);
    if (!Bytes.data) std::printf("  error: %s\n", souls_last_error());
    SoulsTae3* Back = souls_tae3_read_memory(Bytes.data, Bytes.size);
    CHECK(Back != nullptr);
    if (Back) {
        CHECK(souls_tae3_id(Back) == 1234 && std::strcmp(souls_tae3_skeleton_name(Back), "skeleton.hkt") == 0);
        CHECK(souls_tae3_animation_count(Back) == 2 && souls_tae3_find_animation(Back, 6) == 1);
        CHECK(souls_tae3_animation_is_reference(Back, 1) == 1);
        CHECK(souls_tae3_event_count(Back, 0) == Types + 1);
        CHECK(souls_tae3_group_count(Back, 0) == 1);
        double Again = 0;
        CHECK(souls_tae3_get_number(Back, 0, static_cast<size_t>(Jump), "JumpTableID", &Again) == SOULS_OK && Again == 77);
        CHECK(std::fabs(souls_tae3_event_start(Back, 0, static_cast<size_t>(Jump)) - 0.5) < 1e-6);
        // Every event type survived the trip.
        size_t Same = 0;
        for (size_t T = 0; T < Types; ++T) {
            if (souls_tae3_event_type(Back, 0, T) == souls_tae3_type_number(T)) ++Same;
        }
        CHECK(Same == Types);

        SoulsBuffer Again2 = souls_tae3_write_memory(Back);
        CHECK(Again2.size == Bytes.size && Again2.data && std::memcmp(Again2.data, Bytes.data, Bytes.size) == 0);
        souls_buffer_free(Again2);
        souls_tae3_free(Back);
    }
    souls_buffer_free(Bytes);
    souls_tae3_free(Tae);
    return Failures;
}

// Version 0x1000D files (Sekiro, Elden Ring) keep events as raw bytes.
int TestCApiTae3Raw() {
    Failures       = 0;
    SoulsTae3* Tae = souls_tae3_new();
    CHECK(Tae != nullptr);
    if (!Tae) return Failures;
    CHECK(souls_tae3_version(Tae) == 0x1000C);
    CHECK(souls_tae3_set_version(Tae, 0x1234) == SOULS_ERR_INVALID_ARGUMENT);
    CHECK(souls_tae3_set_version(Tae, 0x1000D) == SOULS_OK);
    CHECK(souls_tae3_add_animation(Tae, 3000) == 0);

    std::vector<uint8_t> Fields(32);
    for (size_t I = 0; I < Fields.size(); ++I) Fields[I] = static_cast<uint8_t>(I + 1);
    CHECK(souls_tae3_add_raw_event(Tae, 0, 129, 0.0, 1.5, Fields.data(), Fields.size()) == 0);
    CHECK(souls_tae3_add_raw_event(Tae, 0, 233, 0.5, 2.0, Fields.data(), 16) == 1);

    CHECK(souls_tae3_event_is_raw(Tae, 0, 0) == 1);
    CHECK(souls_tae3_event_type(Tae, 0, 0) == 129);
    CHECK(souls_tae3_event_type_name(Tae, 0, 0) == nullptr);
    double Value = 0;
    CHECK(souls_tae3_get_number(Tae, 0, 0, "Unk00", &Value) == SOULS_ERR_UNSUPPORTED);
    CHECK(souls_tae3_field_count(Tae, 0, 0) == 0);

    SoulsBuffer Bytes = souls_tae3_write_memory(Tae);
    CHECK(Bytes.data != nullptr);
    SoulsTae3* Back = souls_tae3_read_memory(Bytes.data, Bytes.size);
    CHECK(Back != nullptr);
    if (Back) {
        CHECK(souls_tae3_version(Back) == 0x1000D && souls_tae3_event_count(Back, 0) == 2);
        CHECK(souls_tae3_event_is_raw(Back, 0, 1) == 1 && souls_tae3_event_type(Back, 0, 1) == 233);
        size_t Size         = 0;
        const uint8_t* Data = souls_tae3_event_raw_data(Back, 0, 0, &Size);
        CHECK(Data != nullptr && Size >= 32 && std::memcmp(Data, Fields.data(), 32) == 0);
        CHECK(souls_tae3_event_raw_data(Tae, 0, 5, &Size) == nullptr);
        SoulsBuffer Again = souls_tae3_write_memory(Back);
        CHECK(Again.size == Bytes.size && Again.data && std::memcmp(Again.data, Bytes.data, Bytes.size) == 0);
        souls_buffer_free(Again);
        souls_tae3_free(Back);
    }
    souls_buffer_free(Bytes);
    souls_tae3_free(Tae);
    return Failures;
}
