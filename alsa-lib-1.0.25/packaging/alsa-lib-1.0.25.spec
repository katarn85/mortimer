Name:       alsa-lib-1.0.25
Summary:    The Advanced Linux Sound Architecture (ALSA) library
Version:    1.0.25
Release:    5
Group:      System/Libraries
License:    LGPLv2+
URL:        http://www.alsa-project.org/
Source0:    %{name}-%{version}.tar.gz


%description
The Advanced Linux Sound Architecture (ALSA) provides audio and MIDI
functionality to the Linux operating system.

This package includes the ALSA runtime libraries to simplify application
programming and provide higher level functionality as well as support for
the older OSS API, providing binary compatibility for most OSS programs.



%package -n libasound-1.0.25
Summary:    ALSA Library package for multimedia framework middleware package
Group:      Development/Libraries
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig

%description -n libasound-1.0.25
ALSA Library package for multimedia framework middleware package


%package -n libasound-1.0.25-devel
Summary:    ALSA Library package for multimedia framework middleware package
Group:      Development/Libraries
Requires:   libasound-1.0.25

%description -n libasound-1.0.25-devel
ALSA Library package for multimedia framework middleware development package


%prep
%setup -q


%build
export CFLAGS+=" -fPIC"
export LDFLAGS+=" -Wl,--warn-unresolved-symbols -Wl,--hash-style=both -Wl,--as-needed"
chmod +x autogen.sh

%autogen --disable-static
%configure --disable-static \
%ifarch %{arm}
    --with-alsa-devdir=/dev/snd \
%endif
%ifarch %{arm}
    --disable-alisp \
%endif
%ifarch %{arm}
    --disable-seq \
%endif
%ifarch %{arm}
    --disable-rawmidi \
%endif
%ifarch %{arm}
    --disable-python \
%endif
%ifarch %{arm}
    --with-gnu-ld \
%endif
%ifarch %{arm}
    --with-pcm-plugins=rate,linear,plug,dmix,dsnoop,asym,mmap,ioplug,null
%endif

make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}/usr/share/license
cp COPYING %{buildroot}/usr/share/license/%{name}
%make_install

rm -f %{buildroot}/%{_bindir}/aserver

%post -n libasound-1.0.25 -p /sbin/ldconfig

%postun -n libasound-1.0.25 -p /sbin/ldconfig

%files
%manifest alsa-lib.manifest
%defattr(-,root,root,-)
%{_datadir}/license/%{name}

%files -n libasound-1.0.25
%defattr(-,root,root,-)
%{_libdir}/lib*.so.*
%{_libdir}/alsa-lib/smixer/*.so
%{_datadir}/alsa/*

%files -n libasound-1.0.25-devel
%defattr(-,root,root,-)
%{_includedir}/*
%{_libdir}/lib*.so
%{_libdir}/pkgconfig/*
%{_datadir}/aclocal
