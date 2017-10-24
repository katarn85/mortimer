#sbs-git:slp/pkgs/xorg/miscs/usbutils usbutils 0.86 9271bc1e8de96d6f0ee693af5853e58152ce6a59
Name:           usbutils
Version: 0.86
Release:        6
License:        GPLv2+
Url:            http://www.linux-usb.org/
Source:         http://downloads.sourceforge.net/linux-usb/%{name}-%{version}.tar.gz
Summary:        Linux USB utilities
Group:          Applications/System
Source101:      usbutils-rpmlintrc
BuildRequires:  autoconf,
BuildRequires:  libtool,
BuildRequires:  pkgconfig(libusb) >= 0.1.8

%description
This package contains utilities for inspecting devices connected to a
USB bus.

%prep
%setup -q

%build
%reconfigure \
	--datadir=/usr/lib/usbutils

make %{?_smp_mflags}

%install
make install DESTDIR=%{buildroot} pkgconfigdir=/usr/share/pkgconfig
install -D -m 0644 COPYING %{buildroot}/usr/share/license/usbutils

%docs_package

%files
%defattr(-,root,root,-)
%{_sbindir}/*
%{_bindir}/*
%{_datadir}/pkgconfig/usbutils.pc
/usr/lib/usbutils/*
/usr/share/license/usbutils
%doc COPYING

