# InterzoneXXL v0.1.0

**Valley Audio's Interzone synth voice, played from MIDI in mono or poly, running natively inside MPC OS on the
Akai Force.**

InterzoneXXL is a VST2 instrument for MPC OS's built-in plugin host, made by L'Cronx (shown on the device as
**InterzoneXXL** by **ANDREALPHEUS**). It is a port of **Interzone**, Dale Johnson's classic monosynth voice for VCV
Rack: a VCO with glide, pitch modulation, pulse-width modulation and a sub wave, a mixer with noise, a resonant 2/4-pole
OTA filter with a high-pass, an LFO with seven waves, a looping envelope and a VCA. The pages are the module's panel,
drawn from Valley's own artwork, sliders where the module has sliders, knobs where it has knobs, switches where it has
switches.

> [!NOTE]
> **Status: 0.1.0, milestone 1 (the voice).** Built and tested offline: the plugin's output is compared, sample for
> sample, with Valley's own Interzone module code fed the same notes (x86, ASan + UBSan). **Not yet tested on a
> device.** The module's CV inputs get built-in sources (patching, gate/CV sequencers, as in
> [PlateauXXL](https://github.com/sunskiefer/PlateauXXL)) in the next milestones: see the [Roadmap](ROADMAP.md).

| | |
| --- | --- |
| ![VCO](docs/img/vco.png) | ![FILTER / LFO](docs/img/filter-lfo.png) |
| ![MIXER / ENV](docs/img/mixer-env.png) | ![VOICE](docs/img/voice.png) |

*The pages, rendered offline from the skin (on the device MPC fills in the values above the sliders).*

## Highlights

- **Interzone, the voice:** Valley's own DSP (VecDirectOsc, VecOTAFilter, VecLoopingADSR, DLFO), and the module's
  process() re-written for MPC OS line by line. The tests check the result against the module itself.
- **Mono or Poly:** Mono is one voice, last-note priority, Legato (glide between held notes, no retrigger) or
  Retrigger. Poly is Interzone's own polyphony (its DSP runs four voices per SIMD group) with 1 to 16 voices, the
  oldest note stolen when they are all busy. Pitch bend (0-24 semitones) and the sustain pedal work in both.
- **The module's panel:** VCO, FILTER / LFO and MIXER / ENV pages, each a piece of Interzone's dark panel with
  Valley's sliders, Rogan knobs and VCV's switches; values above the sliders.
- **VOICE page:** voice mode, poly voices, mono retrigger, bend range, output level, and RMXXXL's brickwall limiter
  (Drive, Ceiling, Release). **Panic:** one tap back to the defaults, every note off.
- **Q-Links:** every control on a Q-Link, the panel read left to right (bank 1 = Q-Links 1-8, bank 2 = 9-16).
- **Sample-accurate notes:** sequenced notes start at their exact position in the audio block.

## Requirements

- An **Akai Force** (first generation). Other first-generation MPC OS units (MPC Live / Live II, One, X,
  Key 61) should work but are untested.
- **Root SSH access** to the device, for example through MockbaMod. Stock MPC OS can't install third-party
  plugins.
- **MPC OS 3.x.** The skin uses the 3.x format.

## Installation

Download `InterzoneXXL-<version>-mpc-armv7.zip` from [Releases](../../releases), or build it (below). Unzip it and
follow the `INSTALL.md` inside. In short:

```
scp -r InterzoneXXL-<version> root@<device-ip>:/tmp/
ssh -t root@<device-ip> sh /tmp/InterzoneXXL-<version>/install.sh
```

The installer asks for confirmation (`-y` skips it), **stops MPC** (save your project first), copies the plugin to
`/sdcard/Synths/ANDREALPHEUS - VST - InterzoneXXL/`, backs up and edits `MPC.settings`, and starts MPC again. Then
load **InterzoneXXL** (manufacturer ANDREALPHEUS) as a plugin instrument on a track. `uninstall.sh` removes it.

## Building

Linux or WSL with Python 3 (+ Pillow, cairosvg), gcc and [Zig](https://ziglang.org/)
(`pip install ziglang pillow cairosvg`). No Docker.

```
git clone --recursive https://github.com/sunskiefer/InterzoneXXL
cd InterzoneXXL
./build.sh                # build/arm/interzonexxl.so + the skin -> build/package/
test/run_tests.sh         # the framework's host test + the engine against Valley's module, under ASan/UBSan
tools/device_bench.sh <device-ip>   # CPU on the device, Mono and Poly 16 (nothing is installed)
```

- `tools/gen_params.py` is the single source of the parameter list (`params.json`, `src/param_ids.h`). MPC stores
  automation by parameter index, so parameters are appended only.
- `tools/panel.py` places every control where the module has it; `tools/layout.py` writes `layout.conf` from it, and
  `tools/panel_art.py` draws the skin from Valley's and VCV's artwork (`art/`).

## Related projects

- [mpc-vst-plugins](https://github.com/sd88me/mpc-vst-plugins) by sd88me: the framework InterzoneXXL is built on,
  and the plugin catalog.
- [PlateauXXL](https://github.com/sunskiefer/PlateauXXL) and [RMXXXL](https://github.com/sunskiefer/RMXXXL) by the
  same author: the build, the limiter, Panic and the Q-Link handling come from them.
- [ValleyRackFree](https://github.com/ValleyAudio/ValleyRackFree): the VCV Rack module InterzoneXXL ports.

## License

InterzoneXXL is released under the **GNU GPL v3.0 or later** ([LICENSE](LICENSE)), as the Valley and VCV Rack code
it includes. Third-party components keep their own licenses (below). The artwork includes VCV Component Library
graphics (CC BY-NC 4.0), so InterzoneXXL is free and must not be sold.

## Credits

- **Interzone:** Dale Johnson, [Valley Audio](https://github.com/ValleyAudio/ValleyRackFree) (GPL-3.0-or-later). The
  DSP is vendored in `third_party/valley` with one local change, shared filter tables
  ([VENDORED.md](third_party/valley/VENDORED.md)); the module's control layer is re-written for MPC OS in
  `src/engine.cc`.
- **VCV Rack's SIMD and DSP headers:** VCV (GPL-3.0-or-later), unchanged in `third_party/rack`
  ([VENDORED.md](third_party/rack/VENDORED.md)).
- **SIMDe:** Evan Nemerson and contributors (MIT), the SSE-on-NEON layer Rack uses on ARM, in `third_party/simde`.
- **Brickwall limiter, Panic, Q-Link handling:** from [RMXXXL](https://github.com/sunskiefer/RMXXXL) and
  [PlateauXXL](https://github.com/sunskiefer/PlateauXXL) by L'Cronx (GPL-3.0-or-later).
- **VST2 wrapper, skin generator, installer:** sd88me ([mpc-vst-plugins](https://github.com/sd88me/mpc-vst-plugins))
  (MIT), as a submodule in `third_party/mpc-vst-plugins`.
- **Artwork** (`art/`): Interzone's panel, Valley's sliders and Rogan knobs (ValleyRackFree), VCV's switches (VCV
  Component Library, CC BY-NC 4.0); see [art/README.md](art/README.md).
- **Interface font:** [Titillium Web](https://fonts.google.com/specimen/Titillium+Web), SIL Open Font License 1.1.

InterzoneXXL is not affiliated with or endorsed by Valley Audio, VCV or Akai Professional.
