#include "ZlibLib.h"
#include "Logger.h"

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
        [](zlibabi::ZStream* s, zlibabi::ULong n) -> zlibabi::ULong {
            // zlib's uLong is `unsigned long` (32-bit under LLP64) while our
            // ULong is `unsigned int` — same width, different type, so the
            // conversion must happen INSIDE the lambda (MSVC rejects the
            // function-pointer mismatch).
            return deflateBound((z_streamp)s, n);
        },
        [](zlibabi::ULong c, const zlibabi::Byte* b, zlibabi::UInt n) -> zlibabi::ULong {
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

void closeLibrary(LibraryHandle lib) {
#if defined(_WIN32)
    if (lib) FreeLibrary((HMODULE)lib);
#else
    if (lib) dlclose(lib);
#endif
}

void* resolve(LibraryHandle lib, const char* name) {
#if defined(_WIN32)
    return (void*)GetProcAddress((HMODULE)lib, name);
#else
    return dlsym(lib, name);
#endif
}

#if defined(_WIN32)
// The loaded library's real path (for diagnostics).
static std::string loadedLibraryPath(LibraryHandle lib) {
    char buf[MAX_PATH];
    DWORD n = GetModuleFileNameA((HMODULE)lib, buf, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return std::string();
    return std::string(buf, n);
}

// The directory of the running executable — searched FIRST, so a proper
// zlibwapi.dll / zlib1.dll placed next to ctfak-cpp.exe always wins over
// whatever an old GTK#/app directory put on PATH.
static std::string exeDirectory() {
    char buf[MAX_PATH];
    DWORD n = GetModuleFileNameA(NULL, buf, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return std::string();
    std::string path(buf, n);
    const size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? std::string() : path.substr(0, slash + 1);
}
#endif

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
        "Failed to load a usable zlib runtime library.\n"
        "Place zlibwapi.dll next to the executable (the same prebuilt DLL the "
        "original tool ships in its requirements bundle), or set the CTFAK_ZLIB_DLL "
        "environment variable to its full path.\n"
        "Tried:\n";
    for (size_t i = 0; i < attempted.size(); ++i) {
        msg += "  - " + attempted[i] + "\n";
    }
    return msg;
}

ZlibApi loadZlibApi() {
    ZlibApi api{};

    std::vector<std::string> attempted;

    // Candidate paths: the env override verbatim, then every default name
    // prefixed with the executable's directory, then the bare name (standard
    // search order: application dir, system dirs, CWD, PATH).
    std::vector<std::string> candidates;
    if (const char* env = getenv("CTFAK_ZLIB_DLL"); env && *env) candidates.push_back(env);
#if defined(_WIN32)
    const std::string exeDir = exeDirectory();
#endif
    for (const char* name : kDefaultCandidates) {
#if defined(_WIN32)
        if (!exeDir.empty()) candidates.push_back(exeDir + name);
#endif
        candidates.push_back(name);
    }

    LibraryHandle lib = nullptr;
    for (const std::string& cand : candidates) {
        lib = openLibrary(cand.c_str());
        if (!lib) {
            attempted.push_back(cand + ": not found");
            continue;
        }
        // Validate the WHOLE symbol table before accepting the library — a
        // foreign zlib1.dll on PATH (an old GTK# runtime, an app's private
        // copy) must not win over the next candidate.
        ZlibApi probe{};
        const char* missing = nullptr;
        for (const SymbolEntry& sym : kSymbols) {
            void* fn = resolve(lib, sym.name);
            if (!fn) {
                missing = sym.name;
                break;
            }
            *reinterpret_cast<void**>(reinterpret_cast<char*>(&probe) + sym.offset) = fn;
        }
        if (missing) {
#if defined(_WIN32)
            attempted.push_back(cand + " [" + loadedLibraryPath(lib) + "]: missing " + missing);
#else
            attempted.push_back(cand + ": missing " + missing);
#endif
            closeLibrary(lib);
            lib = nullptr;
            continue;
        }
        api = probe;
#if defined(_WIN32)
        Logger::log("zlib bound: " + cand + " [" + loadedLibraryPath(lib) + "] version " +
                    api.zlibVersion());
#endif
        break;
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
