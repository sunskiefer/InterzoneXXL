// SPDX-License-Identifier: GPL-3.0-or-later
// Offline tests for InterzoneXXL on x86 (run under ASan + UBSan by test/run_tests.sh). They drive the engine the way
// the wrapper does (set_param strings, MIDI messages, int16 blocks) and check it two ways:
//   - against Valley's own Interzone module code (test/make_ref.py builds it from the vendored Interzone.cpp, on
//     test/mock_rack.hpp), fed the same notes as VCV's MIDI-CV module would: the outputs must match;
//   - on what can be heard: pitch, octaves, mono legato and retrigger, poly voices and stealing, the sustain pedal,
//     release to silence, Panic, stability at the extremes, any block size, and the display text.
// `interzone_test --wav DIR` also writes a few renders to listen to.
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <string>
#include <functional>
#include <vector>

#include "limiter.h"
#include "limiter_ref.h"

extern "C" {
#include "engine.h"
void* ref_create();
void ref_destroy(void* m);
void ref_param(void* m, int id, float v);
void ref_input(void* m, int id, int channels, const float* v);
void ref_process(void* m);
float ref_output(void* m, int id, int c);
int ref_output_channels(void* m, int id);
int interzonexxl_test_voice_gate(void* inst, int v);
}

namespace {

const int kRate = 44100;
const int kLatency = ixxl::Limiter::kLookahead;   // the limiter's look-ahead delays the output
int failures = 0;
const char* wav_dir = NULL;

void Check(bool ok, const char* what) {
  printf("%s %s\n", ok ? "ok  " : "FAIL", what);
  if (!ok) ++failures;
}

// Interzone.hpp's ids, for the reference module.
enum RefParam {
  OCTAVE_PARAM, COARSE_PARAM, FINE_PARAM, PITCH_MOD_PARAM, PITCH_MOD_ENV_POL_PARAM, PITCH_MOD_SOURCE_PARAM, PW_PARAM,
  PW_MOD_PARAM, PW_MOD_SOURCE_PARAM, PW_MOD_ENV_POL_PARAM, COARSE_MODE_PARAM, GLIDE_PARAM, SUB_OCTAVE_PARAM,
  SUB_WAVE_PARAM, NOISE_TYPE_PARAM, SAW_LEVEL_PARAM, PULSE_LEVEL_PARAM, SUB_LEVEL_PARAM, NOISE_LEVEL_PARAM,
  EXT_LEVEL_PARAM, FILTER_CUTOFF_PARAM, FILTER_Q_PARAM, FILTER_HPF_PARAM, FILTER_POLES_PARAM, FILTER_MOD_PARAM,
  FILTER_VOCT_PARAM, FILTER_ENV_PARAM, FILTER_ENV_POL_PARAM, FILTER_CV_1_PARAM, FILTER_CV_2_PARAM, LFO_RATE_PARAM,
  LFO_FINE_PARAM, LFO_SLEW_PARAM, LFO_WAVE_PARAM, ENV_ATTACK_PARAM, ENV_DECAY_PARAM, ENV_SUSTAIN_PARAM,
  ENV_RELEASE_PARAM, ENV_LENGTH_PARAM, ENV_CYCLE_PARAM, ENV_MANUAL_PARAM, VCA_SOURCE_PARAM, VCA_LEVEL_CV_PARAM
};
enum RefInput { VOCT_INPUT_1, VOCT_INPUT_2, PW_MOD_INPUT, GATE_INPUT, TRIG_INPUT };
const int VCA_OUTPUT = 5;

struct Plug {
  const mpc_engine_t* e;
  void* inst;
  explicit Plug(const char* dir = "build/dsptest/presets") : e(mpc_engine()), inst(e->create(dir)) {}
  ~Plug() { e->destroy(inst); }
  void Set(const char* k, double v) {
    char b[32];
    snprintf(b, sizeof b, "%g", v);
    e->set_param(inst, k, b);
  }
  std::string Get(const char* k) {
    char b[8192] = {0};
    e->get_param(inst, k, b, sizeof b);
    return b;
  }
  std::string Text(const char* k) {
    char key[64], b[64] = {0};
    snprintf(key, sizeof key, "%s_display", k);
    e->get_param(inst, key, b, sizeof b);
    return b;
  }
  void Midi(int a, int b, int c) {
    uint8_t m[3] = { static_cast<uint8_t>(a), static_cast<uint8_t>(b), static_cast<uint8_t>(c) };
    e->midi(inst, m, 3);
  }
  void On(int note) { Midi(0x90, note, 100); }
  void Off(int note) { Midi(0x80, note, 0); }
  // Run n frames in blocks of `block`; append the left channel as floats.
  void Run(int frames, std::vector<float>* out, int block = 128) {
    std::vector<int16_t> o(2 * 128);
    for (int f = 0; f < frames; f += block) {
      int n = frames - f < block ? frames - f : block;
      e->render(inst, o.data(), n);
      for (int i = 0; i < n; ++i) {
        if (out) out->push_back(o[2 * i] / 32768.0f);
        if (o[2 * i] != o[2 * i + 1]) Check(false, "left and right differ");
      }
    }
  }
  int VoiceNote(int v) { return interzonexxl_test_voice_gate(inst, v); }
};

void WriteWav(const char* name, const std::vector<float>& x) {
  if (!wav_dir) return;
  char path[512];
  snprintf(path, sizeof path, "%s/%s.wav", wav_dir, name);
  FILE* f = fopen(path, "wb");
  if (!f) return;
  uint32_t n = static_cast<uint32_t>(x.size()), bytes = n * 2, rate = kRate, brate = kRate * 2;
  uint16_t one = 1, two = 2, bits = 16;
  uint32_t riff = 36 + bytes, fmt = 16;
  fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f); fwrite(&fmt, 4, 1, f);
  fwrite(&one, 2, 1, f); fwrite(&one, 2, 1, f); fwrite(&rate, 4, 1, f); fwrite(&brate, 4, 1, f);
  fwrite(&two, 2, 1, f); fwrite(&bits, 2, 1, f); fwrite("data", 1, 4, f); fwrite(&bytes, 4, 1, f);
  for (float v : x) {
    float c = v * 32767.0f;
    int16_t s = static_cast<int16_t>(c > 32767.0f ? 32767.0f : (c < -32768.0f ? -32768.0f : c));
    fwrite(&s, 2, 1, f);
  }
  fclose(f);
}

