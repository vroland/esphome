#include "esphome/components/poly_synth/keypad_note_map.h"

#include <cassert>
#include <cstdint>

using esphome::poly_synth::KeypadNoteMap;

struct FakeSynth {
  bool held[128]{};
  float last_velocity{0};
  unsigned on_count{0};
  unsigned off_count{0};

  void note_on(uint8_t note, float velocity) {
    held[note] = true;
    last_velocity = velocity;
    on_count++;
  }
  void note_off(uint8_t note) {
    held[note] = false;
    off_count++;
  }
};

int main() {
  const KeypadNoteMap notes("012ABC", 69);
  assert(notes.note_for('0') == 69);
  assert(notes.note_for('1') == 70);
  assert(notes.note_for('2') == 71);
  assert(notes.note_for('A') == 72);
  assert(notes.note_for('B') == 73);
  assert(notes.note_for('C') == 74);
  assert(notes.note_for('X') == -1);

  FakeSynth synth;
  notes.press(&synth, '0', 0.7f);
  notes.press(&synth, 'A', 0.7f);
  notes.press(&synth, 'C', 0.7f);
  assert(synth.held[69] && synth.held[72] && synth.held[74]);
  assert(synth.on_count == 3 && synth.last_velocity == 0.7f);

  notes.release(&synth, 'A');
  assert(synth.held[69] && !synth.held[72] && synth.held[74]);
  assert(synth.off_count == 1);

  notes.press(&synth, 'X', 1.0f);
  notes.release(&synth, 'X');
  assert(synth.on_count == 3 && synth.off_count == 1);

  const KeypadNoteMap high_note("AB", 126);
  assert(high_note.note_for('B') == 127);
}
