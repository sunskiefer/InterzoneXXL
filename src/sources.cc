// SPDX-License-Identifier: GPL-3.0-or-later
// Modulation sources (see sources.h). The DSP is vendored unchanged (third_party/bogaudio, third_party/mutable); the
// glue follows the VCV modules' own process() code, cited per source, with the jacks a Force can't patch replaced by
// the MPC tempo: Bogaudio LFO.cpp (Matt Demanett, GPL-3.0-or-later) and VCV Audible Instruments Tides2.cpp and
// Marbles.cpp (Andrew Belt, GPL-3.0-or-later).
#include "sources.h"
#include "marbles_source.h"

#include <math.h>
#include <string.h>

#include <new>

#include "dsp/oscillator.hpp"
#include "dsp/pitch.hpp"
#include "dsp/signal.hpp"
#include "stmlib/dsp/hysteresis_quantizer.h"
#include "stmlib/dsp/units.h"
#include "stmlib/utils/gate_flags.h"
#include "tides2/poly_slope_generator.h"

namespace ixxl {

namespace {

inline float Clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline int Opt(const float* p, int i) { return static_cast<int>(p[i] + 0.5f); }

// Length in beats of a sync option: 0 = "Free".
inline float SyncBeats(const float* p, int key) {
  int o = Opt(p, key);
  return o <= 0 ? 0.0f : kDivisionBeats[o - 1];
}
inline float StepBeats(const float* p, int key) { return kDivisionBeats[Opt(p, key)]; }
inline double Frac(double x) { return x - floor(x); }

// ------------------------------------------------------------------------------------------------ Bogaudio LFO
// LFO.cpp: modulateChannel() once per block, processChannel() + updateOutput() per frame, for the one output Wave picks.
using bogaudio::dsp::Phasor;

struct Lfo {
  Phasor phasor;
  bogaudio::dsp::SineTableOscillator sine;
  bogaudio::dsp::TriangleOscillator triangle;
  bogaudio::dsp::SawOscillator ramp;
  bogaudio::dsp::SquareOscillator square;
  bogaudio::dsp::SteppedRandomOscillator stepped;
  bogaudio::dsp::ShapedSlewLimiter slew;
  float smooth_sr = 0.0f, smooth_freq = 0.0f, smooth_amount = -1.0f;   // LFOBase::Smoother's cache
  int sample_steps = 1;
  int sample_step = 0;
  float offset = 0.0f;
  float scale = 1.0f;
  float sample = 0.0f;
  bool active = false;
  int wave = -1;

  void Init(float sr) {
    phasor.setSampleRate(sr);
    sample_step = static_cast<int>(phasor._sampleRate);   // Engine::reset()
  }

  // first: P_LFO<n>_WAVE; the LFO's parameters follow in gen_params.py's order
  void Modulate(const float* p, int first, float bpm) {
    const int kWave = 0, kSync = 1, kFreq = 2, kSlow = 3, kSample = 4, kPw = 5, kSmooth = 6, kOffset = 7, kScale = 8;
    int w = Opt(p, first + kWave);
    if (w != wave) { wave = w; active = false; }
    float beats = SyncBeats(p, first + kSync);
    float f;
    if (beats > 0.0f) {
      f = bpm / 60.0f / beats;
    } else {   // LFOBase::setFrequency(): knob + pitch offset (-7, or -11 in Slow), as a 1 V/oct CV from C4
      f = bogaudio::dsp::cvToFrequency(p[first + kFreq] + (Opt(p, first + kSlow) ? -11.0f : -7.0f));
      if (f > 2000.0f) f = 2000.0f;
    }
    phasor.setFrequency(f);
    float pw = p[first + kPw];
    pw *= 1.0f - 2.0f * square.minPulseWidth;
    pw *= 0.5f;
    pw += 0.5f;
    square.setPulseWidth(pw);
    float max_sample_steps = (phasor._sampleRate / phasor._frequency) / 4.0f;
    int steps = static_cast<int>(p[first + kSample] * max_sample_steps);
    int max_steps = static_cast<int>(max_sample_steps);
    sample_steps = steps < 1 ? 1 : (steps > max_steps ? max_steps : steps);
    if (sample_steps < 1) sample_steps = 1;
    float amount = Clampf(p[first + kSmooth], 0.0f, 1.0f);
    if (smooth_sr != phasor._sampleRate || smooth_freq != phasor._frequency || smooth_amount != amount) {
      smooth_sr = phasor._sampleRate;
      smooth_freq = phasor._frequency;
      smooth_amount = amount;
      float millis = 1.0f / smooth_freq / 2.0f * 1000.0f * amount * amount * 10.0f;
      slew.setParams(smooth_sr, millis, 0.5f);
    }
    offset = p[first + kOffset] * 5.0f;   // offset range +/-5 V (the module's default)
    scale = p[first + kScale];
  }

