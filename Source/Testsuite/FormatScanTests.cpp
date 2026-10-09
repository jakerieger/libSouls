//
// Created by Jake Rieger on 10/8/2026.
//

// Reads, rewrites and re-reads a sample of real files for every format that has some in the game installs. A format
// passes when every sample reads, rewrites, and the rewrite reads and rewrites to the same bytes; byte-identical
// rewrites of the original are counted and reported but are not required (some formats have unrecoverable padding or
// ordering choices).

#include "TestResult.hpp"

#include <libSouls/Binders/BND3.hpp>
#include <libSouls/Crypto.hpp>
#include <libSouls/Formats/BHD5.hpp>
#include <libSouls/Binders/BND4.hpp>
#include <libSouls/Formats/ACB.hpp>
#include <libSouls/Formats/BTAB.hpp>
#include <libSouls/Formats/BTL.hpp>
#include <libSouls/Formats/BTPB.hpp>
#include <libSouls/Formats/CCM.hpp>
#include <libSouls/Formats/CLM2.hpp>
#include <libSouls/Formats/EDD.hpp>
#include <libSouls/Formats/ESD.hpp>
#include <libSouls/Formats/GPARAM.hpp>
#include <libSouls/Formats/NGP.hpp>
#include <libSouls/Formats/NVA.hpp>
#include <libSouls/Formats/EDGE.hpp>
#include <libSouls/Formats/EMELD.hpp>
#include <libSouls/Formats/EMEVD.hpp>
#include <libSouls/Formats/ENFL.hpp>
#include <libSouls/Formats/F2TR.hpp>
#include <libSouls/Formats/DRB.hpp>
#include <libSouls/Formats/FMB.hpp>
#include <libSouls/Formats/MQB.hpp>
#include <libSouls/Formats/GRASS.hpp>
#include <libSouls/Formats/LUAGNL.hpp>
#include <libSouls/Formats/LUAINFO.hpp>
#include <libSouls/Formats/MATBIN.hpp>
#include <libSouls/Formats/MCG.hpp>
#include <libSouls/Formats/MCP.hpp>
#include <libSouls/Formats/MTD.hpp>
#include <libSouls/Formats/FXR3.hpp>
#include <libSouls/Formats/MSB/MSB1.hpp>
#include <libSouls/Formats/MSB/MSB3.hpp>
#include <libSouls/Formats/MSB/MSBS.hpp>
#include <libSouls/Formats/TAE3.hpp>
#include <libSouls/Formats/NVM.hpp>
#include <libSouls/Formats/PMDCL.hpp>
#include <libSouls/Formats/RMB.hpp>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace {
    namespace fs = std::filesystem;
    using namespace Souls;

    const fs::path Steam = "C:/Program Files (x86)/Steam/steamapps/common";

    std::string Lower(std::string Text) {
        std::transform(Text.begin(), Text.end(), Text.begin(), [](unsigned char C) { return static_cast<char>(std::tolower(C)); });
        return Text;
    }

    std::string StripDcx(const std::string& LowerName) {
        return LowerName.size() > 4 && LowerName.compare(LowerName.size() - 4, 4, ".dcx") == 0 ? LowerName.substr(0, LowerName.size() - 4) : LowerName;
    }

    bool HasSuffix(const std::string& Text, const std::string& Suffix) {
        return Text.size() >= Suffix.size() && Text.compare(Text.size() - Suffix.size(), Suffix.size(), Suffix) == 0;
    }

    bool MatchesAny(const std::string& Name, const std::vector<std::string>& Suffixes) {
        const std::string Plain = StripDcx(Lower(Name));
        for (const std::string& Suffix : Suffixes) {
            if (HasSuffix(Plain, Suffix)) return true;
        }
        return false;
    }

    std::vector<uint8_t> ReadFile(const fs::path& Path) {
        std::ifstream In(Path, std::ios::binary);
        return std::vector<uint8_t>((std::istreambuf_iterator<char>(In)), std::istreambuf_iterator<char>());
    }

    // Where to look: loose files with these extensions, and files with these extensions inside binders with the
    // given extensions.
    struct Scan {
        std::vector<std::string> Loose;
        std::vector<std::string> BinderExts;
        std::vector<std::string> Inner;
        // Game folders to look in (empty for all of them).
        std::vector<std::string> Games;
        // Files whose path contains any of these are skipped (known-bad data such as developer test files).
        std::vector<std::string> Ignore;
        // Also look in the packed archives of Dark Souls III and Sekiro (the games' BHD5/BDT files).
        bool Archives = false;
    };

    struct Sample {
        std::string Origin;
        std::vector<uint8_t> Bytes;
    };

    template<typename T>
    std::vector<T> Spread(std::vector<T> All, size_t Max) {
        if (All.size() <= Max) return All;
        std::vector<T> Out;
        const double Step = static_cast<double>(All.size()) / static_cast<double>(Max);
        for (size_t I = 0; I < Max; ++I) Out.push_back(All[static_cast<size_t>(static_cast<double>(I) * Step)]);
        return Out;
    }

    bool Ignored(const Scan& Where, const std::string& Text) {
        for (const std::string& Part : Where.Ignore) {
            if (Text.find(Part) != std::string::npos) return true;
        }
        return false;
    }

    std::vector<Sample> Collect(const Scan& Where, size_t Max) {
        std::vector<Sample> Samples;
        for (const char* Game : {"DARK SOULS REMASTERED", "ELDEN RING/Game", "DARK SOULS III/Game", "Sekiro"}) {
            const fs::path Root = Steam / Game;
            if (!fs::exists(Root)) continue;
            if (!Where.Games.empty() && std::find(Where.Games.begin(), Where.Games.end(), std::string(Game)) == Where.Games.end()) continue;

            std::vector<fs::path> Loose, Binders;
            std::error_code Ec;
            for (fs::recursive_directory_iterator It(Root, fs::directory_options::skip_permission_denied, Ec), End; It != End; It.increment(Ec)) {
                if (Ec) break;
                if (!It->is_regular_file(Ec)) continue;
                const std::string Name = It->path().filename().string();
                if (!Where.Loose.empty() && MatchesAny(Name, Where.Loose)) Loose.push_back(It->path());
                if (!Where.BinderExts.empty() && MatchesAny(Name, Where.BinderExts)) Binders.push_back(It->path());
            }
            std::sort(Loose.begin(), Loose.end());
            std::sort(Binders.begin(), Binders.end());

            Loose.erase(std::remove_if(Loose.begin(), Loose.end(), [&](const fs::path& P) { return Ignored(Where, P.string()); }), Loose.end());
            for (const fs::path& Path : Spread(Loose, Max)) {
                Samples.push_back({Path.string(), DCX::Is(ReadFile(Path)) ? DCX::Decompress(ReadFile(Path)) : ReadFile(Path)});
            }

            size_t Inner = 0;
            for (const fs::path& Path : Spread(Binders, Max)) {
                if (Inner >= Max * 2) break;
                try {
                    std::vector<BinderFile> Files;
                    if (BND4::Is(Path)) {
                        Files = BND4::Read(Path).Files;
                    } else if (BND3::Is(Path)) {
                        Files = BND3::Read(Path).Files;
                    }
                    for (BinderFile& File : Files) {
                        if (File.Name && MatchesAny(*File.Name, Where.Inner) && Inner < Max * 2) {
                            Samples.push_back({Path.string() + "|" + *File.Name, DCX::Is(File.Bytes) ? DCX::Decompress(File.Bytes) : File.Bytes});
                            ++Inner;
                        }
                    }
                } catch (const std::exception&) {
                }
            }
        }
        return Samples;
    }

    struct Options {
        bool CanWrite = true;
        bool HasIs    = true;
        // Recognizes the format among unnamed files from the archives; defaults to Is().
        std::function<bool(const std::vector<uint8_t>&)> Sniff;
    };

    // Returns the number of failures.
    template<typename T>
    int RunSamples(const char* Label, const std::vector<Sample>& Samples, Options Opt) {
        if (Samples.empty()) {
            std::printf("[%s] no sample files found\n", Label);
            return 0;
        }

        int Read = 0, Identical = 0, Failed = 0;
        std::vector<std::string> Notes;
        for (const Sample& S : Samples) {
            const std::string Short = fs::path(S.Origin.substr(0, S.Origin.find('|'))).filename().string() +
                                      (S.Origin.find('|') != std::string::npos ? S.Origin.substr(S.Origin.find('|')) : "");
            try {
                if (Opt.HasIs && !T::Is(S.Bytes)) {
                    {
                    char Magic[32] = {};
                    for (size_t I = 0; I < 8 && I < S.Bytes.size(); ++I) std::snprintf(Magic + I * 3, 4, "%02X ", S.Bytes[I]);
                    throw std::runtime_error(std::string("not recognized by Is() (starts ") + Magic + ")");
                }
                }
                T Value = T::Read(S.Bytes);
                ++Read;
                if (!Opt.CanWrite) continue;

                Value.Compression  = DCX::Type::None;
                const auto Written = Value.Write();
                if (Written == S.Bytes) {
                    ++Identical;
                    continue;
                }
                T Again            = T::Read(Written);
                Again.Compression  = DCX::Type::None;
                const auto Written2 = Again.Write();
                if (Written2 != Written) {
                    throw std::runtime_error("rewrite is not stable");
                }
                if (const char* Dump = std::getenv("SOULS_DUMP"); Dump && Notes.size() < 1) {
                    std::ofstream(fs::path(Dump) / (std::string(Label) + "_orig.bin"), std::ios::binary).write(reinterpret_cast<const char*>(S.Bytes.data()), static_cast<std::streamsize>(S.Bytes.size()));
                    std::ofstream(fs::path(Dump) / (std::string(Label) + "_new.bin"), std::ios::binary).write(reinterpret_cast<const char*>(Written.data()), static_cast<std::streamsize>(Written.size()));
                }
                if (Notes.size() < 3) {
                    size_t At = 0;
                    while (At < Written.size() && At < S.Bytes.size() && Written[At] == S.Bytes[At]) ++At;
                    char Buffer[160];
                    std::snprintf(Buffer, sizeof Buffer, "differs: first at 0x%zX, sizes %zu vs %zu", At, S.Bytes.size(), Written.size());
                    Notes.push_back(Short + " " + Buffer);
                }
            } catch (const std::exception& E) {
                ++Failed;
                if (const char* Dump = std::getenv("SOULS_DUMP"); Dump && Failed == 1) {
                    std::ofstream(fs::path(Dump) / (std::string(Label) + "_fail.bin"), std::ios::binary).write(reinterpret_cast<const char*>(S.Bytes.data()), static_cast<std::streamsize>(S.Bytes.size()));
                }
                if (Notes.size() < 20) Notes.push_back(Short + " FAILED: " + E.what());
            }
        }
        std::printf("[%s] %zu samples: %d read, %d byte-identical, %d failed\n", Label, Samples.size(), Read, Identical, Failed);
        for (const std::string& Note : Notes) std::printf("    %s\n", Note.c_str());
        return Failed;
    }

    struct Registration {
        std::string Label;
        Scan Where;
        std::function<int(const std::vector<Sample>&)> Run;
        std::function<bool(const std::vector<uint8_t>&)> Sniff;
        std::vector<Sample> ArchiveSamples;
    };

    template<typename T>
    void Register(std::vector<Registration>& All, const char* Label, Scan Where, Options Opt = {}) {
        Registration R;
        R.Label = Label;
        R.Where = std::move(Where);
        R.Run   = [Label = std::string(Label), Opt](const std::vector<Sample>& Samples) { return RunSamples<T>(Label.c_str(), Samples, Opt); };
        if (Opt.Sniff) {
            R.Sniff = Opt.Sniff;
        } else if (Opt.HasIs) {
            R.Sniff = [](const std::vector<uint8_t>& Bytes) { return T::Is(Bytes); };
        }
        All.push_back(std::move(R));
    }

    bool WantsGame(const Scan& Where, const std::string& Game) {
        return Where.Games.empty() || std::find(Where.Games.begin(), Where.Games.end(), Game) != Where.Games.end();
    }

    // Reads files out of the packed archives of DS3 and Sekiro and files them under the formats that want them: the
    // contents of binders by name, other files by sniffing.
    void CollectFromArchives(std::vector<Registration*>& Wanted, size_t Max) {
        struct ArchiveGame {
            const char* Dir;
            const char* Exe;
        };
        std::map<std::string, int> ExtCounts;
        const char* EnvFiles        = std::getenv("SOULS_ARCHIVE_FILES");
        const size_t FilesPerHeader = EnvFiles ? static_cast<size_t>(std::strtoull(EnvFiles, nullptr, 10)) : 3000;
        for (const ArchiveGame& Game : {ArchiveGame{"DARK SOULS III/Game", "DarkSoulsIII.exe"}, ArchiveGame{"Sekiro", "sekiro.exe"}}) {
            const fs::path Root = Steam / Game.Dir;
            if (!fs::exists(Root / Game.Exe)) continue;
            std::vector<Registration*> Here;
            for (Registration* R : Wanted) {
                if (WantsGame(R->Where, Game.Dir)) Here.push_back(R);
            }
            if (Here.empty()) continue;

            std::vector<std::string> Keys;
            try {
                Keys = Crypto::FindRsaPublicKeys(ReadFile(Root / Game.Exe));
            } catch (const std::exception&) {
            }
            if (Keys.empty()) {
                std::printf("(no RSA keys found in %s; skipping its archives)\n", Game.Exe);
                continue;
            }

            std::vector<fs::path> Headers;
            for (const auto& Entry : fs::directory_iterator(Root)) {
                if (Lower(Entry.path().extension().string()) == ".bhd") Headers.push_back(Entry.path());
            }
            std::sort(Headers.begin(), Headers.end());

            for (const fs::path& HeaderPath : Headers) {
                fs::path BdtPath = HeaderPath;
                BdtPath.replace_extension(".bdt");
                if (!fs::exists(BdtPath)) continue;

                const std::vector<uint8_t> Encrypted = ReadFile(HeaderPath);
                std::vector<uint8_t> Header;
                for (const std::string& Key : Keys) {
                    try {
                        auto Candidate = Crypto::DecryptRsaBlocks(Key, Encrypted);
                        if (Candidate.size() >= 4 && std::memcmp(Candidate.data(), "BHD5", 4) == 0) {
                            Header = std::move(Candidate);
                            break;
                        }
                    } catch (const std::exception&) {
                    }
                }
                if (Header.empty()) {
                    std::printf("(%s: no key decrypts the header)\n", HeaderPath.filename().string().c_str());
                    continue;
                }

                try {
                    const BHD5 Bhd = BHD5::Read(Header, BHD5::Game::DarkSouls3);
                    BinaryReader Bdt(BdtPath);
                    size_t Read = 0, Binders = 0;
                    for (const auto& Bucket : Bhd.Buckets) {
                        for (const auto& File : Bucket) {
                            if (Read >= FilesPerHeader) break;
                            if (File.PaddedFileSize <= 0 || File.PaddedFileSize > 24 * 1024 * 1024 || File.FileOffset + File.PaddedFileSize > Bdt.Length()) continue;
                            ++Read;

                            std::vector<uint8_t> Bytes;
                            try {
                                Bytes = File.ReadFile(Bdt);
                                if (DCX::Is(Bytes)) Bytes = DCX::Decompress(Bytes);
                            } catch (const std::exception&) {
                                continue;
                            }
                            const std::string Origin = std::string(Game.Dir).substr(0, 4) + ":" + HeaderPath.stem().string();
                            const auto Wants         = [&](Registration* R) { return R->ArchiveSamples.size() < Max * 2; };

                            if (Bytes.size() >= 4 && std::memcmp(Bytes.data(), "BND4", 4) == 0) {
                                ++Binders;
                                try {
                                    BND4 Archive = BND4::Read(Bytes);
                                    for (BinderFile& Inner : Archive.Files) {
                                        if (!Inner.Name) continue;
                                        if (std::getenv("SOULS_EXTS")) ++ExtCounts[fs::path(StripDcx(Lower(*Inner.Name))).extension().string()];
                                        for (Registration* R : Here) {
                                            if (Wants(R) && !R->Where.Inner.empty() && MatchesAny(*Inner.Name, R->Where.Inner)) {
                                                R->ArchiveSamples.push_back({Origin + "|" + *Inner.Name, DCX::Is(Inner.Bytes) ? DCX::Decompress(Inner.Bytes) : Inner.Bytes});
                                            }
                                        }
                                    }
                                } catch (const std::exception&) {
                                }
                            } else {
                                for (Registration* R : Here) {
                                    if (Wants(R) && R->Sniff && Bytes.size() > 8) {
                                        bool Match = false;
                                        try {
                                            Match = R->Sniff(Bytes);
                                        } catch (const std::exception&) {
                                        }
                                        if (Match) {
                                            R->ArchiveSamples.push_back({Origin + "|" + std::to_string(File.FileNameHash), Bytes});
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    std::printf("(%s: %zu files read, %zu binders)\n", HeaderPath.filename().string().c_str(), Read, Binders);
                } catch (const std::exception& E) {
                    std::printf("(%s: %s)\n", HeaderPath.filename().string().c_str(), E.what());
                }
            }
        }
        for (const auto& [Ext, Count] : ExtCounts) std::printf("  inner %s x%d\n", Ext.c_str(), Count);
    }
}  // namespace

int RunFormatScanTests(const std::string& Filter, size_t Max) {
    if (!fs::exists(Steam / "DARK SOULS REMASTERED") && !fs::exists(Steam / "ELDEN RING/Game")) {
        std::printf("No game installs found; skipping\n");
        return TestSkipped;
    }

    std::vector<Registration> All;
    const Options Plain;
    const Options NoIs{true, false};

    Register<LUAGNL>(All, "LUAGNL", Scan{{".luagnl"}, {".luabnd"}, {".luagnl"}}, NoIs);
    Register<LUAINFO>(All, "LUAINFO", Scan{{".luainfo"}, {".luabnd"}, {".luainfo"}}, Plain);
    Register<BTAB>(All, "BTAB", Scan{{".btab"}, {".btabbnd"}, {".btab"}}, NoIs);
    Register<MCP>(All, "MCP", Scan{{".mcp"}, {}, {}}, NoIs);
    Register<MCG>(All, "MCG", Scan{{".mcg"}, {}, {}}, NoIs);
    Register<NVM>(All, "NVM", Scan{{".nvm"}, {".nvmbnd"}, {".nvm"}}, NoIs);
    Register<CCM>(All, "CCM", Scan{{".ccm"}, {}, {}}, NoIs);
    Register<ENFL>(All, "ENFL", Scan{{".entryfilelist"}, {}, {}}, Plain);
    Register<EMEVD>(All, "EMEVD", Scan{{".emevd", ".evd"}, {}, {}}, Plain);
    Register<EMELD>(All, "EMELD", Scan{{".emeld", ".eld"}, {}, {}}, Plain);
    Register<BTL>(All, "BTL", Scan{{".btl"}, {".btlbnd"}, {".btl"}}, NoIs);
    Register<BTPB>(All, "BTPB", Scan{{".btpb"}, {".btpbnd", ".btpbbnd"}, {".btpb"}}, NoIs);
    Register<CLM2>(All, "CLM2", Scan{{".clm"}, {}, {".clm"}}, Plain);
    Register<F2TR>(All, "F2TR", Scan{{".flver2tri"}, {}, {".flver2tri"}}, Plain);
    Register<PMDCL>(All, "PMDCL", Scan{{".pmdcl"}, {}, {".pmdcl"}}, NoIs);
    Register<RMB>(All, "RMB", Scan{{".rmb"}, {".rumblebnd"}, {".rmb"}}, NoIs);
    Register<GRASS>(All, "GRASS", Scan{{".grass"}, {}, {".grass"}}, Plain);
    Register<EDGE>(All, "EDGE", Scan{{".edge"}, {}, {".edge"}}, NoIs);
    Register<MATBIN>(All, "MATBIN", Scan{{".matbin"}, {".matbinbnd"}, {".matbin"}}, Plain);
    Register<MTD>(All, "MTD", Scan{{".mtd"}, {".mtdbnd"}, {".mtd"}}, Plain);
    Register<ESD>(All, "ESD", Scan{{".esd"}, {".talkesdbnd", ".menuesdbnd", ".chresdbnd"}, {".esd"}}, Plain);
    Register<EDD>(All, "EDD", Scan{{".edd"}, {}, {".edd"}}, Options{false, true});
    // Elden Ring changed the GPARAM and NVA layouts; the Elden Ring variants aren't supported yet.
    Register<GPARAM>(All, "GPARAM", Scan{{".gparam", ".fltparam"}, {".gparambnd"}, {".gparam", ".fltparam"}, {"DARK SOULS REMASTERED", "DARK SOULS III/Game", "Sekiro"}, {}, true}, Plain);
    Register<ACB>(All, "ACB", Scan{{".acb"}, {}, {".acb"}}, Plain);
    Register<NGP>(All, "NGP", Scan{{".ngp"}, {}, {".ngp"}}, Plain);
    Register<NVA>(All, "NVA", Scan{{".nva"}, {".nvabnd"}, {".nva"}, {"DARK SOULS REMASTERED", "DARK SOULS III/Game", "Sekiro"}, {}, true}, Plain);
    Register<MSB1>(All, "MSB1", Scan{{".msb"}, {}, {}, {"DARK SOULS REMASTERED"}, {"m99_"}}, NoIs);
    Register<DRB>(All, "DRB", Scan{{".drb"}, {}, {}, {"DARK SOULS REMASTERED"}}, Plain);
    Register<MQB>(All, "MQB", Scan{{".mqb"}, {".remobnd"}, {".mqb"}}, Plain);
    Register<FMB>(All, "FMB", Scan{{".expb"}, {}, {}}, Plain);
    // Packed games: files come out of the archives (their names aren't known, so unnamed files are sniffed).
    Register<MSBS>(All, "MSBS", Scan{{}, {}, {}, {"Sekiro"}, {}, true}, Plain);
    Register<MSB3>(All, "MSB3", Scan{{}, {}, {}, {"DARK SOULS III/Game"}, {}, true}, Plain);
    // Sekiro and Elden Ring use newer TAE versions than upstream handles.
    Register<TAE3>(All, "TAE3", Scan{{}, {}, {".tae"}, {"DARK SOULS III/Game"}, {}, true}, Plain);
    Register<FXR3>(All, "FXR3", Scan{{}, {}, {".fxr"}, {"DARK SOULS III/Game", "Sekiro"}, {}, true}, Plain);

    std::vector<Registration*> Selected;
    for (Registration& R : All) {
        if (Filter.empty() || Filter == "all" || Filter == R.Label) Selected.push_back(&R);
    }
    if (Selected.empty()) {
        std::printf("Unknown format \"%s\"\n", Filter.c_str());
        return 1;
    }

    std::vector<Registration*> NeedArchives;
    for (Registration* R : Selected) {
        if (R->Where.Archives) NeedArchives.push_back(R);
    }
    if (!NeedArchives.empty()) CollectFromArchives(NeedArchives, Max);

    int Failures = 0;
    for (Registration* R : Selected) {
        const bool ArchiveOnly = R->Where.Archives && R->Where.Loose.empty() && R->Where.BinderExts.empty();
        std::vector<Sample> Samples = ArchiveOnly ? std::vector<Sample>() : Collect(R->Where, Max);
        Samples.insert(Samples.end(), std::make_move_iterator(R->ArchiveSamples.begin()), std::make_move_iterator(R->ArchiveSamples.end()));
        Failures += R->Run(Samples);
    }
    std::printf(Failures == 0 ? "Format scan passed\n" : "Format scan: %d failure(s)\n", Failures);
    return Failures;
}
