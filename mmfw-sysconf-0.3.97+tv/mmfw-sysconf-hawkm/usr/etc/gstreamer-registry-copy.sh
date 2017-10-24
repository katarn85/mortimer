#!/bin/sh
export GST_REGISTRY=/home/app/.gstreamer-0.10/registry.bin

file_orig="/usr/share/.gstreamer-0.10/registry.bin"
file_new="/home/app/.gstreamer-0.10/registry.bin"
file_time="/home/app/.gstreamer-0.10/.registry-birthtime"

if [ -f "$file_new" ]
then
    if [ "$(<$file_time)" != "$(stat -c %y $file_orig)" ]
    then
        cp $file_orig $file_new
        chsmack -t -a "system::homedir" $file_new
        echo $(stat -c %y $file_orig) > $file_time
        chown app:app /home/app/.gstreamer-0.10 -R
     fi
else
    if [ ! -d "/home/app/.gstreamer-0.10" ]
    then
        mkdir /home/app/.gstreamer-0.10
        chsmack -t -a "system::homedir" /home/app/.gstreamer-0.10/
    fi
    cp $file_orig $file_new
    chsmack -t -a "system::homedir" $file_new
    echo $(stat -c %y $file_orig) > $file_time
    chown app:app /home/app/.gstreamer-0.10 -R
fi
