#!/bin/sh

/usr/bin/amixer -c 0 cset iface=MIXER,name="Audio Mode Capture" "off"
/usr/bin/amixer -c 0 cset iface=MIXER,name="Dsp Parameter" 31
/usr/bin/amixer -c 0 cset iface=MIXER,name="Dsp Parameter" 32
/usr/bin/amixer -c 0 cset iface=MIXER,name="Dsp Parameter" 33
/usr/bin/amixer -c 0 cset iface=MIXER,name="Dsp Parameter" 34
/usr/bin/amixer -c 0 cset iface=MIXER,name="Output Path" "SP"
/usr/bin/amixer -c 0 cset iface=MIXER,name="LineIn1 Volume" 33,33
/usr/bin/amixer -c 0 cset iface=MIXER,name="LineIn1 Switch" 1,1
/usr/bin/amixer -c 0 cset iface=MIXER,name="Adif1 Input Volume" 96,96
/usr/bin/amixer -c 0 cset iface=MIXER,name="Adif1 Input Switch" 1,1
/usr/bin/amixer -c 0 cset iface=MIXER,name="Dac1 Output Volume" 96,96
/usr/bin/amixer -c 0 cset iface=MIXER,name="Dac1 Output Switch" 1,1
/usr/bin/amixer -c 0 cset iface=MIXER,name="Speaker Volume" 95,95
/usr/bin/amixer -c 0 cset iface=MIXER,name="Speaker Switch" 1,1
/usr/bin/amixer -c 0 cset iface=MIXER,name="Audio Mode Playback" "audioae"
/usr/bin/amixer -c 0 cset iface=MIXER,name="Audio Mode Capture" "off"