// SPDX-License-Identifier: GPL-3.0-or-later
// InterzoneXXL: Valley Audio's Interzone (Dale Johnson), a monosynth voice for VCV Rack, as an MPC OS VST2
// instrument, played from MIDI in mono or poly. The DSP is Valley's, vendored (third_party/valley, see VENDORED.md):
// VecDirectOsc, VecOTAFilter, VecOnePoleFilters, VecLoopingADSR, DLFO and the noise sources. This file is the
// module's control layer, Interzone.cpp's process() (getParams(), getCV(), tickSynth()) re-written without VCV Rack,
// in the same order and with the same math, driving it from MPC parameters through the mpc-vst-plugins engine
// interface (wrapper/engine.h).
//
// MIDI stands in for VCV's MIDI-CV module patched into Interzone: V/Oct 1 is the note's pitch (0 V = C4 = note 60,
// plus pitch bend), Gate is 10 V while the key is down, Trig is a 1 ms 10 V pulse where a voice is re-struck without
// its gate falling. Interzone is polyphonic on VCV Rack (its DSP runs in groups of four voices), so Poly is the
// module's own polyphony with up to 16 voices; Mono is one voice (one channel), last-note priority, legato or
// retriggered. Glide is the module's pitch slew, per voice.
//
// The module's other inputs (VOct 2, PWM, Mixer Ext, Filter Freq 1-2, Res, VCA Level, LFO Rate / Trig / Reset)
// read 0 V for now: they get built-in sources in a later version, as PlateauXXL's CV inputs do.
//
// Levels: VCV Rack's audio interface maps +-10 V to full scale. The voices' VCA outputs are summed (as a polyphonic
// cable into VCV's Audio module sums them), scaled by Level, then go through RMXXXL's brickwall limiter.
//
// Q-Links and taps (RMXXXL 1.2.1, PlateauXXL): a trigger fires on every tap and on a Q-Link turn to the right, once
// per turn; an on/off switch flips once per turn either way; an option list moves at most one step per 0.2 s.
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <new>

#include "valley_sse_include.h"
#include "rack.hpp"
#include "dsp/generators/VecDirectOsc.hpp"
#include "dsp/filters/VecOTAFilter.hpp"
#include "dsp/filters/VecOnePoleFilters.hpp"
#include "dsp/modulation/VecLoopingADSR.hpp"
#include "dsp/filters/OnePoleFilters.hpp"
#include "dsp/modulation/DLFO.hpp"
#include "dsp/generators/Noise.hpp"
#include "utilities/Utilities.hpp"

#include "denormals.h"
#include "limiter.h"
#include "param_ids.h"

extern "C" {
#include "engine.h"
#include "params.h"
}

namespace {

using rack::simd::float_4;

const float kSampleRate = 44100.0f;
const int kMaxBlock = 128;
const int kGroups = 4;                   // Interzone::kMaxNumVoiceGroups
const int kLanes = 4;                    // Interzone::kNumVoicesPerGroup
const int kMaxVoices = kGroups * kLanes;
const int kTrigFrames = 44;              // 1 ms, as VCV's MIDI-CV retrigger pulse
const float kGateVolts = 10.0f;
const float kVolts = 10.0f;              // full scale = 10 V (VCV Rack's audio interface)
const int kHeldMax = 32;                 // notes remembered for mono last-note priority
const uint64_t kStepLockFrames = 8820;   // 0.2 s: the shortest gap between two one-step moves of an option list
const uint64_t kGestureFrames = 15435;   // 0.35 s: sets of a switch or trigger closer than this are one turn

enum PWMSources { EXTERNAL_PWM, LFO_PWM, ENVELOPE_PWM };   // Interzone.hpp

inline float Clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

static_assert(P_COUNT <= NPARAMS, "params.h is out of date: run tools/gen_params.py, then build");

struct Voice {
  int note;          // -1: never played
  bool gate;         // key down (or held by the sustain pedal)
  bool sustained;    // key up while the pedal is down: the gate falls when the pedal comes up
  int trig;          // frames left of the retrigger pulse
  uint32_t stamp;    // when the voice was last struck (gate high) or released (gate low)
};

// Interzone's DSP and the variables of its process(), as members of Interzone in Interzone.hpp.
struct Synth {
  __m128 __zero, __one, __two, __five, __ten;
  __m128 __negTwo, __negTen;
  __m128 __half, __quarter;

  int cvCounter;
  float pitchParam;
  float glideParam;
  __m128 vPitchParam, vPitchModParam, vPitchModSource, vPitchModEnvPol;
  __m128 vPitch;
  float_4 rackSimd_vPitch;
  __m128 vFreq;
  float_4 rackSimd_vFreq;
  int subOctave;
  __m128 vPwmDepth, vPulseWidth, vSubWidth;
  __m128 vSawLevel, vPulseLevel, vSubLevel, vNoiseLevel, vExtInLevel;
  __m128 vFilterKeyTrack;
  float_4 vAttack, vDecay, vSustain, vRelease;
  __m128 vPwmEnvPol, vFilterEnvPol;

