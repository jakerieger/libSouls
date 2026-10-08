add_executable(Testsuite
        main.cpp
        TestResult.hpp
        BinaryTests.cpp
        BHD5Tests.cpp
        BinderTests.cpp
        BinderFileTests.cpp
        OodleTests.cpp
        RealFileTests.cpp
        SoulsFileTests.cpp
)

target_link_libraries(Testsuite PRIVATE Souls)
target_include_directories(Testsuite PRIVATE ${CMAKE_SOURCE_DIR}/Source)

# One CTest entry per suite. Exit code 77 means "skipped" (no Oodle DLL / no game install).
function(souls_add_test Name Suite)
    add_test(NAME ${Name} COMMAND Testsuite ${Suite} ${ARGN})
    set_tests_properties(${Name} PROPERTIES SKIP_RETURN_CODE 77)
endfunction()

souls_add_test(Binary binary)
souls_add_test(BHD5 bhd5)
souls_add_test(Binder binder)
souls_add_test(Oodle oodle)
souls_add_test(SoulsFile soulsfile)
souls_add_test(Regulation regulation)
souls_add_test(BinderFiles binderfiles 40)
souls_add_test(RealFiles realfiles 25)

# These need the Oodle DLL and/or an unpacked game install.
set_tests_properties(BHD5 Oodle SoulsFile Regulation BinderFiles RealFiles PROPERTIES LABELS "integration")
set_tests_properties(RealFiles PROPERTIES TIMEOUT 1200)
