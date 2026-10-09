// SPDX-License-Identifier: GPL-3.0-or-later
// Mutable Instruments Marbles (VCV "Random Sampler") as a modulation source: in its own translation unit, because
// Marbles' and Tides 2's resource headers define the same macro names.
#include "marbles_source.h"

#include <math.h>

#include "marbles/note_filter.h"
#include "marbles/random/random_generator.h"
#include "marbles/random/random_stream.h"
#include "marbles/random/t_generator.h"
#include "marbles/random/x_y_generator.h"
#include "stmlib/utils/gate_flags.h"

namespace ixxl {

namespace {

#include "marbles_scales.inc"

inline float Clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline int Opt(const float* p, int i) { return static_cast<int>(p[i] + 0.5f); }
inline double Frac(double x) { return x - floor(x); }
inline float SyncBeats(const float* p, int key) {
  int o = Opt(p, key);
  return o <= 0 ? 0.0f : kDivisionBeats[o - 1];
}

// ------------------------------------------------------------------------------------------------ Marbles
// Marbles.cpp process()/stepBlock(), blocks of 8 frames (VCV: 5). The T CLOCK jack is the MPC tempo when Sync is set.
const int kMarblesBlock = 8;
const int kLoopLength[] = { 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 5, 5, 6, 6, 6, 7, 7, 8, 8, 8, 10, 10,
                            12, 12, 12, 14, 14, 16, 16 };
const marbles::Ratio kYDividerRatios[] = { { 1, 64 }, { 1, 48 }, { 1, 32 }, { 1, 24 }, { 1, 16 }, { 1, 12 }, { 1, 8 },
                                           { 1, 6 }, { 1, 4 }, { 1, 3 }, { 1, 2 }, { 1, 1 } };

struct Marbles {
  marbles::RandomGenerator random_generator;
  marbles::RandomStream random_stream;
  marbles::TGenerator t_generator;
  marbles::XYGenerator xy_generator;
  marbles::NoteFilter note_filter;
  stmlib::GateFlags last_t_clock = 0;

  void Init(float sr) {
    random_generator.Init(1);
    random_stream.Init(&random_generator);
    note_filter.Init();
    t_generator.Init(&random_stream, sr);
    xy_generator.Init(&random_stream, sr);
    for (int i = 0; i < 6; i++) xy_generator.LoadScale(i, preset_scales[i]);
    // Y too. The firmware and VCV load scales into X1-X3 only, so Y keeps OutputChannel::Init's placeholder (one
    // degree of weight 0) and, once Steps quantizes it, reads past the 16-entry voltage table (level first = 0xff;
    // found under UBSan). Y follows X's scale here, which is what the Y output's Steps control implies.
    for (int i = 0; i < 6; i++) xy_generator.LoadScale(marbles::kNumXChannels, i, preset_scales[i]);
  }

