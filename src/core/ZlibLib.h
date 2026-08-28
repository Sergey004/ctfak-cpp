// Runtime zlib loading, mirroring the original tool's distribution model:
// the executable needs no build-time zlib at all — a zlib runtime library
// (zlibwapi.dll on Windows, libz.so on Linux) is located and bound on first use.
//
// An alternative compile-time binding is available: define CTFAKPP_LINK_ZLIB
// (CMake option CTFAKPP_LINK_ZLIB=ON) to link zlib normally instead.
//
// The z_stream structure below is a layout-compatible redefinition of the
// public zlib stream descriptor (see zlib.h). Field types match the platform's
// zlib build: on Windows (LLP64) uLong is 32-bit, on Linux/macOS (LP64) it is
// 64-bit. In link mode, compatibility with <zlib.h> is verified at compile time.
#pragma once

#include <cstddef>
#include <cstdint>

namespace ctfak::zlibabi {

#if defined(_WIN32)
using ULong = unsigned int;    // zlib uLong under LLP64
#else
using ULong = unsigned long;   // zlib uLong under LP64
#endif
using UInt = unsigned int;     // zlib uInt
using Byte = unsigned char;    // zlib Bytef

// Layout-compatible z_stream (must match sizeof/offsets of the target library).
struct ZStream {
    const Byte* next_in;
    UInt avail_in;
    ULong total_in;

    Byte* next_out;
    UInt avail_out;
    ULong total_out;

    const char* msg;
    void* state;

    void* (*zalloc)(void*, UInt, UInt);
    void (*zfree)(void*, void*);
    void* opaque;

    int data_type;
    ULong adler;
    ULong reserved;
};

} // namespace ctfak::zlibabi

namespace ctfak {

// Function table resolved from the runtime zlib library (or bound at compile
// time when CTFAKPP_LINK_ZLIB is defined).
struct ZlibApi {
    const char* (*zlibVersion)();
    int (*inflateInit_)(zlibabi::ZStream*, const char*, int);
    int (*inflate)(zlibabi::ZStream*, int);
    int (*inflateEnd)(zlibabi::ZStream*);
    int (*deflateInit_)(zlibabi::ZStream*, int, const char*, int);
    int (*deflate)(zlibabi::ZStream*, int);
    int (*deflateEnd)(zlibabi::ZStream*);
    zlibabi::ULong (*deflateBound)(zlibabi::ZStream*, zlibabi::ULong);
    zlibabi::ULong (*crc32)(zlibabi::ULong, const zlibabi::Byte*, zlibabi::UInt);
};

// Resolved zlib API. Loads the runtime library on first use (runtime mode) or
// returns compile-time-bound entry points (link mode); throws
// std::runtime_error with an actionable message when no library can be bound.
const ZlibApi& zlibApi();

// CRC-32 convenience wrapper over the bound library.
uint32_t zlibCrc32(uint32_t crc, const uint8_t* data, size_t len);

} // namespace ctfak
