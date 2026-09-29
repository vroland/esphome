// Derived from DaisySP Source/Synthesis/oscillator.{h,cpp}, MIT license in DAISYSP_LICENSE.txt.
#pragma once
#include <cmath>
namespace daisysp {
class Oscillator {
 public:
  enum { WAVE_POLYBLEP_SAW };
  void Init(float sample_rate) { reciprocal_ = 1.0f / sample_rate; phase_ = 0; SetFreq(100); }
  void SetFreq(float hz) { increment_ = hz * reciprocal_; }
  void SetWaveform(unsigned char waveform) { (void) waveform; }
  void Reset(float phase = 0) { phase_ = phase; }
  float Process() {
    // DaisySP's WAVE_POLYBLEP_SAW: band-limit the discontinuity at phase wrap.
    const float t = phase_;
    float correction = 0;
    if (t < increment_) {
      float x = t / increment_;
      correction = x + x - x * x - 1.0f;
    } else if (t > 1.0f - increment_) {
      float x = (t - 1.0f) / increment_;
      correction = x * x + x + x + 1.0f;
    }
    phase_ += increment_;
    if (phase_ >= 1.0f) phase_ -= 1.0f;
    return -((2.0f * t - 1.0f) - correction);
  }

 private:
  float reciprocal_{}, phase_{}, increment_{};
};
}  // namespace daisysp
