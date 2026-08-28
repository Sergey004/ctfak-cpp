#include "ZlibLib.h"

#include <stdexcept>
#include <string>
#include <vector>

#if defined(CTFAKPP_LINK_ZLIB)
#include <zlib.h>
#elif defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace ctfak {

#if defined(CTFAKPP_LINK_ZLIB)

// ---------------------------------------------------------------- link mode
// Our zlibabi::ZStream is layout-identical to the linked zlib's z_stream on
// this platform; verify that before punning between the two.

static_assert(sizeof(zlibabi::ZStream) == sizeof(z_stream), "z_stream layout mismatch");
static_assert(offsetof(zlibabi::ZStream, next_in) == offsetof(z_stream, next_in));
static_assert(offsetof(zlibabi::ZStream, avail_in) == offsetof(z_stream, avail_in));
static_assert(offsetof(zlibabi::ZStream, total_in) == offsetof(z_stream, total_in));
static_assert(offsetof(zlibabi::ZStream, next_out) == offsetof(z_stream, next_out));
static_assert(offsetof(zlibabi::ZStream, avail_out) == offsetof(z_stream, avail_out));
static_assert(offsetof(zlibabi::ZStream, total_out) == offsetof(z_stream, total_out));
static_assert(offsetof(zlibabi::ZStream, msg) == offsetof(z_stream, msg));
static_assert(offsetof(zlibabi::ZStream, state) == offsetof(z_stream, state));
static_assert(offsetof(zlibabi::ZStream, zalloc) == offsetof(z_stream, zalloc));
static_assert(offsetof(zlibabi::ZStream, zfree) == offsetof(z_stream, zfree));
static_assert(offsetof(zlibabi::ZStream, opaque) == offsetof(z_stream, opaque));
static_assert(offsetof(zlibabi::ZStream, data_type) == offsetof(z_stream, data_type));
static_assert(offsetof(zlibabi::ZStream, adler) == offsetof(z_stream, adler));
static_assert(offsetof(zlibabi::ZStream, reserved) == offsetof(z_stream, reserved));

const ZlibApi& zlibApi() {
    static const ZlibApi api = {
        [] { return zlibVersion(); },
        [](zlibabi::ZStream* s, const char* v, int n) {
            return inflateInit_((z_streamp)s, v, n);
        },
        [](zlibabi::ZStream* s, int f) { return inflate((z_streamp)s, f); },
        [](zlibabi::ZStream* s) { return inflateEnd((z_streamp)s); },
        [](zlibabi::ZStream* s, int l, const char* v, int n) {
            return deflateInit_((z_streamp)s, l, v, n);
        },
        [](zlibabi::ZStream* s, int f) { return deflate((z_streamp)s, f); },
        [](zlibabi::ZStream* s) { return deflateEnd((z_streamp)s); },
        [](zlibabi::ZStream* s, zlibabi::ULong n) {
            return deflateBound((z_streamp)s, n);
        },
        [](zlibabi::ULong c, const zlibabi::Byte* b, zlibabi::UInt n) {
            return crc32((uLong)c, b, n);
        },
    };
    return api;
}

uint32_t zlibCrc32(uint32_t crc, const uint8_t* data, size_t len) {
    return static_cast<uint32_t>(
        crc32(static_cast<uLong>(crc), data, static_cast<uInt>(len)));
}

#else

// ------------------------------------------------------------- runtime mode

