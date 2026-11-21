#include "scale_filter.hpp"

#include <algorithm>
#include <vector>

namespace tg {
namespace {

inline uint32_t pack(const Rgb& c) { return uint32_t(c.r) << 16 | uint32_t(c.g) << 8 | c.b; }

void nearest(const Image& s, uint32_t* d, int n) {
    const int W = s.w * n;
    for (int y = 0; y < s.h; y++) {
        uint32_t* row = d + size_t(y) * n * W;
        for (int x = 0; x < s.w; x++) {
            uint32_t c = pack(s.rgb[size_t(y) * s.w + x]);
            for (int k = 0; k < n; k++) row[x * n + k] = c;
        }
        for (int k = 1; k < n; k++) std::copy(row, row + W, row + size_t(k) * W);
    }
}

// Each source pixel is split into quadrants (the middle row/column of an odd factor stays the pixel's own colour); a corner quadrant takes
// the colour of the neighbour above/below when that equals the horizontal neighbour and differs from the opposite two (the EPX / Scale2x rule),
// which rounds diagonal edges but keeps flat areas and dithering untouched.
void sharper(const Image& s, uint32_t* d, int n) {
    const int W = s.w * n;
    auto at = [&](int x, int y) { x = std::max(0, std::min(s.w - 1, x)); y = std::max(0, std::min(s.h - 1, y)); return pack(s.rgb[size_t(y) * s.w + x]); };
    for (int y = 0; y < s.h; y++)
        for (int x = 0; x < s.w; x++) {
            const uint32_t P = at(x, y), U = at(x, y - 1), D = at(x, y + 1), L = at(x - 1, y), R = at(x + 1, y);
            const uint32_t tl = (U == L && U != R && L != D) ? U : P;
            const uint32_t tr = (U == R && U != L && R != D) ? U : P;
            const uint32_t bl = (D == L && D != R && L != U) ? D : P;
            const uint32_t br = (D == R && D != L && R != U) ? D : P;
            for (int sy = 0; sy < n; sy++) {
                const int vy = 2 * sy + 1 < n ? -1 : (2 * sy + 1 > n ? 1 : 0);
                uint32_t* row = d + size_t(y * n + sy) * W + size_t(x) * n;
                for (int sx = 0; sx < n; sx++) {
                    const int vx = 2 * sx + 1 < n ? -1 : (2 * sx + 1 > n ? 1 : 0);
                    row[sx] = (vx == 0 || vy == 0) ? P : (vy < 0 ? (vx < 0 ? tl : tr) : (vx < 0 ? bl : br));
                }
            }
        }
}

// Bilinear interpolation between the centres of the source pixels (8-bit weights).
void smooth(const Image& s, uint32_t* d, int n) {
    const int W = s.w * n, H = s.h * n;
    struct Tap { int i0, i1, w; };   // w = weight of i1, 0..256
    auto taps = [&](int count, int srcCount) {
        std::vector<Tap> t; t.resize(size_t(count));
        for (int o = 0; o < count; o++) {
            const int num = 2 * o + 1 - n;   // position of the output pixel centre in source pixels is num / (2n)
            int i0 = num < 0 ? -1 : num / (2 * n), w = (num < 0 ? num + 2 * n : num % (2 * n)) * 256 / (2 * n);
            int i1 = i0 + 1;
            t[size_t(o)] = {std::max(0, std::min(srcCount - 1, i0)), std::max(0, std::min(srcCount - 1, i1)), w};
        }
        return t;
    };
    const std::vector<Tap> tx = taps(W, s.w);
    const std::vector<Tap> ty = taps(H, s.h);
    for (int y = 0; y < H; y++) {
        const Tap& a = ty[size_t(y)];
        const Rgb* r0 = &s.rgb[size_t(a.i0) * s.w]; const Rgb* r1 = &s.rgb[size_t(a.i1) * s.w];
        for (int x = 0; x < W; x++) {
            const Tap& b = tx[size_t(x)];
            const int w00 = (256 - a.w) * (256 - b.w), w01 = (256 - a.w) * b.w, w10 = a.w * (256 - b.w), w11 = a.w * b.w;   // sum 65536
            const Rgb &c00 = r0[b.i0], &c01 = r0[b.i1], &c10 = r1[b.i0], &c11 = r1[b.i1];
            const unsigned r = unsigned((c00.r * w00 + c01.r * w01 + c10.r * w10 + c11.r * w11 + 32768) >> 16);
            const unsigned g = unsigned((c00.g * w00 + c01.g * w01 + c10.g * w10 + c11.g * w11 + 32768) >> 16);
            const unsigned bl = unsigned((c00.b * w00 + c01.b * w01 + c10.b * w10 + c11.b * w11 + 32768) >> 16);
            d[size_t(y) * W + x] = r << 16 | g << 8 | bl;
        }
    }
}

}  // namespace

void scaleFrame(const Image& src, uint32_t* dst, int scale, ScaleFilter mode) {
    if (scale < 1) scale = 1;
    switch (mode) {
    case ScaleFilter::Sharper: sharper(src, dst, scale); break;
    case ScaleFilter::Smooth: smooth(src, dst, scale); break;
    default: nearest(src, dst, scale); break;
    }
}

}  // namespace tg
