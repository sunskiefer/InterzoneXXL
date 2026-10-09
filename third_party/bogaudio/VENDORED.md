# Vendored: Bogaudio DSP library (for the LFO)

- Upstream: https://github.com/bogaudio/BogaudioModules
- Commit: `656eaae458e045602dc974bae82e15a11e104958`
- Licence: source GPL-3.0-or-later (`LICENSE.md`, `gpl-3.0.txt`), Copyright (c) Matt Demanett. `lib/ffft` (FFTReal) is
  under the WTFPL (`LICENSE-dist.md`).

## Files

`src/dsp/` -> `dsp/`: `base.hpp`, `math.hpp/.cpp`, `table.hpp/.cpp`, `oscillator.hpp/.cpp`, `signal.hpp/.cpp`,
`pitch.hpp`, `noise.hpp/.cpp`, `analyzer.hpp/.cpp`, `buffer.hpp`, `filters/filter.hpp/.cpp`. `lib/ffft/` -> `lib/ffft/`
(needed only because `table.cpp` uses `analyzer.hpp`'s Hamming window).

## Local changes

None. The files are byte-identical to the upstream commit.

The LFO module itself (`src/LFO.cpp`, `src/lfo_base.cpp`: Rack glue) is not vendored; its `modulateChannel()` and
`processChannel()` / `updateOutput()` are re-written without Rack in `src/sources.cc` (`Lfo`), for the one output the
Wave parameter picks, with the MPC tempo as an alternative to the frequency knob (Sync).

## Re-vendoring

Copy the files above from a newer commit, update the commit here and run `test/run_tests.sh`.
