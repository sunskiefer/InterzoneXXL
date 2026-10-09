# Vendored: Mutable Instruments Tides 2 and Marbles DSP

- Upstream: VCV's fork of Emilie Gillet's eurorack code, https://github.com/VCVRack/pichenettes-eurorack, at the
  commit VCV Audible Instruments pins (`a1cd335`): `9739c0227708fab7a28b4efef7b27a9a1bc098d1`; its `stmlib` submodule
  at `c0c42ec91d315a2ec9c0a3845e593b71ffa429db`. The fork takes the sample rate as a parameter, so the code runs at
  44.1 kHz as it does in VCV Rack.
- Licence: MIT (the STM32 projects; `README-licence.md`, `stmlib/LICENSE`), Copyright (c) Emilie Gillet.

## Files

- `stmlib/`: `stmlib.h`, `dsp/{dsp.h, units.h, units.cc, parameter_interpolator.h, polyblep.h, hysteresis_quantizer.h,
  delay_line.h}`, `utils/{gate_flags.h, random.h, random.cc, ring_buffer.h}`
- `tides2/`: `poly_slope_generator.h/.cc`, `ramp_generator.h`, `ramp_shaper.h`, `ratio.h`, `resources.h/.cc`
- `marbles/`: `random/*`, `ramp/*`, `resources.h/.cc`, `note_filter.h`

## Local changes

One, in InterzoneXXL (marked "InterzoneXXL local change"): `marbles/random/lag_processor.cc` clamps the ramp phase to
0 .. 0.99999 before reading `lut_raised_cosine`. A phase of 1.0 (seen when the framework's host test swept every
parameter with an X output patched in) read past the end of the 257-entry table (ASan: global-buffer-overflow). The
other files are byte-identical to the upstream commits.

The VCV modules' glue (VCV Audible Instruments `src/Tides2.cpp` and `src/Marbles.cpp`, GPL-3.0-or-later, Andrew Belt)
is not vendored; their `process()` code is re-written without Rack in `src/sources.cc` (`Tides`) and
`src/marbles_source.cc`, with the MPC tempo on the CLOCK / T CLOCK inputs. Marbles' six preset scales are copied
from `src/Marbles.cpp` into `src/marbles_scales.inc`, and loaded into the Y channel too (the firmware and VCV load
them into X1-X3 only, which leaves Y reading past its scale table once Steps quantizes it; found under UBSan). The names follow VCV's: "Tidal Modulator 2" and "Random
Sampler". Mutable Instruments' panel artwork is not used (VCV distributes it by permission only); the pages are
drawn here in the panels' colours and layout style.

## Re-vendoring

Copy the files above from a newer commit of the fork, update the commits here and run `test/run_tests.sh`.
