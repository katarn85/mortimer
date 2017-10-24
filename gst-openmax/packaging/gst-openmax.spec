
Name:       gst-openmax
Summary:    GStreamer plug-in that allows communication with OpenMAX IL components
Version:    0.10.16+tv
Release:    31
Group:      Application/Multimedia
License:    LGPLv2.1
Source0:    %{name}-%{version}.tar.gz
BuildRequires: which
BuildRequires: kernel-headers
BuildRequires: pkgconfig(gstreamer-0.10)
#BuildRequires: pkgconfig(tvs-userdata-parser)
BuildRequires: pkgconfig(audio-session-mgr)
BuildRequires: pkgconfig(libavoc)
#BuildRequires: pkgconfig(divxdrm)

%description
gst-openmax is a GStreamer plug-in that allows communication with OpenMAX IL components.
Multiple OpenMAX IL implementations can be used.

%package devel
Summary:    gst-openmax Library
Group:      Development/Libraries
Provides:   libgstomx.so

%description devel
gst-openmax library for development

%prep
%setup -q

%build
./autogen.sh --noconfigure
%configure --disable-static --prefix=/usr

make %{?jobs:-j%jobs}


%install
rm -rf %{buildroot}
%make_install

%files
%manifest gst-openmax.manifest
%{_libdir}/gstreamer-0.10/libgstomx.so

%files devel
%defattr(-,root,root,-)
%{_includedir}/gstomx/*.h
%{_libdir}/gstreamer-0.10/*.so
%{_libdir}/pkgconfig/gst-openmax.pc
