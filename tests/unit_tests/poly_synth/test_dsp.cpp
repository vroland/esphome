#include "esphome/components/poly_synth/daisysp_adsr.h"
#include "esphome/components/poly_synth/daisysp_oscillator.h"
#include "esphome/components/poly_synth/daisysp_svf.h"
#include <cassert>
#include <cmath>

int main() {
  daisysp::Oscillator oscillator;
  daisysp::Adsr envelope;
  daisysp::Svf filter;
  oscillator.Init(48000);
  oscillator.SetFreq(440);
  envelope.Init(48000);
  envelope.SetAttackTime(0.005f);
  envelope.SetDecayTime(0.05f);
  envelope.SetSustainLevel(0.55f);
  envelope.SetReleaseTime(0.03f);
  filter.Init(48000);
  filter.SetFreq(1500);

  float energy = 0;
  for (int i = 0; i < 48000; i++) {
    float amp = envelope.Process(true);
    filter.Process(oscillator.Process());
    float sample = amp * filter.Low();
    assert(std::isfinite(sample));
    energy += sample * sample;
  }
  assert(energy > 100);
  assert(envelope.IsRunning());
  for (int i = 0; i < 48000; i++) envelope.Process(false);
  assert(!envelope.IsRunning());

  // A tap can be fully queued before rendering starts (speaker startup, or
  // note-on and note-off in one block). Retrigger must not sustain with gate=false.
  envelope.Retrigger(true);
  for (int i = 0; i < 48000; i++) envelope.Process(false);
  assert(!envelope.IsRunning());
  assert(envelope.Process(false) == 0.0f);

  // Re-press during a release, then release again before any held sample.
  for (int i = 0; i < 48000; i++) envelope.Process(true);
  for (int i = 0; i < 128; i++) envelope.Process(false);
  envelope.Retrigger(false);
  for (int i = 0; i < 48000; i++) envelope.Process(false);
  assert(!envelope.IsRunning());
  assert(envelope.Process(false) == 0.0f);

  // Normal held notes and release still work after those rapid taps.
  envelope.Retrigger(true);
  for (int i = 0; i < 48000; i++) envelope.Process(true);
  assert(envelope.IsRunning());
  assert(std::fabs(envelope.Process(true) - 0.55f) < 0.001f);
  for (int i = 0; i < 48000; i++) envelope.Process(false);
  assert(!envelope.IsRunning());
}
