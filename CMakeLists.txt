cmake_minimum_required(VERSION 3.16)
project(ctfak-cpp VERSION 1.4.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# zlib is ALWAYS linked at build time now: the system one when CMake can see
# it, otherwise the vendored zlib 1.3.1 in third_party/zlib — no runtime DLL
# hunt on any machine (the old model crashed the MSVC build on a foreign
# zlib1.dll from PATH and then on a machine with no zlib DLL at all).
# FORCEd so existing build caches flip automatically; to debug the old
# runtime-load path, edit this line (the loader code is kept).
find_package(ZLIB QUIET)
set(CTFAKPP_LINK_ZLIB ON CACHE BOOL
    "Link zlib at build time (system or vendored) instead of loading it at runtime" FORCE)
if(CTFAKPP_LINK_ZLIB)
    if(ZLIB_FOUND)
        set(CTFAK_ZLIB_TARGET ZLIB::ZLIB)
        message(STATUS "ctfak: linking system zlib")
    else()
        # Vendored zlib 1.3.1 (Zlib license, see third_party/zlib/LICENSE).
        # Minimal set: inflate/deflate/crc32 — no gz* file API needed.
        set(CTFAK_ZLIB_SOURCES
            third_party/zlib/adler32.c
            third_party/zlib/crc32.c
            third_party/zlib/deflate.c
            third_party/zlib/infback.c
            third_party/zlib/inffast.c
            third_party/zlib/inflate.c
            third_party/zlib/inftrees.c
            third_party/zlib/trees.c
            third_party/zlib/zutil.c)
        add_library(ctfak_zlib STATIC ${CTFAK_ZLIB_SOURCES})
        target_include_directories(ctfak_zlib PUBLIC third_party/zlib)
        if(MSVC)
            target_compile_definitions(ctfak_zlib PRIVATE _CRT_SECURE_NO_WARNINGS)
        endif()
        set(CTFAK_ZLIB_TARGET ctfak_zlib)
        message(STATUS "ctfak: building vendored zlib 1.3.1")
    endif()
endif()

set(CTFAK_LIB_SOURCES
    src/core/BinaryReader.cpp
    src/core/Logger.cpp
    src/core/Settings.cpp
    src/core/Zlib.cpp
    src/core/ZlibLib.cpp
    src/core/Crypto.cpp
    src/core/LZ4.cpp
    src/core/BitDict.cpp
    src/io/PngWriter.cpp
    src/io/JsonWriter.cpp
    src/io/WavWriter.cpp
    src/util/Utils.cpp
    src/util/SoundExt.cpp
    src/util/ApkReader.cpp
    src/util/Config.cpp
    src/format/Chunk.cpp
    src/format/ChunkNames.cpp
    src/format/PackData.cpp
    src/format/ExeFile.cpp
    src/format/AppHeader.cpp
    src/format/ExtendedHeader.cpp
    src/format/StringChunks.cpp
    src/format/Extensions.cpp
    src/format/Shaders.cpp
    src/format/Frame.cpp
    src/format/ObjectInfo.cpp
    src/format/ImageBank.cpp
    src/format/ImageDecoder.cpp
    src/format/SoundBank.cpp
    src/format/FontBank.cpp
    src/format/Events.cpp
    src/format/GameData.cpp
    src/plugins/Plugin.cpp
    src/plugins/ImageDumper.cpp
    src/plugins/SoundDumper.cpp
    src/plugins/SortedImageDumper.cpp
    src/plugins/ChunkTrace.cpp
    src/plugins/JsonExport.cpp
    src/plugins/FrameLayout.cpp
    src/plugins/FrameReplay.cpp
    src/plugins/EventsListing.cpp
    src/plugins/DxtEncoder.cpp
    src/plugins/RecompPack.cpp
)

add_library(ctfak_core STATIC ${CTFAK_LIB_SOURCES})
target_include_directories(ctfak_core PUBLIC src)
if(CTFAKPP_LINK_ZLIB)
    target_compile_definitions(ctfak_core PUBLIC CTFAKPP_LINK_ZLIB)
    target_link_libraries(ctfak_core PUBLIC ${CTFAK_ZLIB_TARGET})
else()
    # dlopen/dlsym on POSIX (empty on modern glibc and on Windows).
    target_link_libraries(ctfak_core PUBLIC ${CMAKE_DL_LIBS})
endif()
if(MSVC)
    target_compile_options(ctfak_core PRIVATE /W3 /utf-8)
    target_compile_definitions(ctfak_core PUBLIC _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(ctfak_core PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

add_executable(ctfak-cpp src/main.cpp)
target_link_libraries(ctfak-cpp PRIVATE ctfak_core)

# ---------------------------------------------------------------- tests
enable_testing()
add_executable(ctfak-tests
    tests/test_main.cpp
    tests/test_zlib.cpp
    tests/test_lz4.cpp
    tests/test_crypto.cpp
    tests/test_png.cpp
    tests/test_json.cpp
    tests/test_reader.cpp
    tests/test_layout.cpp
    tests/test_events_listing.cpp
)
target_link_libraries(ctfak-tests PRIVATE ctfak_core)
add_test(NAME unit-tests COMMAND ctfak-tests)