  void Render(const Clock& clock, float beats, int frames, float* out) {
    if (clock.restart) phasor.resetPhase();
    for (int i = 0; i < frames; ++i) {
      if (beats > 0.0f) {   // locked to the tempo: the phase is the song position
        phasor._phase = static_cast<Phasor::phase_t>(clock.beat[i] / beats * static_cast<double>(Phasor::cyclePhase));
      } else {
        phasor.advancePhase();
      }
      bool use_sample = false;
      if (sample_steps > 1) {
        ++sample_step;
        if (sample_step >= sample_steps) sample_step = 0;
        else use_sample = true;
      }
      Phasor* osc = &sine;
      bool invert = false;
      switch (wave) {
        case 1: osc = &triangle; break;
        case 2: osc = &ramp; break;
        case 3: osc = &ramp; invert = true; break;
        case 4: osc = &square; use_sample = false; break;
        case 5: osc = &stepped; use_sample = false; break;
      }
      if (!use_sample || !active) {
        sample = osc->nextFromPhasor(phasor) * 5.0f * scale;
        if (invert) sample = -sample;
        sample += offset;
      }
      active = true;
      out[i] = Clampf(slew.next(sample), -12.0f, 12.0f);
    }
  }
};

// ------------------------------------------------------------------------------------------------ Tides 2
// Tides2.cpp process(): blocks of tides2::kBlockSize. The CLOCK jack is the MPC tempo: in Cycle mode the generator
// follows the tempo's phase (the ramp a clock input gives it), times the ratio Frequency picks; in AD and AR the
// tempo's divisions are the TRIG input. Free: the internal oscillator, and AD/AR wait for a trigger.
const float kRootScaled[3] = { 0.125f, 2.0f, 130.81f };
const tides2::Ratio kRatios[20] = {
  { 0.0625f, 16 }, { 0.125f, 8 }, { 0.1666666f, 6 }, { 0.25f, 4 }, { 0.3333333f, 3 }, { 0.5f, 2 }, { 0.6666666f, 3 },
  { 0.75f, 4 }, { 0.8f, 5 }, { 1, 1 }, { 1, 1 }, { 1.25f, 4 }, { 1.3333333f, 3 }, { 1.5f, 2 }, { 2.0f, 1 },
  { 3.0f, 1 }, { 4.0f, 1 }, { 6.0f, 1 }, { 8.0f, 1 }, { 16.0f, 1 },
};
const size_t kTidesBlock = 8;   // tides2::kBlockSize (io_buffer.h, not vendored)

struct Tides {
  tides2::PolySlopeGenerator generator;
  stmlib::HysteresisQuantizer ratio_quantizer;
  stmlib::GateFlags previous_trig = stmlib::GATE_FLAG_LOW;
  int previous_output_mode = 0;
  float sr = 44100.0f;

  void Init(float sample_rate) {
    sr = sample_rate;
    generator.Init();
    ratio_quantizer.Init();
  }

