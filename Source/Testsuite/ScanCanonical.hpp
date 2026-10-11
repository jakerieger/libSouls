//
// Created by Jake Rieger on 10/11/2026.
//

#pragma once

// Text forms of files for the format scan: two files whose text is equal say the same thing, even if their bytes are
// laid out differently (unshared strings, stale bytes left at the end, ...). Used to tell a harmless rewrite from one
// that loses data.

#include <libSouls/Formats/ESD.hpp>
#include <libSouls/Formats/FXR3.hpp>
#include <libSouls/Formats/MATBIN.hpp>

#include <cstdio>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

namespace ScanCanonical {
    using namespace Souls;

    inline std::string HexOf(const std::vector<uint8_t>& Bytes) {
        std::string Text;
        char Buffer[4];
        for (const uint8_t B : Bytes) {
            std::snprintf(Buffer, sizeof Buffer, "%02X", B);
            Text += Buffer;
        }
        return Text;
    }

    inline std::string Matbin(const std::vector<uint8_t>& Bytes) {
        const MATBIN File = MATBIN::Read(Bytes);
        std::string Text  = File.ShaderPath + "|" + File.SourcePath + "|" + std::to_string(File.Key);
        for (const MATBIN::Param& P : File.Params) {
            Text += "|P:" + P.Name + ":" + std::to_string(static_cast<uint32_t>(P.Type)) + ":" + std::to_string(P.Key) + ":";
            std::visit(
                [&](const auto& V) {
                    using VT = std::decay_t<decltype(V)>;
                    if constexpr (std::is_same_v<VT, std::vector<int32_t>> || std::is_same_v<VT, std::vector<float>>) {
                        for (const auto X : V) Text += std::to_string(X) + ",";
                    } else {
                        Text += std::to_string(V);
                    }
                },
                P.Value);
        }
        for (const MATBIN::Sampler& S : File.Samplers) {
            Text += "|S:" + S.Type + ":" + S.Path + ":" + std::to_string(S.Key) + ":" + std::to_string(S.Unk14.X) + "," + std::to_string(S.Unk14.Y);
        }
        return Text;
    }

    inline void Commands(std::string& Text, const std::vector<ESD::CommandCall>& List) {
        for (const ESD::CommandCall& C : List) {
            Text += "(" + std::to_string(C.CommandBank) + ":" + std::to_string(C.CommandID);
            for (const auto& A : C.Arguments) Text += "," + HexOf(A);
            Text += ")";
        }
    }

    inline void Condition(std::string& Text, const ESD::Condition& C) {
        Text += "{T" + (C.TargetState ? std::to_string(*C.TargetState) : std::string("-")) + ";P";
        Commands(Text, C.PassCommands);
        Text += ";E" + HexOf(C.Evaluator) + ";S[";
        for (const auto& Sub : C.Subconditions) Condition(Text, *Sub);
        Text += "]}";
    }

    inline std::string Esd(const std::vector<uint8_t>& Bytes) {
        const ESD File   = ESD::Read(Bytes);
        std::string Text = std::to_string(File.DarkSoulsCount) + "|" + File.Name.value_or("-") + "|" + std::to_string(File.Unk70) + "," +
                           std::to_string(File.Unk74) + "," + std::to_string(File.Unk78) + "," + std::to_string(File.Unk7C);
        for (const auto& [GroupId, Group] : File.StateGroups) {
            Text += "|G" + std::to_string(GroupId);
            for (const auto& [StateId, State] : Group) {
                Text += "#S" + std::to_string(StateId) + "C[";
                for (const auto& C : State.Conditions) Condition(Text, *C);
                Text += "]E";
                Commands(Text, State.EntryCommands);
                Text += "X";
                Commands(Text, State.ExitCommands);
                Text += "W";
                Commands(Text, State.WhileCommands);
            }
        }
        return Text;
    }

    inline std::string Ints(const std::vector<int32_t>& V) {
        std::string Text = "[";
        for (const int32_t X : V) Text += std::to_string(X) + ",";
        return Text + "]";
    }

    inline std::string Property(const FXR3::FFXProperty& P) {
        std::string Text = "P(" + std::to_string(P.Unk00) + "," + std::to_string(P.Unk04) + ",8[";
        for (const auto& S8 : P.Section8s) {
            Text += "(" + std::to_string(S8.Unk00) + "," + std::to_string(S8.Unk04) + ",9[";
            for (const auto& S9 : S8.Section9s) Text += "(" + std::to_string(S9.Unk04) + Ints(S9.Section11s) + ")";
            Text += "]" + Ints(S8.Section11s) + ")";
        }
        return Text + "]" + Ints(P.Section11s) + ")";
    }

    inline std::string Host(const FXR3::FFXDrawEntityHost& H) {
        std::string Text = "H(" + std::to_string(H.Unk00) + "," + std::to_string(H.Unk02) + "," + std::to_string(H.Unk03) + "," + std::to_string(H.Unk04) + ";";
        for (const auto& P : H.Properties1) Text += Property(P);
        Text += ";";
        for (const auto& P : H.Properties2) Text += Property(P);
        Text += ";";
        for (const auto& S10 : H.Section10s) Text += "S10" + Ints(S10.Section11s);
        return Text + ";" + Ints(H.Section11s1) + Ints(H.Section11s2) + ")";
    }

    inline std::string Section4(const FXR3::Section4& S) {
        std::string Text = "S4(" + std::to_string(S.Unk00) + ";";
        for (const auto& Child : S.Section4s) Text += Section4(Child);
        Text += ";";
        for (const auto& S5 : S.Section5s) {
            Text += "S5(" + std::to_string(S5.Unk00) + ";";
            for (const auto& H : S5.Section6s) Text += Host(H);
            Text += ")";
        }
        Text += ";";
        for (const auto& H : S.Section6s) Text += Host(H);
        return Text + ")";
    }

    inline std::string Fxr3(const std::vector<uint8_t>& Bytes) {
        const FXR3 File  = FXR3::Read(Bytes);
        std::string Text = std::to_string(static_cast<int>(File.Version)) + "|" + std::to_string(File.ID) + "|";
        for (const auto& S2 : File.Section1Tree.Section2s) {
            Text += "S2(";
            for (const auto& S3 : S2.Section3s) {
                Text += "S3(" + std::to_string(S3.Unk08) + "," + std::to_string(S3.Unk10) + "," + std::to_string(S3.Unk38) + "," +
                        std::to_string(S3.Section11Data1) + "," + std::to_string(S3.Section11Data2) + ")";
            }
            Text += ")";
        }
        return Text + "|" + Section4(File.Section4Tree) + "|" + Ints(File.Section12s) + Ints(File.Section13s);
    }
}  // namespace ScanCanonical