  __m128 vPwmSource;
  __m128 vExternalPwm;
  __m128 vLfoPwm;
  __m128 vEnvPwm;
  __m128 vPwm;
  __m128 vOscPitchMod;

  __m128 vFilterCutoffParam, vFilterCutoff;
  __m128 vFilterQParam, vFilterQ;

  int filterMode;
  __m128 vFilterCV1In, vFilterCV2In;
  __m128 vFilterCV1Depth, vFilterCV2Depth, vFilterLFODepth, vFilterEnvParam;
  __m128 vHpfCutoff;
  __m128 vFilterOutput;
  __m128 vVCACVInput, vVCACVParam;

  float lfoValue;
  __m128 vLfoValue;
  float_4 vGate;
  float_4 vManualGate;
  float_4 vTrigger;

  VecOnePoleLPFilter vGlide[kGroups];
  VecDirectOsc vOsc[kGroups];
  float noise;
  __m128 vNoise;
  __m128 vSubWave;

  __m128 vMix;
  __m128 vFilterInput;
  __m128 vExtInput;

  VecOTAFilter vFilter[kGroups];
  VecOnePoleHPFilter vHighpass[kGroups];
  __m128 vOutputLevel[kGroups];
  __m128 vOutput;

  DLFO lfo;
  PinkNoise pink;
  OnePoleLPFilter lfoSlew;
  VecOnePoleLPFilter vGateSlew[kGroups];
  VecLoopingADSR vEnv[kGroups];

