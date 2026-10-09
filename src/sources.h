// SPDX-License-Identifier: GPL-3.0-or-later
// The modulation sources that stand in for the cables a VCV patch would plug into the module's inputs (from PlateauXXL):
// four Bogaudio LFOs, Mutable Instruments' Tides 2 (VCV "Tidal Modulator 2") and Marbles (VCV "Random Sampler"), two CV step
// sequencers and two gate sequencers. Each source renders a block of volts at the audio rate, as its VCV module
// does, from the plugin's parameters; the LFOs, Tidal, Random and the sequencers can follow the MPC tempo.
#pragma once
#include <stdint.h>

#include "param_ids.h"

namespace ixxl {

const int kMaxBlock = 128;

// Where the MPC tempo is for each frame of a block.
struct Clock {
  float bpm;            // beats (quarter notes) per minute
  double beat[kMaxBlock];   // song position in beats at each frame (counted from transport start)
  bool restart;         // the transport (re)started at the first frame: reset phases, as a Reset input would
};

class Sources {
 public:
  Sources();
  ~Sources();
  void Init(float sample_rate);
  // Render `frames` frames of every source that `needed[src]` asks for into out[src][]. param: values in units.
  void Render(const float* param, const Clock& clock, const bool* needed, int frames, float out[][kMaxBlock]);

 private:
  struct Impl;
  Impl* impl_;
  Sources(const Sources&);
  Sources& operator=(const Sources&);
};

}  // namespace ixxl
