// SPDX-License-Identifier: GPL-3.0-or-later
// InterzoneXXL: the part of VCV Rack's rack.hpp that Valley's Interzone DSP uses (VecLoopingADSR.hpp includes
// "rack.hpp" for rack::simd::float_4 and rack::math::clamp). It includes the vendored, unchanged Rack headers
// (third_party/rack/include) and exposes rack::math inside rack as rack.hpp does; the rest of the Rack API (app,
// engine, widgets, logger) is not needed and not linked.
#pragma once

#include <common.hpp>
#include <math.hpp>
#include <simd/Vector.hpp>
#include <simd/functions.hpp>
#include <dsp/common.hpp>
#include <dsp/approx.hpp>

namespace rack {
using namespace math;
}  // namespace rack