  // Interzone::Interzone(), minus configParam and the global calcGTable() (a table only the scalar OTAFilter reads).
  // Every DSP object is constructed for 44100 Hz already; MPC runs at 44100 Hz only.
  Synth() {
    lfo.setSampleRate(kSampleRate);
    lfoSlew.setSampleRate(kSampleRate);
    lfoSlew.setCutoffFreq(14000.f);
    pink.setSampleRate(kSampleRate);
    for (int i = 0; i < kGroups; ++i) {
      vOsc[i].setSampleRate(kSampleRate);
      vHighpass[i].setSampleRate(kSampleRate);
      vEnv[i].setSampleRate(kSampleRate);
      vGateSlew[i].setSampleRate(kSampleRate);
      vGateSlew[i].setCutoffFreq(90.f);
      vGlide[i].setSampleRate(kSampleRate);
    }
    __zero = _mm_set1_ps(0.f);
    __one = _mm_set1_ps(1.f);
    __two = _mm_set1_ps(2.f);
    __negTwo = _mm_set1_ps(-2.f);
    __five = _mm_set1_ps(5.f);
    __ten = _mm_set1_ps(10.f);
    __negTen = _mm_set1_ps(-10.f);
    __half = _mm_set1_ps(0.5f);
    __quarter = _mm_set1_ps(0.25f);
    vFilterCutoff = __zero;
    vFilterCutoffParam = __zero;
    cvCounter = 0;
    for (int i = 0; i < kGroups; ++i) vOutputLevel[i] = __zero;
  }
};

// The module's input jacks, in volts, for one frame. Per-voice inputs are polyphonic cables (one value per voice).
struct Inputs {
  float voct1[kMaxVoices], voct2[kMaxVoices];
  float gate[kMaxVoices], trig[kMaxVoices];
  float pwm, ext, cutoff1, cutoff2, res, vca;     // mono cables: the same value on every voice
  float lfo_rate, lfo_trig, lfo_sync;
};

struct Instance {
  Synth synth;
  ixxl::Limiter limiter;
  float s_drive, s_ceiling;
  float param[P_COUNT];
  // voices
  Voice voice[kMaxVoices];
  int active_voices;           // channels: 1 in Mono, Poly Voices in Poly
  int mode;                    // the Voice Mode the voices were laid out for
  uint32_t clock;              // voice stamps
  bool key_down[128];
  bool pedal;
  int held[kHeldMax];          // mono: notes sounding, oldest first
  int held_count;
  float bend;                  // pitch bend, -1..1
  Inputs in;
  float out_l[kMaxBlock], out_r[kMaxBlock];
  // Q-Links: when each parameter was last set (switches, triggers) or last moved one step (lists)
  uint64_t frames;
  uint64_t last_touch[P_COUNT];
  uint64_t last_step[P_COUNT];
  bool loading;
  volatile int display_rev;
};

// ------------------------------------------------------------------------------------------------ parameters
float MinOf(int p) { return PARAMS[p].nopts > 0 ? 0.0f : PARAMS[p].min; }
float MaxOf(int p) { return PARAMS[p].nopts > 0 ? static_cast<float>(PARAMS[p].nopts - 1) : PARAMS[p].max; }
float DefaultValue(int p) {
  if (PARAMS[p].nopts > 0) return floorf(PARAMS[p].def * (PARAMS[p].nopts - 1) + 0.5f);
  return PARAMS[p].min + PARAMS[p].def * (PARAMS[p].max - PARAMS[p].min);
}
int Option(const Instance* s, int p) { return static_cast<int>(s->param[p] + 0.5f); }

// Interzone::getParams(): the module reads its knobs every 16 samples (cvDivider); `p` holds MPC's values in the
// module's own units. PW_PARAM is 0.5 - 0.5 x the Width slider's position (gen_params.py).
void GetParams(Instance* s) {
  Synth& z = s->synth;
  const float* p = s->param;
  z.lfo.setFrequency(0.1f * powf(2.f, p[P_LFO_RATE] + p[P_LFO_FINE] + s->in.lfo_rate));
  z.lfoSlew.setCutoffFreq(1760.f * pow(2.f, (p[P_LFO_SLEW] * 2.f) * -6.f));

  z.glideParam = 330.f * pow(2.f, (p[P_GLIDE] * 2.f) * -7.f);

  z.pitchParam = Option(s, P_COARSE_MODE) > 0 ? semitone(p[P_COARSE] + 0.04f) : p[P_COARSE];
  z.pitchParam -= 1.f;
  z.pitchParam += (Option(s, P_OCTAVE) - 2) + p[P_FINE];
  z.vPitchParam = _mm_set1_ps(z.pitchParam);

  z.vPulseWidth = _mm_set1_ps(0.5f - 0.5f * p[P_WIDTH]);
  z.vPwmDepth = _mm_set1_ps(p[P_PWM]);
  z.vPwmEnvPol = _mm_add_ps(_mm_mul_ps(_mm_set1_ps(static_cast<float>(Option(s, P_PWM_POL))), z.__negTwo), z.__one);
  z.vPwmSource = _mm_set1_ps(static_cast<float>(Option(s, P_PWM_SRC)));
  z.subOctave = Option(s, P_SUB_OCT);

  z.vSawLevel = _mm_set1_ps(p[P_SAW]);
  z.vPulseLevel = _mm_set1_ps(p[P_PULSE]);
  z.vSubLevel = _mm_set1_ps(p[P_SUB]);
  z.vNoiseLevel = _mm_set1_ps(p[P_NOISE]);
  z.vExtInLevel = _mm_set1_ps(p[P_EXT]);

  z.vFilterCV1Depth = _mm_set1_ps(p[P_FLT_CV1]);
  z.vFilterCV2Depth = _mm_set1_ps(p[P_FLT_CV2]);
  z.vFilterLFODepth = _mm_set1_ps(p[P_FLT_LFO]);
  z.vFilterLFODepth = _mm_mul_ps(_mm_mul_ps(z.vFilterLFODepth, z.vFilterLFODepth), z.__five);
  z.vFilterEnvParam = _mm_set1_ps(p[P_FLT_ENV] * 10.f);
  z.vFilterEnvPol = _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(static_cast<float>(Option(s, P_FLT_ENV_POL))), z.__two), z.__one);
  z.vFilterCutoffParam = _mm_set1_ps(p[P_CUTOFF]);
  z.vFilterQParam = _mm_set1_ps(p[P_RES]);
  z.filterMode = Option(s, P_POLES);
  z.vFilterKeyTrack = _mm_set1_ps(p[P_FLT_VOCT]);

  z.vHpfCutoff = _mm_set1_ps(440.f * powf(2.0, p[P_HPF] - 5.f));
  z.vVCACVParam = _mm_set1_ps(p[P_VCA_CV] * 0.1f);

  z.vAttack = p[P_ATTACK];
  z.vDecay = p[P_DECAY];
  z.vSustain = p[P_SUSTAIN];
  z.vRelease = p[P_RELEASE];
  for (int i = 0; i < kGroups; ++i) {
    z.vGlide[i].setCutoffFreq(z.glideParam);
    z.vEnv[i].setADSR(z.vAttack, z.vDecay, z.vSustain, z.vRelease);
    z.vEnv[i].looping = Option(s, P_ENV_CYCLE) > 0;
    z.vEnv[i].setTimeScale(Option(s, P_ENV_LENGTH) > 0 ? 10.f : 1.f);
    z.vHighpass[i].setCutoffFreq(z.vHpfCutoff);
  }
}

