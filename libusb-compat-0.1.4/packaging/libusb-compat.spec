#
# spec file for package libusb-compat
#
# Copyright (c) 2012 SUSE LINUX Products GmbH, Nuernberg, Germany.
#
# All modifications and additions to the file contributed by third parties
# remain the property of their copyright owners, unless otherwise agreed
# upon. The license for this file, and modifications and additions to the
# file, is the same license as for the pristine package itself (unless the
# license for the pristine package is not an Open Source License, in which
# case the license is the MIT License). An "Open Source License" is a
# license that conforms to the Open Source Definition (Version 1.9)
# published by the Open Source Initiative.

# Please submit bugfixes or comments via http://bugs.opensuse.org/
#


Name:           libusb-compat
Url:            http://libusb.wiki.sourceforge.net/LibusbCompat0.1
Summary:        libusb-1.0 Compatibility Layer for libusb-0.1
License:        BSD-3-Clause ; LGPL-2.1+
Group:          System/Libraries
Version:        0.1.4
Release:        3
Source:         %{name}-%{version}.tar.gz
source1:		libusb-compat.manifest
BuildRequires:  libtool
BuildRequires:  pkgconfig(libusb-1.0)

%description
A compatibility layer allowing applications written for libusb-0.1 to
work with libusb-1.0. libusb-compat-0.1 attempts to look, feel, smell
and walk like libusb-0.1.

%package devel
Summary:        libusb-1.0 Compatibility Layer for libusb-0.1
Group:          Development/Libraries/C and C++
Requires: %{name} = %{version}-%{release}

%description devel
A compatibility layer allowing applications written for libusb-0.1 to
work with libusb-1.0. libusb-compat-0.1 attempts to look, feel, smell
and walk like libusb-0.1.

%prep
%setup -q

%build
cp %{SOURCE1} .
%configure\
	--disable-static\
	--disable-build-docs
make %{?jobs:-j%jobs}

%install
%makeinstall
rm %{buildroot}%{_libdir}/*.la

install -D -m 0644 COPYING %{buildroot}/usr/share/license/libusb-compat
cat LICENSE >> %{buildroot}/usr/share/license/libusb-compat


%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%defattr(-,root,root)
%doc AUTHORS COPYING ChangeLog LICENSE NEWS README
%{_libdir}/*.so.*
%{_datadir}/license/libusb-compat

%files devel
%defattr(-,root,root)
%{_bindir}/*-config
%{_includedir}/*.h
%{_libdir}/*.so
%{_libdir}/pkgconfig/*.pc

