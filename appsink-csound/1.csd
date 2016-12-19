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
ksmps = 1024
nchnls = 1
0dbfs  = 1

instr 1 
aout in
aout moogladder aout, 50000, 0.25
	out aout
endin
</CsInstruments>
<CsScore>

i 1 0 20
</CsScore>
</CsoundSynthesizer>