// Interzone::getCV() and tickSynth() for one frame; returns the sum of the active voices' VCA outputs, in volts.
float TickFrame(Instance* s, int groups) {
  Synth& z = s->synth;
  const Inputs& in = s->in;
  const float* p = s->param;

  // ---- getCV()
  z.lfo.sync(in.lfo_sync);
  z.lfo.trigger(in.lfo_trig);
  z.lfo.process();

  z.lfoSlew.input = z.lfo.out[Option(s, P_LFO_WAVE)];
  z.lfoSlew.process();
  z.lfoValue = p[P_LFO_SLEW] > 0.0001f ? static_cast<float>(z.lfoSlew.output) : static_cast<float>(z.lfoSlew.input);
  z.vLfoValue = _mm_set1_ps(z.lfoValue);

  z.pink.process();
  z.noise = Option(s, P_NOISE_TYPE) > 0 ? static_cast<float>(z.pink.getValue()) : z.lfo.out[DLFO::NOISE_WAVE];
  z.vNoise = _mm_set1_ps(z.noise);

  z.vManualGate.v = _mm_set_ps(0.f, 0.f, 0.f, static_cast<float>(Option(s, P_ENV_MANUAL)));

  z.vPitchModEnvPol = _mm_set1_ps(Option(s, P_VCO_MOD_POL) * 2.f - 1.f);
  z.vPitchModSource = Option(s, P_VCO_MOD_SRC) > 0 ? z.__zero : _mm_high_ps();
  z.vPitchModParam = _mm_set1_ps(p[P_VCO_MOD]);
  z.vPitchModParam = _mm_mul_ps(z.vPitchModParam, z.vPitchModParam);
  z.vSubWidth = _mm_set1_ps(Option(s, P_SUB_WAVE) < 1 ? 0.75f : 0.5f);

  const bool vcaGate = Option(s, P_VCA_SRC) > 0;
  const __m128 vPwmIn = _mm_set1_ps(in.pwm);
  const __m128 vCut1 = _mm_set1_ps(in.cutoff1), vCut2 = _mm_set1_ps(in.cutoff2), vRes = _mm_set1_ps(in.res);
  const __m128 vVca = _mm_set1_ps(in.vca);
  for (int i = 0; i < groups; ++i) {
    const int c = i * kLanes;
    z.vPitch = _mm_loadu_ps(in.voct1 + c);
    z.vPitch = _mm_add_ps(z.vPitch, _mm_loadu_ps(in.voct2 + c));

    z.vGate = float_4::load(in.gate + c);
    z.vGate += i == 0 ? z.vManualGate : float_4::zero();
    z.vTrigger = float_4::load(in.trig + c);
    z.vGateSlew[i].process(z.vGate.v);

    z.vEnv[i].process(z.vGate, z.vTrigger);
    z.vVCACVInput = vVca;
    z.vOutputLevel[i] = vcaGate ? z.vGateSlew[i]._z : z.vEnv[i].env.v;
    z.vOutputLevel[i] = _mm_add_ps(z.vOutputLevel[i], _mm_mul_ps(z.vVCACVInput, z.vVCACVParam));
    z.vOutputLevel[i] = _mm_clamp_ps(z.vOutputLevel[i], z.__zero, z.__one);

    z.vPitch = z.vGlide[i].process(z.vPitch);
    z.vFilterCV1In = _mm_mul_ps(vCut1, z.vFilterCV1Depth);
    z.vFilterCV2In = _mm_mul_ps(vCut2, z.vFilterCV2Depth);
    z.vFilterCutoff = _mm_add_ps(z.vFilterCutoffParam, _mm_add_ps(z.vFilterCV1In, z.vFilterCV2In));
    z.vFilterCutoff = _mm_add_ps(z.vFilterCutoff, _mm_mul_ps(z.vPitch, z.vFilterKeyTrack));
    z.vFilterCutoff = _mm_add_ps(z.vFilterCutoff, _mm_mul_ps(z.vLfoValue, z.vFilterLFODepth));
    z.vFilterCutoff = _mm_add_ps(z.vFilterCutoff,
                                 _mm_mul_ps(_mm_mul_ps(z.vEnv[i].env.v, z.vFilterEnvPol), z.vFilterEnvParam));
    z.vFilterQ = _mm_add_ps(z.vFilterQParam, vRes);

    z.vOscPitchMod = _mm_switch_ps(_mm_mul_ps(z.vEnv[i].env.v, z.vPitchModEnvPol), z.vLfoValue, z.vPitchModSource);
    z.vOscPitchMod = _mm_mul_ps(z.vOscPitchMod, z.vPitchModParam);
    z.vPitch = _mm_add_ps(z.vPitch, z.vPitchParam);
    z.rackSimd_vPitch = _mm_add_ps(z.vPitch, z.vOscPitchMod);
    z.rackSimd_vFreq = rack::dsp::FREQ_C4 * rack::dsp::approxExp2_taylor5(z.rackSimd_vPitch + 30.f) / 1073741824.f;
    z.vFreq = z.rackSimd_vFreq.v;
    z.vOsc[i].setFrequency(z.vFreq);
    z.vOsc[i].setSubOctave(z.subOctave);

    z.vExternalPwm = _mm_mul_ps(vPwmIn, _mm_set1_ps(-0.1f));
    z.vLfoPwm = _mm_set1_ps(z.lfoValue * -0.5f - 0.5f);
    z.vEnvPwm = _mm_mul_ps(z.vEnv[i].env.v, z.vPwmEnvPol);

    z.vPwm = _mm_switch_ps(z.vExternalPwm, z.vLfoPwm, _mm_cmpeq_ps(z.vPwmSource, _mm_set1_ps(LFO_PWM)));
    z.vPwm = _mm_switch_ps(z.vPwm, z.vEnvPwm, _mm_cmpeq_ps(z.vPwmSource, _mm_set1_ps(ENVELOPE_PWM)));
    z.vPwm = _mm_mul_ps(z.vPwm, z.vPwmDepth);
    z.vPwm = _mm_add_ps(z.vPwm, z.vPulseWidth);
    z.vOsc[i].__pwm = _mm_clamp_ps(z.vPwm, z.__zero, z.__half);
    z.vOsc[i].setSubWidth(z.vSubWidth);

    z.vFilter[i].setCutoff(z.vFilterCutoff);
    z.vFilter[i].setQ(z.vFilterQ);
    z.vFilter[i].setMode(z.filterMode);
  }

  // ---- tickSynth()
  const bool subSaw = Option(s, P_SUB_WAVE) > 1;
  const __m128 vExt = _mm_set1_ps(in.ext);
  float sum = 0.0f;
  alignas(16) float lane[4];
  for (int i = 0; i < groups; ++i) {
    z.vOsc[i].process();
    z.vSubWave = subSaw ? z.vOsc[i].__subSaw : z.vOsc[i].__subPulse;
    z.vExtInput = vExt;
    z.vMix = _mm_mul_ps(z.vOsc[i].__saw, z.vSawLevel);
    z.vMix = _mm_add_ps(z.vMix, _mm_mul_ps(z.vOsc[i].__pulse, z.vPulseLevel));
    z.vMix = _mm_add_ps(z.vMix, _mm_mul_ps(z.vSubWave, z.vSubLevel));
    z.vMix = _mm_add_ps(z.vMix, _mm_mul_ps(z.vNoise, z.vNoiseLevel));
    z.vMix = _mm_add_ps(z.vMix, _mm_mul_ps(z.vExtInput, z.vExtInLevel));
    z.vFilterInput = _mm_mul_ps(z.vMix, z.__two);

    z.vFilter[i].process(_mm_add_ps(z.vFilterInput, _mm_mul_ps(z.vNoise, _mm_set1_ps(8e-5f))));
    z.vFilterOutput = z.vHighpass[i].process(_mm_mul_ps(z.vFilter[i].out, z.__five));
    z.vOutput = _mm_mul_ps(z.vFilterOutput, z.vOutputLevel[i]);
    z.vOutput = _mm_clamp_ps(z.vOutput, z.__negTen, z.__ten);

    _mm_store_ps(lane, z.vOutput);
    for (int l = 0; l < kLanes; ++l)
      if (i * kLanes + l < s->active_voices) sum += lane[l];
  }
  return sum;
}

