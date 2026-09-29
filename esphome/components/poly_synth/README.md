# poly_synth (ESP32 external component)

Use this component from this ESPHome fork using `external_components`; see
[`poly_synth_example.yaml`](../../../poly_synth_example.yaml) for a complete
PCM5102A / matrix keyboard / mixer configuration. The `media_mixer_input`
source is reserved for a future Sendspin or other media producer (configure
that producer to output **48 kHz** when mixing concurrently). The synth itself
does not access I2S. It writes interleaved 48-kHz signed 16-bit stereo PCM to
any ESPHome `speaker::Speaker` sink; the example uses an ESPHome mixer source.

`id`, `output_speaker`, `sample_rate` (currently only 48000), `polyphony`
(1–16, default 12), and `gain` (0–1, default 0.30) are supported. `oscillator`
accepts `waveform: polyblep_saw` and `detune_cents` (default +7). Optional
`amp_envelope` and `filter_envelope` accept attack, decay, sustain, release;
`filter_envelope` also accepts a frequency `amount`. `filter` accepts
`type: low_pass`, `cutoff`, `resonance`. See the example for defaults.

The optional `keypad: keyboard_id` registers a separate
`MatrixKeypadListener` adapter. Keys `0`–`9` / `A`–`Q` map to MIDI 69–95;
other codes are ignored. This requires the **forked diode matrix keypad**
support for simultaneous independent press/release events. Without `keypad`,
use `id(keyboard_synth).note_on(midi_note, velocity)`, `.note_off(midi_note)`,
`.all_notes_off()` or the `poly_synth.note_on`, `.note_off`,
`.all_notes_off` automation actions. DSP never depends on the keyboard.

## Audio / scheduling

Each active voice uses two detuned PolyBLEP saws, amplitude/filter ADSRs,
and a two-pass low-pass SVF. These are **adapted from DaisySP**, pinned to
[`2c72eaf9eac5fc0dca1919d65d606da907832618`](https://github.com/daisyaudio/DaisySP/commit/2c72eaf9eac5fc0dca1919d65d606da907832618),
licensed under MIT (see `DAISYSP_LICENSE.txt`). Only the required algorithms
are included: upstream uses `Source/` and has no PlatformIO `library.json`,
whereas ESPHome's external component loader copies only top-level component
files. The adapted subset removes unused waveforms and modules and caches
SVF resonance calculations. No libDaisy or hardware-specific code is linked.

A FreeRTOS task renders 128-frame blocks (2.67 ms) into a fixed PCM buffer;
the main loop only logs aggregated telemetry. A 64-event FreeRTOS queue
serializes note changes; overflow releases all gates to avoid stuck notes.
Allocation prefers a free voice, the quietest releasing voice, then the
oldest held voice. Held repeated notes retrigger the same voice. A very short
tail smooths forced steals. Filter cutoff is updated every 32 frames.
Polyphony headroom scales as `gain / sqrt(polyphony)` with a final saturation
safety bound.

The sink's stream info is set before playback. The render task requests
`start()` and waits for `is_running()` before rendering the first note, then
handles partial/blocked writes without spinning. After the final release,
the synth sends `finish()` after a 200-ms warm interval; a new note queues
while a mixer source is starting/stopping. Source, mixer and I2S startup
latencies are additional to the 2.67-ms render block. With the example's
source buffer of 20 ms and I2S buffer of 40 ms, measure actual key-to-DAC
latency on hardware before tightening those buffers.

Set the logger to `VERBOSE` to see 10-second aggregate readings: maximum
render time per block (compare with 2667 µs), queue high-water mark, active
voices, partial writes, stalls, and internal free heap. Queue overflow warns
at most every five seconds. These counters help profile the target ESP32;
compile success alone does **not** establish 12-voice CPU headroom or
hardware latency. A mixer may itself run at a 25-ms scheduling interval.

## Verification

Host allocator/frequency tests:

```sh
g++ -std=c++17 -I. tests/unit_tests/poly_synth/test_voice_state.cpp -o /tmp/poly_synth_voice_test
/tmp/poly_synth_voice_test
g++ -std=c++17 -I. tests/unit_tests/poly_synth/test_dsp.cpp -o /tmp/poly_synth_dsp_test
/tmp/poly_synth_dsp_test
```

Compile the example with `python -m esphome compile poly_synth_example.yaml`.
On the actual ESP32 verify held-note sustain and release, independent 3/6/10
note chords, repeated notes/runs and 13+ note stealing, idle restart after
several seconds, long playing with stable heap/watchdog/render times, and
concurrent 48-kHz media through `media_mixer_input`. No physical keyboard,
DAC, or Sendspin measurements are available from a compile-only test.
