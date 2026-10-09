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
// Patching (as PlateauXXL): every other input jack of the module (VOct 2, PWM, Mixer Ext, Filter Freq 1 and 2, Res,
// VCA Level, LFO Rate / Trig / Reset, Env Gate / Trig) picks a source and has an amount (Freq 1/2 and VCA Level use
// the module's own attenuverters). The sources are PlateauXXL's built-in modules (sources.h: four Bogaudio LFOs,
// Tidal Modulator 2, Random Sampler, two CV and two gate sequencers, on the MPC tempo), the Ext Osc (an extra
// oscillator per voice that follows the played note), Interzone's own output jacks (the LFO's seven, the envelope's
// two and the VCO's three; per voice in Poly, one sample late as on a VCV cable) and MIDI velocity, mod wheel and
// pressure. A per-voice source on a poly jack is a poly cable; any other source is a mono cable (every voice).
// Env Gate and Env Trig add to the MIDI gate and retrigger; a patched gate plays every voice that has had a note
// (and voice 1 always), at its last pitch.
//
// Presets (PlateauXXL's): 16 slots, files "Preset NN.txt" holding the state string in /sdcard/InterzoneXXL Presets,
// written and read on a worker thread.
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

#include <pthread.h>
#include <sys/stat.h>
#include <time.h>

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
#include "sources.h"

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
const int kPresetSlots = 16;
const int kStateMax = 8192;              // the wrapper's chunk buffer
static_assert(kMaxBlock == ixxl::kMaxBlock, "block sizes");

// The engine's own sources (gen_params.py SOURCES, after the modules')
enum {
  SRC_EXT_OSC = kNumModuleSources, SRC_IZ_SINE, SRC_IZ_TRI, SRC_IZ_SAW_UP, SRC_IZ_SAW_DN, SRC_IZ_PULSE, SRC_IZ_SH,
  SRC_IZ_NOISE, SRC_ENV_POS, SRC_ENV_NEG, SRC_VCO_SAW, SRC_VCO_PULSE, SRC_VCO_SUB, SRC_VELOCITY, SRC_MOD_WHEEL,
  SRC_PRESSURE, SRC_END
};
static_assert(SRC_END == kNumSources, "gen_params.py SOURCES and the engine's list differ");

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

// The module's input jacks, in volts, for one frame: one value per voice (a poly cable; a mono cable is the same
// value on every voice), and the LFO's jacks, which the module reads on its first channel only.
struct Inputs {
  alignas(16) float voct1[kMaxVoices];
  alignas(16) float voct2[kMaxVoices];
  alignas(16) float gate[kMaxVoices];
  alignas(16) float trig[kMaxVoices];
  alignas(16) float pwm[kMaxVoices];
  alignas(16) float ext[kMaxVoices];
  alignas(16) float cutoff1[kMaxVoices];
  alignas(16) float cutoff2[kMaxVoices];
  alignas(16) float res[kMaxVoices];
  alignas(16) float vca[kMaxVoices];
  float lfo_rate, lfo_trig, lfo_sync;
};

// What the module's output jacks carried on the last frame (a VCV cable is one sample late), for the sources.
struct Taps {
  alignas(16) float env[kMaxVoices];
  alignas(16) float saw[kMaxVoices];
  alignas(16) float pulse[kMaxVoices];
  alignas(16) float sub[kMaxVoices];
  alignas(16) float pitch[kMaxVoices];   // the glided V/Oct (V/Oct 1 + 2), for the Ext Osc
  float lfo[7];                          // DLFO::out, x 5 V; [6] = the noise jack (the selected noise)
};

// Ext Osc: one simple oscillator per voice, +-5 V.
struct ExtOsc {
  float phase[kMaxVoices];
  uint32_t rng;
};