// ------------------------------------------------------------------------------------------------ voices
bool IsPoly(const Instance* s) { return Option(s, P_VOICE_MODE) > 0; }
int PolyVoices(const Instance* s) { return static_cast<int>(Clamp(floorf(s->param[P_VOICES] + 0.5f), 1, kMaxVoices)); }

void AllNotesOff(Instance* s) {
  for (int v = 0; v < kMaxVoices; ++v) {
    s->voice[v].gate = false;
    s->voice[v].sustained = false;
    s->voice[v].trig = 0;
  }
  memset(s->key_down, 0, sizeof s->key_down);
  s->held_count = 0;
}

// Voice Mode or Poly Voices changed: no voice keeps sounding outside the new layout.
void LayOutVoices(Instance* s) {
  int mode = Option(s, P_VOICE_MODE);
  int n = mode ? PolyVoices(s) : 1;
  if (mode != s->mode) {
    AllNotesOff(s);
    s->mode = mode;
  }
  for (int v = n; v < kMaxVoices; ++v) {
    s->voice[v].gate = false;
    s->voice[v].sustained = false;
    s->voice[v].trig = 0;
  }
  s->active_voices = n;
}

void Strike(Instance* s, Voice& v, int note) {
  if (v.gate) v.trig = kTrigFrames;   // re-struck without the gate falling: retrigger the envelope
  v.note = note;
  v.gate = true;
  v.sustained = false;
  v.stamp = ++s->clock;
}

void Release(Instance* s, Voice& v) {
  v.gate = false;
  v.sustained = false;
  v.stamp = ++s->clock;
}

// Mono: the newest sounding note plays (last-note priority).
void MonoUpdate(Instance* s, bool struck) {
  Voice& v = s->voice[0];
  if (s->held_count == 0) {
    if (v.gate) Release(s, v);
    return;
  }
  int note = s->held[s->held_count - 1];
  bool retrig = Option(s, P_MONO_TRIG) > 0;
  if (!v.gate) {
    Strike(s, v, note);
  } else if (note != v.note || struck) {
    v.note = note;
    if (retrig) v.trig = kTrigFrames;
  }
}

void MonoRemove(Instance* s, int note) {
  int w = 0;
  for (int r = 0; r < s->held_count; ++r)
    if (s->held[r] != note) s->held[w++] = s->held[r];
  s->held_count = w;
}

