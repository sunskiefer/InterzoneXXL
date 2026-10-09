// SPDX-License-Identifier: GPL-3.0-or-later
// TEST ONLY: just enough of VCV Rack's Module API for Valley's own Interzone.cpp (the module part, extracted by
// test/make_ref.py) to run offline as a reference for src/engine.cc. Ports behave as Rack's engine::Port does
// (getPolyVoltageSimd broadcasts a one-channel cable; an unconnected port reads 0 V); the rest is bookkeeping.
#pragma once
#include <string>
#include <vector>

#include "valley_sse_include.h"
#include "rack.hpp"
#include <dsp/digital.hpp>

using namespace rack;

typedef struct json_t json_t;

namespace ref {

const int kPortChannels = 16;

struct Param {
  float value = 0.f;
  float getValue() const { return value; }
  void setValue(float v) { value = v; }
};

struct Port {
  alignas(16) float voltages[kPortChannels] = {};
  int channels = 0;
  int getChannels() const { return channels; }
  void setChannels(int c) {
    for (int i = c; i < kPortChannels; ++i) voltages[i] = 0.f;
    channels = c;
  }
  float getVoltage(int c = 0) const { return voltages[c]; }
  void setVoltage(float v, int c = 0) { voltages[c] = v; }
  float getVoltageSum() const {
    float s = 0.f;
    for (int i = 0; i < channels; ++i) s += voltages[i];
    return s;
  }
  float* getVoltages(int c = 0) { return voltages + c; }
  template <typename T> T getVoltageSimd(int c) const { return T::load(voltages + c); }
  template <typename T> T getPolyVoltageSimd(int c) const {
    return channels == 1 ? T(voltages[0]) : getVoltageSimd<T>(c);
  }
  template <typename T> void setVoltageSimd(T v, int c) { v.store(voltages + c); }
};

struct Light {
  float value = 0.f;
};

struct ProcessArgs {
  float sampleRate = 44100.f;
  float sampleTime = 1.f / 44100.f;
};

struct Engine {
  float getSampleRate() const { return 44100.f; }
};
struct App {
  Engine engineObj;
  Engine* engine = &engineObj;
};
extern App* APP;

struct Module {
  std::vector<Param> params;
  std::vector<Port> inputs, outputs;
  std::vector<Light> lights;
  typedef ref::ProcessArgs ProcessArgs;
  virtual ~Module() {}
  void config(int p, int i, int o, int l) {
    params.resize(p);
    inputs.resize(i);
    outputs.resize(o);
    lights.resize(l);
  }
  void configParam(int id, float lo, float hi, float def, ...) { params[id].value = def; (void)lo; (void)hi; }
  void configSwitch(int id, float lo, float hi, float def, const char*, std::vector<std::string>) {
    params[id].value = def;
    (void)lo;
    (void)hi;
  }
  void configInput(int, const char*) {}
  void configOutput(int, const char*) {}
  virtual void process(const ProcessArgs&) {}
  virtual void onSampleRateChange() {}
  virtual json_t* dataToJson() { return NULL; }
  virtual void dataFromJson(json_t*) {}
};

}  // namespace ref

using ref::APP;
typedef ref::Module Module;
