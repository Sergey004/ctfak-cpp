cmake_minimum_required(VERSION 3.16)
project(ctfak-cpp VERSION 1.4.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# By default the build has NO external dependencies: zlib is loaded at runtime
# (zlibwapi.dll / zlib1.dll on Windows, libz.so on Linux), mirroring the
# original tool's distribution model. Set this to ON to link zlib the
# classic way instead.
option(CTFAKPP_LINK_ZLIB "Link zlib at build time instead of loading it at runtime" OFF)
if(CTFAKPP_LINK_ZLIB)
    find_package(ZLIB REQUIRED)
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
    target_link_libraries(ctfak_core PUBLIC ZLIB::ZLIB)
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
