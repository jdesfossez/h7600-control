
HEADM adc 2 2 g1-out g2-out "Test Gain" "" 1 page-obj
MENUPAGE page "gain params" "gain" 1 lvl-obj
KNOB lvl "level: %4.1f dB" "" -40 0 0.1000 -6
GAIN g1 adc-in1 lvl-out
GAIN g2 adc-in2 lvl-out
TAIL test
