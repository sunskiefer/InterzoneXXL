# Vendored: Valley Audio's Interzone DSP

- Upstream: https://github.com/ValleyAudio/ValleyRackFree
- Commit: `86f02e431136a7f5c96a872b99b7115b7e133e05` ("Bump version number"), the same commit PlateauXXL vendors from
- Licence: GPL-3.0-or-later (`LICENSE.md`, `LICENSE-GPLv3.txt`), Copyright (c) Dale Johnson / Valley Audio Soft.
  Some files also carry a BSD-style header from Dale Johnson; they are kept as they are.

## Files (paths as upstream, under `src/`)

| File | What it is |
| --- | --- |
| `Interzone/Interzone.hpp`, `Interzone/Interzone.cpp` | The VCV module. **Not compiled into the plugin**: `src/engine.cc` re-writes its process() without Rack, and `test/make_ref.py` builds its module code, unchanged, as the reference the tests compare the plugin with |
| `dsp/generators/VecDirectOsc.hpp` | Saw, pulse and the sub wave, four voices per SIMD vector |
| `dsp/filters/VecOTAFilter.hpp/.cpp` | The 2/4-pole OTA low-pass filter |
| `dsp/filters/VecOnePoleFilters.hpp/.cpp` | Glide, gate slew, the high-pass filter, the oscillators' DC blockers |
| `dsp/filters/OnePoleFilters.hpp/.cpp` | The LFO slew |
| `dsp/filters/OTAFilter.hpp/.cpp` | Included by VecOTAFilter.hpp; its `calcGTable()` is only linked into the test (the module calls it, the vector filter does not read its table) |
| `dsp/modulation/VecLoopingADSR.hpp/.cpp` | The envelope (VCV Fundamental's ADSR with Interzone's loop and time scale) |
| `dsp/modulation/DLFO.hpp`, `dsp/generators/Noise.hpp` | The LFO (seven waves) and the white / pink noise |
| `dsp/shaping/VecNonLinear.hpp`, `dsp/shaping/NonLinear.hpp` | The filter stages' saturation |
| `simd/SIMDUtilities.hpp`, `valley_sse_include.h` | SSE helpers; on ARM the SSE calls go through SIMDe (`third_party/simde`) |
| `utilities/Utilities.hpp/.cpp` | `semitone()` and small helpers |

## Local changes

One, in `dsp/filters/VecOTAFilter.hpp/.cpp`, marked "InterzoneXXL local change":
- Each filter computed two 1.1-million-entry tables (4.4 MB each) of the cutoff coefficients for the sample rate, so an
  Interzone held 35 MB of identical tables and computed them four times at load. The tables now live once per
  process, computed on first use under a mutex and shared by every filter. Same formula, same values (the tests
  compare the plugin with the module sample for sample; see below). The shared table has one entry more than
  upstream's, since `setCutoff()` reads one past its position at the top of the range.

Every other file is byte-identical to the upstream commit.

## How the port is checked

`test/run_tests.sh` builds Interzone's own module code (`test/make_ref.py`: `struct Interzone` and its constructor,
process(), getParams(), getCV() and tickSynth(), copied unchanged from the vendored files) on a small stand-in for
VCV's Module API (`test/mock_rack.hpp`), feeds it what VCV's MIDI-CV module would send for the same notes, and
requires the plugin's output to match the module's VCA output within 2e-3 relative RMS (it matches to about 1e-4:
16-bit output, and the noise sources are seeded differently). Mono, legato with glide, retrigger, a full patch
(pitch mod, PWM, sub, filter env/LFO/key tracking, cycling envelope), semitone coarse, the gate VCA, a 4-voice and a
16-voice chord are compared.

## Re-vendoring

Copy the files above from a newer upstream commit, re-apply the local change, update the commit here and run
`test/run_tests.sh`: the comparison with the module tells whether the port still matches.
