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
    with open(os.path.join(ROOT, "src", "param_ids.h"), "w") as f:
        f.write("\n".join(lines) + "\n")
    print("gen_params: %d parameters" % len(P))


if __name__ == "__main__":
    main()
