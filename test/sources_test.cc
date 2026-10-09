// SPDX-License-Identifier: GPL-3.0-or-later
// Offline tests of the modulation sources (src/sources.h) on their own: what each would put on a cable in VCV,
// at the right rate and in the right range, free running and locked to the MPC tempo.
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <vector>

#include "param_ids.h"
#include "sources.h"

extern "C" {
#include "params.h"
}

namespace {

const float kSr = 44100.0f;
int failures = 0;

void Check(bool ok, const char* what) {
  printf("%s %s\n", ok ? "ok  " : "FAIL", what);
  if (!ok) ++failures;
}

int FindOption(int p, const char* name) {
  for (int i = 0; i < PARAMS[p].nopts; ++i) if (!strcmp(PARAMS[p].opts[i], name)) return i;
  printf("no option %s\n", name);
  return 0;
}

struct Rig {
  ixxl::Sources src;
  float param[P_COUNT];
  float bpm = 120.0f;
  double beat = 0.0;
  bool restart = true;
  float out[kNumSources][ixxl::kMaxBlock];
  Rig() {
    for (int p = 0; p < P_COUNT; ++p) {
      param[p] = PARAMS[p].nopts > 0 ? floorf(PARAMS[p].def * (PARAMS[p].nopts - 1) + 0.5f) : PARAMS[p].min + PARAMS[p].def * (PARAMS[p].max - PARAMS[p].min);
    }
    src.Init(kSr);
  }
  // Render `seconds` of source `which`, returning one value per frame.
  std::vector<float> Run(int which, double seconds) {
    std::vector<float> v;
    bool needed[kNumSources] = {};
    needed[which] = true;
    int total = static_cast<int>(seconds * kSr);
    for (int done = 0; done < total; done += ixxl::kMaxBlock) {
      ixxl::Clock c;
      c.bpm = bpm;
      c.restart = restart;
      restart = false;
      for (int i = 0; i < ixxl::kMaxBlock; ++i) c.beat[i] = beat + i * bpm / 60.0 / kSr;
      beat += ixxl::kMaxBlock * bpm / 60.0 / kSr;
      src.Render(param, c, needed, ixxl::kMaxBlock, out);
      v.insert(v.end(), out[which], out[which] + ixxl::kMaxBlock);
    }
    return v;
  }
};

void MinMax(const std::vector<float>& v, float* lo, float* hi) {
  *lo = 1e9f; *hi = -1e9f;
  for (float x : v) { *lo = fminf(*lo, x); *hi = fmaxf(*hi, x); }
}

// Upward crossings of `level`, after the first `skip` frames.
int Crossings(const std::vector<float>& v, float level, size_t skip = 0) {
  int n = 0;
  for (size_t i = skip + 1; i < v.size(); ++i) if (v[i - 1] < level && v[i] >= level) ++n;
  return n;
}

}  // namespace

