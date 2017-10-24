#!/bin/sh

/usr/bin/amixer -c 0 cset iface=MIXER,name="adc in" "adc on"
/usr/bin/amixer -c 0 cset iface=MIXER,name="sub out source select" "LineIn/Composite"
/usr/bin/amixer -c 0 cset iface=MIXER,name="main out gain" 100
/usr/bin/amixer -c 0 cset iface=MIXER,name="main out mute" 0
