#include "Zlib.h"
#include "ZlibLib.h"
#include <stdexcept>
#include <string>     // std::to_string (MSVC does not leak it via <stdexcept>)
#include <cstring>

namespace ctfak {

std::vector<uint8_t> zlibInflate(const uint8_t* data, size_t size, size_t hintSize) {
    std::vector<uint8_t> out;
    out.reserve(hintSize ? hintSize : size * 4);
    const ZlibApi& z = zlibApi();
    zlibabi::ZStream zs{};
    if (z.inflateInit_(&zs, z.zlibVersion(), static_cast<int>(sizeof(zlibabi::ZStream))) != 0)
        throw std::runtime_error("inflateInit failed");
    zs.next_in = data;
    zs.avail_in = static_cast<zlibabi::UInt>(size);
    int ret = 0;
    std::vector<uint8_t> chunk(65536);
    do {
        zs.next_out = chunk.data();
        zs.avail_out = static_cast<zlibabi::UInt>(chunk.size());
        ret = z.inflate(&zs, 0 /* Z_NO_FLUSH */);
        if (ret != 0 /* Z_OK */ && ret != 1 /* Z_STREAM_END */ && ret != -5 /* Z_BUF_ERROR */) {
            z.inflateEnd(&zs);
            throw std::runtime_error("inflate failed with code " + std::to_string(ret));
        }
        out.insert(out.end(), chunk.data(), chunk.data() + (chunk.size() - zs.avail_out));
        if (ret == -5 && zs.avail_in == 0) break;
    } while (ret != 1 /* Z_STREAM_END */);
    z.inflateEnd(&zs);
    return out;
}

std::vector<uint8_t> zlibInflate(const std::vector<uint8_t>& data, size_t hintSize) {
    return zlibInflate(data.data(), data.size(), hintSize);
}

std::vector<uint8_t> zlibDeflate(const uint8_t* data, size_t size, int level) {
    const ZlibApi& z = zlibApi();
    zlibabi::ZStream zs{};
    if (z.deflateInit_(&zs, level, z.zlibVersion(), static_cast<int>(sizeof(zlibabi::ZStream))) != 0)
        throw std::runtime_error("deflateInit failed");
    zs.next_in = data;
    zs.avail_in = static_cast<zlibabi::UInt>(size);
    std::vector<uint8_t> out(z.deflateBound(&zs, static_cast<zlibabi::ULong>(size)));
    zs.next_out = out.data();
    zs.avail_out = static_cast<zlibabi::UInt>(out.size());
    int ret = z.deflate(&zs, 4 /* Z_FINISH */);
    z.deflateEnd(&zs);
    if (ret != 1 /* Z_STREAM_END */)
        throw std::runtime_error("deflate failed");
    out.resize(zs.total_out);
    return out;
}

std::vector<uint8_t> zlibDeflate(const std::vector<uint8_t>& data, int level) {
    return zlibDeflate(data.data(), data.size(), level);
}

} // namespace ctfak
