<CsoundSynthesizer>
<CsOptions>
; Select audio/midi flags here according to platform
-odac    ;;;realtime audio out
;-iadc    ;;;uncomment -iadc if realtime audio input is needed too
; For Non-realtime ouput leave only the line below:
; -o oscils.wav -W ;;; for file output any platform
</CsOptions>
<CsInstruments>

sr = 44100
ksmps = 1
nchnls = 1
0dbfs  = 1

instr 1 

aout in
;aout reson aout, 18000, 18000
	out aout
endin

instr 2
aout in
aout1 reson aout, 15000, 10
aout compress aout1, aout, 0, 60, 70, 3, 0.1, 0.5, 0.02
	out aout
endin

instr 3
adel	init 0
ilev    = p4				;level of direct sound
idelay  = p5 *.001			;Delay in ms
ifd	= p6				;feedback
aout in
adel delay aout + (adel*ifd), idelay
aout moogvcf adel, 1500, .6, 1
	out aout
endin

instr 4
  ; Input mixed with output of phaser1 to generate notches.
  ; Shows the effects of different iorder values on the sound
  aout in
  idur   = p3 
  iamp   = p4 * .05
  iorder = p5        ; number of 1st-order stages in phaser1 network.
                     ; Divide iorder by 2 to get the number of notches.
  ifreq  = p6        ; frequency of modulation of phaser1
  ifeed  = p7        ; amount of feedback for phaser1

  kamp   linseg 0, .2, iamp, idur - .2, iamp, .2, 0

  iharms = (sr*.4) / 100

  asig   gbuzz 1, 100, iharms, 1, .95, 1  ; "Sawtooth" waveform modulation oscillator for phaser1 ugen.
  kfreq  oscili 5500, ifreq, 2
  kmod   = kfreq + 5600

  aphs   phaser1 aout, kmod, iorder, ifeed
  out    (aout + aphs) * iamp

endin

instr 5
aout in
kfeedback = p4
asnd vco2 .2, 50
adel linseg 0, p3*.5, 0.02, p3*.5, 0	;max delay time =20ms	
aflg flanger aout, asnd, kfeedback
asig clip aflg, 1, 1
	out (asig+aout)
endin

</CsInstruments>
<CsScore>

f1 0  16384 9 .5 -1 0
; cosine wave for gbuzz
f2 0  8192 9 1 1 .25

;i 1 + 600
;i 2 0 400
;i4 0 700 7000 64 .2 .9
;i 5  0 100 .2
;i 3 0 400 2 200 .95
;i 3 4 5 2 20 .95
i 3 0 50 2 50 .95
i 2 50 100
i 1 100 600
;i 3 + 3 3 5 0
</CsScore>
</CsoundSynthesizer>
