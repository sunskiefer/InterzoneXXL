#!/usr/bin/env python3
"""The single source of InterzoneXXL's parameter list: writes params.json (for mpc-vst-plugins' gen_vst.py) and
src/param_ids.h (P_<KEY> index constants).

    python3 tools/gen_params.py

MPC stores automation and saved values by parameter INDEX, so this list is append-only: never reorder, rename a key or
remove a parameter that has shipped, and never change the number of options of a shipped option list.

Interzone's own controls come first, with the module's ranges, defaults and option order (Interzone.cpp, the
configParam/configSwitch calls), so a value means here what it means on VCV Rack. Option lists drawn as Rack switches
(CKSS, CKSSThree) keep Rack's order: option 0 is the switch's bottom position.
"""
import json
import os

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
OFF_ON = ["Off", "On"]
# Modulation sources a module input can pick (index = the "<input>_src" option), in volts as on VCV Rack.
# 1-19: PlateauXXL's built-in modules (sources.h); 20 on: computed by the engine, per voice where marked (*).
SOURCES = ["Off", "LFO 1", "LFO 2", "LFO 3", "LFO 4", "Tidal 1", "Tidal 2", "Tidal 3", "Tidal 4",
           "X1", "X2", "X3", "Y", "T1", "T2", "T3", "Seq 1", "Seq 2", "Gate 1", "Gate 2",
           "Ext Osc",                                                      # * the extra oscillator, +-5 V
           "IZ Sine", "IZ Tri", "IZ Saw Up", "IZ Saw Dn", "IZ Pulse", "IZ S+H", "IZ Noise",   # Interzone's LFO jacks
           "Env +", "Env -", "VCO Saw", "VCO Pulse", "VCO Sub",           # * Interzone's env and VCO jacks
           "Velocity", "Mod Wheel", "Pressure"]                            # MIDI, 0-10 V (* velocity)
NUM_MODULE_SOURCES = 20
# Tempo divisions: name, length in beats (quarter notes).
DIVISIONS = [("8 Bars", 32), ("4 Bars", 16), ("2 Bars", 8), ("1 Bar", 4), ("1/2", 2), ("1/4", 1), ("1/8", 0.5),
             ("1/16", 0.25), ("1/32", 0.125), ("1/2 T", 4 / 3.0), ("1/4 T", 2 / 3.0), ("1/8 T", 1 / 3.0),
             ("1/16 T", 1 / 6.0), ("1/2 .", 3), ("1/4 .", 1.5), ("1/8 .", 0.75)]
SYNC = ["Free"] + [d[0] for d in DIVISIONS]          # LFO, Tidal, Random: free running or locked to the MPC tempo
STEP_DIVS = [d[0] for d in DIVISIONS]                 # sequencers: always on the tempo
WAVES = ["Sine", "Triangle", "Ramp Up", "Ramp Down", "Square", "Stepped"]
POLARITY = ["Negative", "Positive"]


def knob(key, name, lo, hi, default, **kw):
    p = {"key": key, "name": name, "min": lo, "max": hi, "default": default, "dynamic_display": True}
    p.update(kw)
    return p


def opts(key, name, options, default=0, **kw):
    p = {"key": key, "name": name, "options": options, "default": default}
    p.update(kw)
    return p


def trigger(key, name):
    return {"key": key, "name": name, "min": 0.0, "max": 1.0, "momentary": True, "type": "trigger", "default": 0.0}


