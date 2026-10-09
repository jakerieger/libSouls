//
// Created by Jake Rieger on 10/8/2026.
//

#include "FXR3.hpp"

#include <algorithm>

namespace Souls {
    namespace {
        using Section1          = FXR3::Section1;
        using Section2          = FXR3::Section2;
        using Section3          = FXR3::Section3;
        using Section4          = FXR3::Section4;
        using Section5          = FXR3::Section5;
        using FFXDrawEntityHost = FXR3::FFXDrawEntityHost;
        using FFXProperty       = FXR3::FFXProperty;
        using Section8          = FXR3::Section8;
        using Section9          = FXR3::Section9;
        using Section10         = FXR3::Section10;

        std::string Idx(const char* Name, int64_t I) {
            return std::string(Name) + "[" + std::to_string(I) + "]";
        }

        std::vector<int32_t> GetInt32s(BinaryReader& Reader, int64_t Offset, int64_t Count) {
            Reader.StepIn(Offset);
            std::vector<int32_t> Values = Reader.ReadArray<int32_t>(static_cast<size_t>(Count));
            Reader.StepOut();
            return Values;
        }

        template<typename T, typename Fn>
        std::vector<T> ReadList(BinaryReader& Reader, int64_t Offset, int64_t Count, Fn ReadOne) {
            std::vector<T> Items;
            Reader.StepIn(Offset);
            for (int64_t I = 0; I < Count; ++I) {
                Items.push_back(ReadOne(Reader));
            }
            Reader.StepOut();
            return Items;
        }

        Section3 ReadSection3(BinaryReader& R) {
            Section3 S;
            R.Assert<int16_t>(11);
            R.Assert<uint8_t>(0);
            R.Assert<uint8_t>(1);
            R.Assert<int32_t>(0);
            S.Unk08 = R.ReadInt32();
            R.Assert<int32_t>(0);
            S.Unk10 = R.Assert<int32_t>(0x100FFFC, 0x100FFFD);
            R.Assert<int32_t>(0);
            R.Assert<int32_t>(1);
            R.Assert<int32_t>(0);
            const int32_t Offset1 = R.ReadInt32();
            R.AssertPattern(20, 0);
            S.Unk38 = R.Assert<int32_t>(0x100FFFC, 0x100FFFD);
            R.Assert<int32_t>(0);
            R.Assert<int32_t>(1);
            R.Assert<int32_t>(0);
            const int32_t Offset2 = R.ReadInt32();
            R.AssertPattern(20, 0);
            S.Section11Data1 = R.ReadAt<int32_t>(Offset1);
            S.Section11Data2 = R.ReadAt<int32_t>(Offset2);
            return S;
        }

        Section2 ReadSection2(BinaryReader& R) {
            Section2 S;
            R.Assert<int32_t>(0);
            const int32_t Count  = R.ReadInt32();
            const int32_t Offset = R.ReadInt32();
            R.Assert<int32_t>(0);
            S.Section3s = ReadList<Section3>(R, Offset, Count, ReadSection3);
            return S;
        }

        Section9 ReadSection9(BinaryReader& R) {
            Section9 S;
            R.Assert<int16_t>(48);
            R.Assert<uint8_t>(0);
            R.Assert<uint8_t>(1);
            S.Unk04              = R.ReadInt32();
            const int32_t Count  = R.ReadInt32();
            R.Assert<int32_t>(0);
            const int32_t Offset = R.ReadInt32();
            R.Assert<int32_t>(0);
            S.Section11s = GetInt32s(R, Offset, Count);
            return S;
        }

        Section8 ReadSection8(BinaryReader& R) {
            Section8 S;
            S.Unk00 = R.ReadInt16();
            R.Assert<uint8_t>(0);
            R.Assert<uint8_t>(1);
            S.Unk04                     = R.ReadInt32();
            const int32_t Section11Count = R.ReadInt32();
            const int32_t Section9Count  = R.ReadInt32();
            const int32_t Section11Offset = R.ReadInt32();
            R.Assert<int32_t>(0);
            const int32_t Section9Offset = R.ReadInt32();
            R.Assert<int32_t>(0);
            S.Section9s  = ReadList<Section9>(R, Section9Offset, Section9Count, ReadSection9);
            S.Section11s = GetInt32s(R, Section11Offset, Section11Count);
            return S;
        }

