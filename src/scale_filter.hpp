// Enlarging the 320x200 frame for the window layers (F8 cycles the modes). The game itself always renders 320x200.
#pragma once
#include <cstdint>

#include "image.hpp"

namespace tg {

enum class ScaleFilter { Off = 0, Sharper = 1, Smooth = 2 };   // nearest neighbour / edge-preserving (EPX style) / bilinear

// Writes (src.w*scale) x (src.h*scale) pixels as 0x00RRGGBB into dst (scale >= 1).
void scaleFrame(const Image& src, uint32_t* dst, int scale, ScaleFilter mode);

inline ScaleFilter nextFilter(ScaleFilter f) { return ScaleFilter((int(f) + 1) % 3); }

}  // namespace tg