struct Instance {
  Synth synth;
  ixxl::Sources sources;
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
  Taps taps;
  ExtOsc xo;
  float velocity[kMaxVoices];  // 0-10 V
  float mod_wheel, pressure;   // 0-10 V
  // tempo
  float bpm;
  double beat;
  bool restart_pending;
  float src[kNumSources][kMaxBlock];   // this block of the modules' sources (sources.h)
  float out_l[kMaxBlock], out_r[kMaxBlock];
  // Q-Links: when each parameter was last set (switches, triggers) or last moved one step (lists)
  uint64_t frames;
  uint64_t last_touch[P_COUNT];
  uint64_t last_step[P_COUNT];
  bool loading;
  volatile int display_rev;
  volatile int panic_pending;
  // presets (file I/O on the worker thread)
  char preset_dir[512];
  pthread_mutex_t preset_mutex;        // guards save_text / save_slot / load_slot (screen thread vs worker)
  char* save_text;                     // a snapshot waiting to be written (the worker frees it)
  int save_slot, load_slot;
  char* loaded_text;                   // read by the worker, applied by the audio thread (atomic hand-over)
  char* retired_text;                  // applied, handed back to the worker to free
  volatile unsigned slot_used;         // bit per slot: a file exists
  volatile int preset_status;          // 0 idle, 1 saved, 2 loaded, 3 save failed, 4 empty slot, 5 load failed
  volatile int status_slot;
  pthread_t worker;
  bool worker_running;
  volatile bool worker_quit;
  pthread_mutex_t worker_mutex;
  pthread_cond_t worker_wake;
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
  Taps& tap = s->taps;
  for (int i = 0; i < groups; ++i) {
    const int c = i * kLanes;
    const __m128 vPwmIn = _mm_loadu_ps(in.pwm + c);
    const __m128 vCut1 = _mm_loadu_ps(in.cutoff1 + c), vCut2 = _mm_loadu_ps(in.cutoff2 + c);
    const __m128 vRes = _mm_loadu_ps(in.res + c), vVca = _mm_loadu_ps(in.vca + c);
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
    _mm_storeu_ps(tap.pitch + c, z.vPitch);
    _mm_storeu_ps(tap.env + c, z.vEnv[i].env.v);
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
  float sum = 0.0f;
  alignas(16) float lane[4];
  for (int i = 0; i < groups; ++i) {
    z.vOsc[i].process();
    z.vSubWave = subSaw ? z.vOsc[i].__subSaw : z.vOsc[i].__subPulse;
    z.vExtInput = _mm_loadu_ps(in.ext + i * kLanes);
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

    _mm_storeu_ps(tap.saw + i * kLanes, _mm_mul_ps(z.vOsc[i].__saw, z.__five));
    _mm_storeu_ps(tap.pulse + i * kLanes, _mm_mul_ps(z.vOsc[i].__pulse, z.__five));
    _mm_storeu_ps(tap.sub + i * kLanes, _mm_mul_ps(z.vSubWave, z.__five));
    _mm_store_ps(lane, z.vOutput);
    for (int l = 0; l < kLanes; ++l)
      if (i * kLanes + l < s->active_voices) sum += lane[l];
  }
  for (int w = 0; w < 6; ++w) tap.lfo[w] = z.lfo.out[w] * 5.f;   // the LFO jacks: sine .. S+H
  tap.lfo[6] = z.noise * 5.f;                                    // and the noise jack
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

void NoteOn(Instance* s, int note, int velocity) {
  const float vel_v = velocity * (10.0f / 127.0f);
  s->key_down[note] = true;
  if (!IsPoly(s)) {
    MonoRemove(s, note);
    if (s->held_count == kHeldMax) {   // forget the oldest
      memmove(s->held, s->held + 1, sizeof(int) * (kHeldMax - 1));
      --s->held_count;
    }
    s->held[s->held_count++] = note;
    s->velocity[0] = vel_v;
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
  s->velocity[pick] = vel_v;
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
      if (d2 > 0) { NoteOn(s, d1, d2); break; }
      // fall through: velocity 0 is a note off
    case 0x80:
      NoteOff(s, d1);
      break;
    case 0xD0:   // channel pressure
      s->pressure = d1 * (10.0f / 127.0f);
      break;
    case 0xE0:
      s->bend = (((d2 << 7) | d1) - 8192) / 8192.0f;
      break;
    case 0xB0:
      if (d1 == 1) {
        s->mod_wheel = d2 * (10.0f / 127.0f);
      } else if (d1 == 64) {
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
// ------------------------------------------------------------------------------------------------ presets
// PlateauXXL's (from RMXXXL): "Preset NN.txt" files holding the state string; the worker thread does the file I/O.
void PresetPath(const Instance* s, int slot, char* buf, size_t len) {
  snprintf(buf, len, "%s/Preset %02d.txt", s->preset_dir, slot + 1);
}

void ScanPresets(Instance* s) {
  unsigned used = 0;
  char path[600];
  for (int i = 0; i < kPresetSlots; ++i) {
    struct stat st;
    PresetPath(s, i, path, sizeof path);
    if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) used |= 1u << i;
  }
  s->slot_used = used;
}

void SetPresetStatus(Instance* s, int slot, int status) {
  s->status_slot = slot;
  s->preset_status = status;
  __atomic_add_fetch(&s->display_rev, 1, __ATOMIC_RELAXED);
}

bool WriteText(const char* dir, const char* path, const char* text) {
  mkdir(dir, 0777);
  char tmp[640];
  snprintf(tmp, sizeof tmp, "%s.tmp", path);
  FILE* f = fopen(tmp, "w");
  if (!f) return false;
  size_t len = strlen(text);
  bool ok = fwrite(text, 1, len, f) == len;
  ok = (fclose(f) == 0) && ok;
  if (ok) ok = rename(tmp, path) == 0;
  if (!ok) remove(tmp);
  return ok;
}

char* ReadText(const char* path) {
  FILE* f = fopen(path, "r");
  if (!f) return NULL;
  char* buf = static_cast<char*>(malloc(kStateMax));
  size_t len = buf ? fread(buf, 1, kStateMax - 1, f) : 0;
  fclose(f);
  if (!buf) return NULL;
  buf[len] = 0;
  return buf;
}

void PresetWork(Instance* s) {
  free(__atomic_exchange_n(&s->retired_text, (char*)NULL, __ATOMIC_ACQ_REL));
  pthread_mutex_lock(&s->preset_mutex);
  char* text = s->save_text;
  int save_slot = s->save_slot, load_slot = s->load_slot;
  s->save_text = NULL;
  s->load_slot = -1;
  pthread_mutex_unlock(&s->preset_mutex);
  char path[600];
  if (text) {
    PresetPath(s, save_slot, path, sizeof path);
    bool ok = WriteText(s->preset_dir, path, text);
    free(text);
    if (ok) s->slot_used = s->slot_used | (1u << save_slot);
    SetPresetStatus(s, save_slot, ok ? 1 : 3);
  }
  if (load_slot >= 0) {
    PresetPath(s, load_slot, path, sizeof path);
    char* loaded = ReadText(path);
    if (!loaded) {
      s->slot_used = s->slot_used & ~(1u << load_slot);
      SetPresetStatus(s, load_slot, 4);
    } else {
      s->status_slot = load_slot;
      free(__atomic_exchange_n(&s->loaded_text, loaded, __ATOMIC_ACQ_REL));
    }
  }
}

void* WorkerLoop(void* arg) {
  Instance* s = static_cast<Instance*>(arg);
  while (!s->worker_quit) {
    pthread_mutex_lock(&s->worker_mutex);
    timespec until;
    clock_gettime(CLOCK_REALTIME, &until);
    until.tv_nsec += 20000000;   // 20 ms
    if (until.tv_nsec >= 1000000000) { until.tv_sec += 1; until.tv_nsec -= 1000000000; }
    if (!s->worker_quit) pthread_cond_timedwait(&s->worker_wake, &s->worker_mutex, &until);
    pthread_mutex_unlock(&s->worker_mutex);
    PresetWork(s);
  }
  return NULL;
}

// ------------------------------------------------------------------------------------------------ engine interface
void* Create(const char* data_dir) {
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
  s->sources.Init(kSampleRate);
  s->bpm = 120.0f;
  s->xo.rng = 22222u;
  // As on VCV Rack, the knobs are first read on the 16th sample (cvDivider); until then the DSP keeps its constructed
  // state, which is silent.
  snprintf(s->preset_dir, sizeof s->preset_dir, "%s", data_dir ? data_dir : "InterzoneXXL Presets");
  pthread_mutex_init(&s->preset_mutex, NULL);
  s->load_slot = -1;
  s->status_slot = -1;
  ScanPresets(s);
  pthread_mutex_init(&s->worker_mutex, NULL);
  pthread_cond_init(&s->worker_wake, NULL);
  s->worker_running = pthread_create(&s->worker, NULL, WorkerLoop, s) == 0;
  return s;
}

void Destroy(void* inst) {
  Instance* s = static_cast<Instance*>(inst);
  if (!s) return;
  if (s->worker_running) {
    pthread_mutex_lock(&s->worker_mutex);
    s->worker_quit = true;
    pthread_cond_signal(&s->worker_wake);
    pthread_mutex_unlock(&s->worker_mutex);
    pthread_join(s->worker, NULL);
  }
  pthread_cond_destroy(&s->worker_wake);
  pthread_mutex_destroy(&s->worker_mutex);
  pthread_mutex_destroy(&s->preset_mutex);
  free(s->save_text);
  free(s->loaded_text);
  free(s->retired_text);
  s->~Instance();
  free(s);
}

int FindParam(const char* key, size_t len) {
  for (int p = 0; p < P_COUNT; ++p)
    if (strlen(PARAMS[p].key) == len && !strncmp(key, PARAMS[p].key, len)) return p;
  return -1;
}

void SetParam(void* inst, const char* key, const char* val);

// Not saved or loaded: triggers and the preset status readout.
bool StateSkips(int p) { return PARAMS[p].momentary || p == P_PRESET_INFO; }

int SaveState(const Instance* s, char* buf, int buf_len) {
  int len = 0;
  for (int p = 0; p < P_COUNT && len < buf_len; ++p) {
    if (StateSkips(p)) continue;
    len += snprintf(buf + len, buf_len - len, "%s=%g;", PARAMS[p].key, s->param[p]);
  }
  return len < buf_len ? len : buf_len - 1;
}

// preset: a user preset (keeps the slot) rather than MPC restoring a project.
void LoadState(Instance* s, const char* state, bool preset) {
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
        if (strcmp(item, "state") && !(preset && !strcmp(item, "preset_slot"))) SetParam(s, item, eq + 1);
      }
    }
    state += n;
    if (*state == ';') ++state;
  }
  s->loading = false;
  LayOutVoices(s);
}

void RequestSave(Instance* s) {
  char* text = static_cast<char*>(malloc(kStateMax));
  if (!text) return;
  SaveState(s, text, kStateMax);
  pthread_mutex_lock(&s->preset_mutex);
  free(s->save_text);
  s->save_text = text;
  s->save_slot = Option(s, P_PRESET_SLOT);
  pthread_mutex_unlock(&s->preset_mutex);
}

void RequestLoad(Instance* s) {
  pthread_mutex_lock(&s->preset_mutex);
  s->load_slot = Option(s, P_PRESET_SLOT);
  pthread_mutex_unlock(&s->preset_mutex);
}

// Panic: every parameter back to its default (the preset slot stays), every note off, the sources restarted.
void Panic(Instance* s) {
  for (int p = 0; p < P_COUNT; ++p) {
    if (p == P_PRESET_SLOT) continue;
    s->param[p] = DefaultValue(p);
  }
  AllNotesOff(s);
  s->pedal = false;
  s->bend = 0.0f;
  s->mod_wheel = s->pressure = 0.0f;
  LayOutVoices(s);
  s->limiter.Init();
  s->sources.Init(kSampleRate);
  memset(s->src, 0, sizeof s->src);
  s->beat = 0.0;
  ++s->display_rev;
}

void SetParam(void* inst, const char* key, const char* val) {
  Instance* s = static_cast<Instance*>(inst);
  if (!strcmp(key, "state")) { LoadState(s, val, false); return; }
  if (!strcmp(key, "lfo_bpm")) {   // the MPC tempo (vst.json HAS_LFO_BPM)
    float b = static_cast<float>(atof(val));
    if (b >= 20.0f && b <= 400.0f) s->bpm = b;
    return;
  }
  if (!strcmp(key, "transport")) {   // play pressed or the song position jumped back (HAS_TRANSPORT)
    if (atoi(val) == 1) s->restart_pending = true;
    return;
  }
  int p = FindParam(key, strlen(key));
  if (p < 0 || p == P_PRESET_INFO) return;
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
    if (p == P_PRESET_SAVE) RequestSave(s);
    if (p == P_PRESET_LOAD) RequestLoad(s);
    if (s->worker_running && (p == P_PRESET_SAVE || p == P_PRESET_LOAD)) pthread_cond_signal(&s->worker_wake);
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

bool KeyEndsWith(const char* key, const char* tail) {
  size_t a = strlen(key), b = strlen(tail);
  return a >= b && !strcmp(key + a - b, tail);
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
    case P_TD_FREQ:
      return snprintf(buf, len, "%+.1f st", v);
    case P_MB_T_RATE:
      return snprintf(buf, len, "%+.0f st", v * 60.0f);
    case P_XO_TUNE:
      return snprintf(buf, len, "%+.2f st", v);
    case P_XO_PW:
      return snprintf(buf, len, "%.0f %%", v * 100.0f);
    case P_PRESET_INFO: {
      int slot = Option(s, P_PRESET_SLOT);
      int st = s->status_slot == slot ? s->preset_status : 0;
      static const char* const kStatus[6] = { NULL, "SAVED", "LOADED", "SAVE FAILED", "EMPTY", "LOAD FAILED" };
      if (st > 0 && st < 6) return snprintf(buf, len, "PRESET %d: %s", slot + 1, kStatus[st]);
      return snprintf(buf, len, "PRESET %d: %s", slot + 1, (s->slot_used >> slot) & 1u ? "STORED" : "EMPTY");
    }
  }
  const char* key = PARAMS[p].key;
  if (!strncmp(key, "lfo", 3) && key[3] >= '1' && key[3] <= '4') {   // PlateauXXL's Bogaudio LFO readouts
    int first = P_LFO1_WAVE + (key[3] - '1') * (P_LFO2_WAVE - P_LFO1_WAVE);
    if (KeyEndsWith(key, "_freq")) {
      if (Option(s, first + 1) > 0) return snprintf(buf, len, "Sync");
      float cv = v + (Option(s, first + 3) ? -11.0f : -7.0f);
      float hz = 261.626f * powf(2.0f, cv);
      return FormatHz(buf, len, hz > 2000.0f ? 2000.0f : hz);
    }
    if (KeyEndsWith(key, "_pw")) return snprintf(buf, len, "%+.0f %%", v * 100.0f);
    if (KeyEndsWith(key, "_offset")) return snprintf(buf, len, "%+.2f V", v * 5.0f);
    return snprintf(buf, len, "%.0f %%", v * 100.0f);
  }
  if (KeyEndsWith(key, "_cv")) return snprintf(buf, len, "%+.0f %%", v * 100.0f);
  if (!strncmp(key, "sq", 2) && key[3] == '_' && key[4] == 's' && key[5] >= '0' && key[5] <= '9')
    return snprintf(buf, len, "%+.2f V", v);
  if (PARAMS[p].nopts == 0 && PARAMS[p].min == 0.0f && PARAMS[p].max == 1.0f)
    return snprintf(buf, len, "%.0f %%", v * 100.0f);
  if (PARAMS[p].nopts == 0 && PARAMS[p].min == 0.05f && PARAMS[p].max == 1.0f)
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

// Ext Osc, one frame for every active voice: a simple oscillator on the voice's glided pitch, +-5 V.
void ExtOscFrame(Instance* s, float* out) {
  const int wave = Option(s, P_XO_WAVE);
  const float shift = (Option(s, P_XO_OCT) - 3) + s->param[P_XO_TUNE] / 12.0f;
  const float pw = s->param[P_XO_PW];
  for (int v = 0; v < s->active_voices; ++v) {
    float hz = 261.6256f * exp2f(s->taps.pitch[v] + shift);
    float dt = Clamp(hz / kSampleRate, 0.0f, 0.45f);
    float& ph = s->xo.phase[v];
    ph += dt;
    if (ph >= 1.0f) ph -= 1.0f;
    float y;
    switch (wave) {
      case 0: {   // saw, polyBLEP
        y = 2.0f * ph - 1.0f;
        if (ph < dt) { float t = ph / dt; y -= t + t - t * t - 1.0f; }
        else if (ph > 1.0f - dt) { float t = (ph - 1.0f) / dt; y -= t * t + t + t + 1.0f; }
        break;
      }
      case 1: {   // pulse (Width), polyBLEP on both edges
        y = ph < pw ? 1.0f : -1.0f;
        float edges[2] = { 0.0f, pw };
        for (int e = 0; e < 2; ++e) {
          float t = ph - edges[e];
          if (t < 0.0f) t += 1.0f;
          float sign = e == 0 ? 1.0f : -1.0f;
          if (t < dt) { float x = t / dt; y += sign * -(x + x - x * x - 1.0f); }
          else if (t > 1.0f - dt) { float x = (t - 1.0f) / dt; y += sign * -(x * x + x + x + 1.0f); }
        }
        break;
      }
      case 2: y = ph < 0.5f ? 4.0f * ph - 1.0f : 3.0f - 4.0f * ph; break;   // triangle
      case 3: y = sinf(6.2831853f * ph); break;                             // sine
      default: {  // white noise
        s->xo.rng = s->xo.rng * 1664525u + 1013904223u;
        y = (s->xo.rng >> 8) * (2.0f / 16777216.0f) - 1.0f;
      }
    }
    out[v] = 5.0f * y;
  }
}

// Volts a source puts on the jack at frame f, for voice v.
inline float SourceVolts(const Instance* s, int src, int f, int v, const float* xo) {
  if (src <= 0) return 0.0f;
  if (src < kNumModuleSources) return s->src[src][f];
  switch (src) {
    case SRC_EXT_OSC: return xo[v];
    case SRC_ENV_POS: return s->taps.env[v] * 10.0f;
    case SRC_ENV_NEG: return s->taps.env[v] * -10.0f;
    case SRC_VCO_SAW: return s->taps.saw[v];
    case SRC_VCO_PULSE: return s->taps.pulse[v];
    case SRC_VCO_SUB: return s->taps.sub[v];
    case SRC_VELOCITY: return s->velocity[v];
    case SRC_MOD_WHEEL: return s->mod_wheel;
    case SRC_PRESSURE: return s->pressure;
  }
  if (src >= SRC_IZ_SINE && src <= SRC_IZ_NOISE) return s->taps.lfo[src - SRC_IZ_SINE];
  return 0.0f;
}

void RenderBlock(Instance* s, int16_t* out_lr, int frames) {
  if (s->panic_pending) { s->panic_pending = 0; Panic(s); }
  char* preset = __atomic_exchange_n(&s->loaded_text, (char*)NULL, __ATOMIC_ACQ_REL);
  if (preset) {
    LoadState(s, preset, true);
    SetPresetStatus(s, s->status_slot, 2);
    free(__atomic_exchange_n(&s->retired_text, preset, __ATOMIC_ACQ_REL));   // the worker frees it
  }
  // ---- tempo and the modules' sources (PlateauXXL)
  ixxl::Clock clock;
  clock.bpm = s->bpm;
  clock.restart = s->restart_pending;
  if (s->restart_pending) { s->beat = 0.0; s->restart_pending = false; }
  const double beat_step = s->bpm / 60.0 / kSampleRate;
  for (int i = 0; i < frames; ++i) clock.beat[i] = s->beat + i * beat_step;
  s->beat += frames * beat_step;
  if (s->beat > 1e9) s->beat = fmod(s->beat, 6144.0);
  int src_of[kNumCvInputs];
  bool needed[kNumSources] = {};
  bool want_xo = false;
  for (int c = 0; c < kNumCvInputs; ++c) {
    int src = Option(s, kCvInput[c][0]);
    src_of[c] = src > 0 && src < kNumSources ? src : 0;
    needed[src_of[c]] = true;
    want_xo = want_xo || src_of[c] == SRC_EXT_OSC;
  }
  s->sources.Render(s->param, clock, needed, frames, s->src);

  const int groups = (s->active_voices + kLanes - 1) / kLanes;
  const float bend_v = s->bend * floorf(s->param[P_BEND] + 0.5f) / 12.0f;
  const float gain = s->param[P_LEVEL] / kVolts;
  float amount[kNumCvInputs];
  for (int c = 0; c < kNumCvInputs; ++c)   // the module's own attenuverters (Freq 1/2, VCA) scale inside TickFrame
    amount[c] = kCvInput[c][1] >= 0 && !kCvInputScaledByModule[c] ? s->param[kCvInput[c][1]] : 1.0f;
  Inputs& in = s->in;
  float xo[kMaxVoices] = {};
  float* const poly_in[] = { in.voct2, in.pwm, in.ext, in.cutoff1, in.cutoff2, in.res, in.vca };
  const int poly_id[] = { IN_VOCT2, IN_PWM_IN, IN_EXT_IN, IN_CUT1, IN_CUT2, IN_RES_IN, IN_VCA_IN };
  for (int f = 0; f < frames; ++f) {
    if (want_xo) ExtOscFrame(s, xo);
    // the MIDI-CV cables, with Env Gate and Env Trig's sources added
    for (int v = 0; v < kMaxVoices; ++v) {
      Voice& vo = s->voice[v];
      const bool active = v < s->active_voices;
      if (active && vo.note >= 0) in.voct1[v] = (vo.note - 60) / 12.0f + bend_v;
      float gate = vo.gate && active ? kGateVolts : 0.0f;
      float trig = vo.trig > 0 ? kGateVolts : 0.0f;
      if (vo.trig > 0) --vo.trig;
      if (active && (v == 0 || vo.note >= 0)) {
        gate += SourceVolts(s, src_of[IN_EGATE], f, v, xo);
        trig += SourceVolts(s, src_of[IN_ETRIG], f, v, xo);
      }
      in.gate[v] = gate;
      in.trig[v] = trig;
    }
    // the patched jacks
    for (int k = 0; k < 7; ++k) {
      const int c = poly_id[k], src = src_of[c];
      float* dst = poly_in[k];
      if (!src) { memset(dst, 0, sizeof(float) * kMaxVoices); continue; }
      for (int v = 0; v < s->active_voices; ++v) dst[v] = SourceVolts(s, src, f, v, xo) * amount[c];
    }
    in.lfo_rate = SourceVolts(s, src_of[IN_LRATE], f, 0, xo) * amount[IN_LRATE];
    in.lfo_trig = SourceVolts(s, src_of[IN_LTRIG], f, 0, xo);
    in.lfo_sync = SourceVolts(s, src_of[IN_LRESET], f, 0, xo);
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