        FFXProperty ReadProperty(BinaryReader& R) {
            FFXProperty P;
            P.Unk00 = R.ReadInt16();
            R.Assert<uint8_t>(0);
            R.Assert<uint8_t>(1);
            P.Unk04                      = R.ReadInt32();
            const int32_t Section11Count = R.ReadInt32();
            R.Assert<int32_t>(0);
            const int32_t Section11Offset = R.ReadInt32();
            R.Assert<int32_t>(0);
            const int32_t Section8Offset = R.ReadInt32();
            R.Assert<int32_t>(0);
            const int32_t Section8Count = R.ReadInt32();
            R.Assert<int32_t>(0);
            P.Section8s  = ReadList<Section8>(R, Section8Offset, Section8Count, ReadSection8);
            P.Section11s = GetInt32s(R, Section11Offset, Section11Count);
            return P;
        }

        Section10 ReadSection10(BinaryReader& R) {
            Section10 S;
            const int32_t Offset = R.ReadInt32();
            R.Assert<int32_t>(0);
            const int32_t Count = R.ReadInt32();
            R.Assert<int32_t>(0);
            S.Section11s = GetInt32s(R, Offset, Count);
            return S;
        }

        FFXDrawEntityHost ReadHost(BinaryReader& R) {
            FFXDrawEntityHost H;
            H.Unk00                       = R.ReadInt16();
            H.Unk02                       = R.ReadBool();
            H.Unk03                       = R.ReadBool();
            H.Unk04                       = R.ReadInt32();
            const int32_t Section11Count1 = R.ReadInt32();
            const int32_t Section10Count  = R.ReadInt32();
            const int32_t Section7Count1  = R.ReadInt32();
            const int32_t Section11Count2 = R.ReadInt32();
            R.Assert<int32_t>(0);
            const int32_t Section7Count2  = R.ReadInt32();
            const int32_t Section11Offset = R.ReadInt32();
            R.Assert<int32_t>(0);
            const int32_t Section10Offset = R.ReadInt32();
            R.Assert<int32_t>(0);
            const int32_t Section7Offset = R.ReadInt32();
            R.Assert<int32_t>(0);
            R.Assert<int32_t>(0);
            R.Assert<int32_t>(0);

            R.StepIn(Section7Offset);
            for (int32_t I = 0; I < Section7Count1; ++I) H.Properties1.push_back(ReadProperty(R));
            for (int32_t I = 0; I < Section7Count2; ++I) H.Properties2.push_back(ReadProperty(R));
            R.StepOut();
            H.Section10s = ReadList<Section10>(R, Section10Offset, Section10Count, ReadSection10);
            R.StepIn(Section11Offset);
            H.Section11s1 = R.ReadArray<int32_t>(static_cast<size_t>(Section11Count1));
            H.Section11s2 = R.ReadArray<int32_t>(static_cast<size_t>(Section11Count2));
            R.StepOut();
            return H;
        }

        Section5 ReadSection5(BinaryReader& R) {
            Section5 S;
            S.Unk00 = R.ReadInt16();
            R.Assert<uint8_t>(0);
            R.Assert<uint8_t>(1);
            R.Assert<int32_t>(0);
            R.Assert<int32_t>(0);
            const int32_t Count = R.ReadInt32();
            R.Assert<int32_t>(0);
            R.Assert<int32_t>(0);
            const int32_t Offset = R.ReadInt32();
            R.Assert<int32_t>(0);
            S.Section6s = ReadList<FFXDrawEntityHost>(R, Offset, Count, ReadHost);
            return S;
        }

