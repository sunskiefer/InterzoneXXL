"""Layout of the source pages, from PlateauXXL's layout.conf (same controls and look; {t} = the tab's index).
LFO (four Bogaudio LFOs), TIDAL / RANDOM (Tides 2, Marbles), SEQ (two CV and two gate sequencers, SEQ SET).
"""

LFO = r"""
[tab LFO]
#@panel tab={t} color=dddddd ink=1a1a1a
#@text tab={t} cx=170 cy=128 size=54 weight=light spacing=12 label="LFO"
#@box tab={t} x=740 y=150 w=520 h=470 fill=fafafa
#@box tab={t} x=1000 y=150 w=260 h=470 fill=bbbbbb
#@text tab={t} cx=640 cy=692 size=30 spacing=8 label="BOGAUDIO"
#@text tab={t} cx=170 cy=196 size=18 label="SYNC"
#@text tab={t} cx=170 cy=548 size=18 label="SLOW"
#@text tab={t} cx=1000 cy=176 size=20 label="WAVE"
#@text tab={t} cx=1000 cy=600 size=15 color=6a6a6a label="the LFO's one output: which of the module's six jacks"
popup cx=170 cy=240 w=210 h=48 label="" key=lfo1_sync banks="LFO 1" accent=1a1a1a
knob cx=170 cy=400 r=57 label="FREQ" key=lfo1_freq banks="LFO 1" ink=1a1a1a ink_dim=3a3a3a
toggle cx=170 cy=590 label="" key=lfo1_slow ns=0 banks="LFO 1"
knob cx=430 cy=260 r=42 label="SAM" key=lfo1_sample banks="LFO 1" ink=1a1a1a ink_dim=3a3a3a
knob cx=610 cy=260 r=42 label="PW" key=lfo1_pw banks="LFO 1" ink=1a1a1a ink_dim=3a3a3a
knob cx=395 cy=490 r=34 label="SMTH" key=lfo1_smooth bw=120 banks="LFO 1" ink=1a1a1a ink_dim=3a3a3a
knob cx=525 cy=490 r=34 label="OFF" key=lfo1_offset bw=120 banks="LFO 1" ink=1a1a1a ink_dim=3a3a3a
knob cx=655 cy=490 r=34 label="SCL" key=lfo1_scale bw=120 banks="LFO 1" ink=1a1a1a ink_dim=3a3a3a
enum_h cx=1000 cy=330 label="" key=lfo1_wave sw=230 sh=56 rows=3 options="SINE,TRIANGLE,RAMP UP,RAMP DOWN,SQUARE,STEPPED" banks="LFO 1"
popup cx=170 cy=240 w=210 h=48 label="" key=lfo2_sync banks="LFO 2" accent=1a1a1a
knob cx=170 cy=400 r=57 label="FREQ" key=lfo2_freq banks="LFO 2" ink=1a1a1a ink_dim=3a3a3a
toggle cx=170 cy=590 label="" key=lfo2_slow ns=0 banks="LFO 2"
knob cx=430 cy=260 r=42 label="SAM" key=lfo2_sample banks="LFO 2" ink=1a1a1a ink_dim=3a3a3a
knob cx=610 cy=260 r=42 label="PW" key=lfo2_pw banks="LFO 2" ink=1a1a1a ink_dim=3a3a3a
knob cx=395 cy=490 r=34 label="SMTH" key=lfo2_smooth bw=120 banks="LFO 2" ink=1a1a1a ink_dim=3a3a3a
knob cx=525 cy=490 r=34 label="OFF" key=lfo2_offset bw=120 banks="LFO 2" ink=1a1a1a ink_dim=3a3a3a
knob cx=655 cy=490 r=34 label="SCL" key=lfo2_scale bw=120 banks="LFO 2" ink=1a1a1a ink_dim=3a3a3a
enum_h cx=1000 cy=330 label="" key=lfo2_wave sw=230 sh=56 rows=3 options="SINE,TRIANGLE,RAMP UP,RAMP DOWN,SQUARE,STEPPED" banks="LFO 2"
popup cx=170 cy=240 w=210 h=48 label="" key=lfo3_sync banks="LFO 3" accent=1a1a1a
knob cx=170 cy=400 r=57 label="FREQ" key=lfo3_freq banks="LFO 3" ink=1a1a1a ink_dim=3a3a3a
toggle cx=170 cy=590 label="" key=lfo3_slow ns=0 banks="LFO 3"
knob cx=430 cy=260 r=42 label="SAM" key=lfo3_sample banks="LFO 3" ink=1a1a1a ink_dim=3a3a3a
knob cx=610 cy=260 r=42 label="PW" key=lfo3_pw banks="LFO 3" ink=1a1a1a ink_dim=3a3a3a
knob cx=395 cy=490 r=34 label="SMTH" key=lfo3_smooth bw=120 banks="LFO 3" ink=1a1a1a ink_dim=3a3a3a
knob cx=525 cy=490 r=34 label="OFF" key=lfo3_offset bw=120 banks="LFO 3" ink=1a1a1a ink_dim=3a3a3a
knob cx=655 cy=490 r=34 label="SCL" key=lfo3_scale bw=120 banks="LFO 3" ink=1a1a1a ink_dim=3a3a3a
enum_h cx=1000 cy=330 label="" key=lfo3_wave sw=230 sh=56 rows=3 options="SINE,TRIANGLE,RAMP UP,RAMP DOWN,SQUARE,STEPPED" banks="LFO 3"
popup cx=170 cy=240 w=210 h=48 label="" key=lfo4_sync banks="LFO 4" accent=1a1a1a
knob cx=170 cy=400 r=57 label="FREQ" key=lfo4_freq banks="LFO 4" ink=1a1a1a ink_dim=3a3a3a
toggle cx=170 cy=590 label="" key=lfo4_slow ns=0 banks="LFO 4"
knob cx=430 cy=260 r=42 label="SAM" key=lfo4_sample banks="LFO 4" ink=1a1a1a ink_dim=3a3a3a
knob cx=610 cy=260 r=42 label="PW" key=lfo4_pw banks="LFO 4" ink=1a1a1a ink_dim=3a3a3a
knob cx=395 cy=490 r=34 label="SMTH" key=lfo4_smooth bw=120 banks="LFO 4" ink=1a1a1a ink_dim=3a3a3a
knob cx=525 cy=490 r=34 label="OFF" key=lfo4_offset bw=120 banks="LFO 4" ink=1a1a1a ink_dim=3a3a3a
knob cx=655 cy=490 r=34 label="SCL" key=lfo4_scale bw=120 banks="LFO 4" ink=1a1a1a ink_dim=3a3a3a
enum_h cx=1000 cy=330 label="" key=lfo4_wave sw=230 sh=56 rows=3 options="SINE,TRIANGLE,RAMP UP,RAMP DOWN,SQUARE,STEPPED" banks="LFO 4"
qlinks "LFO 1" = lfo1_sync,lfo1_sample,lfo1_pw,lfo1_wave,lfo1_freq,lfo1_smooth,lfo1_offset,lfo1_scale,lfo1_slow
qlinks "LFO 2" = lfo2_sync,lfo2_sample,lfo2_pw,lfo2_wave,lfo2_freq,lfo2_smooth,lfo2_offset,lfo2_scale,lfo2_slow
qlinks "LFO 3" = lfo3_sync,lfo3_sample,lfo3_pw,lfo3_wave,lfo3_freq,lfo3_smooth,lfo3_offset,lfo3_scale,lfo3_slow
qlinks "LFO 4" = lfo4_sync,lfo4_sample,lfo4_pw,lfo4_wave,lfo4_freq,lfo4_smooth,lfo4_offset,lfo4_scale,lfo4_slow
"""