float Rms(const std::vector<float>& x, size_t a, size_t b) {
  double s = 0;
  for (size_t i = a; i < b && i < x.size(); ++i) s += x[i] * x[i];
  return b > a ? static_cast<float>(sqrt(s / (b - a))) : 0.0f;
}

float Peak(const std::vector<float>& x, size_t a, size_t b) {
  float p = 0;
  for (size_t i = a; i < b && i < x.size(); ++i) p = fmaxf(p, fabsf(x[i]));
  return p;
}

bool Finite(const std::vector<float>& x) {
  for (float v : x)
    if (!(v == v) || fabsf(v) > 1.5f) return false;
  return true;
}

// Fundamental by autocorrelation over [a, b), searching 30 Hz .. 2 kHz.
float Pitch(const std::vector<float>& x, size_t a, size_t b) {
  int lo = kRate / 2000, hi = kRate / 30;
  double best = -1e30;
  int lag = 0;
  std::vector<double> r(hi + 2, 0.0);
  for (int l = lo; l <= hi + 1; ++l) {
    double s = 0;
    for (size_t i = a; i + l < b; ++i) s += x[i] * x[i + l];
    r[l] = s;
  }
  for (int l = lo + 1; l <= hi; ++l)
    if (r[l] > r[l - 1] && r[l] >= r[l + 1] && r[l] > best * 1.02) { best = r[l]; lag = l; }
  if (!lag) return 0;
  // the first peak within 10 % of the best is the period (not a multiple of it)
  for (int l = lo + 1; l <= hi; ++l)
    if (r[l] > r[l - 1] && r[l] >= r[l + 1] && r[l] > 0.9 * best) { lag = l; break; }
  double y0 = r[lag - 1], y1 = r[lag], y2 = r[lag + 1];
  double d = (y0 - y2) / (2.0 * (y0 - 2.0 * y1 + y2));
  return static_cast<float>(kRate / (lag + d));
}

// ------------------------------------------------------------------------------------------------ reference
// One scenario: parameters for both, then a list of note events; the reference gets what VCV's MIDI-CV would send.
struct Event {
  int frame;
  int on;      // 1 note on, 0 note off
  int note;
};

struct RefSetting {
  const char* key;
  double value;     // the plugin's value
  int ref_id;
  double ref_value; // the module's value
};