void NoteOn(Instance* s, int note) {
  s->key_down[note] = true;
  if (!IsPoly(s)) {
    MonoRemove(s, note);
    if (s->held_count == kHeldMax) {   // forget the oldest
      memmove(s->held, s->held + 1, sizeof(int) * (kHeldMax - 1));
      --s->held_count;
    }
    s->held[s->held_count++] = note;
    MonoUpdate(s, true);
    return;
  }
  const int n = s->active_voices;
  int pick = -1;
  for (int v = 0; v < n && pick < 0; ++v)   // the voice already playing this note
    if (s->voice[v].gate && s->voice[v].note == note) pick = v;
  if (pick < 0) {                           // a free voice: the one that last played this note, else the longest free
    for (int v = 0; v < n; ++v) {
      if (s->voice[v].gate) continue;
      if (pick < 0) { pick = v; continue; }
      bool same = s->voice[v].note == note, pick_same = s->voice[pick].note == note;
      if ((same && !pick_same) || (same == pick_same && s->voice[v].stamp < s->voice[pick].stamp)) pick = v;
    }
  }
  if (pick < 0) {                           // all busy: steal the one struck longest ago
    pick = 0;
    for (int v = 1; v < n; ++v)
      if (s->voice[v].stamp < s->voice[pick].stamp) pick = v;
  }
  Strike(s, s->voice[pick], note);
}

void NoteOff(Instance* s, int note) {
  s->key_down[note] = false;
  if (!IsPoly(s)) {
    if (s->pedal) return;   // the note keeps sounding until the pedal comes up
    MonoRemove(s, note);
    MonoUpdate(s, false);
    return;
  }
  for (int v = 0; v < s->active_voices; ++v) {
    Voice& vo = s->voice[v];
    if (vo.gate && vo.note == note && !vo.sustained) {
      if (s->pedal) vo.sustained = true;
      else Release(s, vo);
    }
  }
}

void PedalUp(Instance* s) {
  s->pedal = false;
  if (!IsPoly(s)) {
    int w = 0;
    for (int r = 0; r < s->held_count; ++r)
      if (s->key_down[s->held[r]]) s->held[w++] = s->held[r];
    s->held_count = w;
    MonoUpdate(s, false);
    return;
  }
  for (int v = 0; v < s->active_voices; ++v)
    if (s->voice[v].gate && s->voice[v].sustained) Release(s, s->voice[v]);
}

void Midi(void* inst, const uint8_t* msg, int len) {
  Instance* s = static_cast<Instance*>(inst);
  if (len < 2) return;
  int status = msg[0] & 0xF0;
  int d1 = msg[1] & 0x7F, d2 = len > 2 ? msg[2] & 0x7F : 0;
  LayOutVoices(s);
  switch (status) {
    case 0x90:
      if (d2 > 0) { NoteOn(s, d1); break; }
      // fall through: velocity 0 is a note off
    case 0x80:
      NoteOff(s, d1);
      break;
    case 0xE0:
      s->bend = (((d2 << 7) | d1) - 8192) / 8192.0f;
      break;
    case 0xB0:
      if (d1 == 64) {
        if (d2 >= 64) s->pedal = true;
        else PedalUp(s);
      } else if (d1 == 120 || d1 == 123) {
        AllNotesOff(s);
        s->pedal = false;
      }
      break;
  }
}

// ------------------------------------------------------------------------------------------------ engine interface
void* Create(const char*) {
  // Valley's DSP holds __m128 members and, like the module's, leaves some to be set before use: zeroed and 16-byte
  // aligned memory, as VCV Rack gives a module.
  void* mem = NULL;
  if (posix_memalign(&mem, 64, sizeof(Instance)) != 0 || !mem) return NULL;
  memset(mem, 0, sizeof(Instance));
  Instance* s = new (mem) Instance();
  for (int p = 0; p < P_COUNT; ++p) s->param[p] = DefaultValue(p);
  for (int v = 0; v < kMaxVoices; ++v) s->voice[v].note = -1;
#ifdef IXXL_BENCH_POLY
  // tools/device_bench.sh: mpc-vst-plugins' CPU bench plays 1-16 note chords; this build starts in Poly, 16 voices.
  s->param[P_VOICE_MODE] = 1.0f;
  s->param[P_VOICES] = 16.0f;
#endif
  s->mode = Option(s, P_VOICE_MODE);
  LayOutVoices(s);
  s->limiter.Init();
  s->s_drive = DefaultValue(P_LIM_DRIVE);
  s->s_ceiling = DefaultValue(P_LIM_CEILING);
  // As on VCV Rack, the knobs are first read on the 16th sample (cvDivider); until then the DSP keeps its constructed
  // state, which is silent.
  return s;
}

void Destroy(void* inst) {
  Instance* s = static_cast<Instance*>(inst);
  if (!s) return;
  s->~Instance();
  free(s);
}

int FindParam(const char* key, size_t len) {
  for (int p = 0; p < P_COUNT; ++p)
    if (strlen(PARAMS[p].key) == len && !strncmp(key, PARAMS[p].key, len)) return p;
  return -1;
}

void SetParam(void* inst, const char* key, const char* val);

int SaveState(const Instance* s, char* buf, int buf_len) {
  int len = 0;
  for (int p = 0; p < P_COUNT && len < buf_len; ++p) {
    if (PARAMS[p].momentary) continue;
    len += snprintf(buf + len, buf_len - len, "%s=%g;", PARAMS[p].key, s->param[p]);
  }
  return len < buf_len ? len : buf_len - 1;
}