TIDAL_RANDOM = r"""
[tab TIDAL / RANDOM]
#@panel tab={t} color=e6e6e6 ink=211e1e
frame x=0 y=86 w=1280 h=628 title="" banks="TIDAL"
frame x=0 y=86 w=1280 h=628 title="" banks="RANDOM 1|RANDOM 2"
#@braid tab={t} y=128
#@box tab={t} x=0 y=640 w=1280 h=74 fill=636161
# ---- Tidal Modulator 2 (Tides 2)
#@text tab={t} cx=1240 cy=104 size=22 weight=light align=right label="tidal modulator 2" banks="TIDAL"
#@text tab={t} cx=200 cy=150 size=18 label="RANGE" banks="TIDAL"
#@text tab={t} cx=520 cy=150 size=18 label="RAMP" banks="TIDAL"
#@text tab={t} cx=870 cy=150 size=18 label="OUTPUT MODE" banks="TIDAL"
#@text tab={t} cx=1180 cy=150 size=18 label="CLOCK" banks="TIDAL"
#@dots tab={t} x1=200 y1=470 x2=330 y2=640 banks="TIDAL"
#@dots tab={t} x1=1000 y1=470 x2=930 y2=640 banks="TIDAL"
#@dots tab={t} x1=440 y1=590 x2=470 y2=640 banks="TIDAL"
#@dots tab={t} x1=800 y1=590 x2=770 y2=640 banks="TIDAL"
#@text tab={t} cx=230 cy=677 size=22 color=e6e6e6 label="1" banks="TIDAL"
#@text tab={t} cx=500 cy=677 size=22 color=e6e6e6 label="2" banks="TIDAL"
#@text tab={t} cx=770 cy=677 size=22 color=e6e6e6 label="3" banks="TIDAL"
#@text tab={t} cx=1040 cy=677 size=22 color=e6e6e6 label="4" banks="TIDAL"
#@text tab={t} cx=640 cy=700 size=14 color=bdbaba label="sources TIDAL 1 - 4" banks="TIDAL"
enum_h cx=200 cy=196 label="" key=td_range sw=96 sh=40 options="LOW,MED,HIGH" banks="TIDAL"
enum_h cx=520 cy=196 label="" key=td_ramp sw=96 sh=40 options="AD,CYCLE,AR" banks="TIDAL"
enum_h cx=870 cy=196 label="" key=td_output sw=100 sh=40 options="GATES,AMP,SLOPE,FREQ" banks="TIDAL"
popup cx=1180 cy=196 w=160 h=44 label="" key=td_sync banks="TIDAL" accent=02a1ab
knob cx=200 cy=380 r=56 label="FREQUENCY" key=td_freq banks="TIDAL" ink=211e1e ink_dim=636161
knob cx=1000 cy=380 r=56 label="SHAPE" key=td_shape banks="TIDAL" ink=211e1e ink_dim=636161
knob cx=600 cy=330 r=40 label="SMOOTHNESS" key=td_smooth banks="TIDAL" ink=211e1e ink_dim=636161
knob cx=420 cy=500 r=40 label="SLOPE" key=td_slope banks="TIDAL" ink=211e1e ink_dim=636161
knob cx=780 cy=500 r=40 label="SHIFT/LEVEL" key=td_shift banks="TIDAL" ink=211e1e ink_dim=636161
qlinks "TIDAL" = td_range,td_ramp,td_output,td_sync,td_freq,td_smooth,td_shape,td_slope,td_shift
# ---- Random Sampler (Marbles)
#@text tab={t} cx=1240 cy=104 size=22 weight=light align=right label="random sampler" banks="RANDOM 1|RANDOM 2"
#@text tab={t} cx=60 cy=162 size=40 italic=1 font=serif label="t" banks="RANDOM 1|RANDOM 2"
#@text tab={t} cx=1220 cy=162 size=40 italic=1 font=serif label="X" banks="RANDOM 1|RANDOM 2"
#@dotcurve tab={t} x1=110 y1=160 x2=1170 y2=160 banks="RANDOM 1|RANDOM 2"
#@text tab={t} cx=640 cy=180 size=16 label="CLOCK" banks="RANDOM 1|RANDOM 2"
#@text tab={t} cx=80 cy=300 size=16 label="DEJA VU" banks="RANDOM 1|RANDOM 2"
#@text tab={t} cx=1200 cy=300 size=16 label="DEJA VU" banks="RANDOM 1|RANDOM 2"
#@text tab={t} cx=150 cy=622 size=16 align=right label="RANGE" banks="RANDOM 1|RANDOM 2"
#@text tab={t} cx=1130 cy=622 size=16 align=left label="RANGE" banks="RANDOM 1|RANDOM 2"
#@text tab={t} cx=560 cy=594 size=16 label="SCALE" banks="RANDOM 1|RANDOM 2"
#@text tab={t} cx=720 cy=594 size=16 label="Y DIVIDER" banks="RANDOM 1|RANDOM 2"
#@text tab={t} cx=120 cy=677 size=20 italic=1 font=serif color=e6e6e6 label="t1" banks="RANDOM 1|RANDOM 2"
#@text tab={t} cx=290 cy=677 size=20 italic=1 font=serif color=e6e6e6 label="t2" banks="RANDOM 1|RANDOM 2"
#@text tab={t} cx=460 cy=677 size=20 italic=1 font=serif color=e6e6e6 label="t3" banks="RANDOM 1|RANDOM 2"
#@text tab={t} cx=640 cy=677 size=20 italic=1 font=serif color=e6e6e6 label="Y" banks="RANDOM 1|RANDOM 2"
#@text tab={t} cx=820 cy=677 size=20 italic=1 font=serif color=e6e6e6 label="X1" banks="RANDOM 1|RANDOM 2"
#@text tab={t} cx=990 cy=677 size=20 italic=1 font=serif color=e6e6e6 label="X2" banks="RANDOM 1|RANDOM 2"
#@text tab={t} cx=1160 cy=677 size=20 italic=1 font=serif color=e6e6e6 label="X3" banks="RANDOM 1|RANDOM 2"
enum_h cx=300 cy=212 label="" key=mb_t_mode sw=110 sh=36 options="COIN,CLUSTER,DRUMS" banks="RANDOM 1|RANDOM 2"
knob cx=300 cy=320 r=56 label="RATE" key=mb_t_rate banks="RANDOM 1|RANDOM 2" ink=211e1e ink_dim=636161
knob cx=170 cy=478 r=48 label="BIAS" key=mb_t_bias bw=116 banks="RANDOM 1|RANDOM 2" ink=211e1e ink_dim=636161
knob cx=430 cy=478 r=48 label="JITTER" key=mb_t_jitter bw=116 banks="RANDOM 1|RANDOM 2" ink=211e1e ink_dim=636161
enum_h cx=300 cy=622 label="" key=mb_t_range sw=96 sh=34 options="1/4,X1,X4" banks="RANDOM 1|RANDOM 2"
toggle cx=80 cy=340 label="" key=mb_t_dv ns=0 banks="RANDOM 1|RANDOM 2"
popup cx=640 cy=212 w=170 h=40 label="" key=mb_sync banks="RANDOM 1|RANDOM 2" accent=02a1ab
knob cx=640 cy=300 r=48 label="DEJA VU" key=mb_deja_vu banks="RANDOM 1|RANDOM 2" ink=211e1e ink_dim=636161
knob cx=640 cy=470 r=48 label="LENGTH" key=mb_length banks="RANDOM 1|RANDOM 2" ink=211e1e ink_dim=636161
popup cx=560 cy=622 w=150 h=40 label="" key=mb_x_scale banks="RANDOM 1|RANDOM 2" accent=e69e11 cols=2
popup cx=720 cy=622 w=150 h=40 label="" key=mb_y_div banks="RANDOM 1|RANDOM 2" accent=d9265c cols=3
enum_h cx=980 cy=212 label="" key=mb_x_mode sw=110 sh=36 options="IDENT,BUMP,TILT" banks="RANDOM 1|RANDOM 2"
knob cx=980 cy=320 r=56 label="SPREAD" key=mb_x_spread banks="RANDOM 1|RANDOM 2" ink=211e1e ink_dim=636161
knob cx=850 cy=478 r=48 label="BIAS" key=mb_x_bias bw=116 banks="RANDOM 1|RANDOM 2" ink=211e1e ink_dim=636161
knob cx=1110 cy=478 r=48 label="STEPS" key=mb_x_steps bw=116 banks="RANDOM 1|RANDOM 2" ink=211e1e ink_dim=636161
enum_h cx=980 cy=622 label="" key=mb_x_range sw=96 sh=34 options="+2V,+5V,+-5V" banks="RANDOM 1|RANDOM 2"
toggle cx=1200 cy=340 label="" key=mb_x_dv ns=0 banks="RANDOM 1|RANDOM 2"
#@qrows "RANDOM 1" = 1-2
qlinks "RANDOM 1" = mb_t_mode,mb_sync,mb_x_mode,mb_t_dv,mb_t_rate,mb_deja_vu,mb_x_spread,mb_x_dv
#@qrows "RANDOM 2" = 3-4
qlinks "RANDOM 2" = mb_t_bias,mb_t_jitter,mb_length,mb_x_bias,mb_x_steps,mb_t_range,mb_x_scale,mb_y_div,mb_x_range
"""

