// SPDX-License-Identifier: GPL-3.0-or-later
// TEST ONLY: RMXXXL's limiter as it is there (a scan of the window), to check src/limiter.h against. Brickwall limiter (https://github.com/sunskiefer/RMXXXL, src/dsp.h, by the same author).
// Stereo-linked look-ahead peak limiter. Drive pushes the signal in, Ceiling is the absolute output maximum.
// The gain needed for each incoming sample is known kLookahead samples before that sample is played; a sliding
// minimum over that window, smoothed to reach its value within the window, pulls the gain down before the peak
// arrives (no distortion on transients), and a final clamp at the ceiling makes it a true brickwall even if the
// smoothing lags. Adds kLookahead samples (1.45 ms) of latency.
#pragma once
#define PLATEAU_LIMITER_REF
#include <math.h>
#include <string.h>

namespace ixxl {

struct LimiterRef {
  static const int kLookahead = 64;
  float delay[kLookahead][2];
  float need[kLookahead];        // gain each delayed sample needs
  int pos;
  float gain;
  float gr_db;                   // last block's deepest gain reduction

  void Init() {
    memset(this, 0, sizeof *this);
    gain = 1.0f;
    for (int i = 0; i < kLookahead; ++i) need[i] = 1.0f;
  }

  static float DbToGain(float db) { return powf(10.0f, db / 20.0f); }
  static float Clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }

  void Process(float* l, float* r, int n, float drive_db, float ceiling_db, float release_ms, float sample_rate) {
    float drive = DbToGain(drive_db), ceiling = DbToGain(ceiling_db);
    float attack = 1.0f - expf(-1.0f / (kLookahead / 5.0f));            // ~99 % within the look-ahead
    float release = 1.0f - expf(-1.0f / (Clampf(release_ms, 1.0f, 2000.0f) * 0.001f * sample_rate));
    float deepest = 1.0f;
    for (int i = 0; i < n; ++i) {
      float a = l[i] * drive, b = r[i] * drive;
      float peak = fmaxf(fabsf(a), fabsf(b));
      need[pos] = peak > ceiling ? ceiling / peak : 1.0f;
      float out_l = delay[pos][0], out_r = delay[pos][1];
      delay[pos][0] = a; delay[pos][1] = b;
      pos = (pos + 1) % kLookahead;
      float target = 1.0f;
      for (int k = 0; k < kLookahead; ++k) target = fminf(target, need[k]);
      gain += (target - gain) * (target < gain ? attack : release);
      l[i] = Clampf(out_l * gain, -ceiling, ceiling);    // the brickwall
      r[i] = Clampf(out_r * gain, -ceiling, ceiling);
      if (gain < deepest) deepest = gain;
    }
    gr_db = 20.0f * log10f(fmaxf(deepest, 1e-6f));
  }
};

}  // namespace ixxl