void LoadState(Instance* s, const char* state) {
  char item[64];
  s->loading = true;
  while (*state) {
    size_t n = strcspn(state, ";");
    if (n < sizeof item) {
      memcpy(item, state, n);
      item[n] = 0;
      char* eq = strchr(item, '=');
      if (eq) {
        *eq = 0;
        if (strcmp(item, "state")) SetParam(s, item, eq + 1);
      }
    }
    state += n;
    if (*state == ';') ++state;
  }
  s->loading = false;
  LayOutVoices(s);
}

// Panic: every parameter back to its default, every note off.
void Panic(Instance* s) {
  for (int p = 0; p < P_COUNT; ++p) s->param[p] = DefaultValue(p);
  AllNotesOff(s);
  s->pedal = false;
  s->bend = 0.0f;
  LayOutVoices(s);
  s->limiter.Init();
  ++s->display_rev;
}

void SetParam(void* inst, const char* key, const char* val) {
  Instance* s = static_cast<Instance*>(inst);
  if (!strcmp(key, "state")) { LoadState(s, val); return; }
  int p = FindParam(key, strlen(key));
  if (p < 0) return;
  float v = Clamp(static_cast<float>(atof(val)), MinOf(p), MaxOf(p));
  uint64_t now = s->frames ? s->frames : 1;
  if (PARAMS[p].momentary) {
    // Triggers read back 0 at once, so a screen tap (which sends the opposite of what it read) always sends 1.
    // A Q-Link turn to the right sends small values above 0: it fires once per turn.
    if (s->loading || v <= 0.0f) return;
    bool tap = v > 0.5f;
    bool in_gesture = s->last_touch[p] != 0 && now - s->last_touch[p] < kGestureFrames;
    s->last_touch[p] = now;
    if (!tap && in_gesture) return;
    if (p == P_PANIC) Panic(s);
    return;
  }
  if (PARAMS[p].nopts == 2 && !s->loading) {
    // A Q-Link turn either way flips the switch, once per turn; a tap is a turn of one. The rest of a turn is
    // ignored, and display_rev puts MPC's own guess back to ours.
    bool in_gesture = s->last_touch[p] != 0 && now - s->last_touch[p] < kGestureFrames;
    s->last_touch[p] = now;
    if (in_gesture) { ++s->display_rev; return; }
    int cur = Option(s, p), want = static_cast<int>(v + 0.5f);
    s->param[p] = static_cast<float>(want != cur ? want : 1 - cur);
    ++s->display_rev;
    if (p == P_VOICE_MODE) LayOutVoices(s);
    return;
  }
  if (PARAMS[p].nopts > 2 && !s->loading) {
    // One step per kStepLockFrames of audio time however fast a Q-Link turns; a jump of more than one step (a tap
    // on another option) is never held back.
    int from = Option(s, p), to = static_cast<int>(v + 0.5f);
    if (to - from == 1 || from - to == 1) {
      if (s->last_step[p] != 0 && s->frames - s->last_step[p] < kStepLockFrames) { ++s->display_rev; return; }
      s->last_step[p] = now;
    }
  }
  s->param[p] = v;
  if (p == P_VOICES && !s->loading) LayOutVoices(s);
}

int FormatHz(char* buf, int len, float hz) {
  if (hz >= 1000.0f) return snprintf(buf, len, "%.2f kHz", hz / 1000.0f);
  if (hz >= 10.0f) return snprintf(buf, len, "%.0f Hz", hz);
  if (hz >= 1.0f) return snprintf(buf, len, "%.2f Hz", hz);
  return snprintf(buf, len, "%.3f Hz", hz);
}

int Display(const Instance* s, int p, char* buf, int len) {
  float v = s->param[p];
  switch (p) {
    case P_COARSE:   // the module's tooltip: semitones
      if (Option(s, P_COARSE_MODE) > 0) return snprintf(buf, len, "%+d st", static_cast<int>((v + 0.04f) * 12.0f) - 12);
      return snprintf(buf, len, "%+.1f st", v * 12.0f - 12.0f);
    case P_FINE:
      return snprintf(buf, len, "%+.0f ct", v * 1200.0f);
    case P_WIDTH:    // duty cycle
      return snprintf(buf, len, "%.0f %%", (0.5f - 0.5f * v) * 100.0f);
    case P_PWM:
      return snprintf(buf, len, "%.0f %%", v * 200.0f);
    case P_VCO_MOD:  // depth in octaves (the module squares the slider)
      return snprintf(buf, len, "%.2f oct", v * v);
    case P_CUTOFF: case P_HPF:
      return FormatHz(buf, len, 440.0f * powf(2.0f, v - 5.0f));
    case P_RES:
      return snprintf(buf, len, "%.0f %%", v * 10.0f);
    case P_FLT_CV1: case P_FLT_CV2: case P_VCA_CV:
      return snprintf(buf, len, "%+.2fx", v);
    case P_LFO_RATE:
      return FormatHz(buf, len, 0.1f * powf(2.0f, v + s->param[P_LFO_FINE]));
    case P_LFO_FINE:
      return snprintf(buf, len, "%+.2f", v);
    case P_VOICES:
      return snprintf(buf, len, "%d", static_cast<int>(v + 0.5f));
    case P_BEND:
      return snprintf(buf, len, "%d st", static_cast<int>(v + 0.5f));
    case P_LEVEL:
      if (v <= 0.0001f) return snprintf(buf, len, "-inf dB");
      return snprintf(buf, len, "%.1f dB", 20.0f * log10f(v));
    case P_LIM_DRIVE:
      return snprintf(buf, len, "+%.1f dB", v);
    case P_LIM_CEILING:
      return snprintf(buf, len, "%.1f dB", v);
    case P_LIM_RELEASE:
      return snprintf(buf, len, "%.0f ms", v);
  }
  if (PARAMS[p].nopts == 0 && PARAMS[p].min == 0.0f && PARAMS[p].max == 1.0f)
    return snprintf(buf, len, "%.0f %%", v * 100.0f);
  return 0;
}

