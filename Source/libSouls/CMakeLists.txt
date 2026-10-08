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
        Formats/BHD5.cpp
        Formats/BHD5.hpp
        Formats/DCX.cpp
        Formats/DCX.hpp
        Binders/Binder.cpp
        Binders/Binder.hpp
        Binders/BinderFile.hpp
        Binders/BinderFileHeader.cpp
        Binders/BinderFileHeader.hpp
        Binders/BinderHashTable.cpp
        Binders/BinderHashTable.hpp
        Binders/IBinder.hpp
        Binders/BND3.cpp
        Binders/BND3.hpp
        Binders/BND4.cpp
        Binders/BND4.hpp
        Binders/BXF3.cpp
        Binders/BXF3.hpp
        Binders/BXF4.cpp
        Binders/BXF4.hpp
        Binders/Regulation.cpp
        Binders/Regulation.hpp
)

target_link_libraries(Souls PRIVATE zlibstatic libzstd_static bcrypt)
# zconf.h is generated into zlib's build dir, zlib.h lives in its source dir.
target_include_directories(Souls PRIVATE ${CMAKE_SOURCE_DIR}/Source ${zlib_SOURCE_DIR} ${zlib_BINARY_DIR} ${zstd_SOURCE_DIR}/lib)

target_compile_definitions(Souls PRIVATE
        LIBSOULS_EXPORTS
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
)