  // out: T1, T2, T3, Y, X1, X2, X3 is the module's jack order; here X1, X2, X3, Y, T1, T2, T3 (the source list's)
  void Render(const float* p, const Clock& clock, int frames, float* out[7]) {
    float beats = SyncBeats(p, P_MB_SYNC);
    bool t_external_clock = beats > 0.0f;
    float deja_vu = Clampf(p[P_MB_DEJA_VU], 0.0f, 1.0f);
    int length_count = static_cast<int>(sizeof(kLoopLength) / sizeof(kLoopLength[0]));
    int deja_vu_length = kLoopLength[static_cast<int>(roundf(Clampf(p[P_MB_LENGTH], 0.0f, 1.0f) * (length_count - 1)))];
    bool t_deja_vu = Opt(p, P_MB_T_DV) != 0, x_deja_vu = Opt(p, P_MB_X_DV) != 0;

    t_generator.set_model(static_cast<marbles::TGeneratorModel>(Opt(p, P_MB_T_MODE)));
    t_generator.set_range(static_cast<marbles::TGeneratorRange>(Opt(p, P_MB_T_RANGE)));
    t_generator.set_rate(60.0f * p[P_MB_T_RATE]);
    t_generator.set_bias(Clampf(p[P_MB_T_BIAS], 0.0f, 1.0f));
    t_generator.set_jitter(Clampf(p[P_MB_T_JITTER], 0.0f, 1.0f));
    t_generator.set_deja_vu(t_deja_vu ? deja_vu : 0.0f);
    t_generator.set_length(deja_vu_length);
    t_generator.set_pulse_width_mean(0.0f);
    t_generator.set_pulse_width_std(0.0f);

    marbles::GroupSettings x;
    x.control_mode = static_cast<marbles::ControlMode>(Opt(p, P_MB_X_MODE));
    x.voltage_range = static_cast<marbles::VoltageRange>(Opt(p, P_MB_X_RANGE));
    float x_spread = Clampf(p[P_MB_X_SPREAD], 0.0f, 1.0f);
    float note_cv = 0.5f * x_spread;
    float u = note_filter.Process(0.5f * (note_cv + 1.0f));
    x.register_mode = false;   // the module's External mode (X from the X spread input) has no input to sample here
    x.register_value = u;
    x.spread = x_spread;
    x.bias = Clampf(p[P_MB_X_BIAS], 0.0f, 1.0f);
    x.steps = Clampf(p[P_MB_X_STEPS], 0.0f, 1.0f);
    x.deja_vu = x_deja_vu ? deja_vu : 0.0f;
    x.length = deja_vu_length;
    x.ratio.p = 1;
    x.ratio.q = 1;
    x.scale_index = Opt(p, P_MB_X_SCALE);
    marbles::GroupSettings y;
    y.control_mode = marbles::CONTROL_MODE_IDENTICAL;
    y.voltage_range = x.voltage_range;
    y.register_mode = false;
    y.register_value = 0.0f;
    y.spread = x.spread;
    y.bias = x.bias;
    y.steps = x.steps;
    y.deja_vu = 0.0f;
    y.length = 1;
    y.ratio = kYDividerRatios[Opt(p, P_MB_Y_DIV)];
    y.scale_index = x.scale_index;

    stmlib::GateFlags t_clocks[kMarblesBlock], xy_clocks[kMarblesBlock];
    float ramp_master[kMarblesBlock], ramp_external[kMarblesBlock], ramp_slave[2][kMarblesBlock];
    bool gates[kMarblesBlock * 2];
    float voltages[kMarblesBlock * 4];
    for (int start = 0; start < frames; start += kMarblesBlock) {
      int n = frames - start < kMarblesBlock ? frames - start : kMarblesBlock;
      for (int i = 0; i < n; ++i) {
        bool high = t_external_clock && Frac(clock.beat[start + i] / beats) < 0.5;
        last_t_clock = stmlib::ExtractGateFlags(last_t_clock, high);
        t_clocks[i] = last_t_clock;
        xy_clocks[i] = stmlib::GATE_FLAG_LOW;
      }
      marbles::Ramps ramps;
      ramps.master = ramp_master;
      ramps.external = ramp_external;
      ramps.slave[0] = ramp_slave[0];
      ramps.slave[1] = ramp_slave[1];
      t_generator.Process(t_external_clock, t_clocks, ramps, gates, n);
      xy_generator.Process(marbles::CLOCK_SOURCE_INTERNAL_T1_T2_T3, x, y, xy_clocks, ramps, voltages, n);
      for (int i = 0; i < n; ++i) {
        out[0][start + i] = voltages[i * 4 + 0];
        out[1][start + i] = voltages[i * 4 + 1];
        out[2][start + i] = voltages[i * 4 + 2];
        out[3][start + i] = voltages[i * 4 + 3];
        out[4][start + i] = gates[i * 2 + 0] ? 10.0f : 0.0f;
        out[5][start + i] = ramp_master[i] < 0.5f ? 10.0f : 0.0f;
        out[6][start + i] = gates[i * 2 + 1] ? 10.0f : 0.0f;
      }
    }
  }
};

}  // namespace

struct MarblesSource::Impl : Marbles {};

MarblesSource::MarblesSource() : impl_(new Impl()) {}
MarblesSource::~MarblesSource() { delete impl_; }
void MarblesSource::Init(float sr) { impl_->Init(sr); }
void MarblesSource::Render(const float* p, const Clock& clock, int frames, float* out[7]) { impl_->Render(p, clock, frames, out); }

}  // namespace ixxl
