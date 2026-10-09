// SPDX-License-Identifier: GPL-3.0-or-later
// Marbles (VCV "Random Sampler") source: outputs X1, X2, X3, Y, T1, T2, T3 in volts (see marbles_source.cc).
#pragma once
#include "sources.h"

namespace ixxl {

class MarblesSource {
 public:
  MarblesSource();
  ~MarblesSource();
  void Init(float sample_rate);
  void Render(const float* param, const Clock& clock, int frames, float* out[7]);

 private:
  struct Impl;
  Impl* impl_;
  MarblesSource(const MarblesSource&);
  MarblesSource& operator=(const MarblesSource&);
};

}  // namespace ixxl
