Name:       gst-plugins-ugly
Summary:    GStreamer plugins from the "ugly" set
Version:    0.10.19
Release:    56
Group:      Applications/Multimedia
License:    LGPLv2+
Source0:    %{name}-%{version}.tar.gz
Patch0 :    gst-plugins-ugly-disable-gtk-doc.patch
BuildRequires:  gettext-tools
BuildRequires:  which
BuildRequires:  gst-plugins-base-devel
BuildRequires:  pkgconfig(gstreamer-0.10)
BuildRequires:  pkgconfig(glib-2.0)
#BuildRequires:  pkgconfig(drm-client)
#BuildRequires:  pkgconfig(drm-trusted)
BuildRequires:  pkgconfig(opencore-amrnb)
BuildRequires:  pkgconfig(opencore-amrwb)
#BuildRequires:  lame-devel

%description
 GStreamer is a streaming media framework, based on graphs of filters
 which operate on media data.  Applications using this library can do
 anything from real-time sound processing to playing videos, and just
 about anything else media-related.  Its plugin-based architecture means
 that new data types or processing capabilities can be added simply by
 installing new plug-ins.
 .
 This packages contains plugins from the "ugly" set, a set of
 good-quality plug-ins that might pose distribution problems.



%prep
%setup -q 
%patch0 -p1


%build
./autogen.sh
%configure --prefix=%{_prefix}\
 --disable-static\
 --disable-nls\
 --with-html-dir=/tmp/dump\
 --disable-examples\
 --disable-dvdlpcmdec\
 --disable-dvdsub\
 --disable-iec958\
 --disable-mpegstream\
 --disable-synaesthesia\
 --disable-a52dec\
 --disable-cdio\
 --disable-dvdread\
 --disable-dvdnav\
 --disable-mad\
 --disable-mpeg2dec\
 --disable-sidplay\
 --disable-twolame\
 --disable-x264



make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}/usr/share/license
cp COPYING %{buildroot}/usr/share/license/%{name}
%make_install


%files
%manifest gst-plugins-ugly.manifest
%defattr(-,root,root,-)
%{_libdir}/gstreamer-0.10/libgstmpegaudioparse.so
/usr/share/license/%{name}
%{_libdir}/gstreamer-0.10/libgstasf.so
%{_libdir}/gstreamer-0.10/libgstamrnb.so
%{_libdir}/gstreamer-0.10/libgstamrwbdec.so
%{_libdir}/gstreamer-0.10/libgstrmdemux.so
#%{_libdir}/gstreamer-0.10/libgstlame.so
%exclude %{_datadir}/gstreamer-0.10/presets/GstAmrnbEnc.prs

