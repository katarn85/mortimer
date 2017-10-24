#!/bin/sh
# this script provides feedback for GStreamer debugging
# the user can run this and provide the GStreamer developers with information
# about their system

command_output ()
{
  /bin/echo "+++ $1"
  $1
}

/bin/echo "GStreamer feedback script."
/bin/echo "Please attach the output of this script to your bug reports."
/bin/echo "Bug reports should go into Gnome's bugzilla (http://bugzilla.gnome.org)"
/bin/echo

/bin/echo "+   SYSTEM INFORMATION"
command_output "uname -a"

if test -f /etc/redhat-release; then
  /bin/echo "+++  distribution: Red Hat"
  /bin/cat /etc/redhat-release
fi

if test -f /etc/debian_version; then
  /bin/echo "+++  distribution: Debian"
  /bin/cat /etc/debian_version
fi

command_output "cat /etc/issue"

/bin/echo

/bin/echo "+   USER INFORMATION"
command_output "id"
/bin/echo

/bin/echo "+   PKG-CONFIG INFORMATION"
for mm in 0.6 0.7 0.8
do
  /bin/echo "+   $mm"
  command_output "pkg-config --version"
  command_output "pkg-config gstreamer-$mm --modversion" 2>/dev/null
  command_output "pkg-config gstreamer-$mm --cflags" 2>/dev/null
  command_output "pkg-config gstreamer-$mm --libs" 2>/dev/null
  command_output "pkg-config gstreamer-libs-$mm --modversion" 2>/dev/null
  command_output "pkg-config gstreamer-libs-$mm --cflags" 2>/dev/null
  command_output "pkg-config gstreamer-libs-$mm --libs" 2>/dev/null
  /bin/echo
done

for mm in 0.9 0.10
do
  for module in gstreamer gstreamer-base gstreamer-check gstreamer-controller\
                gstreamer-dataprotocol gstreamer-plugins-base gstreamer-net\
                gst-python
  do
    /bin/echo "+   $mm"
    command_output "pkg-config $module-$mm --modversion" 2>/dev/null
    command_output "pkg-config $module-$mm --cflags" 2>/dev/null
    command_output "pkg-config $module-$mm --libs" 2>/dev/null
    /bin/echo
  done
done

/bin/echo "+   GSTREAMER INFORMATION (unversioned)"
command_output "which gst-inspect"
command_output "gst-inspect"
command_output "gst-inspect fakesrc"
command_output "gst-inspect fakesink"
command_output "gst-launch fakesrc num-buffers=5 ! fakesink"
for mm in 0.6 0.7 0.8 0.9 0.10
do
  /bin/echo "+   GSTREAMER INFORMATION ($mm)"
  command_output "which gst-inspect-$mm"
  command_output "gst-inspect-$mm"
  command_output "gst-inspect-$mm fakesrc"
  command_output "gst-inspect-$mm fakesink"
  command_output "gst-launch-$mm fakesrc num-buffers=5 ! fakesink"
done

/bin/echo "++  looking for gstreamer libraries in common locations"
for dirs in /usr/lib /usr/local/lib; do
  if test -d $dirs; then
    /bin/find $dirs -name libgstreamer* | grep so
  fi
done
echo "++  looking for gstreamer headers in common locations"
for dirs in /usr/include /usr/local/include; do
  if test -d $dirs; then
    /bin/find $dirs -name gst.h
  fi
done

/bin/echo "+   GSTREAMER PLUG-INS INFORMATION"
command_output "gst-inspect volume"
for mm in 0.6 0.7 0.8 0.9 0.10
do
  command_output "gst-inspect-$mm volume"
done

/bin/echo "++  looking for gstreamer volume plugin in common locations"
for dirs in /usr/lib /usr/local/lib; do
  if test -d $dirs; then
    /bin/find $dirs -name libgstvolume* | grep so
  fi
done
echo "++  looking for gstreamer headers in common locations"
for dirs in /usr/include /usr/local/include; do
  if test -d $dirs; then
    /bin/find $dirs -name audio.h
  fi
done


