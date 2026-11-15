#include "../app/streaming/input/penpressure.h"

#include <cstdio>
#include <cstdlib>
#include <limits>

static void check(bool condition, const char* message)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}

int main()
{
    using namespace PenPressure;
    check(std::abs(map(0.25f, 0.5, true) - 0.5f) < 0.00001f, "S Pen soft curve");
    check(map(0.25f, 1.0, true) == 0.25f, "Wacom default passthrough");
    check(map(0.0f, 0.3, true) == 0.0f, "zero must stay zero");
    check(map(1.0f, 0.5, false) == 0.0f, "pen-up and hover must have zero pressure");
    check(map(-1.0f, 0.5, true) == 0.0f, "negative input");
    check(map(2.0f, 0.5, true) == 1.0f, "oversized input clamped");
    check(map(std::numeric_limits<float>::quiet_NaN(), 0.5, true) == 0.0f, "NaN input");
    check(validGamma(std::numeric_limits<double>::quiet_NaN(), 0.5) == 0.5, "corrupt setting fallback");
    check(validGamma(0.0) == 0.3 && validGamma(99.0) == 2.0, "settings limits");
    check(map(0.70f, 0.5, true, 70) == 1.0f, "S Pen 70% raw reaches 100%");
    check(map(0.90f, 0.5, true, 70) == 1.0f, "above threshold saturates at 100%");
    check(map(0.35f, 1.0, true, 70) == 0.5f, "linear customization maps 35% to 50%");
    check(std::abs(map(0.35f, 0.5, true, 70) - std::sqrt(0.5f)) < 0.00001f, "softness applied after threshold normalization");
    check(map(0.7f, 1.0, true, 100) == 0.7f, "Wacom threshold unchanged");
    check(validMaxPercent(0) == 20 && validMaxPercent(999) == 100, "threshold settings limits");
    for (int percent : {20, 50, 70, 100}) {
        for (double gamma : {0.3, 0.5, 1.0, 2.0}) {
            check(map(percent / 100.0f, gamma, true, percent) == 1.0f, "threshold independent of softness");
            check(map(1.0f, gamma, false, percent) == 0.0f, "release and hover stay zero with threshold");
            check(map(0.0f, gamma, true, percent) == 0.0f, "zero stays zero with threshold");
            float last = 0.0f;
            for (int i = 0; i <= 1024; ++i) {
                float value = map(i / 1024.0f, gamma, true, percent);
                check(std::isfinite(value) && value >= last && value <= 1.0f, "custom threshold bounded and monotone");
                last = value;
            }
        }
    }
    for (double gamma : {0.3, 0.5, 1.0, 2.0}) {
        float previous = 0.0f;
        for (int i = 0; i <= 1024; i++) {
            float p = i / 1024.0f;
            float result = map(p, gamma, true);
            check(std::isfinite(result) && result >= previous && result <= 1.0f, "bounded monotone curve");
            if (gamma == 1.0) check(result == p, "all 1025 default pressure values unchanged");
            previous = result;
        }
    }
    std::puts("PASS: pressure endpoints, default passthrough, soft curve, release/hover, invalid input, monotonicity");
}
