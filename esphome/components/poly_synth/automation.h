#pragma once
#include "poly_synth.h"
#include "esphome/core/automation.h"

namespace esphome::poly_synth {

template<typename... Ts> class NoteOnAction : public Action<Ts...> {
 public:
  explicit NoteOnAction(PolySynth *synth) : synth_(synth) {}
  TEMPLATABLE_VALUE(uint8_t, note)
  TEMPLATABLE_VALUE(float, velocity)

  void play(const Ts &...x) override { this->synth_->note_on(this->note_.value(x...), this->velocity_.value(x...)); }

 protected:
  PolySynth *synth_;
};

template<typename... Ts> class NoteOffAction : public Action<Ts...> {
 public:
  explicit NoteOffAction(PolySynth *synth) : synth_(synth) {}
  TEMPLATABLE_VALUE(uint8_t, note)

  void play(const Ts &...x) override { this->synth_->note_off(this->note_.value(x...)); }

 protected:
  PolySynth *synth_;
};

template<typename... Ts> class AllNotesOffAction : public Action<Ts...> {
 public:
  explicit AllNotesOffAction(PolySynth *synth) : synth_(synth) {}

  void play(const Ts &...x) override { this->synth_->all_notes_off(); }

 protected:
  PolySynth *synth_;
};

}  // namespace esphome::poly_synth