namespace {

using LibraryHandle = void*;

const char* kDefaultCandidates[] = {
#if defined(_WIN32)
    "zlibwapi.dll", // official prebuilt zlib (what the original tool ships)
    "zlib1.dll",    // MinGW / vcpkg convention
    "zlibd1.dll",   // debug builds
#else
    "libz.so.1",
    "libz.so",
    "libz.dylib",
#endif
};

LibraryHandle openLibrary(const char* name) {
#if defined(_WIN32)
    return (LibraryHandle)LoadLibraryA(name);
#else
    return dlopen(name, RTLD_NOW | RTLD_LOCAL);
#endif
}

void* resolve(LibraryHandle lib, const char* name) {
#if defined(_WIN32)
    return (void*)GetProcAddress((HMODULE)lib, name);
#else
    return dlsym(lib, name);
#endif
}

// Symbol table: entry name -> target field offset within ZlibApi.
struct SymbolEntry {
    const char* name;
    size_t offset;
};

const SymbolEntry kSymbols[] = {
    {"zlibVersion", offsetof(ZlibApi, zlibVersion)},
    {"inflateInit_", offsetof(ZlibApi, inflateInit_)},
    {"inflate", offsetof(ZlibApi, inflate)},
    {"inflateEnd", offsetof(ZlibApi, inflateEnd)},
    {"deflateInit_", offsetof(ZlibApi, deflateInit_)},
    {"deflate", offsetof(ZlibApi, deflate)},
    {"deflateEnd", offsetof(ZlibApi, deflateEnd)},
    {"deflateBound", offsetof(ZlibApi, deflateBound)},
    {"crc32", offsetof(ZlibApi, crc32)},
};

// Builds the failure message listing everything that was attempted.
std::string buildLoadError(const std::vector<std::string>& attempted) {
    std::string msg =
        "Failed to load a zlib runtime library.\n"
        "Place zlibwapi.dll next to the executable (the same prebuilt DLL the "
        "original tool ships in its requirements bundle), or set the CTFAK_ZLIB_DLL "
        "environment variable to its full path.\n"
        "Tried: ";
    for (size_t i = 0; i < attempted.size(); ++i) {
        if (i) msg += ", ";
        msg += attempted[i];
    }
    return msg;
}

ZlibApi loadZlibApi() {
    ZlibApi api{};

    std::vector<std::string> attempted;

    std::vector<const char*> candidates;
    const char* env = getenv("CTFAK_ZLIB_DLL");
    if (env && *env) candidates.push_back(env);
    for (const char* c : kDefaultCandidates) candidates.push_back(c);

    LibraryHandle lib = nullptr;
    for (const char* name : candidates) {
        lib = openLibrary(name);
        attempted.push_back(name);
        if (lib) break;
    }

#if !defined(_WIN32)
    if (!lib) {
        // The process may already carry zlib (linked by the host or another
        // dependency) — probe the global symbol scope before failing.
        if (dlsym(RTLD_DEFAULT, "inflate")) {
            for (const SymbolEntry& sym : kSymbols) {
                *reinterpret_cast<void**>(reinterpret_cast<char*>(&api) + sym.offset) =
                    dlsym(RTLD_DEFAULT, sym.name);
            }
            return api;
        }
        attempted.push_back("dlsym(RTLD_DEFAULT)");
    }
#endif

    if (!lib) throw std::runtime_error(buildLoadError(attempted));

    for (const SymbolEntry& sym : kSymbols) {
        void* fn = resolve(lib, sym.name);
        if (!fn) {
            throw std::runtime_error(std::string("Loaded zlib runtime library is missing ") +
                                     sym.name + " - incompatible library?");
        }
        *reinterpret_cast<void**>(reinterpret_cast<char*>(&api) + sym.offset) = fn;
    }

    return api;
}

} // namespace

const ZlibApi& zlibApi() {
    // Function-local static: initialization is guaranteed to run exactly once,
    // and a thrown error is retried on the next call.
    static const ZlibApi& ref = loadZlibApi();
    return ref;
}

uint32_t zlibCrc32(uint32_t crc, const uint8_t* data, size_t len) {
    return static_cast<uint32_t>(
        zlibApi().crc32(static_cast<zlibabi::ULong>(crc), data,
                        static_cast<zlibabi::UInt>(len)));
}

#endif // CTFAKPP_LINK_ZLIB

} // namespace ctfak