P = []
# ---- 0.1.0: Interzone (indices 0-42) -------------------------------------------------------------------------------
# VCO
P += [
    knob("glide", "Glide", 0.0, 1.0, 0.0),
    knob("vco_mod", "Pitch Mod", 0.0, 1.0, 0.0),
    opts("vco_mod_pol", "Pitch Mod Env Polarity", POLARITY, 1),
    opts("vco_mod_src", "Pitch Mod Source", ["LFO", "Envelope"], 0),
    opts("coarse_mode", "Coarse Mode", ["Continuous", "Semitone"], 0),
    opts("octave", "Octave", ["32'", "16'", "8'", "4'", "2'"], 2),
    knob("coarse", "Coarse", 0.0, 2.0, 1.0),
    knob("fine", "Fine", -0.0833333, 0.0833333, 0.0),
    # Width: upstream's PW_PARAM runs from 0.5 (slider down, a square) to 0 (slider up, a narrow pulse); here the
    # parameter is the slider's position, 0 at the bottom, and pulse width = 0.5 - 0.5 x position.
    knob("width", "Pulse Width", 0.0, 1.0, 0.0),
    knob("pwm", "PWM Depth", 0.0, 0.5, 0.0),
    opts("pwm_pol", "PWM Env Polarity", POLARITY, 0),
    opts("pwm_src", "PWM Source", ["External", "LFO", "Envelope"], 1),
    opts("sub_oct", "Sub Octave", ["-2 oct", "-1 oct", "-4th", "0", "+5th", "+1 oct", "+2 oct"], 3),
]
# Mixer
P += [
    knob("saw", "Saw Level", 0.0, 1.0, 0.8), knob("pulse", "Pulse Level", 0.0, 1.0, 0.0),
    knob("sub", "Sub Level", 0.0, 1.0, 0.0), knob("noise", "Noise Level", 0.0, 1.0, 0.0),
    knob("ext", "Ext Level", 0.0, 1.0, 0.0),
    opts("sub_wave", "Sub Wave", ["Pulse", "Square", "Saw"], 1),
    opts("noise_type", "Noise Type", ["White", "Pink"], 0),
]
# Filter
P += [
    knob("cutoff", "Filter Freq", 0.0, 10.0, 10.0), knob("res", "Filter Res", 0.0, 10.0, 0.0),
    knob("hpf", "HPF Freq", 0.0, 10.0, 0.0),
    opts("poles", "Filter Poles", ["2 Pole", "4 Pole"], 1),
    knob("flt_env", "Filter Env", 0.0, 1.0, 0.0), knob("flt_lfo", "Filter LFO", 0.0, 1.0, 0.0),
    knob("flt_voct", "Filter V/Oct", 0.0, 1.0, 0.0),
    opts("flt_env_pol", "Filter Env Polarity", POLARITY, 1),
    knob("flt_cv1", "Filter CV 1", -1.0, 1.0, 0.0), knob("flt_cv2", "Filter CV 2", -1.0, 1.0, 0.0),
]
# LFO
P += [
    knob("lfo_rate", "LFO Rate", 0.0, 11.0, 0.0), knob("lfo_fine", "LFO Fine", -0.5, 0.5, 0.0),
    knob("lfo_slew", "LFO Slew", 0.0, 1.0, 0.0),
    opts("lfo_wave", "LFO Wave", ["Sine", "Triangle", "Saw Up", "Saw Down", "Square", "S+H", "Noise"], 0),
]
# Envelope and VCA. Man. is a held button on the module; here it latches (a tap holds the gate, a tap releases it).
P += [
    knob("attack", "Attack", 0.0, 1.0, 0.0), knob("decay", "Decay", 0.0, 1.0, 0.0),
    knob("sustain", "Sustain", 0.0, 1.0, 1.0), knob("release", "Release", 0.0, 1.0, 0.0),
    opts("env_length", "Env Length", ["Short", "Long"], 0),
    opts("env_cycle", "Env Cycle", ["Gate", "Cycle"], 0),
    opts("env_manual", "Manual Gate", OFF_ON, 0),
    opts("vca_src", "VCA Source", ["Envelope", "Gate"], 0),
    knob("vca_cv", "VCA Level CV", -1.0, 1.0, 0.0),
]
assert len(P) == 43

# ---- 0.1.0: the voice (MIDI), output level, limiter, panic ----------------------------------------------------------
P += [
    opts("voice_mode", "Voice Mode", ["Mono", "Poly"], 0),
    knob("voices", "Poly Voices", 1, 16, 8, display="int"),
    opts("mono_trig", "Mono Retrigger", ["Legato", "Retrigger"], 0),
    knob("bend", "Bend Range", 0, 24, 2, display="int"),
    knob("level", "Level", 0.0, 1.0, 0.5),
    knob("lim_drive", "Limiter Drive", 0.0, 18.0, 0.0, unit="dB"),
    knob("lim_ceiling", "Ceiling", -12.0, 0.0, -0.3, unit="dB"),
    knob("lim_release", "Limiter Release", 10.0, 500.0, 80.0, unit="ms"),
    trigger("panic", "Panic"),
]

# ---- the module's input jacks: a source each, and an amount ---------------------------------------------------------
# (key, label, amount): amount is the module's own attenuverter (flt_cv1, flt_cv2, vca_cv), None for a gate input, or
# "new" for a jack the module has no knob for (then an amount knob here, 100 % = the cable plugged straight in).
CV_INPUTS = [("voct2", "VOct 2", "new"), ("pwm_in", "PWM", "new"), ("ext_in", "Mixer Ext", "new"),
             ("cut1", "Freq 1", "flt_cv1"), ("cut2", "Freq 2", "flt_cv2"), ("res_in", "Res", "new"),
             ("vca_in", "VCA Level", "vca_cv"), ("lrate", "LFO Rate", "new"), ("ltrig", "LFO Trig", None),
             ("lreset", "LFO Reset", None), ("egate", "Env Gate", None), ("etrig", "Env Trig", None)]
