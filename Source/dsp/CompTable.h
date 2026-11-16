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
      1.3324f, 1.1399f, 0.9367f, 0.7452f, 0.5831f, 0.4596f, 0.3729f, 0.3164f, 0.2819f, 0.2623f, 0.2514f, 0.2457f, 0.2429f, 0.2415f, 0.2408f, 0.2405f, 0.2403f, 0.2402f, 0.2401f, 0.2401f, 0.2401f },
    { // Fuzz
      0.2743f, 0.2188f, 0.1947f, 0.1851f, 0.1825f, 0.1837f, 0.1872f, 0.1922f, 0.1984f, 0.2058f, 0.2057f, 0.2057f, 0.2057f, 0.2056f, 0.2056f, 0.2056f, 0.2056f, 0.2056f, 0.2056f, 0.2056f, 0.2056f }
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
