#!/bin/sh

/usr/bin/amixer -c 0 cset iface=MIXER,name="Audio Mode Playback" "off"
/usr/bin/amixer -c 0 cset iface=MIXER,name="Dsp Parameter" 35
/usr/bin/amixer -c 0 cset iface=MIXER,name="Dsp Parameter" 32
/usr/bin/amixer -c 0 cset iface=MIXER,name="Dsp Parameter" 37
/usr/bin/amixer -c 0 cset iface=MIXER,name="Dsp Parameter" 40
/usr/bin/amixer -c 0 cset iface=MIXER,name="Output Path" "LO1"
/usr/bin/amixer -c 0 cset iface=MIXER,name="Main Mic" "MIC2"
/usr/bin/amixer -c 0 cset iface=MIXER,name="Sub Mic" "MIC3"
/usr/bin/amixer -c 0 cset iface=MIXER,name="ADC0L Source" "MainMIC"
/usr/bin/amixer -c 0 cset iface=MIXER,name="ADC0R Source" "SubMIC"
/usr/bin/amixer -c 0 cset iface=MIXER,name="ADIF0L Source" "ADC0L"
/usr/bin/amixer -c 0 cset iface=MIXER,name="ADIF0R Source" "ADC0R"
/usr/bin/amixer -c 0 cset iface=MIXER,name="Input Path" "ADIF0"
/usr/bin/amixer -c 0 cset iface=MIXER,name="Mic2 Volume" 60
/usr/bin/amixer -c 0 cset iface=MIXER,name="Mic2 Switch" 1
/usr/bin/amixer -c 0 cset iface=MIXER,name="Mic3 Volume" 60
/usr/bin/amixer -c 0 cset iface=MIXER,name="Mic3 Switch" 1
/usr/bin/amixer -c 0 cset iface=MIXER,name="Adif0 Input Volume" 96,96
/usr/bin/amixer -c 0 cset iface=MIXER,name="Adif0 Input Switch" 1,1
/usr/bin/amixer -c 0 cset iface=MIXER,name="Dac0 Output Volume" 96,96
/usr/bin/amixer -c 0 cset iface=MIXER,name="Dac0 Output Switch" 1,1
/usr/bin/amixer -c 0 cset iface=MIXER,name="LineOut1 Volume" 111,111
/usr/bin/amixer -c 0 cset iface=MIXER,name="LineOut1 Switch" 1,1
/usr/bin/amixer -c 0 cset iface=MIXER,name="Audio Mode Playback" "off"
/usr/bin/amixer -c 0 cset iface=MIXER,name="Audio Mode Capture" "audio"
/usr/bin/amixer -c 0 cset iface=MIXER,name="adc in" "adc on"
/usr/bin/amixer -c 0 cset iface=MIXER,name="main out source connect" "LineIn/Composite"
/usr/bin/amixer -c 0 cset iface=MIXER,name="main out gain" 100
/usr/bin/amixer -c 0 cset iface=MIXER,name="main out mute" 0