// Compare the plugin (Level 1, so its output is the summed VCA volts / 10) with the module over `frames` frames.
// Mono: one MIDI-CV channel with last-note priority; poly: channel = the voice the plugin chose (read back).
float CompareWithModule(const char* name, const std::vector<RefSetting>& settings, const std::vector<Event>& events,
                        int frames, bool poly, int voices, bool retrig = false,
                        const std::vector<std::pair<const char*, double> >& patch = {},
                        std::function<void(long, void*)> ref_extra = nullptr) {
  Plug p;
  void* m = ref_create();
  p.Set("level", 1.0);
  p.Set("lim_ceiling", 0.0);
  if (poly) {
    p.Set("voice_mode", 1);
    p.Set("voices", voices);
  }
  if (retrig) p.Set("mono_trig", 1);
  for (const auto& kv : patch) p.Set(kv.first, kv.second);
  long total = 0;   // frames since both were created (the plugin's tempo clock counts from creation)
  for (const RefSetting& s : settings) {
    p.Set(s.key, s.value);
    ref_param(m, s.ref_id, static_cast<float>(s.ref_value));
  }
  const int channels = poly ? voices : 1;
  float voct[16] = {0}, gate[16] = {0}, trig[16] = {0};
  int trig_left[16] = {0};
  std::vector<int> held;
  std::vector<float> got, want;
  size_t ev = 0;
  std::vector<int16_t> o(2);
  // VCV runs a module long before a note comes: warm both up first (the module reads its knobs on its 16th sample)
  const int warm = 2048;
  for (int f = 0; f < warm; ++f) {
    ref_input(m, VOCT_INPUT_1, channels, voct);
    ref_input(m, GATE_INPUT, channels, gate);
    ref_input(m, TRIG_INPUT, channels, trig);
    if (ref_extra) ref_extra(total, m);
    ++total;
    ref_process(m);
    p.e->render(p.inst, o.data(), 1);
  }
  for (int f = 0; f < frames; ++f) {
    while (ev < events.size() && events[ev].frame == f) {
      const Event& e = events[ev++];
      if (e.on) p.On(e.note);
      else p.Off(e.note);
      if (poly) {
        for (int v = 0; v < channels; ++v) {
          int n = p.VoiceNote(v);
          if (n >= 0) {
            voct[v] = (n - 60) / 12.0f;
            gate[v] = 10;
          } else {
            gate[v] = 0;
          }
        }
      } else {
        for (size_t i = 0; i < held.size(); ++i)
          if (held[i] == e.note) { held.erase(held.begin() + i); break; }
        if (e.on) held.push_back(e.note);
        if (!held.empty()) {
          bool was = gate[0] > 0;
          voct[0] = (held.back() - 60) / 12.0f;
          gate[0] = 10;
          if (retrig && was) { trig[0] = 10; trig_left[0] = 44; }
        } else {
          gate[0] = 0;
        }
      }
    }
    ref_input(m, VOCT_INPUT_1, channels, voct);
    ref_input(m, GATE_INPUT, channels, gate);
    ref_input(m, TRIG_INPUT, channels, trig);
    if (ref_extra) ref_extra(total, m);
    ++total;
    ref_process(m);
    for (int c = 0; c < 16; ++c)
      if (trig_left[c] > 0 && --trig_left[c] == 0) trig[c] = 0;
    float sum = 0;
    for (int c = 0; c < ref_output_channels(m, VCA_OUTPUT); ++c) sum += ref_output(m, VCA_OUTPUT, c);
    want.push_back(sum / 10.0f);
    p.e->render(p.inst, o.data(), 1);   // one frame at a time, as SAMPLE_ACCURATE allows
    got.push_back(o[0] / 32768.0f);
  }
  ref_destroy(m);
  double err = 0, sig = 0;
  for (int f = 0; f + kLatency < frames; ++f) {
    double d = got[f + kLatency] - want[f];
    err += d * d;
    sig += want[f] * want[f];
  }
  float rel = sig > 0 ? static_cast<float>(sqrt(err / sig)) : 1.0f;
  char what[160];
  snprintf(what, sizeof what, "%s: matches the module (relative error %.2e, signal rms %.3f)", name, rel,
           static_cast<float>(sqrt(sig / frames)));
  Check(sig > 0 && rel < 2e-3, what);
  WriteWav(name, got);
  return rel;
}

