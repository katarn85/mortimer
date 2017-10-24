# Note that this is NOT a relocatable package
%define ver      2.6.15
%define enable_mono 0
%define enable_gtk_doc 0

%if %{enable_mono}
%define mono_configure_flags --enable-mono
%else
%define mono_configure_flags --disable-mono
%endif

%if %{enable_gtk_doc}
%define gtkdoc_configure_flags --enable-gtk-doc
%else
%define gtkdoc_configure_flags --disable-gtk-doc
%endif

Summary: MIME library
Name: gmime
Version: %ver
Release: 2
License: LGPL
Group: Development/Libraries
URL: http://spruce.sourceforge.net/gmime/

Source: gmime-%{version}.tar.bz2

Requires: glib2 >= 2.12.0
BuildRequires: glib2-devel >= 2.12.0
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(gio-2.0)
BuildRequires:  pkgconfig(zlib)

%description
GMime is a set of utilities for parsing and creating messages using
the Multipurpose Internet Mail Extension (MIME)

%package devel
Summary:    GMime Development package
Group:      Development/Libraries
Requires:   %{name} = %{version}-%{release}

%description devel
GMime Development package

%prep
%setup

%build
if [ ! -f configure ]; then
  CFLAGS="$RPM_OPT_FLAGS" ./autogen.sh $ARCHFLAG %{config_opts} %{mono_configure_flags}
fi
CFLAGS="$RPM_OPT_FLAGS" ./configure --prefix=%{_prefix} %{mono_configure_flags}
make

%install
rm -rf %{buildroot}
make install DESTDIR=%{buildroot}

mkdir -p %{buildroot}/usr/share/license/gmime
cp -p ./COPYING %{buildroot}/usr/share/license/gmime/COPYING
# rename to prevent conflict with uu* utils from sharutils
#mv %{buildroot}%{_prefix}/bin/uuencode %{buildroot}%{_prefix}/bin/gmime-uuencode
#mv %{buildroot}%{_prefix}/bin/uudecode %{buildroot}%{_prefix}/bin/gmime-uudecode

%clean
rm -rf %{buildroot}

%files
%defattr(-, root, root)

#%doc doc/html/* AUTHORS ChangeLog NEWS README COPYING TODO
#%{_prefix}/bin/*
#%{_prefix}/lib/*.sh
%{_libdir}/libgmime*
%{_prefix}/share/license/gmime/*
%exclude %{_libdir}/*.a
%exclude %{_libdir}/*.la


%files devel
%{_includedir}/gmime-2.6/gmime/*.h
%{_libdir}/libgmime*
%{_libdir}/pkgconfig/*.pc
%exclude %{_libdir}/*.a
%exclude %{_libdir}/*.la

%changelog
* Mon Jun 17 2013 Minsoo Kim <minnsoo.kim@samsung.com>
- Modify .spec for Tizen
