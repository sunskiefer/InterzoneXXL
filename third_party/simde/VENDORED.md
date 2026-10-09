# Vendored: SIMDe (SIMD Everywhere)

- Upstream: https://github.com/simd-everywhere/simde, commit `dd0b662fd8cf4b1617dbbb4d08aa053e512b08e4`, the commit
  VCV Rack v2.6.6 pins as its `dep/simde` submodule
- Licence: MIT (`COPYING`)

Valley's DSP is written with SSE intrinsics. On the MPC's ARM CPU, `valley_sse_include.h` and Rack's
`simd/common.hpp` include SIMDe's `x86/sse2.h` / `x86/sse4.2.h` with `SIMDE_ENABLE_NATIVE_ALIASES`, which turns the
SSE calls into NEON. Only the headers those two include are vendored (`simde/*.h` common headers, `simde/x86/mmx.h`,
`sse.h`, `sse2.h`, `sse3.h`, `ssse3.h`, `sse4.1.h`, `sse4.2.h`), unchanged.