void TestAgainstModule() {
  const int S = kRate / 4;
  // the plugin's slider/switch values -> the module's parameter values
  CompareWithModule("ref-mono-default", {}, {{0, 1, 60}, {S, 0, 60}, {S + 2000, 1, 67}, {2 * S, 0, 67}}, 3 * S,
                    false, 1);
  CompareWithModule("ref-mono-legato-glide",
                    {{"glide", 0.5, GLIDE_PARAM, 0.5}, {"cutoff", 6.5, FILTER_CUTOFF_PARAM, 6.5},
                     {"res", 4, FILTER_Q_PARAM, 4}, {"release", 0.5, ENV_RELEASE_PARAM, 0.5}},
                    {{0, 1, 48}, {S, 1, 60}, {2 * S, 1, 55}, {2 * S + 500, 0, 60}, {3 * S, 0, 48}, {3 * S, 0, 55}},
                    4 * S, false, 1);
  CompareWithModule("ref-mono-retrigger",
                    {{"attack", 0.3, ENV_ATTACK_PARAM, 0.3}, {"decay", 0.4, ENV_DECAY_PARAM, 0.4},
                     {"sustain", 0.3, ENV_SUSTAIN_PARAM, 0.3}, {"flt_env", 0.6, FILTER_ENV_PARAM, 0.6},
                     {"cutoff", 3, FILTER_CUTOFF_PARAM, 3}},
                    {{0, 1, 50}, {S, 1, 53}, {2 * S, 0, 53}, {2 * S + 100, 0, 50}}, 3 * S, false, 1, true);
  CompareWithModule("ref-mono-full-patch",
                    {{"octave", 1, OCTAVE_PARAM, -1}, {"coarse", 1.25, COARSE_PARAM, 1.25},
                     {"fine", 0.03, FINE_PARAM, 0.03}, {"vco_mod", 0.4, PITCH_MOD_PARAM, 0.4},
                     {"vco_mod_src", 1, PITCH_MOD_SOURCE_PARAM, 1}, {"vco_mod_pol", 0, PITCH_MOD_ENV_POL_PARAM, 0},
                     {"width", 0.6, PW_PARAM, 0.5 - 0.5 * 0.6}, {"pwm", 0.3, PW_MOD_PARAM, 0.3},
                     {"pwm_src", 2, PW_MOD_SOURCE_PARAM, 2}, {"pwm_pol", 1, PW_MOD_ENV_POL_PARAM, 1},
                     {"sub_oct", 1, SUB_OCTAVE_PARAM, 1}, {"sub_wave", 2, SUB_WAVE_PARAM, 2},
                     {"saw", 0.5, SAW_LEVEL_PARAM, 0.5}, {"pulse", 0.6, PULSE_LEVEL_PARAM, 0.6},
                     {"sub", 0.7, SUB_LEVEL_PARAM, 0.7}, {"cutoff", 5, FILTER_CUTOFF_PARAM, 5},
                     {"res", 6, FILTER_Q_PARAM, 6}, {"hpf", 2, FILTER_HPF_PARAM, 2}, {"poles", 0, FILTER_POLES_PARAM, 0},
                     {"flt_env", 0.4, FILTER_ENV_PARAM, 0.4}, {"flt_lfo", 0.5, FILTER_MOD_PARAM, 0.5},
                     {"flt_voct", 0.7, FILTER_VOCT_PARAM, 0.7}, {"lfo_rate", 4, LFO_RATE_PARAM, 4},
                     {"lfo_wave", 1, LFO_WAVE_PARAM, 1}, {"lfo_slew", 0.3, LFO_SLEW_PARAM, 0.3},
                     {"attack", 0.2, ENV_ATTACK_PARAM, 0.2}, {"decay", 0.5, ENV_DECAY_PARAM, 0.5},
                     {"sustain", 0.5, ENV_SUSTAIN_PARAM, 0.5}, {"release", 0.4, ENV_RELEASE_PARAM, 0.4},
                     {"env_cycle", 1, ENV_CYCLE_PARAM, 1}},
                    {{0, 1, 57}, {2 * S, 0, 57}}, 3 * S, false, 1);
  CompareWithModule("ref-mono-semitone-coarse-gate-vca",
                    {{"coarse_mode", 1, COARSE_MODE_PARAM, 1}, {"coarse", 1.6, COARSE_PARAM, 1.6},
                     {"vca_src", 1, VCA_SOURCE_PARAM, 1}, {"env_length", 1, ENV_LENGTH_PARAM, 1},
                     {"vco_mod", 0.2, PITCH_MOD_PARAM, 0.2}, {"lfo_rate", 3, LFO_RATE_PARAM, 3}},
                    {{0, 1, 62}, {S, 0, 62}}, 2 * S, false, 1);
  CompareWithModule("ref-poly-chord",
                    {{"cutoff", 7, FILTER_CUTOFF_PARAM, 7}, {"release", 0.45, ENV_RELEASE_PARAM, 0.45},
                     {"saw", 0.4, SAW_LEVEL_PARAM, 0.4}},
                    {{0, 1, 60}, {100, 1, 64}, {200, 1, 67}, {300, 1, 71}, {S, 0, 64}, {S + 50, 1, 74},
                     {2 * S, 0, 60}, {2 * S, 0, 67}, {2 * S, 0, 71}, {2 * S, 0, 74}},
                    3 * S, true, 4);
  CompareWithModule("ref-poly-16",
                    {{"saw", 0.15, SAW_LEVEL_PARAM, 0.15}, {"glide", 0.3, GLIDE_PARAM, 0.3},
                     {"cutoff", 8, FILTER_CUTOFF_PARAM, 8}, {"release", 0.3, ENV_RELEASE_PARAM, 0.3}},
                    {{0, 1, 40}, {10, 1, 43}, {20, 1, 47}, {30, 1, 50}, {40, 1, 53}, {50, 1, 57}, {60, 1, 60},
                     {70, 1, 64}, {80, 1, 67}, {90, 1, 71}, {100, 1, 74}, {110, 1, 77}, {120, 1, 81},
                     {130, 1, 84}, {140, 1, 88}, {150, 1, 91}, {S, 0, 40}, {S, 0, 43}, {S, 0, 47}, {S, 0, 50},
                     {S, 0, 53}, {S, 0, 57}, {S, 0, 60}, {S, 0, 64}, {S, 0, 67}, {S, 0, 71}, {S, 0, 74},
                     {S, 0, 77}, {S, 0, 81}, {S, 0, 84}, {S, 0, 88}, {S, 0, 91}},
                    2 * S, true, 16);
}

