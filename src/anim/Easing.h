#pragma once

#include <cmath>

namespace chess3d::anim {

using EasingFn = float (*)(float);

inline float linear(float t)         { return t; }
inline float easeInQuad(float t)     { return t * t; }
inline float easeOutQuad(float t)    { return 1.0f - (1.0f - t) * (1.0f - t); }
inline float easeInOutQuad(float t)  { return t < 0.5f ? 2*t*t : 1 - std::pow(-2*t + 2, 2)/2; }
inline float easeOutCubic(float t)   { return 1.0f - std::pow(1.0f - t, 3); }
inline float easeInOutCubic(float t) {
    return t < 0.5f ? 4*t*t*t : 1 - std::pow(-2*t + 2, 3)/2;
}
inline float easeOutBack(float t) {
    constexpr float c1 = 1.70158f;
    constexpr float c3 = c1 + 1.0f;
    const float u = t - 1.0f;
    return 1.0f + c3 * u * u * u + c1 * u * u;
}
inline float easeOutBounce(float t) {
    constexpr float n1 = 7.5625f;
    constexpr float d1 = 2.75f;
    if (t < 1.0f/d1) {
        return n1*t*t;
    } else if (t < 2.0f/d1) {
        t -= 1.5f/d1;
        return n1*t*t + 0.75f;
    } else if (t < 2.5f/d1) {
        t -= 2.25f/d1;
        return n1*t*t + 0.9375f;
    } else {
        t -= 2.625f/d1;
        return n1*t*t + 0.984375f;
    }
}

}  // namespace chess3d::anim
