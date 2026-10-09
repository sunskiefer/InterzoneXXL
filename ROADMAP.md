# Roadmap

One milestone at a time, each tested on the Force before the next.

## Milestone 1: the voice (0.1.0)
Interzone played from MIDI in Mono and Poly, its three panel pages with the module's sliders, knobs and switches,
the VOICE page. **To check on the Force:** it loads as an instrument, plays from pads and sequences, Mono legato /
retrigger and Poly behave, the sliders, knobs and switches look and move right (touch and Q-Links), values show,
projects save and reload, CPU (`tools/device_bench.sh`) for choosing the default voice count.

## Milestone 2: patching (as PlateauXXL)
- A CV IN page: every input jack of the module (VOct 2, PWM, Mixer Ext, Filter Freq 1 and 2 with their blue
  attenuverters, Res, VCA Level with its attenuverter, LFO Rate, Trig and Reset, Env Gate and Trig) picks a source
  and has an amount.
- The shared source pool from PlateauXXL: four Bogaudio LFOs, Tidal Modulator 2, Random Sampler, two CV step and
  two gate sequencers, on the MPC tempo, each page drawn like its module.
- Interzone's own outputs as sources (LFO waves, S+H, noise, envelope + and -), per voice in Poly.
- Mixer Ext / Noise Ext: a simple extra oscillator (noise and sub waves, following the played note) or any source.

## Milestone 3
- 16 user presets (PlateauXXL's), a catalog listing (mpc-vst-plugins), an Instruments-browser tile.
- MIDI velocity and mod wheel as sources.
