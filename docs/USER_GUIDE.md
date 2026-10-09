# InterzoneXXL user guide (0.1.1)

## A first patch
On MIXER / ENV raise Ext: the Ext Osc adds a square an octave below the VCO. On SEQ, GATE 1, switch on a few
steps; on CV IN, CV 2, set ENV GATE to Gate 1; press play: the voice plays the gates. Set VOCT 2 to Seq 1 (CV 1) and
turn some SEQ 1 steps: it plays a melody.

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
either polarity, the LFO, or Ext: the PWM jack, patched on CV IN).

**FILTER / LFO:** Freq (cutoff, 13.75 Hz to 14 kHz) · Res · HPF (high-pass cutoff) · envelope polarity · 4P / 2P ·
Env, LFO and VOct (how far the envelope, the LFO and the played pitch move the cutoff) · LFO Rate (0.1 Hz to 200 Hz) ·
Wave (sine, triangle, saw up, saw down, square, sample and hold, noise) · Fine · Slew.

**MIXER / ENV:** the saw, pulse and sub levels, the sub's octave (+2 to -2: two octaves down, one down, a fourth
down, unison, a fifth up, one and two octaves up) and wave (saw, square, narrow pulse), Pink / White, the noise and
Ext levels (Ext is the Mixer Ext jack: the Ext Osc by default) · Long / Short (envelope time scale x10) · Cycle / Gate (Cycle loops
the envelope while a key is held) · Man. (holds the gate on its own: tap on, tap off; a drone) · A, D, S, R · VCA Gate
/ Env (the VCA follows the gate or the envelope).

**VOICE** (three Q-Link sub-pages). VOICE: Mode (Mono / Poly) · Poly Voices (1-16) · Mono Notes (Legato: a note played while another is held changes
the pitch without restarting the envelope, gliding if Glide is up; Retrigger: every note restarts the envelope) ·
Bend Range (0-24 semitones) · Level (the summed voices, -6 dB by default) · the limiter: Drive, Ceiling, Release ·
Panic (every setting back to its default, every note off; the preset slot stays). EXT OSC: the extra oscillator's
wave (saw, square, triangle, sine, noise), octave (-3 to +2 from the played note), tune (+-12 semitones) and width
(the square's). It follows each voice's pitch, Glide included. PRESETS: pick a slot, SAVE or LOAD; files in
`/sdcard/InterzoneXXL Presets`.

**CV IN** (two Q-Link sub-pages): Interzone's input jacks. Each has a source (the list below it) and an amount (the
knob above it; 100 % is the cable plugged straight in, minus inverts). Freq 1 and Freq 2 use the module's own blue
attenuverters and VCA Level its orange one, so they start at 0 as on the module: turn them up. CV 1: VOct 2 (1 V per
octave: a sequencer here transposes), PWM (used when the VCO's PWM source is Ext), Mixer Ext (the Ext slider's
input; Ext Osc by default), Freq 1, Freq 2, Res, VCA Level, LFO Rate (1 V per octave). CV 2, the gate jacks (high
above 1 V): LFO Trig (a new S+H value), LFO Reset, Env Gate (added to the MIDI gate: a gate sequencer plays the voice
with no key down, at its last pitch; in Poly every voice that has had a note), Env Trig (retriggers held notes).

The sources: LFO 1-4, Tidal 1-4, X1-X3, Y and T1-T3 (Random Sampler), Seq 1-2, Gate 1-2, Ext Osc, Interzone's own
jacks (IZ Sine, Tri, Saw Up, Saw Down, Pulse, S+H, Noise; Env + and -; VCO Saw, Pulse and Sub), Velocity, Mod Wheel
and Pressure (0-10 V). The envelope, VCO, Ext Osc and velocity sources are per voice in Poly (a poly cable).

**LFO** (LFO 1-4), **TIDAL / RANDOM** and **SEQ** (Seq 1-2, Gate 1-2, SEQ SET) are PlateauXXL's pages: four
Bogaudio LFOs, Tidal Modulator 2, Random Sampler, two 16-step CV sequencers (+-5 V, slew) and two 16-step gate
sequencers. Sync locks them to the MPC tempo; press play and they start from the top. See PlateauXXL's user guide for
every control of these modules.

## Q-Links
Every control is on a Q-Link. On the Interzone pages the panel reads left to right: Q-Links 1-8 are the left half of the page, 9-16 the
right half (VCO: Glide, Mod, Mod polarity, Mod source, Semi/Cont, Octave, Coarse, Fine | Width, PWM, PWM polarity,
PWM source).

## Poly and CPU
Each group of four voices costs the same whether one or four notes sound, so 4, 8, 12 and 16 voices are the natural
steps. Use the fewest voices your part needs.
