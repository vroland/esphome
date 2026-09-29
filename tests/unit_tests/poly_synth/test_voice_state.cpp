#include "esphome/components/poly_synth/voice_state.h"
#include <cassert>
#include <cmath>
using namespace esphome::poly_synth;

int main() {
  assert(std::fabs(midi_frequency(69) - 440.0f) < 0.001f);
  assert(std::fabs(midi_frequency(81) - 880.0f) < 0.001f);
  VoiceSlot slots[3]{};
  assert(choose_voice(slots, 3, 69) == 0);
  slots[0] = {true, true, 69, 0.6f, 1};
  assert(choose_voice(slots, 3, 69) == 0);  // retrigger held note
  assert(choose_voice(slots, 3, 70) == 1);  // free slot
  slots[1] = {true, true, 70, 0.4f, 2};
  slots[2] = {true, true, 71, 0.8f, 3};
  assert(choose_voice(slots, 3, 72) == 0);  // oldest held voice
  release_note(slots, 3, 70);
  assert(!slots[1].gate && slots[0].gate && slots[2].gate);
  assert(choose_voice(slots, 3, 72) == 1);  // release first
  slots[2].gate = false;
  assert(choose_voice(slots, 3, 72) == 1);  // quietest release
  release_all(slots, 3);
  assert(!slots[0].gate && !slots[1].gate && !slots[2].gate);
}