int GetParam(void* inst, const char* key, char* buf, int buf_len) {
  const Instance* s = static_cast<const Instance*>(inst);
  if (!strcmp(key, "state")) return SaveState(s, buf, buf_len);
  if (!strcmp(key, "display_rev")) return snprintf(buf, buf_len, "%d", s->display_rev);
  size_t len = strlen(key);
  if (len > 8 && !strcmp(key + len - 8, "_display")) {
    int p = FindParam(key, len - 8);
    return p < 0 ? 0 : Display(s, p, buf, buf_len);
  }
  int p = FindParam(key, len);
  if (p < 0) return 0;
  if (PARAMS[p].momentary) return snprintf(buf, buf_len, "0");
  if (PARAMS[p].nopts > 0) return snprintf(buf, buf_len, "%d", Option(s, p));
  return snprintf(buf, buf_len, "%g", s->param[p]);
}

inline int16_t ToShort(float x) {
  x *= 32768.0f;
  return static_cast<int16_t>(x > 32767.0f ? 32767.0f : (x < -32768.0f ? -32768.0f : x));
}

void RenderBlock(Instance* s, int16_t* out_lr, int frames) {
  const int groups = (s->active_voices + kLanes - 1) / kLanes;
  const float bend_v = s->bend * floorf(s->param[P_BEND] + 0.5f) / 12.0f;
  const float gain = s->param[P_LEVEL] / kVolts;
  Inputs& in = s->in;
  for (int f = 0; f < frames; ++f) {
    // the MIDI-CV cables: V/Oct, Gate, Trig per voice
    for (int v = 0; v < kMaxVoices; ++v) {
      Voice& vo = s->voice[v];
      if (v < s->active_voices && vo.note >= 0) in.voct1[v] = (vo.note - 60) / 12.0f + bend_v;
      in.gate[v] = vo.gate && v < s->active_voices ? kGateVolts : 0.0f;
      in.trig[v] = vo.trig > 0 ? kGateVolts : 0.0f;
      if (vo.trig > 0) --vo.trig;
    }
    // cvDivider: the knobs every 16 samples
    if (++s->synth.cvCounter >= 16) {
      s->synth.cvCounter = 0;
      GetParams(s);
    }
    float y = TickFrame(s, groups) * gain;
    s->out_l[f] = y;
    s->out_r[f] = y;
  }
  s->s_drive += 0.25f * (s->param[P_LIM_DRIVE] - s->s_drive);
  s->s_ceiling += 0.25f * (s->param[P_LIM_CEILING] - s->s_ceiling);
  s->limiter.Process(s->out_l, s->out_r, frames, s->s_drive, s->s_ceiling, s->param[P_LIM_RELEASE], kSampleRate);
  for (int i = 0; i < frames; ++i) {
    out_lr[2 * i] = ToShort(s->out_l[i]);
    out_lr[2 * i + 1] = ToShort(s->out_r[i]);
  }
}

void Render(void* inst, int16_t* out_lr, int frames) {
  Instance* s = static_cast<Instance*>(inst);
  ScopedDenormalDisable no_denormals;
  for (int off = 0; off < frames; off += kMaxBlock) {
    int n = frames - off < kMaxBlock ? frames - off : kMaxBlock;
    RenderBlock(s, out_lr + 2 * off, n);
  }
  s->frames += frames;
}

const mpc_engine_t kEngine = { Create, Destroy, Midi, SetParam, GetParam, Render, NULL };

}  // namespace

extern "C" const mpc_engine_t* mpc_engine(void) { return &kEngine; }

// The tests' window onto the voices (hidden in the release build, -fvisibility=hidden).
extern "C" int interzonexxl_test_voice_gate(void* inst, int v) {
  const Instance* s = static_cast<const Instance*>(inst);
  return v >= 0 && v < kMaxVoices && s->voice[v].gate ? s->voice[v].note : -1;
}
