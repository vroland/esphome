#pragma once
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace esphome::poly_synth {
inline float midi_frequency(uint8_t note) { return 440.0f * std::pow(2.0f, (int(note) - 69) / 12.0f); }

// DSP-independent allocation policy; only the audio task accesses these slots.
struct VoiceSlot {
  bool active{false};
  bool gate{false};
  uint8_t note{0};
  float level{0};
  uint32_t age{0};
};

inline size_t choose_voice(const VoiceSlot *slots, size_t count, uint8_t note) {
  for (size_t i = 0; i < count; i++)
    if (slots[i].active && slots[i].gate && slots[i].note == note) return i;
  for (size_t i = 0; i < count; i++)
    if (!slots[i].active) return i;
  size_t candidate = count;
  for (size_t i = 0; i < count; i++)
    if (!slots[i].gate && (candidate == count || slots[i].level < slots[candidate].level)) candidate = i;
  if (candidate != count) return candidate;
  candidate = 0;
  for (size_t i = 1; i < count; i++)
    if (int32_t(slots[i].age - slots[candidate].age) < 0) candidate = i;
  return candidate;
}

inline void release_note(VoiceSlot *slots, size_t count, uint8_t note) {
  for (size_t i = 0; i < count; i++)
    if (slots[i].active && slots[i].note == note) slots[i].gate = false;
}
inline void release_all(VoiceSlot *slots, size_t count) {
  for (size_t i = 0; i < count; i++) slots[i].gate = false;
}
}  // namespace esphome::poly_synth
