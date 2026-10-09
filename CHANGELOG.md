# Changelog

## 0.1.0 (unreleased: waiting for the Force test)
Milestone 1: the voice.
- Interzone (Valley Audio) as an MPC OS VST2 instrument: VCO (glide, pitch mod, pulse width and PWM, octave, coarse
  continuous or semitone, fine), mixer (saw, pulse, sub with its octave and wave, white / pink noise, Ext), filter
  (2/4-pole OTA low-pass, resonance, high-pass, envelope with polarity, LFO, V/Oct tracking), LFO (seven waves, rate,
  fine, slew), envelope (ADSR, short / long, gate / cycle, Man. as a latching gate) and VCA (envelope or gate).
- Played from MIDI: Mono (last-note priority, Legato or Retrigger) or Poly (1-16 voices, Interzone's own polyphony),
  pitch bend with a 0-24 semitone range, the sustain pedal, CC 120 / 123 all notes off. Sample-accurate note starts.
- Pages drawn from the module: VCO, FILTER / LFO and MIXER / ENV, each Interzone's own dark panel with Valley's
  sliders and Rogan knobs and VCV's switches at the module's own positions. VOICE page: mode, voices, retrigger, bend,
  Level, RMXXXL's brickwall limiter and Panic.
- Every control on a Q-Link, the panel read left to right.
- The module's CV inputs read 0 V for now (patching comes next; see ROADMAP.md).
- Tests: the plugin against Valley's own module code (sample for sample), voices, pedal, Panic, state, stability.
