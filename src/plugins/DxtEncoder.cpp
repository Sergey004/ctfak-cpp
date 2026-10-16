// DXT1/DXT5 block compressor implementation. See DxtEncoder.h.
//
// Block layouts (bytes, in file order):
//   DXT1 (8 bytes):  color0:u16be, color1:u16be, indices:u32be
//   DXT5 (16 bytes): a0:u8, a1:u8, alphaIdx:u48be,
//                    color0:u16be, color1:u16be, colorIdx:u32be
//
// Color values are RGB565. In 4-color mode color0 > color1 and the palette is
// {c0, c1, (2*c0+c1)/3, (c0+2*c1)/3}. DXT1 blocks that contain cutout
// transparency use 3-color mode (color0 <= color1): palette {c0, c1,
// (c0+c1)/2, transparent}, transparent pixels get index 3.

#include "DxtEncoder.h"

#include <algorithm>
#include <cstring>

namespace ctfak {

namespace {

constexpr int kAlign = 4;      // DXT block edge
constexpr int kAlphaCut = 128; // alpha below this counts as transparent

uint16_t rgbTo565(int r, int g, int b) {
    return static_cast<uint16_t>(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

void expand565(uint16_t v, int& r, int& g, int& b) {
    r = ((v >> 11) & 0x1F) << 3; r |= r >> 5;
    g = ((v >> 5) & 0x3F) << 2;  g |= g >> 6;
    b = (v & 0x1F) << 3;         b |= b >> 5;
}

uint16_t getU16be(const uint8_t* p) {
    return static_cast<uint16_t>((p[0] << 8) | p[1]);
}

uint32_t getU32be(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
}

uint64_t getU48be(const uint8_t* p) {
    uint64_t v = 0;
    for (int i = 0; i < 6; ++i) v = (v << 8) | p[i];
    return v;
}

struct BlockPixels {
    uint8_t r[16], g[16], b[16], a[16];
    bool hasTransparent = false;
};

void gatherBlock(const uint8_t* rgba, int w, int h, int bx, int by, BlockPixels& px) {
    for (int y = 0; y < 4; ++y) {
        int sy = std::min(by + y, h - 1);
        for (int x = 0; x < 4; ++x) {
            int sx = std::min(bx + x, w - 1);
            const uint8_t* p = rgba + (static_cast<size_t>(sy) * w + sx) * 4;
            int i = y * 4 + x;
            px.r[i] = p[0]; px.g[i] = p[1]; px.b[i] = p[2]; px.a[i] = p[3];
            if (px.a[i] < kAlphaCut) px.hasTransparent = true;
        }
    }
}

// Choose 565 endpoints for the block. Init from the per-channel range (the
// principal-axis stand-in), then run a few classic cluster-fit rounds:
// nearest-palette assignment -> weighted least-squares re-solve of the two
// endpoints -> requantize. This replaces the old luminance-minmax one-shot
// endpoints, which banded hard on the games' dark gradient photos.
// When threeColorWithBlack is true, palette[3] means "transparent" and
// transparent pixels are assigned index 3 (skipped by the fit).
void fitColorBlock(const BlockPixels& px, bool threeColorWithBlack,
                   uint16_t& c0, uint16_t& c1, uint8_t idx[16]) {
    // init: per-channel extremes (the widened bounding diagonal beats a
    // luminance axis as a start for the iteration)
    int minR = 255, maxR = 0, minG = 255, maxG = 0, minB = 255, maxB = 0;
    for (int i = 0; i < 16; ++i) {
        if (threeColorWithBlack && px.a[i] < kAlphaCut) continue;
        minR = std::min(minR, (int)px.r[i]); maxR = std::max(maxR, (int)px.r[i]);
        minG = std::min(minG, (int)px.g[i]); maxG = std::max(maxG, (int)px.g[i]);
        minB = std::min(minB, (int)px.b[i]); maxB = std::max(maxB, (int)px.b[i]);
    }
    c0 = rgbTo565(maxR, maxG, maxB);
    c1 = rgbTo565(minR, minG, minB);

    // weights mapping each palette slot to (1-w) of c0 + w of c1
    const float w4[4] = { 0.0f, 1.0f, 1.0f / 3.0f, 2.0f / 3.0f };
    const float w3[4] = { 0.0f, 1.0f, 0.5f, 0.0f };
    const float* wt = threeColorWithBlack ? w3 : w4;

    for (int iter = 0; iter < 4; ++iter) {
        uint16_t e0 = c0, e1 = c1;
        if (threeColorWithBlack ? (e0 > e1) : (e0 < e1)) std::swap(e0, e1);
        if (!threeColorWithBlack && e0 == e1)
            e1 = static_cast<uint16_t>(e1 > 0 ? e1 - 1 : e1 + 1);

        int r0, g0, b0, r1, g1, b1;
        expand565(e0, r0, g0, b0);
        expand565(e1, r1, g1, b1);
        int palR[4], palG[4], palB[4];
        palR[0] = r0; palG[0] = g0; palB[0] = b0;
        palR[1] = r1; palG[1] = g1; palB[1] = b1;
        if (threeColorWithBlack) {
            palR[2] = (r0 + r1) / 2; palG[2] = (g0 + g1) / 2; palB[2] = (b0 + b1) / 2;
            palR[3] = palG[3] = palB[3] = 0;
        } else {
            palR[2] = (2 * r0 + r1) / 3; palG[2] = (2 * g0 + g1) / 3; palB[2] = (2 * b0 + b1) / 3;
            palR[3] = (r0 + 2 * r1) / 3; palG[3] = (g0 + 2 * g1) / 3; palB[3] = (b0 + 2 * b1) / 3;
        }
        for (int i = 0; i < 16; ++i) {
            if (threeColorWithBlack && px.a[i] < kAlphaCut) { idx[i] = 3; continue; }
            const int maxP = threeColorWithBlack ? 3 : 4;   // opaque pixels never pick the cutout slot
            int best = 0, bestD = 1 << 30;
            for (int p = 0; p < maxP; ++p) {
                int dr = palR[p] - px.r[i], dg = palG[p] - px.g[i], db = palB[p] - px.b[i];
                int d = dr * dr + dg * dg + db * db;
                if (d < bestD) { bestD = d; best = p; }
            }
            idx[i] = static_cast<uint8_t>(best);
        }

        // weighted LSQ re-solve: p_i ~= c0*(1-w_i) + c1*w_i per channel
        double a = 0, b = 0, c = 0;
        double dr0 = 0, dg0 = 0, db0 = 0, dr1 = 0, dg1 = 0, db1 = 0;
        for (int i = 0; i < 16; ++i) {
            const double w = wt[idx[i]];
            const double u = 1.0 - w;
            a += u * u; b += u * w; c += w * w;
            dr0 += u * px.r[i]; dr1 += w * px.r[i];
            dg0 += u * px.g[i]; dg1 += w * px.g[i];
            db0 += u * px.b[i]; db1 += w * px.b[i];
        }
        const double det = a * c - b * b;
        int nr, ng, nb, nr2, ng2, nb2;
        if (det < 1e-9) {                                    // flat-ish block
            nr = ng = nb = nr2 = ng2 = nb2 = 0;
            double cnt = (a + c > 1e-9) ? 1.0 : 0.0;
            if (cnt > 0) {
                nr = (int)((dr0 + dr1) / (a + c) + 0.5);
                ng = (int)((dg0 + dg1) / (a + c) + 0.5);
                nb = (int)((db0 + db1) / (a + c) + 0.5);
                nr2 = nr; ng2 = ng; nb2 = nb;
            }
        } else {
            nr  = (int)((c * dr0 - b * dr1) / det + 0.5);
            nr2 = (int)((a * dr1 - b * dr0) / det + 0.5);
            ng  = (int)((c * dg0 - b * dg1) / det + 0.5);
            ng2 = (int)((a * dg1 - b * dg0) / det + 0.5);
            nb  = (int)((c * db0 - b * db1) / det + 0.5);
            nb2 = (int)((a * db1 - b * db0) / det + 0.5);
        }
        auto cl = [](int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); };
        c0 = rgbTo565(cl(nr), cl(ng), cl(nb));
        c1 = rgbTo565(cl(nr2), cl(ng2), cl(nb2));
    }

    if (threeColorWithBlack) {
        if (c0 > c1) std::swap(c0, c1);
        if (c0 == c1) c1 = static_cast<uint16_t>(c1 + 1);
    } else {
        if (c0 < c1) std::swap(c0, c1);
        if (c0 == c1) {
            // Degenerate: nudge c1 so the interpolants stay valid.
            c1 = static_cast<uint16_t>(c1 > 0 ? c1 - 1 : c1 + 1);
        }
    }

    // final palette + assignment from the fitted endpoints
    int er, eg, eb, er2, eg2, eb2;
    expand565(c0, er, eg, eb);
    expand565(c1, er2, eg2, eb2);

    int palR[4], palG[4], palB[4];
    if (threeColorWithBlack) {
        palR[0] = er;  palG[0] = eg;  palB[0] = eb;
        palR[1] = er2; palG[1] = eg2; palB[1] = eb2;
        palR[2] = (er + er2) / 2; palG[2] = (eg + eg2) / 2; palB[2] = (eb + eb2) / 2;
        palR[3] = palG[3] = palB[3] = 0;
    } else {
        palR[0] = er;  palG[0] = eg;  palB[0] = eb;
        palR[1] = er2; palG[1] = eg2; palB[1] = eb2;
        palR[2] = (2 * er + er2) / 3; palG[2] = (2 * eg + eg2) / 3; palB[2] = (2 * eb + eb2) / 3;
        palR[3] = (er + 2 * er2) / 3; palG[3] = (eg + 2 * eg2) / 3; palB[3] = (eb + 2 * eb2) / 3;
    }

    for (int i = 0; i < 16; ++i) {
        if (threeColorWithBlack && px.a[i] < kAlphaCut) { idx[i] = 3; continue; }
        const int maxP = threeColorWithBlack ? 3 : 4;   // opaque pixels never pick the cutout slot
        int best = 0, bestD = 1 << 30;
        for (int p = 0; p < maxP; ++p) {
            int dr = palR[p] - px.r[i], dg = palG[p] - px.g[i], db = palB[p] - px.b[i];
            int d = dr * dr + dg * dg + db * db;
            if (d < bestD) { bestD = d; best = p; }
        }
        idx[i] = static_cast<uint8_t>(best);
    }
}

uint64_t packAlphaIndices(const uint8_t a[16]) {
    int minA = 255, maxA = 0;
    for (int i = 0; i < 16; ++i) {
        int v = a[i];
        minA = std::min(minA, v); maxA = std::max(maxA, v);
    }
    if (maxA - minA < 2) {
        // Flat block: alpha0 == alpha1, every index reads back as that value.
        return (static_cast<uint64_t>(maxA) << 56) | (static_cast<uint64_t>(maxA) << 48);
    }
    uint8_t a0 = static_cast<uint8_t>(maxA), a1 = static_cast<uint8_t>(minA);
    uint8_t pal[8];
    pal[0] = a0; pal[1] = a1;
    for (int i = 0; i < 6; ++i)
        pal[2 + i] = static_cast<uint8_t>(((8 - (i + 2)) * a0 + (i + 1) * a1) / 7);
    uint64_t indices = 0;
    for (int i = 0; i < 16; ++i) {
        int best = 0, bestD = 1 << 30;
        for (int p = 0; p < 8; ++p) {
            int d = pal[p] - a[i]; if (d < 0) d = -d;
            if (d < bestD) { bestD = d; best = p; }
        }
        indices |= static_cast<uint64_t>(best) << (3 * i);
    }
    return (static_cast<uint64_t>(a0) << 56) | (static_cast<uint64_t>(a1) << 48) | indices;
}

} // namespace

bool dxtCompress(const uint8_t* rgba, int w, int h, bool useDxt5,
                 std::vector<uint8_t>& out) {
    if (!rgba || w <= 0 || h <= 0 || (w % kAlign) != 0 || (h % kAlign) != 0) return false;
    const size_t start = out.size();
    const size_t need = static_cast<size_t>(w / 4) * (h / 4) * (useDxt5 ? 16 : 8);
    out.resize(start + need);
    uint8_t* o = out.data() + start;

    for (int by = 0; by < h; by += 4) {
        for (int bx = 0; bx < w; bx += 4) {
            BlockPixels px;
            gatherBlock(rgba, w, h, bx, by, px);
            uint16_t c0, c1;
            uint8_t idx[16];
            if (useDxt5) {
                // Alpha block first: a0, a1, then the 48-bit index word --
                // packAlphaIndices packs all three into one big-endian u64.
                uint64_t alphaWord = packAlphaIndices(px.a);
                for (int s = 56; s >= 0; s -= 8) *o++ = static_cast<uint8_t>(alphaWord >> s);
                fitColorBlock(px, false, c0, c1, idx); // alpha lives in the alpha block
            } else {
                fitColorBlock(px, px.hasTransparent, c0, c1, idx);
            }
            {
                // Direct big-endian writes into the pre-sized output buffer.
                uint8_t* q = o;
                q[0] = static_cast<uint8_t>(c0 >> 8); q[1] = static_cast<uint8_t>(c0);
                q[2] = static_cast<uint8_t>(c1 >> 8); q[3] = static_cast<uint8_t>(c1);
                o += 4;
                uint32_t bits = 0;
                for (int i = 0; i < 16; ++i) bits |= static_cast<uint32_t>(idx[i]) << (2 * i);
                for (int s = 24; s >= 0; s -= 8) *o++ = static_cast<uint8_t>(bits >> s);
            }
        }
    }
    return true;
}

void dxt1DecodeBlock(const uint8_t* in, uint8_t* outRGBA) {
    uint16_t c0 = getU16be(in), c1 = getU16be(in + 2);
    uint32_t bits = getU32be(in + 4);
    int r0, g0, b0, r1, g1, b1;
    expand565(c0, r0, g0, b0);
    expand565(c1, r1, g1, b1);
    int pr[4], pg[4], pb[4];
    pr[0] = r0; pg[0] = g0; pb[0] = b0;
    pr[1] = r1; pg[1] = g1; pb[1] = b1;
    if (c0 > c1) {
        pr[2] = (2 * r0 + r1) / 3; pg[2] = (2 * g0 + g1) / 3; pb[2] = (2 * b0 + b1) / 3;
        pr[3] = (r0 + 2 * r1) / 3; pg[3] = (g0 + 2 * g1) / 3; pb[3] = (b0 + 2 * b1) / 3;
    } else {
        pr[2] = (r0 + r1) / 2; pg[2] = (g0 + g1) / 2; pb[2] = (b0 + b1) / 2;
        pr[3] = pg[3] = pb[3] = 0;
    }
    for (int i = 0; i < 16; ++i) {
        int code = (bits >> (2 * i)) & 3;
        outRGBA[i * 4 + 0] = static_cast<uint8_t>(pr[code]);
        outRGBA[i * 4 + 1] = static_cast<uint8_t>(pg[code]);
        outRGBA[i * 4 + 2] = static_cast<uint8_t>(pb[code]);
        outRGBA[i * 4 + 3] = (c0 <= c1 && code == 3) ? 0 : 255;
    }
}

void dxt5DecodeBlock(const uint8_t* in, uint8_t* outRGBA) {
    int a0 = in[0], a1 = in[1];
    uint64_t abits = getU48be(in + 2);
    uint8_t apal[8];
    apal[0] = static_cast<uint8_t>(a0);
    apal[1] = static_cast<uint8_t>(a1);
    if (a0 > a1) {
        for (int i = 0; i < 6; ++i)
            apal[2 + i] = static_cast<uint8_t>(((6 - i) * a0 + (1 + i) * a1) / 7);
    } else {
        for (int i = 0; i < 4; ++i)
            apal[2 + i] = static_cast<uint8_t>(((4 - i) * a0 + (1 + i) * a1) / 5);
        apal[6] = 0; apal[7] = 255;
    }
    uint16_t c0 = getU16be(in + 8), c1 = getU16be(in + 10);
    uint32_t cbits = getU32be(in + 12);
    int r0, g0, b0, r1, g1, b1;
    expand565(c0, r0, g0, b0);
    expand565(c1, r1, g1, b1);
    int pr[4], pg[4], pb[4];
    pr[0] = r0; pg[0] = g0; pb[0] = b0;
    pr[1] = r1; pg[1] = g1; pb[1] = b1;
    pr[2] = (2 * r0 + r1) / 3; pg[2] = (2 * g0 + g1) / 3; pb[2] = (2 * b0 + b1) / 3;
    pr[3] = (r0 + 2 * r1) / 3; pg[3] = (g0 + 2 * g1) / 3; pb[3] = (b0 + 2 * b1) / 3;
    for (int i = 0; i < 16; ++i) {
        int acode = static_cast<int>((abits >> (3 * i)) & 7);
        int ccode = (cbits >> (2 * i)) & 3;
        outRGBA[i * 4 + 0] = static_cast<uint8_t>(pr[ccode]);
        outRGBA[i * 4 + 1] = static_cast<uint8_t>(pg[ccode]);
        outRGBA[i * 4 + 2] = static_cast<uint8_t>(pb[ccode]);
        outRGBA[i * 4 + 3] = apal[acode];
    }
}

void padImageRGBA(const std::vector<uint8_t>& src, int w, int h,
                  std::vector<uint8_t>& dst, int newW, int newH) {
    dst.assign(static_cast<size_t>(newW) * newH * 4, 0);
    for (int y = 0; y < newH; ++y) {
        int sy = std::min(y, h - 1);
        for (int x = 0; x < newW; ++x) {
            int sx = std::min(x, w - 1);
            const uint8_t* p = &src[(static_cast<size_t>(sy) * w + sx) * 4];
            uint8_t* q = &dst[(static_cast<size_t>(y) * newW + x) * 4];
            q[0] = p[0]; q[1] = p[1]; q[2] = p[2]; q[3] = p[3];
        }
    }
}

} // namespace ctfak