  void Render(const float* p, const Clock& clock, int frames, float* out[4]) {
    int range = Opt(p, P_TD_RANGE);
    tides2::OutputMode output_mode = static_cast<tides2::OutputMode>(Opt(p, P_TD_OUTPUT));
    tides2::RampMode ramp_mode = static_cast<tides2::RampMode>(Opt(p, P_TD_RAMP));
    tides2::Range range_mode = range < 2 ? tides2::RANGE_CONTROL : tides2::RANGE_AUDIO;
    float transposition = Clampf(p[P_TD_FREQ], -96.0f, 96.0f);
    float slope = Clampf(p[P_TD_SLOPE], 0.0f, 1.0f), shape = Clampf(p[P_TD_SHAPE], 0.0f, 1.0f);
    float smoothness = Clampf(p[P_TD_SMOOTH], 0.0f, 1.0f), shift = Clampf(p[P_TD_SHIFT], 0.0f, 1.0f);
    float beats = SyncBeats(p, P_TD_SYNC);
    bool locked = beats > 0.0f && ramp_mode == tides2::RAMP_MODE_LOOPING;
    bool triggered = beats > 0.0f && !locked;
    tides2::Ratio r = { 1.0f, 1 };
    if (locked) r = ratio_quantizer.Lookup(kRatios, 0.5f + transposition * 0.0105f, 20);
    if (static_cast<int>(output_mode) != previous_output_mode) {
      generator.Reset();
      previous_output_mode = output_mode;
    }
    tides2::PolySlopeGenerator::OutputSample o[kTidesBlock];
    stmlib::GateFlags trig[kTidesBlock];
    float ramp[kTidesBlock];
    for (int start = 0; start < frames; start += kTidesBlock) {
      int n = frames - start < static_cast<int>(kTidesBlock) ? frames - start : static_cast<int>(kTidesBlock);
      for (int i = 0; i < n; ++i) {
        bool high = triggered && Frac(clock.beat[start + i] / beats) < 0.5;
        trig[i] = stmlib::ExtractGateFlags(previous_trig, high);
        previous_trig = trig[i];
        if (locked) ramp[i] = static_cast<float>(Frac(clock.beat[start + i] / beats * r.ratio));
      }
      float frequency = locked ? r.ratio * clock.bpm / 60.0f / beats / sr
                               : kRootScaled[range] / sr * stmlib::SemitonesToRatio(transposition);
      generator.Render(ramp_mode, output_mode, range_mode, frequency, slope, shape, smoothness, shift, trig,
                       locked ? ramp : NULL, o, n);
      for (int i = 0; i < n; ++i) {
        for (int c = 0; c < 4; ++c) out[c][start + i] = o[i].channel[c];
      }
    }
  }
};

// ------------------------------------------------------------------------------------------------ sequencers
// 16-step CV sequencer: the step under the song position, +/-5 V, with a slew whose time is a share of a step.
struct StepSeq {
  float value = 0.0f;
  void Render(const float* p, int first, const Clock& clock, float sr, int frames, float* out) {
    const int kLen = 16, kDiv = 17, kSlew = 18;
    int len = static_cast<int>(p[first + kLen] + 0.5f);
    len = len < 1 ? 1 : (len > 16 ? 16 : len);
    float beats = StepBeats(p, first + kDiv);
    float slew = Clampf(p[first + kSlew], 0.0f, 1.0f);
    float step_seconds = beats * 60.0f / clock.bpm;
    float tau = slew * slew * step_seconds;
    float a = tau > 1e-5f ? 1.0f - expf(-1.0f / (tau * sr)) : 1.0f;
    for (int i = 0; i < frames; ++i) {
      long step = static_cast<long>(floor(clock.beat[i] / beats)) % len;
      if (step < 0) step += len;
      value += a * (p[first + step] - value);
      out[i] = value;
    }
  }
};

// 16-step gate sequencer: 10 V for Width of each step that is on.
struct GateSeq {
  void Render(const float* p, int first, const Clock& clock, int frames, float* out) {
    const int kLen = 16, kDiv = 17, kWidth = 18;
    int len = static_cast<int>(p[first + kLen] + 0.5f);
    len = len < 1 ? 1 : (len > 16 ? 16 : len);
    float beats = StepBeats(p, first + kDiv);
    double width = Clampf(p[first + kWidth], 0.0f, 1.0f);
    for (int i = 0; i < frames; ++i) {
      double pos = clock.beat[i] / beats;
      long step = static_cast<long>(floor(pos)) % len;
      if (step < 0) step += len;
      bool on = Opt(p, first + step) != 0 && Frac(pos) < width;
      out[i] = on ? 10.0f : 0.0f;
    }
  }
};

}  // namespace

struct Sources::Impl {
  float sr = 44100.0f;
  Lfo lfo[4];
  Tides tides;
  MarblesSource marbles;
  StepSeq seq[2];
  GateSeq gate[2];
};

Sources::Sources() : impl_(new Impl()) {}
Sources::~Sources() { delete impl_; }

void Sources::Init(float sample_rate) {
  impl_->sr = sample_rate;
  for (int i = 0; i < 4; ++i) impl_->lfo[i].Init(sample_rate);
  impl_->tides.Init(sample_rate);
  impl_->marbles.Init(sample_rate);
}

void Sources::Render(const float* p, const Clock& clock, const bool* needed, int frames, float out[][kMaxBlock]) {
  Impl& s = *impl_;
  const int lfo_first[4] = { P_LFO1_WAVE, P_LFO2_WAVE, P_LFO3_WAVE, P_LFO4_WAVE };
  for (int i = 0; i < 4; ++i) {
    if (!needed[1 + i]) continue;
    s.lfo[i].Modulate(p, lfo_first[i], clock.bpm);
    s.lfo[i].Render(clock, SyncBeats(p, lfo_first[i] + 1), frames, out[1 + i]);
  }
  if (needed[5] || needed[6] || needed[7] || needed[8]) {
    float* o[4] = { out[5], out[6], out[7], out[8] };
    s.tides.Render(p, clock, frames, o);
  }
  bool any_marbles = false;
  for (int i = 9; i <= 15; ++i) any_marbles = any_marbles || needed[i];
  if (any_marbles) {
    float* o[7] = { out[9], out[10], out[11], out[12], out[13], out[14], out[15] };
    s.marbles.Render(p, clock, frames, o);
  }
  if (needed[16]) s.seq[0].Render(p, P_SQ1_S1, clock, s.sr, frames, out[16]);
  if (needed[17]) s.seq[1].Render(p, P_SQ2_S1, clock, s.sr, frames, out[17]);
  if (needed[18]) s.gate[0].Render(p, P_GT1_G1, clock, frames, out[18]);
  if (needed[19]) s.gate[1].Render(p, P_GT2_G1, clock, frames, out[19]);
}

}  // namespace ixxl