// ------------------------------------------------------------------------------------------------ behaviour
void TestPitch() {
  Plug p;
  p.Set("cutoff", 6.0);   // a gentler wave for the autocorrelation
  std::vector<float> out;
  p.On(57);   // A3 = 220 Hz
  p.Run(kRate / 2, &out);
  float hz = Pitch(out, kRate / 4, kRate / 2);
  char what[96];
  snprintf(what, sizeof what, "note 57 sounds at 220 Hz (%.2f Hz)", hz);
  Check(fabsf(hz - 220.0f) < 0.5f, what);
  p.Set("octave", 3);   // 4'
  out.clear();
  p.Run(kRate / 2, &out);
  hz = Pitch(out, kRate / 4, kRate / 2);
  snprintf(what, sizeof what, "Octave 4' doubles it (%.2f Hz)", hz);
  Check(fabsf(hz - 440.0f) < 1.0f, what);
  p.Set("octave", 2);
  p.Set("bend", 12);
  p.Midi(0xE0, 0x7F, 0x7F);   // bend fully up: +12 semitones
  out.clear();
  p.Run(kRate / 2, &out);
  hz = Pitch(out, kRate / 4, kRate / 2);
  snprintf(what, sizeof what, "pitch bend up 12 semitones (%.2f Hz)", hz);
  Check(fabsf(hz - 440.0f) < 1.5f, what);
}

void TestReleaseAndSilence() {
  Plug p;
  std::vector<float> out;
  p.On(60);
  p.Run(kRate / 4, &out);
  Check(Rms(out, 0, out.size()) > 0.05f, "a held note sounds");
  p.Off(60);
  out.clear();
  p.Run(kRate / 4, &out);
  Check(Peak(out, kRate / 8, out.size()) == 0.0f, "Release 0: silence after the note off");
  p.Set("release", 0.6);
  p.On(60);
  p.Run(kRate / 4, NULL);
  p.Off(60);
  out.clear();
  p.Run(kRate / 10, &out);
  Check(Rms(out, kRate / 20, out.size()) > 0.01f, "Release 0.6: the note rings on after the note off");
}

void TestMono() {
  Plug p;
  p.On(60);
  p.Run(256, NULL);
  p.On(64);
  p.Run(256, NULL);
  Check(p.VoiceNote(0) == 64, "mono: the newest note plays");
  p.Off(64);
  p.Run(256, NULL);
  Check(p.VoiceNote(0) == 60, "mono: back to the held note on release");
  p.Off(60);
  p.Run(256, NULL);
  Check(p.VoiceNote(0) == -1, "mono: gate off when no key is down");
  // sustain pedal
  p.Midi(0xB0, 64, 127);
  p.On(62);
  p.Off(62);
  p.Run(256, NULL);
  Check(p.VoiceNote(0) == 62, "mono: the pedal holds a released note");
  p.Midi(0xB0, 64, 0);
  p.Run(256, NULL);
  Check(p.VoiceNote(0) == -1, "mono: pedal up releases it");
}

void TestPoly() {
  Plug p;
  p.Set("voice_mode", 1);
  p.Set("voices", 3);
  p.On(60); p.On(64); p.On(67);
  p.Run(256, NULL);
  int sounding = 0;
  for (int v = 0; v < 16; ++v) sounding += p.VoiceNote(v) >= 0;
  Check(sounding == 3, "poly: three notes on three voices");
  p.On(71);   // steals the oldest (60)
  p.Run(256, NULL);
  bool has60 = false, has71 = false;
  for (int v = 0; v < 16; ++v) { has60 |= p.VoiceNote(v) == 60; has71 |= p.VoiceNote(v) == 71; }
  Check(has71 && !has60, "poly: a fourth note on three voices steals the oldest");
  p.Off(64); p.Off(67); p.Off(71);
  p.Run(256, NULL);
  sounding = 0;
  for (int v = 0; v < 16; ++v) sounding += p.VoiceNote(v) >= 0;
  Check(sounding == 0, "poly: all released");
  // pedal
  p.Midi(0xB0, 64, 127);
  p.On(50); p.Off(50); p.On(52); p.Off(52);
  p.Run(256, NULL);
  sounding = 0;
  for (int v = 0; v < 16; ++v) sounding += p.VoiceNote(v) >= 0;
  Check(sounding == 2, "poly: the pedal holds released notes");
  p.Midi(0xB0, 64, 0);
  p.Run(256, NULL);
  sounding = 0;
  for (int v = 0; v < 16; ++v) sounding += p.VoiceNote(v) >= 0;
  Check(sounding == 0, "poly: pedal up releases them");
  // fewer voices while notes sound
  p.Set("voices", 8);
  for (int n = 0; n < 8; ++n) p.On(60 + n);
  p.Run(256, NULL);
  p.Set("voices", 2);
  p.Run(256, NULL);
  sounding = 0;
  for (int v = 0; v < 16; ++v) sounding += p.VoiceNote(v) >= 0;
  Check(sounding <= 2, "poly: lowering Poly Voices silences the voices above it");
  // switching to mono drops every note (a while after the last switch: a switch flips once per Q-Link turn)
  p.Run(kRate / 2, NULL);
  p.Set("voice_mode", 0);
  p.Run(256, NULL);
  sounding = 0;
  for (int v = 0; v < 16; ++v) sounding += p.VoiceNote(v) >= 0;
  Check(sounding == 0, "switching to Mono releases every voice");
  // all notes off
  p.Run(kRate / 2, NULL);
  p.Set("voice_mode", 1);
  p.On(60); p.On(61);
  p.Midi(0xB0, 123, 0);
  p.Run(256, NULL);
  sounding = 0;
  for (int v = 0; v < 16; ++v) sounding += p.VoiceNote(v) >= 0;
  Check(sounding == 0, "CC 123 (all notes off)");
}

