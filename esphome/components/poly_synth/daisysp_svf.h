// Derived from DaisySP Source/Filters/svf.{h,cpp}, MIT license in DAISYSP_LICENSE.txt.
#pragma once
#include "daisysp_dsp.h"
namespace daisysp {
class Svf {
 public:
  void Init(float sample_rate) { rate_ = sample_rate; low_ = band_ = output_ = 0; SetFreq(200); SetRes(0.15f); }
  void SetFreq(float hz) {
    freq_ = 2.0f * std::sin(PI_F * std::min(0.25f, fclamp(hz, 1.0f, rate_ / 3) / (rate_ * 2)));
    update_damping_();
  }
  void SetRes(float resonance) {
    resonance_ = fclamp(resonance, 0, 1);
    resonance_damping_ = 2.0f * (1.0f - std::pow(resonance_, 0.25f));
    update_damping_();
  }
  void Process(float input) {
    output_ = 0;
    for (int i = 0; i < 2; i++) {
      float notch = input - damping_ * band_;
      low_ += freq_ * band_;
      float high = notch - low_;
      band_ += freq_ * high;
      output_ += 0.5f * low_;
    }
  }
  float Low() const { return output_; }

 private:
  void update_damping_() {
    damping_ = std::min(resonance_damping_,
                        std::min(2.0f, 2.0f / freq_ - freq_ * 0.5f));
  }
  float rate_{}, freq_{}, resonance_{}, resonance_damping_{2}, damping_{}, low_{}, band_{}, output_{};
};
}  // namespace daisysp
