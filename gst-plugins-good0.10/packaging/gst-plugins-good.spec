#sbs-git:slp/pkgs/g/gst-plugins-good0.10 gst-plugins-good 0.10.31 6e8625ba6fe94fb9d09e6c3be220b54ffaa01273
Name:       gst-plugins-good
Summary:    GStreamer plugins from the "good" set
Version:    0.20.31
Release:    21_9
Group:      Applications/Multimedia
License:    LGPLv2+
Source0:    %{name}-%{version}.tar.gz
#Patch0 :    gst-plugins-good-divx-drm.patch
#Patch1 :    gst-plugins-good-ebml-read.patch
#Patch2 :    gst-plugins-good-gstrtph264pay.patch
#Patch3 :    gst-plugins-good-mkv-demux.patch
#Patch4 :    gst-plugins-good-parse-frame.patch
#Patch5 :    gst-plugins-good-qtdemux.patch
Patch6 :    gst-plugins-good-disable-gtk-doc.patch
BuildRequires:  gettext
BuildRequires:  which
BuildRequires:  gst-plugins-base-devel
BuildRequires:  libjpeg-turbo-devel
BuildRequires:  pkgconfig(gstreamer-0.10) 
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(libpng)
BuildRequires:  pkgconfig(libsoup-2.4)
BuildRequires:  pkgconfig(libpulse)
BuildRequires:  pkgconfig(x11)
BuildRequires:  pkgconfig(xfixes)
BuildRequires:  pkgconfig(xdamage)
BuildRequires:  pkgconfig(xext)
BuildRequires:  pkgconfig(vconf)
BuildRequires:  pkgconfig(iniparser)
#BuildRequires:  pkgconfig(drm-client)
#BuildRequires:  pkgconfig(drm-trusted)
BuildRequires:  pkgconfig(gstreamer-interfaces-0.10)

%description
GStreamer is a streaming media framework, based on graphs of filters
which operate on media data.  Applications using this library can do
anything from real-time sound processing to playing videos, and just
about anything else media-related.  Its plugin-based architecture means
that new data types or processing capabilities can be added simply by
installing new plug-ins.
This package contains the GStreamer plugins from the "good" set, a set
of good-quality plug-ins under the LGPL license.


%prep
%setup -q 
#%patch0 -p1
#%patch1 -p1
#%patch2 -p1
#%patch3 -p1
#%patch4 -p1
#%patch5 -p1
%patch6 -p1

%build
./autogen.sh

export CFLAGS+=" -Wall -g -fPIC\
 -DGST_EXT_SOUP_MODIFICATION \
 -DGST_SUPPORT_STORAGE_SERVICE \
 -DGST_SUPPORT_STOP_AT \
 -DGST_EXT_RTSPSRC_MODIFICATION \
 -DGST_EXT_AMRPARSER_MODIFICATION \
 -DGST_EXT_AACPARSER_MODIFICATION \
 -DGST_EXT_SS_TYPE \
 -DGST_EXT_V4L2_MODIFICATION \
 -DGST_EXT_MPEGAUDIO_MODIFICATION \
 -DGST_EXT_ENHANCEMENT \
 -DGST_EXT_XV_ENHANCEMENT \
 -DGST_EXT_VD_WAVPARSE"

%configure --prefix=%{_prefix}\
 --disable-static\
%ifarch %{arm}
 --enable-divx-drm\
%endif
 --disable-nls\
 --with-html-dir=/tmp/dump\
 --disable-examples\
 --disable-gconftool\
 --disable-alpha\
 --disable-audiofx\
 --disable-auparse\
 --disable-cutter\
 --disable-debugutils\
 --disable-deinterlace\
 --disable-effectv\
 --disable-equalizer\
 --disable-flx\
 --disable-goom\
 --disable-goom2k1\
 --disable-level\
 --disable-monoscope\
 --disable-replaygain\
 --disable-smpte\
 --disable-spectrum\
 --disable-videobox\
 --disable-videomixer\
 --disable-y4m\
 --disable-directsound\
 --disable-oss\
 --disable-sunaudio\
 --disable-osx_aidio\
 --disable-osx_video\
 --disable-aalib\
 --disable-aalibtest\
 --disable-annodex\
 --disable-cairo\
 --disable-esd\
 --disable-esdtest\
 --disable-flac\
 --disable-gconf\
 --disable-hal\
 --disable-libcaca\
 --disable-libdv\
 --disable-dv1394\
 --disable-shout2\
 --disable-shout2test\
 --disable-speex\
 --disable-taglib


make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}/usr/share/license
cp COPYING %{buildroot}/usr/share/license/%{name}
%make_install

%post
[ ! -f /etc/nsswitch.conf ] && ( [ -f /etc/nsswitch.conf.tizen ] && ln -s /etc/nsswitch.conf.tizen /etc/nsswitch.conf ) || [ -f /etc/nsswitch.conf ] && echo -n || echo "warn: nsswitch could not renamed"

%files
%manifest gst-plugins-good.manifest
%defattr(-,root,root,-)
%{_libdir}/gstreamer-0.10
%{_libdir}/gstreamer-0.10/libgstavi.so
%{_libdir}/gstreamer-0.10/libgstflv.so
%{_libdir}/gstreamer-0.10/libgstmatroska.so
%{_libdir}/gstreamer-0.10/libgstrtsp.so
%{_libdir}/gstreamer-0.10/libgstisomp4.so
%{_libdir}/gstreamer-0.10/libgstvideocrop.so
%{_libdir}/gstreamer-0.10/libgstid3demux.so
%{_libdir}/gstreamer-0.10/libgstpulse.so
%{_libdir}/gstreamer-0.10/libgstmultifile.so
%{_libdir}/gstreamer-0.10/libgstpng.so
%{_libdir}/gstreamer-0.10/libgstudp.so
%{_libdir}/gstreamer-0.10/libgstximagesrc.so
%{_libdir}/gstreamer-0.10/libgstalaw.so
%{_libdir}/gstreamer-0.10/libgstrtpmanager.so
%{_libdir}/gstreamer-0.10/libgstaudioparsers.so
%{_libdir}/gstreamer-0.10/libgstimagefreeze.so
%{_libdir}/gstreamer-0.10/libgstjpeg.so
%{_libdir}/gstreamer-0.10/libgstautodetect.so
%{_libdir}/gstreamer-0.10/libgstvideofilter.so
%{_libdir}/gstreamer-0.10/libgstmulaw.so
%{_libdir}/gstreamer-0.10/libgstrtp.so
%{_libdir}/gstreamer-0.10/libgstwavparse.so
%{_libdir}/gstreamer-0.10/libgstwavenc.so
%{_libdir}/gstreamer-0.10/libgstvideo4linux2.so
%{_libdir}/gstreamer-0.10/libgstshapewipe.so
%{_libdir}/gstreamer-0.10/libgstoss4audio.so
%{_libdir}/gstreamer-0.10/libgstsouphttpsrc.so
/usr/share/license/%{name}
