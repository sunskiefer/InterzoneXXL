**InterzoneXXL — Valley Audio's Interzone synth voice, played from MIDI in mono or poly, running natively inside MPC OS on the Akai Force.**

Load it as a plugin instrument on a track: a VCO with glide, pitch mod, PWM and a sub wave, a mixer with noise, a resonant 2/4-pole OTA filter with a high-pass, an LFO with seven waves, a looping envelope and a VCA, on pages drawn from the module's own panel, sliders and switches.

> 🧪 **0.1.0 is milestone 1 (the voice), built and tested offline**: the plugin's output is checked against Valley's own Interzone module code, sample for sample, under ASan + UBSan. **Not yet tested on a device.** Save your projects before installing, and report problems under **Issues**. Full list: [CHANGELOG](https://github.com/sunskiefer/InterzoneXXL/blob/main/CHANGELOG.md).

## Pages
**VCO** · **FILTER / LFO** · **MIXER / ENV** · **VOICE** (Mono / Poly 1-16, retrigger, bend range, level, limiter, Panic)

## Requirements
- Akai Force (first generation); other Gen1 MPC OS units should work but are untested
- MPC OS 3.x
- Root SSH access (for example MockbaMod)

## Install
Download **InterzoneXXL-0.1.0-mpc-armv7.zip** below, unzip it, then:

    scp -r InterzoneXXL-0.1.0 root@<device-ip>:/tmp/
    ssh -t root@<device-ip> sh /tmp/InterzoneXXL-0.1.0/install.sh

The installer stops MPC (save first), installs the plugin, and starts MPC again. Then load **InterzoneXXL** (manufacturer ANDREALPHEUS) as a plugin instrument.

Guide: [docs/USER_GUIDE.md](https://github.com/sunskiefer/InterzoneXXL/blob/main/docs/USER_GUIDE.md)

## Credits
- **Interzone:** Dale Johnson (Valley Audio)
- **Brickwall limiter, Panic, Q-Link handling:** from [RMXXXL](https://github.com/sunskiefer/RMXXXL) and [PlateauXXL](https://github.com/sunskiefer/PlateauXXL)
- **Plugin framework, skin tools and installer:** sd88me ([mpc-vst-plugins](https://github.com/sd88me/mpc-vst-plugins))
- **Artwork:** Valley (ValleyRackFree), VCV Component Library (CC BY-NC 4.0) · **Interface font:** Titillium Web (SIL Open Font License)

Licensed GPL-3.0-or-later. Free, not for sale.
