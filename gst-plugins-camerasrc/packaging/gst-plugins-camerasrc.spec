Name:       gst-plugins-camerasrc
Summary:    Gstreamer plugins package for v4l2 camera
Version:    0.1.1
Release:    4
Group:      libs
License:    LGPLv2+
Source0:    %{name}-%{version}.tar.gz
ExclusiveArch:  %arm
BuildRequires:  pkgconfig(gstreamer-plugins-base-0.10)
BuildRequires:  pkgconfig(mm-ta)
BuildRequires:  gstreamer-devel
BuildRequires:  autoconf
BuildRequires:  kernel-headers

%description
Gstreamer plugins package for v4l2 camera
 GStreamer is a streaming media framework, based on graphs of filters
 which operate on media data. Multimedia Framework using this plugins
 library can encode and decode video, audio, and speech..

%prep
%setup -q

%build
sh ./autogen.sh
%configure --disable-static
make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}/usr/share/license
cp LICENSE.LGPLv2.1 %{buildroot}/usr/share/license/%{name}
%make_install

%files
%manifest gst-plugins-camerasrc.manifest
%defattr(-,root,root,-)
%{_libdir}/gstreamer-0.10/lib*.so*
%{_datadir}/license/%{name}
