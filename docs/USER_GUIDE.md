# InterzoneXXL user guide (0.1.0)

## Load it
After installing (README), on a track choose a plugin instrument: **InterzoneXXL** by ANDREALPHEUS. Play it from
the pads, a MIDI keyboard or a sequence. It starts in **Mono** with the module's defaults: a saw, the filter open,
the envelope at A 0 / D 0 / S 100 % / R 0.

## Pages
The first three pages are Interzone's panel. Touch a slider and drag it up or down, or turn its Q-Link; its value
shows above it. Tap a switch to flip it (a Q-Link turn flips it once per turn). The detent knobs (Octave, LFO Wave)
and the three-way switches (PWM source, sub wave) step through their positions as you drag or turn. Double-tap a
slider or knob for MPC's large value overlay.

**VCO:** Glide (the pitch slew, between any two notes) · Mod (pitch modulation depth, from the LFO or the envelope,
the envelope with either polarity) · Semi / Cont (Coarse in semitones or continuous) · Octave (32' to 2') · Coarse
(+-12 semitones) · Fine (+-100 cents) · Width (pulse width, square at the bottom) · PWM (depth, from the envelope with
either polarity, the LFO, or Ext, which is 0 V until patching arrives).

**FILTER / LFO:** Freq (cutoff, 13.75 Hz to 14 kHz) · Res · HPF (high-pass cutoff) · envelope polarity · 4P / 2P ·
Env, LFO and VOct (how far the envelope, the LFO and the played pitch move the cutoff) · LFO Rate (0.1 Hz to 200 Hz) ·
Wave (sine, triangle, saw up, saw down, square, sample and hold, noise) · Fine · Slew.

**MIXER / ENV:** the saw, pulse and sub levels, the sub's octave (+2 to -2: two octaves down, one down, a fourth
down, unison, a fifth up, one and two octaves up) and wave (saw, square, narrow pulse), Pink / White, the noise and
Ext levels (Ext is silent until patching arrives) · Long / Short (envelope time scale x10) · Cycle / Gate (Cycle loops
the envelope while a key is held) · Man. (holds the gate on its own: tap on, tap off; a drone) · A, D, S, R · VCA Gate
/ Env (the VCA follows the gate or the envelope).

**VOICE:** Mode (Mono / Poly) · Poly Voices (1-16) · Mono Notes (Legato: a note played while another is held changes
the pitch without restarting the envelope, gliding if Glide is up; Retrigger: every note restarts the envelope) ·
Bend Range (0-24 semitones) · Level (the summed voices, -6 dB by default) · the limiter: Drive, Ceiling, Release ·
Panic (every setting back to its default, every note off).

## Q-Links
Every control is on a Q-Link, the panel read left to right: Q-Links 1-8 are the left half of the page, 9-16 the
right half (VCO: Glide, Mod, Mod polarity, Mod source, Semi/Cont, Octave, Coarse, Fine | Width, PWM, PWM polarity,
PWM source).

## Poly and CPU
Each group of four voices costs the same whether one or four notes sound, so 4, 8, 12 and 16 voices are the natural
steps. Use the fewest voices your part needs.
