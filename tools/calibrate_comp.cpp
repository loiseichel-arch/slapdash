// Measures the level of each type with no compensation and writes CompTable.h.
// Build and run from the project folder:
//   c++ -O2 -std=c++17 -ISource tools/calibrate_comp.cpp -o calibrate && ./calibrate
#include "dsp/Saturation.h"
#include <cstdio>
#include <vector>
#include <cmath>
#include <random>
int main (int argc, char** argv)
{
    const float fs = 192000.0f;
    // Test signal: noise shaped like a guitar (band 80 Hz..5 kHz), -18 dBFS RMS,
    // with a slow level wobble so dynamics (sag, envelope) behave as in playing.
    const int N = (int) (fs * 2.0f);
    std::vector<float> sig ((size_t) N);
    std::mt19937 rng (3); std::normal_distribution<float> nd;
    slap::Biquad hp, lp, pk; hp.highPass (fs, 80, 0.7f); lp.lowPass (fs, 5000, 0.7f); pk.peak (fs, 400, 0.8f, 4);
    slap::Biquad::State a, b, c;
    double ss = 0;
    for (int i = 0; i < N; ++i) { float x = pk.process (lp.process (hp.process (nd (rng), a), b), c); sig[(size_t) i] = x; ss += x * x; }
    const float k = std::pow (10.0f, -18.0f / 20.0f) / std::sqrt ((float) (ss / N));
    for (auto& x : sig) x *= k;

    const int D = 11, M = 5;
    float tab[4][M][D];
    for (int t = 0; t < 4; ++t)
        for (int m = 0; m < M; ++m)
            for (int d = 0; d < D; ++d)
            {
                slap::BandSettings st; st.type = (slap::Type) t; st.drive = d / (float) (D - 1); st.more = m / (float) (M - 1);
                auto measure = [&] (float c)
                {
                    slap::BandSaturator s2; s2.prepare (fs); s2.setSettings (st); s2.setCompensation (c);
                    double in = 0, out = 0;
                    for (int i = 0; i < N; ++i)
                    {
                        s2.tick();
                        const float y = s2.process (0, sig[(size_t) i]);
                        if (i > N / 4) { in += sig[(size_t) i] * sig[(size_t) i]; out += (double) y * y; }
                    }
                    return std::sqrt (out / std::max (1e-20, in));   // output / input level
                };
                tab[t][m][d] = (float) (1.0 / measure (1.0f));
            }

    FILE* f = fopen (argc > 1 ? argv[1] : "Source/dsp/CompTable.h", "w");
    if (f == nullptr) return 1;
    fprintf (f, "// CompTable.h\n// Built-in level compensation of each type, measured with guitar-like\n"
                "// noise at -18 dBFS (tools/calibrate_comp.cpp). Gain to apply after the clipper,\n"
                "// by type, More (0, 25, 50, 75, 100 %%) and Drive (0, 10, ... 100 %%).\n\n"
                "#pragma once\n#include <algorithm>\n\nnamespace slap::comp\n{\n"
                "inline constexpr int driveSteps = %d, moreSteps = %d;\n\n"
                "inline constexpr float table[4][moreSteps][driveSteps] = {\n", D, M);
    const char* names[] = { "Tube", "Tape", "Fuzz", "Screamer" };
    for (int t = 0; t < 4; ++t)
    {
        fprintf (f, "    { // %s\n", names[t]);
        for (int m = 0; m < M; ++m)
        {
            fprintf (f, "        {");
            for (int d = 0; d < D; ++d) fprintf (f, " %.4ff%s", tab[t][m][d], d < D - 1 ? "," : "");
            fprintf (f, " }%s\n", m < M - 1 ? "," : "");
        }
        fprintf (f, "    }%s\n", t < 3 ? "," : "");
    }
    fprintf (f, "};\n\n"
                "// Bilinear interpolation. drive and more in 0..1.\n"
                "inline float lookup (int type, float drive, float more)\n{\n"
                "    type = std::clamp (type, 0, 3);\n"
                "    const float fd = std::clamp (drive, 0.0f, 1.0f) * (float) (driveSteps - 1);\n"
                "    const float fm = std::clamp (more,  0.0f, 1.0f) * (float) (moreSteps - 1);\n"
                "    const int d0 = std::min ((int) fd, driveSteps - 2), m0 = std::min ((int) fm, moreSteps - 2);\n"
                "    const float td = fd - (float) d0, tm = fm - (float) m0;\n"
                "    const auto& t = table[type];\n"
                "    const float lo = t[m0][d0]     + td * (t[m0][d0 + 1]     - t[m0][d0]);\n"
                "    const float hi = t[m0 + 1][d0] + td * (t[m0 + 1][d0 + 1] - t[m0 + 1][d0]);\n"
                "    return lo + tm * (hi - lo);\n}\n\n} // namespace slap::comp\n");
    fclose (f);
    for (int t = 0; t < 4; ++t) printf ("%s: d0 %.2f d50 %.2f d100 %.2f | more100 d100 %.3f\n", names[t], tab[t][0][0], tab[t][0][5], tab[t][0][10], tab[t][4][10]);
}