        Section4 ReadSection4(BinaryReader& R) {
            Section4 S;
            S.Unk00 = R.ReadInt16();
            R.Assert<uint8_t>(0);
            R.Assert<uint8_t>(1);
            R.Assert<int32_t>(0);
            const int32_t Section5Count = R.ReadInt32();
            const int32_t Section6Count = R.ReadInt32();
            const int32_t Section4Count = R.ReadInt32();
            R.Assert<int32_t>(0);
            const int32_t Section5Offset = R.ReadInt32();
            R.Assert<int32_t>(0);
            const int32_t Section6Offset = R.ReadInt32();
            R.Assert<int32_t>(0);
            const int32_t Section4Offset = R.ReadInt32();
            R.Assert<int32_t>(0);
            S.Section4s = ReadList<Section4>(R, Section4Offset, Section4Count, ReadSection4);
            S.Section5s = ReadList<Section5>(R, Section5Offset, Section5Count, ReadSection5);
            S.Section6s = ReadList<FFXDrawEntityHost>(R, Section6Offset, Section6Count, ReadHost);
            return S;
        }

        void WriteInt32s(BinaryWriter& W, const std::vector<int32_t>& Values) {
            W.WriteArray(Values);
        }

        // ---- writing ----

        void WriteSection3(BinaryWriter& W, const Section3& S, std::vector<const Section3*>& List) {
            const size_t Index = List.size();
            W.WriteInt16(11);
            W.WriteByte(0);
            W.WriteByte(1);
            W.WriteInt32(0);
            W.WriteInt32(S.Unk08);
            W.WriteInt32(0);
            W.WriteInt32(S.Unk10);
            W.WriteInt32(0);
            W.WriteInt32(1);
            W.WriteInt32(0);
            W.Reserve<int32_t>(Idx("Section3Section11Offset1", Index));
            W.Pad(20);
            W.WriteInt32(S.Unk38);
            W.WriteInt32(0);
            W.WriteInt32(1);
            W.WriteInt32(0);
            W.Reserve<int32_t>(Idx("Section3Section11Offset2", Index));
            W.Pad(20);
            List.push_back(&S);
        }

        void WriteSection9(BinaryWriter& W, const Section9& S, std::vector<const Section9*>& List) {
            const size_t Index = List.size();
            W.WriteInt16(48);
            W.WriteByte(0);
            W.WriteByte(1);
            W.WriteInt32(S.Unk04);
            W.WriteInt32(static_cast<int32_t>(S.Section11s.size()));
            W.WriteInt32(0);
            W.Reserve<int32_t>(Idx("Section9Section11sOffset", Index));
            W.WriteInt32(0);
            List.push_back(&S);
        }

        void WriteSection8(BinaryWriter& W, const Section8& S, std::vector<const Section8*>& List) {
            const size_t Index = List.size();
            W.WriteInt16(S.Unk00);
            W.WriteByte(0);
            W.WriteByte(1);
            W.WriteInt32(S.Unk04);
            W.WriteInt32(static_cast<int32_t>(S.Section11s.size()));
            W.WriteInt32(static_cast<int32_t>(S.Section9s.size()));
            W.Reserve<int32_t>(Idx("Section8Section11sOffset", Index));
            W.WriteInt32(0);
            W.Reserve<int32_t>(Idx("Section8Section9sOffset", Index));
            W.WriteInt32(0);
            List.push_back(&S);
        }

        void WriteProperty(BinaryWriter& W, const FFXProperty& P, std::vector<const FFXProperty*>& List) {
            const size_t Index = List.size();
            W.WriteInt16(P.Unk00);
            W.WriteByte(0);
            W.WriteByte(1);
            W.WriteInt32(P.Unk04);
            W.WriteInt32(static_cast<int32_t>(P.Section11s.size()));
            W.WriteInt32(0);
            W.Reserve<int32_t>(Idx("Section7Section11sOffset", Index));
            W.WriteInt32(0);
            W.Reserve<int32_t>(Idx("Section7Section8sOffset", Index));
            W.WriteInt32(0);
            W.WriteInt32(static_cast<int32_t>(P.Section8s.size()));
            W.WriteInt32(0);
            List.push_back(&P);
        }

        void WriteSection10(BinaryWriter& W, const Section10& S, std::vector<const Section10*>& List) {
            const size_t Index = List.size();
            W.Reserve<int32_t>(Idx("Section10Section11sOffset", Index));
            W.WriteInt32(0);
            W.WriteInt32(static_cast<int32_t>(S.Section11s.size()));
            W.WriteInt32(0);
            List.push_back(&S);
        }

