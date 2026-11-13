// CompTable.h
// Built-in level compensation of each type, measured with guitar-like
// noise at -18 dBFS (tools/calibrate_comp.cpp). Gain to apply after the clipper,
// by type, More (0, 25, 50, 75, 100 %) and Drive (0, 10, ... 100 %).

#pragma once
#include <algorithm>

namespace slap::comp
{
inline constexpr int driveSteps = 11, moreSteps = 5;

inline constexpr float table[4][moreSteps][driveSteps] = {
    { // Tube
        { 0.9180f, 0.6033f, 0.4146f, 0.3055f, 0.2448f, 0.2121f, 0.1933f, 0.1839f, 0.1792f, 0.1769f, 0.1759f },
        { 0.4488f, 0.3329f, 0.2671f, 0.2313f, 0.2140f, 0.2103f, 0.2020f, 0.1976f, 0.1954f, 0.1943f, 0.1938f },
        { 0.2620f, 0.2217f, 0.1999f, 0.1884f, 0.1827f, 0.1803f, 0.1779f, 0.1766f, 0.1760f, 0.1758f, 0.1756f },
        { 0.1996f, 0.1874f, 0.1811f, 0.1780f, 0.1766f, 0.1759f, 0.1754f, 0.1751f, 0.1750f, 0.1750f, 0.1749f },
        { 0.1813f, 0.1780f, 0.1764f, 0.1756f, 0.1753f, 0.1751f, 0.1750f, 0.1750f, 0.1749f, 0.1749f, 0.1749f }
    },
    { // Tape
        { 1.3533f, 1.1631f, 0.9628f, 0.7742f, 0.6154f, 0.4944f, 0.4099f, 0.3549f, 0.3215f, 0.3025f, 0.2923f },
        { 0.6612f, 0.5895f, 0.5156f, 0.4478f, 0.3925f, 0.3517f, 0.3243f, 0.3072f, 0.2973f, 0.2919f, 0.2891f },
        { 0.3953f, 0.3708f, 0.3463f, 0.3245f, 0.3076f, 0.2959f, 0.2888f, 0.2847f, 0.2827f, 0.2818f, 0.2815f },
        { 0.3114f, 0.3041f, 0.2970f, 0.2909f, 0.2865f, 0.2835f, 0.2818f, 0.2810f, 0.2807f, 0.2807f, 0.2809f },
        { 0.2877f, 0.2857f, 0.2839f, 0.2823f, 0.2812f, 0.2805f, 0.2802f, 0.2801f, 0.2802f, 0.2805f, 0.2808f }
    },
    { // Fuzz
        { 0.3812f, 0.3147f, 0.2849f, 0.2712f, 0.2648f, 0.2618f, 0.2604f, 0.2597f, 0.2594f, 0.2593f, 0.2592f },
        { 0.3089f, 0.2821f, 0.2701f, 0.2646f, 0.2621f, 0.2609f, 0.2604f, 0.2601f, 0.2599f, 0.2599f, 0.2598f },
        { 0.2791f, 0.2678f, 0.2629f, 0.2609f, 0.2600f, 0.2595f, 0.2593f, 0.2592f, 0.2591f, 0.2591f, 0.2591f },
        { 0.2719f, 0.2644f, 0.2614f, 0.2602f, 0.2596f, 0.2593f, 0.2592f, 0.2591f, 0.2591f, 0.2591f, 0.2591f },
        { 0.2701f, 0.2635f, 0.2610f, 0.2600f, 0.2595f, 0.2592f, 0.2591f, 0.2591f, 0.2591f, 0.2591f, 0.2590f }
    },
    { // Screamer
        { 0.7567f, 0.5514f, 0.4086f, 0.3249f, 0.2804f, 0.2578f, 0.2468f, 0.2414f, 0.2388f, 0.2376f, 0.2371f },
        { 0.4237f, 0.3340f, 0.2859f, 0.2614f, 0.2494f, 0.2436f, 0.2408f, 0.2395f, 0.2388f, 0.2385f, 0.2384f },
        { 0.2889f, 0.2621f, 0.2489f, 0.2424f, 0.2394f, 0.2379f, 0.2372f, 0.2369f, 0.2367f, 0.2366f, 0.2366f },
        { 0.2500f, 0.2430f, 0.2396f, 0.2380f, 0.2372f, 0.2369f, 0.2367f, 0.2366f, 0.2365f, 0.2365f, 0.2365f },
        { 0.2399f, 0.2381f, 0.2373f, 0.2369f, 0.2367f, 0.2366f, 0.2365f, 0.2365f, 0.2365f, 0.2365f, 0.2365f }
    }
};

// Bilinear interpolation. drive and more in 0..1.
inline float lookup (int type, float drive, float more)
{
    type = std::clamp (type, 0, 3);
    const float fd = std::clamp (drive, 0.0f, 1.0f) * (float) (driveSteps - 1);
    const float fm = std::clamp (more,  0.0f, 1.0f) * (float) (moreSteps - 1);
    const int d0 = std::min ((int) fd, driveSteps - 2), m0 = std::min ((int) fm, moreSteps - 2);
    const float td = fd - (float) d0, tm = fm - (float) m0;
    const auto& t = table[type];
    const float lo = t[m0][d0]     + td * (t[m0][d0 + 1]     - t[m0][d0]);
    const float hi = t[m0 + 1][d0] + td * (t[m0 + 1][d0 + 1] - t[m0 + 1][d0]);
    return lo + tm * (hi - lo);
}

} // namespace slap::comp
