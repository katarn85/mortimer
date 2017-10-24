Name: gst-plugins-fimcconvert
Summary: Video Scale, Rotate, Colorspace Convert Gstreamer plug-in based on FIMC
Version: 0.1.4
Release: 0
ExclusiveArch: %arm
Group: Applications/Multimedia
License: LGPLv2.1
Source0: %{name}-%{version}.tar.gz

Provides: libgstfimcconvert.so

%description
This package provides the shared library.
Video Scale, Rotate, Colorspace Convert Gstreamer plug-in based on FIMC.

%prep
%setup -q

%build
./autogen.sh
%configure --disable-static

make %{?jobs:-j%jobs}


%install
rm -rf %{buildroot}
make install DESTDIR=%{buildroot}
mkdir -p %{buildroot}/usr/share/license
cp LICENSE.LGPLv2.1 %{buildroot}/usr/share/license/%{name}

%files
%manifest gst-plugins-fimcconvert.manifest
%defattr(-,root,root,-) 
%{_libdir}/gstreamer-0.10/*.so
%{_datadir}/license/%{name}
