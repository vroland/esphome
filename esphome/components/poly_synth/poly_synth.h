#pragma once
#ifdef USE_ESP32
#include "daisysp_adsr.h"
#include "daisysp_oscillator.h"
#include "daisysp_svf.h"
#include "voice_state.h"
#include "esphome/components/speaker/speaker.h"
#include "esphome/core/component.h"
#include "esphome/core/static_task.h"
#include <atomic>
#include <freertos/queue.h>

namespace esphome::poly_synth {
class PolySynth : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  void set_output_speaker(speaker::Speaker *speaker) { output_ = speaker; }
  void set_polyphony(uint8_t count) { polyphony_ = count; }
  void set_gain(float gain) { gain_ = gain; }
  void set_detune(float cents) { detune_ratio_ = std::pow(2.0f, cents / 1200.0f); }
  void set_amp_envelope(float attack, float decay, float sustain, float release) {
    amp_ = {attack, decay, sustain, release};
  }
  void set_filter(float cutoff, float resonance) { cutoff_ = cutoff; resonance_ = resonance; }
  void set_filter_envelope(float amount, float attack, float decay, float sustain, float release) {
    filter_amount_ = amount; filter_env_ = {attack, decay, sustain, release};
  }
  void note_on(uint8_t note, float velocity = 1.0f);
  void note_off(uint8_t note);
  void all_notes_off();

 protected:
  static constexpr size_t MAX_VOICES = 16;
  static constexpr size_t BLOCK_FRAMES = 128;
  struct EnvelopeSettings { float attack, decay, sustain, release; };
  struct Voice {
    VoiceSlot slot;
    daisysp::Oscillator osc1, osc2;
    daisysp::Adsr amplitude, filter_envelope;
    daisysp::Svf filter;
    float velocity{0};
    float steal_tail{0};
  };
  enum class EventType : uint8_t { ON, OFF, ALL_OFF };
  struct Event { EventType type; uint8_t note; float velocity; };
  void enqueue_(Event event);
  void event_(const Event &event);
  void render_();
  static void task_entry_(void *arg);
  void task_main_();

  speaker::Speaker *output_{nullptr};
  QueueHandle_t queue_{nullptr};
  StaticTask task_;
  Voice voices_[MAX_VOICES];
  int16_t pcm_[BLOCK_FRAMES * 2]{};
  uint8_t polyphony_{12};
  uint32_t sequence_{0};
  float gain_{0.30f}, voice_gain_{0}, detune_ratio_{1.004051f};
  float cutoff_{1500}, resonance_{0.15f}, filter_amount_{2500};
  EnvelopeSettings amp_{0.005f, 0.25f, 0.55f, 0.3f};
  EnvelopeSettings filter_env_{0.002f, 0.3f, 0, 0.2f};
  std::atomic<bool> recover_{false};
  std::atomic<uint32_t> overflows_{0}, partial_writes_{0}, stalls_{0}, worst_render_us_{0};
  std::atomic<uint32_t> startup_failures_{0};
  std::atomic<uint8_t> active_voices_{0}, queue_peak_{0};
  uint32_t last_log_{0};
};
}  // namespace esphome::poly_synth
#ifdef USE_MATRIX_KEYPAD
#include "matrix_keypad_adapter.h"
#endif
#include "automation.h"
#endif