void TestStability() {
  Plug p;
  p.Set("voice_mode", 1);
  p.Set("voices", 16);
  p.Set("level", 1.0);
  p.Set("res", 10.0);
  p.Set("cutoff", 4.0);
  p.Set("flt_env", 1.0);
  p.Set("flt_lfo", 1.0);
  p.Set("lfo_rate", 11.0);
  p.Set("vco_mod", 1.0);
  p.Set("pwm", 0.5);
  p.Set("sub", 1.0);
  p.Set("pulse", 1.0);
  p.Set("noise", 1.0);
  p.Set("sub_oct", 6);
  p.Set("octave", 4);
  p.Set("lim_drive", 18.0);
  std::vector<float> out;
  for (int n = 0; n < 16; ++n) p.On(100 + n % 28);
  p.Run(kRate, &out);
  Check(Finite(out), "16 voices at the extremes: finite output");
  Check(Peak(out, 0, out.size()) <= powf(10.0f, -0.3f / 20.0f) + 1e-4f, "the limiter holds the ceiling");
  for (int n = 0; n < 16; ++n) p.Midi(0x80, 100 + n % 28, 0);
  p.Set("octave", 0);
  for (int n = 0; n < 16; ++n) p.On(n);
  out.clear();
  p.Run(kRate / 2, &out);
  Check(Finite(out), "16 voices at the lowest notes: finite output");
  WriteWav("extremes", out);
}

void TestBlockSizes() {
  // SAMPLE_ACCURATE: the wrapper renders any 1..128 frames; the result must not depend on how a block is cut.
  Plug a, b;
  a.Set("lfo_wave", 0);
  b.Set("lfo_wave", 0);
  std::vector<float> x, y;
  a.On(60); b.On(60);
  a.Run(kRate / 4, &x, 128);
  b.Run(kRate / 4, &y, 37);
  // noise differs between instances (each seeds its own), so compare loudness, not samples
  float rx = Rms(x, 0, x.size()), ry = Rms(y, 0, y.size());
  Check(fabsf(rx - ry) < 0.01f * rx, "blocks of 37 frames sound like blocks of 128");
}

void TestPanicAndState() {
  Plug p;
  p.Set("cutoff", 3.0);
  p.Set("voice_mode", 1);
  p.On(60);
  p.Run(256, NULL);
  std::string state = p.Get("state");
  Check(state.find("cutoff=3;") != std::string::npos, "state holds the parameters");
  p.Set("panic", 1.0);
  p.Run(256, NULL);
  Check(p.Get("cutoff") == "10" && p.Get("voice_mode") == "0", "Panic: every parameter back to its default");
  Check(p.VoiceNote(0) == -1, "Panic: every note off");
  Plug q;
  q.e->set_param(q.inst, "state", state.c_str());
  Check(q.Get("cutoff") == "3" && q.Get("voice_mode") == "1", "state restores on another instance");
}

void TestDisplay() {
  Plug p;
  Check(p.Text("cutoff") == "14.08 kHz", "Filter Freq 10 shows 14.08 kHz");
  Check(p.Text("width") == "50 %", "Width at the bottom shows a 50 % duty cycle");
  p.Set("width", 1.0);
  Check(p.Text("width") == "0 %", "Width at the top shows 0 %");
  Check(p.Text("coarse") == "+0.0 st", "Coarse centre is +0.0 st");
  Check(p.Text("fine") == "+0 ct", "Fine centre is +0 ct");
  Check(p.Text("lfo_rate") == "0.100 Hz", "LFO Rate 0 is 0.1 Hz");
  Check(p.Text("level") == "-6.0 dB", "Level default -6 dB");
  Check(p.Get("octave") == "2", "Octave default 8'");
}

// ------------------------------------------------------------------------------------------------ patching
enum { SRC_SEQ1 = 16, SRC_GATE1 = 18, SRC_LFO1 = 1, SRC_EXT_OSC = 20, SRC_ENV_POS = 28, SRC_VCO_SUB = 32,
       SRC_VELOCITY = 33 };