        void WriteHost(BinaryWriter& W, const FFXDrawEntityHost& H, std::vector<const FFXDrawEntityHost*>& List) {
            const size_t Index = List.size();
            W.WriteInt16(H.Unk00);
            W.WriteBool(H.Unk02);
            W.WriteBool(H.Unk03);
            W.WriteInt32(H.Unk04);
            W.WriteInt32(static_cast<int32_t>(H.Section11s1.size()));
            W.WriteInt32(static_cast<int32_t>(H.Section10s.size()));
            W.WriteInt32(static_cast<int32_t>(H.Properties1.size()));
            W.WriteInt32(static_cast<int32_t>(H.Section11s2.size()));
            W.WriteInt32(0);
            W.WriteInt32(static_cast<int32_t>(H.Properties2.size()));
            W.Reserve<int32_t>(Idx("Section6Section11sOffset", Index));
            W.WriteInt32(0);
            W.Reserve<int32_t>(Idx("Section6Section10sOffset", Index));
            W.WriteInt32(0);
            W.Reserve<int32_t>(Idx("Section6Section7sOffset", Index));
            W.WriteInt32(0);
            W.WriteInt32(0);
            W.WriteInt32(0);
            List.push_back(&H);
        }

        void WriteSection4(BinaryWriter& W, const Section4& S, std::vector<const Section4*>& List) {
            const size_t Index = List.size();
            W.WriteInt16(S.Unk00);
            W.WriteByte(0);
            W.WriteByte(1);
            W.WriteInt32(0);
            W.WriteInt32(static_cast<int32_t>(S.Section5s.size()));
            W.WriteInt32(static_cast<int32_t>(S.Section6s.size()));
            W.WriteInt32(static_cast<int32_t>(S.Section4s.size()));
            W.WriteInt32(0);
            W.Reserve<int32_t>(Idx("Section4Section5sOffset", Index));
            W.WriteInt32(0);
            W.Reserve<int32_t>(Idx("Section4Section6sOffset", Index));
            W.WriteInt32(0);
            W.Reserve<int32_t>(Idx("Section4Section4sOffset", Index));
            W.WriteInt32(0);
            List.push_back(&S);
        }

        void WriteSection4Children(BinaryWriter& W, const Section4& S, std::vector<const Section4*>& List) {
            const size_t Index = static_cast<size_t>(std::find(List.begin(), List.end(), &S) - List.begin());
            if (S.Section4s.empty()) {
                W.Fill<int32_t>(Idx("Section4Section4sOffset", Index), 0);
            } else {
                W.Fill<int32_t>(Idx("Section4Section4sOffset", Index), static_cast<int32_t>(W.Position()));
                for (const Section4& Child : S.Section4s) WriteSection4(W, Child, List);
                for (const Section4& Child : S.Section4s) WriteSection4Children(W, Child, List);
            }
        }

        void WriteSection5(BinaryWriter& W, const Section5& S, size_t Index) {
            W.WriteInt16(S.Unk00);
            W.WriteByte(0);
            W.WriteByte(1);
            W.WriteInt32(0);
            W.WriteInt32(0);
            W.WriteInt32(static_cast<int32_t>(S.Section6s.size()));
            W.WriteInt32(0);
            W.WriteInt32(0);
            W.Reserve<int32_t>(Idx("Section5Section6sOffset", static_cast<int64_t>(Index)));
            W.WriteInt32(0);
        }
    }  // namespace

    bool FXR3::IsImpl(BinaryReader& Reader) {
        if (Reader.Length() < 8) {
            return false;
        }
        Reader.Order = Endian::Little;
        const std::string Magic = Reader.GetASCII(0, 4);
        const int16_t Version_  = Reader.ReadAt<int16_t>(6);
        return Magic == std::string("FXR\0", 4) && (Version_ == 4 || Version_ == 5);
    }