int main() {
  char msg[256];

  // ---- Bogaudio LFO
  {
    Rig r;   // LFO 1: Sine, synced to 1 bar by default; 120 BPM -> 2 s per cycle
    std::vector<float> v = r.Run(SRC_1, 8.0);
    float lo, hi;
    MinMax(v, &lo, &hi);
    snprintf(msg, sizeof msg, "LFO sine swings +/-5 V (%.2f .. %.2f V)", lo, hi);
    Check(lo < -4.9f && lo > -5.1f && hi > 4.9f && hi < 5.1f, msg);
    int n = Crossings(v, 0.0f);
    snprintf(msg, sizeof msg, "LFO synced to 1 bar at 120 BPM: 4 cycles in 8 s (%d)", n);
    Check(n == 4 || n == 3, msg);
    float quarter = v[static_cast<size_t>(0.5 * kSr)];
    snprintf(msg, sizeof msg, "LFO locked to the song position: peak on beat 2 (%.2f V at 0.5 s)", quarter);
    Check(quarter > 4.9f, msg);
  }
  {
    Rig r;
    r.param[P_LFO1_SYNC] = 0;   // Free: the module's default 2.04 Hz (knob 0, C-3)
    std::vector<float> v = r.Run(SRC_1, 10.0);
    int n = Crossings(v, 0.0f);
    snprintf(msg, sizeof msg, "LFO free at the module's default rate: %d cycles in 10 s (2.04 Hz -> 20)", n);
    Check(n >= 19 && n <= 21, msg);
    r.param[P_LFO1_WAVE] = FindOption(P_LFO1_WAVE, "Square");
    r.param[P_LFO1_OFFSET] = 1.0f;   // +5 V
    r.param[P_LFO1_SCALE] = 0.5f;
    std::vector<float> sq = r.Run(SRC_1, 2.0);
    float lo, hi;
    MinMax(sq, &lo, &hi);
    snprintf(msg, sizeof msg, "LFO square with Offset +5 V and Scale 50 %%: %.2f .. %.2f V (2.5 .. 7.5)", lo, hi);
    Check(fabsf(lo - 2.5f) < 0.05f && fabsf(hi - 7.5f) < 0.05f, msg);
    r.param[P_LFO1_WAVE] = FindOption(P_LFO1_WAVE, "Stepped");
    r.param[P_LFO1_OFFSET] = 0.0f;
    r.param[P_LFO1_SCALE] = 1.0f;
    std::vector<float> st = r.Run(SRC_1, 5.0);
    int changes = 0;
    for (size_t i = 1; i < st.size(); ++i) if (st[i] != st[i - 1]) ++changes;
    snprintf(msg, sizeof msg, "LFO stepped: one new random value per cycle (%d changes in 5 s at 2.04 Hz)", changes);
    Check(changes >= 8 && changes <= 11, msg);
    r.param[P_LFO1_WAVE] = FindOption(P_LFO1_WAVE, "Ramp Down");
    r.param[P_LFO1_SMOOTH] = 1.0f;
    std::vector<float> rd = r.Run(SRC_1, 4.0);
    float jump = 0;
    for (size_t i = kSr; i < rd.size(); ++i) jump = fmaxf(jump, fabsf(rd[i] - rd[i - 1]));
    snprintf(msg, sizeof msg, "LFO Smooth 100 %% removes the ramp's jump (largest step %.3f V)", jump);
    Check(jump < 0.05f, msg);
  }

  // ---- Tides 2 (Tidal Modulator 2)
  {
    Rig r;   // Cycle mode, Medium range (2 Hz at 0 st), Gates output mode, free
    std::vector<float> v = r.Run(SRC_5, 10.0);
    float lo, hi;
    MinMax(v, &lo, &hi);
    int n = Crossings(v, 0.5f * (lo + hi), kSr / 10);
    snprintf(msg, sizeof msg, "Tidal free, Medium range: %d cycles in 10 s (~20), %.2f .. %.2f V", n, lo, hi);
    Check(n >= 18 && n <= 22 && hi - lo > 4.0f, msg);
    Rig s;
    s.param[P_TD_SYNC] = 1 + 5;   // "1/4": one cycle a beat
    std::vector<float> w = s.Run(SRC_5, 10.0);
    MinMax(w, &lo, &hi);
    n = Crossings(w, 0.5f * (lo + hi), kSr / 10);
    snprintf(msg, sizeof msg, "Tidal synced to 1/4 at 120 BPM: %d cycles in 10 s (20)", n);
    Check(n >= 19 && n <= 21, msg);
    s.param[P_TD_RAMP] = 0;   // AD: one envelope per beat, triggered by the tempo
    std::vector<float> ad = s.Run(SRC_5, 10.0);
    MinMax(ad, &lo, &hi);
    n = Crossings(ad, 0.5f * (lo + hi), kSr / 10);
    snprintf(msg, sizeof msg, "Tidal AD triggered by the tempo: %d envelopes in 10 s (20)", n);
    Check(n >= 19 && n <= 21, msg);
  }

  // ---- Marbles (Random Sampler)
  {
    Rig r;   // free: T rate 0 = 2 Hz, Coin Toss; X range +5 V
    std::vector<float> t2 = r.Run(SRC_14, 10.0);   // T2 follows the master clock
    int n = Crossings(t2, 5.0f);
    snprintf(msg, sizeof msg, "Random T2 = the master clock: %d ticks in 10 s (2 Hz -> 20)", n);
    Check(n >= 19 && n <= 21, msg);
    Rig x;
    std::vector<float> x1 = x.Run(SRC_9, 10.0);
    float lo, hi;
    MinMax(x1, &lo, &hi);
    snprintf(msg, sizeof msg, "Random X1 in the +5 V range: %.2f .. %.2f V", lo, hi);
    Check(lo >= -0.01f && hi <= 5.01f && hi - lo > 1.0f, msg);
    Rig t;
    t.param[P_MB_SYNC] = 1 + 6;   // "1/8": the T clock is the tempo, 4 per second
    std::vector<float> ts = t.Run(SRC_14, 10.0);
    n = Crossings(ts, 5.0f, kSr);
    snprintf(msg, sizeof msg, "Random synced to 1/8 at 120 BPM: %d ticks in the last 9 s (36)", n);
    Check(n >= 34 && n <= 38, msg);
    Rig g;
    std::vector<float> t1 = g.Run(SRC_13, 10.0), t3 = g.Run(SRC_15, 10.0);
    int a = Crossings(t1, 5.0f), b = Crossings(t3, 5.0f);
    snprintf(msg, sizeof msg, "Random T1 and T3 toss a coin per tick (T1 %d, T3 %d gates in 10 s)", a, b);
    Check(a > 3 && b > 3 && a + b >= 15 && a + b <= 25, msg);
  }

  // ---- step and gate sequencers
  {
    Rig r;   // Seq 1: 1/16 at 120 BPM = 8 steps a second
    for (int s = 0; s < 16; ++s) r.param[P_SQ1_S1 + s] = -5.0f + s * (10.0f / 15.0f);
    r.param[P_SQ1_LEN] = 4;
    std::vector<float> v = r.Run(SRC_16, 1.0);
    float a = v[static_cast<size_t>(0.06 * kSr)], b = v[static_cast<size_t>(0.19 * kSr)];
    float c = v[static_cast<size_t>(0.44 * kSr)], d = v[static_cast<size_t>(0.56 * kSr)];
    snprintf(msg, sizeof msg, "Seq steps 1, 2, 4 then back to 1 with Length 4 (%.2f, %.2f, %.2f, %.2f V)", a, b, c, d);
    Check(fabsf(a + 5.0f) < 0.01f && fabsf(b - (-5.0f + 10.0f / 15.0f)) < 0.01f &&
          fabsf(c - (-5.0f + 3 * 10.0f / 15.0f)) < 0.01f && fabsf(d + 5.0f) < 0.01f, msg);
    Rig g;
    for (int s = 0; s < 16; ++s) g.param[P_GT1_G1 + s] = (s % 4 == 0) ? 1.0f : 0.0f;   // every beat
    std::vector<float> gv = g.Run(SRC_18, 4.0);
    int n = Crossings(gv, 5.0f);
    snprintf(msg, sizeof msg, "Gate sequencer on every 4th 1/16: %d gates in 4 s at 120 BPM (8)", n);
    Check(n == 8, msg);
    size_t high = 0;
    for (float x : gv) high += x > 5.0f;
    double width = high / (8.0 * 0.125 * kSr);
    snprintf(msg, sizeof msg, "Gate Width 50 %% of a step (%.0f %%)", width * 100.0);
    Check(fabs(width - 0.5) < 0.02, msg);
  }

  printf(failures ? "FAILED (%d)\n" : "PASSED\n", failures);
  return failures ? 1 : 0;
}
