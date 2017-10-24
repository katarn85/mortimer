#sbs-git:slp/pkgs/g/gstreamer0.10-ffmpeg gst-ffmpeg 0.10.11 eab91d2292960a6c9af3b27ca939ad65a4418984
Name:       gst-ffmpeg
Summary:    FFmpeg plugin for GStreamer
Version:    0.10.14
Release:    26
Group:      TO_BE/FILLED_IN
License:    LGPLv2+
Source0:    %{name}-%{version}.tar.gz
#Patch0:     gst-ffmpeg-autotools.patch
#Patch1:     gst-ffmpeg-ffmux-ehancement.patch
#Patch2:     gst-ffmpeg-swscale-pc.patch
BuildRequires:  gettext
BuildRequires:  which
BuildRequires:  gstreamer-tools
BuildRequires:  gst-plugins-base-devel  
BuildRequires:  pkgconfig(gstreamer-0.10) 
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(zlib)

%description
This GStreamer plugin supports a large number of audio and video compression
formats through the use of the FFmpeg library.  The plugin contains GStreamer
elements for decoding 90+ formats (AVI, MPEG, OGG, Matroska, ASF, ...),
demuxing 30+ formats and colorspace conversion.


%prep
%setup -q 
#%patch0 -p1
#%patch1 -p1
#%patch2 -p1

%build
./autogen.sh 

export CFLAGS+=" -Wall -Wno-unused-but-set-variable -Wno-implicit-function-declaration -g -fPIC\
 -DFFDEC_RANK_MODIFICATION"

export EXTRA_CONFIGURE="--disable-gpl"

%ifarch %{arm}
export EXTRA_CONFIGURE="$EXTRA_CONFIGURE --disable-libvorbis --disable-libtheora --enable-decoders --enable-encoders"
%else
export EXTRA_CONFIGURE="$EXTRA_CONFIGURE --disable-libvorbis --disable-libtheora"
%endif
%configure  --disable-static \
        --disable-nls \
        --prefix=%{_prefix} \
        --with-html-dir=/tmp/dump \
        --with-ffmpeg-extra-configure="$EXTRA_CONFIGURE"

make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}/usr/share/license
cp COPYING.LIB %{buildroot}/usr/share/license/%{name}
%make_install


%files
%manifest gst-ffmpeg.manifest
%defattr(-,root,root,-)
/usr/lib/gstreamer-0.10/libgstffmpeg.so
/usr/share/license/%{name}