    void FXR3::ReadImpl(BinaryReader& Reader) {
        Reader.Order = Endian::Little;
        Reader.AssertMagic(std::string_view("FXR\0", 4));
        Reader.Assert<int16_t>(0);
        const uint16_t RawVersion = Reader.ReadUInt16();
        if (RawVersion != 4 && RawVersion != 5) {
            throw BinaryException("Unknown FXR3 version " + std::to_string(RawVersion));
        }
        Version = static_cast<FXRVersion>(RawVersion);
        Reader.Assert<int32_t>(1);
        ID                          = Reader.ReadInt32();
        const int32_t Section1Offset = Reader.ReadInt32();
        Reader.Assert<int32_t>(1);  // section 1 count
        Reader.ReadInt32();         // section 2 offset
        Reader.ReadInt32();         // section 2 count
        Reader.ReadInt32();         // section 3 offset
        Reader.ReadInt32();         // section 3 count
        const int32_t Section4Offset = Reader.ReadInt32();
        for (int I = 0; I < 15; ++I) {
            Reader.ReadInt32();  // section 4 count; offsets and counts of sections 5 to 11
        }
        Reader.Assert<int32_t>(1);
        Reader.Assert<int32_t>(0);

        Section12s.clear();
        Section13s.clear();
        if (Version == FXRVersion::Sekiro) {
            const int32_t Section12Offset = Reader.ReadInt32();
            const int32_t Section12Count  = Reader.ReadInt32();
            const int32_t Section13Offset = Reader.ReadInt32();
            const int32_t Section13Count  = Reader.ReadInt32();
            Reader.ReadInt32();         // section 14 offset
            Reader.Assert<int32_t>(0);  // section 14 count
            Reader.Assert<int32_t>(0);
            Reader.Assert<int32_t>(0);
            Section12s = GetInt32s(Reader, Section12Offset, Section12Count);
            Section13s = GetInt32s(Reader, Section13Offset, Section13Count);
        }

        Reader.Seek(Section1Offset);
        Reader.Assert<int32_t>(0);
        const int32_t Section2Count  = Reader.ReadInt32();
        const int32_t Section2Offset = Reader.ReadInt32();
        Reader.Assert<int32_t>(0);
        Section1Tree.Section2s = ReadList<Section2>(Reader, Section2Offset, Section2Count, ReadSection2);

        Reader.Seek(Section4Offset);
        Section4Tree = ReadSection4(Reader);
    }

