#include <cstdio>
#include <vector>
#include "../src/scale_filter.hpp"
using namespace tg;
static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)
static Image make(int w, int h) { Image i; i.w = w; i.h = h; i.index.assign(size_t(w) * h, 0); i.rgb.assign(size_t(w) * h, Rgb{0, 0, 0}); return i; }
int main() {
    for (int n = 1; n <= 5; n++) {
        Image a = make(4, 4);
        for (int i = 0; i < 16; i++) a.rgb[size_t(i)] = Rgb{uint8_t(i * 10), uint8_t(255 - i), uint8_t(i * 7)};
        std::vector<uint32_t> o(size_t(16 * n * n));
        scaleFrame(a, o.data(), n, ScaleFilter::Off);
        bool ok = true;
        for (int y = 0; y < 4 * n; y++) for (int x = 0; x < 4 * n; x++) { const Rgb& c = a.rgb[size_t(y / n) * 4 + x / n]; ok &= o[size_t(y) * 4 * n + x] == (uint32_t(c.r) << 16 | uint32_t(c.g) << 8 | c.b); }
        CHECK(ok);   // Off = plain nearest neighbour
        // flat colour stays flat in every mode
        Image f = make(5, 3); for (auto& c : f.rgb) c = Rgb{10, 200, 90};
        for (ScaleFilter m : {ScaleFilter::Sharper, ScaleFilter::Smooth}) {
            std::vector<uint32_t> g(size_t(15 * n * n));
            scaleFrame(f, g.data(), n, m);
            bool flat = true; for (uint32_t v : g) flat &= v == (10u << 16 | 200u << 8 | 90u);
            CHECK(flat);
        }
    }
    {   // diagonal edge: the sharper filter only changes corner quadrants, never the original centre pixel, and uses only colours that exist
        Image d = make(4, 4);
        const Rgb A{255, 255, 255}, B{0, 0, 0};
        for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++) d.rgb[size_t(y * 4 + x)] = x >= y ? A : B;
        std::vector<uint32_t> o(size_t(16 * 9));
        scaleFrame(d, o.data(), 3, ScaleFilter::Sharper);
        bool onlyTwo = true, centres = true;
        for (uint32_t v : o) onlyTwo &= v == 0xFFFFFFu || v == 0u;
        for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++) { const Rgb& c = d.rgb[size_t(y * 4 + x)]; centres &= o[size_t(y * 3 + 1) * 12 + x * 3 + 1] == (c == A ? 0xFFFFFFu : 0u); }
        CHECK(onlyTwo && centres);
        bool changed = false;
        std::vector<uint32_t> n3(size_t(16 * 9));
        scaleFrame(d, n3.data(), 3, ScaleFilter::Off);
        for (size_t i = 0; i < o.size(); i++) changed |= o[i] != n3[i];
        CHECK(changed);   // the edge is actually rounded
    }
    {   // smooth: a black/white step gets intermediate grey between the pixel centres
        Image s = make(2, 1); s.rgb[1] = Rgb{255, 255, 255};
        std::vector<uint32_t> o(size_t(2 * 4));
        scaleFrame(s, o.data(), 4, ScaleFilter::Smooth);
        CHECK((o[0] & 255) == 0 && (o[7] & 255) == 255);
        bool mono = true; for (int i = 1; i < 8; i++) mono &= (o[size_t(i)] & 255) >= (o[size_t(i - 1)] & 255);
        CHECK(mono && (o[3] & 255) > 0 && (o[4] & 255) < 255);
    }
    std::printf(failures ? "%d FAILURES\n" : "all scale filter tests passed\n", failures);
    return failures ? 1 : 0;
}
