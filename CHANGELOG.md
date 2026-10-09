# Changelog

## 0.1.1
- PRESETS: SAVE and LOAD are big buttons with Titillium lettering, like PANIC (they were drawn at the size of the
  framework's bitmap label).
- Docs: the PRESETS, EXT OSC, GATE and SEQ SET pages in the README.

## 0.1.0
- Interzone (Valley Audio) as an MPC OS VST2 instrument: VCO (glide, pitch mod, pulse width and PWM, octave, coarse
  continuous or semitone, fine), mixer (saw, pulse, sub with its octave and wave, white / pink noise, Ext), filter
  (2/4-pole OTA low-pass, resonance, high-pass, envelope with polarity, LFO, V/Oct tracking), LFO (seven waves, rate,
  fine, slew), envelope (ADSR, short / long, gate / cycle, Man. as a latching gate) and VCA (envelope or gate).
- Played from MIDI: Mono (last-note priority, Legato or Retrigger) or Poly (1-16 voices, Interzone's own polyphony),
  pitch bend with a 0-24 semitone range, the sustain pedal, CC 120 / 123 all notes off. Sample-accurate note starts.
- Pages drawn from the module: VCO, FILTER / LFO and MIXER / ENV, each Interzone's own dark panel with Valley's
  sliders and Rogan knobs and VCV's switches at the module's own positions.
- Patching (as PlateauXXL): every input jack of the module picks a source and has an amount on the CV IN page.
  Sources: four Bogaudio LFOs, Tidal Modulator 2, Random Sampler, two CV and two gate sequencers (SEQ, with SEQ SET),
  on the MPC tempo; Interzone's own output jacks (per voice in Poly); MIDI velocity, mod wheel and pressure.
- Ext Osc: an extra oscillator per voice following the played note, Mixer Ext's source by default.
- VOICE tab: VOICE (mode, voices, retrigger, bend, level, RMXXXL's limiter, Panic), EXT OSC, PRESETS (16 slots).
- Every control on a Q-Link.
- Tests: the plugin against Valley's own module code (sample for sample, with patched cables too), the sources,
  voices, pedal, Ext Osc, patching, presets, Panic, state, stability.