DEFAULT_SRC = {"ext_in": "Ext Osc"}
for key, label, amount in CV_INPUTS:
    P.append(opts(key + "_src", label + " Src", SOURCES, SOURCES.index(DEFAULT_SRC.get(key, "Off"))))
    if amount == "new":
        P.append(knob(key + "_cv", label + " Amount", -1.0, 1.0, 1.0))

# LFO 1-4: Bogaudio LFO (one output, picked by Wave), plus tempo sync
for n in range(1, 5):
    k = "lfo%d_" % n
    P += [opts(k + "wave", "LFO%d Wave" % n, WAVES), opts(k + "sync", "LFO%d Sync" % n, SYNC, SYNC.index("1 Bar")),
          knob(k + "freq", "LFO%d Freq" % n, -5.0, 8.0, 0.0), opts(k + "slow", "LFO%d Slow" % n, OFF_ON),
          knob(k + "sample", "LFO%d Sample" % n, 0.0, 1.0, 0.0), knob(k + "pw", "LFO%d PW" % n, -1.0, 1.0, 0.0),
          knob(k + "smooth", "LFO%d Smooth" % n, 0.0, 1.0, 0.0), knob(k + "offset", "LFO%d Offset" % n, -1.0, 1.0, 0.0),
          knob(k + "scale", "LFO%d Scale" % n, 0.0, 1.0, 1.0)]

# Tidal Modulator 2 (Mutable Instruments Tides 2)
P += [knob("td_freq", "Tidal Freq", -48.0, 48.0, 0.0), knob("td_shape", "Tidal Shape", 0.0, 1.0, 0.5),
      knob("td_slope", "Tidal Slope", 0.0, 1.0, 0.5), knob("td_smooth", "Tidal Smooth", 0.0, 1.0, 0.5),
      knob("td_shift", "Tidal Shift", 0.0, 1.0, 1.0),   # module: 0.5, which mutes output 1 in Gates mode
      opts("td_range", "Tidal Range", ["Low", "Medium", "High"], 1),
      opts("td_output", "Tidal Output", ["Gates", "Amplitude", "Slope/Phase", "Frequency"], 0),
      opts("td_ramp", "Tidal Ramp", ["AD", "Cycle", "AR"], 1),
      opts("td_sync", "Tidal Sync", SYNC, 0)]

# Random Sampler (Mutable Instruments Marbles)
P += [knob("mb_t_rate", "T Rate", -1.0, 1.0, 0.0), knob("mb_t_bias", "T Bias", 0.0, 1.0, 0.5),
      knob("mb_t_jitter", "T Jitter", 0.0, 1.0, 0.0),
      opts("mb_t_mode", "T Mode", ["Coin Toss", "Clusters", "Drums"], 0),
      opts("mb_t_range", "T Range", ["x1/4", "x1", "x4"], 1),
      knob("mb_deja_vu", "Deja Vu", 0.0, 1.0, 0.5), knob("mb_length", "Length", 0.0, 1.0, 0.0),
      opts("mb_t_dv", "T Deja Vu", OFF_ON), opts("mb_x_dv", "X Deja Vu", OFF_ON),
      knob("mb_x_spread", "X Spread", 0.0, 1.0, 0.5), knob("mb_x_bias", "X Bias", 0.0, 1.0, 0.5),
      knob("mb_x_steps", "X Steps", 0.0, 1.0, 0.5),
      opts("mb_x_mode", "X Mode", ["Identical", "Bump", "Tilt"], 0),
      opts("mb_x_range", "X Range", ["+2 V", "+5 V", "+/-5 V"], 1),
      opts("mb_x_scale", "X Scale", ["Major", "Minor", "Pentatonic", "Pelog", "Bhairav", "Shri"], 0),
      opts("mb_y_div", "Y Divider", ["1/64", "1/48", "1/32", "1/24", "1/16", "1/12", "1/8", "1/6", "1/4", "1/3", "1/2",
                                     "1/1"], 8),
      opts("mb_sync", "Random Sync", SYNC, 0)]

