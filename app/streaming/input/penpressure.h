#pragma once

#include <algorithm>
#include <cmath>

namespace PenPressure {
// Shared by the native input path and the standalone regression test.
inline double validGamma(double gamma, double fallback = 1.0)
{
    return std::isfinite(gamma) ? std::clamp(gamma, 0.3, 2.0) : fallback;
}

inline int validMaxPercent(int percent)
{
    return std::clamp(percent, 20, 100);
}

inline float map(float pressure, double gamma, bool contact, int maxPercent = 100)
{
    if (!contact || !std::isfinite(pressure) || pressure <= 0.0f) {
        return 0.0f;
    }
    // Normalize raw pressure BEFORE applying the curve, so the selected input
    // threshold always reaches 100%, independently of the softness setting.
    float normalized = std::clamp(pressure / (validMaxPercent(maxPercent) / 100.0f), 0.0f, 1.0f);
    return static_cast<float>(std::pow(normalized, validGamma(gamma)));
}
}
