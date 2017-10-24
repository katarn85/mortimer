# Make GStreamer registry

export GST_REGISTRY=/usr/share/.gstreamer-0.10/registry.bin

/usr/bin/gst-inspect-0.10

chmod 666 /usr/share/.gstreamer-0.10/registry.bin

chown app:app /usr/share/.gstreamer-0.10 -R

chsmack -t -a "system::homedir" /usr/share/.gstreamer-0.10/registry.bin

chsmack -t -a "system::homedir" /usr/share/.gstreamer-0.10/