void TestPatchingAgainstModule() {
  const int S = kRate / 4;
  // Seq 1 at +1 V on every step into VOct 2: the module with 1 V on its V/Oct 2 jack
  std::vector<std::pair<const char*, double> > seq;
  for (int i = 1; i <= 16; ++i) {
    static char keys[16][16];
    snprintf(keys[i - 1], sizeof keys[0], "sq1_s%d", i);
    seq.push_back(std::make_pair(keys[i - 1], 1.0));
  }
  seq.push_back(std::make_pair("voct2_src", (double)SRC_SEQ1));
  CompareWithModule("ref-patch-seq-into-voct2", {}, {{0, 1, 55}, {2 * S, 0, 55}}, 3 * S, false, 1, false, seq,
                    [](long, void* m) {
                      float one[16] = { 1.0f };
                      ref_input(m, VOCT_INPUT_2, 1, one);
                    });
  // Gate 1 (steps 1, 3, 5... on, 1/16 at 120 BPM, width 50 %) into Env Gate, no MIDI: the module's Gate jack
  std::vector<std::pair<const char*, double> > gates;
  for (int i = 1; i <= 16; ++i) {
    static char keys[16][16];
    snprintf(keys[i - 1], sizeof keys[0], "gt1_g%d", i);
    if (i % 2) gates.push_back(std::make_pair(keys[i - 1], 1.0));   // a switch set to its own value flips (a tap)
  }
  gates.push_back(std::make_pair("egate_src", (double)SRC_GATE1));
  CompareWithModule("ref-patch-gate-seq-plays-the-voice",
                    {{"release", 0.3, ENV_RELEASE_PARAM, 0.3}, {"decay", 0.4, ENV_DECAY_PARAM, 0.4},
                     {"sustain", 0.4, ENV_SUSTAIN_PARAM, 0.4}},
                    {}, 4 * S, false, 1, false, gates,
                    [](long t, void* m) {
                      double pos = t * (120.0 / 60.0 / kRate) / 0.25;   // sixteenths since creation
                      long step = static_cast<long>(floor(pos)) % 16;
                      float g[16] = { (step % 2 == 0 && pos - floor(pos) < 0.5) ? 10.0f : 0.0f };
                      ref_input(m, GATE_INPUT, 1, g);
                    });
}

void TestPatching() {
  {   // the Ext Osc through Mixer Ext: a square an octave below the note
    Plug p;
    p.Set("saw", 0.0);
    p.Set("ext", 1.0);
    p.Set("cutoff", 6.0);
    std::vector<float> out;
    p.On(69);   // A4 = 440 Hz; Ext Osc at -1 octave = 220 Hz
    p.Run(kRate / 2, &out);
    float hz = Pitch(out, kRate / 4, kRate / 2);
    char what[96];
    snprintf(what, sizeof what, "Ext Osc into Mixer Ext: a square an octave down (%.1f Hz)", hz);
    Check(Rms(out, kRate / 4, kRate / 2) > 0.03f && fabsf(hz - 220.0f) < 1.0f, what);
    p.Set("xo_oct", 3);   // "0"
    out.clear();
    p.Run(kRate / 2, &out);
    hz = Pitch(out, kRate / 4, kRate / 2);
    snprintf(what, sizeof what, "Ext Osc octave 0 follows the note (%.1f Hz)", hz);
    Check(fabsf(hz - 440.0f) < 2.0f, what);
    for (int w = 0; w < 5; ++w) {
      p.Set("xo_wave", w);
      out.clear();
      p.Run(kRate / 8, &out);
      Check(Finite(out) && Rms(out, 0, out.size()) > 0.01f, "Ext Osc waves sound");
    }
  }
  {   // LFO 1 into Freq 1 with the blue attenuverter up: the brightness moves
    Plug a, b;
    for (Plug* p : {&a, &b}) {
      p->Set("cutoff", 4.0);
      p->Set("res", 3.0);
      p->Set("lfo1_sync", 0);       // free
      p->Set("lfo1_freq", 2.0);     // a few Hz
      p->Set("cut1_src", SRC_LFO1);
      p->On(48);
    }
    b.Set("flt_cv1", 1.0);
    std::vector<float> x, y;
    a.Run(kRate, &x);
    b.Run(kRate, &y);
    const size_t win = 1011;   // three periods of note 48 (130.8 Hz)
    float lo = 1e9f, hi = 0;
    for (size_t i = kRate / 4; i + win < y.size(); i += win) {
      float r = Rms(y, i, i + win);
      lo = fminf(lo, r);
      hi = fmaxf(hi, r);
    }
    float alo = 1e9f, ahi = 0;
    for (size_t i = kRate / 4; i + win < x.size(); i += win) {
      float r = Rms(x, i, i + win);
      alo = fminf(alo, r);
      ahi = fmaxf(ahi, r);
    }
    char what[128];
    snprintf(what, sizeof what, "LFO 1 into Freq 1 moves the filter only with the attenuverter up (%.3f-%.3f vs %.3f-%.3f)",
             lo, hi, alo, ahi);
    Check(hi > lo * 1.5f && ahi < alo * 1.1f, what);
  }
  {   // per-voice sources in Poly: the envelope into PWM, the sub into Freq 2 (FM), velocity into VCA Level
    Plug p;
    p.Set("voice_mode", 1);
    p.Set("voices", 8);
    p.Set("pulse", 0.8);
    p.Set("pwm_src", 0);   // the PWM jack
    p.Set("pwm", 0.4);
    p.Set("pwm_in_src", SRC_ENV_POS);
    p.Set("cut2_src", SRC_VCO_SUB);
    p.Set("flt_cv2", 0.5);
    p.Set("vca_src", 1);
    p.Set("vca_in_src", SRC_VELOCITY);
    p.Set("vca_cv", -0.5);
    std::vector<float> out;
    for (int n = 0; n < 8; ++n) p.Midi(0x90, 50 + 3 * n, 20 + 13 * n);
    p.Run(kRate / 2, &out);
    Check(Finite(out) && Rms(out, 0, out.size()) > 0.01f, "Poly with per-voice sources patched: sound, finite");
  }
  {   // MIDI sources: velocity sets the VCA level per voice
    Plug p;
    p.Set("vca_in_src", SRC_VELOCITY);
    p.Set("vca_cv", -1.0);   // level = envelope - velocity / 10: a soft note is louder
    std::vector<float> soft, hard;
    p.Midi(0x90, 60, 20);
    p.Run(kRate / 4, &soft);
    p.Off(60);
    p.Run(kRate / 4, NULL);
    p.Midi(0x90, 60, 120);
    p.Run(kRate / 4, &hard);
    Check(Rms(soft, kRate / 8, kRate / 4) > 2.0f * Rms(hard, kRate / 8, kRate / 4),
          "Velocity into VCA Level (attenuverter down): soft notes louder than hard ones");
  }
}