SEQ = r"""
[tab SEQ]
#@panel tab={t} color=282828 ink=ffffff
frame x=0 y=86 w=1280 h=628 title="" banks="SEQ 1"
frame x=0 y=86 w=1280 h=628 title="" banks="SEQ 2"
frame x=0 y=86 w=1280 h=628 title="" banks="GATE 1"
frame x=0 y=86 w=1280 h=628 title="" banks="GATE 2"
frame x=0 y=86 w=1280 h=628 title="" banks="SEQ SET"
#@title tab={t} x=10 y=92 label="SEQ SET  -  rate, length and slew / width of the four sequencers (on the MPC tempo)" banks="SEQ SET"
#@line tab={t} x1=28 y1=134 x2=1252 y2=134 color=ffffff width=1 banks="SEQ SET"
frame x=10 y=150 w=1260 h=250 title="" banks="SEQ SET"
popup cx=150 cy=290 w=170 h=44 label="" key=sq1_div cols=4 banks="SEQ SET"
knob cx=340 cy=290 r=37 label="LENGTH" key=sq1_len banks="SEQ SET"
knob cx=500 cy=290 r=37 label="SLEW" key=sq1_slew banks="SEQ SET"
popup cx=790 cy=290 w=170 h=44 label="" key=sq2_div cols=4 banks="SEQ SET"
knob cx=980 cy=290 r=37 label="LENGTH" key=sq2_len banks="SEQ SET"
knob cx=1140 cy=290 r=37 label="SLEW" key=sq2_slew banks="SEQ SET"
frame x=10 y=420 w=1260 h=250 title="" banks="SEQ SET"
popup cx=150 cy=560 w=170 h=44 label="" key=gt1_div cols=4 banks="SEQ SET"
knob cx=340 cy=560 r=37 label="LENGTH" key=gt1_len banks="SEQ SET"
knob cx=500 cy=560 r=37 label="WIDTH" key=gt1_width banks="SEQ SET"
popup cx=790 cy=560 w=170 h=44 label="" key=gt2_div cols=4 banks="SEQ SET"
knob cx=980 cy=560 r=37 label="LENGTH" key=gt2_len banks="SEQ SET"
knob cx=1140 cy=560 r=37 label="WIDTH" key=gt2_width banks="SEQ SET"
#@title tab={t} x=10 y=150 label="STEP SEQUENCERS" banks="SEQ SET"
#@title tab={t} x=10 y=420 label="GATE SEQUENCERS" banks="SEQ SET"
#@text tab={t} cx=150 cy=246 size=16 label="SEQ 1  RATE" banks="SEQ SET"
#@text tab={t} cx=790 cy=246 size=16 label="SEQ 2  RATE" banks="SEQ SET"
#@text tab={t} cx=150 cy=516 size=16 label="GATE 1  RATE" banks="SEQ SET"
#@text tab={t} cx=790 cy=516 size=16 label="GATE 2  RATE" banks="SEQ SET"
#@line tab={t} x1=640 y1=200 x2=640 y2=380 color=6a6a6a banks="SEQ SET"
#@line tab={t} x1=640 y1=470 x2=640 y2=650 color=6a6a6a banks="SEQ SET"
#@title tab={t} x=10 y=92 label="STEP SEQUENCER 1  -  source SEQ 1, +/-5 V" banks="SEQ 1"
#@line tab={t} x1=28 y1=134 x2=1252 y2=134 color=ffffff width=1 banks="SEQ 1"
#@text tab={t} cx=640 cy=680 size=17 color=8a8a8a label="Rate, Length and Slew: SEQ SET page" banks="SEQ 1"
knob cx=80 cy=250 r=36 label="1" key=sq1_s1 bw=150 banks="SEQ 1"
knob cx=240 cy=250 r=36 label="2" key=sq1_s2 bw=150 banks="SEQ 1"
knob cx=400 cy=250 r=36 label="3" key=sq1_s3 bw=150 banks="SEQ 1"
knob cx=560 cy=250 r=36 label="4" key=sq1_s4 bw=150 banks="SEQ 1"
knob cx=720 cy=250 r=36 label="5" key=sq1_s5 bw=150 banks="SEQ 1"
knob cx=880 cy=250 r=36 label="6" key=sq1_s6 bw=150 banks="SEQ 1"
knob cx=1040 cy=250 r=36 label="7" key=sq1_s7 bw=150 banks="SEQ 1"
knob cx=1200 cy=250 r=36 label="8" key=sq1_s8 bw=150 banks="SEQ 1"
knob cx=80 cy=500 r=36 label="9" key=sq1_s9 bw=150 banks="SEQ 1"
knob cx=240 cy=500 r=36 label="10" key=sq1_s10 bw=150 banks="SEQ 1"
knob cx=400 cy=500 r=36 label="11" key=sq1_s11 bw=150 banks="SEQ 1"
knob cx=560 cy=500 r=36 label="12" key=sq1_s12 bw=150 banks="SEQ 1"
knob cx=720 cy=500 r=36 label="13" key=sq1_s13 bw=150 banks="SEQ 1"
knob cx=880 cy=500 r=36 label="14" key=sq1_s14 bw=150 banks="SEQ 1"
knob cx=1040 cy=500 r=36 label="15" key=sq1_s15 bw=150 banks="SEQ 1"
knob cx=1200 cy=500 r=36 label="16" key=sq1_s16 bw=150 banks="SEQ 1"
#@title tab={t} x=10 y=92 label="STEP SEQUENCER 2  -  source SEQ 2, +/-5 V" banks="SEQ 2"
#@line tab={t} x1=28 y1=134 x2=1252 y2=134 color=ffffff width=1 banks="SEQ 2"
#@text tab={t} cx=640 cy=680 size=17 color=8a8a8a label="Rate, Length and Slew: SEQ SET page" banks="SEQ 2"
knob cx=80 cy=250 r=36 label="1" key=sq2_s1 bw=150 banks="SEQ 2"
knob cx=240 cy=250 r=36 label="2" key=sq2_s2 bw=150 banks="SEQ 2"
knob cx=400 cy=250 r=36 label="3" key=sq2_s3 bw=150 banks="SEQ 2"
knob cx=560 cy=250 r=36 label="4" key=sq2_s4 bw=150 banks="SEQ 2"
knob cx=720 cy=250 r=36 label="5" key=sq2_s5 bw=150 banks="SEQ 2"
knob cx=880 cy=250 r=36 label="6" key=sq2_s6 bw=150 banks="SEQ 2"
knob cx=1040 cy=250 r=36 label="7" key=sq2_s7 bw=150 banks="SEQ 2"
knob cx=1200 cy=250 r=36 label="8" key=sq2_s8 bw=150 banks="SEQ 2"
knob cx=80 cy=500 r=36 label="9" key=sq2_s9 bw=150 banks="SEQ 2"
knob cx=240 cy=500 r=36 label="10" key=sq2_s10 bw=150 banks="SEQ 2"
knob cx=400 cy=500 r=36 label="11" key=sq2_s11 bw=150 banks="SEQ 2"
knob cx=560 cy=500 r=36 label="12" key=sq2_s12 bw=150 banks="SEQ 2"
knob cx=720 cy=500 r=36 label="13" key=sq2_s13 bw=150 banks="SEQ 2"
knob cx=880 cy=500 r=36 label="14" key=sq2_s14 bw=150 banks="SEQ 2"
knob cx=1040 cy=500 r=36 label="15" key=sq2_s15 bw=150 banks="SEQ 2"
knob cx=1200 cy=500 r=36 label="16" key=sq2_s16 bw=150 banks="SEQ 2"
#@title tab={t} x=10 y=92 label="GATE SEQUENCER 1  -  source GATE 1, 10 V" banks="GATE 1"
#@line tab={t} x1=28 y1=134 x2=1252 y2=134 color=ffffff width=1 banks="GATE 1"
#@text tab={t} cx=640 cy=680 size=17 color=8a8a8a label="Rate, Length and Width: SEQ SET page" banks="GATE 1"
toggle cx=80 cy=260 label="1" key=gt1_g1 bw=150 banks="GATE 1"
toggle cx=240 cy=260 label="2" key=gt1_g2 bw=150 banks="GATE 1"
toggle cx=400 cy=260 label="3" key=gt1_g3 bw=150 banks="GATE 1"
toggle cx=560 cy=260 label="4" key=gt1_g4 bw=150 banks="GATE 1"
toggle cx=720 cy=260 label="5" key=gt1_g5 bw=150 banks="GATE 1"
toggle cx=880 cy=260 label="6" key=gt1_g6 bw=150 banks="GATE 1"
toggle cx=1040 cy=260 label="7" key=gt1_g7 bw=150 banks="GATE 1"
toggle cx=1200 cy=260 label="8" key=gt1_g8 bw=150 banks="GATE 1"
toggle cx=80 cy=480 label="9" key=gt1_g9 bw=150 banks="GATE 1"
toggle cx=240 cy=480 label="10" key=gt1_g10 bw=150 banks="GATE 1"
toggle cx=400 cy=480 label="11" key=gt1_g11 bw=150 banks="GATE 1"
toggle cx=560 cy=480 label="12" key=gt1_g12 bw=150 banks="GATE 1"
toggle cx=720 cy=480 label="13" key=gt1_g13 bw=150 banks="GATE 1"
toggle cx=880 cy=480 label="14" key=gt1_g14 bw=150 banks="GATE 1"
toggle cx=1040 cy=480 label="15" key=gt1_g15 bw=150 banks="GATE 1"
toggle cx=1200 cy=480 label="16" key=gt1_g16 bw=150 banks="GATE 1"
#@title tab={t} x=10 y=92 label="GATE SEQUENCER 2  -  source GATE 2, 10 V" banks="GATE 2"
#@line tab={t} x1=28 y1=134 x2=1252 y2=134 color=ffffff width=1 banks="GATE 2"
#@text tab={t} cx=640 cy=680 size=17 color=8a8a8a label="Rate, Length and Width: SEQ SET page" banks="GATE 2"
toggle cx=80 cy=260 label="1" key=gt2_g1 bw=150 banks="GATE 2"
toggle cx=240 cy=260 label="2" key=gt2_g2 bw=150 banks="GATE 2"
toggle cx=400 cy=260 label="3" key=gt2_g3 bw=150 banks="GATE 2"
toggle cx=560 cy=260 label="4" key=gt2_g4 bw=150 banks="GATE 2"
toggle cx=720 cy=260 label="5" key=gt2_g5 bw=150 banks="GATE 2"
toggle cx=880 cy=260 label="6" key=gt2_g6 bw=150 banks="GATE 2"
toggle cx=1040 cy=260 label="7" key=gt2_g7 bw=150 banks="GATE 2"
toggle cx=1200 cy=260 label="8" key=gt2_g8 bw=150 banks="GATE 2"
toggle cx=80 cy=480 label="9" key=gt2_g9 bw=150 banks="GATE 2"
toggle cx=240 cy=480 label="10" key=gt2_g10 bw=150 banks="GATE 2"
toggle cx=400 cy=480 label="11" key=gt2_g11 bw=150 banks="GATE 2"
toggle cx=560 cy=480 label="12" key=gt2_g12 bw=150 banks="GATE 2"
toggle cx=720 cy=480 label="13" key=gt2_g13 bw=150 banks="GATE 2"
toggle cx=880 cy=480 label="14" key=gt2_g14 bw=150 banks="GATE 2"
toggle cx=1040 cy=480 label="15" key=gt2_g15 bw=150 banks="GATE 2"
toggle cx=1200 cy=480 label="16" key=gt2_g16 bw=150 banks="GATE 2"
qlinks "SEQ 1" = sq1_s1,sq1_s2,sq1_s3,sq1_s4,sq1_s5,sq1_s6,sq1_s7,sq1_s8,sq1_s9,sq1_s10,sq1_s11,sq1_s12,sq1_s13,sq1_s14,sq1_s15,sq1_s16
qlinks "SEQ 2" = sq2_s1,sq2_s2,sq2_s3,sq2_s4,sq2_s5,sq2_s6,sq2_s7,sq2_s8,sq2_s9,sq2_s10,sq2_s11,sq2_s12,sq2_s13,sq2_s14,sq2_s15,sq2_s16
qlinks "GATE 1" = gt1_g1,gt1_g2,gt1_g3,gt1_g4,gt1_g5,gt1_g6,gt1_g7,gt1_g8,gt1_g9,gt1_g10,gt1_g11,gt1_g12,gt1_g13,gt1_g14,gt1_g15,gt1_g16
qlinks "GATE 2" = gt2_g1,gt2_g2,gt2_g3,gt2_g4,gt2_g5,gt2_g6,gt2_g7,gt2_g8,gt2_g9,gt2_g10,gt2_g11,gt2_g12,gt2_g13,gt2_g14,gt2_g15,gt2_g16
qlinks "SEQ SET" = sq1_div,sq1_len,sq1_slew,sq2_div,sq2_len,sq2_slew,gt1_div,gt1_len,gt1_width,gt2_div,gt2_len,gt2_width
"""
