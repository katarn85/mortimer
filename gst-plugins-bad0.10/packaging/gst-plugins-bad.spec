#sbs-git:slp/pkgs/g/gst-plugins-bad0.10 gst-plugins-bad 0.10.23 3e50f3f786fc9956e191c290b2169d52808cd441
Name:       gst-plugins-bad
Summary:    GStreamer plugins from the "bad" set
Version:    0.20.23.tv
Release:    15
Group:      Applications/Multimedia
License:    LGPLv2+
Source0:    %{name}-%{version}.tar.gz
Patch0:     gst-plugins-bad-disable-gtk-doc.patch
BuildRequires:  gettext
BuildRequires:  gst-plugins-base-devel  
BuildRequires:  pkgconfig(gstreamer-0.10) 
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(libcrypto)
BuildRequires:  pkgconfig(libmms)

%description
GStreamer is a streaming media framework, based on graphs of filters
which operate on media data.  Applications using this library can do
anything from real-time sound processing to playing videos, and just
about anything else media-related.  Its plugin-based architecture means
that new data types or processing capabilities can be added simply by
installing new plug-ins.
.
GStreamer Bad Plug-ins is a set of plug-ins that aren't up to par compared
to the rest. They might be close to being good quality, but they're missing
something - be it a good code review, some documentation, a set of tests, a
real live maintainer, or some actual wide use.


%package devel
Summary:    Development tools for GStreamer bad plugins
Group:      Development/Libraries
Requires:   %{name} = %{version}-%{release}

%description devel
Development files for the GStreamer media framework badplug-ins


%prep
%setup -q 
%patch0 -p1


%build
./autogen.sh

export CFLAGS+=" -Wall -g -fPIC\
 -DGST_EXT_HLS_MODIFICATION\
 -DGST_H263PARSE_MODIFICATION\
 -DGST_H264PARSE_MODIFICATION"

%ifarch %{arm}
export CFLAGS+=" -DSAMSUNG_WFD_SPEC -DWFD_SPEC"
%endif

%configure  --prefix=%{_prefix}\
 --disable-static\
 --disable-nls\
 --with-html-dir=/tmp/dump\
 --disable-examples\
 --disable-aiff\
 --disable-amrparse\
 --disable-asfmux\
 --disable-bayer\
 --disable-cdxaparse\
 --disable-dccp\
 --disable-dvdspu\
 --disable-festival\
 --disable-freeze\
 --disable-frei0r\
 --disable-hdvparse\
 --disable-librfb\
 --disable-modplug\
 --disable-mpegpsmux\
 --disable-mve\
 --disable-mxf\
 --disable-nsf\
 --disable-nuvdemux\
 --disable-pcapparse\
 --disable-pnm\
 --disable-qtmux\
 --disable-real\
 --disable-scaletempo\
 --disable-shapewipe\
 --disable-siren\
 --disable-speed\
 --disable-subenc\
 --disable-stereo\
 --disable-tta\
 --disable-videomeasure\
 --disable-videosignal\
 --disable-vmnc\
 --disable-directsound\
 --disable-directdraw\
 --disable-osx_video\
 --disable-vcd\
 --disable-assrender\
 --disable-amrwb\
 --disable-apexsink\
 --disable-bz2\
 --disable-cdaudio\
 --disable-celt\
 --disable-cog\
 --disable-dc1394\
 --disable-directfb\
 --disable-dirac\
 --disable-dts\
 --disable-divx\
 --disable-dvdnav\
 --disable-faac\
 --disable-faad\
 --disable-fbdev\
 --disable-gsm\
 --disable-ivorbis\
 --disable-jack\
 --disable-jp2k\
 --disable-kate\
 --disable-ladspa\
 --disable-lv2\
 --disable-modplug\
 --disable-mimic\
 --disable-mpeg2enc\
 --disable-mplex\
 --disable-musepack\
 --disable-musicbrainz\
 --disable-mythtv\
 --disable-nas\
 --disable-neon\
 --disable-ofa\
 --disable-timidity\
 --disable-wildmidi\
 --disable-sdl\
 --disable-sdltest\
 --disable-sndfile\
 --disable-soundtouch\
 --disable-spc\
 --disable-gme\
 --disable-swfdec\
 --disable-theoradec\
 --disable-xvid\
 --disable-dvb\
 --disable-oss4\
 --disable-wininet\
 --disable-acm\
 --disable-vdpau\
 --disable-schro\
 --disable-zbar\
 --disable-dataurisrc\
 --disable-coloreffects\
 --disable-colorspace\
 --disable-videomaxrate\
 --disable-jp2kdecimator\
 --disable-interlace\
 --disable-gaudieffects\
 --disable-y4m\
 --disable-adpcmenc\
 --disable-segmentclip\
 --disable-geometrictransform\
 --disable-inter\
 --disable-dvbsuboverlay\
 --disable-ivfparse\
 --disable-gsettings\
 --disable-opus\
 --disable-openal\
 --disable-snapdsp\
 --disable-teletextdec\
 --disable-audiovisualizers\
 --disable-faceoverlay\
 --disable-freeverb\
 --disable-removesilence\
 --disable-smooth\
 --disable-avc\
 --disable-d3dvideosink\
 --disable-vp8\
%ifarch %{arm}
 --enable-samsung-wfd-spec\
%else
 --disable-samsung-wfd-spec\
%endif
 --disable-qc-specific \
 --disable-pvr2d

make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}/usr/share/license
cp %{_builddir}/%{name}-%{version}/COPYING  %{buildroot}/usr/share/license/%{name}
cat %{_builddir}/%{name}-%{version}/COPYING.LIB >> %{buildroot}/usr/share/license/%{name}
%make_install




%files
%manifest gst-plugins-bad.manifest
%defattr(-,root,root,-)
%{_libdir}/libgstbasevideo-0.10.so.*
%{_libdir}/libgstcodecparsers-0.10.so.*
%{_libdir}/libgstbasecamerabinsrc-0.10.so.*
%{_libdir}/libgstphotography-0.10.so.*
%{_libdir}/libgstsignalprocessor-0.10.so.*
%{_libdir}/gstreamer-0.10/libgst*.so
/usr/share/license/%{name}

%files devel
%defattr(-,root,root,-)
%{_libdir}/libgst*.so
%{_includedir}/gstreamer-0.10/gst/*
%{_libdir}/pkgconfig/gstreamer-*.pc
