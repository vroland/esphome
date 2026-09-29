#include "poly_synth.h"
#ifdef USE_ESP32
#include "esphome/components/audio/audio.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include <algorithm>
#include <cmath>
#include <esp_heap_caps.h>

namespace esphome::poly_synth {
static const char *const TAG = "poly_synth";
static constexpr uint32_t IDLE_MS = 200;

void PolySynth::setup() {
  voice_gain_ = gain_ / std::sqrt(float(polyphony_));
  queue_ = xQueueCreate(64, sizeof(Event));
  if (queue_ == nullptr) { mark_failed(); return; }
  output_->set_audio_stream_info(audio::AudioStreamInfo(16, 2, 48000));
  for (size_t i = 0; i < polyphony_; i++) {
    Voice &v = voices_[i];
    v.osc1.Init(48000); v.osc2.Init(48000);
    v.osc1.SetWaveform(daisysp::Oscillator::WAVE_POLYBLEP_SAW);
    v.osc2.SetWaveform(daisysp::Oscillator::WAVE_POLYBLEP_SAW);
    v.osc2.Reset(0.5f);
    v.amplitude.Init(48000); v.filter_envelope.Init(48000); v.filter.Init(48000);
    v.amplitude.SetAttackTime(amp_.attack); v.amplitude.SetDecayTime(amp_.decay);
    v.amplitude.SetSustainLevel(amp_.sustain); v.amplitude.SetReleaseTime(amp_.release);
    v.filter_envelope.SetAttackTime(filter_env_.attack); v.filter_envelope.SetDecayTime(filter_env_.decay);
    v.filter_envelope.SetSustainLevel(filter_env_.sustain); v.filter_envelope.SetReleaseTime(filter_env_.release);
    v.filter.SetRes(resonance_); v.filter.SetFreq(cutoff_);
  }
  ESP_LOGD(TAG, "Heap before audio task: %u", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
  if (!task_.create(task_entry_, "poly_synth", 4096, this, 9, false)) { mark_failed(); return; }
}

void PolySynth::dump_config() {
  ESP_LOGCONFIG(TAG, "Poly Synth: 48000 Hz / 16-bit / stereo, %u voices, gain %.2f, LP %.0f Hz (res %.2f)",
                polyphony_, gain_, cutoff_, resonance_);
  ESP_LOGCONFIG(TAG, "  Detune: %.1f cents; amp ADSR: %.0f/%.0f/%.2f/%.0f ms", 1200 * std::log2(detune_ratio_),
                amp_.attack * 1000, amp_.decay * 1000, amp_.sustain, amp_.release * 1000);
  ESP_LOGCONFIG(TAG, "  Filter envelope: %.0f Hz, %.0f/%.0f/%.2f/%.0f ms", filter_amount_,
                filter_env_.attack * 1000, filter_env_.decay * 1000, filter_env_.sustain, filter_env_.release * 1000);
}

void PolySynth::loop() {
  uint32_t now = App.get_loop_component_start_time();
  if (overflows_.load() && now - last_log_ > 5000) {
    last_log_ = now;
    ESP_LOGW(TAG, "Event queue overflow (%" PRIu32 "); voices reset", overflows_.exchange(0));
  }
  if (startup_failures_.load() && now - last_log_ > 5000) {
    last_log_ = now;
    ESP_LOGW(TAG, "Output speaker did not start (%" PRIu32 " retries)", startup_failures_.exchange(0));
  }
  if (now - last_log_ > 10000 && ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE) {
    last_log_ = now;
    ESP_LOGV(TAG, "voices=%u queue_peak=%u render_worst=%" PRIu32 "us partial=%" PRIu32
                  " stalls=%" PRIu32 " heap=%u",
             active_voices_.load(), queue_peak_.exchange(0), worst_render_us_.exchange(0),
             partial_writes_.exchange(0), stalls_.exchange(0), heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
  }
}

void PolySynth::enqueue_(Event event) {
  if (queue_ == nullptr || xQueueSend(queue_, &event, 0) != pdTRUE) {
    recover_.store(true);
    overflows_.fetch_add(1);
  }
}
void PolySynth::note_on(uint8_t note, float velocity) {
  if (note > 127 || !std::isfinite(velocity)) return;
  enqueue_({EventType::ON, note, std::max(0.0f, std::min(velocity, 1.0f))});
}
void PolySynth::note_off(uint8_t note) { enqueue_({EventType::OFF, note, 0}); }
void PolySynth::all_notes_off() { enqueue_({EventType::ALL_OFF, 0, 0}); }

void PolySynth::event_(const Event &event) {
  if (event.type == EventType::ALL_OFF) {
    for (size_t i = 0; i < polyphony_; i++) voices_[i].slot.gate = false;
  } else if (event.type == EventType::OFF) {
    for (size_t i = 0; i < polyphony_; i++)
      if (voices_[i].slot.active && voices_[i].slot.note == event.note) voices_[i].slot.gate = false;
  } else if (event.velocity > 0) {
    VoiceSlot slots[MAX_VOICES];
    for (size_t i = 0; i < polyphony_; i++) slots[i] = voices_[i].slot;
    Voice &v = voices_[choose_voice(slots, polyphony_, event.note)];
    bool repeated = v.slot.active && v.slot.gate && v.slot.note == event.note;
    // A short decay of the replaced sample smooths forced steals without extending the old gate.
    v.steal_tail = repeated ? 0 : (v.slot.active ? v.slot.level * v.velocity * v.filter.Low() : 0);
    v.slot = {true, true, event.note, 0, ++sequence_};
    v.velocity = event.velocity;
    float hz = midi_frequency(event.note);
    v.osc1.SetFreq(hz); v.osc2.SetFreq(hz * detune_ratio_);
    if (!repeated) { v.osc1.Reset(); v.osc2.Reset(0.5f); v.filter.Init(48000); v.filter.SetRes(resonance_); }
    v.amplitude.Retrigger(!repeated);
    v.filter_envelope.Retrigger(!repeated);
  }
}

void PolySynth::render_() {
  uint32_t start = micros();
  uint8_t active = 0;
  for (size_t v = 0; v < polyphony_; v++) {
    Voice &voice = voices_[v];
    if (!voice.slot.active) continue;
    active++;
  }
  for (size_t i = 0; i < BLOCK_FRAMES; i++) {
    float sample = 0;
    for (size_t j = 0; j < polyphony_; j++) {
      Voice &v = voices_[j];
      if (!v.slot.active) continue;
      float env = v.amplitude.Process(v.slot.gate);
      float filter_env = v.filter_envelope.Process(v.slot.gate);
      v.slot.level = env;
      if (!v.amplitude.IsRunning() && !v.slot.gate) { v.slot.active = false; continue; }
      // 32-sample control interval (~0.67 ms), without trig in every sample.
      if (i % 32 == 0)
        v.filter.SetFreq(std::max(20.0f, std::min(15000.0f, cutoff_ + filter_amount_ * filter_env)));
      v.filter.Process(0.5f * (v.osc1.Process() + v.osc2.Process()));
      sample += v.filter.Low() * env * v.velocity;
      sample += v.steal_tail;
      v.steal_tail *= 0.97f;  // ~0.7 ms fade at 48 kHz
    }
    sample *= voice_gain_;
    // Final safety clamp; normal chords retain dynamic range through polyphony headroom.
    int16_t pcm = static_cast<int16_t>(std::max(-32767.0f, std::min(32767.0f, sample * 32767.0f)));
    pcm_[2 * i] = pcm_[2 * i + 1] = pcm;
  }
  active_voices_.store(active);
  uint32_t elapsed = micros() - start;
  uint32_t worst = worst_render_us_.load();
  while (elapsed > worst && !worst_render_us_.compare_exchange_weak(worst, elapsed)) {}
}

void PolySynth::task_entry_(void *arg) { static_cast<PolySynth *>(arg)->task_main_(); }

void PolySynth::task_main_() {
  bool started = false;
  bool stopping = false;
  uint32_t idle_since = 0;
  uint32_t waiting_since = 0;
  size_t pending_offset = sizeof(pcm_);
  while (true) {
    if (recover_.exchange(false)) {
      xQueueReset(queue_);
      event_({EventType::ALL_OFF, 0, 0});
    }
    uint8_t depth = uxQueueMessagesWaiting(queue_);
    if (depth > queue_peak_.load()) queue_peak_.store(depth);
    Event event;
    while (xQueueReceive(queue_, &event, 0) == pdTRUE) event_(event);
    bool active = false;
    for (size_t i = 0; i < polyphony_; i++) active |= voices_[i].slot.active;
    active |= pending_offset < sizeof(pcm_);
    if (!active) {
      if (started && !idle_since) idle_since = millis();
      if (started && millis() - idle_since >= IDLE_MS) {
        output_->finish();
        stopping = true;
        started = false;
      }
      if (!started) {
        if (xQueueReceive(queue_, &event, pdMS_TO_TICKS(10)) == pdTRUE) event_(event);
        continue;
      }
    } else idle_since = 0;
    // SourceSpeaker processes finish asynchronously. Wait for STOPPED before
    // requesting a new stream; otherwise a stale finish can cut off its attack.
    if (stopping) {
      if (!output_->is_stopped()) { vTaskDelay(pdMS_TO_TICKS(1)); continue; }
      stopping = false;
    }
    if (!started) { output_->start(); started = true; waiting_since = millis(); }
    if (!output_->is_running()) {
      if (output_->is_stopped() && millis() - waiting_since > 250) {
        output_->start();
        waiting_since = millis();
        startup_failures_.fetch_add(1);
      }
      vTaskDelay(pdMS_TO_TICKS(1));
      continue;
    }
    if (pending_offset == sizeof(pcm_)) { render_(); pending_offset = 0; }
    uint32_t write_since = millis();
    while (pending_offset < sizeof(pcm_)) {
      if (!output_->is_running()) { started = false; break; }
      size_t written = output_->play(reinterpret_cast<const uint8_t *>(pcm_) + pending_offset,
                                     sizeof(pcm_) - pending_offset, pdMS_TO_TICKS(5));
      if (written != sizeof(pcm_) - pending_offset) partial_writes_.fetch_add(1);
      if (written == 0) {
        stalls_.fetch_add(1);
        vTaskDelay(pdMS_TO_TICKS(1));
        // Return to the event queue regularly even if the sink is wedged.
        if (millis() - write_since >= 10) break;
      } else {
        write_since = millis();
      }
      pending_offset += written;
    }
  }
}
}  // namespace esphome::poly_synth
#endif
