// CompTable.h
// Built-in level compensation of each type, measured with shaped noise at
// -18 dBFS (tools/calibrate_comp.cpp). Gain to apply after the clipper, by
// type and drive course (0..2: Drive 0..100 %, then More adds another 1).

#pragma once
#include <algorithm>

namespace slap::comp
{
inline constexpr int steps = 21;

inline constexpr float table[4][steps] = {
    { // Transistor
      1.0042f, 0.6409f, 0.4194f, 0.2902f, 0.2181f, 0.1785f, 0.1565f, 0.1441f, 0.1370f, 0.1327f, 0.1302f, 0.1286f, 0.1276f, 0.1270f, 0.1267f, 0.1264f, 0.1263f, 0.1262f, 0.1261f, 0.1261f, 0.1261f },
    { // Tube
      1.0434f, 0.6742f, 0.4523f, 0.3233f, 0.2510f, 0.2116f, 0.1888f, 0.1780f, 0.1733f, 0.1720f, 0.1708f, 0.1702f, 0.1699f, 0.1697f, 0.1696f, 0.1696f, 0.1695f, 0.1695f, 0.1695f, 0.1695f, 0.1695f },
    { // Tape
      0.9950f, 0.6789f, 0.4902f, 0.3794f, 0.3150f, 0.2789f, 0.2593f, 0.2493f, 0.2445f, 0.2426f, 0.2413f, 0.2407f, 0.2404f, 0.2403f, 0.2402f, 0.2401f, 0.2401f, 0.2401f, 0.2400f, 0.2400f, 0.2400f },
    { // Fuzz
      2.6454f, 1.6701f, 1.0213f, 0.6173f, 0.3925f, 0.2812f, 0.2323f, 0.2132f, 0.2081f, 0.2103f, 0.2078f, 0.2067f, 0.2062f, 0.2059f, 0.2058f, 0.2057f, 0.2057f, 0.2057f, 0.2056f, 0.2056f, 0.2056f }
};

// Linear interpolation. course in 0..2.
inline float lookup (int type, float course)
{
    type = std::clamp (type, 0, 3);
    const float fi = std::clamp (course, 0.0f, 2.0f) * 0.5f * (float) (steps - 1);
    const int i0 = std::min ((int) fi, steps - 2);
    const float t = fi - (float) i0;
    return table[type][i0] + t * (table[type][i0 + 1] - table[type][i0]);
}

} // namespace slap::comp