# Step sequencers 1-2 (CV, +/-5 V) and gate sequencers 1-2
for n in range(1, 3):
    k = "sq%d_" % n
    P += [knob(k + "s%d" % s, "Seq%d Step %d" % (n, s), -5.0, 5.0, 0.0) for s in range(1, 17)]
    P += [knob(k + "len", "Seq%d Length" % n, 1, 16, 16, display="int"),
          opts(k + "div", "Seq%d Rate" % n, STEP_DIVS, STEP_DIVS.index("1/16")),
          knob(k + "slew", "Seq%d Slew" % n, 0.0, 1.0, 0.0)]
for n in range(1, 3):
    k = "gt%d_" % n
    P += [opts(k + "g%d" % s, "Gate%d Step %d" % (n, s), OFF_ON) for s in range(1, 17)]
    P += [knob(k + "len", "Gate%d Length" % n, 1, 16, 16, display="int"),
          opts(k + "div", "Gate%d Rate" % n, STEP_DIVS, STEP_DIVS.index("1/16")),
          knob(k + "width", "Gate%d Width" % n, 0.05, 1.0, 0.5)]



# Ext Osc: a simple oscillator per voice that follows the played note (with glide), for Mixer Ext or any input
P += [opts("xo_wave", "Ext Osc Wave", ["Saw", "Square", "Triangle", "Sine", "Noise"], 1),
      opts("xo_oct", "Ext Osc Octave", ["-3", "-2", "-1", "0", "+1", "+2"], 2),
      knob("xo_tune", "Ext Osc Tune", -12.0, 12.0, 0.0),
      knob("xo_pw", "Ext Osc Width", 0.05, 0.95, 0.5)]

# User presets (PlateauXXL's): 16 slots, files outside the plugin folder
P += [opts("preset_slot", "Preset", [str(i) for i in range(1, 17)], 0, qlink_ticks=3),
      trigger("preset_save", "Save Preset"), trigger("preset_load", "Load Preset"),
      {"key": "preset_info", "name": "Preset Status", "min": 0.0, "max": 1.0, "default": 0.0, "dynamic_display": True}]

# Q-Link feel on the Force (RMXXXL 1.2.1, PlateauXXL): option lists of more than two step once per 3 nudges (and the
# engine holds one-step moves to one per 0.2 s); on/off switches take every nudge and the engine flips them once per
# turn.
for _p in P:
    if len(_p.get("options") or []) > 2:
        _p.setdefault("qlink_ticks", 3)


def c_ident(key):
    return "P_" + key.upper()


def main():
    keys = [p["key"] for p in P]
    assert len(keys) == len(set(keys)), "duplicate key"
    with open(os.path.join(ROOT, "params.json"), "w") as f:
        json.dump({"name": "InterzoneXXL", "params": P}, f, indent=1)
        f.write("\n")
    lines = ["// generated by tools/gen_params.py: do not edit", "#pragma once", "", "enum ParamId {"]
    lines += ["  %s = %d," % (c_ident(k), i) for i, k in enumerate(keys)]
    lines += ["  P_COUNT = %d" % len(keys), "};", ""]
    lines.append("static const int kNumSources = %d;" % len(SOURCES))
    lines.append("static const int kNumModuleSources = %d;   // sources.h renders 1 .. this - 1" % NUM_MODULE_SOURCES)
    lines.append("enum SourceId { %s };" % ", ".join("SRC_%d" % i for i in range(len(SOURCES))))
    lines.append("static const float kDivisionBeats[%d] = { %s };" % (
        len(DIVISIONS), ", ".join(repr(float(d[1])) + "f" for d in DIVISIONS)))
    lines.append("static const int kNumCvInputs = %d;" % len(CV_INPUTS))
    lines.append("enum CvInputId { %s };" % ", ".join("IN_" + k.upper() for k, _, _ in CV_INPUTS))
    lines.append("// per input: its source and amount (-1: none; the module's own knob for Freq 1/2 and VCA Level)")
    lines.append("static const int kCvInput[%d][2] = {" % len(CV_INPUTS))
    for key, _, amount in CV_INPUTS:
        a = c_ident(key + "_cv") if amount == "new" else (c_ident(amount) if amount else "-1")
        lines.append("  { %s, %s }," % (c_ident(key + "_src"), a))
    lines.append("};")
    lines.append("static const bool kCvInputScaledByModule[%d] = { %s };" % (
        len(CV_INPUTS), ", ".join("true" if a not in (None, "new") else "false" for _, _, a in CV_INPUTS)))
    with open(os.path.join(ROOT, "src", "param_ids.h"), "w") as f:
        f.write("\n".join(lines) + "\n")
    print("gen_params: %d parameters" % len(P))


if __name__ == "__main__":
    main()
