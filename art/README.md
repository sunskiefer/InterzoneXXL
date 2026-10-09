# Artwork

| Folder | What | From | Licence |
| --- | --- | --- | --- |
| `valley/` | Interzone's dark panel, Valley's sliders (`valleySliderBackground.svg`, `slider*.svg`), Rogan knobs (`v2/`), the LED button | [ValleyRackFree](https://github.com/ValleyAudio/ValleyRackFree) `res/`, commit `86f02e4`, unchanged | ValleyRackFree's (GPL-3.0-or-later); the Rogan knobs are Valley's recoloured versions of VCV's Component Library knobs (graphics by Grayscale, CC BY-NC 4.0) |
| `valley/*.svg` (flat) | Rogan Med / MedSmall / Small knobs for the CV IN, VOICE and SEQ pages | ValleyRackFree `res/v2/`, via PlateauXXL, unchanged | as above |
| `vcv/` | The CKSS and CKSSThree switches; Rogan 3PS, 2PS, 1PS white knobs for TIDAL / RANDOM | [VCV Rack](https://github.com/VCVRack/Rack) `res/ComponentLibrary/`, v2.6.6 / commit `061ccf6`, unchanged | VCV Component Library, CC BY-NC 4.0 |
| `bogaudio/` | Knob68, Knob26, Knob16 for the LFO pages | BogaudioModules `res/` (commit `656eaae`), via PlateauXXL, unchanged | CC BY-SA 4.0, (c) Matt Demanett |
| `fonts/` | Titillium Web SemiBold | Google Fonts | SIL Open Font License 1.1 (`fonts/OFL.txt`) |

`tools/panel_art.py` draws the Interzone pages from these files at build time, `tools/knob_art.py` the other pages'
knobs (the filmstrips made from Bogaudio's knobs are CC BY-SA 4.0 like their source); the built PNGs are not kept in the repository.
Because of the CC BY-NC 4.0 graphics, InterzoneXXL is free and must not be sold.
