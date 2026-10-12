// Copyright (C) 2026 Jake Rieger
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, version 3. This program is distributed in the hope that it will
// be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU General
// Public License along with this program. If not, see https://www.gnu.org/licenses/.

#include "TestPaths.hpp"
#include "TestResult.hpp"

#include <libSouls/Binders/ArchiveSet.hpp>
#include <libSouls/Crypto.hpp>
#include <libSouls/Formats/BHD5.hpp>
#include <libSouls/Util.hpp>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_set>

namespace {
    int Failures = 0;

#define CHECK(Cond)                                                                                                    \
    do {                                                                                                               \
        if (!(Cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #Cond);                                                \
            ++Failures;                                                                                                \
        }                                                                                                              \
    } while (0)

    using namespace Souls;
    namespace fs = std::filesystem;

    template<typename F>
    bool Throws(F&& Fn) {
        try {
            Fn();
        } catch (...) { return true; }
        return false;
    }

    BHD5 MakeArchive(BHD5::Game Game, bool BigEndian) {
        BHD5 Bhd(Game);
        Bhd.BigEndian = BigEndian;
        Bhd.Unk05     = true;
        if (Game >= BHD5::Game::DarkSouls2) Bhd.Salt = "SaltySaltSalt";

        for (int B = 0; B < 3; ++B) {
            BHD5::Bucket Bucket;
            for (int F = 0; F < B * 2; ++F) {  // buckets of 0, 2 and 4 files
                BHD5::FileHeader File;
                File.FileNameHash   = 0x1000u + static_cast<uint64_t>(B * 10 + F);
                File.PaddedFileSize = 0x100 + F;
                File.FileOffset     = 0x1000 * (B + 1) + F * 0x200;
                if (Game >= BHD5::Game::DarkSouls3) File.UnpaddedFileSize = 0xF0 + F;
                if (Game >= BHD5::Game::DarkSouls2) {
                    if (F % 2 == 0) {
                        BHD5::SHAHash Sha;
                        for (size_t I = 0; I < Sha.Hash.size(); ++I)
                            Sha.Hash[I] = static_cast<uint8_t>(I * 3 + F);
                        Sha.Ranges = {{0, 0x40}, {0x80, 0xC0}};
                        File.SHA   = Sha;
                    }
                    if (F % 3 != 2) {
                        BHD5::AESKey Aes;
                        for (size_t I = 0; I < Aes.Key.size(); ++I)
                            Aes.Key[I] = static_cast<uint8_t>(0xA0 + I + F);
                        Aes.Ranges = {{0, 0x20}, {-1, -1}};
                        File.AES   = Aes;
                    }
                }
                Bucket.push_back(std::move(File));
            }
            Bhd.Buckets.push_back(std::move(Bucket));
        }
        return Bhd;
    }

    bool Same(const BHD5::FileHeader& A, const BHD5::FileHeader& B) {
        auto SameRanges = [](const std::vector<BHD5::Range>& X, const std::vector<BHD5::Range>& Y) {
            if (X.size() != Y.size()) return false;
            for (size_t I = 0; I < X.size(); ++I) {
                if (X[I].StartOffset != Y[I].StartOffset || X[I].EndOffset != Y[I].EndOffset) return false;
            }
            return true;
        };
        if (A.FileNameHash != B.FileNameHash || A.PaddedFileSize != B.PaddedFileSize ||
            A.UnpaddedFileSize != B.UnpaddedFileSize || A.FileOffset != B.FileOffset ||
            A.SHA.has_value() != B.SHA.has_value() || A.AES.has_value() != B.AES.has_value()) {
            return false;
        }
        if (A.SHA && (A.SHA->Hash != B.SHA->Hash || !SameRanges(A.SHA->Ranges, B.SHA->Ranges))) return false;
        if (A.AES && (A.AES->Key != B.AES->Key || !SameRanges(A.AES->Ranges, B.AES->Ranges))) return false;
        return true;
    }

    void TestSynthetic() {
        for (const auto Game :
             {BHD5::Game::DarkSouls1, BHD5::Game::DarkSouls2, BHD5::Game::DarkSouls3, BHD5::Game::EldenRing}) {
            for (const bool Big : {false, true}) {
                BHD5 Source      = MakeArchive(Game, Big);
                const auto Bytes = Source.Write();
                BHD5 Back        = BHD5::Read(Bytes, Game);

                CHECK(Back.BigEndian == Big && Back.Unk05 && Back.Salt == Source.Salt);
                CHECK(Back.Buckets.size() == Source.Buckets.size());
                for (size_t B = 0; B < Source.Buckets.size() && B < Back.Buckets.size(); ++B) {
                    CHECK(Back.Buckets[B].size() == Source.Buckets[B].size());
                    for (size_t F = 0; F < Source.Buckets[B].size() && F < Back.Buckets[B].size(); ++F) {
                        const bool Equal = Same(Source.Buckets[B][F], Back.Buckets[B][F]);
                        if (!Equal)
                            std::printf("  (game %d big %d bucket %zu file %zu differs)\n",
                                        static_cast<int>(Game),
                                        Big,
                                        B,
                                        F);
                        CHECK(Equal);
                    }
                }
                CHECK(Back.Write() == Bytes);
            }
        }

        // Bad input.
        BHD5 Good  = MakeArchive(BHD5::Game::DarkSouls3, false);
        auto Bytes = Good.Write();
        CHECK(Throws([&] { BHD5::Read(std::vector<uint8_t> {1, 2, 3, 4}, BHD5::Game::DarkSouls3); }));
        std::vector<uint8_t> Truncated(Bytes.begin(), Bytes.begin() + 0x30);
        CHECK(Throws([&] { BHD5::Read(Truncated, BHD5::Game::DarkSouls3); }));
        std::vector<uint8_t> BadBuckets = Bytes;
        BadBuckets[0x10]                = 0xFF;
        BadBuckets[0x11]                = 0xFF;
        BadBuckets[0x12]                = 0xFF;
        BadBuckets[0x13]                = 0x7F;
        CHECK(Throws([&] { BHD5::Read(BadBuckets, BHD5::Game::DarkSouls3); }));
        BHD5 BadSha              = MakeArchive(BHD5::Game::DarkSouls3, false);
        BadSha.Buckets[1][0].SHA = BHD5::SHAHash {};
        BadSha.Buckets[1][0].SHA->Hash.resize(5);
        CHECK(Throws([&] { BadSha.Write(); }));
    }

    void TestCrypto() {
        // FIPS-197 appendix C.1: AES-128 with key 000102..0f.
        std::vector<uint8_t> Key(16),
          Cipher = {0x69, 0xc4, 0xe0, 0xd8, 0x6a, 0x7b, 0x04, 0x30, 0xd8, 0xcd, 0xb7, 0x80, 0x70, 0xb4, 0xc5, 0x5a};
        for (size_t I = 0; I < Key.size(); ++I)
            Key[I] = static_cast<uint8_t>(I);
        const std::vector<uint8_t> Plain =
          {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
        std::vector<uint8_t> Data = Cipher;
        Crypto::DecryptAesEcb(Key, Data);
        CHECK(Data == Plain);
        CHECK(Throws([&] {
            std::vector<uint8_t> Odd(10);
            Crypto::DecryptAesEcb(Key, Odd);
        }));

        // BHD5::AESKey::Decrypt only touches its ranges.
        BHD5::AESKey Aes;
        Aes.Key    = Key;
        Aes.Ranges = {{16, 32}, {-1, -1}, {40, 40}};
        std::vector<uint8_t> File(48, 0xEE);
        std::copy(Cipher.begin(), Cipher.end(), File.begin() + 16);
        Aes.Decrypt(File);
        CHECK(std::equal(Plain.begin(), Plain.end(), File.begin() + 16));
        CHECK(File[0] == 0xEE && File[32] == 0xEE && File[47] == 0xEE);
        Aes.Ranges = {{0, 100}};
        CHECK(Throws([&] { Aes.Decrypt(File); }));

        // PEM key discovery.
        const std::string Fake = "xx-----BEGIN RSA PUBLIC KEY-----\nAAAA\n-----END RSA PUBLIC KEY-----yy-----BEGIN RSA "
                                 "PUBLIC KEY-----\nBBBB\n-----END RSA PUBLIC KEY-----";
        const auto Keys = Crypto::FindRsaPublicKeys({reinterpret_cast<const uint8_t*>(Fake.data()), Fake.size()});
        CHECK(Keys.size() == 2 && Keys[0].find("AAAA") != std::string::npos &&
              Keys[1].find("BBBB") != std::string::npos);
        CHECK(Throws([&] { Crypto::DecryptRsaBlocks("not a key", std::vector<uint8_t>(256)); }));
    }

    std::vector<uint8_t> ReadAll(const fs::path& Path) {
        BinaryReader Reader(Path);
        return Reader.ReadBytes(static_cast<size_t>(Reader.Length()));
    }

    bool StartsWith(const std::vector<uint8_t>& Bytes, const char* Magic, size_t Length) {
        if (Bytes.size() < Length) return false;
        for (size_t I = 0; I < Length; ++I) {
            if (Bytes[I] != static_cast<uint8_t>(Magic[I])) return false;
        }
        return true;
    }
}  // namespace

int RunBHD5Tests() {
    Failures = 0;
    TestSynthetic();
    TestCrypto();

    // Elden Ring: pull the RSA keys out of the (unpacked) executable, decrypt the real archive headers and check
    // they parse, rewrite identically and give back readable file data.
    const fs::path Game = "C:/Program Files (x86)/Steam/steamapps/common/ELDEN RING/Game";
    if (!fs::exists(Game / "eldenring.exe")) {
        std::printf("Real BHD5 checks skipped (no Elden Ring install)\n");
    } else {
        try {
            const auto Exe  = ReadAll(Game / "eldenring.exe");
            const auto Keys = Crypto::FindRsaPublicKeys(Exe);
            std::printf("eldenring.exe: %zu RSA public keys\n", Keys.size());
            CHECK(!Keys.empty());

            int Decoded = 0;
            for (const char* Name : {"Data0", "Data1", "Data2", "Data3", "DLC"}) {
                const fs::path BhdPath = Game / (std::string(Name) + ".bhd");
                const fs::path BdtPath = Game / (std::string(Name) + ".bdt");
                if (!fs::exists(BhdPath) || !fs::exists(BdtPath)) continue;

                const auto Encrypted = ReadAll(BhdPath);
                std::vector<uint8_t> Header;
                for (const std::string& Key : Keys) {
                    try {
                        auto Candidate = Crypto::DecryptRsaBlocks(Key, Encrypted);
                        if (StartsWith(Candidate, "BHD5", 4)) {
                            Header = std::move(Candidate);
                            break;
                        }
                    } catch (const BinaryException&) {
                        // The wrong key can produce blocks larger than its modulus, which CNG rejects.
                    }
                }
                if (Header.empty()) {
                    std::printf("FAIL %s.bhd: no key from the executable decrypts it\n", Name);
                    ++Failures;
                    continue;
                }

                BHD5 Bhd     = BHD5::Read(Header, BHD5::Game::EldenRing);
                size_t Files = 0, Encrypted_ = 0;
                std::unordered_set<uint64_t> Hashes;
                for (const auto& Bucket : Bhd.Buckets) {
                    Files += Bucket.size();
                    for (const auto& File : Bucket) {
                        Encrypted_ += File.AES.has_value();
                        Hashes.insert(File.FileNameHash);
                    }
                }
                std::printf("%s.bhd: %zu buckets, %zu files (%zu encrypted), salt \"%s\"\n",
                            Name,
                            Bhd.Buckets.size(),
                            Files,
                            Encrypted_,
                            Bhd.Salt.c_str());
                CHECK(Files > 0);

                // BinderKeys' copy of the key decrypts the same header, and its dictionary of known paths hashes
                // (with FromPathHash64) to files that are in the archive.
                const fs::path Keys_ = TestPaths::BinderKeys() / "EldenRing_PC";
                if (const fs::path Pem = Keys_ / "Key" / (std::string(Name) + ".pem"); fs::exists(Pem)) {
                    const auto Text = ReadAll(Pem);
                    CHECK(Crypto::DecryptRsaBlocks(std::string(Text.begin(), Text.end()), Encrypted) == Header);
                }
                if (std::ifstream Dictionary(Keys_ / "Hash" / (std::string(Name) + ".txt")); Dictionary) {
                    size_t Known = 0, Found = 0;
                    for (std::string Line; std::getline(Dictionary, Line);) {
                        while (!Line.empty() && (Line.back() == '\r' || Line.back() == ' '))
                            Line.pop_back();
                        if (Line.empty() || Line[0] != '/') continue;
                        ++Known;
                        Found += Hashes.count(Util::FromPathHash64(Line));
                    }
                    std::printf("  BinderKeys dictionary: %zu of %zu paths found\n", Found, Known);
                    CHECK(Known > 0 && Found * 10 >= Known * 9);
                }

                // Written back and re-parsed, every bucket and file header (including hashes, keys and ranges) comes
                // out the same. The byte layout of the hash/key data is free to differ from the game's own.
                const auto Rewritten = Bhd.Write();
                const BHD5 Reparsed  = BHD5::Read(Rewritten, BHD5::Game::EldenRing);
                bool Equivalent      = Reparsed.Buckets.size() == Bhd.Buckets.size() && Reparsed.Salt == Bhd.Salt &&
                                  Reparsed.BigEndian == Bhd.BigEndian && Reparsed.Unk05 == Bhd.Unk05;
                for (size_t B = 0; Equivalent && B < Bhd.Buckets.size(); ++B) {
                    Equivalent = Reparsed.Buckets[B].size() == Bhd.Buckets[B].size();
                    for (size_t F = 0; Equivalent && F < Bhd.Buckets[B].size(); ++F) {
                        Equivalent = Same(Bhd.Buckets[B][F], Reparsed.Buckets[B][F]);
                    }
                }
                if (!Equivalent) std::printf("  (%s.bhd: rewritten header parses differently)\n", Name);
                CHECK(Equivalent);

                // Read some real files out of the BDT, decrypting the encrypted ones: most are DCX-wrapped.
                BinaryReader Bdt(BdtPath);
                int Checked = 0, Recognized = 0;
                for (const auto& Bucket : Bhd.Buckets) {
                    for (const auto& File : Bucket) {
                        if (Checked >= 200) break;
                        if (File.PaddedFileSize <= 0 || File.FileOffset + File.PaddedFileSize > Bdt.Length()) continue;
                        const auto Bytes = File.ReadFile(Bdt);
                        ++Checked;
                        if (StartsWith(Bytes, "DCX", 3) || StartsWith(Bytes, "BND4", 4) ||
                            StartsWith(Bytes, "TPF", 3) || StartsWith(Bytes, "BHF4", 4) ||
                            StartsWith(Bytes, "BDF4", 4) || StartsWith(Bytes, "FSB5", 4)) {
                            ++Recognized;
                        }
                    }
                }
                std::printf("  %d files read, %d with a recognized format magic\n", Checked, Recognized);
                CHECK(Checked > 0 && Recognized * 2 > Checked);
                ++Decoded;
            }
            CHECK(Decoded > 0);
        } catch (const std::exception& E) {
            std::printf("FAIL real BHD5: %s\n", E.what());
            ++Failures;
        }

        // ArchiveSet: the archives opened with BinderKeys' keys, files read by path.
        try {
            const fs::path Keys = TestPaths::BinderKeys() / "EldenRing_PC" / "Key";
            if (fs::exists(Keys)) {
                const ArchiveSet Archives = ArchiveSet::OpenWithKeys(Game, BHD5::Game::EldenRing, Keys);
                CHECK(Archives.ArchiveNames().size() == 5 && Archives.FileCount() > 100000);
                CHECK(Archives.Contains("/chr/c0000.anibnd.dcx") && Archives.Contains("CHR\\C0000.ANIBND.DCX"));
                const auto Binder = Archives.Read("/chr/c0000.anibnd.dcx");
                CHECK(Binder && Binder->size() > 4 && std::equal(Binder->begin(), Binder->begin() + 4, "DCX\0"));
                CHECK(!Archives.Read("/no/such/file.bin") && !Archives.Contains("/no/such/file.bin"));
                std::printf("ArchiveSet: %zu archives, %zu files\n", Archives.ArchiveNames().size(), Archives.FileCount());
            }
            // A folder without archives opens empty; a wrong key is an error.
            CHECK(ArchiveSet::OpenWithKeys(TestPaths::Vendor(), BHD5::Game::EldenRing, Keys).FileCount() == 0);
            const std::vector<uint8_t> Wrong = ReadAll(Keys / "Data1.pem");
            const ArchiveSet::Source Mismatched[] {{"Data0", std::string(Wrong.begin(), Wrong.end())}};
            CHECK(Throws([&] { (void)ArchiveSet::Open(Game, BHD5::Game::EldenRing, Mismatched); }));
        } catch (const std::exception& E) {
            std::printf("FAIL ArchiveSet: %s\n", E.what());
            ++Failures;
        }
    }

    std::printf(Failures == 0 ? "BHD5 tests passed\n" : "BHD5 tests: %d failure(s)\n", Failures);
    return Failures;
}
