**InterzoneXXL — Valley Audio's Interzone synth voice, played from MIDI in mono or poly and patched with its own LFOs, random sources and sequencers, running natively inside MPC OS on the Akai Force.**

Load it as a plugin instrument on a track: a VCO with glide, pitch mod, PWM and a sub wave, a mixer with noise, a resonant 2/4-pole OTA filter with a high-pass, an LFO with seven waves, a looping envelope and a VCA, on pages drawn from the module's own panel, sliders and switches. Every input jack picks a source running inside the plugin: four Bogaudio LFOs, Tidal Modulator 2, Random Sampler, two CV and two gate sequencers on the MPC tempo, an extra oscillator, Interzone's own outputs, velocity, mod wheel and pressure.

> 🧪 **0.1.1 is built and tested offline**: the plugin's output is checked against Valley's own Interzone module code, sample for sample (patched cables included), under ASan + UBSan. **Not yet tested on a device.** Save your projects before installing, and report problems under **Issues**. Full list: [CHANGELOG](https://github.com/sunskiefer/InterzoneXXL/blob/main/CHANGELOG.md).

## Pages
**VCO** · **FILTER / LFO** · **MIXER / ENV** · **VOICE** (voice, limiter, Panic · EXT OSC · PRESETS) · **CV IN** · **LFO** (1-4) · **TIDAL / RANDOM** · **SEQ** (Seq 1-2, Gate 1-2, SEQ SET)

## Requirements
- Akai Force (first generation); other Gen1 MPC OS units should work but are untested
- MPC OS 3.x
- Root SSH access (for example MockbaMod)

## Install
Download **InterzoneXXL-0.1.1-mpc-armv7.zip** below, unzip it, then:

    scp -r InterzoneXXL-0.1.1 root@<device-ip>:/tmp/
    ssh -t root@<device-ip> sh /tmp/InterzoneXXL-0.1.1/install.sh

The installer stops MPC (save first), installs the plugin, and starts MPC again. Then load **InterzoneXXL** (manufacturer ANDREALPHEUS) as a plugin instrument. Presets go in `/sdcard/InterzoneXXL Presets`.

Guide: [docs/USER_GUIDE.md](https://github.com/sunskiefer/InterzoneXXL/blob/main/docs/USER_GUIDE.md)

## Credits
- **Interzone:** Dale Johnson (Valley Audio)
- **LFO:** Matt Demanett (Bogaudio) · **Tidal Modulator 2 and Random Sampler:** Emilie Gillet (Mutable Instruments Tides 2 and Marbles), VCV's port in Audible Instruments
- **Patching, sources, presets, limiter, Panic:** from [PlateauXXL](https://github.com/sunskiefer/PlateauXXL) and [RMXXXL](https://github.com/sunskiefer/RMXXXL)
- **Plugin framework, skin tools and installer:** sd88me ([mpc-vst-plugins](https://github.com/sd88me/mpc-vst-plugins))
- **Artwork:** Valley (ValleyRackFree), VCV Component Library (CC BY-NC 4.0), Bogaudio (CC BY-SA 4.0) · **Interface font:** Titillium Web (SIL Open Font License)

Licensed GPL-3.0-or-later. Free, not for sale.
