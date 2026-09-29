// Derived from DaisySP Source/Utility/dsp.h, MIT license in DAISYSP_LICENSE.txt.
#pragma once
#include <algorithm>
#include <cmath>
namespace daisysp {
constexpr float PI_F = 3.14159265358979323846f;
inline float fclamp(float x, float lo, float hi) { return std::max(lo, std::min(x, hi)); }
}  // namespace daisysp
