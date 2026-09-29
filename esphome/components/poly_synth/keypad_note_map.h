#pragma once

#include <cstdint>

namespace esphome::poly_synth {

// The keys pointer refers to a code-generated string literal with firmware lifetime.
// Matrix key codes are bytes; schema validation restricts keys to ASCII.
class KeypadNoteMap {
 public:
  KeypadNoteMap(const char *keys, uint8_t base_note) : keys_(keys), base_note_(base_note) {}

  int note_for(uint8_t key) const {
    for (uint8_t i = 0; this->keys_[i] != '\0'; i++) {
      if (static_cast<uint8_t>(this->keys_[i]) == key)
        return this->base_note_ + i;
    }
    return -1;
  }

  template<typename Synth> void press(Synth *synth, uint8_t key, float velocity) const {
    const int note = this->note_for(key);
    if (note >= 0)
      synth->note_on(static_cast<uint8_t>(note), velocity);
  }

  template<typename Synth> void release(Synth *synth, uint8_t key) const {
    const int note = this->note_for(key);
    if (note >= 0)
      synth->note_off(static_cast<uint8_t>(note));
  }

 protected:
  const char *keys_;
  uint8_t base_note_;
};

}  // namespace esphome::poly_synth