    void FXR3::WriteImpl(BinaryWriter& W) {
        W.Order = Endian::Little;
        W.WriteMagic(std::string_view("FXR\0", 4));
        W.WriteInt16(0);
        W.WriteUInt16(static_cast<uint16_t>(Version));
        W.WriteInt32(1);
        W.WriteInt32(ID);
        W.Reserve<int32_t>("Section1Offset");
        W.WriteInt32(1);
        W.Reserve<int32_t>("Section2Offset");
        W.WriteInt32(static_cast<int32_t>(Section1Tree.Section2s.size()));
        W.Reserve<int32_t>("Section3Offset");
        W.Reserve<int32_t>("Section3Count");
        for (int I = 4; I <= 11; ++I) {
            W.Reserve<int32_t>("Section" + std::to_string(I) + "Offset");
            W.Reserve<int32_t>("Section" + std::to_string(I) + "Count");
        }
        W.WriteInt32(1);
        W.WriteInt32(0);
        if (Version == FXRVersion::Sekiro) {
            W.Reserve<int32_t>("Section12Offset");
            W.WriteInt32(static_cast<int32_t>(Section12s.size()));
            W.Reserve<int32_t>("Section13Offset");
            W.WriteInt32(static_cast<int32_t>(Section13s.size()));
            W.Reserve<int32_t>("Section14Offset");
            W.WriteInt32(0);
            W.WriteInt32(0);
            W.WriteInt32(0);
        }

        const auto Here = [&] { return static_cast<int32_t>(W.Position()); };

        W.Fill<int32_t>("Section1Offset", Here());
        W.WriteInt32(0);
        W.WriteInt32(static_cast<int32_t>(Section1Tree.Section2s.size()));
        W.Reserve<int32_t>("Section1Section2sOffset");
        W.WriteInt32(0);
        W.Align(0x10);

        W.Fill<int32_t>("Section2Offset", Here());
        W.Fill<int32_t>("Section1Section2sOffset", Here());
        for (size_t I = 0; I < Section1Tree.Section2s.size(); ++I) {
            W.WriteInt32(0);
            W.WriteInt32(static_cast<int32_t>(Section1Tree.Section2s[I].Section3s.size()));
            W.Reserve<int32_t>("Section2Section3sOffset[" + std::to_string(I) + "]");
            W.WriteInt32(0);
        }
        W.Align(0x10);

        W.Fill<int32_t>("Section3Offset", Here());
        std::vector<const Section3*> Section3s;
        for (size_t I = 0; I < Section1Tree.Section2s.size(); ++I) {
            W.Fill<int32_t>("Section2Section3sOffset[" + std::to_string(I) + "]", Here());
            for (const Section3& S : Section1Tree.Section2s[I].Section3s) {
                WriteSection3(W, S, Section3s);
            }
        }
        W.Fill<int32_t>("Section3Count", static_cast<int32_t>(Section3s.size()));
        W.Align(0x10);

        W.Fill<int32_t>("Section4Offset", Here());
        std::vector<const Section4*> Section4s;
        WriteSection4(W, Section4Tree, Section4s);
        WriteSection4Children(W, Section4Tree, Section4s);
        W.Fill<int32_t>("Section4Count", static_cast<int32_t>(Section4s.size()));
        W.Align(0x10);

        W.Fill<int32_t>("Section5Offset", Here());
        int32_t Section5Count = 0;
        for (size_t I = 0; I < Section4s.size(); ++I) {
            const Section4& S = *Section4s[I];
            if (S.Section5s.empty()) {
                W.Fill<int32_t>(Idx("Section4Section5sOffset", static_cast<int64_t>(I)), 0);
            } else {
                W.Fill<int32_t>(Idx("Section4Section5sOffset", static_cast<int64_t>(I)), Here());
                for (size_t K = 0; K < S.Section5s.size(); ++K) {
                    WriteSection5(W, S.Section5s[K], static_cast<size_t>(Section5Count) + K);
                }
                Section5Count += static_cast<int32_t>(S.Section5s.size());
            }
        }
        W.Fill<int32_t>("Section5Count", Section5Count);
        W.Align(0x10);

        W.Fill<int32_t>("Section6Offset", Here());
        Section5Count = 0;
        std::vector<const FFXDrawEntityHost*> Section6s;
        for (size_t I = 0; I < Section4s.size(); ++I) {
            const Section4& S = *Section4s[I];
            W.Fill<int32_t>(Idx("Section4Section6sOffset", static_cast<int64_t>(I)), Here());
            for (const FFXDrawEntityHost& H : S.Section6s) {
                WriteHost(W, H, Section6s);
            }
            for (size_t K = 0; K < S.Section5s.size(); ++K) {
                W.Fill<int32_t>(Idx("Section5Section6sOffset", Section5Count + static_cast<int64_t>(K)), Here());
                for (const FFXDrawEntityHost& H : S.Section5s[K].Section6s) {
                    WriteHost(W, H, Section6s);
                }
            }
            Section5Count += static_cast<int32_t>(S.Section5s.size());
        }
        W.Fill<int32_t>("Section6Count", static_cast<int32_t>(Section6s.size()));
        W.Align(0x10);

        W.Fill<int32_t>("Section7Offset", Here());
        std::vector<const FFXProperty*> Section7s;
        for (size_t I = 0; I < Section6s.size(); ++I) {
            W.Fill<int32_t>(Idx("Section6Section7sOffset", static_cast<int64_t>(I)), Here());
            for (const FFXProperty& P : Section6s[I]->Properties1) WriteProperty(W, P, Section7s);
            for (const FFXProperty& P : Section6s[I]->Properties2) WriteProperty(W, P, Section7s);
        }
        W.Fill<int32_t>("Section7Count", static_cast<int32_t>(Section7s.size()));
        W.Align(0x10);

        W.Fill<int32_t>("Section8Offset", Here());
        std::vector<const Section8*> Section8s;
        for (size_t I = 0; I < Section7s.size(); ++I) {
            W.Fill<int32_t>(Idx("Section7Section8sOffset", static_cast<int64_t>(I)), Here());
            for (const Section8& S : Section7s[I]->Section8s) WriteSection8(W, S, Section8s);
        }
        W.Fill<int32_t>("Section8Count", static_cast<int32_t>(Section8s.size()));
        W.Align(0x10);

        W.Fill<int32_t>("Section9Offset", Here());
        std::vector<const Section9*> Section9s;
        for (size_t I = 0; I < Section8s.size(); ++I) {
            W.Fill<int32_t>(Idx("Section8Section9sOffset", static_cast<int64_t>(I)), Here());
            for (const Section9& S : Section8s[I]->Section9s) WriteSection9(W, S, Section9s);
        }
        W.Fill<int32_t>("Section9Count", static_cast<int32_t>(Section9s.size()));
        W.Align(0x10);

        W.Fill<int32_t>("Section10Offset", Here());
        std::vector<const Section10*> Section10s;
        for (size_t I = 0; I < Section6s.size(); ++I) {
            W.Fill<int32_t>(Idx("Section6Section10sOffset", static_cast<int64_t>(I)), Here());
            for (const Section10& S : Section6s[I]->Section10s) WriteSection10(W, S, Section10s);
        }
        W.Fill<int32_t>("Section10Count", static_cast<int32_t>(Section10s.size()));
        W.Align(0x10);

        W.Fill<int32_t>("Section11Offset", Here());
        int32_t Section11Count = 0;
        for (size_t I = 0; I < Section3s.size(); ++I) {
            W.Fill<int32_t>(Idx("Section3Section11Offset1", static_cast<int64_t>(I)), Here());
            W.WriteInt32(Section3s[I]->Section11Data1);
            W.Fill<int32_t>(Idx("Section3Section11Offset2", static_cast<int64_t>(I)), Here());
            W.WriteInt32(Section3s[I]->Section11Data2);
            Section11Count += 2;
        }
        for (size_t I = 0; I < Section6s.size(); ++I) {
            const FFXDrawEntityHost& H = *Section6s[I];
            if (H.Section11s1.empty() && H.Section11s2.empty()) {
                W.Fill<int32_t>(Idx("Section6Section11sOffset", static_cast<int64_t>(I)), 0);
            } else {
                W.Fill<int32_t>(Idx("Section6Section11sOffset", static_cast<int64_t>(I)), Here());
                WriteInt32s(W, H.Section11s1);
                WriteInt32s(W, H.Section11s2);
                Section11Count += static_cast<int32_t>(H.Section11s1.size() + H.Section11s2.size());
            }
        }
        for (size_t I = 0; I < Section7s.size(); ++I) {
            const FFXProperty& P = *Section7s[I];
            if (P.Section11s.empty()) {
                W.Fill<int32_t>(Idx("Section7Section11sOffset", static_cast<int64_t>(I)), 0);
            } else {
                W.Fill<int32_t>(Idx("Section7Section11sOffset", static_cast<int64_t>(I)), Here());
                WriteInt32s(W, P.Section11s);
                Section11Count += static_cast<int32_t>(P.Section11s.size());
            }
        }
        for (size_t I = 0; I < Section8s.size(); ++I) {
            W.Fill<int32_t>(Idx("Section8Section11sOffset", static_cast<int64_t>(I)), Here());
            WriteInt32s(W, Section8s[I]->Section11s);
            Section11Count += static_cast<int32_t>(Section8s[I]->Section11s.size());
        }
        for (size_t I = 0; I < Section9s.size(); ++I) {
            W.Fill<int32_t>(Idx("Section9Section11sOffset", static_cast<int64_t>(I)), Here());
            WriteInt32s(W, Section9s[I]->Section11s);
            Section11Count += static_cast<int32_t>(Section9s[I]->Section11s.size());
        }
        for (size_t I = 0; I < Section10s.size(); ++I) {
            W.Fill<int32_t>(Idx("Section10Section11sOffset", static_cast<int64_t>(I)), Here());
            WriteInt32s(W, Section10s[I]->Section11s);
            Section11Count += static_cast<int32_t>(Section10s[I]->Section11s.size());
        }
        W.Fill<int32_t>("Section11Count", Section11Count);
        W.Align(0x10);

        if (Version == FXRVersion::Sekiro) {
            W.Fill<int32_t>("Section12Offset", Here());
            WriteInt32s(W, Section12s);
            W.Align(0x10);
            W.Fill<int32_t>("Section13Offset", Here());
            WriteInt32s(W, Section13s);
            W.Align(0x10);
            W.Fill<int32_t>("Section14Offset", Here());
        }
    }
}  // namespace Souls
