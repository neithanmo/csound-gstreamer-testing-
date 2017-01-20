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
ksmps = 30
nchnls = 1
0dbfs  = 1
cpuprc 1, 30
instr 1 
aout in
aout reson aout, 5500, 5000
	out aout
endin
</CsInstruments>
<CsScore>

i 1 0 200
</CsScore>
</CsoundSynthesizer>
