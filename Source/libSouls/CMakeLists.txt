project(libSouls)

include(FetchContent)

# zlib 1.3.1's CMake predates the minimum policy version newer CMake accepts.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(zlib
        URL https://github.com/madler/zlib/archive/refs/tags/v1.3.1.tar.gz
        URL_HASH SHA256=17e88863f3600672ab49182f217281b6fc4d3c762bde361935e436a95214d05c
        EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(zlib)

set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(ZSTD_LEGACY_SUPPORT OFF CACHE BOOL "" FORCE)
FetchContent_Declare(zstd
        URL https://github.com/facebook/zstd/releases/download/v1.5.6/zstd-1.5.6.tar.gz
        URL_HASH SHA256=8c29e06cf42aacc1eafc4077ae2ec6c6fcb96a626157e0593d5e82a34fd403c1
        SOURCE_SUBDIR build/cmake
        EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(zstd)

FetchContent_Declare(pugixml
        URL https://github.com/zeux/pugixml/releases/download/v1.14/pugixml-1.14.tar.gz
        URL_HASH SHA256=2f10e276870c64b1db6809050a75e11a897a8d7456c4be5c6b2e35a11168a015
        EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(pugixml)

# Formats and binders are picked up automatically; re-run CMake after adding a file.
file(GLOB_RECURSE SOULS_FORMAT_SOURCES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/Formats/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/Formats/*.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Binders/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/Binders/*.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/CApi/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/CApi/*.hpp ${CMAKE_CURRENT_SOURCE_DIR}/CApi/*.h)

add_library(Souls SHARED
        Souls.hpp
        Endian.hpp
        Vector.hpp
        Oodle26.cpp
        Oodle26.hpp
        BinaryWriter.cpp
        BinaryWriter.hpp
        BinaryReader.cpp
        BinaryReader.hpp
        TextEncoding.cpp
        TextEncoding.hpp
        Util.cpp
        Util.hpp
        SoulsFile.cpp
        SoulsFile.hpp
        Crypto.cpp
        Crypto.hpp
        Matrix.hpp
        Color.hpp
        ValuePtr.hpp
        ${SOULS_FORMAT_SOURCES}
)

target_link_libraries(Souls PRIVATE zlibstatic libzstd_static pugixml bcrypt)
# zconf.h is generated into zlib's build dir, zlib.h lives in its source dir.
# Headers are included as <libSouls/...>, relative to the Source folder; consumers get that path too.
target_include_directories(Souls PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_include_directories(Souls PRIVATE ${zlib_SOURCE_DIR} ${zlib_BINARY_DIR} ${zstd_SOURCE_DIR}/lib)

# TAE3 instantiates a template per event type, which needs more object sections than MSVC's default allows.
if (MSVC)
    target_compile_options(Souls PRIVATE /bigobj)
endif ()

target_compile_definitions(Souls PRIVATE
        LIBSOULS_EXPORTS
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
)
