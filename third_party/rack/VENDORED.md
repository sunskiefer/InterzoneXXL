# Vendored: VCV Rack headers

- Upstream: https://github.com/VCVRack/Rack, tag `v2.6.6` (commit `061ccf63c1758599396ac1bb10d47345d9d34076`)
- Licence: GPL-3.0-or-later (`LICENSE.md`, `LICENSE-GPLv3.txt`), Copyright (c) VCV.

Valley's envelope (`VecLoopingADSR`) is written on Rack's SIMD vector type, and Interzone's process() uses Rack's
`dsp::approxExp2_taylor5()` and `dsp::FREQ_C4`. These headers provide them, unchanged:

| File (under `include/`) | What it is |
| --- | --- |
| `simd/Vector.hpp`, `simd/functions.hpp`, `simd/common.hpp`, `simd/sse_mathfun.h`, `simd/sse_mathfun_extension.h` | `rack::simd::float_4` and its math |
| `math.hpp`, `common.hpp`, `arch.hpp`, `logger.hpp` | What those include (header declarations; nothing here is linked) |
| `dsp/common.hpp`, `dsp/approx.hpp` | `FREQ_C4`, `approxExp2_taylor5` |
| `dsp/digital.hpp` | `ClockDivider`: used only by the test's reference module (`test/mock_rack.hpp`) |

`src/shim/rack.hpp` (ours) stands in for Rack's `rack.hpp`: it includes the headers above and nothing of Rack's app,
engine or GUI. On ARM, `simd/common.hpp` takes SSE from SIMDe (`third_party/simde`), as Rack does on ARM.

No local changes.
