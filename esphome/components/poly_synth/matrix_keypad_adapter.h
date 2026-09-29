#pragma once
#include "poly_synth.h"
#include "esphome/components/matrix_keypad/matrix_keypad.h"
namespace esphome::poly_synth {
class MatrixKeypadAdapter : public matrix_keypad::MatrixKeypadListener {
 public:
  explicit MatrixKeypadAdapter(PolySynth *synth) : synth_(synth) {}
  void key_pressed(uint8_t key) override { if (int note = translate_(key); note >= 0) synth_->note_on(note); }
  void key_released(uint8_t key) override { if (int note = translate_(key); note >= 0) synth_->note_off(note); }
 protected:
  static int translate_(uint8_t key) {
    if (key >= '0' && key <= '9') return 69 + key - '0';
    if (key >= 'A' && key <= 'Q') return 79 + key - 'A';
    return -1;
  }
  PolySynth *synth_;
};
}  // namespace esphome::poly_synth
