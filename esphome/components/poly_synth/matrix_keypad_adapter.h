#pragma once
#ifdef USE_POLY_SYNTH_KEYPAD
#include "keypad_note_map.h"
#include "poly_synth.h"
#include "esphome/components/matrix_keypad/matrix_keypad.h"

namespace esphome::poly_synth {
class MatrixKeypadAdapter : public matrix_keypad::MatrixKeypadListener {
 public:
  MatrixKeypadAdapter(PolySynth *synth, const char *keys, uint8_t base_note, float velocity)
      : synth_(synth), notes_(keys, base_note), velocity_(velocity) {}

  void key_pressed(uint8_t key) override { this->notes_.press(this->synth_, key, this->velocity_); }

  void key_released(uint8_t key) override { this->notes_.release(this->synth_, key); }

 protected:
  PolySynth *synth_;
  KeypadNoteMap notes_;
  float velocity_;
};
}  // namespace esphome::poly_synth
#endif  // USE_POLY_SYNTH_KEYPAD
