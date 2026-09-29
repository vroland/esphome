// Derived from DaisySP Source/Control/adsr.{h,cpp}, MIT license in DAISYSP_LICENSE.txt.
#pragma once
#include <cmath>
namespace daisysp {
class Adsr {
 public:
  enum Stage { IDLE, ATTACK, DECAY, RELEASE };
  void Init(float sample_rate) { sample_rate_ = sample_rate; level_ = 0; gate_ = false; stage_ = IDLE; }
  void SetAttackTime(float seconds) { attack_ = coefficient_(seconds, 1.01f); }
  void SetDecayTime(float seconds) { decay_ = coefficient_(seconds); }
  void SetReleaseTime(float seconds) { release_ = coefficient_(seconds); }
  void SetSustainLevel(float level) { sustain_ = level <= 0 ? -0.01f : level; }
  void Retrigger(bool hard) { if (hard) level_ = 0; stage_ = ATTACK; }
  float Process(bool gate) {
    if (gate && !gate_) stage_ = ATTACK;
    if (!gate && gate_) stage_ = RELEASE;
    gate_ = gate;
    if (stage_ == ATTACK) {
      level_ += attack_ * (1.01f - level_);
      if (level_ >= 1) { level_ = 1; stage_ = DECAY; }
    } else if (stage_ == DECAY || stage_ == RELEASE) {
      level_ += (stage_ == DECAY ? decay_ : release_) *
                ((stage_ == DECAY ? sustain_ : -0.01f) - level_);
      if (level_ < 0) { level_ = 0; stage_ = IDLE; }
    }
    return level_;
  }
  bool IsRunning() const { return stage_ != IDLE; }

 private:
  float coefficient_(float seconds, float target = 1.0f) const {
    if (seconds <= 0) return 1;
    return 1.0f - std::exp((target == 1.0f ? -1.0f : std::log(1.0f - 1.0f / target)) /
                           (seconds * sample_rate_));
  }
  float sample_rate_{}, level_{}, attack_{}, decay_{}, release_{}, sustain_{};
  bool gate_{};
  Stage stage_{IDLE};
};
}  // namespace daisysp
