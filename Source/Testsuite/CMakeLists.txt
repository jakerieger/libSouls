add_executable(Testsuite
        main.cpp
        TestResult.hpp
        BinaryTests.cpp
        BHD5Tests.cpp
        BinderTests.cpp
        FMGTests.cpp
        PARAMTests.cpp
        PARAMDEFTests.cpp
        PARAMDEFXmlTests.cpp
        ParamLayoutTests.cpp
        ParamDefRepositoryTests.cpp
        ParamdexTests.cpp
        TPFTests.cpp
        BXFReaderTests.cpp
        FLVERTests.cpp
        FormatScanTests.cpp
        SyntheticFormatTests.cpp
        CApiTests.cpp
        CApiSmoke.c
        BinderFileTests.cpp
        OodleTests.cpp
        RealFileTests.cpp
        SoulsFileTests.cpp
)

target_link_libraries(Testsuite PRIVATE Souls)
if(MSVC)
    # TAE3Events.hpp holds a huge variant of event types.
    target_compile_options(Testsuite PRIVATE /bigobj)
endif()

# One CTest entry per suite. Exit code 77 means "skipped" (no Oodle DLL / no game install).
function(souls_add_test Name Suite)
    add_test(NAME ${Name} COMMAND Testsuite ${Suite} ${ARGN})
    set_tests_properties(${Name} PROPERTIES SKIP_RETURN_CODE 77)
endfunction()

souls_add_test(Binary binary)
souls_add_test(BHD5 bhd5)
souls_add_test(Binder binder)
souls_add_test(FMG fmg 30)
souls_add_test(PARAM param)
souls_add_test(PARAMDEF paramdef)
souls_add_test(PARAMDEFXml paramdefxml)
souls_add_test(ParamLayout paramlayout)
souls_add_test(ParamDefRepository paramdefrepo)
souls_add_test(Paramdex paramdex)
souls_add_test(TPF tpf 100)
souls_add_test(BXFReader bxfreader)
souls_add_test(FLVER flver 40)
souls_add_test(Formats formats all 30)
souls_add_test(Synthetic synthetic)
souls_add_test(CApi capi)
souls_add_test(Oodle oodle)
souls_add_test(SoulsFile soulsfile)
souls_add_test(Regulation regulation)
souls_add_test(BinderFiles binderfiles 40)
souls_add_test(RealFiles realfiles 25)

# These need the Oodle DLL and/or an unpacked game install.
set_tests_properties(BHD5 FMG PARAM PARAMDEF PARAMDEFXml ParamLayout ParamDefRepository Paramdex TPF BXFReader FLVER Formats Oodle SoulsFile Regulation BinderFiles RealFiles PROPERTIES LABELS "integration")
set_tests_properties(RealFiles PROPERTIES TIMEOUT 1200)