void TestPresets() {
  system("rm -rf build/dsptest/presets");
  Plug p;
  p.Set("cutoff", 2.5);
  p.Set("voct2_src", SRC_SEQ1);
  p.Set("preset_slot", 4);
  p.Set("preset_save", 1.0);
  for (int i = 0; i < 50 && p.Text("preset_info").find("SAVED") == std::string::npos; ++i) {
    usleep(20000);
    p.Run(128, NULL);
  }
  Check(p.Text("preset_info") == "PRESET 5: SAVED", "preset save writes the slot");
  p.Set("panic", 1.0);
  p.Run(128, NULL);
  Check(p.Get("cutoff") == "10" && p.Get("voct2_src") == "0" && p.Get("preset_slot") == "4",
        "Panic: defaults, the preset slot kept");
  p.Run(kRate / 2, NULL);   // past the trigger's gesture window
  p.Set("preset_load", 1.0);
  for (int i = 0; i < 50 && p.Get("cutoff") != "2.5"; ++i) {
    usleep(20000);
    p.Run(128, NULL);
  }
  Check(p.Get("cutoff") == "2.5" && p.Get("voct2_src") == std::to_string(SRC_SEQ1), "preset load restores it");
}

void TestLimiterMatchesRmxxxl() {
  // src/limiter.h computes the window minimum faster than RMXXXL's scan; the output must be the same.
  ixxl::Limiter a;
  ixxl::LimiterRef b;
  a.Init();
  b.Init();
  float l1[128], r1[128], l2[128], r2[128];
  unsigned seed = 1;
  bool same = true;
  for (int blk = 0; blk < 200; ++blk) {
    for (int i = 0; i < 128; ++i) {
      seed = seed * 1664525u + 1013904223u;
      float v = ((seed >> 8) / 16777216.0f * 2.0f - 1.0f) * (blk % 7 == 0 ? 3.0f : 0.7f);
      l1[i] = l2[i] = v;
      r1[i] = r2[i] = -0.5f * v;
    }
    a.Process(l1, r1, 128, 6.0f, -0.3f, 80.0f, 44100.0f);
    b.Process(l2, r2, 128, 6.0f, -0.3f, 80.0f, 44100.0f);
    same = same && !memcmp(l1, l2, sizeof l1) && !memcmp(r1, r2, sizeof r1);
  }
  Check(same, "limiter: sample for sample the same as RMXXXL's");
}

}  // namespace

int main(int argc, char** argv) {
  if (argc > 2 && !strcmp(argv[1], "--wav")) wav_dir = argv[2];
  TestAgainstModule();
  TestPitch();
  TestReleaseAndSilence();
  TestMono();
  TestPoly();
  TestStability();
  TestBlockSizes();
  TestPanicAndState();
  TestDisplay();
  TestLimiterMatchesRmxxxl();
  TestPatchingAgainstModule();
  TestPatching();
  TestPresets();
  printf("%s (%d failed)\n", failures ? "FAILED" : "PASSED", failures);
  return failures ? 1 : 0;
}
