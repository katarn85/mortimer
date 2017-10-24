Name:       glib-networking
Summary:    Network extensions for GLib
Version:    2.32.3_1.9
Release:    1
Group:      System/Libraries
License:    LGPLv2
URL:        http://git.gnome.org/browse/glib-networking/
Source0:    %{name}-%{version}.tar.gz

BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(gnutls)
BuildRequires:  intltool

%description
Networking extensions for GLib



%prep
%setup -q -n %{name}-%{version}

%build

%configure --disable-static --without-ca-certificates
make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}/usr/share/license
cp COPYING %{buildroot}/usr/share/license/%{name}

%make_install

%find_lang glib-networking

%files -f glib-networking.lang
%defattr(-,root,root,-)
%{_libdir}/gio/modules/libgiognutls.so
/usr/share/license/%{name}
%manifest glib-networking.manifest
