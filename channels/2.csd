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
ksmps = 50
nchnls = 1
0dbfs  = 1

instr 1
iflg chnget "moog"
asig chnget "ainput"
;asig sum ksig, asig
asig moogladder asig, 50000, 0.25
     out asig
endin
</CsInstruments>
<CsScore>

i 1 0 125
e
</CsScore>
</CsoundSynthesizer>
