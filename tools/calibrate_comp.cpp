// Measures the level of each type with no compensation and writes CompTable.h.
// Build and run from the project folder:
//   c++ -O2 -std=c++17 -ISource tools/calibrate_comp.cpp -o calibrate && ./calibrate
#include "dsp/Saturation.h"
#include <cmath>
#include <cstdio>
#include <random>
#include <vector>

int main (int argc, char** argv)
{
    const float fs = 192000.0f;
    // Test signal: noise shaped like a guitar / drum bus (80 Hz..5 kHz), -18 dBFS RMS.
    const int N = (int) (fs * 2.0f);
    std::vector<float> sig ((size_t) N);
    std::mt19937 rng (3); std::normal_distribution<float> nd;
    slap::Biquad lp, pk, hp;
    lp.lowPass (fs, 5000, 0.7f); pk.peak (fs, 400, 0.8f, 4);
    hp.set (1, -1, 0, 1, -0.9974f, 0);   // ~80 Hz one-pole-ish high-pass
    slap::Biquad::State a, b, c;
    double ss = 0;
    for (int i = 0; i < N; ++i) { float x = pk.process (lp.process (hp.process (nd (rng), a), b), c); sig[(size_t) i] = x; ss += x * x; }
    const float k = std::pow (10.0f, -18.0f / 20.0f) / std::sqrt ((float) (ss / N));
    for (auto& x : sig) x *= k;

    const int S = 21;   // course 0, 0.1, ... 2.0 (Drive 0..100 %, then More)
    float tab[4][S];
    for (int t = 0; t < 4; ++t)
        for (int i = 0; i < S; ++i)
        {
            const float course = 2.0f * (float) i / (float) (S - 1);
            slap::BandSettings st;
            st.type = (slap::Type) t;
            st.drive = std::min (1.0f, course);
            st.more = course > 1.0f;
            if (st.more) st.drive = course - 1.0f;
            slap::BandSaturator s; s.prepare (fs); s.setSettings (st); s.setCompensation (1.0f);
            double in = 0, out = 0;
            for (int n = 0; n < N; ++n)
            {
                s.tick();
                const float y = s.process (0, sig[(size_t) n]);
                if (n > N / 4) { in += (double) sig[(size_t) n] * sig[(size_t) n]; out += (double) y * y; }
            }
            tab[t][i] = (float) std::sqrt (in / std::max (1e-20, out));
        }

    FILE* f = fopen (argc > 1 ? argv[1] : "Source/dsp/CompTable.h", "w");
    if (f == nullptr) return 1;
    fprintf (f, "// CompTable.h\n// Built-in level compensation of each type, measured with shaped noise at\n"
                "// -18 dBFS (tools/calibrate_comp.cpp). Gain to apply after the clipper, by\n"
                "// type and drive course (0..2: Drive 0..100 %%, then More adds another 1).\n\n"
                "#pragma once\n#include <algorithm>\n\nnamespace slap::comp\n{\n"
                "inline constexpr int steps = %d;\n\n"
                "inline constexpr float table[4][steps] = {\n", S);
    const char* names[] = { "Transistor", "Tube", "Tape", "Fuzz" };
    for (int t = 0; t < 4; ++t)
    {
        fprintf (f, "    { // %s\n     ", names[t]);
        for (int i = 0; i < S; ++i) fprintf (f, " %.4ff%s", tab[t][i], i < S - 1 ? "," : "");
        fprintf (f, " }%s\n", t < 3 ? "," : "");
    }
    fprintf (f, "};\n\n"
                "// Linear interpolation. course in 0..2.\n"
                "inline float lookup (int type, float course)\n{\n"
                "    type = std::clamp (type, 0, 3);\n"
                "    const float fi = std::clamp (course, 0.0f, 2.0f) * 0.5f * (float) (steps - 1);\n"
                "    const int i0 = std::min ((int) fi, steps - 2);\n"
                "    const float t = fi - (float) i0;\n"
                "    return table[type][i0] + t * (table[type][i0 + 1] - table[type][i0]);\n}\n\n"
                "} // namespace slap::comp\n");
    fclose (f);
    for (int t = 0; t < 4; ++t) printf ("%-10s course 0 %.3f  0.5 %.3f  1 %.3f  2 %.3f\n", names[t], tab[t][0], tab[t][5], tab[t][10], tab[t][20]);
}
